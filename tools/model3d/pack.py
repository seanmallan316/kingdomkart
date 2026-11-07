#!/usr/bin/env python3
"""Packs a painted kart model for the game: finds the 4 wheels and cuts them off as their own spinning parts,
sizes the kart to race size (2.8 m long, front toward +z, standing on y = 0) and writes content/models/<name>.kkm
plus its picture <name>.png.

.kkm layout (little-endian): 'KKM1', uint32 part count, then per part: uint32 kind (0 body, 1 wheel front-left,
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
for kind, (sx, sz) in enumerate(((1, 1), (-1, 1), (1, -1), (-1, -1)), start=1):
    sel = (np.sign(P[:, 0]) == sx) & (np.sign(P[:, 2]) == sz) & (np.abs(P[:, 0]) > X * 0.72) & (P[:, 1] < H * 0.45)
    if sel.sum() < 30: continue
    Q = P[sel]; cy = (Q[:, 1].min() + Q[:, 1].max()) / 2; R = (Q[:, 1].max() - Q[:, 1].min()) / 2; cz = (Q[:, 2].min() + Q[:, 2].max()) / 2
    zr = (Q[:, 2].max() - Q[:, 2].min()) / 2
    if zr > R * 1.4: cz = Q[:, 2].max() - R if sz > 0 else Q[:, 2].min() + R   # the outline ran along the body: take the wheel's end
    outer = np.abs(Q[:, 0]).max(); inner = outer - R * 0.75
    dyz = np.hypot(fc[:, 1] - cy, fc[:, 2] - cz); w = (~used) & (dyz < R * 1.03) & (np.sign(fc[:, 0]) == sx) & (np.abs(fc[:, 0]) > inner)
    used |= w; parts.append((kind, np.array([sx * (outer + inner) / 2, cy, cz]), R, F[w]))
    print('wheel', kind, 'center (%.2f %.2f %.2f) radius %.2f, %d triangles' % (sx * (outer + inner) / 2, cy, cz, R, w.sum()))
parts.insert(0, (0, np.zeros(3), 0.0, F[~used]))
with open(out_base + '.kkm', 'wb') as f:
    f.write(b'KKM1'); f.write(struct.pack('<I', len(parts)))
    for kind, piv, R, FF in parts:
        idx = FF.reshape(-1); V = np.zeros((len(idx), 12), np.float32)
        V[:, 0:3] = P[idx] - piv; V[:, 3:6] = N[idx]; V[:, 6:8] = UV[idx]; V[:, 8:12] = 1
        f.write(struct.pack('<I4fI', kind, *piv, R, len(V))); f.write(V.tobytes())
Image.open(tex).convert('RGB').save(out_base + '.png')
print('kart %.2f long, %.2f wide, %.2f tall; %d triangles' % (np.ptp(P[:, 2]), np.ptp(P[:, 0]), H, len(F)))
