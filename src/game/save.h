// save.h - progress kept on the SD card: records, achievements and what they unlock. Plain text so Sean can read it.
#ifndef KK_SAVE_H
#define KK_SAVE_H

#define SAVE_MAX_ROADS 8

enum {
    UNLOCK_SAMSON = 1 << 0,        /* win 5 races */
    UNLOCK_NOAH = 1 << 1,          /* win a Grand Prix (any difficulty) */
    UNLOCK_JOSEPH = 1 << 2,        /* win a race with every one of the eight characters */
    UNLOCK_JONAH = 1 << 3,         /* beat the target time in Time Trial on the Sea of Galilee */
    UNLOCK_JOHN = 1 << 4,          /* win the Grand Prix on Hard */
    UNLOCK_HEAVEN = 1 << 5,        /* win the Grand Prix on Normal or Hard */
    UNLOCK_KART_CHARIOT = 1 << 6,  /* win 10 races */
    UNLOCK_KART_BOAT = 1 << 7,     /* beat the target time in Time Trial on every road */
    UNLOCK_KART_ARK = 1 << 8,      /* finish 25 races */
    UNLOCK_GOLIATH = 1 << 9,       /* win a race as David: opens the battle */
    UNLOCK_KART_SLING = 1 << 10,   /* beat Goliath */
    UNLOCK_COUNT = 11
};

typedef struct {
    int best_ms[SAVE_MAX_ROADS];   /* best time trial per road in milliseconds, 0 = none */
    int races, wins;               /* races finished, races won (Grand Prix and Single Race) */
    int gp_wins[3];                /* Grand Prix championships per difficulty */
    int battles, battle_wins;      /* David vs Goliath fights and wins */
    unsigned win_chars;            /* bit per character that has won a race */
    unsigned unlocks;              /* UNLOCK_* bits */
    int music_vol, sfx_vol;        /* 0..10 */
    int rumble;                    /* controller vibration on (1) or off (0) */
    int jericho_ms;                /* best Walls of Jericho time in milliseconds, 0 = none */
    int jericho_wins;
    int dirty;
} Save;

extern Save g_save;

void save_set_path(const char *path);
const char *save_dir_path(char *buf, int n, const char *file);   /* a file next to the save file (ghost recordings) */
void save_load(void);
void save_store(void);                         /* writes the file if anything changed */
unsigned save_check_unlocks(void);             /* works out new unlocks from the stats; returns the bits that are new */
float save_par_time(int road);                 /* time trial target in seconds, 0 = none */
const char *unlock_name(int bit);              /* "NOAH", "ROAD TO HEAVEN", ... */
const char *unlock_hint(int bit);              /* how to earn it, for the locked cards */
#endif
