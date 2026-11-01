// theme.h - everything that makes one road look and feel different from another: lighting, sky, textures, scenery.
#ifndef KK_THEME_H
#define KK_THEME_H
#include "track.h"

#define MAX_SCENERY 160

typedef struct {
    const char *img;        /* picture name in content/img (without .png), or a placeholder id like "#house" */
    int idx;                /* track sample it stands beside */
    float side;             /* signed distance from the road center, meters (negative = left) */
    float height;           /* how tall it is drawn, meters */
    float glow;             /* 0 = none, else the size of a warm light glow at its base */
} Scenery;

typedef struct {
    const char *name;
    int laps;
    const TrackPoint *points; int npoints;
    /* look */
    float sky_r, sky_g, sky_b;           /* clear color behind everything */
    float fog_r, fog_g, fog_b, fog_start, fog_end;
    float light_x, light_y, light_z, ambient;
    float sun_r, sun_g, sun_b;           /* light tint applied to everything */
    float glow_r, glow_g, glow_b;
    int night;                            /* stars and lantern glows */
    const char *tex_road, *tex_dirt, *tex_grass, *tex_wall, *tex_sky;
    const Scenery *scenery; int nscenery;
    float star_x, star_y, star_z, star_size;   /* 0 size = no star */
    int shortcut_from, shortcut_to, shortcut_side;   /* samples where the shoulder on one side is packed dirt (no slowdown); -1 = none */
    const char *prefix;                   /* picture name prefix: "#house1" on a "beth" road loads content/img/beth_house1.png */
    const RoadPad *pads; int npads;
    const Mover *movers; int nmovers;
    int water;
    int grid_idx;                         /* 0 = a normal loop; else the grid is at this sample and the race is one run to the finish at sample 0 */
    const char *finale_img;               /* a big picture standing across the road past the finish (the gates of Heaven) */
    int finale_idx; float finale_w, finale_h;
    int bonus;                            /* unlockable road */
} TrackDef;

extern const TrackDef TRACK_DEFS[];
extern const int NUM_TRACK_DEFS;
void track_from_def(Track *t, const TrackDef *d);
#endif
