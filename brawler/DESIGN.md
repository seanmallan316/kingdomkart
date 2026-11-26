# Super Family Smash — platform-fighter prototype (mechanics breakdown)

An original platform fighter built on the *rules* of the genre (percent damage, knockback, ring-outs, stocks).
No characters, art, music, stages or names from any existing game. This is the base that the droid game will
be re-themed from.

## Core loop
Hit people to raise their **damage %**. The higher the %, the farther they fly. Knock them past the edge of the screen
(the **blast zone**) to take one of their **stocks**. Last fighter with stocks left wins. A match is 2–4 fighters,
humans or CPUs (levels 1–9).

## Controls
| Action | Keyboard P1 | Keyboard P2 | Gamepad | Touch |
|---|---|---|---|---|
| Move / aim | A D W S | Arrows | Left stick / D-pad | Left stick |
| Jump | Space | Numpad 0 or / | X / Y | JUMP |
| Attack | J | Numpad 1 or , | A | ATK |
| Special | K | Numpad 2 or . | B | SPC |
| Smash attack (direction + this) | I | Numpad 5 or ; | Right stick | SMASH |
| Shield / dodge | L or Left Shift | Numpad 3 or Right Shift | LB / RB / LT | SHLD |
| Grab | U | Numpad 4 or ' | RT | GRAB |
| Overdrive (meter full) | Special + Shield together | same | same | OD button |
| Pause | Esc / P | | Start | II |

Keyboard: hold a direction to walk, and after a moment you break into a run (or double-tap to run at once).
Analog sticks: push hard to run, flick + Attack = smash attack.

## Movement numbers (60 fps; 1 fighter ≈ 80 px tall)
| | Light | Mid | Heavy | Floaty |  (archetypes from the first prototype) |
|---|---|---|---|---|
| Weight | 82 | 100 | 128 | 92 |
| Run px/f | 9.6 | 8.4 | 7.2 | 7.6 |
| Air speed | 5.0 | 4.6 | 4.0 | 5.2 |
| Gravity px/f² | 0.46 | 0.45 | 0.55 | 0.34 |
| Max fall / fast fall | 7.2 / 10.5 | 7 / 10 | 8.2 / 11.5 | 5.6 / 8.4 |
| Jumps (ground + air) | 1 + 2 | 1 + 1 | 1 + 1 | 1 + 1 |

Jumpsquat 3 frames; let go of Jump during it for a short hop. Fast fall = tap down while falling.
Landing lag: 3 f normal, aerials 6–16 f, after a recovery move 18 f.

## Combat rules
- **Knockback** = `((p/10 + p·d/20) · 200/(w+100) · 1.4 + 18) · growth/100 + base`, where p is % after the hit, d is damage, w is weight.
- Launch speed = knockback × 0.135 px/f, which shrinks by 0.23 px/f every frame. Gravity still pulls on the fighter. **Hitstun** = knockback × 0.4 frames; over 80 knockback = tumble.
- **Directional influence**: holding the stick at right angles to the launch bends it by up to 15°.
- **Hitstop** (freeze on impact): ⌊d·0.45 + 4⌋ frames, on both fighters; the one hit shakes.
- Angle "361" works like the genre's auto angle: flat along the ground for weak hits on the ground, 40° otherwise.
- Moves: 3-hit jab, side/up/down tilts, dash attack, chargeable side/up/down smashes (up to 1 s, +40% damage),
  5 aerials (neutral, forward, back, up, down; down spikes), grab → pummel and 4 throws, 4 specials per fighter.
- **Shield**: 50 HP, drains while held, grows back when let go; it shrinks on screen. If it breaks you are dizzy for 4 s.
  Raising it within 5 frames of a hit = **parry** (no shield damage, and the attacker freezes so you can punish).
- Dodges: roll and spot dodge on the ground, air dodge in the air (any direction, once until you land).
- **Ledges**: snap and hang with a short time you can't be hit (shorter if you grab again). From the ledge: climb, jump,
  attack, or roll onto the stage.
- **Tech**: press Shield just before hitting the ground in tumble to roll back up straight away, instead of lying down.
- Respawn platform, then 2 s where you can't be hurt.

## Original twist mechanics
1. **Overdrive meter**: fills when you deal damage (and half as fast when you take it). When it's full, press Special + Shield
   for 6 s of +30% damage, +12% knockback and +15% speed. You glow so everyone can see it.
2. **Brick's armour charge**: his side special doesn't flinch from hits under 13%.
3. **Nova's stored charge**: shield out of a charge shot to keep the charge, then fire it later.
4. **Driftway stage**: platforms that move and carry fighters on them.

