#!/usr/bin/env python3
"""make_ground.py - the road, roadside and ground pictures for the roads that had only placeholder patterns.
Each picture is built as a height map first (stones, cracks, pebbles, grass blades), then painted and lit from the
upper left with shadows in the gaps, so it already looks raised. In the game the same brightness is read again as bumps
that catch the real sun (gfx.c, the bump flag), so the stones keep their depth as you drive over them.
Every picture tiles without a seam (all noise and stone layouts wrap round). 512 x 512.
usage: make_ground.py out_dir [name ...]"""
import numpy as np, sys, os
from PIL import Image
from scipy import ndimage
from scipy.spatial import cKDTree

N = 512
rng = np.random.default_rng(7)

def pnoise(cells, seed):
    """smooth noise that wraps round: random values on a cells x cells grid, smoothly enlarged"""
    r = np.random.default_rng(seed).random((cells, cells))
    return ndimage.zoom(r, N / cells, order=3, mode='grid-wrap')[:N, :N]
def fbm(base, octaves, seed, gain=0.5):
    t = np.zeros((N, N)); a = 1.0; s = 0.0
    for o in range(octaves):
        t += a * pnoise(base * 2 ** o, seed + o); s += a; a *= gain
    return t / s
def blur(a, s): return ndimage.gaussian_filter(a, s, mode='wrap')
def voronoi(ncell, seed, jitter=0.85, stretch=(1.0, 1.0)):
    """stones: nearest and second-nearest centre for every pixel (on a wrapping sheet); returns cell id, F1, F2-F1"""
    r = np.random.default_rng(seed); g = np.stack(np.meshgrid(np.arange(ncell), np.arange(ncell), indexing='xy'), -1).reshape(-1, 2).astype(float)
    pts = (g + 0.5 + (r.random(g.shape) - 0.5) * jitter) / ncell
    tiles = np.concatenate([pts + [dx, dy] for dx in (-1, 0, 1) for dy in (-1, 0, 1)]); ids = np.tile(np.arange(len(pts)), 9)
    yy, xx = np.mgrid[0:N, 0:N] / N; q = np.stack([xx.ravel() * stretch[0], yy.ravel() * stretch[1]], -1)
    d, i = cKDTree(tiles * np.array(stretch)).query(q, k=2)
    return ids[i[:, 0]].reshape(N, N), d[:, 0].reshape(N, N) * ncell, (d[:, 1] - d[:, 0]).reshape(N, N) * ncell
def lightit(alb, h, depth, amb=0.42, ao=1.0):
    """light the painted surface from the upper left using its height, with darker gaps (ambient occlusion)"""
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5 * depth; gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5 * depth
    n = np.stack([-gx, -gy, np.ones_like(h)], -1); n /= np.linalg.norm(n, axis=-1, keepdims=True)
    L = np.array([-0.55, -0.6, 0.58]); L /= np.linalg.norm(L)
    dif = np.clip((n * L).sum(-1), 0, 1)
    cav = np.clip(1.0 - (blur(h, 6) - h) * 3.0 * ao, 0.45, 1.15)
    shade = (amb + (1 - amb) * dif / L[2]) * cav
    return np.clip(alb * shade[..., None], 0, 1)
def save(img, out):
    a = (np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8); rgba = np.concatenate([a, np.full((N, N, 1), 255, np.uint8)], -1)
    Image.fromarray(rgba, 'RGBA').save(out, optimize=True); print('wrote', out)
def tint(ids, seed, spread):   # a colour shift per stone
    r = np.random.default_rng(seed); t = r.normal(0, spread, (ids.max() + 1, 3)); b = r.normal(0, spread * 1.6, ids.max() + 1)
    return t[ids] + b[ids][..., None]

def cobbles(pal, grout, ncell, seed, moss=None, round_=1.0, gap=0.16):
    ids, f1, e = voronoi(ncell, seed)
    body = np.clip(e / gap, 0, 1); dome = 1 - np.clip(f1 * 1.15, 0, 1) ** 2 * 0.6 * round_
    rough = fbm(16, 4, seed + 1); fine = fbm(64, 3, seed + 2)
    h = body ** 0.6 * (0.55 + 0.45 * dome) + 0.06 * rough * body + 0.035 * fine
    crack = (np.abs(fbm(20, 3, seed + 3) - 0.5) < 0.006) & (body > 0.9) & (fbm(6, 2, seed + 7) > 0.62)   # a few hairline cracks
    h = h - crack * 0.06
    alb = np.array(pal)[None, None, :] + tint(ids, seed + 4, 0.012) + (rough[..., None] - 0.5) * 0.1 + (fine[..., None] - 0.5) * 0.07
    alb = alb * (0.85 + 0.25 * (h[..., None] - 0.5))                     # worn tops a little paler
    g = np.array(grout)[None, None, :] + (fine[..., None] - 0.5) * 0.1
    if moss is not None: m = (fbm(12, 3, seed + 5) > 0.52)[..., None]; g = np.where(m, np.array(moss)[None, None, :] + (fine[..., None] - 0.5) * 0.15, g)
    w = np.clip(body * 1.6, 0, 1)[..., None]; alb = alb * w + g * (1 - w)
    alb = np.where(crack[..., None], alb * 0.8, alb)
    return lightit(alb, h, 11.0)

