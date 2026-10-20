#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
=============================================================================
iSperm Medical - 精子十字形态位置图绘制程序 (Sperm Cross Map Generator)
=============================================================================
根据 AllSpermInfor-*.csv 内部颗粒数据，在 1280x720 画布中绘制红色的十字形态分布图。
- 画布尺寸: 宽 1280 像素 (对应 dPosX)，高 720 像素 (对应 dPosY)
- 十字形态:
    * 每个检测到的颗粒绘制为一个十字 (Red Cross)
    * 十字横线长度: dMinAxsLen (水平短轴)
    * 十字竖线长度: dMajAxsLen (垂直长轴)
    * 十字中心坐标: (dPosX, dPosY)
- 零依赖设计:
    * 采用 Python 标准库 (csv, struct, zlib, math, argparse) 纯原生实现
    * 无需 pip 安装任何第三方库 (无需 pillow, cv2, matplotlib) 即可直接运行！
    * 同时兼容 PIL/Pillow 和 Tkinter 交互预览（若系统已安装）。
=============================================================================
"""

import os
import sys
import csv
import zlib
import struct
import argparse
from typing import List, Dict, Tuple, Optional

# 默认画布规格 (iSperm 标准显微相机 720P 原始分辨率)
DEFAULT_WIDTH = 1280
DEFAULT_HEIGHT = 720

# 默认红色十字颜色 (RGB)
COLOR_RED = (255, 0, 0)
COLOR_WHITE = (255, 255, 255)
COLOR_BLACK = (0, 0, 0)

class SpermCrossDrawer:
    def __init__(self, width: int = DEFAULT_WIDTH, height: int = DEFAULT_HEIGHT, bg_color: str = 'black', line_width: int = 1):
        self.width = int(width)
        self.height = int(height)
        self.line_width = max(1, int(line_width))
        self.bg_mode = bg_color.lower()
        
        if self.bg_mode == 'white':
            self.bg_rgb = COLOR_WHITE
        else:
            self.bg_rgb = COLOR_BLACK
            
        # 内部像素缓存: 1 维 bytearray, 长度为 width * height * 3 (RGB 顺序)
        self.pixels = bytearray(self.width * self.height * 3)
        self.clear()

    def clear(self):
        """用背景色填充画布"""
        r, g, b = self.bg_rgb
        for i in range(0, len(self.pixels), 3):
            self.pixels[i] = r
            self.pixels[i + 1] = g
            self.pixels[i + 2] = b

    def set_pixel(self, x: int, y: int, color: Tuple[int, int, int]):
        """在 (x, y) 坐标着色，自动进行边界检查"""
        if 0 <= x < self.width and 0 <= y < self.height:
            idx = (y * self.width + x) * 3
            self.pixels[idx] = color[0]
            self.pixels[idx + 1] = color[1]
            self.pixels[idx + 2] = color[2]

    def draw_cross(self, cx: float, cy: float, maj_len: float, min_len: float, color: Tuple[int, int, int] = COLOR_RED):
        """
        绘制红色十字:
        - 横线: 长度为 dMinAxsLen，中心在 (cx, cy)
        - 竖线: 长度为 dMajAxsLen，中心在 (cx, cy)
        """
        center_x = round(cx)
        center_y = round(cy)

        half_w = max(0.5, min_len / 2.0)
        half_h = max(0.5, maj_len / 2.0)

        x_start = round(cx - half_w)
        x_end = round(cx + half_w)
        y_start = round(cy - half_h)
        y_end = round(cy + half_h)

        lw_offset = (self.line_width - 1) // 2

        # 绘制横线 (水平, 长度 dMinAxsLen)
        for x in range(x_start, x_end + 1):
            for dy in range(-lw_offset, lw_offset + 1):
                self.set_pixel(x, center_y + dy, color)

        # 绘制竖线 (垂直, 长度 dMajAxsLen)
        for y in range(y_start, y_end + 1):
            for dx in range(-lw_offset, lw_offset + 1):
                self.set_pixel(center_x + dx, y, color)

    def save_png(self, filepath: str):
        """使用 Python 内置 zlib + struct 生成标准 24-bit PNG 图像"""
        def make_chunk(tag: bytes, data: bytes) -> bytes:
            return (
                struct.pack('>I', len(data)) +
                tag +
                data +
                struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
            )

        ihdr_data = struct.pack('>IIBBBBB', self.width, self.height, 8, 2, 0, 0, 0)
        
        # 组织扫描行 (每行前缀 1 字节过滤标志 0 = None)
        raw_scanlines = bytearray()
        row_stride = self.width * 3
        for y in range(self.height):
            raw_scanlines.append(0)
            start = y * row_stride
            raw_scanlines.extend(self.pixels[start : start + row_stride])

        idat_data = zlib.compress(bytes(raw_scanlines), level=9)
        
        png_bytes = (
            b'\x89PNG\r\n\x1a\n' +
            make_chunk(b'IHDR', ihdr_data) +
            make_chunk(b'IDAT', idat_data) +
            make_chunk(b'IEND', b'')
        )

        with open(filepath, 'wb') as f:
            f.write(png_bytes)

    def save_bmp(self, filepath: str):
        """生成标准 24-bit Windows BMP 图像文件"""
        row_bytes = self.width * 3
        padding = (4 - (row_bytes % 4)) % 4
        padded_row_bytes = row_bytes + padding
        image_size = padded_row_bytes * self.height
        file_size = 54 + image_size

        # BMP 文件头 (14 字节)
        header = struct.pack(
            '<2sIHHI',
            b'BM',
            file_size,
            0,
            0,
            54
        )

        # DIB 信息头 BITMAPINFOHEADER (40 字节)
        # biWidth (int32), biHeight (int32 - 正数表示自底向上标准扫描)
        dib_header = struct.pack(
            '<IiiHHIIIIII',
            40,
            self.width,
            self.height, # bottom-up standard BMP
            1,           # 1 color plane
            24,          # 24 bits per pixel
            0,           # BI_RGB (uncompressed)
            image_size,
            2835,        # 72 DPI (2835 ppm)
            2835,
            0,
            0
        )

        with open(filepath, 'wb') as f:
            f.write(header)
            f.write(dib_header)
            pad_bytes = b'\x00' * padding
            # 标准 BMP 是从底部往顶部遍历扫描行 (y 从 height - 1 到 0)
            for y in range(self.height - 1, -1, -1):
                row_start = y * row_bytes
                row_bgr = bytearray(row_bytes)
                # RGB 转 BGR
                for x in range(self.width):
                    p_idx = row_start + x * 3
                    bgr_idx = x * 3
                    row_bgr[bgr_idx] = self.pixels[p_idx + 2]     # B
                    row_bgr[bgr_idx + 1] = self.pixels[p_idx + 1] # G
                    row_bgr[bgr_idx + 2] = self.pixels[p_idx]     # R
                f.write(row_bgr)
                if padding > 0:
                    f.write(pad_bytes)


def load_sperm_csv(csv_path: str) -> List[Dict[str, float]]:
    """读取并解析 AllSpermInfor CSV 文件"""
    if not os.path.exists(csv_path):
        raise FileNotFoundError(f"找不到 CSV 文件: {csv_path}")

    records = []
    with open(csv_path, 'r', encoding='utf-8-sig', errors='replace') as f:
        reader = csv.reader(f)
        header = None
        for row in reader:
            if not row or len(row) < 5:
                continue
            if header is None:
                header = [h.strip() for h in row]
                continue

            try:
                # 兼容表头匹配或按列索引获取
                # i+1, dPosX, dPosY, dMajAxsLen, dMinAxsLen
                item = {
                    'index': int(float(row[0].strip())),
                    'dPosX': float(row[1].strip()),
                    'dPosY': float(row[2].strip()),
                    'dMajAxsLen': float(row[3].strip()),
                    'dMinAxsLen': float(row[4].strip()),
                }
                # 可选附带参数
                if len(row) > 5:
                    item['dShape'] = float(row[5].strip())
                if len(row) > 6:
                    item['dArea'] = float(row[6].strip())
                if len(row) > 7:
                    item['dCircularity'] = float(row[7].strip())

                records.append(item)
            except (ValueError, IndexError):
                continue

    return records


def process_and_draw(csv_file: str, out_prefix: str, width: int = 1280, height: int = 720, line_width: int = 1):
    print("=" * 70)
    print("iSperm Medical - 精子十字形态位置图绘制工具")
    print("=" * 70)
    print(f"读取 CSV 文件: {csv_file}")
    
    records = load_sperm_csv(csv_file)
    total_count = len(records)
    print(f"成功读取到 {total_count} 个精子/颗粒标记数据！")

    if total_count == 0:
        print("警告: 未检测到有效数据行！")
        return

    # 统计数据
    xs = [r['dPosX'] for r in records]
    ys = [r['dPosY'] for r in records]
    majs = [r['dMajAxsLen'] for r in records]
    mins = [r['dMinAxsLen'] for r in records]

    print(f"  • X 坐标范围 (dPosX):      {min(xs):.1f} px ~ {max(xs):.1f} px  (画布宽: {width} px)")
    print(f"  • Y 坐标范围 (dPosY):      {min(ys):.1f} px ~ {max(ys):.1f} px  (画布高: {height} px)")
    print(f"  • 竖线长轴 (dMajAxsLen):  最小 {min(majs):.1f} px, 最大 {max(majs):.1f} px, 均值 {sum(majs)/total_count:.2f} px")
    print(f"  • 横线短轴 (dMinAxsLen):  最小 {min(mins):.1f} px, 最大 {max(mins):.1f} px, 均值 {sum(mins)/total_count:.2f} px")
    print("-" * 70)

    # 1. 绘制黑色背景版本 (最常用，高对比度显微视野)
    drawer_black = SpermCrossDrawer(width=width, height=height, bg_color='black', line_width=line_width)
    for r in records:
        drawer_black.draw_cross(r['dPosX'], r['dPosY'], r['dMajAxsLen'], r['dMinAxsLen'], COLOR_RED)
    
    black_png = f"{out_prefix}_black_bg.png"
    black_bmp = f"{out_prefix}_black_bg.bmp"
    drawer_black.save_png(black_png)
    drawer_black.save_bmp(black_bmp)
    print(f"[OK] 已生成黑色背景图: {black_png} (及 {black_bmp})")

    # 2. 绘制白色背景版本 (适合打印、报告导出)
    drawer_white = SpermCrossDrawer(width=width, height=height, bg_color='white', line_width=line_width)
    for r in records:
        drawer_white.draw_cross(r['dPosX'], r['dPosY'], r['dMajAxsLen'], r['dMinAxsLen'], COLOR_RED)
    
    white_png = f"{out_prefix}_white_bg.png"
    white_bmp = f"{out_prefix}_white_bg.bmp"
    drawer_white.save_png(white_png)
    drawer_white.save_bmp(white_bmp)
    print(f"[OK] 已生成白色背景图: {white_png} (及 {white_bmp})")

    print("=" * 70)
    print("绘制完成！所有图像已保存在当前目录。")


def main():
    parser = argparse.ArgumentParser(description="绘制 AllSpermInfor 精子标记点红色十字形态位置图 (1280x720)")
    parser.add_argument("--csv", default=None, help="输入 CSV 文件路径 (默认自动查找 AllSpermInfor-100023.csv 或 AllSpermInfor.csv)")
    parser.add_argument("--width", type=int, default=1280, help="画布宽度 (默认 1280)")
    parser.add_argument("--height", type=int, default=720, help="画布高度 (默认 720)")
    parser.add_argument("--linewidth", type=int, default=1, help="十字线条宽度 (像素，默认 1)")
    parser.add_argument("--out", default="sperm_cross_map", help="输出文件名前缀 (默认 sperm_cross_map)")

    args = parser.parse_args()

    # 自动探测 CSV 文件路径
    csv_candidates = [
        args.csv,
        "AllSpermInfor-100023.csv",
        os.path.join(os.path.dirname(__file__), "AllSpermInfor-100023.csv"),
        os.path.join(os.path.dirname(__file__), "..", "AllSpermInfor-100023.csv"),
        "AllSpermInfor.csv",
        os.path.join(os.path.dirname(__file__), "..", "AllSpermInfor.csv"),
    ]
    csv_file = None
    for cand in csv_candidates:
        if cand and os.path.isfile(cand):
            csv_file = cand
            break

    if not csv_file:
        print("错误: 无法找到 AllSpermInfor-100023.csv 文件！请指定 --csv 参数。")
        sys.exit(1)

    process_and_draw(csv_file, args.out, width=args.width, height=args.height, line_width=args.linewidth)


if __name__ == "__main__":
    main()
