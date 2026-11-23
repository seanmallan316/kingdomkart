# Painted Glen: art-method test for Colors of the King

A small playable test of the "hand-drawn art beamed onto simple 3D blocks" method
(the DuneCrawl dev video), in the Colors of the King world.

## How it works
1. `world.js` holds every building as a few plain shapes (the block-out) and the village layout.
2. `tools/bake.mjs` renders each block-out alone from the fixed game camera angle (`assets/blockout/`).
3. An AI image editor paints over each render in one locked comic-book style (prompts below).
4. `tools/fit.py` lines each painting up with its block-out (the painter can shift or resize it)
   and writes `assets/paint/paint.json`.
5. In the game (`index.html`) each painting is projected from the camera direction onto its
   block in the block's own space, so every copy of a building reuses one painting. The blocks
   give real depth (walking behind buildings), collision and shadows.
6. The Veil: lines stay ink, colour drains to grey paper; collecting sparks and lighting the beacon
   floods the colour back with an amber edge.

## Run and test
- `NODE_PATH=$(npm root -g) THREE_NM=<folder with node_modules/three@0.160.0> node tools/shoot.mjs <out>`
  takes phone and PC screenshots, plays to the win and prints console errors.
- URL options: `?autostart`, `?seed=N`. State in `window.__game`.

## Assets
| Object | Tool | Status |
|---|---|---|
| cottage, tower, stall | ElevenLabs gpt-image-2.5-sunburst, paint-over of the block-out | done |
| awning, well, pine, beacon | FLUX.1 Kontext [dev] through Hugging Face Inference Providers (fal), same prompt style; pine re-run at guidance 4 and its ground patch erased | done |

The pine, well, beacon and awning are drawn as cut-outs: leftover white background is see-through,
so their outline follows the painting rather than the plainer block.
| ground | procedural shader in `index.html` | done |
| characters | three.js shapes, toon shading | done |

Prompt pattern: "Paint over this 3D block-out render. Keep the EXACT same shape, silhouette,
proportions, size, position in the frame and camera angle ... Style: hand-drawn comic-book game
illustration ... bold dark brown-black ink outlines, clean flat colours with soft two-tone cel
shading, light hand-drawn texture lines and hatching, warm sunlight from the upper left.
Subject: <object>. Pure plain white background, no ground, no cast shadow, no text."
