package com.isperm.qc;

import android.content.Context;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Collection;
import java.util.HashMap;
import java.util.Locale;

/**
 * iSperm Medical USB 硬件级介质身份提取器
 * =======================================
 * 专门用于从 Android 5.1.1 及其 Linux 内核中提取 USB 存储介质的物理硬件身份。
 *
 * 【硬件防复制核心原则】：
 * 1. 严禁使用 FAT32 卷标（Volume Label，用户可以在 Windows 右键重命名）。
 * 2. 严禁使用普通文本文件中的编号（可被复制）。
 * 3. 严禁仅依赖 FAT32 卷序列号（Volume Serial Number，格式化或克隆软件可轻易伪造）。
 *
 * 【物理身份提取路径】：
 * 1. Android UsbManager 硬件级序列号 (UsbDevice.getSerialNumber()，API 21+ 支持)。
 * 2. Linux sysfs 内核节点：/sys/bus/usb/devices/.../serial (USB 主控芯片出厂硬件烧录唯一码)。
 * 3. USB 硬件三元组联合指纹：VendorID(VID) + ProductID(PID) + Hardware Serial Number。
 *
 * 当用户将 U 盘内的全部文件拷贝到另一个普通的 FAT32 U 盘时，另一个 U 盘拥有完全不同的物理芯片硬件序列号，
 * 系统将计算出不同的 usb_hardware_id，导致官方签发的设备绑定授权文件无法匹配而拒绝导入！
 */
public class QcUsbIdentity {

    private static final String TAG = "QcUsbIdentity";

    private QcUsbIdentity() {
    }

    /**
     * USB 介质硬件识别结果
     */
    public static class UsbHardwareInfo {
        public final String hardwareId;    // 规范化的 USB 硬件身份，例如 "USB-HW-090C-1000-AA0401270001"
        public final String serialNumber;  // 物理芯片出厂序列号
        public final int vendorId;         // VID
        public final int productId;        // PID
        public final boolean isHardwareSerialReliable; // 序列号是否为高可靠硬件出厂码

        public UsbHardwareInfo(String hardwareId, String serialNumber, int vendorId, int productId, boolean isHardwareSerialReliable) {
            this.hardwareId = hardwareId;
            this.serialNumber = serialNumber;
            this.vendorId = vendorId;
            this.productId = productId;
            this.isHardwareSerialReliable = isHardwareSerialReliable;
        }

        @Override
        public String toString() {
            return "UsbHardwareInfo{" +
                    "hardwareId='" + hardwareId + '\'' +
                    ", serialNumber='" + serialNumber + '\'' +
                    ", vendorId=0x" + Integer.toHexString(vendorId) +
                    ", productId=0x" + Integer.toHexString(productId) +
                    ", isReliable=" + isHardwareSerialReliable +
                    '}';
        }
    }