## Camera and UI
The camera follows every fighter and zooms to fit them (0.45×–1.25×), smoothly. Bubbles at the screen edge show anyone
off-screen. A bottom card for each player shows damage % (white → yellow → red, and it shakes when hit), stocks and the
Overdrive meter. Screens: Title → Fighter select (P1–P4: Player / CPU / Off, CPU level), stage and stocks → 3-2-1-GO →
GAME! → results (place, KOs, falls, damage dealt).

## Fighter select (`buildSelect` in game.js)
Laid out like a classic platform-fighter select screen, in the game's own style:
- **Roster grid** on top: a face close-up tile for every family fighter (cut from the family banner, `fam/face_*.webp`),
  **10 locked tiles** reserved for unlockable fighters (`UNLOCKS` in game.js), and a
  RANDOM tile. Coloured P-tokens on each tile show which players picked it.
- **Player cards** below: one per active player (up to 8) with the fighter's photo, a P-badge in the player colour, the
  fighter's name, and a PLAYER n / CPU pill (tap to switch; −/+ sets CPU level); ✕ removes a player, "+ ADD FIGHTER" adds one.
  Tap a card to choose who you're picking for; the chosen card glows and says CHOOSING. A new pick slides in with a burst.
- With controllers, each controller picks for its own player. Stage, stocks, BACK and FIGHT! sit along the bottom.

## Unlockable fighters: the family pets (`pets.js`, `pets/`, `petjs/`)
3D models made with Meshy multi-image-to-3d from the family's own pictures. They have no skeleton, so `pets.js`
animates the whole body procedurally from the fighter's state (trot bob, pounce lunges, rearing up, flips, squash and
stretch). Hurtboxes are wider and lower (`hw`/`hh`). Voices are dog / cat / dragon sound effects.
- **River** (the shaggy dog): unlocked by winning 3 matches. Bark wave, pounce, zoomies. Finale: rush.
- **Rosie** (the tabby cat): unlocked by finishing Story mode. Three jumps, claw flurry, pounce. Finale: storm.
- **Pink Fluff** (Norah's spirit dragon): unlocked by landing Norah's Family Finale. Heavy, four wing-beat jumps.
- A "NEW CHALLENGER!" card, burst and confetti show when one unlocks. 7 locked slots remain for future fighters.

## Norah's Family Finale: Pink Fluff (`dragon.js`)
Norah shouts "Spirit animal, ACTIVATED!", then Pink Fluff (the same 3D model) flies in on a swooping path, snaps up
each opponent one by one (chomp sound, lunge), carries them off and hurls them away with a roar.

## Pink Fluff's flight (dragon.js)
After the reference flight footage: the scanned model has no skeleton, so a vertex shader bends it using weights baked
from the model's own shape (inner wing, outer wing, tail, neck). Wings: slow strong downstroke, quicker upstroke, the
outer wing lags so the tips trail; held out between beats in the approach glide, swept back and folded in a dive, beating
forward to brake in the flare. Tail: a wave travelling to the tip. Neck: lags in turns, lunges on a bite. Breathing.
Flight (items.js): long banking glide in from the distance -> wings-tucked dive at a foe -> flare and bite -> heavy power
climb and wide turn carrying the catch -> next foe -> climb out and fling. Turn rate is limited so every move is a curve,
and she banks in proportion to how hard she turns.

## Time limits
Fighter select has a TIME row (∞, 2, 3, 5 or 8 minutes; default 5). The clock sits top centre, turns red and pulses in
the last 10 seconds with a beep each second. At zero: "TIME!", ranked by stocks then lowest damage. Story rounds are 3 minutes.

## Story mode: a 12-round ladder
Beat each opponent and go straight on to the next (a VS card appears and the fight starts on its own after 5 seconds):
the other eight family members one by one (levels rising 2 to 7) including a 2-v-2 tag match, River, Rosie, a 3-on-1
Family Brawl, the Giant Showdown, and the final boss: Pink Fluff herself. Progress is saved per hero; lose and retry
the same round.

## Pet skeletons (pets.js)
River, Rosie and Pink Fluff come from photos and have no skeleton, so pets.js builds one in the vertex shader: weights
baked from each model's shape for four legs (hip / shoulder and knee pivots), spine, neck and head, tail and (dragon)
two-part wings. A gait system drives them like real animals: four-beat walk, diagonal trot, gallop / bound with a
flexing spine, crouch-and-spring jumps with legs reaching and tucking, rear-ups with paw swipes, stomps, buck kicks, a cat
one-paw swipe, a dog's wagging tail and a cat's slow swish, head look-around and breathing. The gait cycle advances with
ground speed so paws plant. Every joint eases toward its target (faster in attacks).

## Natural motion layer for the family (render3d.js `motionLayer`)
After the Unreal Game Animation Sample (motion matching): walk / run clips advance with distance covered (no foot
sliding), the body leans into acceleration and back when braking, a pivot step when reversing at speed, landings
compress on a spring, and the chest and head track the nearest opponent (not during attacks).

## Universal Animation Library (motion for the family)
The family now moves with the Quaternius **Universal Animation Library 1 + 2** (CC0 public domain, from
opengameart.org; ~90 hand-keyed game animations on one humanoid rig). `tools/retarget_ual.js` retargets the clips onto each
family member's own Meshy skeleton (the library is T-pose, the scans A-pose: each bone's rest direction is aligned before
its motion is transferred), samples them at 30 fps and writes `anims_ual.js` (`ual_*` clips, merged with the Meshy clips).
State mapping (render3d.js `pickClip`): idle, walk, jog / sprint by speed, jump start -> jump loop -> land, ninja-jump flip,
crouch, shield loop and shield hit, shield break, roll / dodge, hit (chest / head) and knockback, lying down and lay-to-idle
get-up, punch jab / cross / hook combos, slide for dash attacks, overhand throw, spell cast for projectiles, sword A/B/C
combos for Ollie, dancing on the victory screen. Kicks and some specials still use the Meshy clips.
Wind-ups play at their own pace to the hit frame, follow-through plays out naturally, and every change cross-fades.

