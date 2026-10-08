#!/system/bin/sh
set -eu
backup=/data/local/tmp/fzn1-libcfg.so
target=/vendor/lib64/libcfg.so
test "$(sha256sum "$backup" | cut -d ' ' -f1)" = d16424ce97a81a571d91e15051de0ba66db397aff00c465245fdd666fea7df8c
mount -o remount,rw /vendor
cp "$backup" /vendor/lib64/libcfg.so.fzn1-restore
chown 0:0 /vendor/lib64/libcfg.so.fzn1-restore
chmod 644 /vendor/lib64/libcfg.so.fzn1-restore
mv -f /vendor/lib64/libcfg.so.fzn1-restore "$target"
restorecon "$target"
sha256sum "$target"
sync
echo ORIGINAL_LIBCFG_RESTORED_REBOOT_REQUIRED
