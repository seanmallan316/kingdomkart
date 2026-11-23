"""Cut the clean middle out of a generated ground picture and make it repeat without seams.
Usage: python3 -I seamless.py RAW.png OUT.png [keep_fraction]"""
import sys
import numpy as np
from PIL import Image
src, out = sys.argv[1], sys.argv[2]
keep = float(sys.argv[3]) if len(sys.argv) > 3 else 0.5
im = Image.open(src).convert('RGB'); W = im.size[0]; c = int(W * keep)
o = (W - c) // 2
a = np.asarray(im.crop((o, o, o + c, o + c)).resize((512, 512), Image.LANCZOS)).astype(float)
n = a.shape[0]
s = np.roll(np.roll(a, n // 2, 0), n // 2, 1)
t = np.minimum(np.arange(n), n - 1 - np.arange(n)) / (n / 2)
w = np.clip((np.minimum.outer(t, t) - 0.05) / 0.25, 0, 1)[..., None]
Image.fromarray((a * w + s * (1 - w)).astype('uint8')).save(out)