## Announcer
A booming arena announcer (generated ElevenLabs voice, not a real person) calls "3! 2! 1! FIGHT!" over the countdown before every match, "GAME!" when a match ends and "TIME!" when the clock runs out (`Sfx.announce`, sfx/ann*.mp3).

## Clean skin weights (reskin.js)
Meshy's auto-rig left some clothing tied to the wrong bones, so fabric stuck or stretched as arms and legs moved.
When each family model loads, every vertex is assigned to one body part (core, left / right arm, left / right leg) by
Meshy's dominant weights; pulls from other parts are removed (except a little spine at the shoulders and hips at the
thighs); the remaining weights are mixed half-and-half with distance-to-bone weights; then smoothed twice across the
surface with UV seams welded, keeping the four strongest bones. `?rawskin` in the URL turns it off for comparison.

## Smoothness (frame rate)
- Adaptive resolution (render3d.js `perfTick`): starts at the screen's pixel ratio capped at 2 (1.5 on touch devices),
  drops by 15% steps (down to 60%) whenever frames average slower than ~45 fps and climbs back when there is room.
  Touch devices also get 2048 shadows, about half the grass, and a 1.5x HUD canvas.
- Every animation runs in real time between game ticks: clip times include the fraction to the next tick (`G.alpha`),
  the walk / run stride and the pets' gait, tail and joint easing use real elapsed time, and bone blending is frame-rate
  independent. (Before, a slow phone made characters both choppy and slower.)
- Options > Show FPS displays the frame rate and current render resolution.

## Music (music.js, music/*.mp3, tools/build_music.py)
Stage songs: `music/<theme>.mp3` plays during matches on that stage (treehouse.mp3 for the Treehouse, bedroom.mp3 for Bunk Beds), quieter when paused, cross-fading with the menu song.

### Menu music
The family's own song (made with Suno) loops on the title, main menu, fighter select, options, story and Locker
screens. It starts on the first tap / key, dips under the intro video, fades out for matches and back in on the menus,
and follows the Options volume.

## Locker: outfits, hats, back items and weapons (`locker.js`)
Every fighter has their own: 7 outfits (recolours / materials of the scan: Shadow, Frost, Chrome, Lava, Neon, Gold
Statue), 7 hats, 5 back items (cape, jetpack, angel wings, dragon wings) and 8 weapons, built in 3D and attached to the
model's bones. Bought with Stars (20 per match + 15 per KO + 60 for a win, 100 for a story win; 300 to start).
Weapons add 6% reach. LOCKER tile on the main menu; drag the preview to spin the fighter.

## Holding things
Held items and Locker weapons use a grip worked out from each model's rest pose: the handle sits in the palm and the item runs across the fist, pointing forward and slightly up (`gripCalc` in locker.js), sized to the fighter.

## Finale poses
Every Family Finale phase plays a real motion clip (celebrate, power pose, run, spin, punch flurry) instead of a frozen frame.

