package com.isperm.qc;

import android.content.Context;
import android.os.Build;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Locale;

/**
 * iSperm Medical 设备硬件唯一指纹识别器
 * =====================================
 * 用于 Android 5.1.1 (RK3288 等医疗分析仪工控板及常见安卓设备)。
 *
 * 核心目标：
 * 提取稳定、不易在普通恢复出厂设置或修改配置时变动、且具备唯一性的设备硬件特征指纹。
 *
 * 采集维度：
 * 1. CPU / SoC 硬件芯片唯一序列号 (ro.serialno / Build.SERIAL / sysfs cpu serial)
 * 2. 主板不可变系统特征 (ro.board.platform, ro.product.board, ro.hardware)
 * 3. 固件底层的 Android ID (Settings.Secure.ANDROID_ID 作为辅助分量)
 * 4. 工控板 eMMC / 内部存储 CID 或唯一硬件标识 (若系统有权限读取)
 *
 * 计算规范：
 * 对规范化后的硬件特征流计算 SHA-256 并格式化为大写的 "ISPERM-DEV-XXXXXXXX-XXXXXXXX"。
 */
public class QcDeviceIdentity {

    private static final String TAG = "QcDeviceIdentity";
    private static volatile String sCachedDeviceId = null;

    private QcDeviceIdentity() {
    }

    /**
     * 获取当前精子分析仪的设备唯一标识符 (例如 "ISPERM-DEV-3A8F91B2-C4D5E6F7")
     *
     * @param context Android 上下文 (可为 Application 或 Activity)
     * @return 稳定唯一的设备编号
     */
    public static String getDeviceId(Context context) {
        if (sCachedDeviceId != null) {
            return sCachedDeviceId;
        }

        synchronized (QcDeviceIdentity.class) {
            if (sCachedDeviceId != null) {
                return sCachedDeviceId;
            }

            StringBuilder sb = new StringBuilder();

            // 1. 系统硬件序列号 (Build.SERIAL 或 ro.serialno)
            String serial = getSystemSerial();
            sb.append("SERIAL:").append(serial).append("|");

            // 2. Android ID (恢复出厂设置前恒定)
            String androidId = "";
            if (context != null) {
                try {
                    androidId = Settings.Secure.getString(context.getContentResolver(), Settings.Secure.ANDROID_ID);
                } catch (Throwable t) {
                    Log.w(TAG, "读取 ANDROID_ID 异常: " + t.getMessage());
                }
            }
            if (TextUtils.isEmpty(androidId)) {
                androidId = "UNKNOWN_AID";
            }
            sb.append("AID:").append(androidId).append("|");

            // 3. SoC 硬件平台特征 (如 rk3288, rk3399 等)
            sb.append("BOARD:").append(Build.BOARD).append("|");
            sb.append("HARDWARE:").append(Build.HARDWARE).append("|");
            sb.append("DEVICE:").append(Build.DEVICE).append("|");
            sb.append("CPU_ABI:").append(Build.CPU_ABI).append("|");

            // 4. Linux 内核级 CPU 序列号 (/proc/cpuinfo 中的 Serial 字段，RK3288 等芯片原生自带)
            String cpuInfoSerial = getCpuInfoSerial();
            sb.append("CPUSERIAL:").append(cpuInfoSerial).append("|");

            String rawFingerprint = sb.toString();
            Log.d(TAG, "设备硬件原始指纹分量: " + rawFingerprint);

            String hash = sha256Hex(rawFingerprint);
            // 截取前 16 字符分为两段，生成美观且易于核对的设备编号
            String part1 = hash.substring(0, 8).toUpperCase(Locale.US);
            String part2 = hash.substring(8, 16).toUpperCase(Locale.US);

            sCachedDeviceId = "ISPERM-DEV-" + part1 + "-" + part2;
            Log.i(TAG, "成功生成分析仪设备唯一编号: " + sCachedDeviceId);
            return sCachedDeviceId;
        }
    }

    /**
     * 获取系统硬件 Serial
     */
    private static String getSystemSerial() {
        String serial = null;
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {
                serial = Build.SERIAL;
            }
        } catch (Throwable t) {
            // ignore
        }

        if (TextUtils.isEmpty(serial) || "unknown".equalsIgnoreCase(serial)) {
            serial = getSystemProperty("ro.serialno");
        }
        if (TextUtils.isEmpty(serial) || "unknown".equalsIgnoreCase(serial)) {
            serial = getSystemProperty("ro.boot.serialno");
        }
        return (serial != null) ? serial.trim() : "UNKNOWN_SERIAL";
    }

    /**
     * 从 /proc/cpuinfo 中解析 Serial 字段 (瑞芯微等工控芯片标准硬件烧录唯一码)
     */
    private static String getCpuInfoSerial() {
        File cpuinfo = new File("/proc/cpuinfo");
        if (!cpuinfo.exists() || !cpuinfo.canRead()) {
            return "NO_CPUINFO";
        }
        try (BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream(cpuinfo), StandardCharsets.UTF_8))) {
            String line;
            while ((line = br.readLine()) != null) {
                if (line.toLowerCase(Locale.US).startsWith("serial")) {
                    int colon = line.indexOf(':');
                    if (colon >= 0 && colon < line.length() - 1) {
                        String s = line.substring(colon + 1).trim();
                        if (!s.isEmpty() && !s.equals("0000000000000000")) {
                            return s;
                        }
                    }
                }
            }
        } catch (Throwable t) {
            Log.w(TAG, "读取 /proc/cpuinfo 失败: " + t.getMessage());
        }
        return "DEFAULT_CPU_SERIAL";
    }

    /**
     * 通过反射读取 Android SystemProperties
     */
    private static String getSystemProperty(String key) {
        try {
            Class<?> clazz = Class.forName("android.os.SystemProperties");
            return (String) clazz.getMethod("get", String.class).invoke(null, key);
        } catch (Throwable t) {
            return null;
        }
    }

    /**
     * SHA-256 辅助函数
     */
    private static String sha256Hex(String text) {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] digest = md.digest(text.getBytes(StandardCharsets.UTF_8));
            StringBuilder hexString = new StringBuilder();
            for (byte b : digest) {
                String hex = Integer.toHexString(0xff & b);
                if (hex.length() == 1) hexString.append('0');
                hexString.append(hex);
            }
            return hexString.toString();
        } catch (Exception e) {
            return Integer.toHexString(text.hashCode());
        }
    }
}
