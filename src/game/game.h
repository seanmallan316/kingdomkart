// game.h - what the platform layer calls each frame.
#ifndef KK_GAME_H
#define KK_GAME_H
#include "../sim/kart.h"

#define KK_PADS 5      /* controller slots: 0 the GamePad, 1..4 the Wii controllers (Pro Controller, Classic Controller, Wii Remote held sideways, or Remote + Nunchuk) */
typedef struct { float sx, sy; int a, b, zl, zr, plus, x, y; int source; } Pad;   /* source bits: 1 GamePad, 2 Pro Controller, 4 Wii Remote, 8 Classic Controller, 16 Nunchuk; 0 = nothing in this slot */

int  game_init(int gfx_status);                 /* gfx_status from gfx_init; the game still runs its loop to show an error color */
void game_frame(const Pad *pads, int npads, float dt);   /* simulates and draws one frame (both screens); pads[0] is the GamePad, the rest the Wii controllers */
void game_shutdown(void);
void platform_rumble(float strength, float seconds);              /* vibration on every controller that is player 1's (the GamePad and, alone, a Pro Controller); a no-op on a PC */
void platform_rumble_pad(int slot, float strength, float seconds);   /* vibration on one controller slot */
#endif
