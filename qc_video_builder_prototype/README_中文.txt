QC Video USB 制作与加密工具 — 最终生产版使用指南
=====================================================

一、用途与设计定位
------------------
本工具（qc_video_builder_final.py）配合精子分析仪（安卓系统）的 QC 解密模块，实现质控视频的商业化交付与安全防护：
1. 【PC端完全保密】：U盘在电脑上无法直接打开或查看图片（30张图片以 AES-256-GCM 高度加密在 .qcv 文件中）。
2. 【防用户自行制作】：采用公司官方 RSA-2048 非对称数字签名，用户无法自行制作假 U 盘，必须向官方购买。
3. 【免网络即插即用】：仪器不需要连接互联网，仪器开机直接进入主控 APP，用户直接在 QC 界面点击【Import Video】选择 U 盘中的 Video-1 / Video-2，自动解密释放到 /mnt/internal_sd/QC/Video-1/000.jpg ~ 029.jpg。
4. 【允许多机使用与借用】：购买了 U 盘的用户可以在多台兼容仪器上使用，符合医疗器械耗材/质控盘的使用习惯。

二、运行环境与准备
------------------
1. 运行平台：Windows 10/11 或 Linux / macOS，Python 3.8+。
2. 安装依赖库：
   pip install cryptography

三、准备测试源图片
------------------
在电脑上建立源目录，例如 D:\QC-Source：
D:\QC-Source\Video-1\000.jpg
D:\QC-Source\Video-1\001.jpg
...
D:\QC-Source\Video-1\029.jpg (每个 Video-X 必须恰好 30 张图片)

D:\QC-Source\Video-2\000.jpg ~ 029.jpg (可支持多个视频)

四、一键运行制作命令
--------------------
命令行执行（假设脚本保存在 C:\QCBuilder\qc_video_builder_final.py）：

python qc_video_builder_final.py --source "D:\QC-Source" --output "E:\"

（注：--output 可以直接指定 U 盘盘符例如 E:\，也可以指定本地目录例如 D:\QC-Output，制作完成后复制）

五、输出文件结构与 U 盘拷贝规范
--------------------------------
制作完成后，在输出目录下会生成标准目录结构：

U 盘盘符:\
└── QC-Video\
    ├── Video-1\
    │   ├── Video-1.qcv       <-- AES-256 加密密文包（电脑无法打开）
    │   ├── manifest.dat      <-- 视频信息与加密密钥信封
    │   └── license.dat       <-- 官方数字签名凭证（防伪防篡改）
    ├── Video-2\
    │   ├── Video-2.qcv
    │   ├── manifest.dat
    │   └── license.dat
    ├── manifest.dat          <-- 全盘清单
    ├── license.dat           <-- 全盘数字签名
    └── README_QC_USB.txt     <-- U盘质控说明

【重要安全规则】：
- company_keys 目录下的 company_signing_rsa_private.pem 是公司核心私钥，必须保存在公司电脑中，【严禁复制到客户 U 盘】！
- U 盘只需要拷贝 QC-Video 文件夹即可！

六、Android 主控 APP 端接入（MODEL 目录）
------------------------------------------
配套的主控 APP 解密代码位于 MODEL 目录下：
- QcConstants.java: 常量配置（公钥与解密主密钥）
- QcVideoImporter.java: 核心解密与图片提取引擎
- QcImportDialog.java: UI 弹窗（一键弹出 USB 视频选择列表，带进度条解密并写入 /mnt/internal_sd/QC/）
- README_MODEL_集成指南.md: 3分钟集成文档

详细集成方式请参阅 MODEL/README_MODEL_集成指南.md。
