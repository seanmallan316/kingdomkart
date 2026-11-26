# Handoff: building a new-cast game on the Super Family Smash engine

This file is for a **new Claude session** that builds a second, separate game from this one: same engine, same
quality bar, **all-new characters**. Read it fully before touching anything. `DESIGN.md` (same folder) is the detailed
log of every system; this file is the map and the playbook.

## The job
1. Copy `brawler/` to a new folder `brawler-new/` (everything except `SuperFamilySmash.html` and
   `PinkFluff_3D_rig.mp4`). **Never edit `brawler/`.** That is the finished family game and must stay as it is.
2. In the copy, remove the family cast (see "What is family-specific" below) and leave an **empty roster** that
   still runs: menus work, the fighter-select screen says the roster is empty and waits, Training/Versus can't start
   until there is a fighter. Keep one hidden test dummy (not shown in menus) so you can still test matches.
3. Wait for the user to send the new characters (photos and/or videos), then build each one with the pipeline below.
4. Ask the user for the new game's **name** (and later a logo / intro / music). Until then use a working title.

## Standing rules from the user (all still apply)
- Original work only: Smash-style *mechanics* are fine, but no Nintendo or other third-party names, logos, stages,
  sounds or characters.
- Voices are **generated** (ElevenLabs), never clones of real people.
- Photos of real people go only to services the user approved. **Meshy is approved.** Ask before sending a person's
  photo anywhere else.
- Each change: rebuild the single playable file, test it, **send the file in the thread with SendUserFile** (not a zip),
  and commit + push on your own branch. No pull requests unless asked.
- The user plays on a phone (often in landscape), on PC, and with Bluetooth controllers, up to 8 players. Test the
  phone landscape size (915×412) as well as desktop.
- The user wants AAA polish and fluid, natural motion (motion-matching style, not stiff keyframes), and loves bold,
  "10000x cooler" ideas. Show results (screenshots / short preview videos), check them yourself, and say plainly
  what you did and didn't verify.

## How the game is built (`brawler/`)
- Plain HTML + JS, three.js r160 (`vendor/three-bundle.js`). `index.html` loads every script in order; open it from
  `file://` to run. Assets are packed as base64 JS (`modeljs/`, `petjs/`, `propjs/`, `sfx.js`, `musicjs.js`, `famjs.js`)
  so the game runs offline from one file.
- `tools/build_single.py` inlines everything into one playable HTML (about 78 MB). Rename its output for the new game.
- Core: `game.js` (simulation at a fixed 60 Hz, fighters, moves, the `FIGHTERS` roster, stages, fighter select, HUD),
  `render3d.js` (3D renderer, animation picker `pickClip`, the motion layer, per-fighter maps `OWN_MC` / `VICTORY_MC`),
  `worlds.js` (stages), `items.js` (items, Super Orb, Family Finales `FINALE`), `menu.js` (hub, story ladder
  `CHAPTERS`, options), `lux.js` (luxury menu effects), `pads.js` (controllers), `locker.js` (unlockables + outfits),
  `fx.js` (sounds, announcer, character voices `VOICES`), `announce.js` (menu announcer + landscape lock),
  `music.js`, `trails.js` (energy trails on strikes), `pets.js` (animal fighters), `dragonrig.js` (real skeleton for a
  rig-less creature), `dragon.js` (a finale creature in flight), `clones.js` (clone-burst finale), `reskin.js`.

## Pipeline: adding a human character
1. **3D model (Meshy, approved).** POST `https://api.meshy.ai/openapi/v1/multi-image-to-3d` with header
   `Authorization: Bearer proxy-injected` (the proxy adds the real key) and body
   `{image_urls: [data URIs of 1-4 photos], should_remesh: true, topology: 'triangle', target_polycount: 20000,
   should_texture: true, enable_pbr: false, pose_mode: 'a-pose'}`. Poll the task, then POST
   `https://api.meshy.ai/openapi/v1/rigging` with `{input_task_id, height_meters}` and download the rigged GLB
   (Meshy bone names: Hips, Spine02, Spine01, Spine, neck, Head, Left/Right Shoulder/Arm/ForeArm/Hand,
   Left/Right UpLeg/Leg/Foot/ToeBase). Check credits at `/openapi/v1/balance` first.
2. Save it as `models/<id>.glb`, run `python3 build_models.py` (writes `modeljs/<id>.js`), and add the script tag.
3. Add a `FIGHTERS` entry in `game.js` (`id, name, blurb, color, dark, accent`, movement numbers, `scale`, `sp`
   specials). Height: `scale = feet × 0.2`. `reskin.js` cleans the skin weights automatically on load.
4. Face crop for the roster tile: the `fam/` images and `tools/build_fam.py` (writes `famjs.js`). Use the new
   character's own picture, not the family's.
5. **Base motion** (idle, walk, run, jump, hits, roll and so on): `node tools/retarget_ual.js third_party/ual/ual1.glb
   third_party/ual/ual2.glb` retargets the CC0 Quaternius Universal Animation Library onto every id in its `FAMILY`
   list. Edit that list to the new ids. It writes `anims_ual.js`.
