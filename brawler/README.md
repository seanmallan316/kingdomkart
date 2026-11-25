# Super Family Smash (platform-fighter prototype)

An original platform fighter that plays by the rules of the genre: damage %, knockback, ring-outs, stocks, ledges, shields,
dodges, grabs and throws, smash attacks you can charge, and 4 specials per fighter. Up to 4 players or CPUs (levels 1–9).
Every character, stage, picture and sound is original and made in code. This is the base for the upcoming droid game.

**Play:** open `index.html` in any browser (no build step). Keyboard, gamepads and touch all work.
See `DESIGN.md` for the full mechanics breakdown, the movement and knockback numbers, and the controls.

**Test:** `node` + Playwright. `window.__clash.startMatch({...})` and `window.__clash.step()` run matches headless.
