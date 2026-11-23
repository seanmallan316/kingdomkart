"""Make an evenly spread, already-repeating ground pattern for the AI to repaint.
Usage: python3 -I pattern.py grass|dirt OUT.png"""
import sys, random
from PIL import Image, ImageDraw, ImageFilter
kind, out = sys.argv[1], sys.argv[2]
N = 1024; rnd = random.Random(5)
base = {'grass': (132, 170, 84), 'dirt': (214, 180, 128)}[kind]
im = Image.new('RGB', (N, N), base); d = ImageDraw.Draw(im)
def wrap(fn, x, y, *a):  # draw at x,y and at the wrapped copies so the picture repeats
    for dx in (-N, 0, N):
        for dy in (-N, 0, N): fn(x + dx, y + dy, *a)
tones = {'grass': [(112, 152, 70), (150, 186, 96), (100, 140, 64)], 'dirt': [(196, 160, 110), (226, 194, 144), (186, 150, 104)]}[kind]
for _ in range(70):   # soft patches in other tones
    x, y, r = rnd.uniform(0, N), rnd.uniform(0, N), rnd.uniform(40, 120); c = rnd.choice(tones)
    wrap(lambda X, Y, R, C: d.ellipse((X - R, Y - R * .7, X + R, Y + R * .7), fill=C), x, y, r, c)
im = im.filter(ImageFilter.GaussianBlur(10)); d = ImageDraw.Draw(im)
if kind == 'grass':
    for _ in range(900):   # tufts: little ink V-ticks
        x, y = rnd.uniform(0, N), rnd.uniform(0, N)
        wrap(lambda X, Y: (d.line((X - 6, Y - 10, X, Y), fill=(40, 70, 30), width=3), d.line((X + 6, Y - 11, X, Y), fill=(40, 70, 30), width=3)), x, y)
    for _ in range(220):   # heather and clover
        x, y = rnd.uniform(0, N), rnd.uniform(0, N); c = rnd.choice([(160, 100, 170), (240, 240, 230)])
        wrap(lambda X, Y, C: d.ellipse((X - 5, Y - 5, X + 5, Y + 5), fill=C, outline=(50, 40, 40)), x, y, c)
else:
    for _ in range(260):   # pebbles
        x, y, r = rnd.uniform(0, N), rnd.uniform(0, N), rnd.uniform(4, 12)
        wrap(lambda X, Y, R: d.ellipse((X - R, Y - R * .75, X + R, Y + R * .75), fill=(150, 130, 110), outline=(60, 45, 35), width=2), x, y, r)
    for _ in range(40):    # cracks
        x, y = rnd.uniform(0, N), rnd.uniform(0, N); pts = [(x, y)]
        for _ in range(4): x += rnd.uniform(-25, 25); y += rnd.uniform(-25, 25); pts.append((x, y))
        for dx in (-N, 0, N):
            for dy in (-N, 0, N): d.line([(px + dx, py + dy) for px, py in pts], fill=(120, 90, 60), width=2)
im.save(out)
