package com.isperm.qc;

/**
 * iSperm Medical QC Video 质控视频常量与安全配置
 * ============================================
 * 本类用于主控 APP 的 QC Video 导入与解密模块。
 * 包含官方数字签名验证公钥、主控 APP 内容解密主密钥及系统路径配置。
 */
public final class QcConstants {

    private QcConstants() {
        // 私有构造，禁止实例化
    }

    /**
     * 视频文件格式魔数（4字节标识）
     */
    public static final String MAGIC_QCV = "QCV2";

    /**
     * 每个质控视频包内包含的标准 JPEG 帧数量
     */
    public static final int EXPECTED_FRAME_COUNT = 30;

    /**
     * 安卓精子分析仪内部默认解密释放目标目录
     * 解密后的图片将写入：/mnt/internal_sd/QC/Video-1/000.jpg ~ 029.jpg
     */
    public static final String DEFAULT_INTERNAL_QC_PATH = "/mnt/internal_sd/QC";

    /**
     * U 盘根目录下的质控视频根文件夹名称
     */
    public static final String QC_VIDEO_FOLDER_NAME = "QC-Video";

    /**
     * 公司官方数字签名公钥（RSA-2048 X.509 SubjectPublicKeyInfo DER Base64 编码）
     * 用于验证 manifest.dat 与 license.dat 的真实性，彻底杜绝用户自行仿冒制作 U 盘。
     * 该公钥由 qc_video_builder_final.py 自动生成并输出。
     */
    public static final String COMPANY_PUBLIC_KEY_B64 =
            "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAtbM32U349VQue2nFLybE/lIMpQnrQN+e" +
            "kc6rHFja2txZUO2LHMwuDARfr3FYZrJZIzqJKgfp1EM4T8wK8/kSylLJ+6FtGBoBHsihXzbopHXv" +
            "JnlQ4/JHEg3LIGJnb9L858fSfudg0NkLVCCId9fGGDEzeFv/sMKSvcChEF1Ux3vxKiAa44AQeMoc" +
            "j3wS6w+bN27gVAXQlAxIzfklkoMhKtSILirNf3ETarrjdbwtkMywLYaxzC/9ez9XmaeZwLdGFrlI" +
            "19UMfnkMdMLZYHh94PAmh0aHHFWhrlvokTIZOK/EOG4hDcaCfNiPI74ylqIwW+GCUfu4TFdFbstH" +
            "f195uwIDAQAB";

    /**
     * 主控 APP 专属解密主密钥（32 字节 AES-256 Base64 编码）
     * 用于解密 manifest.dat 中的 wrapped AES 内容密钥信封。
     * 固化在仪器主控 APP 内部，PC 端无此密钥因而无法在电脑上解密播放。
     */
    public static final String APP_MASTER_KEY_B64 =
            "Sn+bLI4dP1prDC5Njxo7XH6fCitMbY4fOlt8nQ4vSms=";

    /**
     * 安卓系统常见 USB 挂载路径候选列表
     * 覆盖各种芯片方案（如瑞芯微 RK3288/RK3399/全志/AOSP/标准OTG）
     */
    public static final String[] USB_CANDIDATE_PATHS = new String[] {
            "/mnt/usb_storage",
            "/mnt/usb_storage/USB_DISK0",
            "/mnt/usb_storage/USB_DISK1",
            "/mnt/usb_storage/USB_DISK2",
            "/mnt/media_rw",
            "/storage/usbdisk",
            "/storage/usbotg",
            "/storage/usb0",
            "/storage/usb1",
            "/mnt/udisk",
            "/mnt/udisk1",
            "/storage",
            "/mnt/sdcard"
    };
}
