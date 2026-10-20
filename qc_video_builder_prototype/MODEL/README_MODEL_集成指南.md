# iSperm Medical Android 主控 APP - QC Video 解密与一对一绑定导入模块集成指南

本模块专为 **安卓系统精子分析仪** 设计，配合定制加密质控 U 盘（QC Video USB Disk），实现商业化安全交付：**AES-256-GCM 帧认证加密 + RSA-2048 非对称数字签名 + 物理芯片抗克隆硬件指纹 + 一机一盘一对一专属锁定**。

---

## 一、方案核心防护机制（满足商业收费与一对一绑定需求）

1. **PC 端完全保密（防私下查看/泄密）**：
   - U 盘插入 PC 电脑时，用户只能看到 `.qcv`、`manifest.dat`、`license.dat`、`device_binding.lic` 等二进制密文文件。
   - 所有 30 张图片均经过 **AES-256-GCM 工业级认证加密**，Windows 照片查看器、Mac、播放器等均无法识别或打开。
2. **防随意克隆拷贝（防私自复制到普通 U 盘）**：
   - 用户即使将加密 U 盘全盘复制到普通 FAT32 U 盘中，插入分析仪时，底层 `QcUsbIdentity` 会读取当前插入 U 盘主控芯片的出厂硬件 Serial。普通 U 盘硬件序列号与 `device_binding.lic` 签名中的序列号不符，立即被系统拦截拒绝。
3. **一机一盘专属强绑定（防跨机借用与窜货）**：
   - 采用 **仪器 CPU 唯一识别码 (`device_id`) + U 盘物理芯片序列号 (`usb_id`)** 双重绑定。
   - A 医院购买的质控 U 盘插到 B 医院的分析仪上，会因主板硬件识别码不匹配直接阻断（提示：`此 QC USB DISK 未授权用于本仪器`）。
4. **视频内容与设备绑定解耦（支持后期增减视频）**：
   - 绑定授权文件 `device_binding.lic` 针对仪器与 U 盘本身有效。后续官方若为客户增发新视频（如 `Video-3`），只需拷入视频文件夹并更新视频签名，**原绑定授权永久有效，无需重新绑定**！
5. **离线免联网，零网络依赖**：
   - 仪器完全不需要连接互联网，极速即插即用，纯本地高强度密码学校验（耗时 < 0.05 秒）。

---

## 二、模块文件清单（直接复制进 Android Studio 项目）

在您的 Android Studio 主控 APP 源码中，将 `qc_video_builder_prototype/MODEL` 下的所有 Java 类放入同一包路径（推荐 `com.isperm.qc`）：

| 文件名 | 职责与功能说明 |
| :--- | :--- |
| **`QcDeviceIdentity.java`** | **本机设备身份提取器**：稳定读取分析仪主板 CPU 序列号 (`ro.serialno`) 与系统唯一特征码，生成全局标准 `ISPERM-DEV-xxxxxxxx`。 |
| **`QcUsbIdentity.java`** | **USB 物理硬件指纹提取器**：通过 Android `UsbManager` 或 Linux 底层 Sysfs 读取插入 U 盘的主控芯片出厂固件 Serial（格式化亦不改变）。 |
| **`QcDeviceBindingVerifier.java`** | **一对一绑定核心校验器**：使用公司官方 RSA-2048 公钥验证 `device_binding.lic` 的合法性、防篡改签名与硬件指纹双向匹配。 |
| **`QcConstants.java`** | **常量配置中心**：内置公司公钥 Base64 (`COMPANY_PUBLIC_KEY_B64`)、主解密密钥 Base64 (`APP_MASTER_KEY_B64`)、默认存储路径等。 |
| **`QcVideoImporter.java`** | **解密核心引擎**：前置调用绑定校验，通过后对视频包验证数字签名、拆封 AES 密钥信封、解密提取 30 张 JPEG 图像。 |
| **`QcImportDialog.java`** | **UI 交互控制器**：自动检测 USB 挂载，未绑定友好弹窗拦截，绑定成功则展示视频列表弹窗并带进度条导入。 |
| **`QcStandaloneTester.java`** | **离线测试类**：可在 Android/PC 环境独立执行签名验证与解密自检。 |

---

## 三、快速集成步骤（3 步完成）

### 第 1 步：配置 AndroidManifest.xml 权限

确保 `AndroidManifest.xml` 中具备外置与内置存储读写权限，以及 USB 主机特性声明：

