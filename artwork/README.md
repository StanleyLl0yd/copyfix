# Canonical CopyFix artwork

The project owner approved these two raster files as the exact CopyFix icons in the preceding design discussion. Both are stored **unchanged**, with independent SHA-256 checksums.

| Source | Dimensions | SHA-256 |
| --- | --- | --- |
| `app-icon.png` | 1254×1254 RGB PNG | `24be921d182bc7030fea1290167043fc597cd266a0da167cc963eb3cf73c25a1` |
| `tray-16x16.png` | 16×16 RGBA PNG | `d0d26a34c55bec5ffd258cc4f7ad9191b934ad433a71f6feeb5cde96ca2f6e64` |

The full-size source shows a blue rounded square, white document with “Aa”, and a bright green arrow. The separately approved small source retains the bold blue/white/green silhouette without small letters. They are independent, canonical image files; never generate a new source from a screenshot or a comparison sheet.

`resources/copyfix.ico` is the generated derivative with raster entries 16/24/32/48/64/128/256 px. The **16×16 PNG entry is byte-identical** to `tray-16x16.png`; larger sizes use resampling from `app-icon.png`. All output is embedded into the portable EXE as a native resource. No runtime files are needed.

Verify: `python tools/check-icon.py` (Python 3 standard library only). Intentional regeneration only: `python tools/build-icon.py` (requires Pillow; no build-time or runtime dependency). Do not overwrite either canonical PNG.

Changing artwork is a separate owner-approved task and requires updating hashes, derivative resources, tests, documentation and Windows visual sign-off.
