# Paints a 3D model with the racer's own pictures: finds where each picture's camera stood by matching outlines, then
# projects the pictures onto the model's surface (each spot takes its color from the picture that sees it most squarely).
import numpy as np, sys, json
from PIL import Image
sys.path.insert(0, sys.argv[1]); from glb import load_glb
def load_glb_full(path):
    import struct
    b = open(path, 'rb').read(); off = 12; js = None; bn = None
    while off < len(b):
        ln, ty = struct.unpack('<II', b[off:off + 8]); ch = b[off + 8:off + 8 + ln]; off += 8 + ln
        if ty == 0x4E4F534A: js = json.loads(ch)
        else: bn = ch
    def acc(i):
        a = js['accessors'][i]; bv = js['bufferViews'][a['bufferView']]; o = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
        dt = {5126: np.float32, 5125: np.uint32, 5123: np.uint16}[a['componentType']]; nc = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3}[a['type']]
        x = np.frombuffer(bn, dt, a['count'] * nc, o); return x.reshape(a['count'], nc) if nc > 1 else x
    pr = js['meshes'][0]['primitives'][0]; at = pr['attributes']
    return acc(at['POSITION']).astype(np.float64), acc(at['NORMAL']).astype(np.float64), acc(at['TEXCOORD_0']).astype(np.float64), acc(pr['indices']).astype(np.int64).reshape(-1, 3)
src, imgdir, name, out = sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5]
P, N, UV, F = load_glb_full(src)
c = (P.max(0) + P.min(0)) / 2; P = P - c
def basis(yaw, el):   # camera looking at the model from direction (yaw around y, elevation up)
    d = np.array([np.sin(yaw) * np.cos(el), np.sin(el), np.cos(yaw) * np.cos(el)])   # from model toward camera
    r = np.cross([0, 1, 0], d); r /= np.linalg.norm(r); u = np.cross(d, r); return r, u, d
import cv2
from scipy import ndimage
G = 256
def clean_alpha(A):   # the kart's own outline: drop stray crumbs (shadow bits) around it
    m = A[:, :, 3] > 0.5; lab, n = ndimage.label(m)
    if n > 1:
        sz = ndimage.sum(m, lab, range(1, n + 1)); keep = np.isin(lab, 1 + np.nonzero(sz > sz.max() * 0.03)[0]); m = keep
    return m
def proj(r, u):
    x = P @ r; y = P @ u; return x, y
def sil_box(r, u):
    x, y = proj(r, u); return x.min(), x.max(), y.min(), y.max()
def sil(r, u, box):
    x, y = proj(r, u); x0, x1, y0, y1 = box; sc = (G - 8) / max(x1 - x0, y1 - y0)
    px = ((x - (x0 + x1) / 2) * sc + G / 2); py = (G / 2 - (y - (y0 + y1) / 2) * sc)
    pts = np.stack([px, py], 1)[F].astype(np.int32); m = np.zeros((G, G), np.uint8); cv2.fillPoly(m, list(pts), 1); return m > 0
def img_mask(m):
    ys, xs = np.nonzero(m); x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max(); sc = (G - 8) / max(x1 - x0, y1 - y0)
    M = np.ascontiguousarray(np.array([[sc, 0, G / 2 - (x0 + x1) / 2 * sc], [0, sc, G / 2 - (y0 + y1) / 2 * sc]], np.float32))
    return cv2.warpAffine(m.astype(np.uint8), M, (G, G), flags=cv2.INTER_NEAREST) > 0
views = {}; ims = {}; masks = {}
for v in ['f', 'fl', 'l', 'bl', 'b']:
    try: im = Image.open(f'{imgdir}/ride_{name}_{v}.png').convert('RGBA')
    except FileNotFoundError: continue
    ims[v] = im; masks[v] = img_mask(clean_alpha(np.asarray(im).astype(np.float32) / 255))
def fit(v, yaws, els=range(0, 31, 3)):
    best = None
    for yd in yaws:
        for ed in els:
            r, u, d = basis(np.radians(yd), np.radians(ed)); s = sil(r, u, sil_box(r, u)); m = masks[v]; iou = (s & m).sum() / (s | m).sum()
            if best is None or iou > best[0]: best = (iou, yd, ed, 0)
    return best
