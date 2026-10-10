#!/system/bin/sh
# factory/install_on_device.sh
# Run ONCE during manufacturing / engineering flash (needs shell that can su).
# Hospital end users never run this.
#
# Effect:
#   1) Install setuid /system/xbin/nexus_su  (APK can then get uid=0)
#   2) Install APK as /system/priv-app (boot-safe permanent app)
#   3) Optional: keep data install cleaned up
#
# Usage (from PC with USB or Ethernet adb already working once):
#   adb push factory/nexus_su /data/local/tmp/nexus_su
#   adb push release/NexusADBWatchdog.apk /data/local/tmp/NexusADBWatchdog.apk
#   adb push factory/install_on_device.sh /data/local/tmp/install_on_device.sh
#   adb shell "su -c 'sh /data/local/tmp/install_on_device.sh'"
#   adb reboot
#
# After reboot, hospital user only needs: power on + Ethernet.

set -e

NEXUS_SU_SRC="/data/local/tmp/nexus_su"
APK_SRC="/data/local/tmp/NexusADBWatchdog.apk"
NEXUS_SU_DST="/system/xbin/nexus_su"
PRIV_DIR="/system/priv-app/NexusADBWatchdog"
PRIV_APK="$PRIV_DIR/NexusADBWatchdog.apk"

echo "=== Nexus ADB Watchdog factory install ==="
id

if [ ! -f "$APK_SRC" ]; then
  echo "ERROR: missing $APK_SRC"
  exit 1
fi

# Remount system RW (Rockchip Android 5.1.1)
mount -o rw,remount /system 2>/dev/null || mount -o remount,rw /system 2>/dev/null || mount -o rw,remount /dev/block/platform/ff0f0000.rksdmmc/by-name/system /system 2>/dev/null || mount -o remount,rw / 2>/dev/null || true

echo "[1/3] Configure system root access"
# On Rockchip Android, clone stock su (67KB native ELF) and grant setuid 6755
if [ -f "/system/xbin/su" ]; then
  cp /system/xbin/su "$NEXUS_SU_DST"
elif [ -f "$NEXUS_SU_SRC" ]; then
  cp "$NEXUS_SU_SRC" "$NEXUS_SU_DST"
fi
chown 0:0 "$NEXUS_SU_DST" 2>/dev/null || chown root:root "$NEXUS_SU_DST" 2>/dev/null || true
chmod 6755 "$NEXUS_SU_DST" 2>/dev/null || chmod 4755 "$NEXUS_SU_DST" 2>/dev/null || true
chmod 6755 /system/xbin/su 2>/dev/null || chmod 4755 /system/xbin/su 2>/dev/null || true
ls -l "$NEXUS_SU_DST"

echo "[2/3] Verify root access"
/system/xbin/su -c id || "$NEXUS_SU_DST" -c id || true

echo "[3/3] Install APK as priv-app -> $PRIV_APK"
mkdir -p "$PRIV_DIR"
chmod 755 "$PRIV_DIR"
cp "$APK_SRC" "$PRIV_APK"
chmod 644 "$PRIV_APK"
chown 0:0 "$PRIV_APK" 2>/dev/null || chown root:root "$PRIV_APK" 2>/dev/null || true

# Register package immediately
pm install -r "$PRIV_APK" 2>/dev/null || true

# Clean stale status log so fresh run is obvious
rm -f /sdcard/NexusADBWatchdog/watchdog.status 2>/dev/null || true

sync
mount -o remount,ro /system 2>/dev/null || true

echo "=== OK: reboot required ==="
echo "After reboot expect:"
echo "  ROOT_OK=1"
echo "  ROOT_METHOD=NEXUS_SU:/system/xbin/nexus_su"
echo "  ROOT_UID=0"
echo "Log: /sdcard/NexusADBWatchdog/watchdog.status"
