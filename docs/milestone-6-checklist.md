# Milestone 6 (+6b): all the roads, more items, obstacles, unlockables

Checked = host tests with the address sanitizer (tests/track_test.c covers every road) and Cemu emulator
screenshots from the CI tour builds (one per road). NOT yet run on a real Wii U.

| Item | Status |
|---|---|
| Sea of Galilee: beach road, stone causeway over water, jump onto the sandbar, fishing village, sandbar shortcut | done |
| Jerusalem Road: climb to the gate, market street, jump off the temple steps, olive-slope alley shortcut | done |
| Sunrise at the Empty Tomb: garden climb, jump down the garden wall, the tomb with the angel (Sean's art), three crosses | done |
| Bethlehem upgraded: boost arrows, a bounce pad in the square, sheep wandering across the hill road | done |
| Boost arrows (speed) and bounce pads (hop) on every road | done |
| Obstacles crossing the road: sheep, fishermen's nets, market carts, rolling stones; computer drivers steer around them | done |
| New power-ups: SHOFAR (spins out everyone nearby), CHARIOT OF FIRE (long boost + protection), LOCUSTS (slows everyone ahead), DOVE (flies to the leader) | done |
| Item pictures in the HUD slot when a file exists (shofar.png is in) | done |
| Progress saved to sd:/wiiu/kingdomkart_save.txt (records, races, wins, unlocks) | done (emulator cannot write its SD card, so checked on the PC) |
| Achievements and unlocks: Noah (5 wins), Elijah (GP on Hard), Road to Heaven (GP on Normal/Hard), Chariot kart (win with all 8), Boat kart (beat target times), Ark kart (25 races) | done |
| Locked cards with hints on the character, kart and road screens; UNLOCKED banner on the results | done |
| Kart select: Standard, Chariot, Boat, Ark bodies with stat changes | done |
| Road to Heaven: one run from the earth up the switchbacks through the clouds to the plaza, angels and trumpets (Sean's art), the gates with God's hands across the road, WELCOME HOME | done |
| Time trial target times tuned by playing | not yet (set from computer-driver times + margin) |
| Sounds / music | parked |
