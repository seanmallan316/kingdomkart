# Kingdom Kart: Road to Glory

An original 3D kart racing game for the modded Wii U (Aroma homebrew), with Bible characters and a story that follows
Jesus from Bethlehem to the empty tomb — and, as a bonus, all the way Home.

## What is in the game right now
- **Eight characters** (Jesus, Moses, David, Esther, Peter, Paul, Mary, Daniel) with different speed, acceleration, handling
  and weight, plus two to unlock (Noah, Elijah). Four kart bodies (Standard, Chariot, Boat, Ark), three to unlock.
- **Four story roads**: Bethlehem Road (night, under the Star), Sea of Galilee (causeway over the water), Jerusalem Road
  (into the city), Sunrise at the Empty Tomb — and the unlockable **Road to Heaven**, one run from the earth up through the
  clouds to the gates.
- Drifting with mini-turbos, jumps, boost arrows, bounce pads, hidden shortcuts, tunnels with crystals, coins, obstacles
  crossing the road (sheep, nets, carts, rolling stones).
- **Eight power-ups**: Manna, Staff, Stone, Oil Jar, Shofar, Chariot of Fire, Locusts, Dove.
- Modes: Grand Prix (four roads, points, standings), Single Race, Time Trial (records saved), with Easy / Normal / Hard
  computer drivers that never cheat. Every race is under two minutes.
- Achievements that unlock the bonus characters, karts and road; progress saved on the SD card.
- Sound effects for everything (music is on hold).

## Controls (GamePad or Pro Controller)
Left stick / D-pad steer and move through menus · **A** gas / choose · **B** brake / back · **ZR** hold to drift
(let go for a turbo) · **ZL** (or X / Y) use the item · **+** pause · **HOME** quit.

## Install (Mac or PC, same steps)
1. Download `kingdomkart.wuhb` from GitHub (next section) and copy it into the `wiiu/apps` folder on the SD card.
2. One-time: download `glslcompiler.rpl` from https://github.com/Exzap/CafeGLSL/releases and copy it into `wiiu/libs`
   on the SD card (make the `libs` folder if it is not there).
3. Put the card in the Wii U and start **Kingdom Kart** from the Aroma menu.
4. If anything goes wrong, the file `wiiu/kingdomkart_log.txt` on the SD card says what happened. Progress lives in
   `wiiu/kingdomkart_save.txt`; delete it to start over.

### If the screen is a solid color
- **Orange**: `glslcompiler.rpl` is missing from `sd:/wiiu/libs/`.
- **Red**: the shader compiler was found but failed; send the log file.

## Getting the game file from GitHub
Open the repository's **Actions** tab. Every change builds itself ("Build Wii U game", a few minutes). Open the newest
green run and download **KingdomKart** at the bottom; unzip it to get `kingdomkart.wuhb`.

The same run also plays the game in a Wii U emulator and photographs every screen (the `ci-screenshots-road0` … `road4`
branches). That is how the game is checked without a console; it has not yet been run on a real Wii U.

## Adding pictures
Drop pictures into `assets/` (see `assets/README.txt` and the Art List for the exact file names), run
`python3 tools/prep_assets.py`, and commit the small files it writes into `content/img/`. The game uses a picture
whenever one exists and draws its own placeholder otherwise.

## Project layout
`src/sim` kart physics and the race (no graphics, runs in tests on a PC) · `src/world` roads and themes · `src/render`
GX2 drawing · `src/game` screens and the race view · `src/audio` sound synthesizer · `src/platform` Wii U entry point,
input, sound output · `tests/` host tests · `docs/` milestone checklists.