## Family banner on the opening screen (`fam.js`, `fam/`, `famjs.js`)
The family's banner photo (supplied by the family) is cut into nine panels (`fam/*.webp`, packed into `famjs.js` by
`tools/build_fam.py`) and shown as leaning photo cards along the bottom of the title screen, behind the logo and Play:
- cards rise in one by one, a light glint sweeps across each, and a spotlight ripples through them while idle;
- pointing at a card grows it, shows the person's name on a gold plate and dims the others; cards lean toward the pointer;
- choosing a card (click, tap or controller) bursts, plays that fighter's cheer or battle cry, and their 3D self on
  stage celebrates. The full controls list moved to Options to make room.

## Controllers and 8 players (`pads.js`)
- **Up to 8 fighters** (any mix of players and CPUs): 8 slots on the fighter select (off slots fold down), 8 player
  colours, and the match HUD wraps to two rows of four on narrower screens (`hudLayout` in game.js).
- **Any controller the browser sees, wired or Bluetooth** (Xbox, PlayStation, Switch Pro, 8BitDo, generic). Each human
  slot takes the next connected controller in order; the first two humans also get the keyboard maps and the first gets
  touch. Each human slot shows which controller it has.
- Non-standard pads (common over Bluetooth on some phones) are calibrated at rest so resting triggers don't read as a
  held stick, and hat-switch / axis d-pads are decoded.
- Connect / disconnect messages; a disconnect mid-match pauses the game. Rumble on hits and KOs where supported.
- **Menus with a controller:** stick or d-pad moves between options, A chooses (with the wobble / burst), B or
  Back/Select goes back, Start starts from the title and pauses in a match.
- Lighting pass: stronger sun and rim, less flat fill, soft contact shadows under every fighter, a gentle vignette;
  the camera's near plane scales with distance and stacked surfaces use polygon offset (fixes stripes on phone GPUs,
  e.g. the kitchen window), deeper shadow bias (fixes speckles on clothes), and AO is off on touch devices.

## Luxury menus (`lux.js` + the LUX block in index.html)
- **Every menu selection** wobbles or spins with a burst: hub tiles spin their icon 360° and wobble, gold buttons and
  fighter slots wobble (jelly squash), option chips flip-spin around their vertical axis. Each comes with a burst of
  gold sparks, stars, light rays, two shock rings and a flash (drawn on the `#luxfx` overlay canvas), a chime and a
  short vibration on phones. Works with mouse, touch and keyboard (Enter / Space).
- **Gold selector ring:** a rotating gold ring glides to whatever is hovered or focused.
- **Gold foil** buttons with a moving glint and a shine that follows the pointer; glass panels and chips with gold
  hairline edges; gold headings; holographic foil sheen on the hub tiles that follows the pointer.
- **Ambient:** drifting gold dust behind the menus, a sparkle trail behind the mouse cursor, and the 3D menu scene
  leans with the pointer (`window.MENU_PARALLAX`, used by `R3.menuRender`).
- **Screen changes** sweep a gold glint across the screen; the results screen rains confetti.
- Respects "reduce motion" (bursts, dust and trails switch off).

## Heights
Each family fighter's size (`def.scale` in game.js) follows their real height at 0.2 per foot: Layne 6'6" (1.30),
Tyler 5'10" (1.167), Dad 5'9" (1.15), Mom 5'8" (1.133), Jayson 5'8" (1.133), Violet 5.3' (1.06), Norah 4.5' (0.90),
Ray and Ollie 3.5' (0.70). Hurtboxes, reach and the 3D models all scale with it, so taller fighters are bigger targets.

## Game feel
Hitstop, camera shake scaled to knockback, hit sparks, smoke on landings and dashes, a smoke trail while flying, a white
flash on hits strong enough to KO, a blast burst on KO, and synth sound effects (Web Audio).

## Difficulty
CPU level 1–9 changes reaction time (20 → 4 frames), how often it shields or dodges, whether it picks smash attacks at KO
percents, and how well it recovers.

## Next: the droid game
Re-theme on top of this: droid fighters, an arena theme, new music and sound, droid-specific twists. Then touch-first
controls and an Android APK.

## 3D update
The game now draws in real 3D (three.js, bundled in `vendor/`): a lit stage with shadows and fighters built from shapes in
code, posed by the same animation angles as before. The fight rules did not change. `render3d.js` holds the scene.

Family fighters, modelled by hand from reference photos (no photo is stored in the repo or sent anywhere):
- **Ray**: small and floaty with five jumps. Specials: Clog Toss (arcing throw), Cannonball (rolling multi-hit dash),
  Hoodie Hop (short rising recovery), Belly Flop (ground slam, or a plunge from the air).
