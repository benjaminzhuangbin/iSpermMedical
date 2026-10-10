#!/system/bin/sh
# =========================================================================
# iSperm Android 5.1 (RK3288) 相机配置缓存清理脚本
# 解决相机曾经失败过一次后，/data 分区生成错误的 media_profiles.xml
# 导致系统锁死在 "UVC Camera /dev/video0" 而不再探测 MIPI 摄像头的问题
# =========================================================================

echo "[-] Stopping mediaserver and camera services..."
stop mediaserver
stop cameraserver 2>/dev/null

echo "[-] Cleaning cached camera profiles in /data..."
rm -f /data/camera/media_profiles.xml
rm -f /data/media_profiles.xml
rm -rf /data/data/com.android.camera/cache/*
rm -rf /data/data/com.android.camera2/cache/*
rm -rf /data/data/com.join/cache/*

echo "[-] Current /data/camera contents:"
ls -l /data/camera 2>/dev/null

echo "[-] Starting mediaserver..."
start mediaserver

echo "[+] Done! Check logcat with: logcat -s CameraHal"
