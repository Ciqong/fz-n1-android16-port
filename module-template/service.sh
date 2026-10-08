#!/system/bin/sh
MODDIR=${0%/*}
unset ASH_STANDALONE
exec /debug_ramdisk/su -mm -c "/system/bin/sh $MODDIR/start.sh"
