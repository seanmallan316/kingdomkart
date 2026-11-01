// save.h - progress kept on the SD card: records, achievements and what they unlock. Plain text so Sean can read it.
#ifndef KK_SAVE_H
#define KK_SAVE_H

#define SAVE_MAX_ROADS 8

enum {
    UNLOCK_NOAH = 1 << 0,          /* win 5 races */
    UNLOCK_ELIJAH = 1 << 1,        /* win the Grand Prix on Hard */
    UNLOCK_HEAVEN = 1 << 2,        /* win the Grand Prix on Normal or Hard */
    UNLOCK_KART_CHARIOT = 1 << 3,  /* win a race with every one of the eight characters */
    UNLOCK_KART_BOAT = 1 << 4,     /* beat the target time in Time Trial on every road */
    UNLOCK_KART_ARK = 1 << 5,      /* finish 25 races */
    UNLOCK_COUNT = 6
};

typedef struct {
    int best_ms[SAVE_MAX_ROADS];   /* best time trial per road in milliseconds, 0 = none */
    int races, wins;               /* races finished, races won (Grand Prix and Single Race) */
    int gp_wins[3];                /* Grand Prix championships per difficulty */
    unsigned win_chars;            /* bit per character that has won a race */
    unsigned unlocks;              /* UNLOCK_* bits */
    int dirty;
} Save;

extern Save g_save;

void save_set_path(const char *path);
void save_load(void);
void save_store(void);                         /* writes the file if anything changed */
unsigned save_check_unlocks(void);             /* works out new unlocks from the stats; returns the bits that are new */
float save_par_time(int road);                 /* time trial target in seconds, 0 = none */
const char *unlock_name(int bit);              /* "NOAH", "ROAD TO HEAVEN", ... */
const char *unlock_hint(int bit);              /* how to earn it, for the locked cards */
#endif
