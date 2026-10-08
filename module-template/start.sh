#!/system/bin/sh
set -eu
MODDIR=${0%/*}
[ ! -e "$MODDIR/disable" ] && [ ! -e "$MODDIR/remove" ] || exit 0
BASE=/data/adb/fzn1-stock
mkdir -p "$BASE"
BOOT=$(cat /proc/sys/kernel/random/boot_id)
exec >>"$BASE/boot-$BOOT.log" 2>&1
printf '%s\n' "SUITE_START boot=$BOOT"
# Ordinary APKs remain ordinary data apps, preserving their SELinux domain and data.
for spec in "BarcodeReader com.panasonic.mobile.barcodereader.compat" "Diagnostic com.panasonic.psn.android.app.diag.compat"; do
 set -- $spec
 NAME=$1; PKG=$2
 n=0
 while [ "$(getprop sys.boot_completed)" != 1 ]; do n=$((n+1)); [ "$n" -lt 180 ] || exit 1; sleep 1; done
 PATH_OUT=$(pm path "$PKG" 2>&1)
 APK_PATH=$(printf '%s\n' "$PATH_OUT" | sed -n 's/^package://p' | head -n 1)
 WANT=$(sha256sum "$MODDIR/apps/$NAME/$NAME.apk" | cut -d' ' -f1)
 HAVE=''
 [ ! -f "$APK_PATH" ] || HAVE=$(sha256sum "$APK_PATH" | cut -d' ' -f1)
 if [ "$HAVE" != "$WANT" ]; then
  OUT=$(pm install -r "$MODDIR/apps/$NAME/$NAME.apk" 2>&1)
  printf '%s\n' "$OUT"
  printf '%s\n' "$OUT" | grep -q '^Success$' || exit 1
 fi
done
SC=$BASE/scanner
mkdir -p "$SC/payload"
for n in boot.sh magiskpolicy scanner.rules; do cp "$MODDIR/scanner/$n" "$SC/$n"; done
cp "$MODDIR/scanner/payload/"* "$SC/payload/"
chmod 700 "$SC/boot.sh" "$SC/magiskpolicy"
if /system/bin/sh "$SC/boot.sh"; then
 printf '%s\n' "SCANNER_READY boot=$BOOT"
else
 printf '%s\n' "SCANNER_FAILED see scanner/logs/$BOOT/boot.log"
fi
/system/bin/sh "$MODDIR/buttons-start.sh"
printf '%s\n' "BUTTONS_STARTED boot=$BOOT"
