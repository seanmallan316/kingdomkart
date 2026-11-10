#!/usr/bin/env python3
"""content/img/water_tile.png: a seamless water picture (ripples, light caustics, little white glints) that the game scrolls
in two directions at once so seas, rivers and the Red Sea walls look like moving water.   python3 tools/make_water.py"""
import numpy as np
from PIL import Image
N = 256; rng = np.random.default_rng(3); y, x = np.mgrid[0:N, 0:N] / N
def periodic_noise(scale, seed):   # smooth noise that tiles: random phases at whole-number frequencies
    r = np.random.default_rng(seed); out = np.zeros((N, N))
    for _ in range(40):
        fx, fy = r.integers(-scale, scale + 1, 2)
        if fx == 0 and fy == 0: continue
        out += np.sin(2 * np.pi * (fx * x + fy * y) + r.uniform(0, 6.283)) / np.hypot(fx, fy)
    return (out - out.min()) / (out.max() - out.min())
h = periodic_noise(6, 1) * 0.55 + periodic_noise(12, 2) * 0.3 + periodic_noise(24, 3) * 0.15     # the swell
c = np.abs(np.sin(periodic_noise(9, 4) * 9.0)) ; caus = np.clip(1 - c * 3.2, 0, 1) ** 2            # bright caustic lines where light gathers
deep = np.array([0.04, 0.22, 0.42]); light = np.array([0.18, 0.58, 0.78]); foam = np.array([0.92, 0.98, 1.0])
col = deep * (1 - h[..., None]) + light * h[..., None]
col = col + caus[..., None] * 0.35 * np.array([0.7, 0.95, 1.0])
glint = (periodic_noise(30, 5) > 0.86) * (h > 0.6)
col = np.where(glint[..., None], col * 0.4 + foam * 0.6, col)
Image.fromarray((np.clip(col, 0, 1) * 255).astype(np.uint8)).save('content/img/water_tile.png')
print('made water_tile.png')
