"""Line up each AI painting with its block-out render and write assets/paint/paint.json.

The painter may return a different image size or shift the building a little. We find the
shape (everything that is not the white background) in both pictures and work out the
scale and offset that maps block-out UVs onto the painting. Run: python3 -I tools/fit.py RAW_DIR
"""
import json, sys, os
from PIL import Image
import numpy as np

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
raw = sys.argv[1] if len(sys.argv) > 1 else None
proj = json.load(open(os.path.join(HERE, 'assets/blockout/projectors.json')))

def bbox(path):
    a = np.asarray(Image.open(path).convert('RGB')).astype(int)
    mask = (a.min(axis=2) < 235)
    ys, xs = np.nonzero(mask)
    h, w = mask.shape
    # in UV space (0..1, v up)
    return xs.min() / w, xs.max() / w, 1 - ys.max() / h, 1 - ys.min() / h, w, h

out = {}
for oid, m in proj.items():
    entry = {'matrix': m['matrix'], 'xf': [1, 1, 0, 0], 'file': None}
    src = raw and os.path.join(raw, oid + '.png')
    if src and os.path.exists(src):
        b0 = bbox(os.path.join(HERE, 'assets/blockout', oid + '.png'))
        b1 = bbox(src)
        sx = (b1[1] - b1[0]) / (b0[1] - b0[0]); sy = (b1[3] - b1[2]) / (b0[3] - b0[2])
        ox = b1[0] - b0[0] * sx; oy = b1[2] - b0[2] * sy
        entry['xf'] = [round(sx, 5), round(sy, 5), round(ox, 5), round(oy, 5)]
        im = Image.open(src).convert('RGB')
        # keep it light for phones
        if max(im.size) > 1280: im.thumbnail((1280, 1280))
        im.save(os.path.join(HERE, 'assets/paint', oid + '.jpg'), quality=88)
        entry['file'] = oid + '.jpg'
        print(oid, 'fitted', entry['xf'])
    else:
        print(oid, 'no painting yet, using the block-out')
    out[oid] = entry
json.dump(out, open(os.path.join(HERE, 'assets/paint/paint.json'), 'w'), indent=1)