# which way the kart faces: the front picture's best angle, then the rest must follow round the kart in order
cand = []; headz = P[P[:, 1] > np.percentile(P[:, 1], 98)][:, 2].mean(); fc = 0 if headz < 0 else 180   # the driver sits behind the middle, so the front is the other way
print('front toward', '+z' if fc == 0 else '-z')
for yd in range(fc - 60, fc + 61, 4):
    b1 = fit('f', [yd], range(0, 25, 6)); b2 = fit('b', [yd + 180], range(0, 25, 6)); cand.append((b1[0] + b2[0], yd))
yf = max(cand)[1]
best_total = None
for sg in (1, -1):
    res = {}; tot = 0
    for v, off in (('f', 0), ('fl', 45), ('l', 90), ('bl', 135), ('b', 180)):
        if v not in ims: continue
        e = yf + sg * off; r_ = fit(v, range(e - 16, e + 17, 2)); res[v] = r_; tot += r_[0]
    if best_total is None or tot > best_total[0]: best_total = (tot, res)
for v, b_ in best_total[1].items(): print(v, 'IoU %.3f yaw %d elev %d' % b_[:3]); views[v] = (ims[v], b_)
json.dump({k: list(b[1]) for k, b in views.items()}, open(out + '.cams.json', 'w'))

# ---- painting: every texel of the model's picture gets its color from the views that see it ----
T = int(sys.argv[6]) if len(sys.argv) > 6 else 1024
cams = []   # (image array RGBA float, r, u, d, mapping) including mirrored copies for the right side
for v, (im, best) in views.items():
    for mir in ((0, 1) if v in ('fl', 'l', 'bl') else (0,)):
        yaw = np.radians(best[1] if not mir else 2 * yf - best[1]); el = np.radians(best[2]); r, u, d = basis(yaw, el)
        I = im.transpose(Image.FLIP_LEFT_RIGHT) if mir else im; A = np.asarray(I).astype(np.float32) / 255
        a = clean_alpha(A); ys, xs = np.nonzero(a)
        sx0, sx1, sy0, sy1 = sil_box(r, u)
        ix0, ix1, iy0, iy1 = xs.min(), xs.max(), ys.min(), ys.max()
        # model (x,y) -> image pixel: match the outlines' boxes
        kx = (ix1 - ix0) / (sx1 - sx0); ky = (iy1 - iy0) / (sy1 - sy0)
        cams.append(dict(A=A, r=r, u=u, d=d, map=(sx0, sy1, ix0, iy0, kx, ky), name=v + ('_m' if mir else '')))
def to_img(cam, X):
    sx0, sy1, ix0, iy0, kx, ky = cam['map']; return ix0 + (X @ cam['r'] - sx0) * kx, iy0 + (sy1 - X @ cam['u']) * ky
# depth buffers per camera (who is in front), rasterized at image resolution / 2
def zbuf(cam):
    A = cam['A']; H, W = A.shape[0] // 2, A.shape[1] // 2; Z = np.full((H, W), -1e9)
    px, py = to_img(cam, P); px = px / 2; py = py / 2; pz = P @ cam['d']
    for t in F:
        xs, ys, zs = px[t], py[t], pz[t]; x0, x1 = int(max(0, np.floor(xs.min()))), int(min(W - 1, np.ceil(xs.max()))); y0, y1 = int(max(0, np.floor(ys.min()))), int(min(H - 1, np.ceil(ys.max())))
        if x1 < x0 or y1 < y0: continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        den = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
        if abs(den) < 1e-12: continue
        l0 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / den; l1 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / den; l2 = 1 - l0 - l1
        ins = (l0 >= -1e-3) & (l1 >= -1e-3) & (l2 >= -1e-3); z = l0 * zs[0] + l1 * zs[1] + l2 * zs[2]
        sub = Z[y0:y1 + 1, x0:x1 + 1]; upd = ins & (z > sub); sub[upd] = z[upd]
    return Z
