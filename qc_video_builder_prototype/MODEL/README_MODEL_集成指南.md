# iSperm Medical Android 主控 APP - QC Video 解密与导入模块集成指南

本模块专为 **安卓系统精子分析仪** 设计，用于配合定制加密质控 U 盘（QC Video USB Disk），实现安全、合规、零开发难度的质控视频导入与商业化防护。

---

## 一、方案核心防护机制（满足商业收费与防破解需求）

1. **PC 端完全不可见（防私下拷贝/泄露）**：
   - U 盘插入 PC 电脑时，用户只能看到 `.qcv`、`manifest.dat`、`license.dat` 等二进制密文文件。
   - 所有 30 张图片均经过 **AES-256-GCM 工业级认证加密**，Windows 照片查看器、Photoshop、播放器等均无法识别或打开。
2. **防用户自行制作/仿冒（必须向公司购买）**：
   - 清单与授权受 **公司官方私钥数字签名（RSA-2048 / SHA256withRSA）** 保护。
   - 私钥严格保存在公司受控电脑本地（严禁拷入 U 盘），用户即使复制文件也无法伪造签名。
   - 主控 APP 导入前必须验证签名，签名不符立即拦截拒绝导入。
3. **零网络依赖与借用共享**：
   - 仪器完全不需要联网（适用于医院与检验所封闭内网）。
   - 授权与主控 APP 深度绑定，已付费客户可以互相借用或更换 U 盘，插入任意一台兼容分析仪均可自动解密载入。
4. **系统内隔离**：
   - 视频解密后直接释放到内部路径 `/mnt/internal_sd/QC/Video-1/`。
   - 仪器开机直达主控 APP（Kiosk 模式），普通用户无法接触 Android 底层文件系统。

---

## 二、模块文件清单（直接复制进 Android Studio 项目）

在您的 Android Studio 主控 APP 源码中，将 `qc_video_builder_prototype/MODEL` 下的代码放入包路径（如 `com.isperm.qc`）：

| 文件名 | 职责说明 |
| :--- | :--- |
| **`QcConstants.java`** | **常量配置**：内置公司公钥 Base64、APP 主解密密钥 Base64、释放目录 `/mnt/internal_sd/QC`、USB 候选路径。 |
| **`QcVideoImporter.java`** | **解密核心引擎**：数字签名验证、AES 内容密钥信封拆封、AES-GCM 解密、30 张 JPEG 图像提取写入。 |
| **`QcImportDialog.java`** | **UI 交互控制器**：自动扫描 USB 盘、弹出视频单选弹窗、带进度条解密、支持连续重复导入（Video-1, Video-2...）。 |

---

## 三、快速集成步骤（仅需 3 步，10 分钟完成）

### 第 1 步：配置 AndroidManifest.xml 权限

确保 `AndroidManifest.xml` 中已具备外置与内置存储读写权限：
```xml
<uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
<uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
<uses-permission android:name="android.permission.MOUNT_UNMOUNT_FILESYSTEMS" />
```

### 第 2 步：绑定 QC 界面中的【Import Video】按键

在您主控 APP 的 QC 页面（Activity 或 Fragment）中，找到 **Import Video** 按钮，加入如下简单调用：

```java
import com.isperm.qc.QcImportDialog;
import com.isperm.qc.QcVideoImporter;

// ... 在 onCreate 或 onViewCreated 中绑定按钮点击事件 ...
Button btnImportVideo = findViewById(R.id.btn_import_video);

btnImportVideo.setOnClickListener(new View.OnClickListener() {
    @Override
    public void onClick(View v) {
        // 一行代码调起完整的 USB 扫描、选择与解密导入流程！
        QcImportDialog.showImportDialog(MainActivity.this, new QcImportDialog.OnImportCompletedListener() {
            @Override
            public void onImportFinished(QcVideoImporter.ImportResult result) {
                // 导入成功后的业务回调：
                Log.d("QC", "已导入视频: " + result.videoName + ", 存放于: " + result.outputDirectory);
                
                // 刷新主控界面上的质控视频列表或自动加载图片预览
                loadQcVideoImages(result.outputDirectory);
            }
        });
    }
});
```

### 第 3 步：读取使用导入后的 30 张图片

导入完成后，图片已保存在标准路径：
```
/mnt/internal_sd/QC/Video-1/000.jpg
/mnt/internal_sd/QC/Video-1/001.jpg
...
/mnt/internal_sd/QC/Video-1/029.jpg
```
主控 APP 直接像使用本地常规文件一样读取即可：
```java
File videoFolder = new File("/mnt/internal_sd/QC/Video-1");
for (int i = 0; i < 30; i++) {
    String fileName = String.format("%03d.jpg", i);
    File imgFile = new File(videoFolder, fileName);
    Bitmap bitmap = BitmapFactory.decodeFile(imgFile.getAbsolutePath());
    // 送入 CASA 精子分析算法或在屏幕播放...
}
```

---

## 四、支持的 USB 目录组织规范

制作工具 `qc_video_builder_final.py` 输出后，U 盘根目录下结构为：

```text
U 盘根目录/
└── QC-Video/
    ├── Video-1/
    │   ├── Video-1.qcv       <-- AES-256 加密后的密文包
    │   ├── manifest.dat      <-- 清单与密钥信封
    │   └── license.dat       <-- 官方数字签名
    ├── Video-2/
    │   ├── Video-2.qcv
    │   ├── manifest.dat
    │   └── license.dat
    ├── manifest.dat          <-- 全局清单
    └── license.dat           <-- 全局数字签名
```

### 用户操作体验：
1. 用户插入 U 盘到仪器。
2. 点击 **【Import Video】**。
3. 屏幕弹出列表：
   - `Video-1`
   - `Video-2`
4. 用户点击 `Video-1`，屏幕显示进度条并在 0.1 秒内解密完毕。
5. 弹窗提示：`“✓ 成功载入 30 张 QC 图片到系统内部！[继续载入其他视频] [完成]”`。
6. 用户点击“继续载入其他视频”，即可无缝选择载入 `Video-2`，重复操作，体验极其流畅！