```xml
<uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
<uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
<uses-permission android:name="android.permission.MOUNT_UNMOUNT_FILESYSTEMS" />

<!-- 声明 USB 主机特性，便于精确获取 U 盘芯片序列号 -->
<uses-feature android:name="android.hardware.usb.host" android:required="false" />
```

### 第 2 步：在关于界面展示设备编号（便于客户报备与售后绑定）

建议在分析仪【系统设置】->【关于仪器】或 QC 界面上，展示当前仪器的设备识别码：

```java
import com.isperm.qc.QcDeviceIdentity;

// 获取当前仪器的唯一设备识别码
String currentDeviceId = QcDeviceIdentity.getDeviceId(context);

// 在界面 TextView 显示，例如："设备编号: ISPERM-DEV-3A8F91B2-C4D5E6F7"
tvDeviceId.setText("设备编号: " + currentDeviceId);
```
> **提示**：已售老客户远程升级时，只需在界面上查看此编号并告知公司售后人员，售后运行 `qc_device_binder.py` 即可一键签发专属 U 盘！

### 第 3 步：绑定 QC 界面中的【Import Video】按键

在 QC 页面（Activity 或 Fragment）中，找到 **Import Video** 按钮，绑定如下调用：

```java
import com.isperm.qc.QcImportDialog;
import com.isperm.qc.QcVideoImporter;

Button btnImportVideo = findViewById(R.id.btn_import_video);

btnImportVideo.setOnClickListener(new View.OnClickListener() {
    @Override
    public void onClick(View v) {
        // 一行代码调起完整的 USB 检测、设备绑定验证、视频列表选择与解密导入流程！
        QcImportDialog.showImportDialog(MainActivity.this, new QcImportDialog.OnImportCompletedListener() {
            @Override
            public void onImportFinished(QcVideoImporter.ImportResult result) {
                // 导入成功后的业务回调：
                Log.d("QC", "已导入视频: " + result.videoName + ", 存放于: " + result.outputDirectory);
                
                // 刷新主控界面上的质控视频列表或自动载入 30 帧图片预览
                loadQcVideoImages(result.outputDirectory);
            }
        });
    }
});
```

### 第 4 步：读取使用导入后的 30 张图片

导入完成后，图片解密保存在设备内部标准路径：
```text
/mnt/internal_sd/QC/Video-1/000.jpg
/mnt/internal_sd/QC/Video-1/001.jpg
...
/mnt/internal_sd/QC/Video-1/029.jpg
```
主控 APP 直接读取使用即可：
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

## 四、安全拦截提示与用户交互表现

当用户插入 U 盘并点击【Import Video】时，系统自动执行双重安全校验：

1. **合法专属 U 盘（通过）**：
   - 立即弹出质控视频选择弹窗：`Video-1`、`Video-2`。
   - 点击选择后进度条平滑加载，解密 30 张图片（耗时 0.1 秒）。
   - 弹窗提示：`“✓ 成功载入 30 张 QC 图片到系统内部！[继续载入其他视频] [完成]”`。

2. **跨机窜货 / 借用其他医院的 U 盘（机器不符拦截）**：
   - 弹出阻断对话框：
     > **【质控盘未授权】**  
     > 此 QC USB DISK 未授权用于本仪器！  
     > 请使用分配给本仪器的专属 QC USB DISK。  
     > *(本机设备号: ISPERM-DEV-AAAA，授权设备: ISPERM-DEV-BBBB)*

3. **私自克隆全盘文件到普通 U 盘（防伪抗克隆拦截）**：
   - 弹出阻断对话框：
     > **【原装介质校验失败】**  
     > 检测到非原装授权 U 盘！  
     > 质控视频必须配合官方配套发货的原装加密 U 盘使用，严禁私自克隆拷贝。

4. **未经签名的非法制作 U 盘（防伪拦截）**：
   - 弹出阻断对话框：
     > **【授权验证失败】**  
     > 质控盘授权签名无效或已被篡改！请联系官方售后团队。

---

## 五、公钥与密钥配置确认

在发布正式生产 APK 前，确认 `QcConstants.java` 中的公司公钥与公司 PC 制作端公钥完全一致：

```java
// 由 company_keys/company_signing_rsa_public.pem 导出的 Base64 公钥字符串
public static final String COMPANY_PUBLIC_KEY_B64 = 
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA...";
```

> **绝对安全保证**：`COMPANY_PUBLIC_KEY_B64` 为**纯公钥**，仅能用于验签，无法逆向推出私钥。即便 APK 被反编译，外部攻击者也绝无可能伪造授权文件！
