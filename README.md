# Kingdom Kart: Road to Glory

An original 3D kart racing game for the modded Wii U (Aroma homebrew), with Bible characters and a story that follows
Jesus from Bethlehem to the empty tomb — and, as a bonus, all the way Home.

## What is in the game right now
- **Seven characters to begin with** (Jesus, Moses, David, Esther, Peter, Mary, Daniel) with different speed,
  acceleration, handling and weight, and **seven to unlock** (Samson, Noah, Joseph, Jonah, John the Baptist, and the
  archangels Michael and Gabriel). Five kart bodies (Standard, Chariot, Boat, Ark, Sling), four to unlock.
- **Four story roads**: Bethlehem Road (night, under the Star), Sea of Galilee (causeway over the water), Jerusalem Road
  (into the city), Sunrise at the Empty Tomb — and the unlockable **Road to Heaven**, one run from the earth up through the
  clouds to the gates.
- Drifting with mini-turbos, rocket starts, tricks off jumps, boost arrows, bounce pads, hidden shortcuts, tunnels with
  crystals, coins (each one adds a little top speed, up to ten), obstacles crossing the road (sheep, nets, carts, rolling
  stones), GamePad rumble, slow-motion finish.
- **Open land beside the roads**: drive off the road onto the grass (or wade the shallows on the Sea of Galilee) out to a
  low wall, with hidden gems (three coins and a burst of speed), boost arrows and bounce ramps out there. The Road to
  Heaven keeps its edges.
- **Eight blessings instead of weapons** (every item does something for you *and* for another racer): Manna (Exodus 16),
  Loaves and Fishes (Matthew 14), the Good Samaritan (Luke 10), the Shield of Faith (Ephesians 6), the Parting of the Sea
  (Exodus 14), the Jubilee Trumpet (Leviticus 25), the Chariot of Fire (2 Kings 2), Living Water (John 4). Bumping another
  kart gives *them* a boost (turn the other cheek). Boxes float, glow and burst into a rainbow when taken; a window then
  spins through the blessings and stops on the one you got, with its picture, name and verse.
- Modes: Grand Prix with two cups (the **Gospel Cup**, four story roads; **Genesis to Glory**, all six roads in Bible
  order, earned with the Road to Heaven), Single Race, Time Trial (records saved), **Multiplayer for 2 to 4 people on a
  split screen** (computer drivers fill the other seats), **The Walls of Jericho** (seven laps round the city, shofars,
  the great shout, and every wall falls flat) and the David vs Goliath fight in the open Valley of Elah (five smooth
  stones in the brook, ZL slings), with Easy / Normal / Hard computer drivers that never cheat. A race takes about two and
  a quarter minutes.
- Achievements that unlock the bonus characters, karts and road; progress, records and volume settings saved on the SD card.
- Options and records screen, credits screen.
- Sound effects for everything, and a soundtrack: a track per road, the menus, the results screen, the final lap and
  the Goliath fight (MP3 files streamed from the game file; see Music below).

## Controls (GamePad, Pro Controller or Classic Controller)
Left stick / D-pad steer and move through menus · **A** gas / choose · **B** brake / back · **ZR** hold to drift
(let go for a turbo) · **ZL** (or X / Y) use the blessing · **+** pause · **HOME** quit.

**Wii Remote held sideways:** D-pad steer · **2** gas · **1** brake · **B** (trigger) drift · **A** blessing · **+** pause.
**Wii Remote + Nunchuk:** stick steer · **A** gas · **B** brake · **Z** drift · **C** blessing.

## Multiplayer (2 to 4 people)
Main menu → **MULTIPLAYER 2-4**. Everyone presses **A** on their own controller to join (the GamePad and up to four Wii
controllers: Pro Controller, Classic Controller, Wii Remote sideways, or Remote + Nunchuk), then **+** starts. Everyone
picks a character at the same time (A locks in, B changes), player 1 picks the road, and the TV splits: two players top
and bottom, three or four in quarters (with three, the fourth quarter shows the map and standings). The GamePad screen
shows the big map. Computer drivers fill the empty seats so there are always eight karts. Multiplayer races do not count
toward records or unlocks.

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
green run and download **KingdomKart-SD-card** at the bottom: unzip it and copy the `wiiu` folder inside onto the SD
card (merge it with the one already there). It holds the game and the shader compiler, each in its folder.
**KingdomKart** is the game file on its own.

The same run also plays the game in a Wii U emulator and photographs every screen (the `ci-screenshots-road0` … `road12`
branches). That is how the game is checked without a console; it also runs on Sean's Wii U.

## Adding pictures
Drop pictures into `assets/` (see `assets/README.txt` and the Art List for the exact file names), run
`python3 tools/prep_assets.py`, and commit the small files it writes into `content/img/`. The game uses a picture
whenever one exists and draws its own placeholder otherwise.

## Music
Soundtrack files live in `assets/music/` named by what they are for (`music_menu`, `music_select`, `music_beth`, `music_gal`,
`music_jeru`, `music_tomb`, `music_heaven`, `music_eden`, `music_jericho`, `music_elah`, `music_results`, `music_lose`, `music_trophy`, `music_finallap`,
`music_credits`), in any audio format.
`python3 tools/prep_music.py` trims silence, evens the loudness and writes 32 kHz MP3s into `content/music/`, which
the game streams and loops (with a crossfade from the end back to the start). A road without its own file borrows
another road's track; a missing file is simply silent. The MP3 decoder is minimp3 (public domain, `src/third_party`).

## Sound effects
Recorded sounds go in `assets/sfx/` and `python3 tools/prep_sfx.py` writes them to `content/sfx/` (32 kHz WAV). The kart
engine uses `kart_engine_idle`, `kart_engine_acceleration`, `kart_engine_top_speed` and `kart_engine_deceleration`
(loops blended by speed and pitched up with it) plus `kart_engine_start`, `kart_gear_shift`, `kart_exhaust_pop`,
`kart_braking` and `kart_hard_braking`. `sting_countdown` plays over the 3-2-1-GO and `sting_unlock` when something is
unlocked. Any synthesized effect can be replaced by a recording named `sfx_<effect>.wav` (`sfx_coin`, `sfx_lap`, ...).
Without the files, the game synthesizes everything itself.

**Driver voices:** Sean's own recordings in `assets/voices/` (`male_start.wav` "let's do this", `female_win.wav`, ...) are
used as they are, never pitched: `python3 tools/prep_voices.py` writes `content/sfx/voice_<event>.wav`, shared by every
character without lines of their own. Events: `start` (at GO), `boost`, `trick`, `win`, `bless`, `fire`, `ouch`.
Spoken lines made with ElevenLabs (one line per file, `assets/voices/lines/<name>_<event>.mp3`, each character in a voice
of their own; the script is in `assets/voices/lines/LINES.txt`) go through `python3 tools/prep_lines.py`.

## Project layout
`src/sim` kart physics and the race (no graphics, runs in tests on a PC) · `src/world` roads and themes · `src/render`
GX2 drawing · `src/game` screens and the race view · `src/audio` sound synthesizer and music player · `src/platform` Wii U entry point,
input, sound output · `tests/` host tests · `docs/` milestone checklists.
