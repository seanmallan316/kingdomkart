// battle.h - the Valley of Elah: David against Goliath in an arena. Goliath stalks, charges and throws spears;
// David picks up stones from the ground and slings them. Five hits on Goliath win, three hits on David lose.
#ifndef KK_BATTLE_H
#define KK_BATTLE_H
#include "race.h"

#define BATTLE_STONES 6
#define BATTLE_SPEARS 6
#define BATTLE_WIN_HITS 5
#define BATTLE_LIVES 3

enum { GOL_STALK, GOL_WINDUP, GOL_CHARGE, GOL_THROW, GOL_STUNNED, GOL_DOWN };
enum { BEV_NONE = 1, BEV_STONE_PICKED, BEV_STONE_THROWN, BEV_GOLIATH_HIT, BEV_DAVID_HIT, BEV_SPEAR, BEV_CHARGE, BEV_ROAR, BEV_WIN, BEV_LOSE };

typedef struct { float x, z, vx, vz, y, life; int alive; } Missile;
typedef struct {
    Race race;                       /* David's kart runs on the arena road with the normal physics (one kart, no boxes) */
    float gx, gz, gyaw, gspeed;      /* Goliath */
    int gstate; float gtimer, gstun;
    float charge_tx, charge_tz;      /* where the charge is aimed */
    int hits_on_goliath, hits_on_david, stones_held;
    float stone_respawn[BATTLE_STONES]; float stone_x[BATTLE_STONES], stone_z[BATTLE_STONES];
    Missile stone[BATTLE_SPEARS];    /* stones in the air */
    Missile spear[BATTLE_SPEARS];
    float t, invuln;                 /* battle time, seconds David cannot be hit again */
    int over, won, was_throw;
    int ev[16], nev;
    float roar_t;
} Battle;

void battle_init(Battle *b, const Track *arena, int difficulty);
void battle_step(Battle *b, const KartInput *in, int autopilot, float dt);
#endif
