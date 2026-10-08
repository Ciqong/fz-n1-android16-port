#!/system/bin/sh
# Preserve installed app data. Restore the pre-suite scanner startup hook.
BASE=/data/adb/fzn1-stock
if [ -s "$BASE/backup/fzn1-scanner.sh" ] && [ ! -e /data/adb/service.d/fzn1-scanner.sh ]; then
 cp -p "$BASE/backup/fzn1-scanner.sh" /data/adb/service.d/fzn1-scanner.sh
fi
