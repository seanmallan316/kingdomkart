// track.h - a race track is a smooth closed curve with width, height and banking, sampled into a lookup table.
#ifndef KK_TRACK_H
#define KK_TRACK_H
#include "../render/kkmath.h"

#define TRACK_MAX_SAMPLES 1024
#define TRACK_CHECKPOINTS 8
#define SHOULDER_W 4.0f          /* dirt strip on each side of the road, in meters */
#define WALL_DROP 1.2f           /* how far the shoulder slopes down below the road at its outer edge */

#define MAX_OBST 320
typedef struct { float x, y, z, width, bank; int jump; } TrackPoint;   /* a control point of the curve; jump = 1 makes the road drop away right after this point */

typedef struct {
    float x, y, z;          /* center of the road */
    float tx, tz;           /* unit direction of travel (flat) */
    float nx, nz;           /* unit direction to the right (flat) */
    float width;            /* full road width */
    float bank;             /* bank angle in radians, positive = right side higher */
    float dist;             /* distance from the start line along the road */
} TrackSample;

/* things on the road */
enum { PAD_BOOST = 1, PAD_BOUNCE = 2 };
typedef struct { int idx; float lateral, halfw; int kind; } RoadPad;               /* a boost arrow or a bounce pad painted on the road */
typedef struct { const char *img; int idx; float lateral, amp, speed, phase, radius, height; } Mover;   /* an obstacle crossing the road: lateral + amp*sin(time*speed+phase) */
#define MAX_PADS 32
#define MAX_MOVERS 16
#define MAX_COINS 120
#define MAX_TUNNELS 4
typedef struct { int idx, n, step; float lateral; } CoinRow;                  /* n coins from sample idx, every step samples, at a lateral offset */
typedef struct { int idx; float lateral; } Coin;
typedef struct { int from, to; float height; } Tunnel;                        /* the road runs under a roof between these samples */
#define MAX_GEMS 16
#define FIELD_MAX 40.0f          /* the most open land beside the road, past the shoulder, in meters */
typedef struct { int idx; float lateral; int color; } Gem;                    /* a hidden gem out in the open land: coins and a burst of speed */

typedef struct {
    const char *name;
    int laps;
    int n;                                  /* number of samples */
    TrackSample s[TRACK_MAX_SAMPLES];
    float length;
    int checkpoint[TRACK_CHECKPOINTS];      /* sample index of each checkpoint gate; gate 0 is the start/finish line */
    int shortcut_from, shortcut_to, shortcut_side;   /* samples where one shoulder is packed dirt with no slowdown; -1 = none */
    RoadPad pad[MAX_PADS]; int npad;
    Mover mover[MAX_MOVERS]; int nmover;
    int water;                              /* off-road is water (splashes instead of dust) */
    int grid_idx;                           /* sample the grid lines up at; > 0 means the race starts partway (finish at sample 0) */
    Coin coin[MAX_COINS]; int ncoin;
    Tunnel tunnel[MAX_TUNNELS]; int ntunnel;
    float field[TRACK_MAX_SAMPLES][2];      /* open land past the shoulder on the left [0] and right [1] side, in meters (0 = the wall is at the shoulder) */
    float water_y;                          /* water roads: the open land is shallow water at this height */
    Gem gem[MAX_GEMS]; int ngem;
    short def_q[257]; unsigned char def_m[256]; short def_per, def_np;   /* where each stretch of the road's definition went once its S-bends were added */
    float def_scale;                        /* sample numbers in a road's definition (drawn at 20 a stretch) times this = sample numbers here */
    int nobst; struct { float x, z, r; } obst[MAX_OBST]; float widen;   /* widen: how much wider the road is than drawn (scenery moves out with it) */   /* scenery standing out in the open land: trees, rocks, houses to steer round */
} Track;

typedef struct {
    int idx;            /* nearest sample */
    float lateral;      /* signed distance from the center line, positive = right */
    float height;       /* surface height at this spot */
    float slope;        /* dy per meter along the road (uphill positive) */
    int on_road;        /* within the paved width */
    int on_shoulder;    /* within the shoulder */
    int on_field;       /* out in the open land past the shoulder */
    float bound;        /* distance from the center line to the outer wall on this side */
} TrackHit;

void track_build(Track *t, const char *name, int laps, const TrackPoint *pts, int npts, int samples_per_segment);
void track_locate(const Track *t, float x, float z, int hint, TrackHit *out);   /* hint = last known index, -1 for a full search */
float track_surface_height(const Track *t, int idx, float lateral);
int track_def_idx(const Track *t, int def_idx);   /* a sample number from a road's definition (20 a stretch) -> the sample here */
float track_scenery_radius(const char *img, float height);                        /* ground room a piece of scenery takes (0: drive through) */
float track_scenery_lat(const Track *t, int idx, float side, float r);           /* where it stands across the road, clear of the road */
float track_edge(const Track *t, int idx);                                      /* distance from center to the outer edge of the shoulder */
float track_bound(const Track *t, int idx, int side);                           /* distance from center to the outer wall on a side (-1 left, +1 right) */
void  track_make_field(Track *t, int water);                                    /* works out how much open land each stretch can have without reaching another part of the road */
void  track_mover_pos(const Track *t, const Mover *m, float time, float *x, float *y, float *z);

/* built-in tracks (see tracks.c) */
void track_make_test(Track *t);
#endif