    /**
     * 根据 USB 挂载路径获取当前插入的 USB 介质硬件物理指纹
     *
     * @param context         Android 上下文
     * @param usbMountFolder  USB 上的任意子路径（例如 /mnt/usb_storage/USB_DISK0/QC-Video）
     * @return 提取到的 USB 硬件身份对象
     */
    public static UsbHardwareInfo getUsbHardwareInfo(Context context, File usbMountFolder) {
        String sysfsSerial = findSerialFromSysfs(usbMountFolder);
        int vid = 0;
        int pid = 0;

        // 尝试通过 Android UsbManager 枚举外部大容量存储设备
        if (context != null) {
            try {
                UsbManager usbManager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
                if (usbManager != null) {
                    HashMap<String, UsbDevice> deviceList = usbManager.getDeviceList();
                    if (deviceList != null && !deviceList.isEmpty()) {
                        for (UsbDevice device : deviceList.values()) {
                            // 查找大容量存储类设备 (Interface Class 8: USB Mass Storage, 或设备自身类)
                            boolean isMassStorage = false;
                            for (int i = 0; i < device.getInterfaceCount(); i++) {
                                if (device.getInterface(i).getInterfaceClass() == 8) {
                                    isMassStorage = true;
                                    break;
                                }
                            }

                            if (isMassStorage || deviceList.size() == 1) {
                                vid = device.getVendorId();
                                pid = device.getProductId();
                                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                                    String devSerial = device.getSerialNumber();
                                    if (!TextUtils.isEmpty(devSerial) && !"unknown".equalsIgnoreCase(devSerial)) {
                                        sysfsSerial = devSerial.trim();
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            } catch (Throwable t) {
                Log.w(TAG, "通过 UsbManager 获取 USB 硬件信息异常: " + t.getMessage());
            }
        }

        // 综合校验 Serial 的可靠性
        boolean reliable = true;
        if (TextUtils.isEmpty(sysfsSerial) || "unknown".equalsIgnoreCase(sysfsSerial) || sysfsSerial.length() < 4) {
            // 兜底降级方案：从 /sys/block 或 /proc/mounts 提取块设备 UUID/标识
            sysfsSerial = fallbackBlockIdentity(usbMountFolder);
            reliable = false;
        }

        String vidHex = String.format(Locale.US, "%04X", vid & 0xFFFF);
        String pidHex = String.format(Locale.US, "%04X", pid & 0xFFFF);
        String cleanSerial = sysfsSerial.replaceAll("[^a-zA-Z0-9_-]", "").toUpperCase(Locale.US);

        String hardwareId = "USB-HW-" + vidHex + "-" + pidHex + "-" + cleanSerial;
        Log.i(TAG, "成功读取 USB 物理硬件指纹: " + hardwareId + " (可靠出厂码=" + reliable + ")");

        return new UsbHardwareInfo(hardwareId, cleanSerial, vid, pid, reliable);
    }

    /**
     * 遍历 Linux /sys/bus/usb/devices 读取物理 USB 芯片的 serial 硬件节点
     */
    private static String findSerialFromSysfs(File folder) {
        File usbBus = new File("/sys/bus/usb/devices");
        if (!usbBus.exists() || !usbBus.isDirectory()) {
            return null;
        }

        File[] devDirs = usbBus.listFiles();
        if (devDirs == null) return null;

        for (File devDir : devDirs) {
            File serialFile = new File(devDir, "serial");
            if (serialFile.exists() && serialFile.canRead()) {
                String serial = readFirstLine(serialFile);
                if (!TextUtils.isEmpty(serial) && !"unknown".equalsIgnoreCase(serial)) {
                    // 确认该 USB 节点下存在 block 存储设备驱动 (如 1-1/1-1:1.0/hostX/targetX...)
                    if (hasStorageInterface(devDir)) {
                        return serial.trim();
                    }
                }
            }
        }
        return null;
    }

    private static boolean hasStorageInterface(File usbDevDir) {
        // 简单探测目录下是否存在 host* 或包含 bInterfaceClass 为 08 的子接口
        File[] subFiles = usbDevDir.listFiles();
        if (subFiles == null) return false;
        for (File sf : subFiles) {
            if (sf.getName().startsWith("host") || sf.getName().contains(":")) {
                File ifClass = new File(sf, "bInterfaceClass");
                if (ifClass.exists()) {
                    String cls = readFirstLine(ifClass);
                    if ("08".equals(cls)) { // 08 代表 Mass Storage
                        return true;
                    }
                }
            }
        }
        return true; // 若难以判断，默认返回 true
    }

    private static String fallbackBlockIdentity(File folder) {
        // 尝试从 /proc/mounts 找出该挂载点对应的设备节点
        String mountPath = folder.getAbsolutePath();
        String devNode = "";
        try (BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream("/proc/mounts"), StandardCharsets.UTF_8))) {
            String line;
            while ((line = br.readLine()) != null) {
                String[] parts = line.split("\\s+");
                if (parts.length >= 2) {
                    if (mountPath.startsWith(parts[1])) {
                        devNode = parts[0];
                    }
                }
            }
        } catch (Throwable t) {
            // ignore
        }

        if (!TextUtils.isEmpty(devNode)) {
            String devName = new File(devNode).getName(); // 例如 sda1
            // 尝试读取 sysfs 中的分区块 ID
            File uuidFile = new File("/sys/class/block/" + devName);
            if (uuidFile.exists()) {
                return devName.toUpperCase(Locale.US);
            }
        }

        return "GENERIC-USB-DEV";
    }

    private static String readFirstLine(File file) {
        try (BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream(file), StandardCharsets.UTF_8))) {
            return br.readLine();
        } catch (Throwable t) {
            return null;
        }
    }
}
