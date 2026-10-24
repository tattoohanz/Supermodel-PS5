#!/usr/bin/env bash
# Supermodel PS5: the title's art (the home screen icon, its background and
# the launch picture) from ps5/assets/art-src, into ps5/assets/sce_sys, which
# build-title.sh prefers to the placeholder art. Neither folder is in git
# (whose the art is, is not settled).
#
#   ps5/assets/art-src/background.png  any size, scaled to 3840x2160 (pic0 = the
#                                      home screen background, pic1 = the launch
#                                      picture; the same image for both)
#   ps5/assets/art-src/<icon>.png      the icon's artwork, any aspect; its
#                                      transparent border is trimmed and the
#                                      rest fitted, centred, into a square
#
# Usage: ps5/prepare-art.sh [--icon NAME] [--icon-fill COLOUR] [--icon-margin PX]
#   --icon NAME         the icon's artwork, art-src/NAME.png (default: logo-pill)
#   --icon-fill COLOUR  what the square shows around it (default: black)
#   --icon-margin PX    space kept free on each side, of 512 (default: 12)
#
# The BC7 encoding and the checks are PS5_Vulkan's tools/prepare-assets.sh.

set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
vk=${PS5_VULKAN:-/root/ps5/PS5_Vulkan}
src=$here/assets/art-src
out=$here/assets/sce_sys
icon=logo-pill
fill=black
margin=12

while (($#)); do
    case "$1" in
        --icon) icon=$2; shift 2 ;;
        --icon-fill) fill=$2; shift 2 ;;
        --icon-margin) margin=$2; shift 2 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

for file in "$src/background.png" "$src/$icon.png" "$vk/tools/prepare-assets.sh"; do
    [[ -f $file ]] || { echo "missing: $file" >&2; exit 2; }
done
bash "$vk/tools/setup-asset-dependencies.sh" > /dev/null

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

python3 - "$src/$icon.png" "$fill" "$margin" "$work/icon0.png" <<'PY'
import sys
from PIL import Image, ImageColor

source, fill, margin, target = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
art = Image.open(source).convert("RGBA")
box = art.getchannel("A").getbbox()
if box is None:
    raise SystemExit(f"{source} is fully transparent")
art = art.crop(box)
room = 512 - 2 * margin
scale = min(room / art.width, room / art.height)
size = (max(1, round(art.width * scale)), max(1, round(art.height * scale)))
art = art.resize(size, Image.LANCZOS)
square = Image.new("RGBA", (512, 512), ImageColor.getrgb(fill) + (255,))
square.alpha_composite(art, ((512 - size[0]) // 2, (512 - size[1]) // 2))
square.save(target)
print(f"icon: {source} {box} -> {size[0]}x{size[1]} on {fill}")
PY

mkdir -p "$out"
bash "$vk/tools/prepare-assets.sh" --icon "$work/icon0.png" \
    --background "$src/background.png" --output-directory "$out"
