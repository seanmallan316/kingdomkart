#!/usr/bin/env python3
"""Makes the Wii U menu icon (128x128) and the launch splash pictures from Sean's own title art and kart art."""
from PIL import Image, ImageFilter, ImageEnhance, ImageDraw
import sys, os
C = sys.argv[1] if len(sys.argv) > 1 else 'content'
img = lambda n: Image.open(os.path.join(C, 'img', n)).convert('RGBA')
bg, logo, kart = img('title_bg.png'), img('title_art.png'), img('ride_jesus_fl.png')

S = 512   # build big, then shrink for a clean 128
# the heavenly sky behind: a square from the right of the title background (the golden city and clouds)
# a clean heavenly sky (the title background already has the lettering painted in): deep blue to bright at the
# horizon, golden rays from above, soft clouds along the bottom
import math, random
def sky(W, H):
    im = Image.new('RGBA', (W, H)); px = im.load()
    for y in range(H):
        t = y / H
        for x in range(W): px[x, y] = (int(40 + 170 * t), int(110 + 125 * t), int(215 + 35 * t), 255)
    rays = Image.new('RGBA', (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(rays); L = 2 * max(W, H)
    for k in range(14):
        a0 = math.pi * (0.1 + 0.8 * k / 13) - 0.04; a1 = a0 + 0.08
        d.polygon([(W / 2, -40), (W / 2 + math.cos(a0) * L, -40 + math.sin(a0) * L), (W / 2 + math.cos(a1) * L, -40 + math.sin(a1) * L)], fill=(255, 245, 200, 60))
    im = Image.alpha_composite(im, rays.filter(ImageFilter.GaussianBlur(6)))
    cl = Image.new('RGBA', (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(cl); random.seed(3); u = min(W, H) / 512
    for k in range(int(40 * W / H)):
        cx = random.uniform(-40, W + 40); cy = random.uniform(H * 0.62, H * 1.05); r = random.uniform(40, 95) * u
        d.ellipse((cx - r, cy - r * 0.7, cx + r, cy + r * 0.7), fill=(255, 255, 255, 235))
    return Image.alpha_composite(im, cl.filter(ImageFilter.GaussianBlur(5)))
icon = sky(S, S)
# a glow of light behind the kart
glow = Image.new('RGBA', (S, S), (0, 0, 0, 0)); d = ImageDraw.Draw(glow)
for r in range(260, 0, -8): a = int(200 * (1 - r / 260) ** 1.5); d.ellipse((S / 2 - r, S * 0.68 - r * 0.7, S / 2 + r, S * 0.68 + r * 0.7), fill=(255, 240, 190, a))
icon = Image.alpha_composite(icon, glow)
# Jesus in his kart, big, coming toward us
kb = kart.getbbox(); kart = kart.crop(kb); kw = int(S * 0.86); kh = int(kart.height * kw / kart.width)
if kh > S * 0.66: kh = int(S * 0.66); kw = int(kart.width * kh / kart.height)
kart = kart.resize((kw, kh), Image.LANCZOS)
sh = Image.new('RGBA', (S, S), (0, 0, 0, 0)); ImageDraw.Draw(sh).ellipse((S / 2 - kw * 0.42, S - 46, S / 2 + kw * 0.42, S - 6), fill=(0, 0, 0, 120))
icon = Image.alpha_composite(icon, sh.filter(ImageFilter.GaussianBlur(10)))
icon.alpha_composite(kart, (int(S / 2 - kw / 2), S - kh - 14))
# the title lettering across the top (crown, KINGDOM KART), with a soft dark edge so it reads small
lb = logo.getbbox(); logo = logo.crop(lb); lw = int(S * 0.98); lh = int(logo.height * lw / logo.width); logo = logo.resize((lw, lh), Image.LANCZOS)
logo_top = logo.crop((0, 0, lw, int(lh * 0.74)))   # leave off the ROAD TO GLORY ribbon, too small to read at 128
shadow = Image.new('RGBA', logo_top.size, (0, 0, 0, 0)); shadow.putalpha(logo_top.getchannel('A').point(lambda v: min(255, v * 2)))
shadow = shadow.filter(ImageFilter.GaussianBlur(6))
icon.alpha_composite(shadow, (int(S / 2 - lw / 2) + 3, 6)); icon.alpha_composite(logo_top, (int(S / 2 - lw / 2), 2))
# a thin gold frame
fr = ImageDraw.Draw(icon); fr.rectangle((0, 0, S - 1, S - 1), outline=(255, 214, 90, 255), width=10); fr.rectangle((10, 10, S - 11, S - 11), outline=(160, 110, 30, 255), width=4)
icon.convert('RGB').resize((128, 128), Image.LANCZOS).filter(ImageFilter.UnsharpMask(1.2, 80, 2)).save(os.path.join(C, 'icon.png'))
# the splash shown while the game starts: the full title picture
for name, size in (('splash_tv.png', (1280, 720)), ('splash_drc.png', (854, 480))):
    s = sky(*size); L = logo.copy(); lw2 = int(size[0] * 0.62); L = L.resize((lw2, int(L.height * lw2 / L.width)), Image.LANCZOS)
    s.alpha_composite(L, ((size[0] - lw2) // 2, int(size[1] * 0.12))); s.convert('RGB').save(os.path.join(C, '..', 'assets', name))
print('icon and splash made')
