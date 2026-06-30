#!/usr/bin/env python3
"""
Composite product image using original user photos without modifying content.

- image1: instrument (left, scaled slightly smaller than monitor)
- image2: template (monitor hardware kept, screen replaced)
- image3: PC software UI (placed inside monitor screen area)
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image, ImageEnhance, ImageFilter


def load_rgba(path: Path) -> Image.Image:
    img = Image.open(path)
    return img.convert("RGBA") if img.mode != "RGBA" else img


def remove_background(img: Image.Image) -> Image.Image:
    try:
        from rembg import remove
        from io import BytesIO

        result = remove(img.convert("RGBA"))
        if isinstance(result, bytes):
            return Image.open(BytesIO(result)).convert("RGBA")
        return result.convert("RGBA")
    except Exception:
        rgba = img.convert("RGBA")
        data = np.array(rgba)
        rgb = data[:, :, :3].astype(np.float32)
        # image1 has neutral gray studio background
        bg = np.median(rgb.reshape(-1, 3), axis=0)
        dist = np.linalg.norm(rgb - bg, axis=2)
        alpha = np.clip((dist - 12) * 10, 0, 255).astype(np.uint8)
        data[:, :, 3] = np.minimum(data[:, :, 3], alpha)
        return Image.fromarray(data, "RGBA")


def harmonize_lighting(img: Image.Image, brightness: float = 1.04, contrast: float = 1.02) -> Image.Image:
    rgb = ImageEnhance.Brightness(img.convert("RGB")).enhance(brightness)
    rgb = ImageEnhance.Contrast(rgb).enhance(contrast)
    if img.mode == "RGBA":
        out = rgb.convert("RGBA")
        out.putalpha(img.split()[3])
        return out
    return rgb


def soft_shadow(size: tuple[int, int], blur: int = 16, opacity: int = 45) -> Image.Image:
    core = Image.new("RGBA", size, (0, 0, 0, opacity))
    return core.filter(ImageFilter.GaussianBlur(blur))


def paste_cover(base: Image.Image, overlay: Image.Image, box: tuple[int, int, int, int]) -> None:
    x0, y0, x1, y1 = box
    tw, th = x1 - x0, y1 - y0
    sw, sh = overlay.size
    scale = max(tw / sw, th / sh)
    resized = overlay.resize((int(sw * scale), int(sh * scale)), Image.Resampling.LANCZOS)
    rw, rh = resized.size
    left = (rw - tw) // 2
    top = (rh - th) // 2
    cropped = resized.crop((left, top, left + tw, top + th))
    base.paste(cropped, (x0, y0), cropped if cropped.mode == "RGBA" else None)


def composite(
    instrument_path: Path,
    template_path: Path,
    software_path: Path,
    output_path: Path,
    scale_factor: int = 3,
    instrument_scale: float = 0.82,
) -> Path:
    template = load_rgba(template_path)
    instrument = load_rgba(instrument_path)
    software = load_rgba(software_path).convert("RGB")

    tw, th = template.size
    canvas_w, canvas_h = tw * scale_factor, th * scale_factor
    canvas = Image.new("RGB", (canvas_w, canvas_h), (255, 255, 255))

    # image2 layout (measured from source): monitor starts ~x=395
    monitor_crop = template.crop((395, 0, tw, th))
    monitor = monitor_crop.resize(
        (monitor_crop.size[0] * scale_factor, monitor_crop.size[1] * scale_factor),
        Image.Resampling.LANCZOS,
    )

    # Screen area inside monitor (original coords relative to full image2)
    screen_box_orig = (430, 30, 779, 279)
    crop_x0 = 395
    screen_local = tuple(
        (v - crop_x0 if i % 2 == 0 else v) * scale_factor
        for i, v in enumerate(screen_box_orig)
    )

    monitor_rgba = monitor.convert("RGBA")
    paste_cover(monitor_rgba, software, screen_local)
    monitor = monitor_rgba.convert("RGB")

    monitor_x = int(395 * scale_factor)
    monitor_y = 0

    # Instrument: preserve original pixels, remove background, harmonize tone
    instrument_cut = harmonize_lighting(remove_background(instrument))
    inst_target_h = int(monitor.size[1] * instrument_scale)
    inst_scale = inst_target_h / instrument_cut.size[1]
    inst_w = int(instrument_cut.size[0] * inst_scale)
    instrument_resized = instrument_cut.resize((inst_w, inst_target_h), Image.Resampling.LANCZOS)

    inst_x = int(20 * scale_factor)
    inst_y = monitor_y + monitor.size[1] - instrument_resized.size[1]

    # Soft shadows to match image2 studio style
    inst_shadow = soft_shadow((inst_w + 30, 24), blur=14, opacity=40)
    canvas.paste(inst_shadow, (inst_x + 8, inst_y + instrument_resized.size[1] - 6), inst_shadow)
    mon_shadow = soft_shadow((monitor.size[0] + 40, 28), blur=16, opacity=38)
    canvas.paste(mon_shadow, (monitor_x + 12, monitor_y + monitor.size[1] - 5), mon_shadow)

    canvas.paste(instrument_resized, (inst_x, inst_y), instrument_resized)
    canvas.paste(monitor, (monitor_x, monitor_y))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(output_path, quality=95)
    return output_path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--instrument", default="image1.png")
    parser.add_argument("--template", default="image2.png")
    parser.add_argument("--software", default="image3.png")
    parser.add_argument("--output", default="isperm-product-composite.png")
    parser.add_argument("--instrument-scale", type=float, default=0.82)
    args = parser.parse_args()

    out = composite(
        Path(args.instrument),
        Path(args.template),
        Path(args.software),
        Path(args.output),
        instrument_scale=args.instrument_scale,
    )
    print(f"Saved: {out}")


if __name__ == "__main__":
    main()