def pavers(pal, grout, rows, seed):
    """cut limestone blocks in staggered rows of different lengths"""
    r = np.random.default_rng(seed); yy, xx = np.mgrid[0:N, 0:N] / N; rh = 1.0 / rows
    row = np.floor(yy / rh).astype(int); fy = (yy - row * rh) / rh
    ids = np.zeros((N, N), int); fx = np.zeros((N, N)); w_ = np.zeros((N, N))
    for k in range(rows):
        cuts = np.sort(r.random(r.integers(3, 6))); cuts = np.concatenate([cuts, cuts[:1] + 1])   # wraps round
        m = row == k; x = xx[m]
        for j in range(len(cuts) - 1):
            a, b = cuts[j], cuts[j + 1]; sel = ((x - a) % 1.0) < (b - a)
            idx = np.where(m.ravel())[0][sel]; ids.ravel()[idx] = k * 10 + j; fx.ravel()[idx] = ((x[sel] - a) % 1.0) / (b - a); w_.ravel()[idx] = (b - a)
    ex = np.minimum(fx, 1 - fx) * w_ / rh; ey = np.minimum(fy, 1 - fy); e = np.minimum(ex, ey)
    body = np.clip(e / 0.1, 0, 1); rough = fbm(16, 4, seed + 1); fine = fbm(64, 3, seed + 2)
    h = body ** 0.5 * (0.8 + 0.2 * rough) + 0.03 * fine
    chips = (fbm(40, 2, seed + 6) > 0.74) & (body > 0.5); h = h - chips * 0.05
    alb = np.array(pal)[None, None, :] + tint(ids, seed + 4, 0.015) + (rough[..., None] - 0.5) * 0.08 + (fine[..., None] - 0.5) * 0.06
    alb = np.where(chips[..., None], alb * 0.9, alb)
    g = np.array(grout)[None, None, :] + (fine[..., None] - 0.5) * 0.08; w = np.clip(body * 1.8, 0, 1)[..., None]
    return lightit(alb * w + g * (1 - w), h, 8.0)

def gold_tiles(seed):
    yy, xx = np.mgrid[0:N, 0:N] / N; n = 4; fx = (xx * n) % 1; fy = (yy * n) % 1; e = np.minimum(np.minimum(fx, 1 - fx), np.minimum(fy, 1 - fy))
    bevel = np.clip(e / 0.06, 0, 1); fine = fbm(64, 3, seed); swirl = fbm(8, 3, seed + 1)
    h = bevel ** 0.7 + 0.02 * fine
    gem = (np.hypot(np.minimum(fx, 1 - fx), np.minimum(fy, 1 - fy)) < 0.05)
    alb = np.array([0.95, 0.74, 0.3])[None, None, :] + (swirl[..., None] - 0.5) * np.array([0.12, 0.12, 0.06]) + (fine[..., None] - 0.5) * 0.06
    alb = np.where(bevel[..., None] < 0.99, alb * (0.75 + 0.3 * bevel[..., None]), alb)
    alb = np.where(gem[..., None], np.array([0.75, 0.95, 1.0]), alb); h = np.where(gem, 1.05, h)
    return lightit(alb, h, 10.0, amb=0.55)

def earth_path(base, dark, seed, moss=None, pebbles=0.5):
    soil = fbm(8, 5, seed); fine = fbm(64, 3, seed + 1)
    h = 0.35 * soil + 0.15 * fine
    ids, f1, e = voronoi(22, seed + 2, jitter=0.9)
    keep = np.random.default_rng(seed + 3).random(ids.max() + 1) < pebbles; peb = keep[ids] & (f1 < 0.3)
    ph = np.clip(1 - f1 / 0.3, 0, 1) ** 1.2; h = np.where(peb, h + 0.3 * ph, h)
    ruts = 0.5 + 0.5 * np.cos(np.linspace(0, 2 * np.pi * 2, N))[None, :]; h = h - 0.05 * ruts * (1 - peb)   # two faint wheel ruts along the road
    alb = np.array(base)[None, None, :] * (0.8 + 0.4 * soil[..., None]) + (fine[..., None] - 0.5) * 0.08
    alb = alb * (1 - 0.25 * (1 - h[..., None])) + np.array(dark)[None, None, :] * 0.0
    pc = np.array([0.55, 0.5, 0.42])[None, None, :] + tint(ids, seed + 4, 0.012) + (fine[..., None] - 0.5) * 0.1
    pw = np.clip(ph * 3, 0, 1)[..., None]; alb = np.where(peb[..., None], alb * (1 - pw) + pc * (0.8 + 0.25 * ph[..., None]) * pw, alb)
    if moss is not None:
        m = np.clip((fbm(6, 4, seed + 5) - 0.58) * 4, 0, 1) * 0.85 * (1 - peb); alb = alb * (1 - m[..., None]) + (np.array(moss)[None, None, :] * (0.8 + 0.4 * fine[..., None])) * m[..., None]; h = h + 0.08 * m
    return lightit(alb, h, 7.0)

