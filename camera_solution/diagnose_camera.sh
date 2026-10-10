#!/system/bin/sh
# =========================================================================
# iSperm RK3288 摄像头快速诊断排查脚本
# 用于在 10 秒内判断当前主板不出画面是出在:
# 1. 供电 GPIO (PWEN) 异常
# 2. 复位/待机 GPIO (PWDN) 异常
# 3. I2C 总线 3 物理链路不通 (排线接触不良/公差)
# 4. HAL 缓存死锁 (UVC 模式残留)
# =========================================================================

echo "========================================================"
echo "    iSperm RK3288 Camera Diagnostic Tool (Android 5.1)"
echo "========================================================"

echo "\n--- [1] 检查核心内核节点与驱动 ---"
if [ -e /dev/camsys_marvin ]; then
    echo "[OK] /dev/camsys_marvin 存在 (RK3288 ISP 驱动正常运行)"
else
    echo "[FAIL] /dev/camsys_marvin 不存在! 请检查内核驱动"
fi

if [ -e /dev/camsys_marvin1 ]; then
    echo "[INFO] /dev/camsys_marvin1 存在"
fi

echo "\n--- [2] 检查系统相机配置文件 ---"
if [ -f /system/etc/cam_board.xml ]; then
    echo "[OK] /system/etc/cam_board.xml 存在"
    echo "     Sensor 配置数量: $(grep -c '<SensorName' /system/etc/cam_board.xml)"
else
    echo "[FAIL] /system/etc/cam_board.xml 丢失!"
fi

if [ -f /system/etc/OV8825.xml ]; then
    echo "[OK] /system/etc/OV8825.xml 调校文件存在"
else
    echo "[WARN] /system/etc/OV8825.xml 不存在"
fi

echo "\n--- [3] 检查动态缓存文件 (/data/camera/media_profiles.xml) ---"
if [ -f /data/camera/media_profiles.xml ]; then
    echo "[!] 发现动态缓存文件 /data/camera/media_profiles.xml:"
    cat /data/camera/media_profiles.xml
    if grep -q "UVC Camera" /data/camera/media_profiles.xml; then
        echo ">>> [警告] 缓存中被标记为 UVC Camera! 这会导致 HAL 误寻 /dev/video0 而放弃探测 MIPI 摄像头!"
        echo ">>> 建议执行: rm -f /data/camera/media_profiles.xml && reboot"
    fi
else
    echo "[OK] 缓存文件不存在 (下次开机将强制重新全量探测)"
fi

echo "\n--- [4] 检查 Camera HAL 最近一次握手日志 ---"
logcat -d -s CameraHal:D CameraHal:E | tail -n 25

echo "\n--- [5] 快速排查结论指引 ---"
echo "若上方日志包含:"
echo "1. 'load sensor name(OV8825) connect 0' -> 表示 I2C Bus 3 未收到应答 (排线、接触不良、供电或传感器不匹配)"
echo "2. 'load sensor name(OV8825) connect 1' -> 表示相机握手完全成功!"
echo "3. 'Open /dev/video0 failed'            -> 表示当前没有任何可用摄像头，HAL 退回兜底 USB 模式"
echo "========================================================"
