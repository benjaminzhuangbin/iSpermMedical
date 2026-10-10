将 ZIP 解压到 C:\QCBuilder。
解压后，确认目录中有这两个文件：
C:\QCBuilder\
    qc_video_builder.py
    README_中文.txt

QC Video USB 制作工具原型
===========================

用途
----
检查 Video-1、Video-2 等文件夹是否各有 000.jpg 到 029.jpg，然后生成加密的 .qcv 文件、manifest.dat 和 license.dat。

运行环境
--------
Windows 10/11，Python 3.10 或更新版本。

首次安装
--------
1. 安装 Python（安装时勾选 Add Python to PATH）。
2. 打开 CMD。
3. 执行：
   py -m pip install cryptography

准备测试图片
------------
建立例如：
D:\QC-Video\Video-1\000.jpg
D:\QC-Video\Video-1\001.jpg
...
D:\QC-Video\Video-1\029.jpg

源目录中可有 Video-1、Video-2 等多个子文件夹。

运行
----
假设脚本放在 C:\QCBuilder\qc_video_builder.py：

py C:\QCBuilder\qc_video_builder.py --source "D:\QC-Video" --output "D:\QC-Output"

制作成功后，会在 D:\QC_Output 下创建 QC-日期-随机编号 文件夹。
检查输出后，可把该文件夹内的文件复制到 USB DISK 根目录。

======================================================================================================
举例说明：
c:\ADB>py C:\QCBuilder\qc_video_builder.py --source "D:\QC-Video" --output "D:\QC-Output"

制作完成（原型）：
  USB 编号：QC-20261010-DFC2570A
  输出目录：D:\QC-Output\QC-20261010-DFC2570A
  视频包数量：1
  授权文件：D:\QC-Output\QC-20261010-DFC2570A\license.dat
  清单文件：D:\QC-Output\QC-20261010-DFC2570A\manifest.dat
  公司私钥（严禁复制到 U 盘）：D:\QC-Output\company_signing_private.pem
  公司公钥：D:\QC-Output\company_signing_public.pem

重要：此版本用于验证打包/加密/签名流程，不是最终可供 Android 导入的产品版本。
======================================================================================================

重要限制
--------
这是“制作端原型”，用于验证图片检查、打包、加密和签名文件生成。
- 当前版本故意不把 AES 内容密钥放在 U 盘上，因此 Android 还不能解密播放。
- 后续工程师需要设计安全的密钥交付/封装方式，并在 Android APP 中验证签名和解密。
- 私钥 company_signing_private.pem 非常重要：必须保存在公司电脑受控位置，不能复制到客户 U 盘，也不要发给客户。
- 当前原型每次制作使用新的随机视频加密密钥，但暂未实现正式的密钥备份/恢复。丢失本次密钥就需要重新制作。
- 该方案不能阻止整盘克隆到另一只普通 U 盘。可靠防克隆需要具备可验证身份的安全硬件。
- 该原型只检查文件名和非空，不负责验证 JPEG 图像是否能正常解码。



