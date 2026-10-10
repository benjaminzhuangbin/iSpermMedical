package com.isperm.qc;

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
import java.util.Arrays;
import java.util.Base64;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/**
 * iSperm Medical QC Video 独立测试验证器 (PC/单元测试通用版)
 * =======================================================
 * 无需 Android 运行环境，可使用标准 JDK 8+ 直接编译运行，
 * 用于验证 U 盘上的 QC-Video/Video-1 能否在电脑上完成授权验证与解密提取。
 * 
 * 编译运行：
 *   javac -d . QcConstants.java QcStandaloneTester.java
 *   java com.isperm.qc.QcStandaloneTester /path/to/QC-Video/Video-1 /tmp/output
 */
public class QcStandaloneTester {

    public static void main(String[] args) {
        System.out.println("=================================================");
        System.out.println("  iSperm Medical QC Video 解密测试验证器 (Java)");
        System.out.println("=================================================");

        String videoFolderPath = args.length > 0 ? args[0] : "/tmp/test_usb_disk/QC-Video/Video-1";
        String targetOutputPath = args.length > 1 ? args[1] : "/tmp/qc_test_output";

        File videoFolder = new File(videoFolderPath);
        File targetDir = new File(targetOutputPath);

        if (!videoFolder.exists() || !videoFolder.isDirectory()) {
            System.err.println("[错误] 指定的视频文件夹不存在: " + videoFolder.getAbsolutePath());
            System.exit(1);
        }

        try {
            long t0 = System.currentTimeMillis();
            System.out.println("[1/4] 读取视频包与证书: " + videoFolder.getAbsolutePath());

            // 1. 查找 QCV 文件
            File qcvFile = null;
            for (File f : videoFolder.listFiles()) {
                if (f.isFile() && f.getName().toLowerCase().endsWith(".qcv")) {
                    qcvFile = f;
                    break;
                }
            }
            if (qcvFile == null) {
                throw new IOException("未找到 .qcv 密文文件");
            }

            // 2. 查找 manifest.dat 与 license.dat
            File manifestFile = new File(videoFolder, "manifest.dat");
            File licenseFile = new File(videoFolder, "license.dat");
            if (!manifestFile.exists() || !licenseFile.exists()) {
                File parent = videoFolder.getParentFile();
                if (parent != null) {
                    File pm = new File(parent, "manifest.dat");
                    File pl = new File(parent, "license.dat");
                    if (pm.exists() && pl.exists()) {
                        manifestFile = pm;
                        licenseFile = pl;
                    }
                }
            }

            // 3. 验证签名
            System.out.println("[2/4] 正在验证公司 RSA-2048 数字签名...");
            byte[] manifestBytes = readFile(manifestFile);
            String licenseStr = new String(readFile(licenseFile), StandardCharsets.UTF_8);
            String sigB64 = extractJsonString(licenseStr, "signature_base64");
            byte[] sigBytes = Base64.getDecoder().decode(sigB64);

            byte[] pubKeyDer = Base64.getDecoder().decode(QcConstants.COMPANY_PUBLIC_KEY_B64.replaceAll("\\s+", ""));
            X509EncodedKeySpec keySpec = new X509EncodedKeySpec(pubKeyDer);
            KeyFactory kf = KeyFactory.getInstance("RSA");
            PublicKey pubKey = kf.generatePublic(keySpec);

            Signature verifier = Signature.getInstance("SHA256withRSA");
            verifier.initVerify(pubKey);
            verifier.update(manifestBytes);
            boolean valid = verifier.verify(sigBytes);

            if (!valid) {
                System.err.println("❌ 签名验证失败！该 U 盘未经官方授权或文件损坏。");
                System.exit(2);
            }
            System.out.println("  ✓ 官方数字签名验证通过！合法授权 U 盘。");

            // 4. 解析信封并拆封 AES 内容密钥
            System.out.println("[3/4] 拆封 AES 内容密钥信封并解密视频流...");
            String manifestStr = new String(manifestBytes, StandardCharsets.UTF_8);
            String usbId = extractJsonString(manifestStr, "usb_id");
            String envelopeB64 = extractJsonString(manifestStr, "key_envelope");
            String videoName = videoFolder.getName();

            byte[] envelopeBytes = Base64.getDecoder().decode(envelopeB64);
            byte[] envNonce = Arrays.copyOfRange(envelopeBytes, 0, 12);
            byte[] envCiphertext = Arrays.copyOfRange(envelopeBytes, 12, envelopeBytes.length);

            byte[] masterKey = Base64.getDecoder().decode(QcConstants.APP_MASTER_KEY_B64.replaceAll("\\s+", ""));
            SecretKeySpec masterKeySpec = new SecretKeySpec(masterKey, "AES");
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, masterKeySpec, new GCMParameterSpec(128, envNonce));
            byte[] envAad = ("QC-KEY-ENVELOPE|" + videoName + "|" + usbId).getBytes(StandardCharsets.UTF_8);
            cipher.updateAAD(envAad);
            byte[] contentKey = cipher.doFinal(envCiphertext);
            System.out.println("  ✓ AES 内容密钥解密成功 (32 字节)");

            // 5. 解密 QCV
            byte[] qcvBytes = readFile(qcvFile);
            String magic = new String(Arrays.copyOfRange(qcvBytes, 0, 4), StandardCharsets.US_ASCII);
            if (!"QCV2".equals(magic)) {
                throw new IllegalArgumentException("未知魔数: " + magic);
            }
            byte[] vNonce = Arrays.copyOfRange(qcvBytes, 4, 16);
            byte[] vCiphertext = Arrays.copyOfRange(qcvBytes, 16, qcvBytes.length);

            SecretKeySpec contentKeySpec = new SecretKeySpec(contentKey, "AES");
            cipher.init(Cipher.DECRYPT_MODE, contentKeySpec, new GCMParameterSpec(128, vNonce));
            byte[] vAad = ("QCV2|" + videoName + "|" + usbId).getBytes(StandardCharsets.UTF_8);
            cipher.updateAAD(vAad);
            byte[] zipBytes = cipher.doFinal(vCiphertext);
            System.out.println("  ✓ 视频密文包解密成功 (" + zipBytes.length + " 字节)");

            // 6. 提取 30 张图片
            System.out.println("[4/4] 释放图片至目标文件夹: " + targetDir.getAbsolutePath() + "/" + videoName);
            File outVideoDir = new File(targetDir, videoName);
            outVideoDir.mkdirs();

            int count = 0;
            try (ZipInputStream zis = new ZipInputStream(new ByteArrayInputStream(zipBytes))) {
                ZipEntry entry;
                byte[] buf = new byte[4096];
                while ((entry = zis.getNextEntry()) != null) {
                    if (entry.isDirectory()) continue;
                    String name = new File(entry.getName()).getName();
                    if (name.toLowerCase().endsWith(".jpg") || name.toLowerCase().endsWith(".jpeg")) {
                        File outFile = new File(outVideoDir, name);
                        try (FileOutputStream fos = new FileOutputStream(outFile)) {
                            int n;
                            while ((n = zis.read(buf)) > 0) {
                                fos.write(buf, 0, n);
                            }
                        }
                        count++;
                    }
                    zis.closeEntry();
                }
            }

            long elapsed = System.currentTimeMillis() - t0;
            System.out.println("=================================================");
            System.out.println("  ★ 解密测试全部成功！");
            System.out.println("  ★ 恢复图像数量: " + count + " 张 (000.jpg ~ 029.jpg)");
            System.out.println("  ★ 输出目录:     " + outVideoDir.getAbsolutePath());
            System.out.println("  ★ 总计耗时:     " + elapsed + " 毫秒");
            System.out.println("=================================================");

        } catch (Exception e) {
            System.err.println("[异常] 测试失败: " + e.getMessage());
            e.printStackTrace();
            System.exit(3);
        }
    }

    private static String extractJsonString(String json, String key) {
        Pattern pattern = Pattern.compile("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
        Matcher matcher = pattern.matcher(json);
        if (matcher.find()) {
            return matcher.group(1);
        }
        throw new IllegalArgumentException("JSON 中未找到键: " + key);
    }

    private static byte[] readFile(File file) throws IOException {
        try (InputStream in = new FileInputStream(file);
             ByteArrayOutputStream out = new ByteArrayOutputStream((int) file.length())) {
            byte[] b = new byte[4096];
            int n;
            while ((n = in.read(b)) != -1) {
                out.write(b, 0, n);
            }
            return out.toByteArray();
        }
    }
}
