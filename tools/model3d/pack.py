#!/usr/bin/env python3
"""Packs a painted kart model for the game: finds the 4 wheels and cuts them off as their own spinning parts,
sizes the kart to race size (2.8 m long, front toward +z, standing on y = 0) and writes content/models/<name>.kkm
plus its picture <name>.png.

.kkm layout (big-endian, the Wii U's own byte order): 'KKM1', uint32 part count, then per part: uint32 kind (0 body, 1 wheel front-left,
2 front-right, 3 back-left, 4 back-right), float pivot x y z, float wheel radius, uint32 corner count, then the
corners as 12 floats each (x y z, normal x y z, u v, r g b a) - the game's own corner layout."""
import numpy as np, struct, sys
from PIL import Image
mesh, tex, out_base = sys.argv[1], sys.argv[2], sys.argv[3]
d = np.load(mesh); P, N, UV, F = d['P'].copy(), d['N'].copy(), d['UV'], d['F']
# race size, standing on the ground, front toward +z
P[:, 1] -= P[:, 1].min(); s = 2.8 / (P[:, 2].max() - P[:, 2].min()); P *= s; P[:, 0] -= (P[:, 0].max() + P[:, 0].min()) / 2; P[:, 2] -= (P[:, 2].max() + P[:, 2].min()) / 2
H = P[:, 1].max(); X = np.abs(P[:, 0]).max()
# the wheels: the outermost low parts in each corner; each is a round thing, so its center is the middle of its outline
parts = []; used = np.zeros(len(F), bool); fc = P[F].mean(1)
# find the wheels: draw each side of the kart flat, as the picture shows it, and look for the round shapes standing on the ground
import cv2
T = np.asarray(Image.open(tex).convert('RGB')); TS = T.shape[0] - 1
def side_circles(sx):
    res = 300; W = int(3.0 * res); Hh = int((H + 0.1) * res); img = np.full((Hh, W, 3), 255, np.uint8)
    for fi in np.argsort(sx * fc[:, 0]):
        if sx * fc[fi, 0] < 0: continue
        t = F[fi]; pts = np.stack([(P[t, 2] + 1.5) * res, Hh - P[t, 1] * res], 1).astype(np.int32); uv = UV[t].mean(0)
        cv2.fillPoly(img, [pts], tuple(int(x) for x in T[int(np.clip(uv[1], 0, 1) * TS), int(np.clip(uv[0], 0, 1) * TS)]))
    g = cv2.medianBlur(cv2.cvtColor(img, cv2.COLOR_RGB2GRAY), 5)
    c = cv2.HoughCircles(g, cv2.HOUGH_GRADIENT, dp=1.5, minDist=150, param1=80, param2=30, minRadius=int(0.3 * res), maxRadius=int(0.62 * res))
    out = []
    for x, y, r in (c[0] if c is not None else []):
        z, yy, R = x / res - 1.5, (Hh - y) / res, r / res
        if abs(yy - R) < 0.12: out.append((z, yy, R))   # standing on the ground
    return out
for kind, (sx, sz) in enumerate(((1, 1), (-1, 1), (1, -1), (-1, -1)), start=1):
    cs = [c for c in side_circles(sx) if np.sign(c[0]) == sz]
    if not cs: print('no wheel found for', kind); continue
    cz, cy, R = cs[0]
    sel = (np.sign(P[:, 0]) == sx) & (np.hypot(P[:, 1] - cy, P[:, 2] - cz) < R) & (np.abs(P[:, 0]) > X * 0.5)
    outer = np.abs(P[sel, 0]).max() if sel.any() else X; inner = outer - R * 0.8
    vd = np.hypot(P[:, 1] - cy, P[:, 2] - cz); fmax = vd[F].max(1)
    w = (~used) & (fmax < R * 1.03) & (np.sign(fc[:, 0]) == sx) & (np.abs(fc[:, 0]) > inner)
    used |= w; parts.append((kind, np.array([sx * (outer + inner) / 2, cy, cz]), R, F[w]))
    print('wheel', kind, 'center (%.2f %.2f %.2f) radius %.2f, %d triangles' % (sx * (outer + inner) / 2, cy, cz, R, w.sum()))
parts.insert(0, (0, np.zeros(3), 0.0, F[~used]))
with open(out_base + '.kkm', 'wb') as f:
    f.write(b'KKM1'); f.write(struct.pack('>I', len(parts)))
    for kind, piv, R, FF in parts:
        idx = FF.reshape(-1); V = np.zeros((len(idx), 12), np.float32)
        V[:, 0:3] = P[idx] - piv; V[:, 3:6] = N[idx]; V[:, 6:8] = UV[idx]; V[:, 8:12] = 1
        f.write(struct.pack('>I4fI', kind, *piv, R, len(V))); f.write(V.astype('>f4').tobytes())
Image.open(tex).convert('RGB').save(out_base + '.png')
print('kart %.2f long, %.2f wide, %.2f tall; %d triangles' % (np.ptp(P[:, 2]), np.ptp(P[:, 0]), H, len(F)))
