#!/usr/bin/env python3
"""prep_racer.py name portrait_box : Sean's three green-screen pictures of a new racer -> the game's pictures.
assets/<name>_char_src.png (standing), <name>_ride_sheet_src.png (2x2: front, front-left, side, back), <name>_front_src.png (big front)
-> content/img/char_, kart_, portrait_, ride_<name>_f/fl/l/bl/b.png, and a square front picture for Meshy (argv[3])."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from prep_assets import fit
from PIL import Image
import numpy as np
from scipy import ndimage
def key_background(im, mode='cutout'):   # green screen, but only the bright green joined to the edges: leaves and green trim stay
    a = np.array(im.convert('RGBA')).astype(np.int32); r, g, b = a[:, :, 0], a[:, :, 1], a[:, :, 2]; k = g - np.maximum(r, b)
    bg = (k > 70) & (g > 150); lab, n = ndimage.label(bg); edge = set(np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]]))) - {0}
    sz = ndimage.sum(bg, lab, range(1, n + 1)); big = set(i + 1 for i in range(n) if sz[i] > 1500); bgm = np.isin(lab, list(edge | big)); near = ndimage.binary_dilation(bgm, iterations=3) & ~bgm
    alpha = np.where(bgm, 0, 255); fr = near & (k > 15); alpha[fr] = np.clip(255 * (90 - k[fr]) / 75, 0, 255)
    g2 = np.where(near, np.minimum(g, np.maximum(r, b) + 5), g)   # take the green fringe off the edge
    out = np.stack([r, g2, b, alpha], -1).astype(np.uint8); return Image.fromarray(out, 'RGBA')
def clean(k):   # only the picture itself: drop stray bits cut from the neighbouring picture on the sheet
    a = np.array(k); m = a[:, :, 3] > 20; lab, n = ndimage.label(m)
    if n > 1:
        sz = ndimage.sum(m, lab, range(1, n + 1)); keep = np.zeros(n + 1, bool); keep[1:] = sz >= sz.max() * 0.02; a[~keep[lab]] = 0
    return Image.fromarray(a)
def out(im, name, box):
    k = clean(key_background(im, 'cutout')); k = k.crop(k.getbbox()); r = fit(k, box); r.save('content/img/' + name + '.png'); print(name, r.size)
n = sys.argv[1]; pb = [int(v) for v in sys.argv[2].split(',')]; A = 'assets/' + n
out(Image.open(A + '_char_src.png').convert('RGBA'), 'char_' + n, (340, 512))
fr = Image.open(A + '_front_src.png').convert('RGBA'); out(fr, 'kart_' + n, (390, 341))
fit(key_background(fr, 'cutout').crop(tuple(pb)), (256, 256)).save('content/img/portrait_' + n + '.png')
sh = Image.open(A + '_ride_sheet_src.png').convert('RGBA'); W, H = sh.size
q = {'f': (0, 0, W // 2, H // 2), 'fl': (W // 2, 0, W, H // 2), 'l': (0, H // 2, W // 2, H), 'bl': (W // 2, H // 2, W, H)}
for s, b in q.items(): out(sh.crop(b), 'ride_' + n + '_' + s, (512, 384))
out(sh.crop(q['bl']), 'ride_' + n + '_b', (512, 384))
k = clean(key_background(fr, 'cutout')); k = k.crop(k.getbbox()); w, h = k.size; s = int(max(w, h) * 1.12)
c = Image.new('RGBA', (s, s), (0, 0, 0, 0)); c.paste(k, ((s - w) // 2, (s - h) // 2), k); c.resize((1024, 1024), Image.LANCZOS).save(sys.argv[3])
