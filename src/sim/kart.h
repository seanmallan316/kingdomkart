// kart.h - kart physics: driving, drifting, mini-turbos, jumps, walls, laps. No graphics code here, so it runs in tests on a PC.
#ifndef KK_KART_H
#define KK_KART_H
#include "../world/track.h"

#define SIM_DT (1.0f / 120.0f)
#define MAX_LAPS 8
#define WRONGWAY_DELAY 1.5f    /* seconds of driving backwards before the warning shows */

typedef struct { float steer; int gas, brake, drift, item; } KartInput;   /* steer -1..1 (left negative); item = use the held item */
enum { ITEM_NONE = 0, ITEM_MANNA, ITEM_STAFF, ITEM_STONE, ITEM_OIL, ITEM_COUNT };

typedef struct {
    float top_speed, accel, handling, weight;    /* per character stats, 1.0 = average */
} KartStats;

typedef struct {
    /* motion */
    float x, y, z, yaw;        /* yaw 0 = facing +z, increases turning left */
    float speed;               /* forward speed, m/s (negative = reversing) */
    float vy; int air; float air_t;
    float slide;               /* visual yaw offset while drifting */
    float steer_vis, hop, squash;
    /* drift */
    int drifting, drift_dir; float drift_charge, boost_t, boost_mul;
    int mini_turbo_tier;       /* 0 none, 1 small, 2 big, set for the frame the boost starts */
    /* track */
    TrackHit hit; int on_wall, on_wall_prev;
    float dist;                /* progress along the track in meters (grows across laps) */
    /* items and damage */
    int item;                  /* ITEM_NONE or an item id the kart is holding */
    float item_hold;           /* seconds the item has been held (AI uses this) */
    float stun;                /* seconds left spinning out, no control */
    float shield;              /* seconds of shield left (blocks one hit) */
    float spin_vis;            /* visual spin angle while stunned */
    int blocked_flag, hit_flag;/* set for one step when the shield blocks a hit / the kart is hit */
    /* race */
    int lap, next_cp; float wrongway_t, lap_time, total_time, best_lap, lap_times[MAX_LAPS]; int finished, place;
    int skid;                  /* 1 when the tires should leave marks this step */
    KartStats st;
} Kart;

void kart_reset(Kart *k, const Track *t, int grid_slot);
void kart_step(Kart *k, const Track *t, const KartInput *in, float dt);
void kart_autopilot(const Kart *k, const Track *t, KartInput *in, float skill);   /* drives like a decent player */
int  kart_hit(Kart *k, float stun_seconds);                                      /* spin out unless the shield blocks it; returns 1 if it hit */
#endif
