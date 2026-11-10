#!/usr/bin/env python3
"""Smooth shading for the racers' 3D models (Sean: the faces should look better): every point gets the average direction of
the faces around it, so skin and cloth look round instead of cut into flat facets. Sharp folds (over 50 degrees) stay sharp.
Rewrites content/models/<name>.kkm in place.   python3 tools/model3d/kkm_smooth.py name [name ...]"""
import struct, sys, numpy as np
from collections import defaultdict
COS = np.cos(np.radians(50))
def smooth(v):   # v: (n, 12) floats of one part, a triangle list
    n = len(v); P = v[:, :3].reshape(-1, 3, 3); fn = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]); fl = np.linalg.norm(fn, axis=1, keepdims=True); fu = fn / np.maximum(fl, 1e-20)
    keys = np.round(v[:, :3] * 2000).astype(np.int64); groups = defaultdict(list)
    for i, k in enumerate(map(tuple, keys)): groups[k].append(i)
    out = v.copy()
    for idx in groups.values():
        idx = np.array(idx); f = idx // 3; U = fu[f]; W = fn[f]
        sim = U @ U.T >= COS; N = sim.astype(np.float64) @ W; N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-20)
        old = v[idx, 3:6]; flip = (N * old).sum(1) < 0; N[flip] = -N[flip]   # keep the side the model already faces
        out[idx, 3:6] = N
    return out
for name in sys.argv[1:]:
    path = 'content/models/%s.kkm' % name; d = open(path, 'rb').read(); np_ = struct.unpack('>I', d[4:8])[0]; o = 8; parts = []
    for _ in range(np_):
        head = d[o:o + 24]; cnt = struct.unpack('>I', d[o + 20:o + 24])[0]; o += 24
        v = np.frombuffer(d[o:o + 48 * cnt], dtype='>f4').reshape(-1, 12).astype(np.float64) if cnt else np.zeros((0, 12)); o += 48 * cnt
        parts.append((head, smooth(v) if cnt else v))
    with open(path, 'wb') as f:
        f.write(d[:8])
        for head, v in parts: f.write(head); f.write(v.astype('>f4').tobytes())
    print('smoothed', name, flush=True)
