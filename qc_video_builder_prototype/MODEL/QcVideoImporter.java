package com.isperm.qc;

import android.os.Handler;
import android.os.Looper;
import android.util.Base64;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.KeyFactory;
import java.security.PublicKey;
import java.security.Signature;
import java.security.spec.X509EncodedKeySpec;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/**
 * iSperm Medical QC Video 解密与导入核心引擎
 * ========================================
 * 运行在安卓系统精子分析仪主控 APP 中。
 * 
 * 核心流程：
 * 1. 自动定位或接收用户选中的 QC-Video/Video-1 文件夹。
 * 2. 验证 manifest.dat 与 license.dat 的公司数字签名 (SHA256withRSA)，防止仿冒与非法U盘。
 * 3. 使用主控 APP 内置的 APP_MASTER_KEY 解封 AES 内容密钥 (Key Envelope Unwrap)。
 * 4. 采用 AES-256-GCM 工业级认证解密 Video-1.qcv 密文包。
 * 5. 将解密出来的 30 张 JPEG 图像精准解压释放到 /mnt/internal_sd/QC/Video-1/000.jpg ~ 029.jpg。
 * 
 * 兼容性：
 * - 纯 Android 原生 SDK (API 22+ 完美兼容)，无需额外第三方依赖。
 */
public class QcVideoImporter {

    private static final String TAG = "QcVideoImporter";
    private static final ExecutorService EXECUTOR = Executors.newSingleThreadExecutor();
    private static final Handler MAIN_HANDLER = new Handler(Looper.getMainLooper());

    /**
     * 导入结果回调接口
     */
    public interface ImportCallback {
        void onProgress(String statusMessage, int progressPercent);
        void onSuccess(ImportResult result);
        void onError(String errorMessage, Throwable throwable);
    }

    /**
     * 导入成功结果数据类
     */
    public static class ImportResult {
        public final String videoName;
        public final File outputDirectory;
        public final int frameCount;
        public final long elapsedTimeMs;

        public ImportResult(String videoName, File outputDirectory, int frameCount, long elapsedTimeMs) {
            this.videoName = videoName;
            this.outputDirectory = outputDirectory;
            this.frameCount = frameCount;
            this.elapsedTimeMs = elapsedTimeMs;
        }

        @Override
        public String toString() {
            return "ImportResult{" +
                    "videoName='" + videoName + '\'' +
                    ", outputDirectory=" + outputDirectory.getAbsolutePath() +
                    ", frameCount=" + frameCount +
                    ", elapsedTimeMs=" + elapsedTimeMs +
                    '}';
        }
    }

    /**
     * 异步导入指定的视频文件夹（如 U盘中的 QC-Video/Video-1）
     * 
     * @param videoFolder   用户选择的源文件夹（如 /mnt/usb_storage/QC-Video/Video-1）
     * @param targetRootDir 目标根目录（传入 null 则默认 /mnt/internal_sd/QC）
     * @param callback      回调监听器
     */
    public static void importVideoAsync(final File videoFolder, final File targetRootDir, final ImportCallback callback) {
        EXECUTOR.execute(new Runnable() {
            @Override
            public void run() {
                try {
                    postProgress(callback, "正在准备读取视频包...", 10);
                    ImportResult result = importVideoSync(videoFolder, targetRootDir, callback);
                    postSuccess(callback, result);
                } catch (final Exception e) {
                    Log.e(TAG, "QC 视频导入失败: " + e.getMessage(), e);
                    postError(callback, e.getMessage(), e);
                }
            }
        });
    }