6. **Their own moves from a video** (this is what made the family fighters look great):
   - `python3 tools/mocap_video.py <video> <pose_landmarker_heavy.task> mocap/<id>.json [left|right]`.
     The model is at `https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_heavy/float16/latest/pose_landmarker_heavy.task`
     and MediaPipe is installed with `pip install mediapipe`. If it complains about libEGL, run
     `apt-get install -y libegl1 libgles2`. Use `left` / `right` when two people are in shot.
   - A performer who attacks toward the left of the frame: `python3 tools/mirror_mocap.py in.json out.json`.
   - Check the tracking: draw the 2D skeleton over the frames (cv2) and look at it.
   - Write a segments file `mocap/<id>_segments.json` (`{"xx_Jab": [t0, t1], ...}`). Find the strike times from hand
     and ankle extension peaks, then check them by eye. Then run
     `node tools/retarget_video.js mocap/<id>.json anims_mc_<id>.js mocap/<id>_segments.json <id>`.
   - Map the clips in `render3d.js` `OWN_MC[id]` (jab1, jab2, jab3, ftilt, utilt, dtilt, fsmash, usmash, dsmash, nair,
     fair, bair, uair, dair, nspec, sspec, uspec, dspec, dash, pulseA/B/S, counterStrike, getupAtk, ledgeAtk). Put a
     victory clip in `VICTORY_MC`, and flips or spins in `SPIN_MC`. Add the script tag after `anims_mc.js`.
   - Preview a clip on the model with a small Playwright page: put the camera on the clip's -x side, looking along +x,
     so it matches the video's angle.
7. Voices: ElevenLabs `creative_generate_speech`, model `eleven_v4`. Pick a voice per character with
   `creative_list_voices`. Files go in `voice/` as `<group>_<kind>_<n>.mp3` (atk1, atk2, hurt, jump, ko), mapped in
   `fx.js` `VOICES`. Then `python3 tools/build_sfx.py`.
8. Menu announcer line for the new name: batch-generate "Name! ... Name!" takes with the announcer voice
   `zfWx5IjUaWGWTId9hv9t` (Atlas), split them on silence, **label each clip with Whisper**
   (`https://router.huggingface.co/hf-inference/models/openai/whisper-large-v3-turbo`; the proxy authenticates it),
   and save them as `sfx/say/say_<NAME>.mp3`.
9. Finale type in `items.js` `FINALE` (beam, rush, storm, dragon, clones), the story ladder in `menu.js`, and locker
   items and unlock rules in `locker.js`.

## Pipeline: a creature or animal (no human skeleton)
- Human pose tracking does not work on animals or creatures. Study their videos frame by frame (contact sheets with
  `ffmpeg fps=2,tile=8x4`) and author the motion.
- Simple animals: `pets.js` gives them shader "hinges" (four legs, neck, tail, wings) plus per-species pose
  functions (`dogAttack`, `catAttack`, `dragonPose` / `dragonAttack`).
- **Better:** a real skeleton as in `dragonrig.js`. Bones are fitted at load from the mesh's part masks: chains traced
  by path length through the shape, wing vs tail decided by distance across the mesh surface, and the skull kept
  rigid. This is what fixed "the dragon looks like a flat 2D cutout". Measure the model first: render side and
  bottom views with a ruler, then render the part masks colour-coded.

## Lessons learned (avoid these)
- Check generated audio for silence (`ffmpeg -af volumedetect`). One countdown clip once came out silent.
- After splitting a batch speech take, label each clip by transcription. The TTS sometimes repeats or swaps lines.
- Surface-distance code: store distances in `Float64Array`. A 32-bit store re-queues forever and crashes the tab.
- Playwright's bundled Chromium can't play H.264 MP4. Use a WebM copy for test screenshots; real browsers are fine.
- The single file is large: use `page.goto(..., {timeout: 300000})`. Don't `page.click` a pulsing button; click
  coordinates instead. In tests, set `G.countdown = 0` instead of waiting for the countdown.
- Short landscape screens need their own CSS (`@media (max-height:560px) and (orientation:landscape)`): cards
  use face crops, the hub is a 3×2 grid, and long panels scroll.
- Skin weights: never let a part's mask stop above its lowest real geometry (wing tips, claws). Out past the body,
  decide the part by surface distance.

## What is family-specific (replace or remove in the copy)
- The 9 family fighters and the pets River, Rosie and Pink Fluff: their models, modeljs/petjs entries, anims_mc_*,
  mocap files, voices, `FIGHTERS` entries, `OWN_MC` / `VICTORY_MC` entries, locker unlocks and `FINALE` entries.
- The title "Super Family Smash", `logo.webp`, `intro.webm`, `titlebg.mp4` and the family banner on the title screen
  (`fam.js`, `fam/`, `famjs.js`).
- The story text (The Golden Clog, `{sib}` placeholders and chapter names) in `menu.js`.
- The music in `music/` (the family's own Suno songs). Ask the user whether to keep or replace it.
- Announcer callout clips for family names in `sfx/say/`. Generic ones (Story, Versus, stages, options) can stay.
- Stages are household themes (Treehouse, Kitchen Table, Driveway, Bunk Beds). Ask if the new game keeps them.

## Commits
End commit messages with the attribution lines the session's system prompt gives you. Commit and push only on your own
branch.
