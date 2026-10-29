package com.join.qc;

import android.content.Context;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;

/**
 * QC USB 介质硬件身份采集器 (高可靠加固版)
 *
 * 核心机制：
 * 1. 深度采集插入系统的 USB Mass Storage (0x08) 物理芯片出厂序列号与 VID/PID。
 * 2. 多级探测链条：
 *    - 优先匹配：通过 /proc/mounts 精准定位挂载点对应的底层 block 设备 (如 sda/sdb)，
 *      再沿 sysfs (/sys/block/sda/device) 反向追溯到真实的 USB 总线节点，直接读取对应的 serial、idVendor、idProduct。
 *    - 次级匹配：遍历 /sys/bus/usb/devices，通过精准的 0x08 Mass Storage 接口过滤找出对应设备。
 *    - Android UsbManager 增强：带接口过滤（UsbConstants.USB_CLASS_MASS_STORAGE = 8），并打印详尽遍历诊断日志。
 * 3. 规避 Android 权限截断（如 device.getSerialNumber() 在未申请 USB 授权时返回 null 或 unknown），
 *    sysfs 直接读取硬件节点具有免弹窗权限优势。
 */
public class QcUsbIdentity {

    private static final String TAG = "QcUsbIdentity";

    private QcUsbIdentity() {
    }

    /**
     * USB 介质硬件识别结果
     */
    public static class UsbHardwareInfo {
        public final String hardwareId;    // 规范化的 USB 硬件身份，例如 "USB-HW-090C-2000-45345SDFS21200010001"
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
        Log.i(TAG, "==== 开始提取 USB 物理芯片硬件指纹 ====");
        Log.i(TAG, "输入挂载路径: " + (usbMountFolder != null ? usbMountFolder.getAbsolutePath() : "null"));

        String finalSerial = null;
        int finalVid = 0;
        int finalPid = 0;
        boolean reliable = false;

        // =====================================================================
        // 通道 1 (最精准)：从挂载路径反查 /sys/block/*/device 指向的 USB 物理节点
        // 挂载点 -> /dev/block/vold/public:8,1 或 /dev/block/sda1 -> /sys/block/sda -> USB 设备节点
        // =====================================================================
        SysfsUsbDev sysDev = findUsbFromMountFolder(usbMountFolder);
        if (sysDev != null && !TextUtils.isEmpty(sysDev.serial)) {
            Log.i(TAG, "[通道1-挂载溯源命中] 成功定位 USB 节点: " + sysDev.devDir.getName() +
                    ", VID=0x" + Integer.toHexString(sysDev.vid) +
                    ", PID=0x" + Integer.toHexString(sysDev.pid) +
                    ", SN=" + sysDev.serial);
            finalSerial = sysDev.serial;
            finalVid = sysDev.vid;
            finalPid = sysDev.pid;
            reliable = true;
        }

        // =====================================================================
        // 通道 2 (sysfs 全局搜索)：遍历 /sys/bus/usb/devices，只提取 Class 0x08 存储设备
        // =====================================================================
        if (TextUtils.isEmpty(finalSerial)) {
            Log.i(TAG, "[通道1未命中] 启动 [通道2-sysfs存储类扫描]...");
            SysfsUsbDev scannedDev = scanSysfsMassStorage();
            if (scannedDev != null && !TextUtils.isEmpty(scannedDev.serial)) {
                Log.i(TAG, "[通道2-sysfs扫描命中] 找到存储设备: " + scannedDev.devDir.getName() +
                        ", VID=0x" + Integer.toHexString(scannedDev.vid) +
                        ", PID=0x" + Integer.toHexString(scannedDev.pid) +
                        ", SN=" + scannedDev.serial);
                finalSerial = scannedDev.serial;
                finalVid = scannedDev.vid;
                finalPid = scannedDev.pid;
                reliable = true;
            }
        }

