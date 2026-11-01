# Kingdom Kart: Road to Glory

An original 3D kart racing game for the modded Wii U (Aroma homebrew), with Bible characters and a story that follows Jesus from Bethlehem to the empty tomb.

## Where the project is right now: Milestone 1, the foundation test

This build is **not the game yet**. It is a small test that proves the hard parts work on a real Wii U before the game is built on top of them:

- real 3D graphics on the Wii U's own graphics chip (a lit, textured cube on a checkered ground, drawn on the TV and the GamePad)
- the shader compiler that the 3D graphics depend on
- GamePad and Pro Controller input
- reading files packed inside the game file
- measuring how long each frame takes

### What you should see
A sky-blue screen with a gold-and-blue cube sitting on a checkered green ground, with eight white pillars around it.
Move the left stick to turn, hold **A** to drive forward, **B** to back up, **ZR** to spin the cube fast, **ZL** to tilt it. Press **HOME** to leave.

- Bottom of the screen: a bar that shows how long each frame takes. **Green** means the frame budget for 60 frames per second is met; **red** means it is over.
- Top-left: a small square. **Blue** = GamePad is being read, **gold** = Pro Controller is being read, **grey** = no controller yet.

### If the screen is a solid color instead
- **Solid orange**: the shader compiler file is missing. Copy `glslcompiler.rpl` to `sd:/wiiu/libs/` (see below).
- **Solid red**: the shader compiler was found but failed. Send me the log file `sd:/wiiu/kingdomkart_log.txt`.
- **Black screen or back to the menu**: the program crashed. Send me the log file if it exists, and tell me what you saw.

## Install (Mac or PC, same steps)
1. Get `kingdomkart.wuhb` from GitHub (see the next section) and copy it into the `wiiu/apps` folder on your SD card.
2. Download `glslcompiler.rpl` from the CafeGLSL releases page (https://github.com/Exzap/CafeGLSL/releases) and copy it into the `wiiu/libs` folder on your SD card (create `libs` if it does not exist). This is a one-time step; every future build uses the same file.
3. Put the card in the Wii U and start **Kingdom Kart** from the Aroma menu.
4. After trying it, the file `wiiu/kingdomkart_log.txt` on the SD card has everything I need to diagnose a problem.

## How the game file gets built (GitHub does it)
1. Make a free account at github.com and create a new empty repository called `kingdomkart`.
2. Upload everything in this folder (including the hidden `.github` folder) to it.
3. Open the **Actions** tab. A job called "Build Wii U game" runs by itself and takes a few minutes.
4. When it finishes, open it and download **KingdomKart** at the bottom. Unzip it to get `kingdomkart.wuhb`.

The job also tries to run the build inside a Wii U emulator and save screenshots (an "EmulatorScreenshots" download). That part is experimental and may fail; the game file is still good if it does.

## Honest status
- Nothing in this repository has been run on a real Wii U yet. It compiles against the real Wii U library headers, and it is built the same way as an existing homebrew 3D demo, but the first real test is yours.
- The frame-time bar reads the console's own clock, so it is the true number, not an estimate.
