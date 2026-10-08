#!/system/bin/sh
set -eu
original=d16424ce97a81a571d91e15051de0ba66db397aff00c465245fdd666fea7df8c
patched=e6791b7be7d8517c875ece37a8433e6c0a91637db7f62715793179ac12aec0e1
target=/vendor/lib64/libcfg.so
source=/data/local/tmp/fzn1-libcfg-byte-safe.so
backup=/data/local/tmp/fzn1-libcfg.so
new=/vendor/lib64/libcfg.so.fzn1-new
test "$(sha256sum "$target" | cut -d ' ' -f1)" = "$original"
test "$(sha256sum "$backup" | cut -d ' ' -f1)" = "$original"
test "$(sha256sum "$source" | cut -d ' ' -f1)" = "$patched"
ls -lZ "$target"
mount -o remount,rw /vendor
done_ok=0
replaced=0
cleanup() {
    if test "$done_ok" != 1 && test "$replaced" = 1; then
        cp "$backup" "$new"
        chown 0:0 "$new"
        chmod 644 "$new"
        mv -f "$new" "$target"
        restorecon "$target"
        echo PATCH_ROLLED_BACK
    fi
    mount -o remount,ro /vendor
}
trap cleanup EXIT
cp "$source" "$new"
chown 0:0 "$new"
chmod 644 "$new"
test "$(sha256sum "$new" | cut -d ' ' -f1)" = "$patched"
mv -f "$new" "$target"
replaced=1
restorecon "$target"
test "$(sha256sum "$target" | cut -d ' ' -f1)" = "$patched"
ls -lZ "$target"
sha256sum "$target"
sync
done_ok=1
echo PATCH_INSTALLED_VERIFIED
