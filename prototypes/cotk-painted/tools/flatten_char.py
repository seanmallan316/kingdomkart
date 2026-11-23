"""Give a 3D character flat, painted colours with painted-in shade (comic look), keeping its UV layout.
The texture of these models is thousands of tiny scattered islands, so it cannot be smoothed as a
picture. Instead: sample the surface, average colours and surface directions in small 3D cells, soften
across neighbouring cells, then for every texel look the result up smoothly at its 3D position, snap
the colour to a small palette (flat areas with clean edges) and darken the parts that face downward
with the scenery's purple shade. No lighting is needed in the game, so the bumpy surface cannot
make grain.
Usage: python3 -I flatten_char.py MODEL.glb OUT.png [cell_cm] [colours] [soften_cells]"""
import sys
import numpy as np
import trimesh
from PIL import Image, ImageEnhance
from scipy import ndimage

src, out = sys.argv[1], sys.argv[2]
cell = float(sys.argv[3]) / 100 if len(sys.argv) > 3 else 0.005
ncol = int(sys.argv[4]) if len(sys.argv) > 4 else 96
soft = float(sys.argv[5]) if len(sys.argv) > 5 else 0.7
S = 1536
SHADE = np.array([0.60, 0.58, 0.78])          # same purple-blue shade as the scenery
LIGHT = np.array([-0.25, 1.0, 0.35]); LIGHT /= np.linalg.norm(LIGHT)   # from above, a little in front

scene = trimesh.load(src, process=False)
geoms = [g for g in scene.geometry.values() if getattr(g.visual, 'uv', None) is not None and g.visual.material is not None
         and getattr(g.visual.material, 'baseColorTexture', None) is not None]
g = max(geoms, key=lambda m: len(m.faces))
tex = np.asarray(g.visual.material.baseColorTexture.convert('RGB')).astype(np.float32)
TH, TW = tex.shape[:2]
V, F, UV, N = np.asarray(g.vertices), np.asarray(g.faces), np.asarray(g.visual.uv), np.asarray(g.vertex_normals)

# sample every triangle densely enough to cover its texels
tuv, tp, tn = UV[F], V[F], N[F]
e = np.linalg.norm((tuv - np.roll(tuv, 1, axis=1)) * S, axis=2).max(axis=1)
n_of = np.clip(np.ceil(e * 1.5).astype(int) + 1, 2, 48)
uvs, pts, nrm = [], [], []
for n in np.unique(n_of):
    sel = n_of == n
    i, j = np.meshgrid(np.arange(n + 1), np.arange(n + 1), indexing='ij')
    keep = i + j <= n
    b = np.stack([i[keep], j[keep], n - i[keep] - j[keep]], axis=1) / n
    uvs.append(np.einsum('kb,tbc->tkc', b, tuv[sel]).reshape(-1, 2))
    pts.append(np.einsum('kb,tbc->tkc', b, tp[sel]).reshape(-1, 3))
    nrm.append(np.einsum('kb,tbc->tkc', b, tn[sel]).reshape(-1, 3))
uv, p, nn = np.concatenate(uvs), np.concatenate(pts), np.concatenate(nrm)
u, v = uv[:, 0] % 1.0, (1.0 - uv[:, 1]) % 1.0            # trimesh flips v on load
col = tex[np.clip((v * TH).astype(int), 0, TH - 1), np.clip((u * TW).astype(int), 0, TW - 1)]

# average colour and surface direction in 3D cells, soften across neighbours
lo = p.min(axis=0) - cell * 10
fpos = (p - lo) / cell                                   # position in cell units
idx = np.floor(fpos).astype(int)
dims = idx.max(axis=0) + 11
acc = np.zeros((*dims, 3), np.float32); nac = np.zeros((*dims, 3), np.float32); cnt = np.zeros(dims, np.float32)
ix = (idx[:, 0], idx[:, 1], idx[:, 2])
np.add.at(acc, ix, col); np.add.at(nac, ix, nn.astype(np.float32)); np.add.at(cnt, ix, 1)
for c in range(3):
    acc[..., c] = ndimage.gaussian_filter(acc[..., c], soft)
    nac[..., c] = ndimage.gaussian_filter(nac[..., c], 0.028 / cell)     # shade follows the big forms only (about 3 cm)
cnt_c = ndimage.gaussian_filter(cnt, soft)
grid = acc / np.maximum(cnt_c, 1e-6)[..., None]

# smooth lookup at each sample's exact 3D position (no cell blocks)
coords = (fpos - 0.5).T
c_s = np.stack([ndimage.map_coordinates(grid[..., c], coords, order=1, mode='nearest') for c in range(3)], axis=1)
n_s = np.stack([ndimage.map_coordinates(nac[..., c], coords, order=1, mode='nearest') for c in range(3)], axis=1)
n_s /= np.maximum(np.linalg.norm(n_s, axis=1, keepdims=True), 1e-6)

