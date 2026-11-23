"""Give a 3D character flat, painted colours (comic look) while keeping its UV layout.
The texture of these models is thousands of tiny scattered islands, so it cannot be smoothed as a
picture. Instead: sample the surface, average the colours in small 3D cells, soften across neighbouring
cells, snap to a small palette, and write the result back into the same UV layout.
Usage: python3 -I flatten_char.py MODEL.glb OUT.jpg [cell_cm] [colours] [soften]"""
import sys, io
import numpy as np
import trimesh
from PIL import Image, ImageEnhance
from scipy import ndimage

src, out = sys.argv[1], sys.argv[2]
cell = float(sys.argv[3]) / 100 if len(sys.argv) > 3 else 0.012
ncol = int(sys.argv[4]) if len(sys.argv) > 4 else 26
soft = float(sys.argv[5]) if len(sys.argv) > 5 else 1.1
S = 1024

scene = trimesh.load(src, process=False)
geoms = [g for g in scene.geometry.values() if getattr(g.visual, 'uv', None) is not None and g.visual.material is not None
         and getattr(g.visual.material, 'baseColorTexture', None) is not None]
g = max(geoms, key=lambda m: len(m.faces))
tex = np.asarray(g.visual.material.baseColorTexture.convert('RGB')).astype(np.float32)
TH, TW = tex.shape[:2]
V, F, UV = np.asarray(g.vertices), np.asarray(g.faces), np.asarray(g.visual.uv)

# sample every triangle densely enough to cover its texels
tuv, tp = UV[F], V[F]                                    # (T,3,2), (T,3,3)
e = np.linalg.norm((tuv - np.roll(tuv, 1, axis=1)) * S, axis=2).max(axis=1)
n_of = np.clip(np.ceil(e * 1.5).astype(int) + 1, 2, 48)
uvs, pts = [], []
for n in np.unique(n_of):
    sel = n_of == n
    i, j = np.meshgrid(np.arange(n + 1), np.arange(n + 1), indexing='ij')
    keep = i + j <= n
    b = np.stack([i[keep], j[keep], n - i[keep] - j[keep]], axis=1) / n      # barycentric grid
    uvs.append(np.einsum('kb,tbc->tkc', b, tuv[sel]).reshape(-1, 2))
    pts.append(np.einsum('kb,tbc->tkc', b, tp[sel]).reshape(-1, 3))
uv, p = np.concatenate(uvs), np.concatenate(pts)
u, v = uv[:, 0] % 1.0, (1.0 - uv[:, 1]) % 1.0            # trimesh flips v on load
col = tex[np.clip((v * TH).astype(int), 0, TH - 1), np.clip((u * TW).astype(int), 0, TW - 1)]

# average in 3D cells, soften across neighbours
lo = p.min(axis=0) - cell * 4
idx = np.floor((p - lo) / cell).astype(int)
dims = idx.max(axis=0) + 5
acc = np.zeros((*dims, 3), np.float32); cnt = np.zeros(dims, np.float32)
np.add.at(acc, (idx[:, 0], idx[:, 1], idx[:, 2]), col); np.add.at(cnt, (idx[:, 0], idx[:, 1], idx[:, 2]), 1)
for c in range(3): acc[..., c] = ndimage.gaussian_filter(acc[..., c], soft)
cnt = ndimage.gaussian_filter(cnt, soft)
grid = acc / np.maximum(cnt, 1e-6)[..., None]

# snap to a small palette, lift the colour a little
occ = cnt > 1e-4
strip = Image.fromarray(np.clip(grid[occ], 0, 255).astype(np.uint8).reshape(-1, 1, 3))
strip = ImageEnhance.Color(strip).enhance(1.3)
strip = ImageEnhance.Brightness(strip).enhance(1.15)
q = strip.quantize(colors=ncol, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert('RGB')
grid[occ] = np.asarray(q).reshape(-1, 3)

# write back into the UV layout and fill the gaps between islands with the nearest colour
img = np.zeros((S, S, 3), np.uint8); has = np.zeros((S, S), bool)
x, y = np.clip((u * S).astype(int), 0, S - 1), np.clip((v * S).astype(int), 0, S - 1)
img[y, x] = np.clip(grid[idx[:, 0], idx[:, 1], idx[:, 2]], 0, 255).astype(np.uint8); has[y, x] = True
near = ndimage.distance_transform_edt(~has, return_distances=False, return_indices=True)
img = img[near[0], near[1]]
Image.fromarray(img).save(out, quality=93)
print(out, 'samples', len(p), 'cells', int(occ.sum()), 'covered %.0f%%' % (has.mean() * 100))
