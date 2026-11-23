"""Tidy a FLUX Kontext paint-over: drop the ground shadow or puddle the painter added under the
object, and calm the colours so it sits with the other paintings.
Usage: python3 -I clean_paint.py IN.png OUT.png [shadow|blue|none] [saturation]"""
import sys
import numpy as np
from PIL import Image
src, out = sys.argv[1], sys.argv[2]
mode = sys.argv[3] if len(sys.argv) > 3 else 'none'
sat = float(sys.argv[4]) if len(sys.argv) > 4 else 0.78
a = np.asarray(Image.open(src).convert('RGB')).astype(float)
r, g, b = a[..., 0], a[..., 1], a[..., 2]
mx, mn = a.max(axis=2), a.min(axis=2)
h = a.shape[0]
ink = mn < 235
ys = np.nonzero(ink.any(axis=1))[0]; top, bot = ys.min(), ys.max()
low = np.arange(h)[:, None] > bot - (bot - top) * 0.3          # only the bottom part of the object
if mode == 'shadow':   # pale grey / tan smudge under the object
    kill = low & (mn > 110) & ((mx - mn) < 95) & (g < r + 12)
    a[kill] = 255
elif mode == 'blue':   # bright blue puddle
    kill = low & (b > 150) & (r < 110) & (b - r > 70)
    a[kill] = 255
a[a.min(axis=2) > 212] = 255      # faint leftovers
lum = a @ np.array([.299, .587, .114])
keep = (a.min(axis=2) < 250)[..., None]
a = np.where(keep, lum[..., None] + (a - lum[..., None]) * sat, a)
Image.fromarray(np.clip(a, 0, 255).astype('uint8')).save(out)