- **Ollie**: very fast with a foam sword and five jumps. Specials: Whirl (spinning multi-hit, hold Special to float),
  Dash Strike, Spiral (rising recovery), Counter.
- **Norah**: the big sister and the fastest runner, with pink and purple sparkle effects. Specials: Star Toss (a quick
  spinning star), Sparkle Dash (a very fast, long streak), Rocket (rising recovery), Dive Kick. She stands taller than the twins.

## Scanned family models
Ray, Ollie and Norah now use 3D models made from their photos with Meshy (image-to-3D plus auto-rigging), about 35 credits
each. The optimised files are in `models/*.glb` (about 1-1.5 MB each: 1024px WebP texture, quantized mesh).
`build_models.py` packs them into `models.js` so the game loads them from a web page, a local file or an APK alike.
`render3d.js` turns the sim's pose angles into bone directions on the Meshy skeleton, so every existing move works on the
scanned models. If a model is missing, that fighter falls back to the simple shapes.

## Meshy animations
Each family fighter has a set of motion clips from Meshy's animation library (idle, walk, run, jabs, kicks, uppercut, sweep,
smash, spin kick, jump, fall, hit, block, roll, knock-down, get-up, cast, throw, slam, grab, victory; Ollie also has sword
clips, Norah a sprint). `tools/extract_anims.py` keeps only the bone rotations (30 fps) and writes `anims.js`.
`render3d.js` picks the clip for each state or move, lines the clip's furthest-reach moment up with the move's hitbox
frame, and eases between clips. States with no clip (ledge hang, crouch, air dodge, Ray's cannonball) use the hand-posed
fallback. The move-to-clip table is `MOVE_CLIP` / `CHAR_CLIP` in `render3d.js`.

## Effects (`fx.js`)
Original effects and sound; nothing is sampled from another game.
- **Glow layer:** everything bright is drawn on its own canvas that blends as light over the 3D scene (`mix-blend-mode:
  screen`), built from soft sprites (glow, streak, comet, ring, wisp), then bloomed by shrinking and re-adding the image.
- **Ribbons:** smooth tapered curves (Catmull-Rom) follow the scanned models' real hands, feet and sword tip during
  attacks (`R3.limbPoints`), fighters sent flying, and projectiles.
- **Hits:** white bloom, coloured halo, lens streak, soft ring, comet petals, a cone of comets along the launch, wisps, twinkles.
- **Finisher:** long freeze, soft vignette, flowing speed lines, writhing energy arcs, camera punch-in.
- **KO:** a soft beam of light with swirling glows and expanding rings, plus confetti.
- **Sound:** layered synth impacts in three weights, finisher crack, whooshes, blade swishes, footsteps, shield, parry,
  shatter, charge whine, KO blast, countdown and GAME stabs, through a compressor and a small generated reverb.
- Options has a LIGHT effects setting that skips the bloom pass for slower phones.

## Menus and modes (`menu.js`)
- **Main menu:** Story, Versus, Training, Watch, Options. Arrow keys move between tiles.
- **Story, "The Golden Clog":** six chapters with a short card before each. Pick Ray, Norah or Ollie; the other two
  siblings appear as a rival and an ally. Chapter 5 is a 2-v-2 team fight, chapter 6 is the boss: the sixth sibling in the cast as a giant after a carton of Grow Juice (much bigger and
  heavier, 2 stocks). Progress per fighter is kept in the browser.
- **Teams:** fighters on the same team cannot hit each other and CPUs do not target team-mates; a match ends when one side is left.
- **Training:** a fighter who stands still, 99 stocks, no match end.

## The family roster (9 scanned fighters)
| Fighter | Role | Specials (neutral / side / up / down) |
|---|---|---|
| Ray | small, floaty, five jumps | Clog Toss / Cannonball / Hoodie Hop / Belly Flop |
| Norah | fastest runner, sparkle effects | Star Toss / Sparkle Dash / Rocket / Dive Kick |
| Ollie | fast foam sword, five jumps | Whirl / Dash Strike / Spiral / Counter |
| Tyler | strong all-rounder | Charge Punch / Dash Strike / Rocket / Counter |
| Violet | quick and graceful | Blaster / Dash Strike / Teleport / Counter |
| Layne | heavy bruiser | Charge Punch / Armoured Charge / Rocket / Quake |
| Dad | the powerhouse, heaviest | Shockwave / Armoured Charge / Rocket / Quake |
| Mom | sharp and technical | Charge Shot / Bomb / Rocket / Counter |
| Jayson | fast, agile brawler | Blaster / Cannonball / Rocket / Dive Kick |

Tyler and Jayson were built from their front photo only, because their photos mixed outfits; the brand logo on Jayson's
T-shirt was removed first. The original placeholder fighters (Vex, Brick, Nova, Kai) were removed; the roster is the nine family members plus 10 locked unlockable slots. Any family member can be the story hero.

## Recorded-quality sound (`sfx/`, `sfx.js`)
25 sound effects generated to order with ElevenLabs text-to-sound (about 17 credits each), so they are original to this
game: three weights of hit, finisher, two swings, blade, jump, double jump, land, footstep, dash, shield on, shield hit,
parry, shield break, energy shot, charge, KO, grab, countdown, GO, GAME, menu click, Overdrive.
Each was trimmed of leading silence, cut to length, faded out and levelled to the same loudness with ffmpeg, then saved
as an 80 kbps mono MP3. `tools/build_sfx.py` packs them into `sfx.js`. `fx.js` plays the sample with a little random
pitch and falls back to the old synthesized sound if a sample has not decoded. The per-sound volumes are in `fx.js`.
The camera now frames the fighters closely (zoom up to 2.3x) and the HUD shrinks on short screens.

## Character voices (`voice/`)
108 voice clips generated with ElevenLabs text-to-sound (v3 model): six generic voice types (boy, girl, teen boy, young
woman, man, woman) x six kinds (light effort, big battle cry, hurt, jump, KO yell, victory cheer) x three takes each.
The prompts ask for expressive, playful reactions (a laugh after getting hit, a cartoon yell fading away on a KO, a
cheer on a win). They are generated voices, not recordings or clones of the family. Each clip was trimmed to its sound,
faded and levelled, and saved as `voice/<type>_<kind>_<take>.mp3`.

`Sfx.voice` in `fx.js` picks a random take but never the one that fighter just used for that kind, and varies pitch and
volume a little. `VOICES` maps each fighter to a type and a pitch, so fighters who share a type sound different.
To stop chatter, each fighter speaks at most once every ~1.1 s (0.65 s for hurt). Triggers: battle cry on most smash
attacks, specials and throws; light effort on 10-30% of other attacks; hurt on 60% of real hits; jump on 18% of jumps;
KO yell on every ring-out; the winner cheers on the results screen.

## Worlds (`worlds.js`, `props/`)
Each family stage has a full 3D world built by `WORLDS.build` (called from `buildStage` in `render3d.js`): procedural
canvas textures with bump maps, rounded-edge geometry, a sky dome, fog, per-world lighting, and 13 Meshy-generated props
(`props/*.glb`, packed as base64 in `propjs/`). Worlds react to the fight: props wobble when hit or landed near, pillows
squash, cones get knocked flying, the basketball rolls, birds scatter, fairy lights flash on big hits.
- **Treehouse:** plank deck high in an oak, railing, rope ladder, hanging-swing platforms, birdhouse, falling leaves.
- **Kitchen Table:** gingham tablecloth, breakfast props, teal cabinets, glowing window and sunbeam, pendant lamps, popping toast.
- **Driveway:** chalk-drawn asphalt at golden hour, the house and garage behind, bike, hoop, cones, ball, fireflies.
- **Bunk Beds:** quilted mattress, pillow platforms, bunk frame, teddy and blocks, moonlit window, fairy lights.

### Real geometry, not painted boxes
- Wooden floors are separate planks (an `InstancedMesh`) with gaps, joists underneath and a shade per plank.
- Trees have furrowed, root-flared trunks (`trunkGeo`), curving tapered limbs (`branchGeo`, a tube along a spline)
  and leafy canopies (`canopy`): a soft core plus hundreds of leaf cards, darker inside and underneath, swaying in the wind.
- Hedges are leaf canopies too; lawns have thousands of grass tufts (`grassField`) that move in the wind.
- The bed has a tufted quilt top (`tuftedTop`) and plump pillow platforms (`pillowGeo`).
- All rounded shapes use smooth normals (the old faceted look came from flat per-face normals).
- Lighting: an image-based environment (`RoomEnvironment`) on top of the sun, sky and rim lights, plus screen-space
  ambient occlusion (`GTAOPass`) for soft contact shading. AO switches itself off if the device can't keep up
  (add `?noao` to the URL to force it off).
- The match HUD fits four players on a phone held upright.

## Items, the Super Orb and Family Finales (`items.js`)
`items.js` wraps a few game.js functions (step, startMatch, groundActions, airActions, applyHit, ko, canBeHit, aiPoll,
updateCam, render) and draws its models through `R3.addHook`. Set `cfg.items = false` to turn items off.
- **Item boxes** drop from the sky every 8-13 s (at most 4 loose items): a wooden **crate** (hit it to break it, 1-2
  items) or a **capsule** (break it, or pick it up and throw it; it pops open on impact).
- **Controls:** Attack next to an item picks it up; **Grab** next to a crate or capsule lifts it over your head, then Attack or Grab throws it (it bursts open where it lands or on whoever it hits). With a weapon, Attack uses it; Grab throws it
  (hold up to throw up, down to drop). Unclaimed items blink and vanish after ~18 s.
- **Weapons:** Slugger Bat (swing, a chargeable home-run smash, and an air swing), Foam Blaster (16 darts),
  Water Balloon (throw it; it bursts in a splash). **Power-ups:** Mega Mallet (8 s of auto-swinging, can only move and
  jump), Star Candy (8 s invincible), Grow Juice (10 s giant: bigger, hits 35% harder, harder to launch),
  Zoom Sneakers (10 s faster), Heart Cookie (heals 45%).
- **Super Orb:** a rainbow orb floats around the stage every 25-45 s; it takes ~30 damage to break. Whoever breaks it
  glows and can press **Special** for their **Family Finale**: a close-up cinematic (letterbox, speed lines, name card),
  then one of three attacks: **beam** (Dad, Violet, Jayson: a screen-wide energy beam), **rush** (Norah, Tyler, Layne:
  a dash that catches foes, then a close-up barrage) or **storm** (Ray, Ollie, Mom: a vortex that pulls foes in and
  blasts them out). The finale fighter can't be hit while it plays. CPUs chase the orb, grab items and use finales.
- New sounds (ElevenLabs): crate break, pickup chime, orb shatter, finale charge, energy beam, home-run crack.

## Pulse attacks (`items.js`)
Hold **Attack**, **Special** or **Smash** for about half a second (a ring fills around the fighter) to release a
shockwave. Each button has its own pulse and its own 10-second recharge, shown as three small dials (A, B, S) above
each player's card:
- **Power Pulse** (Attack): close shockwave, sends foes out to the sides.
- **Energy Pulse** (Special): the widest ring, launches foes upward.
- **Smash Pulse** (Smash): strongest, big knockback.
Pulses also break item boxes and hit the Super Orb. CPUs use them now and then when someone is close.

## Game feel
- **Game speed** (Options): Slow 0.65x, Normal 0.8x (default), Fast 1.0x. The simulation steps at that rate and fighters
  are drawn between steps (`G.alpha`, `f.px/f.py`), so it stays smooth on any screen refresh rate.
- **Motion:** every state now plays a real motion clip (jump squat, landing, crouch, flips, air dodge, ledge,
  specials without their own clip use cast / air punch / high kick / slam), never the old hand-posed angles.
  New clips fade in over several frames; attacks fade in faster so hits still read.
- Fighters are 1.9x their base size; the camera frames them tightly (zoom up to 3.2x).
- The ambient-occlusion pass skips sprites, leaf and grass cards and other see-through things (they showed as squares).

## One-file build
`python3 tools/build_single.py` writes `SuperFamilySmash.html`: every script, the logo and the intro video inlined into
one page (about 42 MB) that runs by double-clicking it. It is ignored by git; rebuild it after changes.

## Live menus
The menus sit over a live 3D scene (`R3.menuSet` / `menuRender` in `render3d.js`, driven by `setScene` in `menu.js`):
- **Title:** the whole family lined up on stage, fidgeting; tap the background and one of them reacts and calls out.
- **Main menu:** tiles lean toward the pointer and light up; the fighters beside them react to the tile you are on.
- **Fighter select:** the chosen fighters stand on stage; changing one makes them step up and call out. Cards show
  speed, power, weight and air-jump bars.
- **Story:** your hero faces the chapter's opponents. **Results:** the winner celebrates.
A short-screen layout (under 520 px tall) keeps everything on screen on a phone held sideways.

## Arena menu look
- **Logo:** an original Super Family Smash logo: stacked italic lettering in polished chrome with a heavy dark outline
  (CSS), animated flames and sparks rising behind it (a small canvas driven by `flames` in `menu.js`) and a spinning
  star. It deliberately does not imitate any existing game's logo.
- **Tiles:** glass-style tiles with an inner highlight and a coloured glow. Story shows the current hero, Versus shows
  silhouettes of two family fighters; both are drawn from the real 3D models (`tileArt` in `menu.js`).
- **Arena:** a ring with corner posts and glowing ropes at the back and sides (`render3d.js`), plus stadium light banks,
  sweeping coloured beams and drifting sparkles drawn over the scene (`drawMenuBg` in `menu.js`).
- **Portrait phones:** logo, then tiles in two columns, with five of the family in the ring along the bottom; the title
  screen shows all nine in two rows.

## Supplied logo and start animation
- `logo.webp`: the logo image supplied by the owner, with the green screen removed and nothing else changed. Shown on the
  title screen and main menu. (It replaces the earlier CSS-built logo.)
- `intro.webm`: the supplied start animation (logo zooms in and bursts), converted to a transparent VP9 video with its
  sound. Pressing Play runs it over the title screen; the main menu appears at the burst (2.95 s). If the video cannot
  play, a built-in zoom-and-flame transition runs instead. Reduced-motion settings skip both.

## PC build
The game now targets PC first (it still runs in a phone browser, but quality is no longer held back for phones).
- Fighter textures are the scans' full 2048 px (was 1024), with anisotropic filtering.
- Renderer: filmic (ACES) tone mapping, 4096 px soft shadows, pixel ratio up to 2.5.
- Fullscreen: F on the menus, F11, or Options.
- Models are packed one file per fighter in `modeljs/` (built by `build_models.py`); `models.js` is gone.
- To play on a PC: unzip the download and open `index.html` in Chrome or Edge. No install or internet needed.

## Family worlds (stages)
Four original stages, built from simple 3D shapes in `DECOR` (`render3d.js`). The layout types are genre staples; the
layouts, names and art are this game's own, not copies of any existing game's stages.
| Stage | Layout | Setting |
|---|---|---|
| Treehouse (`skyforge`) | three platforms | a deck in a big tree, clouds drifting past |
| Kitchen Table (`driftway`) | two moving platforms | a giant table top with mugs, a fruit bowl and a window |
| Driveway (`proving`) | flat | garage door, basketball hoop and hedges at sunset |
| Bunk Beds (`bunk`) | staircase of three platforms | a bedroom at night with fairy lights and a moon |

Fighters are 1.35x larger relative to the stages (`FIGHTER_SIZE` in `game.js`) and the camera frames them more tightly.
The start video now begins fitted to the screen and grows to fill it, so it is not cropped on an upright phone.

## Announcer in the menus, landscape, opening song
- `announce.js`: the announcer reads out whatever is highlighted in any menu (hover, tap, keyboard or controller focus):
  hub tiles, every fighter, stages, stocks, time limits, options, story buttons, Locker tabs and items. Clips live in
  `sfx/say/say_<WORDS>.mp3` (packed into sfx.js), are cut from batched announcer takes and labelled by speech recognition,
  and `Sfx.say()` cuts the previous line off so fast scrolling never stacks. Options > Menu announcer turns it off.
- Countdown fix: the "2", "1" and "FIGHT!" clips had been cut to silence; they are now re-cut from the original take.
- Landscape: on phones/tablets, PLAY / FIGHT goes full screen and locks the screen sideways where the browser allows
  (Android); otherwise a "turn your phone sideways" card shows while upright. Options > Screen: Landscape / Auto.
- `music/title.mp3`: the opening song on the title screen, cross-fading to the menu song after.

- `titlebg.mp4`: the animated neon-city background behind the title screen (video only, no audio), cut into a seamless
  13.5 s loop by crossfading its end into its start; it only plays while the title is on screen.

## Ray's moves, motion-captured from video (all fighters)
- `tools/mocap_video.py`: MediaPipe Pose (heavy) tracks the body in every frame of the family's reference video
  (`mocap/ray_kata.json`: 33 joints in 3D + image positions, 360/361 frames tracked).
- `tools/retarget_video.js`: joints -> game axes (forward = the striking direction), Gaussian de-jitter, hips/chest
  solved as full orientations, spine blended, head from the ears, limbs solved down each chain by shortest rotation
  (no twist build-up), body height from the image. Cut into 14 moves by `mocap/ray_kata_segments.json` and written
  for all nine family skeletons to `anims_mc.js` (`mc_*` clips).
- `render3d.js` maps them: jabs (lunge punch, cross, guard punch), ftilt side kick, utilt/usmash vertical high kick,
  dtilt low strike, fsmash deep lunge strike, dsmash palm sweep, nair/bair cartwheel, fair flying kick, uair jump
  kick, dair axe kick, specials and pulse attacks palm strike. Sword fighters keep their sword swings.
- `trails.js`: glowing ribbon trails behind fast hands and feet during any attack (camera-facing, Catmull-Rom smoothed,
  white core, fighter's colour), as in the video. Off with Options > Glow effects: Light.
- Landscape phones: player cards use the face close-up, flatter roster tiles, hub in a 3 x 2 grid, cards scroll.
- Picking a fighter always calls out the name.
