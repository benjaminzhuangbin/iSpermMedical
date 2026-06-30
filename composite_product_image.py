#!/usr/bin/env python3
"""
Composite product image — preserves original photo content exactly.

Strategy:
  1. Use image2 as the base canvas (keeps monitor hardware, stand, lighting, shadows).
  2. Replace ONLY the monitor screen pixels with image3.
  3. Replace the left instrument area with image1 (gentle bg removal, no re-lighting).
"""

from __future__ import annotations

import argparse
from collections import deque
from pathlib import Path

import numpy as np
from PIL import Image


# Measured from image2.png (800×337)
LEFT_INSTRUMENT_BOX = (20, 22, 384, 323)
SCREEN_BOX = (445, 45, 764, 254)
MONITOR_HEIGHT = 307 - 34


def load_rgb(path: Path) -> Image.Image:
    return Image.open(path).convert("RGB")


def load_rgba(path: Path) -> Image.Image:
    img = Image.open(path)
    return img.convert("RGBA") if img.mode != "RGBA" else img


def upscale_box(box: tuple[int, int, int, int], factor: int) -> tuple[int, int, int, int]:
    return tuple(v * factor for v in box)


def remove_instrument_background(img: Image.Image, tolerance: float = 22.0) -> Image.Image:
    """Flood-fill background removal; preserves rear shading and floor reflection."""
    rgb = np.array(img.convert("RGB"))
    h, w = rgb.shape[:2]
    bg_mask = np.zeros((h, w), dtype=bool)
    visited = np.zeros((h, w), dtype=bool)

    def flood(seed_y: int, seed_x: int) -> None:
        queue: deque[tuple[int, int]] = deque([(seed_y, seed_x)])
        ref = rgb[seed_y, seed_x].astype(np.float32)
        visited[seed_y, seed_x] = True
        bg_mask[seed_y, seed_x] = True
        while queue:
            cy, cx = queue.popleft()
            for ny, nx in ((cy - 1, cx), (cy + 1, cx), (cy, cx - 1), (cy, cx + 1)):
                if 0 <= ny < h and 0 <= nx < w and not visited[ny, nx]:
                    if np.linalg.norm(rgb[ny, nx].astype(np.float32) - ref) < tolerance:
                        visited[ny, nx] = True
                        bg_mask[ny, nx] = True
                        queue.append((ny, nx))

    for sy, sx in ((0, 0), (0, w - 1), (h - 1, 0), (h - 1, w - 1)):
        if not visited[sy, sx]:
            flood(sy, sx)

    alpha = np.where(bg_mask, 0, 255).astype(np.uint8)
    return Image.fromarray(np.dstack([rgb, alpha]), "RGBA")


def clear_region(base: Image.Image, box: tuple[int, int, int, int]) -> None:
    x0, y0, x1, y1 = box
    base.paste(Image.new("RGB", (x1 - x0, y1 - y0), (255, 255, 255)), (x0, y0))


def paste_cover(base: Image.Image, overlay: Image.Image, box: tuple[int, int, int, int]) -> None:
    """Fill screen area with software screenshot (cover, center-crop)."""
    x0, y0, x1, y1 = box
    tw, th = x1 - x0, y1 - y0
    sw, sh = overlay.size
    scale = max(tw / sw, th / sh)
    resized = overlay.resize((int(sw * scale), int(sh * scale)), Image.Resampling.LANCZOS)
    rw, rh = resized.size
    cropped = resized.crop(((rw - tw) // 2, (rh - th) // 2, (rw + tw) // 2, (rh + th) // 2))
    base.paste(cropped, (x0, y0))


def paste_instrument(base: Image.Image, instrument: Image.Image, box: tuple[int, int, int, int], target_height: int) -> None:
    x0, y0, x1, y1 = box
    scale = target_height / instrument.size[1]
    nw, nh = int(instrument.size[0] * scale), target_height
    resized = instrument.resize((nw, nh), Image.Resampling.LANCZOS)
    px = x0 + max(0, ((x1 - x0) - nw) // 2)
    py = y1 - nh
    base.paste(resized, (px, py), resized)


def composite(
    instrument_path: Path,
    template_path: Path,
    software_path: Path,
    output_path: Path,
    scale_factor: int = 4,
    instrument_scale: float = 0.84,
) -> Path:
    template = load_rgb(template_path)
    tw, th = template.size
    base = template.resize((tw * scale_factor, th * scale_factor), Image.Resampling.LANCZOS)

    instrument = remove_instrument_background(load_rgba(instrument_path))
    software = load_rgb(software_path)

    left_box = upscale_box(LEFT_INSTRUMENT_BOX, scale_factor)
    screen_box = upscale_box(SCREEN_BOX, scale_factor)

    clear_region(base, left_box)
    paste_cover(base, software, screen_box)

    inst_h = int(MONITOR_HEIGHT * scale_factor * instrument_scale)
    paste_instrument(base, instrument, left_box, inst_h)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    base.save(output_path, quality=96)
    return output_path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--instrument", default="image1.png")
    parser.add_argument("--template", default="image2.png")
    parser.add_argument("--software", default="image3.png")
    parser.add_argument("--output", default="isperm-product-composite.png")
    parser.add_argument("--instrument-scale", type=float, default=0.84)
    parser.add_argument("--scale", type=int, default=4)
    args = parser.parse_args()

    out = composite(
        Path(args.instrument),
        Path(args.template),
        Path(args.software),
        Path(args.output),
        scale_factor=args.scale,
        instrument_scale=args.instrument_scale,
    )
    print(f"Saved: {out}")


if __name__ == "__main__":
    main()
