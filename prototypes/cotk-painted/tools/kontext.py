"""Paint over one block-out with FLUX.1 Kontext [dev] through Hugging Face's router (fal provider).
Usage: python3 -I kontext.py BLOCKOUT.png OUT.png "subject text"
The proxy adds the Hugging Face token; nothing secret is in this file."""
import sys, base64, json, io, urllib.request
from PIL import Image

STYLE = ("Repaint this 3D block-out as a hand-drawn comic-book game illustration, like a painted isometric "
         "game world: bold dark brown-black ink outlines, clean flat colours with soft two-tone cel shading, "
         "light hand-drawn hatching, warm sunlight from the upper left. Keep the exact same shape, size, position "
         "and camera angle. Keep the plain white background. Subject: ")
src, out, subject = sys.argv[1], sys.argv[2], sys.argv[3]
b64 = base64.b64encode(open(src, 'rb').read()).decode()
body = {"image_url": "data:image/png;base64," + b64, "prompt": STYLE + subject, "guidance_scale": float(sys.argv[4]) if len(sys.argv) > 4 else 2.5,
        "num_inference_steps": 28, "sync_mode": True, "output_format": "png"}
req = urllib.request.Request("https://router.huggingface.co/fal-ai/fal-ai/flux-kontext/dev",
                             data=json.dumps(body).encode(), method="POST",
                             headers={"Content-Type": "application/json", "Authorization": "Bearer proxy-injected"})
try:
    r = json.load(urllib.request.urlopen(req, timeout=240))
except urllib.error.HTTPError as e:
    print("HTTP", e.code, e.read()[:400]); sys.exit(1)
url = r["images"][0]["url"]
if url.startswith("data:"):
    data = base64.b64decode(url.split(",", 1)[1])
else:
    print("got a link instead of the image:", url[:120]); sys.exit(2)
Image.open(io.BytesIO(data)).convert("RGB").save(out)
print(out, Image.open(out).size)