    /**
     * 同步导入指定的视频文件夹
     * 
     * @param videoFolder   包含加密视频的文件夹 (例如 .../QC-Video/Video-1)
     * @param targetRootDir 写入的目标根目录 (例如 /mnt/internal_sd/QC)
     * @param callback      可选进度回调
     * @return ImportResult 导入成功信息
     * @throws Exception 校验不通过或解密失败异常
     */
    public static ImportResult importVideoSync(File videoFolder, File targetRootDir, ImportCallback callback) throws Exception {
        long startTime = System.currentTimeMillis();

        if (videoFolder == null || !videoFolder.exists() || !videoFolder.isDirectory()) {
            throw new IllegalArgumentException("选择的视频文件夹无效或不存在：" + videoFolder);
        }

        String videoName = videoFolder.getName();
        Log.i(TAG, "开始导入 QC 视频包: " + videoFolder.getAbsolutePath());

        // 1. 查找加密文件 (.qcv)
        File qcvFile = findQcvFile(videoFolder);
        if (qcvFile == null) {
            throw new IOException("文件夹内未找到加密视频文件 (*.qcv)：" + videoFolder.getAbsolutePath());
        }

        // 2. 查找并解析 manifest.dat 和 license.dat (优先子目录，其次父目录)
        File manifestFile = new File(videoFolder, "manifest.dat");
        File licenseFile = new File(videoFolder, "license.dat");
        if (!manifestFile.exists() || !licenseFile.exists()) {
            File parentDir = videoFolder.getParentFile();
            if (parentDir != null) {
                File parentManifest = new File(parentDir, "manifest.dat");
                File parentLicense = new File(parentDir, "license.dat");
                if (parentManifest.exists() && parentLicense.exists()) {
                    manifestFile = parentManifest;
                    licenseFile = parentLicense;
                }
            }
        }

        if (!manifestFile.exists() || !licenseFile.exists()) {
            throw new SecurityException("缺少授权证书或清单文件 (manifest.dat / license.dat)，拒绝导入！");
        }

        postProgress(callback, "正在验证公司官方数字签名与合法授权...", 30);

        // 3. 校验官方数字签名 (防止任何用户自行制作假U盘)
        byte[] manifestBytes = readFileBytes(manifestFile);
        String licenseJsonStr = new String(readFileBytes(licenseFile), StandardCharsets.UTF_8);
        JSONObject licenseJson = new JSONObject(licenseJsonStr);
        String signatureB64 = licenseJson.getString("signature_base64");
        byte[] signatureBytes = Base64.decode(signatureB64, Base64.DEFAULT);

        boolean signatureValid = verifyCompanySignature(manifestBytes, signatureBytes);
        if (!signatureValid) {
            throw new SecurityException("【安全拦截】QC 授权数字签名验证失败！该 U 盘未经官方授权或文件已被非法篡改。");
        }
        Log.i(TAG, "数字签名验证通过！合法授权 U 盘。");

        postProgress(callback, "正在解封安全密钥信封...", 50);

        // 4. 解析清单并提取该视频的密钥信封 (Key Envelope)
        String manifestJsonStr = new String(manifestBytes, StandardCharsets.UTF_8);
        JSONObject manifestJson = new JSONObject(manifestJsonStr);
        String usbId = manifestJson.getString("usb_id");
        JSONArray videosArray = manifestJson.getJSONArray("videos");

        JSONObject targetVideoObj = null;
        for (int i = 0; i < videosArray.length(); i++) {
            JSONObject vObj = videosArray.getJSONObject(i);
            String vName = vObj.getString("video_name");
            if (vName.equalsIgnoreCase(videoName) || vObj.getString("file").equalsIgnoreCase(qcvFile.getName())) {
                targetVideoObj = vObj;
                break;
            }
        }

        // 如果在列表中没有精确匹配但视频列表只有1个，则直接使用
        if (targetVideoObj == null && videosArray.length() == 1) {
            targetVideoObj = videosArray.getJSONObject(0);
        }

        if (targetVideoObj == null) {
            throw new IllegalArgumentException("清单中未找到视频 " + videoName + " 的授权信息！");
        }

        String envelopeB64 = targetVideoObj.getString("key_envelope");
        byte[] envelopePayload = Base64.decode(envelopeB64, Base64.DEFAULT);

        // 5. 使用 APP_MASTER_KEY 解封 AES 内容密钥
        byte[] contentKey = unwrapContentKey(envelopePayload, videoName, usbId);

        postProgress(callback, "正在执行 AES-256 工业级硬件认证解密...", 70);

        // 6. 解密 QCV 视频包
        byte[] qcvBytes = readFileBytes(qcvFile);
        byte[] zipBytes = decryptQcvPayload(qcvBytes, contentKey, videoName, usbId);

        postProgress(callback, "正在释放 30 张 QC 帧图片到系统内部...", 85);

        // 7. 确定目标路径并释放 30 张图片
        if (targetRootDir == null) {
            targetRootDir = new File(QcConstants.DEFAULT_INTERNAL_QC_PATH);
        }
        File targetVideoDir = new File(targetRootDir, videoName);
        if (!targetVideoDir.exists() && !targetVideoDir.mkdirs()) {
            // 如果 /mnt/internal_sd 失败，尝试备用外部存储目录
            Log.w(TAG, "创建目标目录失败: " + targetVideoDir.getAbsolutePath() + "，尝试备用内部存储...");
            File fallbackBase = new File("/sdcard/QC");
            targetVideoDir = new File(fallbackBase, videoName);
            if (!targetVideoDir.exists() && !targetVideoDir.mkdirs()) {
                throw new IOException("无法创建输出目录：" + targetVideoDir.getAbsolutePath());
            }
        }

        int extractedCount = extractZipToDirectory(zipBytes, targetVideoDir);
        if (extractedCount < QcConstants.EXPECTED_FRAME_COUNT) {
            throw new IOException("解压出的图片数量不足 30 张 (实际: " + extractedCount + ")");
        }

        postProgress(callback, "导入完成！", 100);
        long elapsedTime = System.currentTimeMillis() - startTime;
        Log.i(TAG, "成功导入 QC 视频 [" + videoName + "] 到 " + targetVideoDir.getAbsolutePath() + "，耗时 " + elapsedTime + " ms");

        return new ImportResult(videoName, targetVideoDir, extractedCount, elapsedTime);
    }

