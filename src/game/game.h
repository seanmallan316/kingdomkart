// game.h - what the platform layer calls each frame.
#ifndef KK_GAME_H
#define KK_GAME_H
#include "../sim/kart.h"

typedef struct { float sx, sy; int a, b, zl, zr, plus, x, y; int source; } Pad;   /* source: 1 GamePad, 2 Pro Controller, 0 none */

int  game_init(int gfx_status);                 /* gfx_status from gfx_init; the game still runs its loop to show an error color */
void game_frame(const Pad *pad, float dt);      /* simulates and draws one frame (both screens) */
void game_shutdown(void);
void platform_rumble(float strength, float seconds);   /* GamePad / Pro Controller vibration; the platform layer provides it (a no-op on a PC) */
#endif
