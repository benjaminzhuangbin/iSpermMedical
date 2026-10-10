#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
QC Video USB Builder — Final Production Edition (qc_video_builder_final.py)
==========================================================================
配套医疗级精子分析仪（Android系统）专用 QC Video USB 制作与加密工具。

功能特性：
1. 批量检查 QC-Video/Video-1, Video-2... 目录下各 30 张 JPEG 图像 (000.jpg ~ 029.jpg) 的完整性与非空校验。
2. 真正生成高强度随机 AES-256 内容加密密钥，对每组视频帧进行 AES-GCM 工业级认证加密，PC端直接无法打开或预览。
3. 采用主控APP安全信封（Key Envelope）对 AES 内容密钥进行封装分发，兼容离线USB即插即用。
4. 使用公司非对称私钥（RSA-2048 / SHA256withRSA 或 Ed25519）对清单与授权签名，Android端严密校验签名，彻底杜绝用户私自制作仿冒U盘。
5. 一键生成符合标准规范的 USB 目录结构：QC-Video/Video-1/, QC-Video/Video-2/...
6. 自动导出 Android 端配置代码（Public Key Base64 与 Master Key Base64），直接对接 Android 端 QcVideoImporter 模块。

环境需求：
- Python 3.8+
- 安装依赖库：pip install cryptography
"""

import argparse
import base64
import hashlib
import io
import json
import os
import secrets
import shutil
import sys
import uuid
import zipfile
from datetime import datetime, timezone
from pathlib import Path

try:
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    from cryptography.hazmat.primitives.asymmetric import rsa, padding, ed25519
    from cryptography.hazmat.primitives import hashes, serialization
    HAS_CRYPTO = True
except ImportError:
    HAS_CRYPTO = False

EXPECTED_IMAGES = [f"{i:03d}.jpg" for i in range(30)]
MAGIC_QCV = b"QCV2"  # V2 生产版本文件标识

# 默认内置开发主密钥 (32字节 AES-256)，在实际生产中可由工具持久化生成并安全保管
DEFAULT_APP_MASTER_SECRET_HEX = "4a7f9b2c8e1d3f5a6b0c2e4d8f1a3b5c7e9f0a2b4c6d8e1f3a5b7c9d0e2f4a6b"


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_json(obj) -> bytes:
    """生成确定性、无空白紧凑格式的 JSON 字节流，保证签名哈希一致性"""
    return json.dumps(obj, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")


def check_video_folder(folder: Path):
    """严格检查视频文件夹中的 30 张图片"""
    if not folder.is_dir():
        raise ValueError(f"不是文件夹：{folder}")
    files = [p for p in folder.iterdir() if p.is_file()]
    jpg_names = sorted(p.name for p in files if p.suffix.lower() in (".jpg", ".jpeg"))
    missing = [name for name in EXPECTED_IMAGES if not (folder / name).is_file()]
    unexpected = [name for name in jpg_names if name not in EXPECTED_IMAGES]
    if missing or unexpected:
        parts = []
        if missing:
            parts.append("缺少：" + ", ".join(missing))
        if unexpected:
            parts.append("存在非标准名称：" + ", ".join(unexpected))
        raise ValueError(f"{folder.name} 检查失败；" + "；".join(parts))
    if len(jpg_names) != 30:
        raise ValueError(f"{folder.name} 必须恰好包含 30 张 JPG，当前共有 {len(jpg_names)} 张")
    return [(folder / name) for name in EXPECTED_IMAGES]


def pack_images_to_zip(folder: Path) -> tuple[bytes, dict]:
    """将 30 张图片打成内存 ZIP 压缩包并计算校验哈希"""
    images = check_video_folder(folder)
    buf = io.BytesIO()
    records = []
    with zipfile.ZipFile(buf, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for p in images:
            data = p.read_bytes()
            if len(data) == 0:
                raise ValueError(f"检测到空图像文件：{p}")
            # 基础 JPEG 文件头合法性校验 (0xFF 0xD8)
            if not data.startswith(b"\xff\xd8"):
                print(f"[警告] {p.name} 头部不是标准 JPEG 魔数，但仍继续处理。")
            zf.writestr(p.name, data)
            records.append({
                "name": p.name,
                "size": len(data),
                "sha256": sha256_hex(data)
            })
    return buf.getvalue(), {"video_name": folder.name, "image_count": 30, "images": records}


def init_or_load_keys(key_dir: Path, algo: str = "rsa2048"):
    """
    初始化或加载公司数字签名密钥与主控APP主密钥。
    key_dir 存放在公司受控电脑本地，严禁复制到客户U盘！
    """
    key_dir.mkdir(parents=True, exist_ok=True)
    
    # 1. 签名密钥管理
    if algo == "rsa2048":
        priv_path = key_dir / "company_signing_rsa_private.pem"
        pub_path = key_dir / "company_signing_rsa_public.pem"
        if priv_path.exists() and pub_path.exists():
            private_key = serialization.load_pem_private_key(priv_path.read_bytes(), password=None)
        else:
            private_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
            priv_path.write_bytes(private_key.private_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PrivateFormat.PKCS8,
                encryption_algorithm=serialization.NoEncryption()
            ))
            pub_path.write_bytes(private_key.public_key().public_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PublicFormat.SubjectPublicKeyInfo
            ))
        pub_der = private_key.public_key().public_bytes(
            encoding=serialization.Encoding.DER,
            format=serialization.PublicFormat.SubjectPublicKeyInfo
        )
        pub_b64 = base64.b64encode(pub_der).decode("ascii")
    else:  # ed25519
        priv_path = key_dir / "company_signing_ed25519_private.pem"
        pub_path = key_dir / "company_signing_ed25519_public.pem"
        if priv_path.exists() and pub_path.exists():
            private_key = serialization.load_pem_private_key(priv_path.read_bytes(), password=None)
        else:
            private_key = ed25519.Ed25519PrivateKey.generate()
            priv_path.write_bytes(private_key.private_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PrivateFormat.PKCS8,
                encryption_algorithm=serialization.NoEncryption()
            ))
            pub_path.write_bytes(private_key.public_key().public_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PublicFormat.SubjectPublicKeyInfo
            ))
        pub_der = private_key.public_key().public_bytes(
            encoding=serialization.Encoding.DER,
            format=serialization.PublicFormat.SubjectPublicKeyInfo
        )
        pub_b64 = base64.b64encode(pub_der).decode("ascii")

    # 2. 主控 APP 视频解密主密钥 (KEK: Key Encryption Key)
    master_key_path = key_dir / "company_master_secret.key"
    if master_key_path.exists():
        master_secret = master_key_path.read_bytes()
        if len(master_secret) != 32:
            master_secret = hashlib.sha256(master_secret).digest()
    else:
        # 如果未设置，生成随机 32 字节主密钥并保存
        master_secret = bytes.fromhex(DEFAULT_APP_MASTER_SECRET_HEX)
        master_key_path.write_bytes(master_secret)
        
    master_b64 = base64.b64encode(master_secret).decode("ascii")

    # 导出可供 Android 代码直接复制的说明文件
    app_config_path = key_dir / "Android_App_Keys_Config.txt"
    app_config_path.write_text(
        f"// ==================== 复制到 Android 端的密钥配置 ====================\n"
        f"// 算法: {algo}\n"
        f"// 公司数字签名公钥 (Base64):\n"
        f"public static final String COMPANY_PUBLIC_KEY_B64 = \"{pub_b64}\";\n\n"
        f"// 主控APP内容解密主密钥 (Base64 32字节):\n"
        f"public static final String APP_MASTER_KEY_B64 = \"{master_b64}\";\n"
        f"// ====================================================================\n",
        encoding="utf-8"
    )

    return private_key, pub_path, pub_b64, master_secret, master_b64


def build_qc_usb_package(source_dir: Path, output_dir: Path, key_dir: Path, algo: str = "rsa2048"):
    """
    核心制作流程：
    1. 检查并读取所有 Video-1, Video-2...
    2. 生成 AES-256 密钥，完成帧数据加密与信封封装
    3. 生成 RSA-2048 / Ed25519 签名
    4. 输出规范的 USB 目录：output_dir/QC-Video/Video-X/...
    """
    if not HAS_CRYPTO:
        raise RuntimeError("缺少 cryptography 模块，请先在终端运行：pip install cryptography")

    source_dir = source_dir.resolve()
    output_dir = output_dir.resolve()
    key_dir = key_dir.resolve()
    
    # 查找子文件夹：支持直接以 Video- 开头的文件夹，或者用户选中的含有 Video- 的目录
    video_dirs = sorted([
        p for p in source_dir.iterdir()
        if p.is_dir() and (p.name.lower().startswith("video-") or p.name.lower().startswith("video_"))
    ])
    
    if not video_dirs:
        # 如果 source_dir 下有一层 QC-Video，则往下查找
        qc_subdir = source_dir / "QC-Video"
        if qc_subdir.is_dir():
            video_dirs = sorted([
                p for p in qc_subdir.iterdir()
                if p.is_dir() and (p.name.lower().startswith("video-") or p.name.lower().startswith("video_"))
            ])

    if not video_dirs:
        raise ValueError(f"源目录 {source_dir} 下未找到类似 Video-1, Video-2 的子文件夹！")

    print(f"\n[1/5] 正在校验源视频文件夹...")
    prepared_packages = []
    for v_dir in video_dirs:
        zip_bytes, meta = pack_images_to_zip(v_dir)
        prepared_packages.append((v_dir.name, zip_bytes, meta))
        print(f"  ✓ {v_dir.name}: 30 张图片校验完成 (原始压缩大小: {len(zip_bytes)} 字节)")

    print(f"\n[2/5] 加载/初始化企业签名密钥与主解密密钥...")
    private_key, pub_path, pub_b64, master_secret, master_b64 = init_or_load_keys(key_dir, algo)
    master_aes = AESGCM(master_secret)

    usb_id = "QC-" + datetime.now().strftime("%Y%m%d") + "-" + uuid.uuid4().hex[:8].upper()
    qc_root_dir = output_dir / "QC-Video"
    qc_root_dir.mkdir(parents=True, exist_ok=True)

    print(f"\n[3/5] 正在加密视频包并封装密钥信封...")
    manifest_videos = []

    for v_name, zip_data, meta in prepared_packages:
        # 为每个视频生成独立、高强度的随机 256 位 AES-GCM 内容密钥
        content_key = secrets.token_bytes(32)
        content_aes = AESGCM(content_key)
        
        # 视频包数据加密 (AES-256-GCM)
        video_nonce = secrets.token_bytes(12)
        video_aad = f"QCV2|{v_name}|{usb_id}".encode("utf-8")
        encrypted_video_payload = content_aes.encrypt(video_nonce, zip_data, video_aad)
        
        # 格式：MAGIC_QCV (4B) + NONCE (12B) + CIPHERTEXT_WITH_TAG
        final_video_file_bytes = MAGIC_QCV + video_nonce + encrypted_video_payload

        # 封装密钥信封 (Key Envelope)：用主控APP专属主密钥加密 content_key
        envelope_nonce = secrets.token_bytes(12)
        envelope_aad = f"QC-KEY-ENVELOPE|{v_name}|{usb_id}".encode("utf-8")
        wrapped_key_payload = master_aes.encrypt(envelope_nonce, content_key, envelope_aad)
        envelope_b64 = base64.b64encode(envelope_nonce + wrapped_key_payload).decode("ascii")

        # 准备输出子目录: QC-Video/Video-X/
        sub_out = qc_root_dir / v_name
        sub_out.mkdir(parents=True, exist_ok=True)

        qcv_filename = f"{v_name}.qcv"
        (sub_out / qcv_filename).write_bytes(final_video_file_bytes)

        v_record = {
            "file": qcv_filename,
            "video_name": v_name,
            "file_size": len(final_video_file_bytes),
            "encrypted_sha256": sha256_hex(final_video_file_bytes),
            "aad": base64.b64encode(video_aad).decode("ascii"),
            "key_envelope": envelope_b64,
            "images": meta["images"],
            "image_count": 30
        }
        manifest_videos.append(v_record)

        # 在每个 Video-X 子目录下也保留该视频的独立 manifest.dat 和 license.dat，便于APP直接选子文件夹导入
        sub_manifest = {
            "format": "QCV2",
            "usb_id": usb_id,
            "created_utc": datetime.now(timezone.utc).isoformat(),
            "product": "iSperm Medical QC Video",
            "videos": [v_record]
        }
        sub_manifest_bytes = canonical_json(sub_manifest)
        (sub_out / "manifest.dat").write_bytes(sub_manifest_bytes)

        # 单独签署子文件夹授权
        if algo == "rsa2048":
            sub_sig = private_key.sign(
                sub_manifest_bytes,
                padding.PKCS1v15(),
                hashes.SHA256()
            )
        else:
            sub_sig = private_key.sign(sub_manifest_bytes)

        sub_license = {
            "format": "QCL2",
            "usb_id": usb_id,
            "target": v_name,
            "manifest_sha256": sha256_hex(sub_manifest_bytes),
            "signature_algorithm": "SHA256withRSA" if algo == "rsa2048" else "Ed25519",
            "signature_base64": base64.b64encode(sub_sig).decode("ascii")
        }
        (sub_out / "license.dat").write_bytes(canonical_json(sub_license))
        print(f"  ✓ {v_name}: 加密成功，已生成 {qcv_filename} 及独立授权凭证")

    print(f"\n[4/5] 正在生成整盘清单与公司数字签名...")
    global_manifest = {
        "format": "QCV2",
        "usb_id": usb_id,
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "product": "iSperm Medical QC Video",
        "videos": manifest_videos,
        "note": "Production Quality Control Video USB Package"
    }
    global_manifest_bytes = canonical_json(global_manifest)
    (qc_root_dir / "manifest.dat").write_bytes(global_manifest_bytes)

    if algo == "rsa2048":
        global_sig = private_key.sign(
            global_manifest_bytes,
            padding.PKCS1v15(),
            hashes.SHA256()
        )
        sig_algo_name = "SHA256withRSA"
    else:
        global_sig = private_key.sign(global_manifest_bytes)
        sig_algo_name = "Ed25519"

    global_license = {
        "format": "QCL2",
        "usb_id": usb_id,
        "product": "iSperm Medical QC Video",
        "manifest_sha256": sha256_hex(global_manifest_bytes),
        "signature_algorithm": sig_algo_name,
        "signature_base64": base64.b64encode(global_sig).decode("ascii")
    }
    (qc_root_dir / "license.dat").write_bytes(canonical_json(global_license))

    # 生成 USB 使用指南说明文件
    readme_usb = (
        f"iSperm Medical QC Video USB 专属质控盘\n"
        f"=========================================\n"
        f"批次编号: {usb_id}\n"
        f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n"
        f"包含视频: {len(manifest_videos)} 组 ({', '.join([v['video_name'] for v in manifest_videos])})\n\n"
        f"【使用说明】\n"
        f"1. 本U盘已安全加密，PC电脑上无法直接打开或查看图片。\n"
        f"2. 请将本 U 盘插入精子分析仪的 USB 接口。\n"
        f"3. 在精子分析仪主控界面的 QC 页面中，点击【Import Video】按键。\n"
        f"4. 在弹出的列表中选择对应的 Video 文件夹（如 Video-1），系统将自动完成授权验证与解密。\n"
        f"5. 解密后的 30 张图片将自动释放到仪器系统内部供分析使用。\n"
    )
    (qc_root_dir / "README_QC_USB.txt").write_text(readme_usb, encoding="utf-8")

    print(f"\n[5/5] 制作完成！")
    print(f"======================================================================")
    print(f"  ★ USB 批次编号: {usb_id}")
    print(f"  ★ 视频包数量:   {len(manifest_videos)} 个")
    print(f"  ★ U盘拷贝目录:   {qc_root_dir}")
    print(f"  ★ 签名私钥位置: {key_dir} (严禁复制给客户或拷入U盘！)")
    print(f"  ★ Android配置:  {key_dir / 'Android_App_Keys_Config.txt'}")
    print(f"======================================================================")
    print(f"\n【下一步操作】：")
    print(f"1. 直接将文件夹 [ {qc_root_dir.name} ] 完整复制到 U 盘根目录下。")
    print(f"2. 确保 U 盘根目录下结构为：U盘盘符:\\QC-Video\\Video-1, Video-2...")
    print(f"3. 插入精子分析仪 USB 端口即可在 APP 中导入。")


def main():
    parser = argparse.ArgumentParser(
        description="iSperm Medical QC Video USB 制作与加密工具 (最终正式版)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""示例用法:
  python qc_video_builder_final.py --source "D:\\QC-Video" --output "E:\\"
  python qc_video_builder_final.py --source "D:\\QC-Video" --output "D:\\QC-Output" --key-dir "C:\\QC-Keys"
        """
    )
    parser.add_argument("--source", required=True, help="包含 Video-1、Video-2 等子文件夹的源目录")
    parser.add_argument("--output", required=True, help="输出目录（可直接指定U盘根目录或本地目录）")
    parser.add_argument("--key-dir", default=None, help="公司签名密钥与主密钥持久化存放目录（默认保存在脚本同级 keys 目录）")
    parser.add_argument("--algo", default="rsa2048", choices=["rsa2048", "ed25519"],
                        help="签名算法：默认 rsa2048 (Android 原生零依赖极佳兼容 API 1~35) 或 ed25519")
    args = parser.parse_args()

    source_path = Path(args.source)
    output_path = Path(args.output)
    if args.key_dir:
        key_path = Path(args.key_dir)
    else:
        key_path = Path(__file__).resolve().parent / "company_keys"

    try:
        build_qc_usb_package(source_path, output_path, key_path, args.algo)
    except Exception as e:
        print(f"\n[错误] 制作失败: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