# lift the colour, then snap to a small palette: flat areas with clean edges
strip = Image.fromarray(np.clip(c_s, 0, 255).astype(np.uint8).reshape(-1, 1, 3))
strip = ImageEnhance.Color(strip).enhance(1.2)
strip = ImageEnhance.Brightness(strip).enhance(1.18)
strip = ImageEnhance.Contrast(strip).enhance(1.12)
pal_img = strip.resize((1, 200000), Image.NEAREST).quantize(colors=ncol, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
pal = np.asarray(pal_img.getpalette()[:ncol * 3], np.float32).reshape(-1, 3)
cs = np.asarray(strip, np.float32).reshape(-1, 3)
best = np.zeros(len(cs), int); bd = np.full(len(cs), 1e12, np.float32)
for k in range(len(pal)):
    d = ((cs - pal[k]) ** 2).sum(axis=1)
    m = d < bd; best[m] = k; bd[m] = d[m]
flat = pal[best]

# clothing detail: spots clearly darker than what surrounds them (seams, folds, straps, pattern lines) are inked a little
lum_g = acc @ np.array([.299, .587, .114], np.float32)
wide_g = ndimage.gaussian_filter(lum_g, 0.012 / cell) / np.maximum(ndimage.gaussian_filter(cnt_c, 0.012 / cell), 1e-6)
near_g = ndimage.gaussian_filter(lum_g, 0.6) / np.maximum(ndimage.gaussian_filter(cnt_c, 0.6), 1e-6)
dark_g = np.clip((wide_g - near_g - 12.0) / 22.0, 0, 1)
db = ndimage.map_coordinates(dark_g, coords, order=1, mode='nearest')[:, None]
flat = flat * (1 - db * 0.55) + np.array([34.0, 22.0, 20.0]) * (db * 0.55)

# painted-in shade: surfaces that turn away from the light take the purple shade, with a crisp edge
lit = np.clip((n_s @ LIGHT - 0.02) / 0.10, 0, 1)[:, None]

# ---- the face: keep the fine detail and ink in the eyes, brows and mouth so they read from far away ----
top = p[:, 1].max()
head = (p[:, 1] > top - 0.30) & (nn[:, 2] > 0.05)             # the front of the head (models face +z)
if head.sum() > 1000:
    fc = 0.0022                                                # 2 mm cells, head only
    hp = p[head]; hlo = hp.min(axis=0) - fc * 8
    hf = (hp - hlo) / fc; hi = np.floor(hf).astype(int); hd = hi.max(axis=0) + 9
    hacc = np.zeros((*hd, 3), np.float32); hcnt = np.zeros(hd, np.float32)
    hx = (hi[:, 0], hi[:, 1], hi[:, 2])
    np.add.at(hacc, hx, col[head]); np.add.at(hcnt, hx, 1)
    lum_acc = hacc @ np.array([.299, .587, .114], np.float32)
    fine = np.stack([ndimage.gaussian_filter(hacc[..., c], 0.9) for c in range(3)], axis=-1)
    fcnt = ndimage.gaussian_filter(hcnt, 0.9)
    fine /= np.maximum(fcnt, 1e-6)[..., None]
    # how much darker a spot is than the skin around it (about 1.2 cm around): eyes, brows, lips, nostrils
    wide = ndimage.gaussian_filter(lum_acc, 5.5) / np.maximum(ndimage.gaussian_filter(hcnt, 5.5), 1e-6)
    near = ndimage.gaussian_filter(lum_acc, 1.1) / np.maximum(ndimage.gaussian_filter(hcnt, 1.1), 1e-6)
    dark = np.clip((wide - near - 14.0) / 16.0, 0, 1)          # 0 = skin, 1 = a clear dark feature
    hc = (hf - 0.5).T
    f_s = np.stack([ndimage.map_coordinates(fine[..., c], hc, order=1, mode='nearest') for c in range(3)], axis=1)
    d_s = ndimage.map_coordinates(dark, hc, order=1, mode='nearest')
    # lift and lightly flatten the face colours on their own palette
    fstrip = Image.fromarray(np.clip(f_s, 0, 255).astype(np.uint8).reshape(-1, 1, 3))
    fstrip = ImageEnhance.Color(fstrip).enhance(1.2); fstrip = ImageEnhance.Brightness(fstrip).enhance(1.2); fstrip = ImageEnhance.Contrast(fstrip).enhance(1.15)
    fpal_img = fstrip.resize((1, 120000), Image.NEAREST).quantize(colors=40, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    fpal = np.asarray(fpal_img.getpalette()[:120], np.float32).reshape(-1, 3)
    fcs = np.asarray(fstrip, np.float32).reshape(-1, 3)
    fb = np.zeros(len(fcs), int); fd = np.full(len(fcs), 1e12, np.float32)
    for k in range(len(fpal)):
        dd = ((fcs - fpal[k]) ** 2).sum(axis=1)
        m = dd < fd; fb[m] = k; fd[m] = dd[m]
    face = fpal[fb]
    ink = np.array([34.0, 22.0, 20.0])
    face = face * (1 - d_s[:, None] * 0.82) + ink * (d_s[:, None] * 0.82)      # ink in the dark features
    # blend into the rest of the head over the last few centimetres so there is no seam
    w = np.clip((p[head, 1] - (top - 0.30)) / 0.04, 0, 1) * np.clip((nn[head, 2] - 0.05) / 0.25, 0, 1)
    flat[head] = flat[head] * (1 - w[:, None]) + face * w[:, None]
    lit[head] = np.maximum(lit[head], (w * 0.75)[:, None])    # keep the face mostly in the light
    print('face samples', int(head.sum()), 'inked %.1f%%' % (100 * (d_s > 0.5).mean()))

flat = flat * (SHADE + (1 - SHADE) * lit)

# write into the UV layout and fill the gaps between islands with the nearest colour
img = np.zeros((S, S, 3), np.uint8); has = np.zeros((S, S), bool)
x, y = np.clip((u * S).astype(int), 0, S - 1), np.clip((v * S).astype(int), 0, S - 1)
img[y, x] = np.clip(flat, 0, 255).astype(np.uint8); has[y, x] = True
near = ndimage.distance_transform_edt(~has, return_distances=False, return_indices=True)
Image.fromarray(img[near[0], near[1]]).save(out)
print(out, 'samples', len(p), 'covered %.0f%%' % (has.mean() * 100))