    /**
     * 验证公司官方数字签名 (SHA256withRSA)
     */
    private static boolean verifyCompanySignature(byte[] data, byte[] signatureBytes) {
        try {
            byte[] pubKeyDer = Base64.decode(QcConstants.COMPANY_PUBLIC_KEY_B64, Base64.DEFAULT);
            X509EncodedKeySpec spec = new X509EncodedKeySpec(pubKeyDer);
            KeyFactory kf = KeyFactory.getInstance("RSA");
            PublicKey publicKey = kf.generatePublic(spec);

            Signature signature = Signature.getInstance("SHA256withRSA");
            signature.initVerify(publicKey);
            signature.update(data);
            return signature.verify(signatureBytes);
        } catch (Exception e) {
            Log.e(TAG, "签名验证异常: " + e.getMessage(), e);
            return false;
        }
    }

    /**
     * 解封 AES 内容密钥 (Key Envelope Unwrap)
     */
    private static byte[] unwrapContentKey(byte[] envelopePayload, String videoName, String usbId) throws Exception {
        if (envelopePayload.length < 12 + 16) {
            throw new IllegalArgumentException("密钥信封长度非法");
        }
        byte[] nonce = Arrays.copyOfRange(envelopePayload, 0, 12);
        byte[] ciphertext = Arrays.copyOfRange(envelopePayload, 12, envelopePayload.length);

        byte[] masterKeyBytes = Base64.decode(QcConstants.APP_MASTER_KEY_B64, Base64.DEFAULT);
        SecretKeySpec masterSpec = new SecretKeySpec(masterKeyBytes, "AES");

        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        GCMParameterSpec spec = new GCMParameterSpec(128, nonce);
        cipher.init(Cipher.DECRYPT_MODE, masterSpec, spec);

        byte[] aad = ("QC-KEY-ENVELOPE|" + videoName + "|" + usbId).getBytes(StandardCharsets.UTF_8);
        cipher.updateAAD(aad);

        return cipher.doFinal(ciphertext);
    }

