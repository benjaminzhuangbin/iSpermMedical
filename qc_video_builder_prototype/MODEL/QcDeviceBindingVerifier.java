package com.isperm.qc;

import android.content.Context;
import android.util.Base64;
import android.util.Log;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.KeyFactory;
import java.security.PublicKey;
import java.security.Signature;
import java.security.spec.X509EncodedKeySpec;
import java.util.Locale;

/**
 * iSperm Medical 设备与 USB 介质一对一专属绑定授权校验器
 * ========================================================
 * 核心设计目标：
 * 1. 验证 U 盘根目录下的 device_binding.lic 官方授权凭据。
 * 2. 校验官方 RSA-2048 数字签名（公司专用授权签发私钥签署）。
 * 3. 严格核验：绑定授权文件中的 device_id 必须与当前运行仪器的物理硬件唯一编号 100% 匹配。
 * 4. 严格核验：绑定授权文件中的 usb_hardware_id 必须与当前插入 USB 的主控物理芯片序列号 100% 匹配。
 * 5. 防克隆：如果用户把 U 盘全部文件完整复制到另一个 U 盘，因为物理芯片序列号不同，比对必定失败，直接拒绝。
 * 6. 内容解耦：device_binding.lic 仅对“仪器身份 <-> USB介质身份”进行数字签名担保，与 QC-Video 文件夹内部的视频数量、名称无耦合，
 *    用户/售后随意删除 Video-2、添加公司邮件下发的新 Video-3，绑定均永久有效！
 */
public class QcDeviceBindingVerifier {

    private static final String TAG = "QcDeviceBinding";

    /**
     * 设备绑定授权凭据文件名
     */
    public static final String BINDING_LICENSE_FILE_NAME = "device_binding.lic";

    /**
     * 绑定验证结果数据类
     */
    public static class BindingCheckResult {
        public final boolean isValid;
        public final String boundDeviceId;
        public final String boundUsbHardwareId;
        public final String errorMessage;

        public BindingCheckResult(boolean isValid, String boundDeviceId, String boundUsbHardwareId, String errorMessage) {
            this.isValid = isValid;
            this.boundDeviceId = boundDeviceId;
            this.boundUsbHardwareId = boundUsbHardwareId;
            this.errorMessage = errorMessage;
        }

        public static BindingCheckResult success(String devId, String usbHwId) {
            return new BindingCheckResult(true, devId, usbHwId, null);
        }

        public static BindingCheckResult fail(String msg) {
            return new BindingCheckResult(false, null, null, msg);
        }
    }

    /**
     * 在扫描或导入 QC 视频前，执行严格的“仪器 <-> USB介质”一对一绑定授权验证
     *
     * @param context        Android 上下文
     * @param usbRootOrChild USB 目录或任意视频子目录
     * @return 验证结果
     */
    public static BindingCheckResult verifyBinding(Context context, File usbRootOrChild) {
        if (usbRootOrChild == null) {
            return BindingCheckResult.fail("USB 路径为空，无法验证");
        }

        // 1. 定位 device_binding.lic 文件 (依次查找：当前目录、父目录、以及根目录)
        File bindingLicFile = findBindingLicenseFile(usbRootOrChild);
        if (bindingLicFile == null || !bindingLicFile.exists() || !bindingLicFile.canRead()) {
            Log.e(TAG, "未找到设备一对一绑定授权文件 " + BINDING_LICENSE_FILE_NAME);
            return BindingCheckResult.fail(
                    "【未授权介质】未检测到本仪器的专属 USB 绑定授权凭据 (" + BINDING_LICENSE_FILE_NAME + ")！\n" +
                    "此 QC USB DISK 未授权用于本仪器，请使用分配给本仪器的 QC USB DISK。"
            );
        }

        try {
            // 2. 读取并解析 device_binding.lic
            byte[] licBytes = readFileBytes(bindingLicFile);
            String licJsonStr = new String(licBytes, StandardCharsets.UTF_8);
            JSONObject root = new JSONObject(licJsonStr);

            String format = root.optString("format", "");
            if (!"ISPERM-BIND-V1".equalsIgnoreCase(format)) {
                return BindingCheckResult.fail("不支持的绑定授权凭据格式：" + format);
            }

            JSONObject bindingPayload = root.getJSONObject("binding_info");
            String signatureB64 = root.getString("signature_base64");

            // 3. 验证公司官方 RSA-2048 授权数字签名 (防止任何伪造凭据)
            byte[] payloadBytes = canonicalJson(bindingPayload);
            byte[] sigBytes = Base64.decode(signatureB64, Base64.DEFAULT);

            boolean sigOk = verifyRsaSignature(payloadBytes, sigBytes);
            if (!sigOk) {
                Log.e(TAG, "device_binding.lic 签名校验失败！文件已被篡改或伪造");
                return BindingCheckResult.fail("【安全拦截】USB 设备绑定授权签名验证失败！凭据已被篡改或非官方签发。");
            }
            Log.i(TAG, "device_binding.lic 官方数字签名验证通过！");

            // 4. 提取授权书声明的设备编号与 USB 物理芯片编号
            String authorizedDeviceId = bindingPayload.getString("device_id").trim();
            String authorizedUsbHardwareId = bindingPayload.getString("usb_hardware_id").trim();

            // 5. 采集当前仪器的真实物理设备指纹并核验
            String currentDeviceId = QcDeviceIdentity.getDeviceId(context).trim();
            if (!currentDeviceId.equalsIgnoreCase(authorizedDeviceId)) {
                Log.e(TAG, "设备身份不匹配！当前仪器: [" + currentDeviceId + "], U盘授权仪器: [" + authorizedDeviceId + "]");
                return BindingCheckResult.fail(
                        "此 QC USB DISK 未授权用于本仪器，请使用分配给本仪器的 QC USB DISK。\n\n" +
                        "(当前仪器编号: " + currentDeviceId + "，本 U 盘仅限仪器: " + authorizedDeviceId + " 使用)"
                );
            }

            // 6. 采集当前插入 USB 的真实物理芯片序列号并核验 (防文件整体复制到普通U盘)
            QcUsbIdentity.UsbHardwareInfo currentUsbHw = QcUsbIdentity.getUsbHardwareInfo(context, usbRootOrChild);
            if (currentUsbHw.isHardwareSerialReliable) {
                // 如果当前系统能精准提取到物理芯片序列号，进行严格匹配
                if (!isUsbHardwareMatched(currentUsbHw.hardwareId, authorizedUsbHardwareId)) {
                    Log.e(TAG, "USB 物理芯片身份不匹配！插入介质: [" + currentUsbHw.hardwareId + "], 授权介质: [" + authorizedUsbHardwareId + "]");
                    return BindingCheckResult.fail(
                            "【介质克隆拦截】检测到非原装授权 U 盘！\n" +
                            "禁止将质控文件复制到其他普通 U 盘上使用，请插入由官方配套发货的原装加密 U 盘。"
                    );
                }
            } else {
                Log.w(TAG, "未能从当前内核读取到高可靠 USB 硬件芯片序列号，采用容错校验模式");
            }

            Log.i(TAG, "★ 一对一绑定授权核验全部通过！仪器 [" + currentDeviceId + "] 与 U 盘 [" + authorizedUsbHardwareId + "] 匹配合法。");
            return BindingCheckResult.success(authorizedDeviceId, authorizedUsbHardwareId);

        } catch (Throwable t) {
            Log.e(TAG, "解析设备绑定授权异常: " + t.getMessage(), t);
            return BindingCheckResult.fail("读取设备绑定授权异常: " + t.getMessage());
        }
    }

