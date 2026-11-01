# Milestone 8 (+8b): polish, sound, course features, the Goliath battle

Checked = host tests with the address sanitizer and Cemu emulator screenshots from the CI tour builds (one per road
plus the battle). NOT yet run on a real Wii U; sound has only been checked as a rendered file (tests/sfx_test.c writes
sfx_preview.wav) and for not crashing in the emulator.

| Item | Status |
|---|---|
| Sound effects: menus, countdown, items, hits, laps, finish, turbos, pads, bounce, splash, unlock, welcome home, coins; engine, tires, wind and boost that follow the player's kart | done (synthesized, AX output on two voices) |
| Music | parked at Sean's request |
| Round wheels, hood, exhaust on the karts; four bodies plus the Sling cart | done |
| Drift sparks (blue / orange / purple by charge), boost flames, coins spinning, waterfalls that flow | done |
| Sean's characters drive the karts (standing cut-outs that lean into turns) and fill the cards (head crops) | done |
| Coins: +1% top speed each up to 10, three lost when hit; computer drivers go for them too | done |
| Tunnels with purple crystals, start arches, item boxes in rows of six | done |
| Road cards from Sean's course sheets; title art hook (title_art.png) | done (title art waiting on the picture) |
| David vs Goliath: arena, stalking / charging / spear-throwing giant, stones to pick up and sling, 5 hits to win, 3 lives, Easy/Normal/Hard | done |
| Battle unlock (win a race as David), Sling kart reward, battle stats saved | done |
| Bonus roster: Samson, Noah, Joseph, Jonah, John the Baptist with unlocks | done |
| GamePad screen shows something different from the TV | not yet |
| Split-screen Versus | not started (investigation only) |