for cam in cams: cam['Z'] = zbuf(cam)
# texels: rasterize each triangle in UV space
tex = np.zeros((T, T, 3), np.float32); wsum = np.zeros((T, T), np.float32); cover = np.zeros((T, T), bool)
U = UV.copy(); U[:, 1] = U[:, 1]   # glTF uv: v down from top
for t in F:
    us, vs = U[t, 0] * T, U[t, 1] * T; x0, x1 = int(max(0, np.floor(us.min()) - 1)), int(min(T - 1, np.ceil(us.max()) + 1)); y0, y1 = int(max(0, np.floor(vs.min()) - 1)), int(min(T - 1, np.ceil(vs.max()) + 1))
    gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
    den = (vs[1] - vs[2]) * (us[0] - us[2]) + (us[2] - us[1]) * (vs[0] - vs[2])
    if abs(den) < 1e-12: continue
    l0 = ((vs[1] - vs[2]) * (gx - us[2]) + (us[2] - us[1]) * (gy - vs[2])) / den; l1 = ((vs[2] - vs[0]) * (gx - us[2]) + (us[0] - us[2]) * (gy - vs[2])) / den; l2 = 1 - l0 - l1
    ins = (l0 >= -0.02) & (l1 >= -0.02) & (l2 >= -0.02)
    if not ins.any(): continue
    L = np.stack([l0[ins], l1[ins], l2[ins]], 1); X = L @ P[t]; Nn = L @ N[t]; Nn /= np.linalg.norm(Nn, axis=1, keepdims=True) + 1e-9
    ty, tx = (gy[ins] - 0.5).astype(int), (gx[ins] - 0.5).astype(int)
    col = np.zeros((len(X), 3)); ws = np.zeros(len(X))
    for cam in cams:
        facing = Nn @ cam['d']; wgt = np.clip(facing, 0, 1) ** 14
        ix, iy = to_img(cam, X); H, W = cam['A'].shape[:2]
        okk = (wgt > 0.01) & (ix >= 0) & (iy >= 0) & (ix < W - 1) & (iy < H - 1)
        if not okk.any(): continue
        zi = cam['Z'][np.clip((iy / 2).astype(int), 0, cam['Z'].shape[0] - 1), np.clip((ix / 2).astype(int), 0, cam['Z'].shape[1] - 1)]
        vis = okk & ((X @ cam['d']) > zi - 0.02 * (P.max() - P.min()))
        if not vis.any(): continue
        px_ = cam['A'][iy[vis].astype(int), ix[vis].astype(int)]
        good = px_[:, 3] > 0.5; idx = np.nonzero(vis)[0][good]
        col[idx] += px_[good, :3] * wgt[idx, None]; ws[idx] += wgt[idx]
    ok2 = ws > 0; tex[ty[ok2], tx[ok2]] = col[ok2] / ws[ok2, None]; cover[ty, tx] = True; wsum[ty[ok2], tx[ok2]] = 1
# unseen spots (underneath): spread neighboring colors into them
known = wsum > 0; cur = tex.copy()
for it in range(40):
    if known[cover].all(): break
    acc = np.zeros_like(cur); n = np.zeros(known.shape)
    for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        acc += np.roll(np.roll(cur * known[:, :, None], dy, 0), dx, 1); n += np.roll(np.roll(known, dy, 0), dx, 1)
    new = (~known) & (n > 0); cur[new] = acc[new] / n[new, None]; known = known | new
cur[~known] = cur[known].mean(0) * 0.6
# bleed the edges outward so mipmaps don't pick up black
for it in range(4):
    acc = np.zeros_like(cur); n = np.zeros(known.shape); kk = known
    for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        acc += np.roll(np.roll(cur * kk[:, :, None], dy, 0), dx, 1); n += np.roll(np.roll(kk, dy, 0), dx, 1)
Image.fromarray((np.clip(cur, 0, 1) * 255).astype(np.uint8)).save(out + '_tex.png')
print('coverage seen %.1f%%' % (100 * wsum[cover].mean()))
np.savez(out + '_mesh.npz', P=P, N=N, UV=UV, F=F)
