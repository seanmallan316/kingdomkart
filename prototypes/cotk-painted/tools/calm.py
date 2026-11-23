"""Calm a ground tile: pull it toward a target colour, less contrast and saturation.
Usage: python3 -I calm.py IN.png OUT.png R G B contrast saturation"""
import sys
import numpy as np
from PIL import Image
src, out = sys.argv[1], sys.argv[2]
tgt = np.array([float(v) for v in sys.argv[3:6]]); k = float(sys.argv[6]); sat = float(sys.argv[7])
a = np.asarray(Image.open(src).convert('RGB')).astype(float)
mean = a.reshape(-1, 3).mean(0)
a = tgt + (a - mean) * k                         # same detail, new average colour, softer contrast
lum = a @ np.array([.299, .587, .114])
a = lum[..., None] + (a - lum[..., None]) * sat  # less saturation
Image.fromarray(np.clip(a, 0, 255).astype('uint8')).save(out)
