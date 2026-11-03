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
enum { BEV_NONE = 1, BEV_STONE_PICKED, BEV_STONE_THROWN, BEV_GOLIATH_HIT, BEV_DAVID_HIT, BEV_SPEAR, BEV_CHARGE, BEV_ROAR, BEV_WIN, BEV_LOSE, BEV_STEP, BEV_QUOTE, BEV_SHIELD };   /* SHIELD: the shield-bearer caught a stone */
/* words from 1 Samuel 17 (King James Version), spoken during the fight */
typedef struct { const char *who; const char *line; const char *ref; } BattleQuote;
extern const BattleQuote BATTLE_QUOTES[];
#define BATTLE_NQUOTES 6
/* the brook: a straight line across the valley; the stones lie in it */
#define BROOK_X0 -85.0f
#define BROOK_X1  85.0f
#define BROOK_Z   78.0f

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
    float roar_t, step_t;
    int quote; float quote_t;        /* the quote on screen (-1 none) and how long it has left */
    int quotes_said;                 /* bit per quote */
    int blocks;                      /* stones the shield-bearer caught (1 Samuel 17:41: he walks before Goliath; hit him from the side or behind) */
    float sb_yaw;                    /* which way the shield-bearer stands from Goliath: he follows Goliath's turn, but slowly, so a quick kart can get round him */
} Battle;

void battle_init(Battle *b, const Track *arena, int difficulty);
void battle_step(Battle *b, const KartInput *in, int autopilot, float dt);
#endif
