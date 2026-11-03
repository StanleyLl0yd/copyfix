#!/usr/bin/env python3
"""Regenerate the CopyFix ICO from immutable, approved raster artwork.

Requires Pillow only when intentionally regenerating an icon; building CopyFix
uses the already committed ICO and needs no Python or Pillow dependency.
"""
from __future__ import annotations

import hashlib
import io
from pathlib import Path
import struct

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
MASTER = ROOT / "artwork" / "app-icon.png"
TRAY = ROOT / "artwork" / "tray-16x16.png"
OUTPUT = ROOT / "resources" / "copyfix.ico"
SIZES = (16, 24, 32, 48, 64, 128, 256)
MASTER_SHA256 = "24be921d182bc7030fea1290167043fc597cd266a0da167cc963eb3cf73c25a1"
TRAY_SHA256 = "d0d26a34c55bec5ffd258cc4f7ad9191b934ad433a71f6feeb5cde96ca2f6e64"


def verify_source(path: Path, expected: str) -> None:
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != expected:
        raise ValueError(f"Canonical source changed: {path.name} {actual}")


def generate() -> None:
    verify_source(MASTER, MASTER_SHA256)
    verify_source(TRAY, TRAY_SHA256)
    with Image.open(MASTER) as master, Image.open(TRAY) as tray:
        if master.width != master.height or master.width < 256:
            raise ValueError("Canonical icon must be a square raster >= 256 px")
        if tray.size != (16, 16):
            raise ValueError("Approved tray artwork must be exactly 16x16")
        images: list[bytes] = []
        for size in SIZES:
            if size == 16:
                # No re-encoding: this is the exact approved 16x16 PNG.
                images.append(TRAY.read_bytes())
            else:
                raster = master.convert("RGBA").resize((size, size), Image.Resampling.LANCZOS)
                stream = io.BytesIO()
                raster.save(stream, format="PNG")
                images.append(stream.getvalue())
    directory = struct.pack("<HHH", 0, 1, len(SIZES))
    offset = 6 + len(SIZES) * 16
    for size, data in zip(SIZES, images, strict=True):
        directory += struct.pack("<BBBBHHII", size & 0xFF, size & 0xFF, 0, 0,
                                 1, 32, len(data), offset)
        offset += len(data)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes(directory + b"".join(images))
    print(f"ICO: {OUTPUT} ({offset} bytes); sizes={SIZES}")
    print("Master SHA-256:", MASTER_SHA256)
    print("Tray SHA-256:", TRAY_SHA256)


if __name__ == "__main__":
    generate()
