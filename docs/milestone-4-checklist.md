# Milestone 4 checklist: Bethlehem Road (engine side done, art pending)

Build: kingdomkart.wuhb (version M4-0.1). Emulator-verified; not yet run on a real Wii U.

Done in code:
- [x] Bethlehem Road: 568 m village loop at night (main street, village square, hill with the stable, switchbacks, alley shortcut inside the tight corner)
- [x] Night lighting: dark sky with stars, cool moonlight, fog, warm flickering lantern and window glows, the Star over the hill
- [x] Scenery system: cut-out pictures placed beside the road, grouped by picture, only drawn within 340 m
- [x] Picture loading from inside the game file; every slot falls back to a built-in drawing when the real picture is missing
- [x] tools/prep_assets.py turns generated pictures into game-ready ones (background removal, sizing)
- [x] B on the results screen switches between Bethlehem Road and the Test Loop
- [x] Automated: every road is checked for self-crossing, scenery inside the walls, and a full race finishing in time

Waiting on art (see the "Kingdom Kart: Art List" doc): portraits, Bethlehem scenery, textures, sky, icons, title art.
