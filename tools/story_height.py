#!/usr/bin/env python3
"""Makes src/game/storymap_h.h (how high the land of Sean's world map stands) and content/img/story_map.png from
assets/story/world_map_src.png.   python3 tools/story_height.py"""
from PIL import Image, ImageFilter
import numpy as np
im = Image.open('assets/story/world_map_src.png').convert('RGB')
im.resize((1024, 1024), Image.LANCZOS).save('content/img/story_map.png', optimize=True)
a = np.asarray(im).astype(np.float32); r, g, b = a[..., 0], a[..., 1], a[..., 2]
lum = 0.3 * r + 0.59 * g + 0.11 * b; mx = a.max(2); mn = a.min(2); sat = (mx - mn) / (mx + 1)
water = ((b > 120) & (b > r + 40) & (b > g + 5)).astype(np.float32)          # the bright sea and rivers (not the navy name plaques)
cloud = ((lum > 200) & (sat < 0.22)).astype(np.float32)
rock = ((lum < 115) & ~(g > r + 12) & (water < 0.5) & ~(b > r + 25)).astype(np.float32)
NX, NZ = 96, 84
def down(m): return np.asarray(Image.fromarray((m * 255).astype(np.uint8)).resize((NX + 1, NZ + 1), Image.BOX)).astype(np.float32) / 255
def blur(m, s): return np.asarray(Image.fromarray((np.clip(m, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(s))).astype(np.float32) / 255
W = down(water); C = down(cloud); R = down(rock)
land = 1 - blur(np.clip((W - 0.35) * 2.5, 0, 1), 1.0)
h = land * 1.0 + blur(R, 2.0) * 1.8 * land + blur(C, 1.4) * 0.9 * land
h = h / h.max(); q = (h * 255).astype(np.uint8)
L = ["// storymap_h.h - made by tools/story_height.py from Sean's world map: how high the land stands at each point (0 sea .. 255 peak)",
     "#define SMH_NX %d" % NX, "#define SMH_NZ %d" % NZ, "static const unsigned char SMH[%d][%d] = {" % (NZ + 1, NX + 1)]
for row in q: L.append("  {" + ",".join(str(int(v)) for v in row) + "},")
L.append("};"); open('src/game/storymap_h.h', 'w').write("\n".join(L) + "\n")
Image.fromarray(q).resize((480, 420)).save('/tmp/claude-0/-home-claude/fecd6cea-9518-53bf-8507-8392c7159c17/scratchpad/smh.png')
