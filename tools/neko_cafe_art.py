#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the neko-cafe pictures in src/ui/images/ from the Game Boy game's generated tile files:
#   python3 tools/neko_cafe_art.py <neko-cafe game folder>/src
# (tiles.c, sprites.c, map.c from the original neko-cafe game; the art is original.)
import re, sys, os
from PIL import Image

src = sys.argv[1]
out = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src", "ui", "images")
PAL = [(0x9b, 0xbc, 0x0f), (0x8b, 0xac, 0x0f), (0x30, 0x62, 0x30), (0x0f, 0x38, 0x0f)]
SPR = {1: 0, 2: 2, 3: 3}   # sprite colours 1, 2, 3 show as the lightest, dark, darkest shade (0 is clear)

def read(name, array):
    s = open(os.path.join(src, name)).read()
    body = re.search(array + r"\[\] = \{(.*?)\};", s, re.S).group(1)
    body = re.sub(r"//[^\n]*", "", body)
    return [int(x, 0) for x in re.findall(r"0x[0-9a-fA-F]+|\b\d+\b", body)]

def tile(data, n):
    t = data[n * 16:(n + 1) * 16]
    return [[((t[r * 2 + 1] >> (7 - c)) & 1) << 1 | ((t[r * 2] >> (7 - c)) & 1) for c in range(8)] for r in range(8)]

bg = read("tiles.c", "cafe_tiles")
cmap = read("map.c", "cafe_map")
spr = read("sprites.c", "sprite_tiles")
bub = read("tiles.c", "bubble_map")

W, H = 20, 18
img = Image.new("RGB", (W * 8, H * 8))
for i, n in enumerate(cmap[:W * H]):
    x0, y0 = (i % W) * 8, (i // W) * 8
    for r, row in enumerate(tile(bg, n)):
        for c, v in enumerate(row):
            img.putpixel((x0 + c, y0 + r), PAL[v])
img.save(os.path.join(out, "neko-cafe-bg.png"))

b = Image.new("RGB", (40, 16))
for i, n in enumerate(bub[:10]):
    x0, y0 = (i % 5) * 8, (i // 5) * 8
    for r, row in enumerate(tile(bg, n)):
        for c, v in enumerate(row):
            b.putpixel((x0 + c, y0 + r), PAL[v])
b.save(os.path.join(out, "neko-cafe-bubble.png"))

count = len(spr) // 16
cols = count // 2
sheet = Image.new("RGBA", (cols * 8, 16), (0, 0, 0, 0))
for k in range(cols):
    for half in range(2):
        for r, row in enumerate(tile(spr, 2 * k + half)):
            for c, v in enumerate(row):
                if v:
                    sheet.putpixel((k * 8 + c, half * 8 + r), PAL[SPR[v]] + (255,))
sheet.save(os.path.join(out, "neko-cafe-sprites.png"))

# the app's tile icon: the maid, facing the front, twice as big
icon = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
for half in range(2):
    icon.alpha_composite(sheet.crop((half * 2 * 8 // 1 * 0 + half * 8, 0, half * 8 + 8, 16)), (half * 8, 0))
icon.resize((128, 128), Image.NEAREST).save(os.path.join(out, "app-neko-cafe.png"))
print("sprites:", cols, "columns")
