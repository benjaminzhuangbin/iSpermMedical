#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
iSperm Medical 设备与 QC USB DISK 一对一专属绑定授权签发工具
=============================================================
(qc_device_binder.py)

功能与安全架构：
1. 本工具仅运行在公司内部受控电脑，绝不交付给客户或拷贝至 U 盘。
2. 负责签发 device_binding.lic 凭据，使用公司官方 RSA-2048 私钥进行数字签名。
3. 严格绑定：
   - 目标仪器唯一物理编号 (device_id，如 ISPERM-DEV-3A8F91B2-C4D5E6F7)
   - 官方配发 U 盘的主控物理芯片序列号 (usb_hardware_id，如 USB-HW-090C-1000-AA0401270001)
4. 防克隆：如果客户把 U 盘全部文件复制到普通 U 盘，因为物理芯片序列号不同，比对直接失败。
5. 内容解耦：device_binding.lic 只对“仪器编号 <-> USB介质身份”进行担保，不绑定具体的视频哈希；
   客户后续在 U 盘中删除 Video-2 或新增 Video-3，绑定均永久有效！
"""

import argparse
import base64
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path

try:
    from cryptography.hazmat.primitives.asymmetric import rsa, padding
    from cryptography.hazmat.primitives import hashes, serialization
    HAS_CRYPTO = True
except ImportError:
    HAS_CRYPTO = False


def canonical_json(obj) -> bytes:
    """生成确定性、无空白紧凑格式的 JSON 字节流，保证签名哈希一致性"""
    return json.dumps(obj, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")


def load_private_key(key_dir: Path):
    priv_path = key_dir / "company_signing_rsa_private.pem"
    if not priv_path.exists():
        raise FileNotFoundError(f"未找到公司私钥：{priv_path}，请先运行 qc_video_builder_final.py 初始化生成密钥！")
    return serialization.load_pem_private_key(priv_path.read_bytes(), password=None)


def sign_device_binding(device_id: str, usb_hardware_id: str, output_path: Path, key_dir: Path, customer_name: str = ""):
    if not HAS_CRYPTO:
        raise RuntimeError("缺少 cryptography 模块，请先安装：pip install cryptography")

    device_id = device_id.strip()
    usb_hardware_id = usb_hardware_id.strip()

    if not device_id:
        raise ValueError("device_id 不能为空！")
    if not usb_hardware_id:
        raise ValueError("usb_hardware_id 不能为空！")

    private_key = load_private_key(key_dir)

    binding_info = {
        "format": "ISPERM-BIND-V1",
        "device_id": device_id,
        "usb_hardware_id": usb_hardware_id,
        "customer": customer_name,
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "issuer": "iSperm Medical Authority"
    }

    # 规范化确定性 JSON 数据用于签名
    payload_bytes = canonical_json(binding_info)

    # 使用公司 RSA-2048 私钥签署 (SHA256withRSA)
    signature = private_key.sign(
        payload_bytes,
        padding.PKCS1v15(),
        hashes.SHA256()
    )
    sig_b64 = base64.b64encode(signature).decode("ascii")

    license_doc = {
        "format": "ISPERM-BIND-V1",
        "binding_info": binding_info,
        "signature_algorithm": "SHA256withRSA",
        "signature_base64": sig_b64
    }

    # 如果 output_path 是目录，则在目录下创建 device_binding.lic
    if output_path.is_dir():
        target_file = output_path / "device_binding.lic"
    else:
        target_file = output_path

    target_file.parent.mkdir(parents=True, exist_ok=True)
    target_file.write_bytes(canonical_json(license_doc))

    print(f"\n=======================================================")
    print(f"  ★ 一对一专属绑定授权凭据签发成功！")
    print(f"  ★ 目标仪器编号:   {device_id}")
    print(f"  ★ 绑定介质芯片:   {usb_hardware_id}")
    if customer_name:
        print(f"  ★ 客户单位信息:   {customer_name}")
    print(f"  ★ 凭据保存路径:   {target_file.resolve()}")
    print(f"=======================================================\n")
    print("【交付使用指引】：")
    print("1. 将生成的 [ device_binding.lic ] 直接拷贝至官方配发 U 盘根目录（或 QC-Video 目录下）。")
    print("2. 只要该 U 盘插入目标编号的精子分析仪，仪器即刻通过专属认证并允许导入质控视频！")
    print("3. 若插入其他仪器，或将文件复制到其他普通 U 盘，系统均会自动安全拦截。")


def main():
    parser = argparse.ArgumentParser(
        description="iSperm Medical 精子分析仪与 QC USB DISK 一对一专属绑定授权签发工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""示例用法:
  # 场景1：新机发货或加购U盘，签发绑定凭据并直接写入U盘
  python qc_device_binder.py --device-id "ISPERM-DEV-3A8F91B2-C4D5E6F7" --usb-id "USB-HW-090C-1000-AA0401270001" --output "E:\\"

  # 场景2：为远程客户的分析仪签发绑定凭据（通过邮件发送或远程复制）
  python qc_device_binder.py --device-id "ISPERM-DEV-3A8F91B2-C4D5E6F7" --usb-id "USB-HW-090C-1000-AA0401270001" --output "D:\\QC-Licenses" --customer "市第一人民医院生殖科"
        """
    )
    parser.add_argument("--device-id", required=True, help="精子分析仪物理唯一设备编号 (例如 ISPERM-DEV-3A8F91B2-C4D5E6F7)")
    parser.add_argument("--usb-id", required=True, help="配套 U 盘的主控物理芯片序列号 (例如 USB-HW-090C-1000-AA0401270001)")
    parser.add_argument("--output", required=True, help="输出目录或具体文件路径 (可直接指定U盘根目录 E:\\ 或本地目录)")
    parser.add_argument("--customer", default="", help="客户单位或医院名称 (可选备注)")
    parser.add_argument("--key-dir", default=None, help="公司私钥目录 (默认保存在 company_keys 目录)")
    args = parser.parse_args()

    output_path = Path(args.output)
    if args.key_dir:
        key_path = Path(args.key_dir)
    else:
        key_path = Path(__file__).resolve().parent / "company_keys"

    try:
        sign_device_binding(args.device_id, args.usb_id, output_path, key_path, args.customer)
    except Exception as e:
        print(f"\n[错误] 签发失败: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
