#!/usr/bin/env python3
# QC Video USB Builder — prototype
# Requirements: Python 3.10+ and cryptography package
# Install dependency: py -m pip install cryptography

import argparse
import base64
import hashlib
import json
import os
import secrets
import sys
import uuid
import zipfile
from datetime import datetime, timezone
from pathlib import Path

from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization

EXPECTED = [f"{i:03d}.jpg" for i in range(30)]
MAGIC = b"QCV1"

def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def canonical_json(obj) -> bytes:
    return json.dumps(obj, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")

def check_video_folder(folder: Path):
    if not folder.is_dir():
        raise ValueError(f"不是文件夹：{folder}")
    files = [p for p in folder.iterdir() if p.is_file()]
    jpg_names = sorted(p.name for p in files if p.suffix.lower() in (".jpg", ".jpeg"))
    missing = [name for name in EXPECTED if not (folder / name).is_file()]
    unexpected = [name for name in jpg_names if name not in EXPECTED]
    if missing or unexpected:
        parts = []
        if missing:
            parts.append("缺少：" + ", ".join(missing))
        if unexpected:
            parts.append("存在非标准 JPG 名称：" + ", ".join(unexpected))
        raise ValueError(f"{folder.name} 检查失败；" + "；".join(parts))
    if len(jpg_names) != 30:
        raise ValueError(f"{folder.name} 必须恰好有 30 张 JPG/JPEG，当前 {len(jpg_names)} 张")
    return [(folder / name) for name in EXPECTED]

def make_zip_bytes(folder: Path) -> tuple[bytes, dict]:
    images = check_video_folder(folder)
    import io
    buf = io.BytesIO()
    records = []
    with zipfile.ZipFile(buf, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for p in images:
            data = p.read_bytes()
            # Validate file isn't empty; full JPEG decoding is not performed in this prototype.
            if not data:
                raise ValueError(f"空文件：{p}")
            zf.writestr(p.name, data)
            records.append({"name": p.name, "size": len(data), "sha256": sha256(data)})
    return buf.getvalue(), {"video_name": folder.name, "image_count": 30, "images": records}

def load_or_create_signing_key(out_dir: Path):
    # For prototype convenience only. Production: keep the private key offline and never ship it on USB.
    priv_path = out_dir / "company_signing_private.pem"
    pub_path = out_dir / "company_signing_public.pem"
    if priv_path.exists():
        private = serialization.load_pem_private_key(priv_path.read_bytes(), password=None)
    else:
        private = Ed25519PrivateKey.generate()
        priv_path.write_bytes(private.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.PKCS8,
            encryption_algorithm=serialization.NoEncryption()))
        try:
            os.chmod(priv_path, 0o600)
        except OSError:
            pass
    pub_path.write_bytes(private.public_key().public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo))
    return private, pub_path

def build(source_dir: Path, output_dir: Path):
    source_dir = source_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    video_dirs = sorted([p for p in source_dir.iterdir() if p.is_dir() and p.name.lower().startswith("video-")])
    if not video_dirs:
        raise ValueError("源目录下没有找到 Video-1、Video-2 等以 Video- 开头的子文件夹。")

    # Preflight all folders before creating any deliverables.
    prepared = []
    for folder in video_dirs:
        packed, info = make_zip_bytes(folder)
        prepared.append((folder, packed, info))

    usb_id = "QC-" + datetime.now().strftime("%Y%m%d") + "-" + uuid.uuid4().hex[:8].upper()
    content_key = AESGCM.generate_key(bit_length=256)
    aes = AESGCM(content_key)

    package_dir = output_dir / usb_id
    package_dir.mkdir(parents=True, exist_ok=False)

    manifest_videos = []
    for folder, packed, info in prepared:
        nonce = secrets.token_bytes(12)
        aad = (usb_id + "|" + folder.name).encode("utf-8")
        encrypted = MAGIC + nonce + aes.encrypt(nonce, packed, aad)
        out_name = folder.name + ".qcv"
        (package_dir / out_name).write_bytes(encrypted)
        manifest_videos.append({
            "file": out_name,
            "video_name": folder.name,
            "encrypted_sha256": sha256(encrypted),
            "aad": base64.b64encode(aad).decode("ascii"),
            "images": info["images"],
            "image_count": 30
        })

    # The prototype manifest intentionally does not expose the AES content key.
    # The key must be securely delivered to the authorized Android app in the eventual integration.
    manifest = {
        "format": "QCV1",
        "usb_id": usb_id,
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "product": "QC Video",
        "videos": manifest_videos,
        "note": "Prototype manifest. Android key provisioning/integration is not implemented."
    }
    manifest_bytes = canonical_json(manifest)
    (package_dir / "manifest.dat").write_bytes(manifest_bytes)

    private, pub_path = load_or_create_signing_key(output_dir)
    signature = private.sign(manifest_bytes)
    license_obj = {
        "format": "QCL1",
        "usb_id": usb_id,
        "product": "QC Video",
        "license_type": "shared_across_compatible_authorized_instruments",
        "manifest_sha256": sha256(manifest_bytes),
        "signature_algorithm": "Ed25519",
        "signature_base64": base64.b64encode(signature).decode("ascii"),
        "public_key_file_for_development": pub_path.name,
        "note": "Prototype license. Do not put private signing key on USB."
    }
    (package_dir / "license.dat").write_bytes(canonical_json(license_obj))

    # Local development key escrow: necessary only so this standalone prototype can later be wired up.
    # Keep this file OFF the USB; protect it. Production design should use per-package wrapped keys.
    (package_dir / "KEY_DELIVERY_REQUIRED.txt").write_text(
        "This prototype encrypted the video packages with a random AES-256 key.\n"
        "The key was intentionally NOT written to the USB disk.\n"
        "Android playback will not work until the engineering team implements secure key delivery.\n"
        "For this prototype, the content key is not exported, so regenerate the package if needed.\n",
        encoding="utf-8"
    )

    print("\n制作完成（原型）：")
    print(f"  USB 编号：{usb_id}")
    print(f"  输出目录：{package_dir}")
    print(f"  视频包数量：{len(prepared)}")
    print(f"  授权文件：{package_dir / 'license.dat'}")
    print(f"  清单文件：{package_dir / 'manifest.dat'}")
    print(f"  公司私钥（严禁复制到 U 盘）：{output_dir / 'company_signing_private.pem'}")
    print(f"  公司公钥：{pub_path}")
    print("\n重要：此版本用于验证打包/加密/签名流程，不是最终可供 Android 导入的产品版本。")

def main():
    parser = argparse.ArgumentParser(description="QC Video USB 制作工具原型")
    parser.add_argument("--source", required=True, help="包含 Video-1、Video-2 等文件夹的源目录")
    parser.add_argument("--output", required=True, help="输出目录（建议先输出到电脑本地，再复制到 U 盘）")
    args = parser.parse_args()
    try:
        build(Path(args.source), Path(args.output))
    except Exception as e:
        print(f"\n错误：{e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
