// track.h - a race track is a smooth closed curve with width, height and banking, sampled into a lookup table.
#ifndef KK_TRACK_H
#define KK_TRACK_H
#include "../render/kkmath.h"

#define TRACK_MAX_SAMPLES 512
#define TRACK_CHECKPOINTS 8
#define SHOULDER_W 4.0f          /* dirt strip on each side of the road, in meters */
#define WALL_DROP 1.2f           /* how far the shoulder slopes down below the road at its outer edge */

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
} Track;

typedef struct {
    int idx;            /* nearest sample */
    float lateral;      /* signed distance from the center line, positive = right */
    float height;       /* surface height at this spot */
    float slope;        /* dy per meter along the road (uphill positive) */
    int on_road;        /* within the paved width */
    int on_shoulder;    /* within the shoulder */
} TrackHit;

void track_build(Track *t, const char *name, int laps, const TrackPoint *pts, int npts, int samples_per_segment);
void track_locate(const Track *t, float x, float z, int hint, TrackHit *out);   /* hint = last known index, -1 for a full search */
float track_surface_height(const Track *t, int idx, float lateral);
float track_edge(const Track *t, int idx);                                      /* distance from center to the wall */
void  track_mover_pos(const Track *t, const Mover *m, float time, float *x, float *y, float *z);

/* built-in tracks (see tracks.c) */
void track_make_test(Track *t);
#endif
