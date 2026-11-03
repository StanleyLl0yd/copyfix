#!/usr/bin/env python3
"""Validate the exact approved PNG bytes and raster ICO directory (stdlib)."""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
MASTER = ROOT / "artwork/app-icon.png"
TRAY = ROOT / "artwork/tray-16x16.png"
ICO = ROOT / "resources/copyfix.ico"
SHA = {
    MASTER: "24be921d182bc7030fea1290167043fc597cd266a0da167cc963eb3cf73c25a1",
    TRAY: "d0d26a34c55bec5ffd258cc4f7ad9191b934ad433a71f6feeb5cde96ca2f6e64",
}
SIZES = (16, 24, 32, 48, 64, 128, 256)
PNG_SIG = b"\x89PNG\r\n\x1a\n"


def png_size(data: bytes) -> tuple[int, int]:
    if len(data) < 24 or data[:8] != PNG_SIG or data[12:16] != b"IHDR":
        raise ValueError("ICO contains an invalid PNG payload")
    return struct.unpack_from(">II", data, 16)


def main() -> None:
    for path, expected in SHA.items():
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise ValueError(f"Canonical file modified: {path} {actual}")
    assert png_size(TRAY.read_bytes()) == (16, 16)
    raw = ICO.read_bytes()
    reserved, typ, count = struct.unpack_from("<HHH", raw)
    if (reserved, typ, count) != (0, 1, len(SIZES)):
        raise ValueError("Unexpected ICO header or entry count")
    for idx, size in enumerate(SIZES):
        width, height, colors, reserved, planes, depth, length, offset = struct.unpack_from(
            "<BBBBHHII", raw, 6 + idx * 16
        )
        if width != (size & 0xFF) or height != (size & 0xFF) or offset < 6 + 16 * count:
            raise ValueError(f"Incorrect ICO directory entry at size {size}")
        if colors or reserved or planes != 1 or depth != 32 or length <= 24:
            raise ValueError(f"Invalid ICO pixel metadata at size {size}")
        if offset + length > len(raw):
            raise ValueError("ICO payload exceeds file")
        payload = raw[offset:offset + length]
        if png_size(payload) != (size, size):
            raise ValueError(f"ICO PNG dimension mismatch: {size}")
        if size == 16 and payload != TRAY.read_bytes():
            raise ValueError("16x16 ICO entry is not byte-for-byte the approved PNG")
    print("PASS canonical SHA-256 and seven raster ICO sizes; exact 16x16 tray PNG")


if __name__ == "__main__":
    main()
