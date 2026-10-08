#!/system/bin/sh
[ "$ARCH" = arm64 ] || abort "FZ-N1 ARM64 only"
[ "$(getprop ro.build.version.release)" = 16 ] || abort "This build is validated for Android 16 only"
BASE=/data/adb/fzn1-stock
mkdir -p "$BASE/backup"
# Preserve the previous module and legacy boot hook exactly once.
if [ ! -e "$BASE/backup/buttons-module.tar" ] && [ -d /data/adb/modules/fz-n1-userbutton-priv ]; then
 tar -cf "$BASE/backup/buttons-module.tar" -C /data/adb/modules fz-n1-userbutton-priv || abort "Could not back up existing module"
fi
if [ -f /data/adb/service.d/fzn1-scanner.sh ]; then
 cp -p /data/adb/service.d/fzn1-scanner.sh "$BASE/backup/fzn1-scanner.sh" || abort "Could not back up legacy scanner hook"
 # Renaming prevents duplicate scanner application on subsequent boots.
 mv /data/adb/service.d/fzn1-scanner.sh "$BASE/backup/fzn1-scanner.legacy-disabled"
fi
set_perm_recursive "$MODPATH" 0 0 0755 0644
for f in service.sh start.sh buttons-start.sh; do set_perm "$MODPATH/$f" 0 0 0755; done
set_perm "$MODPATH/bin/button_bridge" 0 0 0755
set_perm "$MODPATH/scanner/magiskpolicy" 0 0 0755
ui_print "Unified suite staged; old scanner hook backed up. Reboot to activate."
