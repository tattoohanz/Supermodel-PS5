#!/usr/bin/env bash
# Supermodel PS5: the menu's art, cut to the size the menu draws it at on a
# 3840x2160 display, so the console decodes small PNGs rather than the
# multi-megabyte originals. From ps5/assets/menu-src/ into ps5/assets/menu/,
# which build-title.sh copies to the title's Assets/Menu/. Neither folder is
# in git (whose the art is, is not settled).
#
#   menu-src/boxart/<set>.png   box art, any size; its transparent border is
#                               trimmed, the rest scaled to 1040 px high
#   menu-src/marquee/<set>.*    the cabinet marquee (png or jpg), scaled to
#                               848 px wide
#   menu-src/logo/<set>.png     the game's clear logo; trimmed, fitted into
#                               1000x260 (at most 2.5 times its size)
#
# Made from those: backdrop/<set>.png, the box art blurred and darkened to
# fill the large frame behind it; and header/logo.png, the menu's own logo
# (SEGA MODEL 3) from ps5/assets/art-src/logo-clear.png, 116 px high.
#
# <set> is the ROM set name, or one Menu.cpp maps to it (bass, scudp, ...).
#
# Usage: ps5/prepare-menu-art.sh

set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
src=$here/assets/menu-src
out=$here/assets/menu

[[ -d $src ]] || { echo "missing: $src" >&2; exit 2; }

python3 - "$src" "$out" "$here/assets/art-src/logo-clear.png" <<'PY'
import sys
from pathlib import Path
from PIL import Image, ImageEnhance, ImageFilter

src, out, header = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])

def trimmed(image):
    image = image.convert("RGBA")
    box = image.getchannel("A").getbbox()
    return image.crop(box) if box else image

def save(image, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    image.convert("RGBA").save(path, optimize=True)  # never interlaced: Util::PNG reads no Adam7

counts = {}
for kind in ("boxart", "marquee", "logo", "backdrop", "header"):
    if (out / kind).is_dir():
        for old in (out / kind).glob("*.png"):
            old.unlink()

def backdrop(box, path):
    # Covers the 1096x568 frame (in 1080p units) at a quarter of its 4K
    # size; blurred, so drawn four times larger it still looks smooth.
    width, height = 548, 284
    k = max(width / box.width, height / box.height)
    image = box.resize((round(box.width * k), round(box.height * k)), Image.LANCZOS)
    x, y = (image.width - width) // 2, (image.height - height) // 2
    image = image.crop((x, y, x + width, y + height))
    flat = Image.new("RGBA", image.size, (8, 16, 30, 255))
    flat.alpha_composite(image)
    flat = flat.filter(ImageFilter.GaussianBlur(14))
    flat = ImageEnhance.Brightness(flat).enhance(0.55)
    save(flat, path)

for kind in ("boxart", "marquee", "logo"):
    folder = src / kind
    if not folder.is_dir():
        continue
    target = out / kind
    for path in sorted(folder.iterdir()):
        if path.suffix.lower() not in (".png", ".jpg", ".jpeg"):
            continue
        image = Image.open(path)
        if kind == "boxart":
            image = trimmed(image)
            height = 1040
            width = round(image.width * height / image.height)
        elif kind == "marquee":
            image = image.convert("RGBA")
            width = 848
            height = round(image.height * width / image.width)
        else:
            image = trimmed(image)
            scale = min(1000 / image.width, 260 / image.height, 2.5)
            width, height = round(image.width * scale), round(image.height * scale)
        image = image.resize((width, height), Image.LANCZOS)
        save(image, target / (path.stem.lower() + ".png"))
        counts[kind] = counts.get(kind, 0) + 1
        if kind == "boxart":
            backdrop(image, out / "backdrop" / (path.stem.lower() + ".png"))
if header.is_file():
    image = trimmed(Image.open(header))
    image = image.resize((round(image.width * 116 / image.height), 116), Image.LANCZOS)
    save(image, out / "header" / "logo.png")
    counts["header"] = 1
print("menu art:", ", ".join(f"{n} {k}" for k, n in counts.items()), "->", out)
PY