        // =====================================================================
        // 通道 3 (Android UsbManager)：带严密的 MassStorage(0x08) 过滤与详尽诊断日志
        // =====================================================================
        if (context != null) {
            Log.i(TAG, "启动 [通道3-UsbManager 枚举与诊断]...");
            try {
                UsbManager usbManager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
                if (usbManager != null) {
                    HashMap<String, UsbDevice> deviceList = usbManager.getDeviceList();
                    if (deviceList != null && !deviceList.isEmpty()) {
                        Log.i(TAG, "系统当前总线共检测到 " + deviceList.size() + " 个 USB 设备，开始详细诊断遍历:");
                        List<UsbDevice> storageCandidates = new ArrayList<>();

                        for (UsbDevice device : deviceList.values()) {
                            boolean hasMassStorageIf = false;
                            StringBuilder ifClasses = new StringBuilder("[");

                            for (int i = 0; i < device.getInterfaceCount(); i++) {
                                UsbInterface uif = device.getInterface(i);
                                int ifClass = uif.getInterfaceClass();
                                ifClasses.append(String.format("0x%02X ", ifClass));
                                if (ifClass == UsbConstants.USB_CLASS_MASS_STORAGE) { // 0x08
                                    hasMassStorageIf = true;
                                }
                            }
                            ifClasses.append("]");

                            String devSerial = "";
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                                try {
                                    devSerial = device.getSerialNumber();
                                } catch (SecurityException se) {
                                    devSerial = "<PERMISSION_DENIED>";
                                }
                            }

                            Log.i(TAG, String.format(Locale.US,
                                    "  -> 设备 [%s]: VID=0x%04X, PID=0x%04X, DevClass=0x%02X, Interfaces=%s, SN='%s', 是存储设备=%b",
                                    device.getDeviceName(), device.getVendorId(), device.getProductId(),
                                    device.getDeviceClass(), ifClasses.toString(), devSerial, hasMassStorageIf));

                            if (hasMassStorageIf) {
                                storageCandidates.add(device);
                            }
                        }

                        // 如果通过 UsbManager 找到了 MassStorage 候选设备
                        if (!storageCandidates.isEmpty()) {
                            // 优先取有合法序列号的，若无则取第一个
                            UsbDevice chosen = storageCandidates.get(0);
                            for (UsbDevice dev : storageCandidates) {
                                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                                    try {
                                        String sn = dev.getSerialNumber();
                                        if (!TextUtils.isEmpty(sn) && !"unknown".equalsIgnoreCase(sn)) {
                                            chosen = dev;
                                            break;
                                        }
                                    } catch (Throwable ignored) {
                                    }
                                }
                            }

                            Log.i(TAG, "UsbManager 锁定 Mass Storage 存储设备: " + chosen.getDeviceName() +
                                    ", VID=0x" + Integer.toHexString(chosen.getVendorId()) +
                                    ", PID=0x" + Integer.toHexString(chosen.getProductId()));

                            if (finalVid == 0) finalVid = chosen.getVendorId();
                            if (finalPid == 0) finalPid = chosen.getProductId();

                            if (TextUtils.isEmpty(finalSerial) && Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                                try {
                                    String devSerial = chosen.getSerialNumber();
                                    if (!TextUtils.isEmpty(devSerial) && !"unknown".equalsIgnoreCase(devSerial)) {
                                        finalSerial = devSerial.trim();
                                        reliable = true;
                                    }
                                } catch (Throwable t) {
                                    Log.w(TAG, "从 chosen UsbDevice 获取 SerialNumber 失败: " + t.getMessage());
                                }
                            }
                        } else {
                            Log.w(TAG, "UsbManager 遍历完毕，未发现任何 interfaceClass == 0x08 的设备！");
                        }
                    } else {
                        Log.w(TAG, "UsbManager.getDeviceList() 为空或无权限返回！");
                    }
                }
            } catch (Throwable t) {
                Log.w(TAG, "通过 UsbManager 获取 USB 硬件信息异常: " + t.getMessage());
            }
        }

        // =====================================================================
        // 通道 4：终极兜底降级（当所有渠道都无法提取到芯片 serial 时）
        // =====================================================================
        if (TextUtils.isEmpty(finalSerial) || "unknown".equalsIgnoreCase(finalSerial) || finalSerial.length() < 4) {
            finalSerial = fallbackBlockIdentity(usbMountFolder);
            reliable = false;
            Log.w(TAG, "警告：未能提取到 USB 物理芯片序列号，降级使用块设备标识: " + finalSerial);
        }

        String vidHex = String.format(Locale.US, "%04X", finalVid & 0xFFFF);
        String pidHex = String.format(Locale.US, "%04X", finalPid & 0xFFFF);
        String cleanSerial = finalSerial.replaceAll("[^a-zA-Z0-9_-]", "").toUpperCase(Locale.US);

        String hardwareId = "USB-HW-" + vidHex + "-" + pidHex + "-" + cleanSerial;
        Log.i(TAG, "==== 最终确定的 USB 物理硬件指纹: " + hardwareId + " (高可靠芯片出厂码=" + reliable + ") ====");

        return new UsbHardwareInfo(hardwareId, cleanSerial, finalVid, finalPid, reliable);
    }

    /**
     * 内部承载 sysfs 解析出的 USB 设备属性
     */
    private static class SysfsUsbDev {
        File devDir;
        int vid;
        int pid;
        String serial;

        SysfsUsbDev(File devDir, int vid, int pid, String serial) {
            this.devDir = devDir;
            this.vid = vid;
            this.pid = pid;
            this.serial = serial;
        }
    }

    /**
     * 根据挂载文件夹反查 /sys/block/* 对应的 USB 底层设备节点
     */
    private static SysfsUsbDev findUsbFromMountFolder(File folder) {
        if (folder == null) return null;
        String mountPath = folder.getAbsolutePath();
        String devNode = null;

        // 1. 从 /proc/mounts 找出该挂载点对应的设备节点 (如 /dev/block/vold/public:8,1 或 /dev/block/sda1)
        try (BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream("/proc/mounts"), StandardCharsets.UTF_8))) {
            String line;
            while ((line = br.readLine()) != null) {
                String[] parts = line.split("\\s+");
                if (parts.length >= 2) {
                    if (mountPath.startsWith(parts[1]) && parts[0].startsWith("/dev/")) {
                        devNode = parts[0];
                    }
                }
            }
        } catch (Throwable t) {
            Log.w(TAG, "读取 /proc/mounts 异常: " + t.getMessage());
        }

        if (TextUtils.isEmpty(devNode)) {
            return null;
        }

        Log.i(TAG, "从挂载点匹配到底层设备节点: " + devNode);

        // 2. 尝试从设备节点名 (如 sda1 -> sda) 追溯 /sys/class/block/sda/device
        String nodeName = new File(devNode).getName();
        String blockName = nodeName.replaceAll("\\d+$", ""); // sda1 -> sda
        File blockDir = new File("/sys/class/block/" + blockName);
        if (!blockDir.exists()) {
            blockDir = new File("/sys/class/block/" + nodeName);
        }

        if (blockDir.exists()) {
            try {
                // 读取 canonicalPath 追溯父级 USB 目录
                File cur = blockDir.getCanonicalFile();
                while (cur != null && !"/".equals(cur.getAbsolutePath())) {
                    File idVendor = new File(cur, "idVendor");
                    File idProduct = new File(cur, "idProduct");
                    File serial = new File(cur, "serial");

                    if (idVendor.exists() && idProduct.exists() && serial.exists()) {
                        String vidStr = readFirstLine(idVendor);
                        String pidStr = readFirstLine(idProduct);
                        String snStr = readFirstLine(serial);
                        int vid = parseHex(vidStr);
                        int pid = parseHex(pidStr);
                        if (!TextUtils.isEmpty(snStr) && !"unknown".equalsIgnoreCase(snStr)) {
                            return new SysfsUsbDev(cur, vid, pid, snStr.trim());
                        }
                    }
                    cur = cur.getParentFile();
                }
            } catch (Throwable t) {
                Log.w(TAG, "追溯 /sys/class/block canonical 路径异常: " + t.getMessage());
            }
        }
        return null;
    }

    /**
     * 遍历 /sys/bus/usb/devices，通过 0x08 存储类接口寻找插入的 U 盘
     */
    private static SysfsUsbDev scanSysfsMassStorage() {
        File usbBus = new File("/sys/bus/usb/devices");
        if (!usbBus.exists() || !usbBus.isDirectory()) {
            return null;
        }

        File[] devDirs = usbBus.listFiles();
        if (devDirs == null) return null;

        for (File devDir : devDirs) {
            File idVendor = new File(devDir, "idVendor");
            File idProduct = new File(devDir, "idProduct");
            File serialFile = new File(devDir, "serial");

            if (idVendor.exists() && idProduct.exists() && serialFile.exists() && serialFile.canRead()) {
                String sn = readFirstLine(serialFile);
                if (TextUtils.isEmpty(sn) || "unknown".equalsIgnoreCase(sn)) {
                    continue;
                }

                // 严密检查：该 USB 设备下必须包含 Class 08 (Mass Storage) 接口
                if (hasStorageInterface(devDir)) {
                    int vid = parseHex(readFirstLine(idVendor));
                    int pid = parseHex(readFirstLine(idProduct));
                    return new SysfsUsbDev(devDir, vid, pid, sn.trim());
                }
            }
        }
        return null;
    }

    /**
     * 严格检查该 USB 设备目录下是否存在 bInterfaceClass == 08 的子接口节点或 host* 驱动节点
     */
    private static boolean hasStorageInterface(File usbDevDir) {
        File[] subFiles = usbDevDir.listFiles();
        if (subFiles == null) return false;

        boolean foundMassStorageClass = false;
        boolean foundHostDir = false;

        for (File sf : subFiles) {
            String name = sf.getName();
            // 子接口格式通常形如 3-3.2:1.0
            if (name.contains(":")) {
                File ifClass = new File(sf, "bInterfaceClass");
                if (ifClass.exists()) {
                    String cls = readFirstLine(ifClass);
                    // 08 代表 Mass Storage 大容量存储类
                    if ("08".equalsIgnoreCase(cls)) {
                        foundMassStorageClass = true;
                        break;
                    }
                }
            } else if (name.startsWith("host")) {
                foundHostDir = true;
            }
        }
        return foundMassStorageClass || foundHostDir;
    }

    private static String fallbackBlockIdentity(File folder) {
        if (folder == null) return "FF500000USB";
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
        } catch (Throwable ignored) {
        }

        if (!TextUtils.isEmpty(devNode)) {
            String devName = new File(devNode).getName(); // 例如 sda1
            File uuidFile = new File("/sys/class/block/" + devName);
            if (uuidFile.exists()) {
                return devName.toUpperCase(Locale.US);
            }
        }
        return "FF500000USB";
    }

    private static int parseHex(String s) {
        if (TextUtils.isEmpty(s)) return 0;
        try {
            return Integer.parseInt(s.trim(), 16);
        } catch (Exception e) {
            return 0;
        }
    }

    private static String readFirstLine(File file) {
        try (BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream(file), StandardCharsets.UTF_8))) {
            String line = br.readLine();
            return line != null ? line.trim() : null;
        } catch (Throwable t) {
            return null;
        }
    }
}
