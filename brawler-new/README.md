# New Challengers (working title)

A new game built on the Super Family Smash engine (`../brawler/`, which stays untouched): the same original 3D platform
fighter (damage %, knockback, ring-outs, stocks, ledges, shields, grabs, smash attacks, 4 specials per fighter, items,
Super Orb finales, up to 8 players or CPUs), with **all-new characters still to come**. The roster starts empty.

**Play:** open `index.html` in Chrome or Edge, or build the one-file version with `python3 tools/build_single.py`
(writes `NewChallengers.html`, ignored by git). Keyboard, Bluetooth/wired controllers and touch all work.

**Status**
- Roster: Brent, Justin, Raechel, Chris, Blaine, Sean. Roster tiles and player cards use their school portraits (portraits/). Title: "90's Version!" under the logo.
  Watch and the Locker all explain that a fighter is needed, and a match can't start without one.
- Hidden test dummy: `FIGHTERS[0]` (`id: 'dummy'`, `hidden: true`), an original robot mannequin made with Meshy
  text-to-3D + auto-rig (no real person), moving with the CC0 Universal Animation Library. Add `?dummy` to the address
  to show it in the menus for testing (e.g. `index.html?dummy`).
- Kept from the engine: stages (household themes), items, menus and luxury effects, controllers, announcer, generic
  sound effects, music, generic generated voice banks, and every tool in `tools/`.
- Removed: the family fighters and pets, their models, motion, voices, faces, banner, logo, intro and title videos,
  and the family story. The story mode is now a placeholder ladder built from whoever is in the roster.

**Adding a fighter:** follow `HANDOFF.md` (Meshy model → `models/<id>.glb` → `python3 build_models.py` →
`FIGHTERS` entry in `game.js` → `node tools/retarget_ual.js third_party/ual/ual1.glb third_party/ual/ual2.glb` →
their own moves from video with `tools/mocap_video.py` + `tools/retarget_video.js` → `OWN_MC` / `VICTORY_MC` in
`render3d.js` → voices, roster face (`fam/` + `tools/build_fam.py`), announcer call, finale (`def.finale`), unlocks).
Saves use their own `ncg_` keys, so this game never reads the family game's progress.

See `DESIGN.md` for the mechanics and engine log, and `HANDOFF.md` for the playbook.
