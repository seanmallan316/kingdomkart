// race.h - a whole race: eight karts, AI drivers, item boxes, projectiles, positions. No graphics code.
#ifndef KK_RACE_H
#define KK_RACE_H
#include "kart.h"
#include <stdint.h>

#define NKARTS 8
#define MAX_HUMANS 4                /* people racing at once: the GamePad and up to three Wii controllers */
#define NCHARS 21                   /* 7 to begin with, 14 unlockable (Goliath, Gideon, Zacchaeus, Jacob, Isaac, Abraham, Pharaoh) */
#define NMAIN 7
#define NSTYLES 5                   /* kart bodies: standard + 4 unlockable */
#define MAX_BOX 24
#define MAX_PROJ 24
#define RACE_READY_T 3.0f          /* a calm moment on the grid before 3-2-1 (like Mario Kart) */
#define BATTLE_TIME_LIMIT 120.0f    /* David has two minutes to beat Goliath */
#define RACE_TIME_LIMIT 420.0f      /* a safety net: no race runs past seven minutes (a normal race takes the winner about 3:20) */

enum { RACE_COUNTDOWN, RACE_RUNNING, RACE_FINISHED };
enum { DIFF_EASY, DIFF_NORMAL, DIFF_HARD };
enum { EV_NONE, EV_PICKUP, EV_USE_MANNA, EV_USE_LOAVES, EV_USE_SAMARITAN, EV_USE_SHIELD, EV_HIT, EV_BLOCK, EV_BUMP, EV_LAP, EV_FINAL_LAP, EV_FINISH, EV_GO, EV_COUNT_TICK, EV_USE_PARTING, EV_USE_FIRE, EV_USE_JUBILEE, EV_USE_WATER, EV_PAD_BOOST, EV_PAD_BOUNCE, EV_SLOWED, EV_COIN, EV_TRICK, EV_TRICK_LAND, EV_ROCKET_START, EV_WHEELSPIN,
       EV_BLESSED, EV_CARRIED, EV_TRAIL_BOOST, EV_CHEEK,
       EV_SHOFAR, EV_SHOUT, EV_WALLS_DOWN, EV_GEM, EV_WHALE_WARN, EV_WHALE_UP, EV_SWALLOWED, EV_SPAT_OUT };   /* Jericho: one trumpet blown; the great shout on the seventh lap; the walls are all down */   /* blessed: this kart received something from another; carried: by the Samaritan; trail: ate a manna/fire trail; cheek: a bumped kart got its boost */

typedef struct { const char *name; const char *short_name; KartStats st; float r, g, b; int bonus; } Character;
extern const Character CHARACTERS[NCHARS];
typedef struct { const char *name; KartStats mul; } KartStyle;      /* multiplies the character's stats */
extern const KartStyle KART_STYLES[NSTYLES];

typedef struct { float x, y, z; int active; float respawn; int idx; } ItemBox;
typedef struct { int type; float x, y, z, vx, vz, life, age; int owner, alive, idx, target; float along; } Proj;      /* trails left on the road: TRAIL_MANNA and TRAIL_FIRE give a boost to whoever drives through */
enum { TRAIL_MANNA = 1, TRAIL_FIRE = 2 };
typedef struct { int type, kart; } RaceEvent;

typedef struct {
    const Track *track;
    Kart k[NKARTS]; int nk, player, difficulty;   /* player: the first human's kart (the GamePad's); human[] marks every kart a person drives */
    int human[NKARTS], nhuman;
    int character[NKARTS], style[NKARTS];
    float ai_skill[NKARTS], ai_wobble[NKARTS];
    ItemBox box[MAX_BOX]; int nbox;
    float coin_respawn[MAX_COINS];           /* 0 = the coin is there, else seconds until it comes back */
    float gem_respawn[MAX_GEMS];             /* the same for the hidden gems out in the open land */
    /* the great fish (Jonah 1:17): drive out into the water and it comes up after you */
    float wade_t[NKARTS], swallow_t[NKARTS];  /* seconds in the water; seconds left inside the fish (0 = not swallowed) */
    struct { int active, target; float t, x, z, dx, dz, cool; } whale;   /* t: time since it was set off; x,z: where it comes up; dx,dz: which way it leaps */
    Proj proj[MAX_PROJ];
    int state; float cd, race_t, after_t; int nfin;
    float gas_since[NKARTS];   /* countdown value when a human began holding gas (-1 = not holding); decides the rocket start */
    float parting_t; int parting_idx, parting_len;   /* the parted sea: seconds left, first sample, length in samples */
    float fire_drop[NKARTS];   /* seconds until a blazing kart drops its next fire trail piece */
    int blessed_by[NKARTS]; float blessed_t[NKARTS];
    int given[NKARTS];         /* blessings each kart has given to others (the Most Generous award) */   /* who last blessed this kart and how long ago (for the "BLESSED BY ..." message) */
    /* the Walls of Jericho (Joshua 6): seven laps round the city; a shofar is earned every lap and blown with the item button.
       Laps one to six a blow is a small boost (the priests blew as they marched); on the seventh every shofar held goes at once in the great shout.
       wall_fall runs 0..1 as the walls come down: every blow helps, and on the seventh lap they fall on their own too. */
    int jericho; int shofars[NKARTS], shofar_held[NKARTS]; float wall_fall; int walls_down; float time_limit;
    float collapse_t;   /* Jericho: seconds since the walls began to fall (0 = still standing); they all go at once, at the great shout */
    int order[NKARTS];                       /* order[0] is the leader */
    RaceEvent ev[32]; int nev;               /* events raised this step, for sounds and messages */
    uint32_t rng;
    /* statistics for tests */
    int stat_gems, stat_coins, stat_pickups, stat_hits, stat_blocks, stat_uses, stat_blessings, stat_trails, stat_pads, stat_mover_hits, stat_item_used[ITEM_COUNT];
} Race;

void race_init(Race *r, const Track *t, int player_character, int player_slot, int difficulty, int nk);   /* nk = 1 for a time trial */
void race_init_multi(Race *r, const Track *t, const int *chars, const int *styles, int nhuman, int difficulty);   /* 2..4 people, computer drivers fill the field; humans take the last grid slots in order */
void race_set_unlocked(unsigned mask);     /* which characters the computer may drive (bit per character) */
void race_set_field(const int *chars);   /* NULL = a fresh field each race; a list = the same computer racers in the same seats (a cup) */
void race_set_styles(int player_style, unsigned unlocked_styles);   /* the player's kart body, and the bodies the computer may use */
void race_step(Race *r, const KartInput *player_in, int player_auto, float dt);
void race_step_multi(Race *r, const KartInput *in /* [NKARTS], read for human karts */, int all_auto, float dt);
int  race_human_index(const Race *r, int kart);
void race_init_tt_multi(Race *r, const Track *t, const int *chars, int nhuman, int difficulty);   /* time trial for 2..4 people, no computer drivers */
void race_set_jericho(Race *r);                   /* call right after race_init: the Walls of Jericho rules (no blessing boxes, shofars instead) */   /* 0..nhuman-1 for a human kart (in grid order), -1 for a computer driver */
void race_rank(Race *r);
const char *item_name(int item);
const char *item_verse(int item);          /* the Bible reference behind each blessing */
#endif
