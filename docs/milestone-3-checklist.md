# Milestone 3 checklist: core gameplay

Build: kingdomkart.wuhb (version M3-0.1). Emulator-verified; not yet run on a real Wii U.

What it is: an 8-kart race on the Test Loop with AI drivers, items and results.

Controls: left stick/D-pad steer, A gas, B brake, ZR drift (hold and release for a mini-turbo), ZL/X/Y use the item, + on the results screen cycles Easy/Normal/Hard, A on the results screen races again.

Items (from the blue-and-gold boxes): Manna Boost (speed burst), Shepherd's Staff (shield, blocks one hit), Sling Stone (thrown forward, homes a little), Oil Jar (dropped behind, spins out whoever drives over it).

Checked in the emulator (screenshots in the ci-screenshots branch):
- [x] Eight karts start on a grid, race, and finish; positions update live; the mini-map shows everyone
- [x] Item boxes, item slot, messages (MINI TURBO, FINAL LAP, FINISH, OUCH, BLOCKED)
- [x] Results screen with places and times
Checked on a PC with no graphics (tests/race_test.c, runs on every build):
- [x] 12 races across the three difficulties all end within the 2-minute cap with every kart placed exactly once
- [x] Items are picked up and used, stones and oil jars hit karts, shields block
- [x] No kart ever advances faster than 75 m in a second (no teleporting AI)
- [x] Winners vary by race; grid order is random

Still to check on a real Wii U: frame time with 8 karts and effects.