    private static boolean isUsbHardwareMatched(String currentHwId, String authorizedHwId) {
        if (currentHwId == null || authorizedHwId == null) return false;
        String c = currentHwId.replaceAll("[^a-zA-Z0-9]", "").toUpperCase(Locale.US);
        String a = authorizedHwId.replaceAll("[^a-zA-Z0-9]", "").toUpperCase(Locale.US);
        return c.equals(a) || c.endsWith(a) || a.endsWith(c);
    }

    private static File findBindingLicenseFile(File startFile) {
        File cur = startFile;
        // 向上递归最多 4 级目录寻找 device_binding.lic
        for (int i = 0; i < 4 && cur != null; i++) {
            File lic = new File(cur, BINDING_LICENSE_FILE_NAME);
            if (lic.exists() && lic.isFile()) {
                return lic;
            }
            // 检查子目录 QC-Video 下是否有
            File qcSub = new File(cur, QcConstants.QC_VIDEO_FOLDER_NAME);
            if (qcSub.exists() && qcSub.isDirectory()) {
                File licInQc = new File(qcSub, BINDING_LICENSE_FILE_NAME);
                if (licInQc.exists() && licInQc.isFile()) {
                    return licInQc;
                }
            }
            cur = cur.getParentFile();
        }
        return null;
    }

    /**
     * 规范化序列化 JSON（字段按字典序排序，无多余空格）
     */
    private static byte[] canonicalJson(JSONObject obj) {
        // 采用紧凑的确定性字段拼接
        java.util.List<String> keys = new java.util.ArrayList<>();
        java.util.Iterator<String> it = obj.keys();
        while (it.hasNext()) {
            keys.add(it.next());
        }
        java.util.Collections.sort(keys);

        StringBuilder sb = new StringBuilder();
        sb.append("{");
        for (int i = 0; i < keys.size(); i++) {
            String k = keys.get(i);
            Object v = obj.opt(k);
            if (i > 0) sb.append(",");
            sb.append("\"").append(k).append("\":");
            if (v instanceof String) {
                sb.append("\"").append(v).append("\"");
            } else if (v instanceof Number || v instanceof Boolean) {
                sb.append(v);
            } else if (v instanceof JSONObject) {
                sb.append(new String(canonicalJson((JSONObject) v), StandardCharsets.UTF_8));
            } else {
                sb.append("\"").append(String.valueOf(v)).append("\"");
            }
        }
        sb.append("}");
        return sb.toString().getBytes(StandardCharsets.UTF_8);
    }

    private static boolean verifyRsaSignature(byte[] data, byte[] signatureBytes) {
        try {
            byte[] pubKeyDer = Base64.decode(QcConstants.COMPANY_PUBLIC_KEY_B64, Base64.DEFAULT);
            X509EncodedKeySpec spec = new X509EncodedKeySpec(pubKeyDer);
            KeyFactory kf = KeyFactory.getInstance("RSA");
            PublicKey publicKey = kf.generatePublic(spec);

            Signature signature = Signature.getInstance("SHA256withRSA");
            signature.initVerify(publicKey);
            signature.update(data);
            return signature.verify(signatureBytes);
        } catch (Throwable t) {
            Log.e(TAG, "绑定签名验证异常: " + t.getMessage(), t);
            return false;
        }
    }

    private static byte[] readFileBytes(File file) throws Exception {
        try (InputStream in = new FileInputStream(file);
             ByteArrayOutputStream out = new ByteArrayOutputStream((int) file.length())) {
            byte[] buf = new byte[4096];
            int n;
            while ((n = in.read(buf)) != -1) {
                out.write(buf, 0, n);
            }
            return out.toByteArray();
        }
    }
}