def grass(base, seed, flowers=0.0):
    """lush grass: clumps, blades and the odd flower (a picture of grass seen from above, with depth in the clumps)"""
    clump = fbm(10, 4, seed); fine = fbm(96, 2, seed + 1); blades = fbm(160, 2, seed + 2)
    h = 0.45 * clump + 0.35 * blades + 0.2 * fine
    alb = np.array(base)[None, None, :] * (0.7 + 0.55 * h[..., None]) + np.array([0.05, 0.08, -0.02])[None, None, :] * (blades[..., None] - 0.5) * 3
    dry = np.clip((fbm(6, 3, seed + 3) - 0.6) * 4, 0, 1)[..., None]; alb = alb * (1 - 0.35 * dry) + np.array([0.55, 0.5, 0.25]) * 0.35 * dry
    if flowers > 0:
        ids, f1, e = voronoi(48, seed + 4); keep = np.random.default_rng(seed + 5).random(ids.max() + 1) < flowers; fl = keep[ids] & (f1 < 0.12)
        cols = np.array([[1, 0.95, 0.4], [1, 1, 1], [1, 0.55, 0.7], [0.75, 0.6, 1]])[np.random.default_rng(seed + 6).integers(0, 4, ids.max() + 1)]
        alb = np.where(fl[..., None], cols[ids], alb); h = np.where(fl, h + 0.3, h)
    return lightit(alb, h, 6.0, amb=0.5)

def sand(base, seed):
    rip = np.sin((np.mgrid[0:N, 0:N][0] / N * 2 * np.pi * 9) + fbm(4, 3, seed) * 6) * 0.5 + 0.5
    fine = fbm(128, 2, seed + 1); h = 0.3 * rip + 0.3 * fine + 0.4 * fbm(8, 3, seed + 2)
    ids, f1, e = voronoi(36, seed + 3); keep = np.random.default_rng(seed + 4).random(ids.max() + 1) < 0.25; peb = keep[ids] & (f1 < 0.2)
    h = np.where(peb, h + 0.5 * np.clip(1 - f1 / 0.2, 0, 1) ** 0.5, h)
    alb = np.array(base)[None, None, :] * (0.85 + 0.25 * h[..., None]); alb = np.where(peb[..., None], np.array([0.6, 0.57, 0.52]) + tint(ids, seed + 5, 0.05), alb)
    shells = keep[ids] & (f1 < 0.07) & (np.random.default_rng(seed + 6).random(ids.max() + 1) < 0.3)[ids]; alb = np.where(shells[..., None], np.array([0.98, 0.95, 0.9]), alb)
    return lightit(alb, h, 6.0, amb=0.5)

def cracked_earth(base, seed):
    ids, f1, e = voronoi(9, seed, jitter=0.95); fine = fbm(64, 3, seed + 1)
    body = np.clip(e / 0.05, 0, 1); h = body ** 0.4 * (0.75 + 0.25 * fbm(12, 3, seed + 2)) + 0.05 * fine
    alb = np.array(base)[None, None, :] * (0.85 + 0.2 * fine[..., None]) + tint(ids, seed + 3, 0.025)
    alb = alb * (0.35 + 0.65 * body[..., None])
    return lightit(alb, h, 8.0)

SETS = {
    'gal_road':     lambda: cobbles([0.62, 0.63, 0.62], [0.72, 0.64, 0.48], 10, 11, round_=1.3, gap=0.12),      # round lake stones in sand
    'gal_sand':     lambda: sand([0.93, 0.84, 0.64], 12),
    'jeru_road':    lambda: pavers([0.86, 0.78, 0.62], [0.42, 0.36, 0.28], 6, 21),                              # Jerusalem limestone
    'tomb_road':    lambda: cobbles([0.6, 0.56, 0.52], [0.3, 0.42, 0.2], 6, 31, moss=[0.32, 0.5, 0.2], round_=0.5, gap=0.07),   # garden flagstones, grass between
    'jericho_road': lambda: cobbles([0.78, 0.6, 0.42], [0.5, 0.38, 0.26], 9, 41, round_=0.8),                     # sun-baked stones
    'jericho_dirt': lambda: cracked_earth([0.8, 0.62, 0.42], 42),
    'jericho_grass':lambda: grass([0.55, 0.62, 0.3], 43),
    'eden_road':    lambda: earth_path([0.62, 0.5, 0.34], [0.3, 0.22, 0.15], 51, moss=[0.35, 0.55, 0.2], pebbles=0.3),
    'eden_dirt':    lambda: earth_path([0.55, 0.45, 0.3], [0.3, 0.22, 0.15], 52, moss=[0.35, 0.6, 0.22], pebbles=0.18),
    'eden_grass':   lambda: grass([0.36, 0.7, 0.28], 53, flowers=0.18),
    'heaven_road':  lambda: gold_tiles(61),
}

if __name__ == '__main__':
    out = sys.argv[1]; names = sys.argv[2:] or list(SETS)
    os.makedirs(out, exist_ok=True)
    for n in names: save(SETS[n](), os.path.join(out, n + '.png'))
