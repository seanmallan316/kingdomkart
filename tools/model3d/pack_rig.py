#!/usr/bin/env python3
"""Packs a kart whose driver can move: the kart was cut by Meshy's Auto Split into the kart, the legs, the torso, the
head, both upper arms and the forearms-with-steering-wheel (see rig_prep.py). This finds the joints where the pieces
meet (neck, shoulders, elbows, hips), splits the steering wheel from the hands, and writes content/models/<name>.kkm.

Parts written (kind, pivot): 0 kart body (with the legs) / 1-4 wheels (as pack.py) / 5 torso (hips) / 6 head (neck) /
7, 8 upper arm left, right (shoulder) / 9, 10 forearm left, right (elbow) / 11 steering wheel (its hub) /
12, 13 the left, right grip on the wheel (a point, no corners) / 14 the steering column direction (a point, no corners).
Left is +x: the driver's own left, facing +z.

The torso and the upper arms are optional (Meshy does not always cut them out): a missing torso stays part of the
kart, and an arm with no upper arm turns at the joint where its forearm piece meets the body (elbow or shoulder).

usage: pack_rig.py parts.npz labels.json texture.png out_base
labels.json: {"head": [4], "torso": [3, 7], "uarmL": [0], "uarmR": [], "fore": [2]} - Meshy's part numbers; the rest is the kart."""
import numpy as np, struct, sys, os, colorsys, json
from PIL import Image
from scipy.spatial import cKDTree
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pack import find_wheels

def contact(A, B, thr):
    """middle of the points of A that touch B (where the two pieces were cut apart)"""
    d, _ = cKDTree(B).query(A)
    for t in (thr, thr * 2, thr * 4, thr * 8):
        q = d < t
        if q.sum() >= 6: return A[q].mean(0), int(q.sum())
    return None, 0

