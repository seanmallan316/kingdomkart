// race.h - a whole race: eight karts, AI drivers, item boxes, projectiles, positions. No graphics code.
#ifndef KK_RACE_H
#define KK_RACE_H
#include "kart.h"
#include <stdint.h>

#define NKARTS 8
#define NCHARS 13                   /* 8 to begin with, 5 unlockable */
#define NSTYLES 5                   /* kart bodies: standard + 4 unlockable */
#define MAX_BOX 24
#define MAX_PROJ 24
#define RACE_TIME_LIMIT 120.0f      /* every race ends within two minutes */

enum { RACE_COUNTDOWN, RACE_RUNNING, RACE_FINISHED };
enum { DIFF_EASY, DIFF_NORMAL, DIFF_HARD };
enum { EV_NONE, EV_PICKUP, EV_USE_MANNA, EV_USE_STAFF, EV_USE_STONE, EV_USE_OIL, EV_HIT, EV_BLOCK, EV_BUMP, EV_LAP, EV_FINAL_LAP, EV_FINISH, EV_GO, EV_COUNT_TICK, EV_USE_TRUMPET, EV_USE_FIRE, EV_USE_LOCUSTS, EV_USE_DOVE, EV_PAD_BOOST, EV_PAD_BOUNCE, EV_SLOWED, EV_COIN };

typedef struct { const char *name; const char *short_name; KartStats st; float r, g, b; int bonus; } Character;
extern const Character CHARACTERS[NCHARS];
typedef struct { const char *name; KartStats mul; } KartStyle;      /* multiplies the character's stats */
extern const KartStyle KART_STYLES[NSTYLES];

typedef struct { float x, y, z; int active; float respawn; int idx; } ItemBox;
typedef struct { int type; float x, y, z, vx, vz, life, age; int owner, alive, idx, target; float along; } Proj;      /* ITEM_STONE flies, ITEM_OIL sits, ITEM_DOVE follows the road to the leader */
typedef struct { int type, kart; } RaceEvent;

typedef struct {
    const Track *track;
    Kart k[NKARTS]; int nk, player, difficulty;
    int character[NKARTS], style[NKARTS];
    float ai_skill[NKARTS], ai_wobble[NKARTS];
    ItemBox box[MAX_BOX]; int nbox;
    float coin_respawn[MAX_COINS];           /* 0 = the coin is there, else seconds until it comes back */
    Proj proj[MAX_PROJ];
    int state; float cd, race_t, after_t; int nfin;
    int order[NKARTS];                       /* order[0] is the leader */
    RaceEvent ev[32]; int nev;               /* events raised this step, for sounds and messages */
    uint32_t rng;
    /* statistics for tests */
    int stat_coins, stat_pickups, stat_hits, stat_blocks, stat_uses, stat_stones, stat_oils, stat_pads, stat_mover_hits, stat_item_used[ITEM_COUNT];
} Race;

void race_init(Race *r, const Track *t, int player_character, int player_slot, int difficulty, int nk);   /* nk = 1 for a time trial */
void race_set_unlocked(unsigned mask);     /* which characters the computer may drive (bit per character) */
void race_set_styles(int player_style, unsigned unlocked_styles);   /* the player's kart body, and the bodies the computer may use */
void race_step(Race *r, const KartInput *player_in, int player_auto, float dt);
void race_rank(Race *r);
const char *item_name(int item);
#endif
