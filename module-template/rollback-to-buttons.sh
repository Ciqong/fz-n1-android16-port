#!/system/bin/sh
set -eu
BASE=/data/adb/fzn1-stock
ID=fz-n1-userbutton-priv
[ -s "$BASE/backup/buttons-module.tar" ] || exit 1
[ -s "$BASE/backup/fzn1-scanner.sh" ] || exit 1
DEST=/data/adb/modules_update/$ID
mkdir -p /data/adb/modules_update
# Backup archive contains only this fixed module ID, recorded before suite migration.
tar -xf "$BASE/backup/buttons-module.tar" -C /data/adb/modules_update
cp -p "$BASE/backup/fzn1-scanner.sh" /data/adb/service.d/fzn1-scanner.sh
touch /data/adb/modules/$ID/update
printf '%s\n' "Previous buttons module and scanner hook staged. Reboot to finish rollback."