    /**
     * 解密 QCV 视频包
     */
    private static byte[] decryptQcvPayload(byte[] qcvBytes, byte[] contentKey, String videoName, String usbId) throws Exception {
        if (qcvBytes.length < 4 + 12 + 16) {
            throw new IllegalArgumentException("QCV 文件大小过小，可能已损坏");
        }

        // 校验文件魔数 QCV2
        String magic = new String(Arrays.copyOfRange(qcvBytes, 0, 4), StandardCharsets.US_ASCII);
        if (!QcConstants.MAGIC_QCV.equals(magic)) {
            throw new IllegalArgumentException("未知的文件格式魔数：" + magic + "，必须为 QCV2！");
        }

        byte[] nonce = Arrays.copyOfRange(qcvBytes, 4, 16);
        byte[] ciphertext = Arrays.copyOfRange(qcvBytes, 16, qcvBytes.length);

        SecretKeySpec contentSpec = new SecretKeySpec(contentKey, "AES");
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        GCMParameterSpec spec = new GCMParameterSpec(128, nonce);
        cipher.init(Cipher.DECRYPT_MODE, contentSpec, spec);

        byte[] aad = ("QCV2|" + videoName + "|" + usbId).getBytes(StandardCharsets.UTF_8);
        cipher.updateAAD(aad);

        return cipher.doFinal(ciphertext);
    }

    /**
     * 解压 ZIP 流中的 30 张图片并安全写入目标文件夹
     */
    private static int extractZipToDirectory(byte[] zipBytes, File targetDir) throws IOException {
        int count = 0;
        try (ZipInputStream zis = new ZipInputStream(new ByteArrayInputStream(zipBytes))) {
            ZipEntry entry;
            byte[] buffer = new byte[8192];
            while ((entry = zis.getNextEntry()) != null) {
                if (entry.isDirectory()) {
                    continue;
                }
                String name = new File(entry.getName()).getName();
                // 仅解压 000.jpg ~ 029.jpg 等图片格式
                if (name.toLowerCase().endsWith(".jpg") || name.toLowerCase().endsWith(".jpeg")) {
                    File outFile = new File(targetDir, name);
                    try (FileOutputStream fos = new FileOutputStream(outFile)) {
                        int len;
                        while ((len = zis.read(buffer)) > 0) {
                            fos.write(buffer, 0, len);
                        }
                    }
                    count++;
                }
                zis.closeEntry();
            }
        }
        return count;
    }

    /**
     * 辅助查找文件夹内的 .qcv 文件
     */
    private static File findQcvFile(File folder) {
        File[] files = folder.listFiles();
        if (files == null) return null;
        for (File f : files) {
            if (f.isFile() && f.getName().toLowerCase().endsWith(".qcv")) {
                return f;
            }
        }
        return null;
    }

    /**
     * 读取整个文件的字节流
     */
    private static byte[] readFileBytes(File file) throws IOException {
        try (InputStream in = new FileInputStream(file);
             ByteArrayOutputStream out = new ByteArrayOutputStream((int) file.length())) {
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) != -1) {
                out.write(buf, 0, n);
            }
            return out.toByteArray();
        }
    }

    private static void postProgress(final ImportCallback cb, final String msg, final int p) {
        if (cb != null) {
            MAIN_HANDLER.post(new Runnable() {
                @Override
                public void run() {
                    cb.onProgress(msg, p);
                }
            });
        }
    }

    private static void postSuccess(final ImportCallback cb, final ImportResult res) {
        if (cb != null) {
            MAIN_HANDLER.post(new Runnable() {
                @Override
                public void run() {
                    cb.onSuccess(res);
                }
            });
        }
    }

    private static void postError(final ImportCallback cb, final String msg, final Throwable t) {
        if (cb != null) {
            MAIN_HANDLER.post(new Runnable() {
                @Override
                public void run() {
                    cb.onError(msg, t);
                }
            });
        }
    }
}
