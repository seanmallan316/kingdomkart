"""Make one repeating ground texture with FLUX.1 [dev] through Hugging Face's router (fal provider),
then make it seamless. Usage: python3 -I flux_tile.py OUT.png "what the ground is"
The proxy adds the Hugging Face token; nothing secret is in this file."""
import sys, base64, json, io, urllib.request
import numpy as np
from PIL import Image

STYLE = ("Seamless tileable top-down ground texture for a hand-drawn comic-book isometric game, viewed straight "
         "from above, flat even lighting, no perspective, no horizon, no objects, no shadows, no text. "
         "Bold dark brown ink linework, clean flat colours in two or three cel-shaded tones, light hatching. ")
out, subject = sys.argv[1], sys.argv[2]
body = {"prompt": STYLE + subject, "image_size": "square_hd", "num_inference_steps": 28,
        "guidance_scale": 3.5, "sync_mode": True, "output_format": "png", "seed": int(sys.argv[3]) if len(sys.argv) > 3 else 11}
req = urllib.request.Request("https://router.huggingface.co/fal-ai/fal-ai/flux/dev",
                             data=json.dumps(body).encode(), method="POST",
                             headers={"Content-Type": "application/json", "Authorization": "Bearer proxy-injected"})
try:
    r = json.load(urllib.request.urlopen(req, timeout=240))
except urllib.error.HTTPError as e:
    print("HTTP", e.code, e.read()[:400]); sys.exit(1)
url = r["images"][0]["url"]
if not url.startswith("data:"):
    print("got a link instead of the image:", url[:120]); sys.exit(2)
im = Image.open(io.BytesIO(base64.b64decode(url.split(",", 1)[1]))).convert("RGB")
im.save(out.replace('.png', '_raw.png'))
# make it repeat without a seam: blend the picture with a copy shifted by half,
# using the shifted copy near the edges and the original near the middle
a = np.asarray(im).astype(float); n = a.shape[0]
s = np.roll(np.roll(a, n // 2, 0), n // 2, 1)
t = np.minimum(np.arange(n), n - 1 - np.arange(n)) / (n / 2)       # 0 at edges, 1 in the middle
w = np.clip((np.minimum.outer(t, t) - 0.12) / 0.3, 0, 1)[..., None]
Image.fromarray((a * w + s * (1 - w)).astype('uint8')).save(out)
print(out, im.size)
