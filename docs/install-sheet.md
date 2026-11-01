# Kingdom Kart: Road to Glory — putting it on your Wii U

You need: a Wii U with Aroma already installed (the homebrew menu you already use), its SD card, and a computer
with an SD card reader (your Mac works).

## Part 1: get the game file (one time per new version)
1. On the Mac, open Safari and go to **github.com/seanmallan316/kingdomkart**.
2. Click the **Actions** tab (top of the page, next to "Pull requests").
3. The list shows every build. Click the **top one** with a **green check mark**.
4. Scroll to the bottom of that page to the box called **Artifacts**.
5. Click **KingdomKart**. A file called `KingdomKart.zip` downloads.
6. Double-click `KingdomKart.zip` in your Downloads folder. A file called **kingdomkart.wuhb** appears next to it.

## Part 2: the shader compiler file (one time only, ever)
1. In Safari go to **github.com/Exzap/CafeGLSL/releases**.
2. Under the newest release, click **glslcompiler.rpl** to download it.

## Part 3: copy the two files onto the SD card
1. Turn the Wii U off. Take the SD card out of the front slot and put it in the Mac's card reader.
2. The card appears on the desktop (or in Finder's sidebar). Open it.
3. Open the folder **wiiu**. (It is already there from Aroma.)
4. Open the folder **apps** inside it. Drag **kingdomkart.wuhb** into **apps**. If an older kingdomkart.wuhb is there, say yes to replacing it.
5. Go back to **wiiu**. If there is no folder called **libs**, make one: right-click in the empty space, New Folder, name it `libs` (all lowercase).
6. Drag **glslcompiler.rpl** into **libs**.
7. Eject the card (drag it to the Trash, or click the eject arrow next to it in Finder) and put it back in the Wii U.

## Part 4: play
1. Turn the Wii U on and open the Aroma / homebrew menu the way you normally do.
2. **Kingdom Kart** is in the list with a gold crown icon. Start it.
3. The title screen appears over a demo race. Press **A**.

Controls: left stick steers · **A** gas · **B** brake · **ZR** hold to drift, let go for a turbo · **ZL** use your item
(or sling a stone against Goliath) · **+** pause · **HOME** quit. The GamePad screen shows the map and standings.

## If something is wrong
- **Solid orange screen**: the file from Part 2 is missing or in the wrong folder. It must be exactly `wiiu/libs/glslcompiler.rpl`.
- **Solid red screen**: the compiler was found but failed. Copy the file `wiiu/kingdomkart_log.txt` from the SD card and send it to me.
- **Black screen, or it drops back to the menu**: send me `wiiu/kingdomkart_log.txt` and tell me what you saw and roughly when.
- **No sound**: tell me; the TV volume and the GamePad volume are separate, and the game sends sound to both.
- **It runs but stutters**: tell me how the bar at the bottom-left looks (green = fine, red = too slow) and on which road.

Your progress (records, unlocks) lives in `wiiu/kingdomkart_save.txt` on the card. Delete that file to start fresh.
