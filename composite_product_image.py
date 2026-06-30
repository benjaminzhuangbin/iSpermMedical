#!/usr/bin/env python3
"""
Composite: image2 base + image1 instrument + image3 on monitor screen.
- image2 is the BASE — monitor hardware/bezel/stand/shadows kept 100% intact
- Screen interior pixels replaced with image3 (software UI)
- image1 replaces left instrument using edge-guided watershed segmentation
- Zero content modification to image1 or image3
"""
from __future__ import annotations
import argparse
from pathlib import Path
import cv2
import numpy as np
from PIL import Image
from scipy.ndimage import uniform_filter1d

# ── Screen coordinates measured from image2.png (800×337) ─────────────────────
SCREEN_BOX   = (468, 52, 774, 242)   # screen interior, inside bezels
CLEAR_BOX    = (0, 0, 462, 337)      # left zone to clear
INST_ZONE    = (8, 0, 462, 337)      # where to place instrument
MONITOR_TOP  = 34                    # monitor top y in image2
MONITOR_BOT  = 307                   # monitor bottom y (base of stand)


def extract_instrument(path: Path) -> Image.Image:
    """
    Edge-guided watershed + largest-component cleanup.
    Preserves original pixel colors; only background is made transparent.
    """
    img_bgr = cv2.imread(str(path))
    img_rgb = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
    h, w = img_bgr.shape[:2]
    gray = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2GRAY)

    # --- Watershed ---
    markers = np.zeros((h, w), dtype=np.int32)
    b = 5
    markers[:b,:]=1; markers[h-b:,:]=1; markers[:,:b]=1; markers[:,w-b:]=1
    dark = (gray < 60)
    fg_seed = cv2.dilate(dark.astype(np.uint8)*255, np.ones((15,15),np.uint8))
    markers[fg_seed > 0] = 2
    cv2.watershed(img_bgr, markers)
    fg = (markers == 2).astype(np.uint8) * 255
    fg = cv2.morphologyEx(fg, cv2.MORPH_CLOSE, np.ones((30,30),np.uint8))
    fg = cv2.morphologyEx(fg, cv2.MORPH_DILATE, np.ones((8,8),np.uint8))

    # --- Keep largest connected component ---
    n, labels, stats, _ = cv2.connectedComponentsWithStats(fg, connectivity=8)
    largest = 1 + np.argmax(stats[1:, cv2.CC_STAT_AREA])
    clean = np.where(labels == largest, 255, 0).astype(np.uint8)

    # --- Clip right side row-by-row based on front panel boundary ---
    body_right = np.zeros(h, dtype=int)
    for y in range(h):
        cols = np.where(dark[y,:])[0]
        body_right[y] = min(cols.max() + 60, w-1) if len(cols) else w-1
    body_right = uniform_filter1d(body_right.astype(float), size=40).astype(int)
    for y in range(h):
        clean[y, body_right[y]:] = 0
    clean[:, :8] = 0

    clean = cv2.morphologyEx(clean, cv2.MORPH_CLOSE, np.ones((10,10),np.uint8))
    clean = cv2.GaussianBlur(clean, (13,13), 0)

    rgba = np.dstack([img_rgb, clean])
    return Image.fromarray(rgba, "RGBA")


def fit_cover(img: Image.Image, tw: int, th: int) -> Image.Image:
    sw, sh = img.size
    scale = max(tw/sw, th/sh)
    nw, nh = int(sw*scale+.5), int(sh*scale+.5)
    r = img.resize((nw, nh), Image.Resampling.LANCZOS)
    x0, y0 = (nw-tw)//2, (nh-th)//2
    return r.crop((x0, y0, x0+tw, y0+th))


def composite(
    instrument_path: Path,
    template_path: Path,
    software_path: Path,
    output_path: Path,
    scale: int = 4,
    inst_ratio: float = 0.83,
) -> Path:
    # ── Load & scale up ────────────────────────────────────────────────────────
    template = Image.open(template_path).convert("RGB")
    tw, th = template.size
    base = template.resize((tw*scale, th*scale), Image.Resampling.LANCZOS)

    S = lambda box: tuple(v*scale for v in box)

    # ── Step 1: Paste software UI onto monitor screen ──────────────────────────
    sx0,sy0,sx1,sy1 = S(SCREEN_BOX)
    software = Image.open(software_path).convert("RGB")
    base.paste(fit_cover(software, sx1-sx0, sy1-sy0), (sx0,sy0))

    # ── Step 2: Clear left instrument zone ────────────────────────────────────
    cx0,cy0,cx1,cy1 = S(CLEAR_BOX)
    base.paste(Image.new("RGB",(cx1-cx0,cy1-cy0),(255,255,255)),(cx0,cy0))

    # ── Step 3: Paste instrument ───────────────────────────────────────────────
    inst = extract_instrument(instrument_path)
    monitor_h = (MONITOR_BOT - MONITOR_TOP) * scale
    target_h = int(monitor_h * inst_ratio)
    iw, ih = inst.size
    target_w = int(iw * target_h / ih)
    inst = inst.resize((target_w, target_h), Image.Resampling.LANCZOS)

    iz_x0,iz_y0,iz_x1,iz_y1 = S(INST_ZONE)
    # Center in zone, bottom-aligned with image2 ground line
    px = iz_x0 + max(0, (iz_x1-iz_x0-target_w)//2)
    ground_y = int(MONITOR_BOT * scale * 0.96)
    py = ground_y - target_h
    base.paste(inst, (px, py), inst)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    base.save(str(output_path), quality=96)
    return output_path


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--instrument", default="image1.png")
    p.add_argument("--template",   default="image2.png")
    p.add_argument("--software",   default="image3.png")
    p.add_argument("--output",     default="isperm-product-composite.png")
    p.add_argument("--scale",      type=int,   default=4)
    p.add_argument("--inst-ratio", type=float, default=0.83)
    args = p.parse_args()
    out = composite(
        Path(args.instrument), Path(args.template),
        Path(args.software),   Path(args.output),
        scale=args.scale, inst_ratio=args.inst_ratio,
    )
    print(f"Saved: {out}")

if __name__ == "__main__":
    main()