def main():
    src, labf, tex, out = sys.argv[1:5]
    d = np.load(src); spec = json.load(open(labf))
    names = sorted({k.split('__')[0] for k in d.files})
    get = lambda k: (d[k + '__P'].astype(np.float64), d[k + '__N'].astype(np.float64), d[k + '__UV'].astype(np.float64))
    allP = np.concatenate([d[k + '__P'] for k in names]).astype(np.float64)
    # race size exactly as pack.py does it, measured on the whole kart with its driver
    y0 = allP[:, 1].min(); s = float(os.environ.get('KART_LEN', '2.35')) / (allP[:, 2].max() - allP[:, 2].min())
    cx = (allP[:, 0].max() + allP[:, 0].min()) / 2 * s; cz = (allP[:, 2].max() + allP[:, 2].min()) / 2 * s
    def fix(P): P = P.copy(); P[:, 1] -= y0; P *= s; P[:, 0] -= cx; P[:, 2] -= cz; return P
    owner = {}
    for role, idx in spec.items():
        if isinstance(idx, list):
            for i in idx: owner['model_part%d' % i] = role
    piece = {}; body = [], [], []
    for k in names:
        P, N, UV = get(k); P = fix(P); role = owner.get(k)
        if role: q = piece.setdefault(role, ([], [], [])); q[0].append(P); q[1].append(N); q[2].append(UV)
        else: body[0].append(P); body[1].append(N); body[2].append(UV)
    piece = {k: tuple(np.concatenate(x) for x in v) for k, v in piece.items()}
    body = tuple(np.concatenate(x) for x in body)
    if spec.get('neck'):   # the head piece came with the chest: cut it at the neck, the narrowest place under the face
        P, N, UV = piece['head']; tri = P.reshape(-1, 3, 3); cy = tri[:, :, 1].mean(1); top = P[:, 1].max()
        ys = np.arange(top - 0.75, top - 0.2, 0.01)
        wid = [np.ptp(P[(P[:, 1] > y - 0.015) & (P[:, 1] < y + 0.015)][:, [0, 2]], 0).sum() if ((P[:, 1] > y - 0.015) & (P[:, 1] < y + 0.015)).sum() > 8 else 9 for y in ys]
        ny = float(spec['neck']) if not isinstance(spec['neck'], bool) else ys[int(np.argmin(wid))]
        lo = cy < ny; print('neck cut at %.2f (head top %.2f): %d of %d triangles go to the body' % (ny, top, lo.sum(), len(lo)))
        def take(m): return tri[m].reshape(-1, 3), N.reshape(-1, 3, 3)[m].reshape(-1, 3), UV.reshape(-1, 3, 2)[m].reshape(-1, 2)
        piece['head'] = take(~lo); low = take(lo)
        if 'torso' in piece: piece['torso'] = tuple(np.concatenate([a_, b_]) for a_, b_ in zip(piece['torso'], low))
        else: body = tuple(np.concatenate([a_, b_]) for a_, b_ in zip(body, low))
    print('pieces:', {k: len(v[0]) // 3 for k, v in piece.items()}, 'kart', len(body[0]) // 3)
    L = 2.35; thr = 0.012 * L
    arms = 'fore' in piece
    if arms:
        # the steering wheel and the two forearms come as one piece: the wheel is the round part in front, the hands hold it
        T = np.asarray(Image.open(tex).convert('RGB')) / 255.0; TS = T.shape[0] - 1
        P, N, UV = piece.pop('fore'); tri = P.reshape(-1, 3, 3); c = tri.mean(1); uv = UV.reshape(-1, 3, 2).mean(1)
        col = T[(np.clip(uv[:, 1], 0, 1) * TS).astype(int), (np.clip(uv[:, 0], 0, 1) * TS).astype(int)]
        hsv = np.array([colorsys.rgb_to_hsv(*x) for x in col])
        skin = (hsv[:, 0] < 0.12) & (hsv[:, 1] > 0.18) & (hsv[:, 1] < 0.72) & (hsv[:, 2] > 0.5)
        z0, z1 = c[:, 2].min(), c[:, 2].max(); hw = np.abs(c[:, 0]).max()
        front = (c[:, 2] > z0 + 0.55 * (z1 - z0)) & (np.abs(c[:, 0]) < 0.55 * hw)
        seed = front & ~skin if (front & ~skin).sum() > 30 else front
        ctr = c[seed].mean(0); u, sv, vt = np.linalg.svd(c[seed] - ctr); ax = vt[2] if vt[2][2] > 0 else -vt[2]   # the column points forward
        dist_plane = np.abs((c - ctr) @ ax); rad = np.linalg.norm((c - ctr) - np.outer((c - ctr) @ ax, ax), axis=1)
        R = np.percentile(rad[seed], 92); slab = np.percentile(dist_plane[seed], 90) * 1.4 + 0.01
        wheel = (dist_plane < slab) & (rad < R * 1.12) & (~skin | (rad < R * 0.62))
        # crumbs: small loose bits on the hand side that sit on the wheel's rim belong to the wheel (they would fly off with a wave)
        key = np.round(tri.reshape(-1, 3) / 1e-4).astype(np.int64); _, wid = np.unique(key, axis=0, return_inverse=True); wid = wid.reshape(-1, 3)
        par = np.arange(wid.max() + 1)
        def find(x):
            while par[x] != x: par[x] = par[par[x]]; x = par[x]
            return x
        for t in np.where(~wheel)[0]:
            a, b, e = find(wid[t, 0]), find(wid[t, 1]), find(wid[t, 2]); par[b] = a; par[find(e)] = a
        comp = np.array([find(wid[t, 0]) for t in range(len(wid))])
        for sd in (1, -1):
            side = ~wheel & (np.sign(c[:, 0] - ctr[0] + 1e-9) == sd); ids, cnt = np.unique(comp[side], return_counts=True); big = cnt.max() if len(cnt) else 0
            for cid, n_ in zip(ids, cnt):
                m = side & (comp == cid)
                if n_ < 0.06 * big and np.linalg.norm(c[m].mean(0) - ctr) < R * 1.35: wheel |= m
        print('steering wheel: hub (%.2f %.2f %.2f) radius %.2f, column (%.2f %.2f %.2f), %d of %d triangles' % (*ctr, R, *ax, wheel.sum(), len(c)))
        sel = lambda m: (tri[m].reshape(-1, 3), N.reshape(-1, 3, 3)[m].reshape(-1, 3), UV.reshape(-1, 3, 2)[m].reshape(-1, 2))
        piece['wheel'] = sel(wheel); piece['foreL'] = sel(~wheel & (c[:, 0] > ctr[0])); piece['foreR'] = sel(~wheel & (c[:, 0] <= ctr[0]))
    else: print('no arm pieces: this driver turns the head (and the body, with a torso) only')
    # the joints: where each moving piece meets the piece it hangs from
    piv = {}; has = lambda k: k in piece and len(piece[k][0]) > 0
    root = piece['torso'][0] if has('torso') else body[0]
    if has('torso'):
        piv['torso'], n1 = contact(piece['torso'][0], body[0], thr)
        if piv['torso'] is None or n1 < 20: tp = piece['torso'][0]; piv['torso'] = np.array([0, tp[:, 1].min(), tp[:, 2].mean()]); print('hips guessed')
    piv['head'], _ = contact(piece['head'][0], root, thr)
    if piv['head'] is None: hp = piece['head'][0]; piv['head'] = np.array([hp[:, 0].mean(), hp[:, 1].min(), hp[:, 2].mean()]); print('neck guessed')
    for sd in ('LR' if arms else ''):
        if has('uarm' + sd):
            piv['uarm' + sd], _ = contact(piece['uarm' + sd][0], root, thr)
            if piv['uarm' + sd] is None: up = piece['uarm' + sd][0]; piv['uarm' + sd] = up[up[:, 1] > np.percentile(up[:, 1], 85)].mean(0); print('shoulder guessed', sd)
        fp = piece['fore' + sd][0]; att = piece['uarm' + sd][0] if has('uarm' + sd) else root
        piv['fore' + sd], nc = contact(fp, att, thr)
        if piv['fore' + sd] is None: piv['fore' + sd] = fp[fp[:, 2] < np.percentile(fp[:, 2], 8)].mean(0); print('elbow guessed', sd)
        g, ng = contact(fp, piece['wheel'][0], thr)
        if g is None: g = fp[fp[:, 2] > np.percentile(fp[:, 2], 90)].mean(0)
        piv['grip' + sd] = g
    if arms: piv['wheel'] = ctr
    for k, v in piv.items(): print('joint %-6s' % k, '(%.2f %.2f %.2f)' % tuple(v))
    # the wheels of the kart, cut from the body as before
    BP, BN, BUV = body; BF = np.arange(len(BP)).reshape(-1, 3)
    wparts, used = find_wheels(BP, BN, BUV, BF, tex)
    parts = [(0, np.zeros(3), 0.0, BP[BF[~used].ravel()], BN[BF[~used].ravel()], BUV[BF[~used].ravel()])]
    for kind, pv, R_, FF in wparts: parts.append((kind, pv, R_, BP[FF.ravel()], BN[FF.ravel()], BUV[FF.ravel()]))
    order = [(5, 'torso', 'torso'), (6, 'head', 'head'), (7, 'uarmL', 'uarmL'), (8, 'uarmR', 'uarmR'), (9, 'foreL', 'foreL'), (10, 'foreR', 'foreR'), (11, 'wheel', 'wheel')]
    for kind, pk, jk in order:
        if has(pk) and (arms or kind in (5, 6)): P, N, UV = piece[pk]; parts.append((kind, piv[jk], float(spec.get('look', 0)) if kind == 6 else 0.0, P, N, UV))   # the head's 'radius' = how far it may turn (0: the game's own limit)
    z3 = np.zeros((0, 3)); z2 = np.zeros((0, 2))
    if arms: parts += [(12, piv['gripL'], 0.0, z3, z3, z2), (13, piv['gripR'], 0.0, z3, z3, z2), (14, ax, 0.0, z3, z3, z2)]
    with open(out + '.kkm', 'wb') as f:
        f.write(b'KKM1'); f.write(struct.pack('>I', len(parts)))
        for kind, pv, R_, P, N, UV in parts:
            V = np.zeros((len(P), 12), np.float32); V[:, 0:3] = P - (pv if kind not in (12, 13, 14) else 0); V[:, 3:6] = N; V[:, 6:8] = UV; V[:, 8:12] = 1
            f.write(struct.pack('>I4fI', kind, *pv, R_, len(V))); f.write(V.astype('>f4').tobytes())
    Image.open(tex).convert('RGB').resize((1024, 1024), Image.LANCZOS).save(out + '.png', optimize=True)
    allQ = np.concatenate([p[3] for p in parts if len(p[3])])
    print('kart %.2f long, %.2f wide, %.2f tall; %d triangles (driver %d)' % (np.ptp(allQ[:, 2]), np.ptp(allQ[:, 0]), allQ[:, 1].max(), len(allQ) // 3, sum(len(p[3]) for p in parts if p[0] >= 5) // 3))

if __name__ == '__main__': main()
