#!/usr/bin/env python3
"""Generate qlocate application icons from the master artwork.

Steps:
  1. Flood-fill the white studio background from the image border and turn it
     into transparency (anti-aliased edge pixels get partial alpha and are
     un-premultiplied against white, so no light fringe remains).
  2. Crop to the artwork bounding box and normalize it to a square.
  3. Emit:
       - resources/icon/qlocate.png            full-bleed master (1024)
       - resources/icon/qlocate_{256,64,32}.png  small sizes for QIcon
       - resources/icon/qlocate.icns           macOS bundle icon (824/1024 inset)
       - resources/icon/qlocate.ico            Windows icon

Requires Pillow. `iconutil` (macOS) is used for the .icns when available.

Usage: python3 scripts/make_icons.py [source.png]
"""
from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "resources" / "icon"
SOURCE = OUT_DIR / "qlocate-source.png"

# A pixel is "background" while its distance from pure white stays below this.
BG_TOLERANCE = 110
# Alpha ramp: fully transparent at or below BG_FLOOR, opaque at BG_TOLERANCE.
# The master render has a ~1px anti-aliased edge and faint sensor noise in the
# white surround, so the floor discards the noise without eating the edge.
BG_FLOOR = 14


def strip_white_background(img: Image.Image) -> Image.Image:
    """Replace the white surround with alpha, keeping anti-aliased edges clean."""
    rgb = np.asarray(img.convert("RGB")).astype(np.int16)
    # Distance from white: 0 for pure white, up to 255 for saturated colors.
    dist = 255 - rgb.min(axis=2)

    # Flood fill the near-white region that is connected to the image border.
    # .copy() is required: images built by fromarray wrap a read-only buffer and
    # in-place ImageDraw writes would be discarded.
    seed = Image.fromarray(
        np.where(dist < BG_TOLERANCE, 255, 0).astype(np.uint8), "L"
    ).copy()
    h, w = dist.shape
    for xy in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)):
        if seed.getpixel(xy) == 255:
            ImageDraw.floodfill(seed, xy, 128, thresh=0)
    outside = np.asarray(seed) == 128

    alpha = np.full((h, w), 255, dtype=np.float32)
    ramp = np.clip((dist - BG_FLOOR) / (BG_TOLERANCE - BG_FLOOR), 0.0, 1.0) * 255.0
    alpha[outside] = ramp[outside]

    # Un-premultiply against the white background: observed = a*src + (1-a)*255
    a = (alpha / 255.0)[..., None]
    src = np.where(a > 0.004, (rgb - (1.0 - a) * 255.0) / np.maximum(a, 0.004), 0.0)
    out = np.dstack([np.clip(src, 0, 255).astype(np.uint8), alpha.astype(np.uint8)])
    return Image.fromarray(out, "RGBA")


def crop_square(img: Image.Image) -> Image.Image:
    """Crop to the visible artwork and normalize to a square canvas."""
    bbox = img.getchannel("A").point(lambda v: 255 if v > 24 else 0).getbbox()
    if bbox is None:
        raise SystemExit("source image has no visible content")
    cropped = img.crop(bbox)
    side = max(cropped.size)
    # The artwork is a rounded square; resample to an exact square so it is not
    # padded asymmetrically (the master render is off by ~3% vertically).
    return cropped.resize((side, side), Image.LANCZOS)


def macos_canvas(art: Image.Image, size: int) -> Image.Image:
    """Place the artwork on a transparent canvas using Apple's 824/1024 inset."""
    inset = round(size * 824 / 1024)
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.paste(art.resize((inset, inset), Image.LANCZOS), ((size - inset) // 2,) * 2)
    return canvas


def build_icns(art: Image.Image, dest: Path) -> bool:
    if not shutil.which("iconutil"):
        print("iconutil not found; skipping .icns")
        return False
    with tempfile.TemporaryDirectory() as tmp:
        iconset = Path(tmp) / "qlocate.iconset"
        iconset.mkdir()
        for base in (16, 32, 128, 256, 512):
            macos_canvas(art, base).save(iconset / f"icon_{base}x{base}.png")
            macos_canvas(art, base * 2).save(iconset / f"icon_{base}x{base}@2x.png")
        subprocess.run(
            ["iconutil", "-c", "icns", str(iconset), "-o", str(dest)], check=True
        )
    return True


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    src_arg = sys.argv[1] if len(sys.argv) > 1 else None
    if src_arg:
        shutil.copyfile(src_arg, SOURCE)
    if not SOURCE.exists():
        raise SystemExit(f"missing source artwork: {SOURCE}")

    art = crop_square(strip_white_background(Image.open(SOURCE)))
    print(f"artwork cropped to {art.size[0]}x{art.size[0]}")

    master = art.resize((1024, 1024), Image.LANCZOS)
    master.save(OUT_DIR / "qlocate.png")
    for size in (256, 64, 32):
        art.resize((size, size), Image.LANCZOS).save(OUT_DIR / f"qlocate_{size}.png")

    # macOS Dock/window icon for an unbundled binary: same inset as the .icns so
    # it matches the visual weight of other Dock icons.
    macos_canvas(art, 512).save(OUT_DIR / "qlocate_mac.png")

    build_icns(art, OUT_DIR / "qlocate.icns")

    master.save(
        OUT_DIR / "qlocate.ico",
        sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)],
    )
    print(f"icons written to {OUT_DIR}")


if __name__ == "__main__":
    main()
