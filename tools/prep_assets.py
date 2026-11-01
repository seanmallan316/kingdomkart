#!/usr/bin/env python3
"""Prepares Sean's generated pictures for the game.

Reads every PNG/JPG under assets/, removes a flat magenta/green/black background when the picture is a cut-out,
shrinks to the size the Wii U needs, and writes a small RGBA PNG to content/img/<name>.png.
Run: python3 tools/prep_assets.py      (needs Pillow: pip install pillow)
"""
import os, sys
from PIL import Image

SRC, DST = "assets", "content/img"
# how big each kind of picture is allowed to be in the game (width, height); 0 = keep aspect, fit inside
RULES = [
    ("portrait_", (384, 384), "cutout"),
    ("icon_",     (128, 128), "cutout"),
    ("beth_star", (256, 256), "black"),
    ("beth_sky",  (1024, 256), "plain"),
    ("beth_road", (512, 512), "plain"),
    ("beth_dirt", (512, 512), "plain"),
    ("beth_grass",(512, 512), "plain"),
    ("beth_wall", (512, 512), "plain"),
    ("title_art", (1024, 576), "plain"),
    ("heaven_gates", (1024, 576), "plain"),
    ("heaven_sky", (1024, 256), "plain"),
    ("",          (512, 512), "cutout"),     # everything else is scenery
]

def key_background(im, mode):
    """Make the background transparent: the flat color found in the picture's corners."""
    im = im.convert("RGBA")
    px = im.load(); w, h = im.size
    corners = [px[0, 0], px[w - 1, 0], px[0, h - 1], px[w - 1, h - 1]]
    bg = max(set(c[:3] for c in corners), key=[c[:3] for c in corners].count)
    tol = 70 if mode == "cutout" else 40
    green_screen = bg[1] > bg[0] + 80 and bg[1] > bg[2] + 80
    out = Image.new("RGBA", im.size)
    op = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            d = abs(r - bg[0]) + abs(g - bg[1]) + abs(b - bg[2])
            if green_screen and mode == "cutout":
                # green screen: judge by how green the pixel is, and take the green fringe off the edges
                k = g - max(r, b)
                if k > 60: op[x, y] = (0, 0, 0, 0)
                elif k > 10: op[x, y] = (r, min(g, max(r, b) + 5), b, int(255 * (60 - k) / 50))
                else: op[x, y] = (r, g, b, 255)
            elif mode == "black":
                # the star: brightness becomes alpha, so the glow fades out smoothly
                lum = max(r, g, b)
                op[x, y] = (r, g, b, lum)
            elif d < tol:
                op[x, y] = (r, g, b, 0)
            elif d < tol * 2:
                f = (d - tol) / tol
                op[x, y] = (r, g, b, int(255 * f))
            else:
                op[x, y] = (r, g, b, 255)
    return out

def fit(im, box):
    w, h = im.size; bw, bh = box
    s = min(bw / w, bh / h, 1.0)
    return im.resize((max(1, int(w * s)), max(1, int(h * s))), Image.LANCZOS)

def main():
    os.makedirs(DST, exist_ok=True)
    n = 0
    for root, dirs, files in os.walk(SRC):
        dirs[:] = [d for d in dirs if d != 'raw']          # assets/raw holds originals that are not game pictures
        for f in files:
            if not f.lower().endswith((".png", ".jpg", ".jpeg", ".webp")): continue
            name = os.path.splitext(f)[0].lower()
            rule = next(r for r in RULES if name.startswith(r[0]))
            im = Image.open(os.path.join(root, f))
            if rule[2] in ("cutout", "black"): im = key_background(im, rule[2])
            else: im = im.convert("RGBA")
            im = fit(im, rule[1])
            if rule[2] == "cutout":
                im = im.crop(im.getbbox() or (0, 0, 1, 1))      # trim empty borders
                im = fit(im, rule[1])
            im.save(os.path.join(DST, name + ".png"), optimize=True)
            print(f"{f} -> {DST}/{name}.png {im.size}")
            n += 1
    print(f"{n} pictures prepared")

if __name__ == "__main__":
    main()
