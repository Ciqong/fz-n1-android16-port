#!/system/bin/sh
D=/data/adb/fzn1-stock/scanner
[ ! -e "$D/disable" ] || exit 0
# Interrupted installation is disarmed on the next boot, never repeatedly retried.
if [ -e "$D/applying" ]; then
 touch "$D/disable"
 echo 'Prior boot interrupted during apply; disabled' > "$D/disabled-reason"
 exit 0
fi
B=$(cat /proc/sys/kernel/random/boot_id)
E=$D/logs/$B
mkdir -p "$E"
exec >> "$E/boot.log" 2>&1
set -eu
umask 077
T=/vendor/lib/hw
S=$D/stage
M=/debug_ramdisk/magisk
R=/debug_ramdisk/resetprop
stopped=0
mounted=0
policy=0
dac=0
prop=0
rollback() {
 rc=$?
 trap - EXIT
 if [ "$rc" != 0 ]; then
  echo "FAILED rc=$rc; disabling component and restoring this boot"
  touch "$D/disable"
  AM_OUT=$(am force-stop com.panasonic.mobile.barcodereader.compat 2>&1) || true
  echo "$AM_OUT" || true
  setprop ctl.stop vendor.camera-provider-2-4 || true
  sleep 2
  if [ "$mounted" = 1 ]; then umount "$T" || true; fi
  if [ "$policy" = 1 ]; then "$D/magiskpolicy" --load "$E/policy.before" --live || true; fi
  if [ "$prop" = 1 ]; then "$R" -n persist.odm.camera.expose.bcr "$(cat "$E/expose.before")" || true; fi
  if [ "$dac" = 1 ]; then
   read u g mode < "$E/moto.before"
   chown "$u:$g" /dev/moto_sdl || true
   chmod "$mode" /dev/moto_sdl || true
  fi
  setprop ctl.start vendor.camera-provider-2-4 || true
 fi
 rm -f "$D/applying"
 exit "$rc"
}
trap rollback EXIT
echo "START boot=$B"
for n in $(seq 1 180); do
 [ "$(getprop sys.boot_completed)" = 1 ] && break
 sleep 1
done
[ "$(getprop sys.boot_completed)" = 1 ]
[ "$(getenforce)" = Enforcing ]
[ -x "$M" ] && [ -x "$R" ] && [ -x "$D/magiskpolicy" ]
[ "$(sha256sum "$T/camera.sdm660.so" | cut -d' ' -f1)" = f312a01f163964a0eecd222d2b646357071d2b1e138c66da564569a8444926d7 ]
[ "$(sha256sum "$D/payload/camera.sdm660.backend.so" | cut -d' ' -f1)" = 6d486f29899f6b6f76ab9169dfd93ec58b4358f3cf33da667f92106395c51107 ]
[ "$(sha256sum "$D/payload/camera.sdm660.so" | cut -d' ' -f1)" = d257dfee4ee996fe5f0625394d04cd2b772c64a4ac78dd2f4eaff193f651a403 ]
UID_BCR=$(cmd package list packages -U com.panasonic.mobile.barcodereader.compat 2>&1 | sed -n 's/^package:com.panasonic.mobile.barcodereader.compat uid:\([0-9]*\)$/\1/p')
case "$UID_BCR" in ""|*[!0-9]*) echo "Invalid application UID"; exit 1;; esac
[ "$UID_BCR" -ge 10000 ]
[ -c /dev/moto_sdl ]
getprop persist.odm.camera.expose.bcr > "$E/expose.before"
stat -c '%u %g %a' /dev/moto_sdl > "$E/moto.before"
cat /sys/fs/selinux/policy > "$E/policy.before"
# The policy is always patched from THIS boot, not an old binary policy snapshot.
"$D/magiskpolicy" --load "$E/policy.before" --apply "$D/scanner.rules" --save "$E/policy.active"
# Rebuild a complete directory overlay from current stock files, without removing anything.
mkdir -p "$S"
cp -a "$T/." "$S/"
"$M" --clone-attr "$T" "$S"
for f in "$T"/*; do
 name=${f##*/}
 "$M" --clone-attr "$f" "$S/$name"
 if [ -f "$f" ]; then
  [ "$(sha256sum "$f" | cut -d' ' -f1)" = "$(sha256sum "$S/$name" | cut -d' ' -f1)" ]
 fi
done
cp "$D/payload/camera.sdm660.backend.so" "$S/camera.sdm660.backend.so"
cp "$D/payload/camera.sdm660.so" "$S/camera.sdm660.so"
"$M" --clone-attr "$T/camera.sdm660.so" "$S/camera.sdm660.backend.so"
"$M" --clone-attr "$T/camera.sdm660.so" "$S/camera.sdm660.so"
touch "$D/applying"
AM_OUT=$(am force-stop com.panasonic.mobile.barcodereader.compat 2>&1) || true
  echo "$AM_OUT"
setprop ctl.stop vendor.camera-provider-2-4
stopped=1
for n in $(seq 1 20); do [ "$(getprop init.svc.vendor.camera-provider-2-4)" = stopped ] && break; sleep 1; done
[ "$(getprop init.svc.vendor.camera-provider-2-4)" = stopped ]
mount --bind "$S" "$T"
mounted=1
"$D/magiskpolicy" --load "$E/policy.active" --live
policy=1
"$R" -n persist.odm.camera.expose.bcr 1
prop=1
chown "$UID_BCR:0" /dev/moto_sdl
dac=1
chmod 600 /dev/moto_sdl
setprop ctl.start vendor.camera-provider-2-4
sleep 5
# Do not pipe dumpsys to head: SIGPIPE kills this vendor provider.
CAMERA_OUT=$(timeout 15 dumpsys media.camera 2>&1)
printf '%s\n' "$CAMERA_OUT" > "$E/camera.after"
grep -q 'Number of camera devices: 3' "$E/camera.after"
[ "$(getenforce)" = Enforcing ]
[ "$(sha256sum "$T/camera.sdm660.so" | cut -d' ' -f1)" = d257dfee4ee996fe5f0625394d04cd2b772c64a4ac78dd2f4eaff193f651a403 ]
pidof android.hardware.camera.provider@2.4-service > "$E/provider.pid"
[ -s "$E/provider.pid" ]
rm -f "$D/applying"
printf '%s\n' "$B" > "$D/active-boot"
echo "READY uid=$UID_BCR boot=$B"
trap - EXIT
exit 0
