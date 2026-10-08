#!/system/bin/sh
set -eu
MODDIR=${0%/*}
[ ! -e "$MODDIR/disable" ] && [ ! -e "$MODDIR/remove" ] || exit 0
n=0
while [ "$(getprop sys.boot_completed)" != 1 ]; do
 n=$((n+1)); [ "$n" -lt 180 ] || exit 1; sleep 1
done
BASE=/data/adb/fzn1-buttons
mkdir -p "$BASE"
if [ -f "$BASE/runtime.pid" ]; then
 p=$(cat "$BASE/runtime.pid")
 case "$p" in ''|*[!0-9]*) ;; *)
  if [ "$(readlink /proc/$p/exe 2>/dev/null || true)" = "$BASE/button_bridge" ]; then
   kill "$p" 2>/dev/null || true
   n=0; while kill -0 "$p" 2>/dev/null; do n=$((n+1)); [ "$n" -lt 50 ] || exit 1; sleep .1; done
  fi ;;
 esac
fi
UID_OUT=$(cmd package list packages -U com.panasonic.mobile.userbuttonservice.compat 2>&1)
APP_UID=$(printf '%s\n' "$UID_OUT" | sed -n 's/^package:com.panasonic.mobile.userbuttonservice.compat uid:\([0-9]*\).*$/\1/p')
case "$APP_UID" in ''|*[!0-9]*) exit 1;; esac
[ "$APP_UID" -ge 10000 ] || exit 1
OUT=$(cmd deviceidle whitelist +com.panasonic.mobile.userbuttonservice.compat 2>&1) || true
printf '%s\n' "$OUT" >"$BASE/start.log"
OUT=$(am startservice -n com.panasonic.mobile.userbuttonservice.compat/com.panasonic.mobile.userbuttonservice.UserButtonService 2>&1) || { printf '%s\n' "$OUT" >>"$BASE/start.log"; exit 1; }
printf '%s\n' "$OUT" >>"$BASE/start.log"
cp "$MODDIR/bin/button_bridge" "$BASE/button_bridge"
chmod 755 "$BASE/button_bridge"
"$BASE/button_bridge" "$APP_UID" </dev/null >"$BASE/runtime.log" 2>&1 &
echo $! >"$BASE/runtime.pid"
