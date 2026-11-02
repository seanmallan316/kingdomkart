// view.c - draws the race: track, eight karts, items, effects, HUD, results. The rules live in sim/race.c.
#include "game.h"
#include "save.h"
#include "../audio/sfx.h"
#include "../audio/music.h"
#include "../sim/battle.h"
#include "../sim/race.h"
#include "../render/gfx.h"
#include "../render/batch.h"
#include "../render/assets.h"
#include "../world/theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SKID 500
#define MAX_DUST 200
#define MAX_SPRITE_KINDS 16
#define SPRITE_DRAW_DIST 340.0f
#define NUM_GP_ROUNDS_MAX 4
#define NUM_PLAY_TRACKS (NUM_TRACK_DEFS - 2)     /* the last two are the battle arena and the developers' Test Loop, not race roads */
#define NUM_GP_TRACKS 4                          /* the Grand Prix runs the four story roads; bonus roads are for Single Race and Time Trial */
#ifndef KK_TOUR_ROAD
#define KK_TOUR_ROAD 1
#endif

static int g_status;
static Track g_track; static const TrackDef *g_def; static int g_track_idx = 0;
static Battle g_battle;            /* the battle holds the race; in the racing modes only its race is used */
#define g_race (g_battle.race)
#define ARENA_TRACK (NUM_TRACK_DEFS - 2)   /* the Valley of Elah sits just before the Test Loop */
static int g_player_char = 0, g_difficulty = DIFF_NORMAL;
/* game flow */
enum { SCR_TITLE, SCR_MAIN, SCR_CHAR, SCR_KART, SCR_TRACK, SCR_RACE, SCR_PAUSE, SCR_RESULTS, SCR_GP, SCR_CREDITS, SCR_OPTIONS };
static int g_opt_sel = 0;
static void apply_volumes(void) { music_set_volume(g_save.music_vol / 10.0f); sfx_set_volume(g_save.sfx_vol / 10.0f); }
enum { MODE_GP, MODE_SINGLE, MODE_TT, MODE_BATTLE };
static int g_screen = SCR_TITLE, g_mode = MODE_SINGLE, g_menu_sel = 0, g_pause_sel = 0, g_attract = 1;
static int char_lock_bit(int c) { static const int bits[NCHARS] = {0, 0, 0, 0, 0, 0, 0, UNLOCK_SAMSON, UNLOCK_NOAH, UNLOCK_JOSEPH, UNLOCK_JONAH, UNLOCK_JOHN}; return c >= 0 && c < NCHARS ? bits[c] : 0; }
static int char_unlocked(int c) { return !CHARACTERS[c].bonus || (g_save.unlocks & char_lock_bit(c)); }
static int style_lock_bit(int sty) { return sty == 1 ? UNLOCK_KART_CHARIOT : (sty == 2 ? UNLOCK_KART_BOAT : (sty == 3 ? UNLOCK_KART_ARK : (sty == 4 ? UNLOCK_KART_SLING : 0))); }
static int style_unlocked(int sty) { return sty == 0 || (g_save.unlocks & style_lock_bit(sty)); }
static unsigned unlocked_styles(void) { unsigned m = 0; for (int i = 0; i < NSTYLES; i++) if (style_unlocked(i)) m |= 1u << i; return m; }
static int g_player_style = 0;
static int track_unlocked(int d) { return !TRACK_DEFS[d].bonus || (g_save.unlocks & UNLOCK_HEAVEN); }
static unsigned unlocked_mask(void) { unsigned m = 0; for (int c = 0; c < NCHARS; c++) if (char_unlocked(c)) m |= 1u << c; return m; }
static int g_gp_round = 0, g_gp_points[NCHARS], g_gp_order[NUM_GP_ROUNDS_MAX];
static float g_best_time[8];                 /* best time trial time per road (loaded from the save file) */
static Texture *tx_portrait[NCHARS]; static float g_portrait_aspect[NCHARS];
static Texture *tx_char[NCHARS]; static float g_char_aspect[NCHARS];      /* full-body pictures: the drivers in the karts */
static Texture *tx_kart[NCHARS]; static float g_kart_aspect[NCHARS]; static Mesh *m_kartpic;   /* each character's own kart (kart-select screen) */
static Texture *tx_kartback[NCHARS]; static float g_kartback_aspect[NCHARS]; static Mesh *m_kart_spr[NKARTS];   /* the kart seen from behind: drives on the track */
/* the same kart from more angles (N64 style): 0 behind, 1 back-left 45, 2 left side, 3 front-left 45, 4 front; the right side is the left mirrored */
static Texture *tx_kartview[NCHARS][5]; static float g_kartview_aspect[NCHARS][5];
static float g_shake;
static Texture *tx_goliath; static float g_goliath_aspect = 0.67f; static Mesh *m_goliath, *m_hud2;
static Mesh *m_driver_box, *m_driver_head, *m_driver_spr[NKARTS];
static unsigned g_fresh_unlocks;             /* unlock bits earned by the race just finished, shown on the results */
static Texture *tx_finale; static Mesh *m_finale;
static Texture *tx_title; static Mesh *m_title;      /* title_art.png behind the title, when Sean has made it */
static Texture *tx_card[8]; static Mesh *m_card[8];   /* card_<prefix>.png: the road pictures on the road-select cards */
static Texture *tx_item[ITEM_COUNT];        /* item pictures for the HUD slot, when Sean's files exist */
static Pad g_prev; static float g_nav_t, g_screen_t;
static const int GP_POINTS[8] = {10, 8, 6, 5, 4, 3, 2, 1};
typedef struct { float x[80], z[80]; int n; } Preview;
static Preview g_preview[8];
static float g_idle;                   /* seconds since the last real controller input */
static int g_demo;                     /* autopilot driving the player's kart */
static float g_time, g_msg_t; static char g_msg[40];
static float g_frame_ms = 16.7f;
static float g_results_t;

/* textures */
static Texture *tx_road, *tx_dirt, *tx_grass, *tx_wall, *tx_blob, *tx_font, *tx_flag, *tx_sky, *tx_star;
/* meshes */
static Mesh *m_brook, *m_tunnel, *m_arch, *m_coin, *m_coins, *m_boostpads, *m_bouncepads, *m_road, *m_shoulder, *m_ground, *m_wall, *m_startline, *m_kart_body, *m_kart_rest, *m_body_style[NSTYLES], *m_rest_style[NSTYLES], *m_cube, *m_shield, *m_items, *m_skid, *m_glow, *m_dust, *m_hud, *m_shadow, *m_sky;
static Vertex g_vtmp[8192];
static Batch g_b;

/* camera state */
static float cam_yaw, cam_x, cam_y, cam_z, cam_fov = 62.0f;
static float g_slowmo = 0, g_orbit = 0;      /* finish-line slow motion (seconds left) and the camera swing around the kart */
/* skid marks: ring of quads */
typedef struct { Vec3 l0, r0, l1, r1; float a; int used; } Skid;
static Skid g_skid[MAX_SKID]; static int g_skid_n; static Vec3 g_last_l[NKARTS], g_last_r[NKARTS]; static int g_last_skid[NKARTS];
/* dust */
typedef struct { Vec3 p, v; float life, size; } Dust;
static Dust g_dust[MAX_DUST];
#define MAX_SPARK 160
typedef struct { Vec3 p, v; float life, size, r, g, b; } Spark;
static Spark g_spark[MAX_SPARK]; static Mesh *m_spark;
/* scenery: cut-out pictures standing beside the road, grouped by picture */
typedef struct { const char *img; Texture *tex; float aspect; Mesh *mesh; } SpriteKind;
static SpriteKind g_kind[MAX_SPRITE_KINDS]; static int g_nkind;
typedef struct { Vec3 p; float h, w, glow; int kind; int purple, falls; } Placed;
static Placed g_placed[MAX_SCENERY]; static int g_nplaced;
static int g_mover_kind[MAX_MOVERS];
static int g_was_air[NKARTS];
/* mini-map */
static float map_x0, map_z0, map_scale;

static uint32_t rng = 12345u;
static float frand01(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (rng & 0xFFFFFF) / 16777216.0f; }
static uint32_t rgba(int r, int g, int b, int a) { return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)a; }

// ------------------------------------------------------------------ procedural textures (placeholders until the real art arrives)
static uint32_t g_px[256 * 256];
static Texture *make_tex(int w, int h, int repeat, int nearest, void (*fn)(int x, int y, uint32_t *out)) {
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) fn(x, y, &g_px[y * w + x]);
    return gfx_texture_rgba(g_px, w, h, repeat, nearest);
}
static float noise2(int x, int y) { uint32_t n = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u; n = (n ^ (n >> 13)) * 1274126177u; return (float)(n & 0xFFFF) / 65535.0f; }
static float smooth_noise(float x, float y) {
    int xi = (int)floorf(x), yi = (int)floorf(y); float fx = x - xi, fy = y - yi; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    float a = noise2(xi, yi), b = noise2(xi + 1, yi), c = noise2(xi, yi + 1), d = noise2(xi + 1, yi + 1);
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
}
static void px_road(int x, int y, uint32_t *o) {       /* cobblestone-ish asphalt */
    float n = smooth_noise(x / 9.0f, y / 9.0f) * 0.6f + smooth_noise(x / 3.0f, y / 3.0f) * 0.4f;
    int stone = ((x / 32) + (y / 32)) & 1; int edge = (x % 32 < 2) || (y % 32 < 2);
    int v = 95 + (int)(n * 50) + stone * 12 - edge * 35;
    *o = rgba(v, v - 4, v - 10, 255);
}
static void px_dirt(int x, int y, uint32_t *o) { float n = smooth_noise(x / 12.0f, y / 12.0f) * 0.7f + smooth_noise(x / 4.0f, y / 4.0f) * 0.3f; *o = rgba(150 + (int)(n * 60), 118 + (int)(n * 50), 70 + (int)(n * 30), 255); }
static void px_grass(int x, int y, uint32_t *o) { float n = smooth_noise(x / 16.0f, y / 16.0f) * 0.6f + smooth_noise(x / 5.0f, y / 5.0f) * 0.4f; *o = rgba(70 + (int)(n * 50), 140 + (int)(n * 70), 50 + (int)(n * 30), 255); }
static void px_sand(int x, int y, uint32_t *o) { float n = smooth_noise(x / 10.0f, y / 10.0f) * 0.6f + smooth_noise(x / 3.0f, y / 3.0f) * 0.4f; *o = rgba(222 + (int)(n * 25), 200 + (int)(n * 25), 150 + (int)(n * 25), 255); }
static void px_water(int x, int y, uint32_t *o) { float n = smooth_noise(x / 14.0f, y / 9.0f) * 0.7f + smooth_noise(x / 5.0f, y / 3.0f) * 0.3f; int f = n > 0.72f ? 70 : 0; *o = rgba(40 + (int)(n * 40) + f, 120 + (int)(n * 60) + f, 190 + (int)(n * 50) + f / 2, 255); }
static void px_gold(int x, int y, uint32_t *o) {       /* streets of gold: polished tiles */
    int ex = (x % 32) < 2 || (y % 32) < 2; float n = smooth_noise(x / 9.0f, y / 9.0f);
    *o = ex ? rgba(200, 150, 50, 255) : rgba(235 + (int)(n * 20), 195 + (int)(n * 40), 90 + (int)(n * 40), 255);
}
static void px_cloud(int x, int y, uint32_t *o) {      /* soft white cloud tops */
    float n = smooth_noise(x / 18.0f, y / 18.0f) * 0.6f + smooth_noise(x / 6.0f, y / 6.0f) * 0.4f;
    *o = rgba(225 + (int)(n * 30), 228 + (int)(n * 27), 240 + (int)(n * 15), 255);
}
static void px_wall(int x, int y, uint32_t *o) {       /* stone blocks */
    int row = y / 16, off = (row & 1) * 16; int bx = (x + off) % 32, by = y % 16; int mortar = bx < 2 || by < 2;
    float n = smooth_noise(x / 6.0f, y / 6.0f); int v = 170 + (int)(n * 50) - mortar * 60;
    *o = rgba(v, v - 10, v - 30, 255);
}
/* ---- placeholder scenery pictures (64x96) until the real ones arrive. (x, y) in pixels, y down. */
#define PW 64
#define PH 96
static int inrect(int x, int y, int x0, int y0, int x1, int y1) { return x >= x0 && x < x1 && y >= y0 && y < y1; }
static uint32_t stone(int x, int y, int r, int g, int b) { float n = smooth_noise(x / 5.0f, y / 5.0f); return rgba(r + (int)(n * 30), g + (int)(n * 25), b + (int)(n * 20), 255); }
static void px_house1(int x, int y, uint32_t *o) {       /* small flat-roofed stone house, one glowing window */
    *o = 0;
    if (inrect(x, y, 6, 30, 58, 96)) *o = stone(x, y, 185, 160, 120);
    if (inrect(x, y, 4, 26, 60, 32)) *o = stone(x, y, 140, 115, 85);                     /* roof edge */
    if (inrect(x, y, 26, 62, 38, 96)) *o = rgba(70, 45, 25, 255);                          /* door */
    if (inrect(x, y, 42, 44, 52, 56)) *o = rgba(255, 190, 90, 255);                        /* window */
}
static void px_house2(int x, int y, uint32_t *o) {       /* two-story house with stairs, two windows */
    *o = 0;
    if (inrect(x, y, 4, 8, 60, 96)) *o = stone(x, y, 175, 150, 115);
    if (inrect(x, y, 2, 4, 62, 10)) *o = stone(x, y, 130, 105, 80);
    if (inrect(x, y, 10, 20, 20, 32)) *o = rgba(255, 190, 90, 255);
    if (inrect(x, y, 42, 20, 52, 32)) *o = rgba(255, 180, 80, 255);
    if (inrect(x, y, 26, 66, 38, 96)) *o = rgba(70, 45, 25, 255);
    for (int s = 0; s < 6; s++) if (inrect(x, y, 44 + s * 3, 50 + s * 7, 64, 56 + s * 7)) *o = stone(x, y, 150, 130, 100);   /* outside stairs */
}
static void px_inn(int x, int y, uint32_t *o) {          /* wide inn with an arched door and a sign */
    *o = 0;
    if (inrect(x, y, 2, 24, 62, 96)) *o = stone(x, y, 190, 165, 125);
    if (inrect(x, y, 0, 20, 64, 26)) *o = stone(x, y, 120, 95, 70);
    { int dx = x - 32, dy = y - 70; if (dy >= 0 && abs(dx) < 10) *o = rgba(255, 200, 110, 255); if (dy < 0 && dx * dx + dy * dy < 100) *o = rgba(255, 200, 110, 255); }
    if (inrect(x, y, 8, 40, 18, 52) || inrect(x, y, 46, 40, 56, 52)) *o = rgba(255, 190, 90, 255);
    if (inrect(x, y, 22, 30, 42, 38)) *o = rgba(110, 70, 35, 255);                         /* sign */
}
static void px_palm(int x, int y, uint32_t *o) {         /* tall date palm */
    *o = 0;
    float t = (96 - y) / 96.0f; int cx = 32 + (int)(10 * t * t);
    if (y > 28 && abs(x - cx) < 3 + (int)(2 * (1 - t))) *o = rgba(120 - (int)(y / 3), 80 - (int)(y / 4), 40, 255);
    float dx = x - 42, dy = y - 24;
    for (int f = 0; f < 7; f++) { float a = -2.8f + f * 0.9f; float px2 = dx * cosf(a) + dy * sinf(a), py2 = -dx * sinf(a) + dy * cosf(a);
        if (px2 > 0 && px2 < 24 && fabsf(py2) < 2.5f + (24 - px2) * 0.12f) *o = rgba(40 + (int)(px2 * 2), 120 + (int)(px2 * 2), 40, 255); }
}
static void px_olive(int x, int y, uint32_t *o) {        /* gnarled olive tree */
    *o = 0;
    if (y > 60 && abs(x - 32 - (int)(4 * sinf(y * 0.3f))) < 4) *o = rgba(90, 70, 45, 255);
    float dx = (x - 32) / 28.0f, dy = (y - 40) / 26.0f; float d = dx * dx + dy * dy; float n = smooth_noise(x / 4.0f, y / 4.0f);
    if (d < 1.0f && n > 0.25f) *o = rgba(110 + (int)(n * 50), 140 + (int)(n * 50), 100 + (int)(n * 30), 255);
}
static void px_lantern(int x, int y, uint32_t *o) {      /* lantern post */
    *o = 0;
    if (abs(x - 32) < 2 && y > 20) *o = rgba(80, 55, 30, 255);
    if (inrect(x, y, 24, 8, 40, 24)) *o = rgba(255, 205, 110, 255);
    if (inrect(x, y, 22, 6, 42, 9) || inrect(x, y, 22, 23, 42, 26)) *o = rgba(60, 40, 20, 255);
}
static void px_well(int x, int y, uint32_t *o) {         /* stone well with a little roof */
    *o = 0;
    if (inrect(x, y, 8, 60, 56, 96)) *o = stone(x, y, 150, 140, 125);
    if (abs(x - 14) < 2 || abs(x - 50) < 2) if (y > 24 && y < 62) *o = rgba(90, 65, 35, 255);
    if (y >= 14 && y < 26 && abs(x - 32) < 30 - (26 - y) * 2) *o = rgba(150, 90, 50, 255);
}
static void px_stable(int x, int y, uint32_t *o) {       /* open stable with a glowing inside */
    *o = 0;
    int roof_y = 20 + abs(x - 32);
    if (y >= roof_y && y < roof_y + 10) *o = rgba(170, 130, 70, 255);
    if (y >= roof_y + 10 && y < 96 && x > 6 && x < 58) *o = rgba(255, 220, 150, 255);
    if (y >= roof_y + 10 && y < 96 && (x < 12 || x > 52)) *o = rgba(110, 75, 40, 255);
    if (inrect(x, y, 24, 72, 40, 86)) *o = rgba(200, 150, 60, 255);                        /* manger */
}
static void px_sheep(int x, int y, uint32_t *o) {        /* three sheep */
    *o = 0;
    for (int s = 0; s < 3; s++) { int cx = 12 + s * 20, cy = 60 + (s & 1) * 10; float dx = (x - cx) / 10.0f, dy = (y - cy) / 7.0f;
        if (dx * dx + dy * dy < 1.0f) *o = rgba(235, 235, 225, 255);
        if (inrect(x, y, cx + 6, cy - 4, cx + 12, cy + 2)) *o = rgba(40, 35, 30, 255);
        if (inrect(x, y, cx - 6, cy + 6, cx - 3, cy + 14) || inrect(x, y, cx + 3, cy + 6, cx + 6, cy + 14)) *o = rgba(40, 35, 30, 255); }
}
static void px_star(int x, int y, uint32_t *o) {         /* four-pointed star with a glow; brightness is the alpha */
    float dx = (x - 32) / 32.0f, dy = (y - 32) / 32.0f; float d = sqrtf(dx * dx + dy * dy);
    float rays = fmaxf(0, 1 - fabsf(dx) * 6) * fmaxf(0, 1 - fabsf(dy) * 1.2f) + fmaxf(0, 1 - fabsf(dy) * 6) * fmaxf(0, 1 - fabsf(dx) * 1.2f);
    float halo = fmaxf(0, 1 - d * 1.6f); float v = clampf(rays + halo * halo * 0.7f, 0, 1);
    *o = rgba(255, 245, 200, (int)(v * 255));
}
static void px_sky_night(int x, int y, uint32_t *o) {    /* dark blue sky with stars */
    float t = y / 255.0f; int r = (int)(6 + 30 * t), g = (int)(8 + 30 * t), b = (int)(40 + 70 * t);
    float n = noise2(x, y); if (n > 0.985f && t < 0.75f) { int s = (int)((n - 0.985f) / 0.015f * 180) + 70; r += s; g += s; b += s; }
    *o = rgba(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b, 255);
}
static void px_tree(int x, int y, uint32_t *o) {       /* 64x64 cypress-like tree on transparent background */
    float cx = (x - 32) / 32.0f, cy = (y - 44) / 40.0f; float d = cx * cx * 2.2f + cy * cy;
    int trunk = (x > 28 && x < 36 && y > 50);
    if (trunk) { *o = rgba(90, 60, 30, 255); return; }
    if (d < 0.9f && y > 2) { float n = smooth_noise(x / 5.0f, y / 5.0f); *o = rgba(30 + (int)(n * 40), 90 + (int)(n * 70), 35 + (int)(n * 30), 255); return; }
    *o = rgba(0, 0, 0, 0);
}
static void px_blob(int x, int y, uint32_t *o) { float dx = (x - 32) / 32.0f, dy = (y - 32) / 32.0f; float d = sqrtf(dx * dx + dy * dy); float a = d > 1 ? 0 : (1 - d) * (1 - d); *o = rgba(255, 255, 255, (int)(a * 255)); }
static void px_flag(int x, int y, uint32_t *o) { int c = ((x / 8) + (y / 8)) & 1; *o = c ? rgba(245, 245, 245, 255) : rgba(20, 20, 20, 255); }
static float sky_noise(float x, float y) {              /* repeats every 256 pixels in x so the sky ring has no seams */
    float a = smooth_noise(x / 40.0f, y / 20.0f) * 0.6f + smooth_noise(x / 15.0f, y / 8.0f) * 0.4f;
    float b = smooth_noise((x + 256) / 40.0f, y / 20.0f) * 0.6f + smooth_noise((x + 256) / 15.0f, y / 8.0f) * 0.4f;
    float w = x / 256.0f; return a * (1 - w) + b * w;
}
static float g_sky_wrap = 3.0f;   /* how many times the sky picture goes around: a wide painted sky goes around once */
static float g_skytop[3] = {0.31f, 0.59f, 0.9f}, g_skyhor[3] = {0.78f, 0.9f, 0.98f};   /* set from the road's colors before the sky is drawn */
static void px_sky(int x, int y, uint32_t *o) {        /* vertical gradient (road colors) with a few soft clouds */
    float t = y / 255.0f; float n = sky_noise((float)x, (float)y);
    int r = (int)(255 * (g_skytop[0] + (g_skyhor[0] - g_skytop[0]) * t)), g = (int)(255 * (g_skytop[1] + (g_skyhor[1] - g_skytop[1]) * t)), b = (int)(255 * (g_skytop[2] + (g_skyhor[2] - g_skytop[2]) * t));
    float cloud = clampf((n - 0.55f) * 4.0f, 0, 1) * (t > 0.25f && t < 0.75f ? 1.0f : 0.0f) * 0.8f;
    r = (int)(r + (250 - r) * cloud); g = (int)(g + (250 - g) * cloud); b = (int)(b + (255 - b) * cloud);
    *o = rgba(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b, 255);
}

// ------------------------------------------------------------------ track meshes
static Vec3 road_point(const Track *t, int i, float lat) {
    const TrackSample *s = &t->s[i];
    return v3(s->x + s->nx * lat, track_surface_height(t, i, lat), s->z + s->nz * lat);
}
static Vec3 face_normal(Vec3 a, Vec3 b, Vec3 c) { return v3_norm(v3_cross(v3_sub(b, a), v3_sub(c, a))); }
static void strip_quad(Batch *b, const Track *t, int i, float l0, float l1, float vscale, float r, float g, float bl) {
    int j = (i + 1) % t->n;
    Vec3 p0 = road_point(t, i, l0), p1 = road_point(t, i, l1), p2 = road_point(t, j, l1), p3 = road_point(t, j, l0);
    Vec3 n = face_normal(p0, p1, p3); if (n.y < 0) n = v3(-n.x, -n.y, -n.z);
    float v0 = t->s[i].dist * vscale, v1 = (i + 1 < t->n ? t->s[j].dist : t->length) * vscale;
    /* counter-clockwise seen from above: left-near, right-near, right-far, left-far */
    batch_quad4(b, p0, p1, p2, p3, n, 0, v0, 1, v1, r, g, bl, 1);
}
static void free_track_meshes(void) {
    Mesh **ms[] = {&m_brook, &m_tunnel, &m_arch, &m_coin, &m_coins, &m_finale, &m_boostpads, &m_bouncepads, &m_road, &m_shoulder, &m_wall, &m_startline, &m_ground, &m_sky, &m_skid, &m_dust, &m_spark, &m_hud, &m_kart_body, &m_kart_rest, &m_cube, &m_shield, &m_items, &m_shadow};
    for (size_t i = 0; i < sizeof ms / sizeof ms[0]; i++) { gfx_mesh_free(*ms[i]); *ms[i] = NULL; }
    for (int i = 1; i < NSTYLES; i++) { gfx_mesh_free(m_body_style[i]); gfx_mesh_free(m_rest_style[i]); m_body_style[i] = m_rest_style[i] = NULL; }
    gfx_mesh_free(m_driver_box); gfx_mesh_free(m_driver_head); m_driver_box = m_driver_head = NULL;
    for (int i = 0; i < NKARTS; i++) { gfx_mesh_free(m_driver_spr[i]); m_driver_spr[i] = NULL; }
}
static void build_track_meshes(void) {
    const Track *t = &g_track;
    batch_init(&g_b, g_vtmp, 8192);
    /* road */
    batch_clear(&g_b);
    for (int i = 0; i < t->n; i++) strip_quad(&g_b, t, i, -t->s[i].width * 0.5f, t->s[i].width * 0.5f, 1.0f / 8.0f, 1, 1, 1);
    m_road = gfx_mesh_static(g_b.v, g_b.n);
    /* shoulders (dirt) */
    batch_clear(&g_b);
    for (int i = 0; i < t->n; i++) {
        float h = t->s[i].width * 0.5f;
        strip_quad(&g_b, t, i, -h - SHOULDER_W, -h, 1.0f / 6.0f, 1, 1, 1);
        strip_quad(&g_b, t, i, h, h + SHOULDER_W, 1.0f / 6.0f, 1, 1, 1);
    }
    m_shoulder = gfx_mesh_static(g_b.v, g_b.n);
    /* low stone walls at the shoulder edge */
    batch_clear(&g_b);
    for (int i = 0; i < t->n; i++) {
        int j = (i + 1) % t->n; float e = track_edge(t, i), ej = track_edge(t, j);
        for (int side = -1; side <= 1; side += 2) {
            Vec3 a = road_point(t, i, side * e), b = road_point(t, j, side * ej);
            Vec3 at = v3(a.x, a.y + 0.9f, a.z), bt = v3(b.x, b.y + 0.9f, b.z);
            Vec3 n = v3(-t->s[i].nx * side, 0, -t->s[i].nz * side);
            float v0 = t->s[i].dist / 3.0f, v1 = v0 + 0.7f;
            if (side < 0) batch_quad4(&g_b, a, b, bt, at, n, v0, 0, v1, 1, 1, 1, 1, 1);
            else batch_quad4(&g_b, b, a, at, bt, n, v0, 0, v1, 1, 1, 1, 1, 1);
        }
    }
    m_wall = gfx_mesh_static(g_b.v, g_b.n);
    /* start line across the road */
    batch_clear(&g_b);
    { Vec3 p0 = road_point(t, 0, -t->s[0].width * 0.5f), p1 = road_point(t, 0, t->s[0].width * 0.5f), p2 = road_point(t, 2, t->s[2].width * 0.5f), p3 = road_point(t, 2, -t->s[2].width * 0.5f);
      p0.y += 0.03f; p1.y += 0.03f; p2.y += 0.03f; p3.y += 0.03f;
      batch_quad4(&g_b, p0, p1, p2, p3, v3(0, 1, 0), 0, 0, 4, 1, 1, 1, 1, 1); }
    m_startline = gfx_mesh_static(g_b.v, g_b.n);
    /* boost arrows and bounce pads painted on the road */
    batch_clear(&g_b);
    for (int p = 0; p < t->npad; p++) { const RoadPad *pd = &t->pad[p]; if (pd->kind != PAD_BOOST) continue;
        /* a chevron pointing down the road, made of two slanted bars */
        for (int bar = -1; bar <= 1; bar += 2) {
            Vec3 a = road_point(t, pd->idx, pd->lateral), b = road_point(t, (pd->idx + 2) % t->n, pd->lateral + bar * pd->halfw);
            Vec3 c = road_point(t, (pd->idx + 3) % t->n, pd->lateral + bar * pd->halfw), d = road_point(t, (pd->idx + 1) % t->n, pd->lateral);
            a.y += 0.04f; b.y += 0.04f; c.y += 0.04f; d.y += 0.04f;
            if (bar < 0) batch_quad4(&g_b, a, b, c, d, v3(0, 1, 0), 0, 0, 1, 1, 1, 0.85f, 0.2f, 1); else batch_quad4(&g_b, a, d, c, b, v3(0, 1, 0), 0, 0, 1, 1, 1, 0.85f, 0.2f, 1);
        } }
    m_boostpads = gfx_mesh_static(g_b.v, g_b.n);
    batch_clear(&g_b);
    for (int p = 0; p < t->npad; p++) { const RoadPad *pd = &t->pad[p]; if (pd->kind != PAD_BOUNCE) continue;
        Vec3 c = road_point(t, pd->idx, pd->lateral); c.y += 0.05f; const TrackSample *sm = &t->s[pd->idx];
        Vec3 ax2 = v3(sm->nx * pd->halfw, 0, sm->nz * pd->halfw), ay2 = v3(sm->tx * pd->halfw, 0, sm->tz * pd->halfw);
        batch_quad(&g_b, c, ax2, ay2, v3(0, 1, 0), 0, 0, 1, 1, 0.3f, 0.9f, 1.0f, 1);
        Vec3 ax3 = v3(ax2.x * 0.55f, 0, ax2.z * 0.55f), ay3 = v3(ay2.x * 0.55f, 0, ay2.z * 0.55f); c.y += 0.01f;
        batch_quad(&g_b, c, ax3, ay3, v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 1); }
    m_bouncepads = gfx_mesh_static(g_b.v, g_b.n);
    /* the finale picture (the gates of Heaven): a tall quad across the road, facing the racers */
    batch_clear(&g_b);
    if (g_def->finale_img) { int fi = g_def->finale_idx % t->n; const TrackSample *fs = &t->s[fi];
        float hw = g_def->finale_w * 0.5f, hh = g_def->finale_h;
        Vec3 c = v3(fs->x, fs->y - 2.0f, fs->z);
        batch_quad(&g_b, v3(c.x, c.y + hh * 0.5f, c.z), v3(fs->nx * hw, 0, fs->nz * hw), v3(0, hh * 0.5f, 0), v3(-fs->tx, 0, -fs->tz), 0, 0, 1, 1, 1, 1, 1, 1); }
    m_finale = gfx_mesh_static(g_b.v, g_b.n);
    /* tunnels: a roof over the road and walls up to it, with an arch ring at each end */
    batch_clear(&g_b);
    for (int tn = 0; tn < t->ntunnel; tn++) { const Tunnel *tu = &t->tunnel[tn]; int len = (tu->to - tu->from + t->n) % t->n;
        for (int q = 0; q <= len; q++) { int i = (tu->from + q) % t->n, j = (i + 1) % t->n; float e = track_edge(t, i), ej = track_edge(t, j);
            Vec3 a = road_point(t, i, -e), b = road_point(t, j, -ej), c = road_point(t, j, ej), d = road_point(t, i, e);
            Vec3 at = v3(a.x, a.y + tu->height, a.z), bt = v3(b.x, b.y + tu->height, b.z), ct = v3(c.x, c.y + tu->height, c.z), dt2 = v3(d.x, d.y + tu->height, d.z);
            float v0 = t->s[i].dist / 4.0f, v1 = t->s[j].dist / 4.0f;
            if (q < len) {
                batch_quad4(&g_b, at, dt2, ct, bt, v3(0, -1, 0), 0, v0, 2, v1, 0.45f, 0.42f, 0.5f, 1);                          /* roof, seen from below */
                batch_quad4(&g_b, a, b, bt, at, v3(t->s[i].nx, 0, t->s[i].nz), v0, 0, v1, 1.5f, 0.55f, 0.5f, 0.6f, 1);          /* left wall */
                batch_quad4(&g_b, d, dt2, ct, c, v3(-t->s[i].nx, 0, -t->s[i].nz), v0, 0, v1, 1.5f, 0.55f, 0.5f, 0.6f, 1);      /* right wall */
            }
            if (q == 0 || q == len) {    /* the arch ring: a band of stone around the mouth */
                for (int sd = -1; sd <= 1; sd += 2) { Vec3 base = road_point(t, i, sd * (e + 0.6f)); batch_box(&g_b, v3(base.x, base.y + tu->height * 0.5f, base.z), v3(0.7f, tu->height * 0.5f + 0.6f, 0.7f), 0.6f, 0.55f, 0.5f); }
                Vec3 mid = road_point(t, i, 0); batch_box(&g_b, v3(mid.x, mid.y + tu->height + 0.5f, mid.z), v3(e + 1.3f, 0.6f, 0.7f), 0.6f, 0.55f, 0.5f);
            } } }
    m_tunnel = gfx_mesh_static(g_b.v, g_b.n);
    /* the brook across the Valley of Elah (battle arena only) */
    batch_clear(&g_b);
    if (g_def->bonus == 2) { float hw = 3.0f; for (int q = 0; q < 34; q++) { float x0 = BROOK_X0 + (BROOK_X1 - BROOK_X0) * q / 34.0f, x1 = BROOK_X0 + (BROOK_X1 - BROOK_X0) * (q + 1) / 34.0f;
        float wob0 = sinf(q * 0.9f) * 1.2f, wob1 = sinf((q + 1) * 0.9f) * 1.2f;
        batch_quad4(&g_b, v3(x0, 0.06f, BROOK_Z + wob0 - hw), v3(x1, 0.06f, BROOK_Z + wob1 - hw), v3(x1, 0.06f, BROOK_Z + wob1 + hw), v3(x0, 0.06f, BROOK_Z + wob0 + hw), v3(0, 1, 0), q * 0.5f, 0, q * 0.5f + 0.5f, 1, 1, 1, 1, 0.8f); } }
    m_brook = gfx_mesh_static(g_b.v, g_b.n);
    /* the start/finish arch: two pillars and a checkered banner across the road */
    batch_clear(&g_b);
    { int i = 0; float e = track_edge(t, i);
      for (int sd = -1; sd <= 1; sd += 2) { Vec3 base = road_point(t, i, sd * (e + 0.5f)); batch_box(&g_b, v3(base.x, base.y + 3.2f, base.z), v3(0.45f, 3.2f, 0.45f), 0.95f, 0.85f, 0.4f); }
      Vec3 mid = road_point(t, i, 0); batch_box(&g_b, v3(mid.x, mid.y + 6.1f, mid.z), v3(e + 1.0f, 0.75f, 0.3f), 1, 1, 1); }
    m_arch = gfx_mesh_static(g_b.v, g_b.n);
    /* a coin: a thin golden eight-sided disc, drawn spinning at every coin spot */
    batch_clear(&g_b);
    { Vec3 c = v3(0, 0, 0); float rad = 0.55f, hw = 0.06f;
      for (int q = 0; q < 8; q++) { float a0 = q * 6.2831853f / 8, a1 = (q + 1) * 6.2831853f / 8;
          Vec3 p0 = v3(cosf(a0) * rad, sinf(a0) * rad, hw), p1 = v3(cosf(a1) * rad, sinf(a1) * rad, hw), p2 = v3(cosf(a1) * rad, sinf(a1) * rad, -hw), p3 = v3(cosf(a0) * rad, sinf(a0) * rad, -hw);
          batch_quad4(&g_b, p0, p1, p2, p3, v3(cosf((a0 + a1) * 0.5f), sinf((a0 + a1) * 0.5f), 0), 0, 0, 1, 1, 0.9f, 0.7f, 0.2f, 1);
          Vertex f0 = {c.x, c.y, hw, 0, 0, 1, 0, 0, 1.0f, 0.85f, 0.35f, 1}, f1 = {p0.x, p0.y, hw, 0, 0, 1, 0, 0, 1.0f, 0.85f, 0.35f, 1}, f2 = {p1.x, p1.y, hw, 0, 0, 1, 0, 0, 1.0f, 0.85f, 0.35f, 1};
          Vertex b0 = {c.x, c.y, -hw, 0, 0, -1, 0, 0, 1.0f, 0.85f, 0.35f, 1}, b1 = {p1.x, p1.y, -hw, 0, 0, -1, 0, 0, 1.0f, 0.85f, 0.35f, 1}, b2 = {p0.x, p0.y, -hw, 0, 0, -1, 0, 0, 1.0f, 0.85f, 0.35f, 1};
          if (g_b.n + 6 <= g_b.cap) { g_b.v[g_b.n++] = f0; g_b.v[g_b.n++] = f1; g_b.v[g_b.n++] = f2; g_b.v[g_b.n++] = b0; g_b.v[g_b.n++] = b1; g_b.v[g_b.n++] = b2; } } }
    m_coin = gfx_mesh_static(g_b.v, g_b.n);
    m_coins = gfx_mesh_dynamic(MAX_COINS * 6);
    /* ground: a big grass plane under everything */
    batch_clear(&g_b);
    { float lowest = 1e9f; for (int i = 0; i < t->n; i++) { float y = track_surface_height(t, i, track_edge(t, i)); if (y < lowest) lowest = y; y = track_surface_height(t, i, -track_edge(t, i)); if (y < lowest) lowest = y; }
      batch_quad(&g_b, v3(0, lowest - 0.6f, 0), v3(600, 0, 0), v3(0, 0, -600), v3(0, 1, 0), 0, 0, 120, 120, 1, 1, 1, 1); }
    m_ground = gfx_mesh_static(g_b.v, g_b.n);
    /* sky: a big cylinder-ish ring far away (drawn without depth) */
    batch_clear(&g_b);
    for (int k = 0; k < 24; k++) {
        float a0 = k * 6.2831853f / 24, a1 = (k + 1) * 6.2831853f / 24, R = 900.0f;
        float top = g_sky_wrap < 1.5f ? -60 + 6.2831853f * R / 4.0f : 420;      /* a painted 4:1 sky goes around once, so the ring is a quarter of its circumference tall to keep the clouds in shape */
        Vec3 p0 = v3(cosf(a0) * R, -60, sinf(a0) * R), p1 = v3(cosf(a1) * R, -60, sinf(a1) * R), p2 = v3(cosf(a1) * R, top, sinf(a1) * R), p3 = v3(cosf(a0) * R, top, sinf(a0) * R);
        batch_quad4(&g_b, p1, p0, p3, p2, v3(0, 1, 0), k / 24.0f * g_sky_wrap, 1, (k + 1) / 24.0f * g_sky_wrap, 0, 1, 1, 1, 1);
    }
    m_sky = gfx_mesh_static(g_b.v, g_b.n);
    m_skid = gfx_mesh_dynamic(MAX_SKID * 6);
    m_dust = gfx_mesh_dynamic(MAX_DUST * 6);
    m_spark = gfx_mesh_dynamic(MAX_SPARK * 6);
    m_hud = gfx_mesh_dynamic(8192);
    /* placeholder kart: boxes until the character art arrives. The body is white and tinted with the character's color. */
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.42f, 0), v3(0.75f, 0.17f, 1.05f), 1, 1, 1);                    /* body (tinted) */
    batch_box(&g_b, v3(0, 0.48f, 0.95f), v3(0.6f, 0.12f, 0.2f), 1, 1, 1);                  /* front bumper */
    m_kart_body = gfx_mesh_static(g_b.v, g_b.n); m_body_style[0] = m_kart_body;
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.62f, -0.15f), v3(0.55f, 0.08f, 0.55f), 0.2f, 0.2f, 0.25f);     /* seat */
    batch_box(&g_b, v3(0, 0.62f, 0.75f), v3(0.35f, 0.06f, 0.2f), 0.9f, 0.9f, 0.95f);        /* a little hood */
    batch_box(&g_b, v3(-0.45f, 0.42f, -1.1f), v3(0.07f, 0.07f, 0.12f), 0.3f, 0.3f, 0.32f);  /* exhaust */
    for (int wx = -1; wx <= 1; wx += 2) for (int wz = -1; wz <= 1; wz += 2)
        batch_wheel(&g_b, v3(wx * 0.85f, 0.32f, wz * 0.78f), 0.15f, 0.32f, 0.1f, 0.1f, 0.11f);
    m_kart_rest = gfx_mesh_static(g_b.v, g_b.n); m_rest_style[0] = m_kart_rest;
    /* CHARIOT: a low open car on two big back wheels, a rail in front */
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.45f, -0.2f), v3(0.6f, 0.12f, 0.8f), 1, 1, 1);
    batch_box(&g_b, v3(0, 0.85f, 0.55f), v3(0.6f, 0.3f, 0.06f), 1, 1, 1);                   /* the front wall */
    m_body_style[1] = gfx_mesh_static(g_b.v, g_b.n);
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.5f, 1.1f), v3(0.08f, 0.08f, 0.5f), 0.75f, 0.6f, 0.3f);           /* the shaft, gold */
    for (int wx = -1; wx <= 1; wx += 2) batch_wheel(&g_b, v3(wx * 0.8f, 0.5f, -0.5f), 0.1f, 0.5f, 0.75f, 0.55f, 0.15f);   /* big gold wheels */
    for (int wx = -1; wx <= 1; wx += 2) batch_wheel(&g_b, v3(wx * 0.7f, 0.25f, 0.8f), 0.1f, 0.25f, 0.1f, 0.1f, 0.11f);
    m_rest_style[1] = gfx_mesh_static(g_b.v, g_b.n);
    /* BOAT: a hull with a pointed bow and a little mast */
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.45f, -0.1f), v3(0.7f, 0.2f, 0.9f), 1, 1, 1);
    batch_box(&g_b, v3(0, 0.5f, 1.0f), v3(0.4f, 0.18f, 0.35f), 1, 1, 1);
    batch_box(&g_b, v3(0, 0.55f, 1.35f), v3(0.15f, 0.15f, 0.2f), 1, 1, 1);                   /* bow */
    m_body_style[2] = gfx_mesh_static(g_b.v, g_b.n);
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 1.3f, 0.5f), v3(0.05f, 0.7f, 0.05f), 0.5f, 0.35f, 0.2f);           /* mast */
    batch_box(&g_b, v3(0.3f, 1.4f, 0.5f), v3(0.3f, 0.45f, 0.02f), 0.95f, 0.93f, 0.85f);       /* sail */
    for (int wx = -1; wx <= 1; wx += 2) for (int wz = -1; wz <= 1; wz += 2)
        batch_wheel(&g_b, v3(wx * 0.8f, 0.3f, wz * 0.7f), 0.12f, 0.3f, 0.1f, 0.1f, 0.11f);
    m_rest_style[2] = gfx_mesh_static(g_b.v, g_b.n);
    /* ARK: a big wooden box with a roof */
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.6f, 0), v3(0.85f, 0.35f, 1.1f), 1, 1, 1);
    m_body_style[3] = gfx_mesh_static(g_b.v, g_b.n);
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 1.1f, 0.5f), v3(0.9f, 0.12f, 0.6f), 0.45f, 0.3f, 0.18f);           /* the roof over the front */
    batch_box(&g_b, v3(0, 1.4f, 0.5f), v3(0.5f, 0.2f, 0.4f), 0.5f, 0.35f, 0.2f);
    for (int wx = -1; wx <= 1; wx += 2) for (int wz = -1; wz <= 1; wz += 2)
        batch_wheel(&g_b, v3(wx * 0.95f, 0.32f, wz * 0.85f), 0.15f, 0.32f, 0.1f, 0.1f, 0.11f);
    m_rest_style[3] = gfx_mesh_static(g_b.v, g_b.n);
    /* SLING: a slim shepherd's cart with a leather sling pouch on the back */
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.42f, 0.1f), v3(0.6f, 0.15f, 1.0f), 1, 1, 1);
    batch_box(&g_b, v3(0, 0.5f, 1.0f), v3(0.45f, 0.1f, 0.15f), 1, 1, 1);
    m_body_style[4] = gfx_mesh_static(g_b.v, g_b.n);
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.62f, -0.15f), v3(0.5f, 0.08f, 0.5f), 0.45f, 0.3f, 0.18f);       /* leather seat */
    batch_box(&g_b, v3(0, 0.75f, -1.05f), v3(0.35f, 0.25f, 0.15f), 0.55f, 0.38f, 0.22f);    /* the pouch */
    batch_box(&g_b, v3(0, 0.8f, -1.05f), v3(0.12f, 0.12f, 0.12f), 0.55f, 0.52f, 0.5f);      /* a stone in it */
    for (int wx = -1; wx <= 1; wx += 2) for (int wz = -1; wz <= 1; wz += 2)
        batch_wheel(&g_b, v3(wx * 0.75f, 0.3f, wz * 0.75f), 0.12f, 0.3f, 0.1f, 0.1f, 0.11f);
    m_rest_style[4] = gfx_mesh_static(g_b.v, g_b.n);
    /* the box driver, used for characters without a picture yet */
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 1.0f, -0.25f), v3(0.22f, 0.3f, 0.2f), 0.9f, 0.9f, 0.9f);          /* robe (tinted) */
    m_driver_box = gfx_mesh_static(g_b.v, g_b.n);
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 1.48f, -0.25f), v3(0.19f, 0.19f, 0.19f), 0.98f, 0.82f, 0.65f);   /* head */
    m_driver_head = gfx_mesh_static(g_b.v, g_b.n);
    for (int i = 0; i < NKARTS; i++) m_driver_spr[i] = gfx_mesh_dynamic(6);
    /* item box: a cube */
    batch_clear(&g_b); batch_box(&g_b, v3(0, 0, 0), v3(0.65f, 0.65f, 0.65f), 1, 1, 1); m_cube = gfx_mesh_static(g_b.v, g_b.n);
    /* shield: a bigger cube drawn transparent around the kart */
    batch_clear(&g_b); batch_box(&g_b, v3(0, 0.8f, 0), v3(1.4f, 1.2f, 1.6f), 1, 1, 1); m_shield = gfx_mesh_static(g_b.v, g_b.n);
    m_items = gfx_mesh_dynamic(MAX_PROJ * 6 + 6);
    m_shadow = gfx_mesh_dynamic((NKARTS + MAX_MOVERS + 1) * 6);
}

// ------------------------------------------------------------------ theme: textures and scenery pictures for the current road
/* placeholders for the other roads */
static void px_hut(int x, int y, uint32_t *o) {          /* fisherman's hut with a reed roof */
    *o = 0;
    if (inrect(x, y, 8, 44, 56, 96)) *o = stone(x, y, 200, 180, 140);
    if (y >= 20 && y < 48 && x >= 4 + (48 - y) && x < 60 - (48 - y)) *o = rgba(150 + (y & 3) * 8, 120, 60, 255);
    if (inrect(x, y, 26, 66, 38, 96)) *o = rgba(80, 55, 30, 255);
}
static void px_net(int x, int y, uint32_t *o) {          /* a fishing net hung between two poles */
    *o = 0;
    if (inrect(x, y, 6, 30, 10, 96) || inrect(x, y, 54, 30, 58, 96)) *o = rgba(110, 80, 40, 255);
    if (inrect(x, y, 10, 36, 54, 80) && (((x + y) % 7) == 0 || ((x - y + 64) % 7) == 0)) *o = rgba(230, 220, 180, 255);
}
static void px_rock(int x, int y, uint32_t *o) {         /* a round boulder */
    *o = 0; float dx = (x - 32) / 28.0f, dy = (y - 66) / 28.0f;
    if (dx * dx + dy * dy < 1.0f) *o = stone(x, y, 130 - (int)(dy * 30), 125 - (int)(dy * 30), 120 - (int)(dy * 30));
}
static void px_boat(int x, int y, uint32_t *o) {         /* a small wooden fishing boat with a mast */
    *o = 0;
    if (inrect(x, y, 30, 20, 34, 70)) *o = rgba(120, 85, 45, 255);
    if (inrect(x, y, 34, 24, 56, 60) && (x - 34) < (60 - y) * 1.0f) *o = rgba(240, 235, 215, 255);     /* sail */
    if (y >= 70 && y < 86 && x >= 6 + (y - 70) && x < 58 - (y - 70)) *o = rgba(140, 95, 50, 255);
    if (inrect(x, y, 6, 66, 58, 71)) *o = rgba(90, 60, 30, 255);
}
static void px_cypress(int x, int y, uint32_t *o) {      /* tall dark cypress */
    *o = 0; float w = 6 + (y / 96.0f) * 14; if (y > 86) w = 3;
    if (fabsf((float)(x - 32)) < w && y > 4) *o = rgba(30 + (y / 3), 70 + (y / 4), 35, 255);
}
static void px_pillar(int x, int y, uint32_t *o) {       /* a stone column */
    *o = 0;
    if (inrect(x, y, 24, 14, 40, 90)) *o = stone(x, y, 215, 205, 180);
    if (inrect(x, y, 18, 6, 46, 14) || inrect(x, y, 18, 88, 46, 96)) *o = stone(x, y, 190, 180, 155);
}
static void px_tower(int x, int y, uint32_t *o) {        /* a city wall tower with battlements */
    *o = 0;
    if (inrect(x, y, 10, 14, 54, 96)) *o = stone(x, y, 200, 180, 140);
    if (inrect(x, y, 10, 6, 54, 14) && ((x / 8) & 1)) *o = stone(x, y, 190, 170, 130);
    if (inrect(x, y, 28, 40, 36, 52)) *o = rgba(40, 30, 20, 255);
}
static void px_wallseg(int x, int y, uint32_t *o) {      /* a stretch of city wall */
    *o = 0;
    if (inrect(x, y, 0, 30, 64, 96)) *o = stone(x, y, 200, 180, 140);
    if (inrect(x, y, 0, 22, 64, 30) && ((x / 8) & 1)) *o = stone(x, y, 190, 170, 130);
}
static void px_arch(int x, int y, uint32_t *o) {         /* the city gate: an arch */
    *o = 0; float dx = (x - 32) / 18.0f, dy = (y - 48) / 24.0f;
    if (inrect(x, y, 4, 10, 60, 96)) *o = stone(x, y, 205, 185, 145);
    if (inrect(x, y, 4, 2, 60, 10) && ((x / 8) & 1)) *o = stone(x, y, 190, 170, 130);
    if ((dx * dx + dy * dy < 1.0f && y > 30) || inrect(x, y, 14, 48, 50, 96)) *o = 0;      /* the opening */
}
static void px_stall(int x, int y, uint32_t *o) {        /* a market stall with a striped awning */
    *o = 0;
    if (inrect(x, y, 6, 40, 58, 50)) *o = ((x / 6) & 1) ? rgba(220, 60, 50, 255) : rgba(245, 240, 220, 255);
    if (inrect(x, y, 8, 50, 12, 96) || inrect(x, y, 52, 50, 56, 96)) *o = rgba(120, 85, 45, 255);
    if (inrect(x, y, 8, 70, 56, 96)) *o = rgba(170, 120, 60, 255);
    if (inrect(x, y, 12, 62, 52, 70)) *o = rgba(220, 150, 60, 255);                      /* goods */
}
static void px_cart(int x, int y, uint32_t *o) {         /* a wooden hand cart */
    *o = 0;
    if (inrect(x, y, 10, 50, 54, 74)) *o = rgba(150, 100, 50, 255);
    if (inrect(x, y, 14, 40, 50, 50)) *o = rgba(210, 170, 90, 255);                      /* a load of grain */
    float dx = (x - 20) / 12.0f, dy = (y - 80) / 12.0f; if (dx * dx + dy * dy < 1.0f && dx * dx + dy * dy > 0.5f) *o = rgba(80, 55, 30, 255);
    dx = (x - 44) / 12.0f; if (dx * dx + dy * dy < 1.0f && dx * dx + dy * dy > 0.5f) *o = rgba(80, 55, 30, 255);
}
static void px_lily(int x, int y, uint32_t *o) {         /* white lilies on green stems */
    *o = 0;
    for (int f = 0; f < 3; f++) { int cx = 14 + f * 18, cy = 40 + (f & 1) * 10;
        if (inrect(x, y, cx - 1, cy, cx + 2, 96)) *o = rgba(60, 130, 50, 255);
        float dx = (x - cx) / 8.0f, dy = (y - cy) / 7.0f; if (dx * dx + dy * dy < 1.0f) *o = rgba(250, 250, 240, 255);
        if (dx * dx + dy * dy < 0.12f) *o = rgba(250, 210, 80, 255); }
}
static void px_tomb(int x, int y, uint32_t *o) {         /* the rock tomb with the stone rolled aside and light inside */
    *o = 0; float dx = (x - 36) / 26.0f, dy = (y - 70) / 34.0f;
    if (dx * dx + dy * dy < 1.0f) *o = stone(x, y, 160, 150, 135);
    float ex = (x - 36) / 9.0f, ey = (y - 80) / 14.0f; if (ex * ex + ey * ey < 1.0f) *o = rgba(255, 245, 200, 255);   /* light from inside */
    float sx2 = (x - 8) / 7.0f, sy2 = (y - 86) / 8.0f; if (sx2 * sx2 + sy2 * sy2 < 1.0f) *o = stone(x, y, 140, 130, 120);   /* the stone, rolled away */
}
static void px_crosses(int x, int y, uint32_t *o) {      /* three crosses on the far hill */
    *o = 0;
    for (int c = 0; c < 3; c++) { int cx = 12 + c * 20, top = c == 1 ? 10 : 24;
        if (inrect(x, y, cx - 2, top, cx + 2, 96)) *o = rgba(70, 50, 35, 255);
        if (inrect(x, y, cx - 9, top + 12, cx + 9, top + 16)) *o = rgba(70, 50, 35, 255); }
}
static void px_cloudpuff(int x, int y, uint32_t *o) {    /* a puffy cloud (scenery) */
    *o = 0;
    for (int c = 0; c < 4; c++) { static const int cx[4] = {18, 32, 46, 30}, cy[4] = {70, 60, 70, 76}, cr[4] = {14, 18, 14, 20};
        float dx = (float)(x - cx[c]) / cr[c], dy = (float)(y - cy[c]) / (cr[c] * 0.8f); if (dx * dx + dy * dy < 1.0f) *o = rgba(245, 246, 252, 255); }
}
static void px_gpillar(int x, int y, uint32_t *o) {      /* a golden column with a flame on top */
    *o = 0;
    if (inrect(x, y, 26, 22, 38, 92)) *o = rgba(235 + ((x & 2) ? 0 : 15), 195, 80, 255);
    if (inrect(x, y, 20, 14, 44, 22) || inrect(x, y, 20, 90, 44, 96)) *o = rgba(210, 165, 50, 255);
    float dx = (x - 32) / 7.0f, dy = (y - 8) / 8.0f; if (dx * dx + dy * dy < 1.0f) *o = rgba(255, 240, 180, 255);
}
static void px_crystal(int x, int y, uint32_t *o) {      /* a cluster of purple crystals */
    *o = 0;
    for (int c = 0; c < 3; c++) { static const int cx[3] = {20, 34, 46}, top[3] = {30, 14, 40}, hw[3] = {8, 10, 7};
        int h = 96 - top[c]; if (y < top[c]) continue; float w = (float)hw[c] * (float)(y - top[c]) / (float)h;
        if (fabsf((float)(x - cx[c])) < w) *o = rgba(170 + ((x + y) & 8 ? 40 : 0), 80, 220 + ((x - y) & 8 ? 30 : 0), 255); }
}
static void px_waterfall(int x, int y, uint32_t *o) {    /* falling water: vertical streaks, tiles vertically */
    float n = smooth_noise(x / 6.0f, y / 40.0f) * 0.7f + smooth_noise(x / 2.5f, y / 12.0f) * 0.3f;
    int a = x < 4 || x > 59 ? 0 : 230; int w = n > 0.6f ? 60 : 0;
    *o = rgba(170 + w, 215 + w / 2, 255, a);
}
static void px_person(int x, int y, uint32_t *o, int r, int g, int b, int helmet) {   /* a simple standing soldier */
    *o = 0;
    float dx = (x - 32) / 7.0f, dy = (y - 18) / 8.0f; if (dx * dx + dy * dy < 1.0f) *o = helmet && y < 18 ? rgba(170, 150, 90, 255) : rgba(225, 180, 140, 255);
    if (inrect(x, y, 22, 26, 42, 66)) *o = rgba(r, g, b, 255);
    if (inrect(x, y, 14, 28, 22, 54) || inrect(x, y, 42, 28, 50, 54)) *o = rgba(225, 180, 140, 255);
    if (inrect(x, y, 24, 66, 31, 96) || inrect(x, y, 33, 66, 40, 96)) *o = rgba(110, 80, 50, 255);
    if (helmet && inrect(x, y, 48, 6, 52, 70)) *o = rgba(120, 90, 50, 255);      /* a spear */
}
static void px_israelite(int x, int y, uint32_t *o) { px_person(x, y, o, 200, 190, 160, 0); }
static void px_philistine(int x, int y, uint32_t *o) { px_person(x, y, o, 150, 40, 40, 1); }
static void px_banner(int x, int y, uint32_t *o, int r, int g, int b) { *o = 0; if (inrect(x, y, 30, 4, 34, 96)) *o = rgba(110, 80, 50, 255); if (inrect(x, y, 34, 8, 60, 44)) *o = rgba(r, g, b, 255); }
static void px_banner_israel(int x, int y, uint32_t *o) { px_banner(x, y, o, 40, 70, 200); }
static void px_banner_philistine(int x, int y, uint32_t *o) { px_banner(x, y, o, 180, 40, 40); }
static void px_tent(int x, int y, uint32_t *o) { *o = 0; if (y > 40 && x > 6 + (96 - y) / 2 - 20 && x < 58 - (96 - y) / 2 + 20 && fabsf((float)(x - 32)) < (float)(y - 40) * 0.6f + 2) *o = rgba(170 + (y & 4), 140, 100, 255); if (inrect(x, y, 30, 70, 34, 96)) *o = rgba(60, 40, 25, 255); }
static void px_stone(int x, int y, uint32_t *o) {        /* a rolling round stone */
    *o = 0; float dx = (x - 32) / 30.0f, dy = (y - 64) / 30.0f;
    if (dx * dx + dy * dy < 1.0f) *o = stone(x, y, 150 - (int)(dy * 40), 140 - (int)(dy * 40), 130 - (int)(dy * 40));
}
static Texture *placeholder_sprite(const char *id, float *aspect) {
    *aspect = (float)PW / (float)PH;
    static const struct { const char *id; void (*fn)(int, int, uint32_t *); } table[] = {
        {"#hut", px_hut}, {"#net", px_net}, {"#rock", px_rock}, {"#boat", px_boat}, {"#cypress", px_cypress}, {"#pillar", px_pillar},
        {"#tower", px_tower}, {"#wallseg", px_wallseg}, {"#arch", px_arch}, {"#stall", px_stall}, {"#cart", px_cart}, {"#lily", px_lily},
        {"#tomb", px_tomb}, {"#crosses", px_crosses}, {"#stone", px_stone}, {"#cloud", px_cloudpuff}, {"#gpillar", px_gpillar}, {"#crystal", px_crystal},
        {"#israelite", px_israelite}, {"#philistine", px_philistine}, {"#banner_israel", px_banner_israel}, {"#banner_philistine", px_banner_philistine}, {"#tent", px_tent}, {"#camp", px_tent} };
    if (!strcmp(id, "#waterfall")) { *aspect = 0.5f; return make_tex(64, 128, 1, 0, px_waterfall); }
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) if (!strcmp(id, table[i].id)) return make_tex(PW, PH, 0, 0, table[i].fn);
    if (!strcmp(id, "#house1")) return make_tex(PW, PH, 0, 0, px_house1);
    if (!strcmp(id, "#house2")) return make_tex(PW, PH, 0, 0, px_house2);
    if (!strcmp(id, "#inn")) return make_tex(PW, PH, 0, 0, px_inn);
    if (!strcmp(id, "#palm")) return make_tex(PW, PH, 0, 0, px_palm);
    if (!strcmp(id, "#olive")) return make_tex(PW, PH, 0, 0, px_olive);
    if (!strcmp(id, "#lantern")) return make_tex(PW, PH, 0, 0, px_lantern);
    if (!strcmp(id, "#well")) return make_tex(PW, PH, 0, 0, px_well);
    if (!strcmp(id, "#stable")) return make_tex(PW, PH, 0, 0, px_stable);
    if (!strcmp(id, "#sheep")) return make_tex(PW, PH, 0, 0, px_sheep);
    *aspect = 1.0f;
    return make_tex(64, 64, 0, 0, px_tree);
}
/* a scenery picture: Sean's file if it exists (beth_house1 for "#house1"), else the placeholder drawing */
static int sprite_kind(const char *img) {
    for (int i = 0; i < g_nkind; i++) if (!strcmp(g_kind[i].img, img)) return i;
    if (g_nkind >= MAX_SPRITE_KINDS) return 0;
    SpriteKind *k = &g_kind[g_nkind]; k->img = img; k->tex = NULL;
    { char name[64]; int w = 0, h = 0;
      if (img[0] == '#') snprintf(name, sizeof name, "%s_%s", g_def->prefix, img + 1); else snprintf(name, sizeof name, "%s", img);
      k->tex = asset_texture(name, 0, &w, &h);
      if (!k->tex && img[0] == '#') k->tex = asset_texture(img + 1, 0, &w, &h);   /* a picture shared by every road (waterfall.png) */
      if (k->tex) k->aspect = (float)w / (float)h; }
    if (!k->tex) k->tex = placeholder_sprite(img, &k->aspect);
    k->mesh = gfx_mesh_dynamic((MAX_SCENERY + MAX_MOVERS) * 6);
    return g_nkind++;
}
static Texture *theme_tex(const char *name, int w, int h, void (*fallback)(int, int, uint32_t *)) {
    Texture *t = asset_texture(name, 1, NULL, NULL);
    return t ? t : make_tex(w, h, 1, 0, fallback);
}
static void free_theme(void) {
    Texture **ts[] = {&tx_road, &tx_dirt, &tx_grass, &tx_wall, &tx_sky, &tx_star};
    for (size_t i = 0; i < sizeof ts / sizeof ts[0]; i++) { gfx_texture_free(*ts[i]); *ts[i] = NULL; }
    for (int i = 0; i < g_nkind; i++) { gfx_texture_free(g_kind[i].tex); gfx_mesh_free(g_kind[i].mesh); }
    g_nkind = 0; gfx_mesh_free(m_glow); m_glow = NULL;
}
static void load_theme(void) {
    const TrackDef *d = g_def;
    int heaven = d->grid_idx > 0;
    tx_road = theme_tex(d->tex_road, 256, 256, heaven ? px_gold : px_road);
    tx_dirt = theme_tex(d->tex_dirt, 128, 128, d->water ? px_sand : (heaven ? px_cloud : px_dirt));
    tx_grass = theme_tex(d->tex_grass, 128, 128, d->water ? px_water : px_grass);
    tx_wall = theme_tex(d->tex_wall, 64, 64, heaven ? px_cloud : px_wall);
    g_skytop[0] = d->sky_r * 0.8f; g_skytop[1] = d->sky_g * 0.85f; g_skytop[2] = d->sky_b; g_skyhor[0] = d->fog_r; g_skyhor[1] = d->fog_g; g_skyhor[2] = d->fog_b;
    { int sw = 0, sh = 0; Texture *painted = asset_texture(d->tex_sky, 1, &sw, &sh);
      if (painted) { tx_sky = painted; g_sky_wrap = sw >= sh * 3 ? 1.0f : 3.0f; }
      else { tx_sky = make_tex(256, 256, 1, 0, d->night ? px_sky_night : px_sky); g_sky_wrap = 3.0f; } }
    gfx_texture_free(tx_finale); tx_finale = d->finale_img ? asset_texture(d->finale_img, 0, NULL, NULL) : NULL;
    tx_star = NULL;
    if (d->star_size > 0) { tx_star = asset_texture("beth_star", 0, NULL, NULL); if (!tx_star && g_sky_wrap > 1.5f) tx_star = make_tex(64, 64, 0, 0, px_star); }
    g_nkind = 0; g_nplaced = 0;
    for (int i = 0; i < g_track.nmover; i++) g_mover_kind[i] = sprite_kind(g_track.mover[i].img);
    for (int i = 0; i < d->nscenery && g_nplaced < MAX_SCENERY; i++) {
        const Scenery *sc = &d->scenery[i]; int idx = sc->idx % g_track.n;
        Placed *p = &g_placed[g_nplaced++];
        p->kind = sprite_kind(sc->img);
        float edge = track_edge(&g_track, idx);
        p->p = v3(g_track.s[idx].x + g_track.s[idx].nx * sc->side, 0, g_track.s[idx].z + g_track.s[idx].nz * sc->side);
        p->p.y = track_surface_height(&g_track, idx, (sc->side < 0 ? -1.0f : 1.0f) * edge) - 0.2f;   /* stands on the ground just outside the wall */
        p->h = sc->height; p->w = sc->height * g_kind[p->kind].aspect; p->glow = sc->glow; p->purple = !strcmp(sc->img, "#crystal"); p->falls = !strcmp(sc->img, "#waterfall");
    }
    m_glow = gfx_mesh_dynamic(MAX_SCENERY * 6 + 6);
}

// ------------------------------------------------------------------ setup
static void set_track(int idx) {
    g_track_idx = idx % NUM_TRACK_DEFS; g_def = &TRACK_DEFS[g_track_idx];
    track_from_def(&g_track, g_def);
    float minx = 1e9f, maxx = -1e9f, minz = 1e9f, maxz = -1e9f;
    for (int i = 0; i < g_track.n; i++) { TrackSample *sm = &g_track.s[i]; if (sm->x < minx) minx = sm->x; if (sm->x > maxx) maxx = sm->x; if (sm->z < minz) minz = sm->z; if (sm->z > maxz) maxz = sm->z; }
    float span = fmaxf(maxx - minx, maxz - minz) + 20.0f; map_scale = 190.0f / span; map_x0 = (minx + maxx) * 0.5f; map_z0 = (minz + maxz) * 0.5f;
    if (g_status == GFX_OK) { free_theme(); free_track_meshes(); load_theme(); build_track_meshes(); }
}
static void start_race(int attract) {
    g_attract = attract; g_slowmo = 0; g_orbit = 0;
    int nk = (g_mode == MODE_TT && !attract) ? 1 : NKARTS;
    race_init(&g_race, &g_track, g_player_char, nk == 1 ? 0 : 5, g_difficulty, nk);
    if (!attract) { sfx_play(SFX_COUNTDOWN, 1, 0); sfx_play(SFX_ENGINE_START, 0.8f, 0); }
    g_skid_n = 0; memset(g_skid, 0, sizeof g_skid); memset(g_dust, 0, sizeof g_dust); memset(g_last_skid, 0, sizeof g_last_skid);
    Kart *k = &g_race.k[g_race.player];
    cam_yaw = k->yaw; cam_x = k->x - sinf(cam_yaw) * 8; cam_z = k->z - cosf(cam_yaw) * 8; cam_y = k->y + 3.5f;
    g_msg_t = 0; g_results_t = 0; g_demo = attract; g_idle = attract ? 99 : 0;
}
static void build_previews(void) {
    for (int d = 0; d < NUM_TRACK_DEFS && d < 8; d++) {
        static Track tmp; track_from_def(&tmp, &TRACK_DEFS[d]);
        float minx = 1e9f, maxx = -1e9f, minz = 1e9f, maxz = -1e9f;
        for (int i = 0; i < tmp.n; i++) { if (tmp.s[i].x < minx) minx = tmp.s[i].x; if (tmp.s[i].x > maxx) maxx = tmp.s[i].x; if (tmp.s[i].z < minz) minz = tmp.s[i].z; if (tmp.s[i].z > maxz) maxz = tmp.s[i].z; }
        float span = fmaxf(maxx - minx, maxz - minz) + 10.0f; Preview *pv = &g_preview[d]; pv->n = 0;
        for (int i = 0; i < tmp.n && pv->n < 80; i += (tmp.n / 80 + 1)) { pv->x[pv->n] = (tmp.s[i].x - (minx + maxx) * 0.5f) / span; pv->z[pv->n] = (tmp.s[i].z - (minz + maxz) * 0.5f) / span; pv->n++; }
    }
}
int game_init(int status) {
    g_status = status; music_set_logger(gfx_log); sfx_set_logger(gfx_log); sfx_load_samples();
    save_load(); save_check_unlocks(); save_store(); apply_volumes();
#ifdef KK_TOUR
    if (KK_TOUR_ROAD >= 4) g_save.unlocks = 0xFFFF;      /* the CI tour of the bonus road / the battle */
#endif
    for (int d = 0; d < 8 && d < NUM_TRACK_DEFS; d++) g_best_time[d] = g_save.best_ms[d] / 1000.0f;
    race_set_unlocked(unlocked_mask()); race_set_styles(0, unlocked_styles());
    build_previews();
    if (status == GFX_OK) {
        tx_blob = make_tex(64, 64, 0, 0, px_blob);
        tx_flag = make_tex(32, 32, 1, 1, px_flag);
        { static uint32_t fpx[FONT_TEX_W * FONT_TEX_H]; font_build_texture(fpx); tx_font = gfx_texture_rgba(fpx, FONT_TEX_W, FONT_TEX_H, 0, 1); }
        static const char *pn[NCHARS] = {"portrait_jesus", "portrait_moses", "portrait_david", "portrait_esther", "portrait_peter", "portrait_mary", "portrait_daniel", "portrait_samson", "portrait_noah", "portrait_joseph", "portrait_jonah", "portrait_john"};
        for (int i = 0; i < NCHARS; i++) { int w = 1, h = 1; tx_portrait[i] = asset_texture(pn[i], 0, &w, &h); g_portrait_aspect[i] = h > 0 ? (float)w / (float)h : 1.0f;
            char cn[48]; snprintf(cn, sizeof cn, "char_%s", pn[i] + 9); w = h = 1; tx_char[i] = asset_texture(cn, 0, &w, &h); g_char_aspect[i] = h > 0 ? (float)w / (float)h : 1.0f;
            snprintf(cn, sizeof cn, "kart_%s", pn[i] + 9); w = h = 1; tx_kart[i] = asset_texture(cn, 0, &w, &h); g_kart_aspect[i] = h > 0 ? (float)w / (float)h : 1.6f;
            snprintf(cn, sizeof cn, "kartback_%s", pn[i] + 9); w = h = 1; tx_kartback[i] = asset_texture(cn, 0, &w, &h); g_kartback_aspect[i] = h > 0 ? (float)w / (float)h : 1.6f;
            tx_kartview[i][0] = tx_kartback[i]; g_kartview_aspect[i][0] = g_kartback_aspect[i]; tx_kartview[i][4] = tx_kart[i]; g_kartview_aspect[i][4] = g_kart_aspect[i];
            static const char *suf[3] = {"bl", "l", "fl"};
            for (int v = 0; v < 3; v++) { snprintf(cn, sizeof cn, "kartview_%s_%s", pn[i] + 9, suf[v]); w = h = 1; tx_kartview[i][v + 1] = asset_texture(cn, 0, &w, &h); g_kartview_aspect[i][v + 1] = h > 0 ? (float)w / (float)h : 1.6f; } }
        for (int i = 0; i < NKARTS; i++) m_kart_spr[i] = gfx_mesh_dynamic(6);
        m_kartpic = gfx_mesh_dynamic(6);
        static const char *in[ITEM_COUNT] = {"", "icon_manna", "icon_staff", "icon_stone", "icon_oil", "shofar", "icon_fire", "icon_locusts", "icon_dove"};
        for (int i = 1; i < ITEM_COUNT; i++) tx_item[i] = asset_texture(in[i], 0, NULL, NULL);
        { int w = 2, h = 3; tx_goliath = asset_texture("char_goliath", 0, &w, &h); g_goliath_aspect = (float)w / (float)h; m_goliath = gfx_mesh_dynamic(6); m_hud2 = gfx_mesh_dynamic(8192); }
        tx_title = asset_texture("title_art", 0, NULL, NULL);
        for (int d = 0; d < NUM_TRACK_DEFS && d < 8; d++) { char cn[48]; snprintf(cn, sizeof cn, "card_%s", TRACK_DEFS[d].prefix); tx_card[d] = asset_texture(cn, 0, NULL, NULL); }
        if (tx_title) { batch_init(&g_b, g_vtmp, 8192); batch_clear(&g_b); batch_rect_uv(&g_b, 0, 0, 1280, 720, 0, 0, 1, 1, 1, 1, 1, 1); m_title = gfx_mesh_static(g_b.v, g_b.n); }
    }
    set_track(0);
    start_race(1);
    g_time = 0; g_screen = SCR_TITLE;
    gfx_log("track '%s': %d samples, %.0f m", g_track.name, g_track.n, g_track.length);
    return status == GFX_OK;
}
void game_shutdown(void) {}

// ------------------------------------------------------------------ effects
static void add_skid(int ki, Vec3 l, Vec3 r) {
    if (g_last_skid[ki]) {
        float jx = l.x - g_last_l[ki].x, jz = l.z - g_last_l[ki].z;
        if (jx * jx + jz * jz > 1.2f * 1.2f) { g_last_l[ki] = l; g_last_r[ki] = r; return; }
        Skid *s = &g_skid[g_skid_n % MAX_SKID]; g_skid_n++;
        s->l0 = g_last_l[ki]; s->r0 = g_last_r[ki]; s->l1 = l; s->r1 = r; s->a = 0.55f; s->used = 1;
    }
    g_last_l[ki] = l; g_last_r[ki] = r; g_last_skid[ki] = 1;
}
static void add_dust(Vec3 p, float spread, float up) {
    for (int i = 0; i < MAX_DUST; i++) if (g_dust[i].life <= 0) {
        Dust *d = &g_dust[i];
        d->p = v3(p.x + (frand01() - 0.5f) * spread, p.y + 0.2f, p.z + (frand01() - 0.5f) * spread);
        d->v = v3((frand01() - 0.5f) * 3, up + frand01() * 2, (frand01() - 0.5f) * 3);
        d->life = 0.5f + frand01() * 0.5f; d->size = 0.5f + frand01() * 0.6f;
        return;
    }
}
/* a sound coming from a kart: quieter when far from the player, panned to its side */
static void sfx_at(int id, Kart *q, float vol) {
    Race *r = &g_race; Kart *k = &r->k[r->player];
    float dx = q->x - k->x, dz = q->z - k->z, d = sqrtf(dx * dx + dz * dz);
    float g = vol / (1.0f + d * 0.05f); if (g < 0.05f) return;
    float side = -(dx * cosf(cam_yaw) - dz * sinf(cam_yaw));           /* screen-right is -x when facing +z */
    sfx_play(id, g, clampf(side * 0.05f, -0.8f, 0.8f));
}
static void add_spark(Vec3 p, Vec3 v, float size, float life, float r, float g, float b) {
    for (int i = 0; i < MAX_SPARK; i++) if (g_spark[i].life <= 0) { Spark *s = &g_spark[i]; s->p = p; s->v = v; s->size = size; s->life = life; s->r = r; s->g = g; s->b = b; return; }
}
static const char *place_name(int p) { static const char *n[] = {"", "1ST", "2ND", "3RD", "4TH", "5TH", "6TH", "7TH", "8TH"}; return p >= 1 && p <= 8 ? n[p] : ""; }
static void fmt_time(char *buf, size_t n, float t) { snprintf(buf, n, "%d:%02d.%02d", (int)(t / 60), (int)t % 60, (int)(t * 100) % 100); }

// ------------------------------------------------------------------ menu drawing helpers (blue panels, gold frames)
static void panel(float x, float y, float w, float h) {
    batch_rect(&g_b, x - 4, y - 4, w + 8, h + 8, 0.95f, 0.78f, 0.3f, 0.95f);
    batch_rect(&g_b, x, y, w, h, 0.08f, 0.14f, 0.36f, 0.94f);
}
static void checker_band(float y, float h) {
    for (float x = 0; x < 1280; x += h) { int c = ((int)(x / h)) & 1; batch_rect(&g_b, x, y, h, h, c ? 0.95f : 0.1f, c ? 0.95f : 0.1f, c ? 0.95f : 0.1f, 0.9f); }
}
static void text_c(const char *s, float y, float size, float r, float g, float b, float a) { batch_text(&g_b, s, 640 - text_width(s, size) / 2, y, size, r, g, b, a); }
static void menu_item(const char *s, float y, int selected, float size) {
    if (selected) { batch_rect(&g_b, 400, y - 8, 480, size + 16, 0.95f, 0.78f, 0.3f, 0.35f); text_c(s, y, size, 1, 0.95f, 0.5f, 1); }
    else text_c(s, y, size, 0.9f, 0.92f, 1, 1);
}
static void stat_bar(float x, float y, const char *label, float v) {
    batch_text(&g_b, label, x, y, 14, 0.85f, 0.9f, 1, 1);
    batch_rect(&g_b, x + 80, y + 2, 100, 10, 0.05f, 0.08f, 0.2f, 0.9f);
    float f = clampf((v - 0.8f) / 0.5f, 0.1f, 1.0f);
    batch_rect(&g_b, x + 80, y + 2, 100 * f, 10, 0.3f + 0.6f * f, 0.85f, 0.3f, 1);
}
static void draw_preview(const Preview *pv, float cx, float cy, float size, float r, float g, float b) {
    for (int i = 0; i < pv->n; i++) batch_rect(&g_b, cx + pv->x[i] * size - 2.5f, cy - pv->z[i] * size - 2.5f, 5, 5, r, g, b, 1);
}

// ------------------------------------------------------------------ the race HUD
static void hud_gamepad(void) {     /* the GamePad shows the big map and the standings while the TV shows the race */
    Race *r = &g_race; char buf[64];
    batch_rect(&g_b, 0, 0, 1280, 720, 0.04f, 0.07f, 0.18f, 1);
    text_c(g_track.name, 20, 28, 1, 0.85f, 0.3f, 1);
    float mx = 420, mz = 380, sc = map_scale * 2.6f;
    batch_rect(&g_b, mx - 330, mz - 300, 660, 600, 0.02f, 0.04f, 0.1f, 1);
    for (int i = 0; i < g_track.n; i++) { TrackSample *sm = &g_track.s[i]; batch_rect(&g_b, mx + (sm->x - map_x0) * sc - 5, mz - (sm->z - map_z0) * sc - 5, 10, 10, 0.85f, 0.85f, 0.85f, 1); }
    for (int b = 0; b < r->nbox; b++) if (r->box[b].active) batch_rect(&g_b, mx + (r->box[b].x - map_x0) * sc - 4, mz - (r->box[b].z - map_z0) * sc - 4, 8, 8, 0.3f, 0.6f, 1.0f, 1);
    for (int p = r->nk - 1; p >= 0; p--) { int i = r->order[p]; Kart *q = &r->k[i]; const Character *c = &CHARACTERS[r->character[i]]; float sz = i == r->player ? 26 : 18;
        batch_rect(&g_b, mx + (q->x - map_x0) * sc - sz / 2, mz - (q->z - map_z0) * sc - sz / 2, sz, sz, c->r, c->g, c->b, 1);
        if (i == r->player) batch_rect(&g_b, mx + (q->x - map_x0) * sc - 6, mz - (q->z - map_z0) * sc - 6, 12, 12, 1, 1, 1, 1); }
    if (g_mode == MODE_BATTLE) { Battle *b = &g_battle; batch_rect(&g_b, mx + (b->gx - map_x0) * sc - 20, mz - (b->gz - map_z0) * sc - 20, 40, 40, 0.9f, 0.2f, 0.15f, 1); batch_text(&g_b, "G", mx + (b->gx - map_x0) * sc - 8, mz - (b->gz - map_z0) * sc - 10, 20, 1, 1, 1, 1); }
    /* standings down the right side */
    batch_text(&g_b, g_mode == MODE_BATTLE ? "THE FIGHT" : "STANDINGS", 800, 80, 24, 1, 0.85f, 0.3f, 1);
    if (g_mode == MODE_BATTLE) { Battle *b = &g_battle; snprintf(buf, sizeof buf, "GOLIATH HIT %d OF %d", b->hits_on_goliath, BATTLE_WIN_HITS); batch_text(&g_b, buf, 800, 130, 20, 1, 1, 1, 1);
        snprintf(buf, sizeof buf, "DAVID HIT %d OF %d", b->hits_on_david, BATTLE_LIVES); batch_text(&g_b, buf, 800, 165, 20, 1, 1, 1, 1); snprintf(buf, sizeof buf, "STONES x%d", b->stones_held); batch_text(&g_b, buf, 800, 200, 20, 1, 0.9f, 0.5f, 1); }
    else for (int p = 0; p < r->nk; p++) { int i = r->order[p]; const Character *c = &CHARACTERS[r->character[i]]; float y = 125 + p * 42; int me = i == r->player;
        if (me) batch_rect(&g_b, 790, y - 5, 460, 36, 1, 0.85f, 0.3f, 0.25f);
        batch_text(&g_b, place_name(p + 1), 800, y, 22, 1, 1, 1, 1); batch_rect(&g_b, 880, y + 2, 22, 22, c->r, c->g, c->b, 1); batch_text(&g_b, c->short_name, 915, y, 22, 1, 1, 1, 1);
        if (r->k[i].lap >= 1) { snprintf(buf, sizeof buf, "LAP %d", r->k[i].lap > g_track.laps ? g_track.laps : r->k[i].lap); batch_text(&g_b, buf, 1120, y, 18, 0.8f, 0.9f, 1, 1); } }
    if (g_mode != MODE_TT && g_mode != MODE_BATTLE) { Kart *k = &r->k[r->player]; snprintf(buf, sizeof buf, "ITEM: %s", k->item ? item_name(k->item) : "-"); batch_text(&g_b, buf, 800, 500, 22, 1, 0.9f, 0.4f, 1);
        snprintf(buf, sizeof buf, "COINS x%d", k->coins); batch_text(&g_b, buf, 800, 540, 22, 1, 0.9f, 0.5f, 1); }
    { char tb[32]; fmt_time(tb, sizeof tb, r->k[r->player].total_time); snprintf(buf, sizeof buf, "TIME %s", tb); batch_text(&g_b, buf, 800, 600, 22, 1, 1, 1, 1); }
    if (g_msg_t > 0) text_c(g_msg, 660, 30, 1, 0.95f, 0.4f, 1);
}
static void hud_race(const Pad *pad) {
    Race *r = &g_race; Kart *k = &r->k[r->player]; char buf[64];
    int lap = k->lap < 1 ? 1 : (k->lap > g_track.laps ? g_track.laps : k->lap);
    snprintf(buf, sizeof buf, "LAP %d/%d", lap, g_track.laps); batch_text(&g_b, buf, 40, 30, 36, 1, 1, 1, 1);
    fmt_time(buf, sizeof buf, k->lap >= 1 ? k->lap_time : 0.0f); batch_text(&g_b, buf, 40, 72, 28, 1, 1, 0.6f, 1);
    if (k->best_lap > 0) { char tb[32]; fmt_time(tb, sizeof tb, k->best_lap); snprintf(buf, sizeof buf, "BEST %s", tb); batch_text(&g_b, buf, 40, 104, 20, 0.8f, 0.9f, 1, 1); }
    if (g_mode == MODE_TT) { fmt_time(buf, sizeof buf, k->total_time); batch_text(&g_b, buf, 1240 - text_width(buf, 40), 30, 40, 1, 0.9f, 0.3f, 1);
        if (g_best_time[g_track_idx] > 0) { char tb[32]; fmt_time(tb, sizeof tb, g_best_time[g_track_idx]); snprintf(buf, sizeof buf, "RECORD %s", tb); batch_text(&g_b, buf, 1240 - text_width(buf, 20), 78, 20, 0.8f, 0.9f, 1, 1); } }
    else {
        int mypos = 0; for (int p = 0; p < r->nk; p++) if (r->order[p] == r->player) mypos = p + 1;
        if (k->finished && k->place) mypos = k->place;
        batch_text(&g_b, place_name(mypos), 1240 - text_width(place_name(mypos), 64), 24, 64, 1, 0.9f, 0.3f, 1);
    }
    snprintf(buf, sizeof buf, "%3.0f KM/H", fabsf(k->speed) * 3.6f); batch_text(&g_b, buf, 1240 - text_width(buf, 24), 96, 24, 1, 1, 1, 1);
    if (g_track.ncoin) { batch_rect(&g_b, 40, 215, 16, 16, 1, 0.85f, 0.3f, 1); snprintf(buf, sizeof buf, "x%d", k->coins); batch_text(&g_b, buf, 62, 214, 18, 1, 0.9f, 0.5f, 1); }
    if (g_mode != MODE_TT) {
        batch_rect(&g_b, 40, 140, 150, 60, 0.05f, 0.1f, 0.25f, 0.7f);
        batch_rect(&g_b, 43, 143, 144, 54, 0.15f, 0.3f, 0.6f, 0.7f);
        if (k->item && tx_item[k->item]) batch_text(&g_b, item_name(k->item), 115 - text_width(item_name(k->item), 12) / 2, 203, 12, 1, 0.9f, 0.4f, 1);
        else if (k->item) batch_text(&g_b, item_name(k->item), 115 - text_width(item_name(k->item), 18) / 2, 161, 18, 1, 0.9f, 0.4f, 1);
        else batch_text(&g_b, "ZL: ITEM", 115 - text_width("ZL: ITEM", 16) / 2, 162, 16, 0.7f, 0.8f, 1, 0.8f);
    }
    if (k->drifting) { float c = clampf(k->drift_charge / 1.6f, 0, 1); batch_rect(&g_b, 1040, 130, 200, 14, 0.1f, 0.1f, 0.1f, 0.6f);
        batch_rect(&g_b, 1040, 130, 200 * c, 14, c >= 1 ? 1.0f : 0.3f, c >= 0.5f ? 0.9f : 0.4f, 0.2f, 1); }
    { float mx = 1070, mz = 560;
      batch_rect(&g_b, mx - 105, mz - 105, 210, 210, 0.0f, 0.0f, 0.0f, 0.35f);
      for (int i = 0; i < g_track.n; i += 2) { TrackSample *s = &g_track.s[i];
          batch_rect(&g_b, mx + (s->x - map_x0) * map_scale - 2, mz - (s->z - map_z0) * map_scale - 2, 4, 4, 0.9f, 0.9f, 0.9f, 0.8f); }
      for (int p = r->nk - 1; p >= 0; p--) { int i = r->order[p]; Kart *q = &r->k[i]; const Character *c = &CHARACTERS[r->character[i]]; float sz = i == r->player ? 12 : 8;
          batch_rect(&g_b, mx + (q->x - map_x0) * map_scale - sz / 2, mz - (q->z - map_z0) * map_scale - sz / 2, sz, sz, c->r, c->g, c->b, 1);
          if (i == r->player) batch_rect(&g_b, mx + (q->x - map_x0) * map_scale - 3, mz - (q->z - map_z0) * map_scale - 3, 6, 6, 1, 1, 1, 1); } }
    if (r->state == RACE_COUNTDOWN) { text_c(g_track.name, 180, 34, 1, 1, 1, 1); int n = (int)ceilf(r->cd - 0.5f); if (n >= 1 && n <= 3) { snprintf(buf, sizeof buf, "%d", n); text_c(buf, 250, 110, 1, 0.9f, 0.3f, 1); } }
    if (g_msg_t > 0) text_c(g_msg, 260, 60, 1, 0.95f, 0.4f, 1);
    if (k->wrongway_t > WRONGWAY_DELAY && ((int)(g_time * 4) & 1)) text_c("WRONG WAY", 330, 56, 1, 0.3f, 0.2f, 1);
    if (g_demo && !g_attract) text_c("DEMO - MOVE THE STICK TO DRIVE", 680, 20, 1, 1, 1, 0.8f);
    (void)pad;
}

// ------------------------------------------------------------------ screens
static void hud_title(void) {
    checker_band(150, 24);
    panel(240, 190, 800, 230);
    text_c("KINGDOM KART", 215, 86, 1, 0.85f, 0.3f, 1);
    text_c("ROAD TO GLORY", 315, 44, 1, 1, 1, 1);
    text_c("A RACE THROUGH THE STORY OF JESUS", 372, 18, 0.8f, 0.9f, 1, 1);
    checker_band(430, 24);
    if ((int)(g_time * 2) & 1) text_c("PRESS A", 520, 36, 1, 1, 1, 1);
}
static void hud_main(void) {
    panel(360, 150, 560, 420);
    text_c("MAIN MENU", 170, 40, 1, 0.85f, 0.3f, 1);
    static const char *dn[3] = {"EASY", "NORMAL", "HARD"}; char buf[64];
    menu_item("GRAND PRIX", 220, g_menu_sel == 0, 30);
    menu_item("SINGLE RACE", 266, g_menu_sel == 1, 30);
    menu_item("TIME TRIAL", 312, g_menu_sel == 2, 30);
    if (g_save.unlocks & UNLOCK_GOLIATH) menu_item("DAVID VS GOLIATH", 358, g_menu_sel == 3, 30);
    else { menu_item("? LOCKED", 358, g_menu_sel == 3, 30); if (g_menu_sel == 3) text_c("WIN A RACE AS DAVID", 392, 13, 1, 0.9f, 0.5f, 1); }
    snprintf(buf, sizeof buf, "DIFFICULTY: %s", dn[g_difficulty]); menu_item(buf, 413, g_menu_sel == 4, 26);
    menu_item("OPTIONS AND RECORDS", 455, g_menu_sel == 5, 24);
    menu_item("CREDITS", 495, g_menu_sel == 6, 24);
    text_c("A: CHOOSE   B: BACK   LEFT/RIGHT: CHANGE", 535, 16, 0.8f, 0.85f, 1, 1);
}
static void volume_row(const char *label, int vol, float y, int selected) {
    float x = 400;
    if (selected) batch_rect(&g_b, x - 16, y - 6, 480, 36, 1, 0.85f, 0.3f, 0.25f);
    batch_text(&g_b, label, x, y, 24, selected ? 1 : 0.9f, selected ? 0.85f : 0.95f, selected ? 0.3f : 1, 1);
    for (int i = 0; i < 10; i++) batch_rect(&g_b, x + 270 + i * 18, y + 2, 12, 22, i < vol ? 1 : 0.3f, i < vol ? 0.85f : 0.33f, i < vol ? 0.3f : 0.45f, 1);
}
static void hud_options(void) {
    panel(240, 60, 800, 600); char buf[64];
    text_c("OPTIONS AND RECORDS", 80, 36, 1, 0.85f, 0.3f, 1);
    volume_row("MUSIC VOLUME", g_save.music_vol, 140, g_opt_sel == 0);
    volume_row("SOUND VOLUME", g_save.sfx_vol, 180, g_opt_sel == 1);
    text_c("LEFT/RIGHT: CHANGE   B: BACK", 224, 14, 0.8f, 0.85f, 1, 1);
    batch_text(&g_b, "RECORDS", 280, 262, 24, 1, 0.85f, 0.3f, 1);
    batch_text(&g_b, "ROAD", 280, 300, 16, 0.8f, 0.85f, 1, 1); batch_text(&g_b, "BEST TIME", 660, 300, 16, 0.8f, 0.85f, 1, 1); batch_text(&g_b, "TARGET", 860, 300, 16, 0.8f, 0.85f, 1, 1);
    for (int d = 0; d < NUM_PLAY_TRACKS && d < SAVE_MAX_ROADS; d++) { float y = 326 + d * 30;
        batch_text(&g_b, TRACK_DEFS[d].name, 280, y, 18, 1, 1, 1, 1);
        if (g_save.best_ms[d] > 0) { fmt_time(buf, sizeof buf, g_save.best_ms[d] / 1000.0f); int beat = save_par_time(d) > 0 && g_save.best_ms[d] <= (int)(save_par_time(d) * 1000); batch_text(&g_b, buf, 660, y, 18, beat ? 1 : 1, beat ? 0.85f : 1, beat ? 0.3f : 1, 1); }
        else batch_text(&g_b, "--:--.--", 660, y, 18, 0.6f, 0.65f, 0.75f, 1);
        if (save_par_time(d) > 0) { fmt_time(buf, sizeof buf, save_par_time(d)); batch_text(&g_b, buf, 860, y, 18, 0.8f, 0.85f, 1, 1); } }
    float y = 326 + NUM_PLAY_TRACKS * 30 + 16;
    snprintf(buf, sizeof buf, "RACES FINISHED %d   RACES WON %d", g_save.races, g_save.wins); batch_text(&g_b, buf, 280, y, 18, 1, 1, 1, 1);
    snprintf(buf, sizeof buf, "GRAND PRIX WON: EASY %d  NORMAL %d  HARD %d", g_save.gp_wins[0], g_save.gp_wins[1], g_save.gp_wins[2]); batch_text(&g_b, buf, 280, y + 30, 18, 1, 1, 1, 1);
    snprintf(buf, sizeof buf, "GOLIATH: %d FIGHTS, %d WON", g_save.battles, g_save.battle_wins); batch_text(&g_b, buf, 280, y + 60, 18, 1, 1, 1, 1);
    int n = 0; for (int b = 0; b < UNLOCK_COUNT; b++) if (g_save.unlocks & (1u << b)) n++;
    snprintf(buf, sizeof buf, "UNLOCKED %d OF %d", n, UNLOCK_COUNT); batch_text(&g_b, buf, 280, y + 90, 18, 1, 0.85f, 0.3f, 1);
}
/* the credits roll up over the demo race; A or B skips */
typedef struct { const char *text; float size; int gold; float gap; } CreditLine;
static const CreditLine CREDITS[] = {
    {"KINGDOM KART", 56, 1, 0}, {"ROAD TO GLORY", 36, 1, 0}, {"A RACE THROUGH THE STORY OF JESUS", 18, 0, 60},
    {"CREATED BY", 18, 1, 0}, {"SEAN", 34, 0, 60},
    {"CHARACTERS, KARTS, ROADS AND ART", 18, 1, 0}, {"SEAN", 34, 0, 60},
    {"MUSIC AND SOUND", 18, 1, 0}, {"SEAN", 34, 0, 60},
    {"PROGRAMMING", 18, 1, 0}, {"CLAUDE", 34, 0, 60},
    {"THE ROADS", 18, 1, 0}, {"BETHLEHEM ROAD - LUKE 2", 24, 0, 0}, {"SEA OF GALILEE - MATTHEW 4 AND 14", 24, 0, 0},
    {"JERUSALEM ROAD - MATTHEW 21", 24, 0, 0}, {"SUNRISE AT THE EMPTY TOMB - MATTHEW 28", 24, 0, 0},
    {"ROAD TO HEAVEN - REVELATION 21 AND 22", 24, 0, 0}, {"THE VALLEY OF ELAH - 1 SAMUEL 17", 24, 0, 60},
    {"THE RACERS", 18, 1, 0}, {"JESUS - MOSES - DAVID - ESTHER", 24, 0, 0}, {"PETER - MARY - DANIEL", 24, 0, 0},
    {"SAMSON - NOAH - JOSEPH - JONAH - JOHN THE BAPTIST", 24, 0, 60},
    {"BUILT WITH", 18, 1, 0}, {"DEVKITPRO AND WUT - CAFEGLSL - MINIMP3 - STB IMAGE", 20, 0, 0}, {"FOR THE WII U AROMA HOMEBREW ENVIRONMENT", 20, 0, 60},
    {"THIS IS AN ORIGINAL FAN-MADE GAME", 18, 0, 0}, {"NOT AFFILIATED WITH NINTENDO", 18, 0, 90},
    {"I AM THE WAY, THE TRUTH, AND THE LIFE", 26, 1, 0}, {"JOHN 14:6", 18, 0, 90},
    {"THE BATTLE IS THE LORD'S", 26, 1, 0}, {"1 SAMUEL 17:47", 18, 0, 90},
    {"TO GOD BE THE GLORY", 40, 1, 90},
    {"THANK YOU FOR PLAYING", 30, 0, 0},
};
#define NCREDITS (int)(sizeof CREDITS / sizeof CREDITS[0])
#define CREDITS_SPEED 38.0f
static float credits_height(void) { float h = 0; for (int i = 0; i < NCREDITS; i++) h += CREDITS[i].size * 1.5f + CREDITS[i].gap; return h; }
static void hud_credits(void) {
    batch_rect(&g_b, 0, 0, 1280, 720, 0.02f, 0.05f, 0.15f, 0.6f);
    float y = 720 - g_screen_t * CREDITS_SPEED;
    for (int i = 0; i < NCREDITS; i++) {
        const CreditLine *c = &CREDITS[i];
        if (y > -80 && y < 760) { float fade = y < 60 ? y / 60 : (y > 640 ? (720 - y) / 80 : 1); if (fade < 0) fade = 0;
            if (c->gold) text_c(c->text, y, c->size, 1, 0.85f, 0.3f, fade); else text_c(c->text, y, c->size, 0.95f, 0.97f, 1, fade); }
        y += c->size * 1.5f + c->gap;
    }
    text_c("A: BACK", 690, 14, 0.8f, 0.85f, 1, 0.8f);
}
static void char_card_pos(int i, float *x, float *y) { *x = 40 + (i % 5) * 245, *y = 78 + (i / 5) * 202; }
static void hud_char(void) {
    text_c("CHOOSE YOUR CHARACTER", 22, 36, 1, 0.85f, 0.3f, 1);
    for (int i = 0; i < NCHARS; i++) {
        const Character *c = &CHARACTERS[i]; int sel = i == g_player_char, open = char_unlocked(i);
        float x, y, w = 225, h = 186; char_card_pos(i, &x, &y);
        if (sel) batch_rect(&g_b, x - 6, y - 6, w + 12, h + 12, 1, 0.85f, 0.3f, 1);
        batch_rect(&g_b, x, y, w, h, 0.08f, 0.14f, 0.36f, 0.95f);
        if (!open) {
            batch_rect(&g_b, x + 8, y + 8, 90, 90, 0.2f, 0.22f, 0.3f, 1);
            batch_text(&g_b, "?", x + 53 - text_width("?", 60) / 2, y + 22, 60, 0.5f, 0.55f, 0.7f, 1);
            batch_text(&g_b, "LOCKED", x + 106, y + 14, 16, 0.8f, 0.8f, 0.9f, 1);
            const char *hint = unlock_hint(char_lock_bit(i)); char line[24]; int li = 0, row = 0;
            for (const char *q = hint; ; q++) { if ((*q == ' ' || *q == 0) && li >= 11) { line[li] = 0; batch_text(&g_b, line, x + 8, y + 108 + row * 17, 12, 1, 0.9f, 0.5f, 1); row++; li = 0; if (*q == 0) break; continue; }
                if (*q == 0) { line[li] = 0; if (li) batch_text(&g_b, line, x + 8, y + 108 + row * 17, 12, 1, 0.9f, 0.5f, 1); break; }
                if (li < 22) line[li++] = *q; }
            continue;
        }
        batch_rect(&g_b, x + 8, y + 8, 90, 90, c->r, c->g, c->b, 1);
        if (!tx_portrait[i]) { char ini[2] = {c->short_name[0], 0}; batch_text(&g_b, ini, x + 53 - text_width(ini, 60) / 2, y + 22, 60, 0.1f, 0.1f, 0.15f, 0.8f); }
        batch_text(&g_b, c->short_name, x + 106, y + 14, 17, 1, 1, 1, 1);
        if (c->bonus) batch_text(&g_b, "BONUS", x + 106, y + 36, 12, 1, 0.85f, 0.3f, 1);
        stat_bar(x + 8, y + 108, "SPEED", c->st.top_speed);
        stat_bar(x + 8, y + 126, "ACCEL", c->st.accel);
        stat_bar(x + 8, y + 144, "HANDLE", c->st.handling);
        stat_bar(x + 8, y + 162, "WEIGHT", c->st.weight);
    }
    text_c("A: CHOOSE   B: BACK", 692, 14, 0.8f, 0.85f, 1, 1);
}
static void draw_portraits(void) {   /* the real portraits are separate textures, drawn after the HUD batch; one mesh each (a dynamic mesh can only be filled once per frame) */
    static Mesh *pm[NCHARS]; Mat4 id = m4_identity();
    for (int i = 0; i < NCHARS; i++) { if (!tx_portrait[i] || !char_unlocked(i)) continue;
        if (!pm[i]) { float x, y; char_card_pos(i, &x, &y);
            float a = g_portrait_aspect[i], pw = a < 1 ? 90 * a : 90, ph = a < 1 ? 90 : 90 / a;
            batch_clear(&g_b); batch_rect_uv(&g_b, x + 8 + (90 - pw) / 2, y + 8 + (90 - ph) / 2, pw, ph, 0, 0, 1, 1, 1, 1, 1, 1);
            pm[i] = gfx_mesh_static(g_b.v, g_b.n); }
        gfx_bind(tx_portrait[i]); gfx_draw(pm[i], &id, 1, 1, 1, 1); }
}
static void hud_kart(void) {
    char ttl[64]; snprintf(ttl, sizeof ttl, "%s'S KART", CHARACTERS[g_player_char].short_name); text_c(ttl, 20, 34, 1, 0.85f, 0.3f, 1);
    if (tx_kart[g_player_char]) { batch_rect(&g_b, 290, 62, 700, 230, 0.05f, 0.08f, 0.2f, 0.6f); text_c("CHOOSE A BODY", 300, 20, 1, 0.85f, 0.3f, 1); }
    static const char *desc[NSTYLES][2] = {{"THE EVERYDAY KART", "BALANCED"}, {"BUILT FOR SPEED", "WIDER TURNS"}, {"LIGHT AND NIMBLE", "QUICK TURNS"}, {"HEAVY AND STURDY", "FAST OFF THE LINE"}, {"DAVID'S SLING CART", "NIMBLE AND QUICK"}};
    for (int i = 0; i < NSTYLES; i++) {
        int sel = i == g_player_style, open = style_unlocked(i); float x = 40 + i * 245, y = tx_kart[g_player_char] ? 330 : 150, w = 230, h = tx_kart[g_player_char] ? 340 : 360;
        if (sel) batch_rect(&g_b, x - 8, y - 8, w + 16, h + 16, 1, 0.85f, 0.3f, 1);
        batch_rect(&g_b, x, y, w, h, 0.08f, 0.14f, 0.36f, 0.95f);
        if (!open) {
            batch_rect(&g_b, x + 20, y + 20, 190, 150, 0.2f, 0.22f, 0.3f, 1);
            batch_text(&g_b, "?", x + 115 - text_width("?", 90) / 2, y + 50, 90, 0.5f, 0.55f, 0.7f, 1);
            batch_text(&g_b, "LOCKED", x + 20, y + 200, 20, 0.8f, 0.8f, 0.9f, 1);
            const char *hint = unlock_hint(style_lock_bit(i)); char line[24]; int li = 0, row = 0;
            for (const char *q = hint; ; q++) { if ((*q == ' ' || *q == 0) && li >= 12) { line[li] = 0; batch_text(&g_b, line, x + 20, y + 235 + row * 20, 14, 1, 0.9f, 0.5f, 1); row++; li = 0; if (*q == 0) break; continue; }
                if (*q == 0) { line[li] = 0; if (li) batch_text(&g_b, line, x + 20, y + 235 + row * 20, 14, 1, 0.9f, 0.5f, 1); break; }
                if (li < 22) line[li++] = *q; }
            continue;
        }
        batch_rect(&g_b, x + 20, y + 20, 190, 150, 0.05f, 0.08f, 0.2f, 1);       /* the kart itself is drawn in 3D over this window */
        batch_text(&g_b, KART_STYLES[i].name, x + 20, y + 195, 22, 1, 1, 1, 1);
        batch_text(&g_b, desc[i][0], x + 20, y + 225, 13, 0.8f, 0.9f, 1, 1); batch_text(&g_b, desc[i][1], x + 20, y + 243, 13, 0.8f, 0.9f, 1, 1);
        const KartStats *c = &CHARACTERS[g_player_char].st, *m = &KART_STYLES[i].mul;
        stat_bar(x + 20, y + 270, "SPEED", c->top_speed * m->top_speed);
        stat_bar(x + 20, y + 292, "ACCEL", c->accel * m->accel);
        stat_bar(x + 20, y + 314, "HANDLE", c->handling * m->handling);
        stat_bar(x + 20, y + 336, "WEIGHT", c->weight * m->weight);
    }
    text_c("A: CHOOSE   B: BACK", 690, 16, 0.8f, 0.85f, 1, 1);
}
static void draw_kart_models(void) {    /* little 3D karts turning in the kart-select windows */
    const Character *c = &CHARACTERS[g_player_char];
    for (int i = 0; i < NSTYLES; i++) { if (!style_unlocked(i)) continue;
        float x = 40 + i * 245 + 115, y = (tx_kart[g_player_char] ? 330 : 150) + 95;
        /* a tiny perspective view into the window: the kart at the origin, camera in front */
        Mat4 view = m4_look_at(v3(3.2f * sinf(g_time * 0.8f), 1.6f, 3.2f * cosf(g_time * 0.8f)), v3(0, 0.7f, 0), v3(0, 1, 0));
        Mat4 proj = m4_perspective(40.0f * 3.14159265f / 180.0f, 190.0f / 150.0f, 0.3f, 50.0f);
        Mat4 vp = m4_mul(proj, view);
        /* squeeze it into the window: scale and shift in clip space */
        Mat4 win = m4_identity(); win.m[0] = 190.0f / 1280.0f; win.m[5] = 150.0f / 720.0f; win.m[12] = (x / 640.0f) - 1.0f; win.m[13] = 1.0f - (y / 360.0f);
        Mat4 full = m4_mul(win, vp);
        gfx_camera(&full, v3(0.3f, 1, 0.5f), 0.5f, 0, 0, 0, 1e6f, 2e6f);
        gfx_state(ST_DEPTH | ST_DEPTH_WRITE | ST_CULL); gfx_bind(NULL); Mat4 id = m4_identity();
        gfx_draw(m_body_style[i], &id, c->r, c->g, c->b, 1); gfx_draw(m_rest_style[i], &id, 1, 1, 1, 1); }
}
static void hud_track(void) {
    text_c("CHOOSE A ROAD", 30, 40, 1, 0.85f, 0.3f, 1);
    for (int d = 0; d < NUM_PLAY_TRACKS; d++) {
        int sel = d == g_track_idx, open = track_unlocked(d); float x = 40 + d * 245, y = 110, w = 225, h = 400;
        if (sel) batch_rect(&g_b, x - 8, y - 8, w + 16, h + 16, 1, 0.85f, 0.3f, 1);
        batch_rect(&g_b, x, y, w, h, 0.08f, 0.14f, 0.36f, 0.95f);
        const TrackDef *td = &TRACK_DEFS[d]; char buf[48];
        if (!open) {
            batch_rect(&g_b, x + 12, y + 12, 200, 200, 0.2f, 0.22f, 0.3f, 1);
            batch_text(&g_b, "?", x + 112 - text_width("?", 90) / 2, y + 60, 90, 0.5f, 0.55f, 0.7f, 1);
            batch_text(&g_b, "BONUS ROAD", x + 12, y + 225, 18, 1, 0.85f, 0.3f, 1);
            batch_text(&g_b, "WIN THE", x + 12, y + 255, 14, 0.9f, 0.9f, 1, 1); batch_text(&g_b, "GRAND PRIX", x + 12, y + 275, 14, 0.9f, 0.9f, 1, 1); batch_text(&g_b, "ON NORMAL", x + 12, y + 295, 14, 0.9f, 0.9f, 1, 1);
            continue;
        }
        batch_rect(&g_b, x + 12, y + 12, 200, 200, td->sky_r, td->sky_g, td->sky_b, 1);
        if (tx_card[d]) draw_preview(&g_preview[d], x + 112, y + 165, 80, 0.95f, 0.95f, 0.95f);   /* the picture goes on top (drawn after the HUD batch), the map small under it */
        else draw_preview(&g_preview[d], x + 112, y + 112, 175, 0.95f, 0.95f, 0.95f);
        /* the name, split onto two lines when long */
        const char *nm = td->name; size_t len = strlen(nm);
        size_t cut = 0;    /* the space nearest the middle of the name, if any */
        for (size_t q = 0; q < len; q++) if (nm[q] == ' ' && (cut == 0 || labs((long)q - (long)len / 2) < labs((long)cut - (long)len / 2))) cut = q;
        if (text_width(nm, 18) > w - 24 && cut > 0) { char l1[32], l2[32]; snprintf(l1, sizeof l1, "%.*s", (int)cut, nm); snprintf(l2, sizeof l2, "%s", nm + cut + 1);
            batch_text(&g_b, l1, x + 12, y + 225, 18, 1, 1, 1, 1); batch_text(&g_b, l2, x + 12, y + 248, 18, 1, 1, 1, 1); }
        else batch_text(&g_b, nm, x + 12, y + 225, text_width(nm, 18) > w - 24 ? 15 : 18, 1, 1, 1, 1);
        snprintf(buf, sizeof buf, td->laps == 1 ? "ONE RUN TO THE TOP" : "%d LAPS", td->laps); batch_text(&g_b, buf, x + 12, y + 280, 14, 0.8f, 0.9f, 1, 1);
        if (g_best_time[d] > 0) { char tb[32]; fmt_time(tb, sizeof tb, g_best_time[d]); snprintf(buf, sizeof buf, "RECORD %s", tb); batch_text(&g_b, buf, x + 12, y + 305, 14, 1, 0.9f, 0.5f, 1); }
        if (save_par_time(d) > 0) { char tb[32]; fmt_time(tb, sizeof tb, save_par_time(d)); snprintf(buf, sizeof buf, "TARGET %s", tb); batch_text(&g_b, buf, x + 12, y + 328, 14, 0.7f, 0.95f, 0.8f, 1); }
        if (td->bonus) batch_text(&g_b, "BONUS ROAD", x + 12, y + 360, 14, 1, 0.85f, 0.3f, 1);
    }
    text_c("A: RACE   B: BACK", 690, 16, 0.8f, 0.85f, 1, 1);
}
static void hud_pause(void) {
    batch_rect(&g_b, 0, 0, 1280, 720, 0, 0, 0, 0.45f);
    panel(340, 170, 600, 380);
    text_c("PAUSED", 190, 40, 1, 0.85f, 0.3f, 1);
    menu_item("CONTINUE", 262, g_pause_sel == 0, 32);
    menu_item("RESTART", 312, g_pause_sel == 1, 32);
    menu_item("QUIT TO MENU", 362, g_pause_sel == 2, 32);
    volume_row("MUSIC", g_save.music_vol, 420, g_pause_sel == 3);
    volume_row("SOUND", g_save.sfx_vol, 460, g_pause_sel == 4);
    text_c("LEFT/RIGHT: VOLUME", 510, 14, 0.8f, 0.85f, 1, 1);
}
static void hud_battle(void) {
    Battle *b = &g_battle; char buf[64];
    text_c("THE VALLEY OF ELAH", 60, 16, 1, 0.85f, 0.3f, 1);
    batch_text(&g_b, "DAVID", 40, 30, 22, 1, 1, 1, 1);
    for (int i = 0; i < BATTLE_LIVES; i++) batch_rect(&g_b, 40 + i * 34, 60, 28, 24, i < BATTLE_LIVES - b->hits_on_david ? 0.95f : 0.25f, 0.2f, 0.25f, 1);
    snprintf(buf, sizeof buf, "STONES x%d", b->stones_held); batch_text(&g_b, buf, 40, 95, 18, 0.9f, 0.9f, 0.85f, 1);
    batch_text(&g_b, "GOLIATH", 1240 - text_width("GOLIATH", 22), 30, 22, 1, 1, 1, 1);
    for (int i = 0; i < BATTLE_WIN_HITS; i++) batch_rect(&g_b, 1240 - (i + 1) * 36, 60, 30, 24, 0.9f, i < BATTLE_WIN_HITS - b->hits_on_goliath ? 0.65f : 0.15f, 0.15f, 1);
    { char tb[32]; fmt_time(tb, sizeof tb, RACE_TIME_LIMIT - b->race.race_t > 0 ? RACE_TIME_LIMIT - b->race.race_t : 0); batch_text(&g_b, tb, 640 - text_width(tb, 26) / 2, 30, 26, 1, 0.9f, 0.5f, 1); }
    if (b->race.state == RACE_COUNTDOWN) { int n = (int)ceilf(b->race.cd - 0.5f); if (n >= 1 && n <= 3) { snprintf(buf, sizeof buf, "%d", n); text_c(buf, 250, 110, 1, 0.9f, 0.3f, 1); } text_c("PICK UP STONES, FACE HIM, PRESS ZL TO SLING", 180, 18, 1, 1, 1, 1); }
    if (g_msg_t > 0) text_c(g_msg, 260, 60, 1, 0.95f, 0.4f, 1);
    if (b->quote >= 0 && b->quote < BATTLE_NQUOTES) {      /* the words of 1 Samuel 17, wrapped into a panel at the bottom */
        const BattleQuote *q = &BATTLE_QUOTES[b->quote]; float y0 = 560;
        batch_rect(&g_b, 140, y0, 1000, 120, 0.05f, 0.08f, 0.2f, 0.85f); batch_rect(&g_b, 140, y0, 1000, 3, 1, 0.85f, 0.3f, 1);
        char head[64]; snprintf(head, sizeof head, "%s%s%s", q->who, q->who[0] ? "  -  " : "", q->ref); batch_text(&g_b, head, 160, y0 + 12, 14, 1, 0.85f, 0.3f, 1);
        char line[80]; int li = 0, row = 0; const char *p = q->line;
        for (;; p++) { if ((*p == ' ' || *p == 0) && text_width(line, 18) > 860) { line[li] = 0; batch_text(&g_b, line, 160, y0 + 36 + row * 24, 18, 1, 1, 1, 1); row++; li = 0; if (*p == 0) break; continue; }
            if (*p == 0) { line[li] = 0; batch_text(&g_b, line, 160, y0 + 36 + row * 24, 18, 1, 1, 1, 1); break; }
            if (li < 78) { line[li++] = *p; line[li] = 0; } }
    }
    if (g_demo && !g_attract) text_c("DEMO - MOVE THE STICK TO DRIVE", 700, 16, 1, 1, 1, 0.8f);
}
static void hud_battle_results(void) {
    Battle *b = &g_battle; char buf[64];
    panel(290, 150, 700, 400);
    text_c(b->won ? "GIANT SLAYER!" : "GOLIATH WINS", 175, 44, 1, 0.85f, 0.3f, 1);
    snprintf(buf, sizeof buf, "HITS ON GOLIATH: %d OF %d", b->hits_on_goliath, BATTLE_WIN_HITS); text_c(buf, 260, 24, 1, 1, 1, 1);
    snprintf(buf, sizeof buf, "HITS ON DAVID: %d OF %d", b->hits_on_david, BATTLE_LIVES); text_c(buf, 300, 24, 1, 1, 1, 1);
    { char tb[32]; fmt_time(tb, sizeof tb, b->race.race_t); snprintf(buf, sizeof buf, "TIME %s", tb); text_c(buf, 340, 24, 1, 1, 1, 1); }
    if (b->won) text_c("THE BATTLE IS THE LORD'S", 400, 20, 0.9f, 0.95f, 1, 1);
    text_c("A: FIGHT AGAIN   B: MENU", 500, 20, 0.9f, 0.95f, 1, 1);
    if (g_fresh_unlocks) { float yb = 600; batch_rect(&g_b, 240, yb, 800, 60, 1, 0.85f, 0.3f, 0.95f); batch_rect(&g_b, 246, yb + 6, 788, 48, 0.08f, 0.14f, 0.36f, 1);
        char line[96] = "UNLOCKED: "; int first = 1; for (int q = 0; q < UNLOCK_COUNT; q++) if (g_fresh_unlocks & (1u << q)) { if (!first) strncat(line, ", ", sizeof line - strlen(line) - 1); strncat(line, unlock_name(1 << q), sizeof line - strlen(line) - 1); first = 0; }
        text_c(line, yb + 20, 20, 1, 0.95f, 0.5f, 1); }
}
static void hud_results(void) {
    Race *r = &g_race; char buf[64];
    panel(290, 90, 700, 540);
    text_c(g_mode == MODE_TT ? "TIME TRIAL" : "RESULTS", 110, 44, 1, 0.85f, 0.3f, 1);
    int rows[NKARTS]; for (int p = 0; p < r->nk; p++) rows[p] = r->order[p];
    for (int a2 = 1; a2 < r->nk; a2++) { int v = rows[a2], j = a2 - 1; while (j >= 0 && r->k[rows[j]].place > r->k[v].place) { rows[j + 1] = rows[j]; j--; } rows[j + 1] = v; }
    for (int p = 0; p < r->nk; p++) { int i = rows[p]; Kart *q = &r->k[i]; const Character *c = &CHARACTERS[r->character[i]];
        float y = 170 + p * 50; int me = i == r->player;
        if (me) batch_rect(&g_b, 310, y - 6, 660, 44, 1, 0.85f, 0.3f, 0.25f);
        batch_text(&g_b, place_name(q->place ? q->place : p + 1), 330, y, 30, 1, 1, 1, 1);
        batch_rect(&g_b, 440, y + 2, 26, 26, c->r, c->g, c->b, 1);
        batch_text(&g_b, c->short_name, 480, y, 30, me ? 1.0f : 0.9f, me ? 0.95f : 0.9f, me ? 0.5f : 0.9f, 1);
        if (q->lap > g_track.laps) fmt_time(buf, sizeof buf, q->total_time); else snprintf(buf, sizeof buf, "--:--.--");
        batch_text(&g_b, buf, 720, y, 30, 1, 1, 1, 1);
        if (g_mode == MODE_GP) { int pl = q->place ? q->place : p + 1; snprintf(buf, sizeof buf, "+%d", GP_POINTS[pl - 1]); batch_text(&g_b, buf, 900, y, 30, 1, 0.9f, 0.4f, 1); } }
    if (g_mode == MODE_TT) { Kart *k = &r->k[r->player]; fmt_time(buf, sizeof buf, k->best_lap); char line[96]; snprintf(line, sizeof line, "BEST LAP %s", buf); text_c(line, 240, 28, 0.9f, 0.95f, 1, 1);
        if (g_best_time[g_track_idx] > 0 && k->total_time <= g_best_time[g_track_idx] + 0.001f) text_c("NEW RECORD!", 300, 40, 1, 0.9f, 0.3f, 1); }
    text_c(g_mode == MODE_GP ? "A: NEXT ROAD" : "A: RACE AGAIN   B: MENU", 585, 20, 0.9f, 0.95f, 1, 1);
    if (g_fresh_unlocks) {      /* a banner for what this race unlocked */
        float yb = 640; batch_rect(&g_b, 240, yb, 800, 70, 1, 0.85f, 0.3f, 0.95f); batch_rect(&g_b, 246, yb + 6, 788, 58, 0.08f, 0.14f, 0.36f, 1);
        char line[96] = "UNLOCKED: "; int first = 1;
        for (int b = 0; b < UNLOCK_COUNT; b++) if (g_fresh_unlocks & (1u << b)) { if (!first) strncat(line, ", ", sizeof line - strlen(line) - 1); strncat(line, unlock_name(1 << b), sizeof line - strlen(line) - 1); first = 0; }
        float sz = text_width(line, 24) > 760 ? 16 : 24; text_c(line, yb + 35 - sz / 2, sz, 1, 0.95f, 0.5f, (int)(g_time * 3) & 1 ? 1.0f : 0.8f);
    }
}
static void hud_gp(void) {
    panel(290, 80, 700, 560); char buf[64];
    text_c(g_gp_round >= NUM_GP_TRACKS ? "GRAND PRIX CHAMPION" : "STANDINGS", 100, 40, 1, 0.85f, 0.3f, 1);
    int nk = g_race.nk; int ord[NKARTS]; for (int i = 0; i < nk; i++) ord[i] = g_race.character[i];
    for (int a2 = 1; a2 < nk; a2++) { int v = ord[a2], j = a2 - 1; while (j >= 0 && g_gp_points[ord[j]] < g_gp_points[v]) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = v; }
    for (int p = 0; p < nk; p++) { int c = ord[p]; float y = 160 + p * 50; int me = c == g_player_char;
        if (me) batch_rect(&g_b, 310, y - 6, 660, 44, 1, 0.85f, 0.3f, 0.25f);
        batch_text(&g_b, place_name(p + 1), 330, y, 30, 1, 1, 1, 1);
        batch_rect(&g_b, 440, y + 2, 26, 26, CHARACTERS[c].r, CHARACTERS[c].g, CHARACTERS[c].b, 1);
        batch_text(&g_b, CHARACTERS[c].short_name, 480, y, 30, me ? 1.0f : 0.9f, me ? 0.95f : 0.9f, me ? 0.5f : 0.9f, 1);
        snprintf(buf, sizeof buf, "%d PTS", g_gp_points[c]); batch_text(&g_b, buf, 780, y, 30, 1, 0.9f, 0.4f, 1); }
    if (g_gp_round >= NUM_GP_TRACKS) { if (ord[0] == g_player_char) text_c("YOU ARE THE CHAMPION!", 570, 28, 1, 0.9f, 0.3f, 1); text_c("A: MENU", 610, 20, 0.9f, 0.95f, 1, 1); }
    else { snprintf(buf, sizeof buf, "A: ROAD %d OF %d", g_gp_round + 1, NUM_GP_TRACKS); text_c(buf, 600, 20, 0.9f, 0.95f, 1, 1); }
}

// ------------------------------------------------------------------ per-frame update
#ifdef KK_TOUR
/* CI tour: the game presses its own buttons on a timer so the emulator test can photograph every screen */
static void tour_pad(Pad *p, float t) {
    static const struct { float t; char key; } script[] = {
#if KK_TOUR_ROAD == 6
        {8, 'A'}, {11, 'U'}, {14, 'A'},          /* the credits, then (back at the title) options and records */
        {88, 'B'}, {91, 'A'}, {94, 'U'}, {96, 'U'}, {98, 'A'},
#else
        {8, 'A'}, {11, 'D'}, {13, KK_TOUR_ROAD == 5 ? 'D' : 0}, {15, KK_TOUR_ROAD == 5 ? 'D' : 0}, {18, KK_TOUR_ROAD == 5 ? 'A' : 0}, {14, KK_TOUR_ROAD == 5 ? 0 : 'A'}, {17, KK_TOUR_ROAD == 5 ? 0 : 'R'}, {22, KK_TOUR_ROAD == 5 ? 0 : 'A'}, {26, KK_TOUR_ROAD == 5 ? 0 : 'A'},
        {27, KK_TOUR_ROAD >= 1 ? 'R' : 0}, {29, KK_TOUR_ROAD >= 2 ? 'R' : 0}, {31, KK_TOUR_ROAD >= 3 ? 'R' : 0}, {33, KK_TOUR_ROAD >= 4 ? 'R' : 0}, {36, 'A'},
        {70, 'P'}, {84, 'P'}
#endif
    };
    memset(p, 0, sizeof *p);
    for (unsigned i = 0; i < sizeof script / sizeof script[0]; i++) {
        if (t >= script[i].t && t < script[i].t + 0.15f) {
            switch (script[i].key) { case 'A': p->a = 1; break; case 'B': p->b = 1; break; case 'P': p->plus = 1; break;
                                     case 'D': p->sy = -1; break; case 'U': p->sy = 1; break; case 'R': p->sx = 1; break; case 'L': p->sx = -1; break; }
        }
    }
}
#endif
static void begin_player_race(void) { set_track(g_mode == MODE_GP ? g_gp_order[g_gp_round] : g_track_idx); start_race(0); g_screen = SCR_RACE; }
static void begin_battle(void) {
    set_track(ARENA_TRACK); g_attract = 0;
    battle_init(&g_battle, &g_track, g_difficulty);
    g_skid_n = 0; memset(g_skid, 0, sizeof g_skid); memset(g_dust, 0, sizeof g_dust); memset(g_last_skid, 0, sizeof g_last_skid);
    Kart *k = &g_race.k[0]; cam_yaw = k->yaw; cam_x = k->x - sinf(cam_yaw) * 8; cam_z = k->z - cosf(cam_yaw) * 8; cam_y = k->y + 3.5f;
    g_msg_t = 0; g_results_t = 0; g_demo = 0; g_idle = 0; g_screen = SCR_RACE;
}

/* which music should be playing right now. Files are content/music/NAME.mp3; a road without its own track borrows one. */
static int gp_player_rank(void) {   /* 1 = leading the championship */
    int rank = 1; for (int c = 0; c < NCHARS; c++) if (c != g_player_char && g_gp_points[c] > g_gp_points[g_player_char]) rank++;
    return rank;
}
static const char *pick_music(void) {
    static char buf[48];
    if (g_screen == SCR_RACE || g_screen == SCR_PAUSE) {
        if (g_mode == MODE_BATTLE) return music_available("music_elah") ? "music_elah" : (music_available("music_jeru") ? "music_jeru" : "music_menu");
        Race *r = &g_race; Kart *pk = &r->k[r->player];
        if (r->state == RACE_COUNTDOWN && sfx_has_sample(SFX_COUNTDOWN)) return NULL;   /* the recorded 3-2-1-GO plays alone */
        if (g_track.laps > 1 && pk->lap >= g_track.laps && r->state != RACE_COUNTDOWN && music_available("music_finallap")) return "music_finallap";
        snprintf(buf, sizeof buf, "music_%s", g_def->prefix);
        if (music_available(buf)) return buf;
        static const char *roads[5] = {"music_beth", "music_gal", "music_jeru", "music_tomb", "music_heaven"};
        for (int i = 0; i < 5; i++) if (music_available(roads[i])) return roads[i];
        return "music_menu";
    }
    if (g_screen == SCR_RESULTS) {
        int lost = g_mode == MODE_BATTLE ? !g_battle.won : (g_mode != MODE_TT && g_race.k[g_race.player].place > 3);
        if (lost && music_available("music_lose")) return "music_lose";
        return music_available("music_results") ? "music_results" : "music_menu";
    }
    if (g_screen == SCR_GP) {
        if (g_gp_round >= NUM_GP_TRACKS) { int rank = gp_player_rank();
            if (rank == 1 && music_available("music_trophy")) return "music_trophy";
            if (rank > 3 && music_available("music_lose")) return "music_lose"; }
        return music_available("music_results") ? "music_results" : "music_menu";
    }
    if ((g_screen == SCR_CHAR || g_screen == SCR_KART || g_screen == SCR_TRACK) && music_available("music_select")) return "music_select";
    if (g_screen == SCR_CREDITS && music_available("music_credits")) return "music_credits";
    return "music_menu";
}

void game_frame(const Pad *pad, float dt) {
    if (dt > 0.1f) dt = 0.1f;
#ifdef KK_TOUR
    Pad tour; tour_pad(&tour, g_time); pad = &tour;
#endif
    g_frame_ms = dt * 1000.0f;
    g_time += dt; g_screen_t += dt;
    Race *r = &g_race;
    /* button edges and menu navigation with repeat */
    int pa = pad->a && !g_prev.a, pb = pad->b && !g_prev.b, pplus = pad->plus && !g_prev.plus;
    int nx = (pad->sx > 0.5f) - (pad->sx < -0.5f), ny = (pad->sy > 0.5f) - (pad->sy < -0.5f);
    int pnx = (g_prev.sx > 0.5f) - (g_prev.sx < -0.5f), pny = (g_prev.sy > 0.5f) - (g_prev.sy < -0.5f);
    int mx = 0, my = 0;
    if (nx != pnx || ny != pny) { mx = nx; my = ny; g_nav_t = 0; } else if (nx || ny) { g_nav_t += dt; if (g_nav_t > 0.35f) { mx = nx; my = ny; g_nav_t = 0.22f; } }
    g_prev = *pad;
    if (g_screen_t < 0.3f) { pa = pb = 0; mx = my = 0; }      /* ignore presses right after a screen change */

    int sim_running = 1;
    switch (g_screen) {
    case SCR_TITLE: if (pa) { g_screen = SCR_MAIN; g_screen_t = 0; g_menu_sel = 0; sfx_play(SFX_MENU_OK, 1, 0); } break;
    case SCR_OPTIONS:
        if (my) { g_opt_sel = (g_opt_sel - my + 2) % 2; sfx_play(SFX_MENU_MOVE, 1, 0); }
        if (mx) { int *v = g_opt_sel == 0 ? &g_save.music_vol : &g_save.sfx_vol; *v = *v + mx < 0 ? 0 : (*v + mx > 10 ? 10 : *v + mx); g_save.dirty = 1; apply_volumes(); sfx_play(SFX_MENU_MOVE, 1, 0); }
        if (pb || pa) { save_store(); g_screen = SCR_MAIN; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_CREDITS:
        if (pa || pb || g_screen_t * CREDITS_SPEED > credits_height() + 760) { g_screen = SCR_TITLE; g_screen_t = 0; if (pa || pb) sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_MAIN:
        if (my) { g_menu_sel = (g_menu_sel - my + 7) % 7; sfx_play(SFX_MENU_MOVE, 1, 0); }
        if (g_menu_sel == 4 && mx) g_difficulty = (g_difficulty + mx + 3) % 3;
        if (pa) {
            if (g_menu_sel == 6) { g_screen = SCR_CREDITS; g_screen_t = 0; sfx_play(SFX_MENU_OK, 1, 0); }
            else if (g_menu_sel == 5) { g_screen = SCR_OPTIONS; g_opt_sel = 0; g_screen_t = 0; sfx_play(SFX_MENU_OK, 1, 0); }
            else if (g_menu_sel == 4) { g_difficulty = (g_difficulty + 1) % 3; sfx_play(SFX_MENU_OK, 1, 0); }
            else if (g_menu_sel == 3) { if (g_save.unlocks & UNLOCK_GOLIATH) { sfx_play(SFX_MENU_OK, 1, 0); g_mode = MODE_BATTLE; g_player_char = 2; begin_battle(); g_screen_t = 0; } else sfx_play(SFX_WRONG_WAY, 1, 0); }
            else { sfx_play(SFX_MENU_OK, 1, 0); g_mode = g_menu_sel == 0 ? MODE_GP : (g_menu_sel == 1 ? MODE_SINGLE : MODE_TT); g_screen = SCR_CHAR; g_screen_t = 0; } }
        if (pb) { g_screen = SCR_TITLE; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_CHAR:
        if (mx) { int row = g_player_char / 5, ncol = row == 2 ? NCHARS - 10 : 5; g_player_char = row * 5 + ((g_player_char % 5) + mx + ncol) % ncol; sfx_play(SFX_MENU_MOVE, 1, 0); }
        if (my) { int nc = g_player_char - my * 5; if (nc < 0) nc += 15; if (nc >= 15) nc -= 15; if (nc >= NCHARS) nc = my < 0 ? nc - 5 : nc % 5; g_player_char = nc; sfx_play(SFX_MENU_MOVE, 1, 0); }
        if (pa) sfx_play(char_unlocked(g_player_char) ? SFX_MENU_OK : SFX_WRONG_WAY, 1, 0);
        if (pa && char_unlocked(g_player_char)) { g_screen = SCR_KART; g_screen_t = 0; }
        if (pb) { g_screen = SCR_MAIN; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_KART:
        if (mx) { g_player_style = (g_player_style + mx + NSTYLES) % NSTYLES; sfx_play(SFX_MENU_MOVE, 1, 0); }
        if (pa) sfx_play(style_unlocked(g_player_style) ? SFX_MENU_OK : SFX_WRONG_WAY, 1, 0);
        if (pa && style_unlocked(g_player_style)) { race_set_styles(g_player_style, unlocked_styles());
            if (g_mode == MODE_GP) { memset(g_gp_points, 0, sizeof g_gp_points); g_gp_round = 0; for (int i = 0; i < NUM_GP_TRACKS; i++) g_gp_order[i] = i; begin_player_race(); g_screen_t = 0; } else { g_screen = SCR_TRACK; g_screen_t = 0; } }
        if (pb) { g_screen = SCR_CHAR; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_TRACK:
        if (mx) { sfx_play(SFX_MENU_MOVE, 1, 0); int d = (g_track_idx + mx + NUM_PLAY_TRACKS) % NUM_PLAY_TRACKS; g_track_idx = d; if (track_unlocked(d)) { set_track(d); start_race(1); } }
        if (pa) sfx_play(track_unlocked(g_track_idx) ? SFX_MENU_OK : SFX_WRONG_WAY, 1, 0);
        if (pa && track_unlocked(g_track_idx)) { begin_player_race(); g_screen_t = 0; }
        if (pb) { g_screen = SCR_KART; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_RACE:
        if (pplus && r->state != RACE_FINISHED && g_screen_t > 0.5f) { g_screen = SCR_PAUSE; g_pause_sel = 0; g_screen_t = 0; }
        if (r->state == RACE_FINISHED) {
            g_results_t += dt;
            if (g_results_t > 1.5f) {
                if (g_mode == MODE_GP) { race_rank(r); for (int p = 0; p < r->nk; p++) { Kart *q = &r->k[r->order[p]]; int pl = q->place ? q->place : p + 1; g_gp_points[r->character[r->order[p]]] += GP_POINTS[pl - 1]; } }
                if (g_mode == MODE_TT) { Kart *k = &r->k[r->player]; if (k->lap > g_track.laps && (g_best_time[g_track_idx] <= 0 || k->total_time < g_best_time[g_track_idx])) { g_best_time[g_track_idx] = k->total_time; if (g_track_idx < SAVE_MAX_ROADS) { g_save.best_ms[g_track_idx] = (int)(k->total_time * 1000); g_save.dirty = 1; } } }
                else if (!g_demo && g_mode != MODE_BATTLE) { Kart *k = &r->k[r->player]; g_save.races++; if (k->place == 1) { g_save.wins++; g_save.win_chars |= 1u << g_player_char; } g_save.dirty = 1; }
                g_fresh_unlocks = 0;
                if (g_mode == MODE_BATTLE && !g_demo) { g_save.battles++; if (g_battle.won) g_save.battle_wins++; g_save.dirty = 1; }
                if (g_mode == MODE_GP && g_gp_round + 1 >= NUM_GP_TRACKS && !g_demo) {   /* the championship is decided */
                    int best = 0; for (int c = 1; c < NCHARS; c++) if (g_gp_points[c] > g_gp_points[best]) best = c;
                    if (best == g_player_char) { g_save.gp_wins[g_difficulty]++; g_save.dirty = 1; } }
                g_fresh_unlocks = save_check_unlocks(); save_store(); race_set_unlocked(unlocked_mask()); race_set_styles(g_player_style, unlocked_styles());
                if (g_fresh_unlocks) sfx_play(SFX_UNLOCK, 1, 0); else if (!g_demo && r->k[r->player].place == 1) sfx_play(SFX_WIN, 1, 0);
                g_screen = SCR_RESULTS; g_screen_t = 0;
            }
        }
        break;
    case SCR_PAUSE:
        sim_running = 0;
        if (my) { g_pause_sel = (g_pause_sel - my + 5) % 5; sfx_play(SFX_MENU_MOVE, 1, 0); }
        if (mx && g_pause_sel >= 3) { int *v = g_pause_sel == 3 ? &g_save.music_vol : &g_save.sfx_vol; *v = *v + mx < 0 ? 0 : (*v + mx > 10 ? 10 : *v + mx); g_save.dirty = 1; apply_volumes(); sfx_play(SFX_MENU_MOVE, 1, 0); }
        if ((pa && g_pause_sel < 3) || pplus) {
            if (g_pause_sel == 0 || pplus) { g_screen = SCR_RACE; g_screen_t = 0; }
            else if (g_pause_sel == 1) { if (g_mode == MODE_BATTLE) begin_battle(); else start_race(0); g_screen = SCR_RACE; g_screen_t = 0; }
            else { if (g_mode == MODE_BATTLE) set_track(0); start_race(1); g_screen = SCR_MAIN; g_screen_t = 0; }
        }
        if (pb) { g_screen = SCR_RACE; g_screen_t = 0; save_store(); }
        break;
    case SCR_RESULTS:
        if (pa) { if (g_mode == MODE_GP) { g_gp_round++; g_screen = SCR_GP; g_screen_t = 0; } else if (g_mode == MODE_BATTLE) { begin_battle(); g_screen_t = 0; } else { start_race(0); g_screen = SCR_RACE; g_screen_t = 0; } }
        if (pb && g_mode != MODE_GP) { set_track(0); start_race(1); g_screen = SCR_MAIN; g_screen_t = 0; }
        break;
    case SCR_GP:
        if (pa) { if (g_gp_round >= NUM_GP_TRACKS) { start_race(1); g_screen = SCR_MAIN; g_screen_t = 0; } else { begin_player_race(); g_screen_t = 0; } }
        break;
    }
    /* the attract-mode race restarts itself when it ends */
    if (g_attract && r->state == RACE_FINISHED) { g_results_t += dt; if (g_results_t > 3.0f) start_race(1); }
    int in_race = g_screen == SCR_RACE || g_screen == SCR_PAUSE || g_screen == SCR_RESULTS || g_screen == SCR_GP;
    if (in_race && !g_attract) {
        int real_input = pad->source && (fabsf(pad->sx) > 0.15f || pad->a || pad->b || pad->zr || pad->zl);
        if (real_input) { g_idle = 0; g_demo = 0; } else { g_idle += dt; if (g_idle > 8.0f) g_demo = 1; }
    }
    if (g_msg_t > 0) g_msg_t -= dt;
    { Kart *pk = &r->k[r->player]; int racing = (g_screen == SCR_RACE) && !g_attract && r->state != RACE_FINISHED;
      sfx_engine(fabsf(pk->speed) / (36.0f * pk->st.top_speed), pk->drifting, pk->boost_t > 0, !pk->hit.on_road, pk->air, g_track.water, racing, g_screen == SCR_RACE && pad->a, g_screen == SCR_RACE && pad->b);
      static float ww_t = 0; if (racing && pk->wrongway_t > WRONGWAY_DELAY) { ww_t += dt; if (ww_t > 1.0f) { ww_t = 0; sfx_play(SFX_WRONG_WAY, 0.8f, 0); } } else ww_t = 1.0f; }
    music_want(pick_music()); music_set_duck(g_screen == SCR_PAUSE ? 0.3f : 1.0f); music_update(16384);

    /* fixed-step simulation */
    if (g_slowmo > 0) { g_slowmo -= dt; g_orbit += dt * 1.6f; }
    float sim_dt = g_slowmo > 0 ? dt * 0.3f : dt;
    static float acc = 0; if (sim_running) acc += sim_dt; else acc = 0;
    KartInput in; memset(&in, 0, sizeof in);
    if (g_screen == SCR_RACE) { in.steer = -pad->sx;   /* the sim turns toward +x for positive steer, and +x is screen-left from behind the kart */ in.gas = pad->a; in.brake = pad->b; in.drift = pad->zr; in.item = pad->zl || pad->x || pad->y; }
    int steps = 0;
    while (acc >= SIM_DT && steps < 8) {
        if (g_mode == MODE_BATTLE) {
            battle_step(&g_battle, &in, g_demo || g_attract, SIM_DT);
            for (int e = 0; e < g_battle.nev; e++) switch (g_battle.ev[e]) {
                case BEV_STONE_PICKED: sfx_play(SFX_COIN, 0.8f, 0); break;
                case BEV_STONE_THROWN: sfx_play(SFX_THROW, 1, 0); break;
                case BEV_GOLIATH_HIT: snprintf(g_msg, sizeof g_msg, "HIT!"); g_msg_t = 0.8f; sfx_play(SFX_HIT, 1, 0); add_dust(v3(g_battle.gx, 2, g_battle.gz), 3, 3); break;
                case BEV_DAVID_HIT: snprintf(g_msg, sizeof g_msg, "OUCH!"); g_msg_t = 0.8f; sfx_play(SFX_HIT, 1, 0); platform_rumble(1.0f, 0.45f); break;
                case BEV_SPEAR: sfx_play(SFX_THROW, 0.7f, 0); break;
                case BEV_ROAR: sfx_play(SFX_SHOFAR, 0.6f, 0); break;
                case BEV_CHARGE: snprintf(g_msg, sizeof g_msg, "HE CHARGES!"); g_msg_t = 1.0f; sfx_play(SFX_FIRE, 0.7f, 0); break;
                case BEV_WIN: snprintf(g_msg, sizeof g_msg, "GIANT SLAYER!"); g_msg_t = 3.0f; sfx_play(SFX_WIN, 1, 0); break;
                case BEV_STEP: { float ddx = g_battle.gx - r->k[0].x, ddz = g_battle.gz - r->k[0].z, dd = sqrtf(ddx * ddx + ddz * ddz); g_shake = fmaxf(g_shake, clampf(1.2f - dd / 50.0f, 0.1f, 1.0f) * (g_battle.gstate == GOL_CHARGE ? 0.5f : 0.3f)); sfx_play(SFX_BUMP, clampf(1.5f - dd / 60.0f, 0.2f, 1.0f), 0); platform_rumble(clampf(1.2f - dd / 50.0f, 0.1f, 0.8f), 0.15f); } break;
                case BEV_LOSE: snprintf(g_msg, sizeof g_msg, "GOLIATH WINS"); g_msg_t = 3.0f; sfx_play(SFX_WRONG_WAY, 1, 0); break;
                default: break; }
        } else race_step(r, &in, g_demo || g_attract, SIM_DT);
        acc -= SIM_DT; steps++;
        for (int e = 0; e < r->nev && !g_attract; e++) {
            RaceEvent *ev = &r->ev[e]; int mine = ev->kart == r->player;
            switch (ev->type) {
                case EV_COUNT_TICK: sfx_play(SFX_COUNT_TICK, 1, 0); break;
                case EV_GO: snprintf(g_msg, sizeof g_msg, "GO!"); g_msg_t = 1.0f; sfx_play(SFX_GO, 1, 0); break;
                case EV_LAP: if (mine) sfx_play(SFX_LAP, 1, 0); break;
                case EV_FINAL_LAP: if (mine) { snprintf(g_msg, sizeof g_msg, "FINAL LAP!"); g_msg_t = 2.0f; sfx_play(SFX_FINAL_LAP, 1, 0); } break;
                case EV_FINISH: if (mine) { snprintf(g_msg, sizeof g_msg, g_def->grid_idx ? "WELCOME HOME!" : "FINISH!"); g_msg_t = 3.0f; sfx_play(g_def->grid_idx ? SFX_WELCOME_HOME : SFX_FINISH, 1, 0); if (!g_attract && !g_demo) { g_slowmo = 1.1f; g_orbit = 0; } } break;
                case EV_HIT: if (mine) { snprintf(g_msg, sizeof g_msg, "OUCH!"); g_msg_t = 0.8f; platform_rumble(1.0f, 0.45f); } add_dust(v3(r->k[ev->kart].x, r->k[ev->kart].y, r->k[ev->kart].z), 2.0f, 3.0f); sfx_at(SFX_HIT, &r->k[ev->kart], 1.0f); break;
                case EV_BLOCK: if (mine) { snprintf(g_msg, sizeof g_msg, "BLOCKED!"); g_msg_t = 0.8f; } sfx_at(SFX_BLOCK, &r->k[ev->kart], 0.9f); break;
                case EV_BUMP: sfx_at(SFX_BUMP, &r->k[ev->kart], 0.7f); if (mine) platform_rumble(0.4f, 0.12f); break;
                case EV_TRICK: if (mine) { snprintf(g_msg, sizeof g_msg, "TRICK!"); g_msg_t = 0.6f; sfx_play(SFX_MINI_TURBO, 0.5f, 0); } break;
                case EV_TRICK_LAND: if (mine) { snprintf(g_msg, sizeof g_msg, "NICE LANDING!"); g_msg_t = 0.9f; sfx_play(SFX_MINI_TURBO, 0.9f, 0); platform_rumble(0.5f, 0.15f); } add_dust(v3(r->k[ev->kart].x, r->k[ev->kart].y, r->k[ev->kart].z), 1.0f, 2.0f); break;
                case EV_ROCKET_START: if (mine) { snprintf(g_msg, sizeof g_msg, "ROCKET START!"); g_msg_t = 1.2f; sfx_play(SFX_SUPER_TURBO, 1, 0); platform_rumble(0.7f, 0.35f); } break;
                case EV_WHEELSPIN: if (mine) { snprintf(g_msg, sizeof g_msg, "TOO EARLY!"); g_msg_t = 1.2f; sfx_play(SFX_WRONG_WAY, 0.8f, 0); platform_rumble(0.3f, 0.5f); } add_dust(v3(r->k[ev->kart].x, r->k[ev->kart].y, r->k[ev->kart].z), 1.5f, 2.5f); break;
                case EV_PICKUP: if (mine) { snprintf(g_msg, sizeof g_msg, "%s", item_name(r->k[ev->kart].item)); g_msg_t = 1.0f; sfx_play(SFX_PICKUP, 1, 0); } break;
                case EV_USE_MANNA: case EV_USE_STAFF: sfx_at(SFX_USE_ITEM, &r->k[ev->kart], 0.9f); break;
                case EV_USE_STONE: case EV_USE_OIL: case EV_USE_DOVE: sfx_at(ev->type == EV_USE_DOVE ? SFX_DOVE : SFX_THROW, &r->k[ev->kart], 0.9f); break;
                case EV_USE_FIRE: sfx_at(SFX_FIRE, &r->k[ev->kart], 1.0f); break;
                case EV_USE_LOCUSTS: sfx_at(SFX_LOCUSTS, &r->k[ev->kart], 1.0f); break;
                case EV_PAD_BOOST: if (mine) { snprintf(g_msg, sizeof g_msg, "BOOST!"); g_msg_t = 0.7f; platform_rumble(0.35f, 0.2f); } add_dust(v3(r->k[ev->kart].x, r->k[ev->kart].y, r->k[ev->kart].z), 1.0f, 2.0f); sfx_at(SFX_BOOST_PAD, &r->k[ev->kart], 0.9f); break;
                case EV_PAD_BOUNCE: if (mine) { snprintf(g_msg, sizeof g_msg, "BOING!"); g_msg_t = 0.7f; } sfx_at(SFX_BOUNCE, &r->k[ev->kart], 0.9f); break;
                case EV_COIN: if (mine) sfx_play(SFX_COIN, 0.8f, 0); break;
                case EV_SLOWED: if (mine) { snprintf(g_msg, sizeof g_msg, "LOCUSTS!"); g_msg_t = 1.5f; } break;
                case EV_USE_TRUMPET: for (int q = 0; q < 10; q++) add_dust(v3(r->k[ev->kart].x, r->k[ev->kart].y + 1, r->k[ev->kart].z), 6.0f, 2.5f); sfx_at(SFX_SHOFAR, &r->k[ev->kart], 1.0f); break;
                default: break;
            }
        }
        for (int i = 0; i < r->nk; i++) {
            Kart *q = &r->k[i];
            float hx = sinf(q->yaw + q->slide), hz = cosf(q->yaw + q->slide), rx = cosf(q->yaw + q->slide), rz = -sinf(q->yaw + q->slide);
            if (q->skid && !q->air) add_skid(i, v3(q->x - rx * 0.85f - hx * 0.8f, q->y + 0.04f, q->z - rz * 0.85f - hz * 0.8f), v3(q->x + rx * 0.85f - hx * 0.8f, q->y + 0.04f, q->z + rz * 0.85f - hz * 0.8f));
            else g_last_skid[i] = 0;
            if (i == r->player && q->mini_turbo_tier && !g_attract) { snprintf(g_msg, sizeof g_msg, q->mini_turbo_tier == 2 ? "SUPER TURBO!" : "MINI TURBO!"); g_msg_t = 1.2f; sfx_play(q->mini_turbo_tier == 2 ? SFX_SUPER_TURBO : SFX_MINI_TURBO, 1, 0); }
            if (i == r->player && !g_attract && g_was_air[i] && !q->air) { sfx_play(g_track.water && !q->hit.on_road ? SFX_SPLASH : SFX_LAND, 0.8f, 0); platform_rumble(0.3f, 0.1f); }
            g_was_air[i] = q->air;
        }
    }
    if (steps == 8) acc = 0;
    Kart *k = &r->k[r->player];

    /* effects */
    for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i];
        if (q->drifting && !q->air && q->speed > 5.0f) {       /* sparks from the rear wheel on the inside of the drift, colored by the charge */
            float hx = sinf(q->yaw + q->slide), hz = cosf(q->yaw + q->slide), rx = cosf(q->yaw + q->slide), rz = -sinf(q->yaw + q->slide);
            float side = (float)q->drift_dir; int tier = q->drift_charge >= 1.6f ? 2 : (q->drift_charge >= 0.8f ? 1 : 0);
            float cr = tier == 2 ? 1.0f : (tier == 1 ? 1.0f : 0.4f), cg = tier == 2 ? 0.5f : (tier == 1 ? 0.8f : 0.7f), cb = tier == 2 ? 1.0f : (tier == 1 ? 0.2f : 1.0f);
            for (int n2 = 0; n2 < 2; n2++) add_spark(v3(q->x + rx * side * 0.85f - hx * 0.8f, q->y + 0.1f, q->z + rz * side * 0.85f - hz * 0.8f),
                v3(rx * side * (2 + frand01() * 3) - hx * 4 + (frand01() - 0.5f) * 2, 1.5f + frand01() * 3, rz * side * (2 + frand01() * 3) - hz * 4 + (frand01() - 0.5f) * 2), 0.12f + 0.08f * tier, 0.25f + frand01() * 0.2f, cr, cg, cb); }
        if (q->boost_t > 0) {                                   /* flames out of the back */
            float hx = sinf(q->yaw), hz = cosf(q->yaw);
            for (int n2 = 0; n2 < 2; n2++) add_spark(v3(q->x - hx * 1.2f + (frand01() - 0.5f) * 0.5f, q->y + 0.4f + frand01() * 0.3f, q->z - hz * 1.2f + (frand01() - 0.5f) * 0.5f),
                v3(-hx * (6 + frand01() * 4), 1.0f + frand01(), -hz * (6 + frand01() * 4)), 0.35f + frand01() * 0.3f, 0.12f + frand01() * 0.12f, 1.0f, 0.55f + frand01() * 0.4f, 0.15f); }
        float near = (q->x - k->x) * (q->x - k->x) + (q->z - k->z) * (q->z - k->z) < 60.0f * 60.0f;
        if (near && !q->air && (!q->hit.on_road || q->drifting) && q->speed > 5.0f && frand01() < 0.6f) add_dust(v3(q->x, q->y, q->z), 1.5f, 1.5f);
        if (near && q->boost_t > 0 && frand01() < 0.9f) add_dust(v3(q->x - sinf(q->yaw) * 1.2f, q->y, q->z - cosf(q->yaw) * 1.2f), 0.6f, 1.5f); }
    if (sim_running) {
        for (int i = 0; i < MAX_DUST; i++) if (g_dust[i].life > 0) { Dust *d = &g_dust[i]; d->life -= dt; d->p.x += d->v.x * dt; d->p.y += d->v.y * dt; d->p.z += d->v.z * dt; d->v.y -= 2.0f * dt; d->size += 1.2f * dt; }
        for (int i = 0; i < MAX_SKID; i++) if (g_skid[i].used) { g_skid[i].a -= dt * 0.02f; if (g_skid[i].a <= 0) g_skid[i].used = 0; }
        for (int i = 0; i < MAX_SPARK; i++) if (g_spark[i].life > 0) { Spark *s = &g_spark[i]; s->life -= dt; s->p.x += s->v.x * dt; s->p.y += s->v.y * dt; s->p.z += s->v.z * dt; s->v.y -= 9.0f * dt; }
    }

    /* chase camera: sits behind the kart, lags on turns, pulls back with speed */
    float want_yaw = k->yaw + k->slide * 0.3f + (g_slowmo > 0 ? g_orbit : 0);
    float dy = want_yaw - cam_yaw; while (dy > 3.14159f) dy -= 6.28318f; while (dy < -3.14159f) dy += 6.28318f;
    cam_yaw += dy * fminf(1.0f, 5.0f * dt);
    float back = 4.6f + k->speed * 0.12f, up = 2.7f + k->speed * 0.02f;   /* close when stopped (the kart behind on the grid is 5 m back), pulls back with speed */
    float tx = k->x - sinf(cam_yaw) * back, tz = k->z - cosf(cam_yaw) * back, ty = k->y + up;
    cam_x += (tx - cam_x) * fminf(1.0f, 12.0f * dt); cam_z += (tz - cam_z) * fminf(1.0f, 12.0f * dt); cam_y += (ty - cam_y) * fminf(1.0f, 6.0f * dt);
    float want_fov = 62.0f + (k->boost_t > 0 ? 10.0f : 0.0f) + k->speed * 0.15f;
    cam_fov += (want_fov - cam_fov) * fminf(1.0f, 4.0f * dt);
    if (g_shake > 0) g_shake = fmaxf(0, g_shake - dt * 1.5f);
    float shk = g_shake * g_shake * 0.6f;
    Vec3 eye = v3(cam_x + sinf(g_time * 37.0f) * shk, cam_y + sinf(g_time * 53.0f) * shk, cam_z + cosf(g_time * 41.0f) * shk), at = v3(k->x + sinf(k->yaw) * 3.0f, k->y + 1.2f, k->z + cosf(k->yaw) * 3.0f);
    /* dynamic geometry shared by both screens: trees, skid marks, dust, shadows, items */
    float cy = cam_yaw; Vec3 ax = v3(-cosf(cy), 0, sinf(cy));   /* screen-right in world space, so pictures are not mirrored */
    {
        for (int kd = 0; kd < g_nkind; kd++) {
            batch_clear(&g_b);
            for (int i = 0; i < g_nplaced; i++) { Placed *pl = &g_placed[i]; if (pl->kind != kd) continue;
                float dx = pl->p.x - cam_x, dz = pl->p.z - cam_z; if (dx * dx + dz * dz > SPRITE_DRAW_DIST * SPRITE_DRAW_DIST) continue;
                float w = pl->w * 0.5f;
                float v0 = pl->falls ? g_time * 1.5f : 0.0f;
                batch_quad(&g_b, v3(pl->p.x, pl->p.y + pl->h * 0.5f, pl->p.z), v3(ax.x * w, 0, ax.z * w), v3(0, pl->h * 0.5f, 0), v3(0, 1, 0), 0, v0, 1, v0 + (pl->falls ? pl->h / 3.0f : 1.0f), 1, 1, 1, 1); }
            for (int m = 0; m < g_track.nmover; m++) { if (g_mover_kind[m] != kd) continue; const Mover *mv = &g_track.mover[m];
                float mx, my, mz; track_mover_pos(&g_track, mv, r->race_t, &mx, &my, &mz); float w = mv->height * g_kind[kd].aspect * 0.5f;
                batch_quad(&g_b, v3(mx, my + mv->height * 0.5f, mz), v3(ax.x * w, 0, ax.z * w), v3(0, mv->height * 0.5f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 1); }
            gfx_mesh_update(g_kind[kd].mesh, g_b.v, g_b.n);
        }
        /* warm glows at lanterns and windows, and the Star */
        batch_clear(&g_b);
        for (int i = 0; i < g_nplaced; i++) { Placed *pl = &g_placed[i]; if (pl->glow <= 0) continue;
            float dx = pl->p.x - cam_x, dz = pl->p.z - cam_z; if (dx * dx + dz * dz > SPRITE_DRAW_DIST * SPRITE_DRAW_DIST) continue;
            float fl = 0.85f + 0.15f * sinf(g_time * 9.0f + i * 1.7f), s = pl->glow * fl;
            if (pl->purple) batch_quad(&g_b, v3(pl->p.x, pl->p.y + pl->h * 0.55f, pl->p.z), v3(ax.x * s, 0, ax.z * s), v3(0, s, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.7f, 0.35f, 1.0f, 0.6f);
            else batch_quad(&g_b, v3(pl->p.x, pl->p.y + pl->h * 0.55f, pl->p.z), v3(ax.x * s, 0, ax.z * s), v3(0, s, 0), v3(0, 1, 0), 0, 0, 1, 1, g_def->glow_r, g_def->glow_g, g_def->glow_b, 0.55f); }
        gfx_mesh_update(m_glow, g_b.v, g_b.n);
        batch_clear(&g_b);
        for (int i = 0; i < MAX_SKID; i++) if (g_skid[i].used) { Skid *s = &g_skid[i];
            batch_quad4(&g_b, s->l0, s->l1, s->r1, s->r0, v3(0, 1, 0), 0, 0, 1, 1, 0.05f, 0.05f, 0.05f, s->a); }
        gfx_mesh_update(m_skid, g_b.v, g_b.n);
        batch_clear(&g_b);
        for (int i = 0; i < MAX_DUST; i++) if (g_dust[i].life > 0) { Dust *d = &g_dust[i]; float s = d->size * 0.5f;
            if (g_track.water) batch_quad(&g_b, d->p, v3(ax.x * s, 0, ax.z * s), v3(0, s, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.8f, 0.92f, 1.0f, d->life * 0.8f);
            else batch_quad(&g_b, d->p, v3(ax.x * s, 0, ax.z * s), v3(0, s, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.85f, 0.8f, 0.7f, d->life * 0.7f); }
        gfx_mesh_update(m_dust, g_b.v, g_b.n);
        batch_clear(&g_b);
        for (int i = 0; i < MAX_SPARK; i++) if (g_spark[i].life > 0) { Spark *sp = &g_spark[i]; float s = sp->size;
            batch_quad(&g_b, sp->p, v3(ax.x * s, 0, ax.z * s), v3(0, s, 0), v3(0, 1, 0), 0, 0, 1, 1, sp->r, sp->g, sp->b, clampf(sp->life * 4.0f, 0, 1)); }
        gfx_mesh_update(m_spark, g_b.v, g_b.n);
        batch_clear(&g_b);
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; TrackHit h2; track_locate(&g_track, q->x, q->z, q->hit.idx, &h2);
            float hx = sinf(q->yaw), hz = cosf(q->yaw);
            batch_quad(&g_b, v3(q->x, h2.height + 0.05f, q->z), v3(cosf(q->yaw) * 1.1f, 0, -sinf(q->yaw) * 1.1f), v3(hx * 1.3f, 0, hz * 1.3f), v3(0, 1, 0), 0, 0, 1, 1, 0, 0, 0, 0.45f * (1.0f - clampf((q->y - h2.height) / 6.0f, 0, 0.8f))); }
        for (int m = 0; m < g_track.nmover; m++) { const Mover *mv = &g_track.mover[m]; float mx, my, mz; track_mover_pos(&g_track, mv, r->race_t, &mx, &my, &mz);
            batch_quad(&g_b, v3(mx, my + 0.05f, mz), v3(mv->radius, 0, 0), v3(0, 0, -mv->radius), v3(0, 1, 0), 0, 0, 1, 1, 0, 0, 0, 0.4f); }
        if (g_mode == MODE_BATTLE) { TrackHit h; track_locate(&g_track, g_battle.gx, g_battle.gz, -1, &h); batch_quad(&g_b, v3(g_battle.gx, h.height + 0.05f, g_battle.gz), v3(2.6f, 0, 0), v3(0, 0, -2.6f), v3(0, 1, 0), 0, 0, 1, 1, 0, 0, 0, 0.5f); }
        gfx_mesh_update(m_shadow, g_b.v, g_b.n);
        /* oil puddles, flying stones and the dove as sprites */
        batch_clear(&g_b);
        for (int p = 0; p < MAX_PROJ; p++) { Proj *pr = &r->proj[p]; if (!pr->alive) continue;
            if (pr->type == ITEM_OIL) batch_quad(&g_b, v3(pr->x, pr->y + 0.06f, pr->z), v3(1.6f, 0, 0), v3(0, 0, -1.6f), v3(0, 1, 0), 0, 0, 1, 1, 0.08f, 0.05f, 0.02f, 0.9f);
            else if (pr->type == ITEM_DOVE) { float fl = 0.7f + 0.3f * sinf(g_time * 20.0f); batch_quad(&g_b, v3(pr->x, pr->y, pr->z), v3(ax.x * 0.9f * fl, 0, ax.z * 0.9f * fl), v3(0, 0.5f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 1); }
            else batch_quad(&g_b, v3(pr->x, pr->y + 0.3f, pr->z), v3(ax.x * 0.45f, 0, ax.z * 0.45f), v3(0, 0.45f, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.5f, 0.5f, 0.5f, 1); }
        gfx_mesh_update(m_items, g_b.v, g_b.n);
    }

    /* ---- draw both screens */
    gfx_frame_begin();
    for (int drc = 0; drc < 2; drc++) {
        if (g_status == GFX_NO_COMPILER) { gfx_view_begin(drc, 1.0f, 0.5f, 0.0f); gfx_view_end(drc); continue; }
        if (g_status == GFX_SHADER_FAILED) { gfx_view_begin(drc, 0.8f, 0.05f, 0.05f); gfx_view_end(drc); continue; }
        int gamepad_map = drc && (g_screen == SCR_RACE || g_screen == SCR_PAUSE) && !g_attract;
#ifdef KK_TOUR
        if (!drc && g_time > 98.0f && g_time < 103.0f && g_screen == SCR_RACE) gamepad_map = 1;   /* the CI camera only sees the TV: show the GamePad layout there for one photo */
#endif
        if (gamepad_map) { gfx_view_begin(drc, 0.04f, 0.07f, 0.18f); gfx_camera_2d(); gfx_state(ST_HUD); Mat4 id2 = m4_identity();
            batch_clear(&g_b); hud_gamepad(); if (g_screen == SCR_PAUSE) hud_pause(); gfx_mesh_update(m_hud2, g_b.v, g_b.n); gfx_bind(tx_font); gfx_draw(m_hud2, &id2, 1, 1, 1, 1); gfx_view_end(drc); continue; }
        gfx_view_begin(drc, g_def->sky_r, g_def->sky_g, g_def->sky_b);
        Mat4 view = m4_look_at(eye, at, v3(0, 1, 0));
        Mat4 proj = m4_perspective(cam_fov * 3.14159265f / 180.0f, gfx_aspect(drc), 0.3f, 1200.0f);
        Mat4 vp = m4_mul(proj, view);
        Mat4 id = m4_identity();
        Vec3 ldir = v3(g_def->light_x, g_def->light_y, g_def->light_z);
        gfx_camera(&vp, ldir, 1.0f, g_def->fog_r, g_def->fog_g, g_def->fog_b, 1e6f, 2e6f);
        gfx_state(ST_BLEND); gfx_bind(tx_sky); { Mat4 sm = m4_translate(cam_x, -30.0f, cam_z); gfx_draw(m_sky, &sm, 1, 1, 1, 1); }
        if (tx_star) { gfx_state(ST_BLEND | ST_ADD); gfx_bind(tx_star);
            batch_clear(&g_b); float ss = g_def->star_size * (0.95f + 0.05f * sinf(g_time * 3.0f));
            Vec3 sp = v3(cam_x + g_def->star_x, g_def->star_y, cam_z + g_def->star_z);
            batch_quad(&g_b, sp, v3(ax.x * ss, 0, ax.z * ss), v3(0, ss, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 1);
            static Mesh *m_star = NULL; if (!m_star) m_star = gfx_mesh_dynamic(12); gfx_mesh_update(m_star, g_b.v, g_b.n); gfx_draw(m_star, &id, 1, 1, 1, 1); }
        gfx_camera(&vp, ldir, g_def->ambient, g_def->fog_r, g_def->fog_g, g_def->fog_b, g_def->fog_start, g_def->fog_end);
        gfx_state(ST_SOLID);
        float sr = g_def->sun_r, sg = g_def->sun_g, sb = g_def->sun_b;
        gfx_bind(tx_grass); gfx_draw(m_ground, &id, sr, sg, sb, 1);
        gfx_bind(tx_dirt); gfx_draw(m_shoulder, &id, sr, sg, sb, 1);
        gfx_bind(tx_road); gfx_draw(m_road, &id, sr, sg, sb, 1);
        if (g_def->bonus != 2) { gfx_bind(tx_flag); gfx_draw(m_startline, &id, sr, sg, sb, 1); }
        gfx_bind(tx_wall); gfx_draw(m_wall, &id, sr, sg, sb, 1);
        gfx_bind(tx_wall); gfx_draw(m_tunnel, &id, sr * 0.8f, sg * 0.8f, sb * 0.8f, 1);
        if (g_def->bonus != 2) { gfx_bind(tx_flag); gfx_draw(m_arch, &id, sr, sg, sb, 1); }
        if (g_def->bonus == 2 && m_brook) { static Texture *tx_brook = NULL; if (!tx_brook) tx_brook = make_tex(128, 128, 1, 0, px_water); gfx_state(ST_TRANSPARENT); gfx_bind(tx_brook);
            Mat4 flow = m4_translate(sinf(g_time * 2.0f) * 0.3f, 0, 0); gfx_draw(m_brook, &flow, 1, 1, 1, 0.75f); gfx_state(ST_SOLID); }
        { float pulse = 0.75f + 0.25f * sinf(g_time * 6.0f);
          gfx_state(ST_TRANSPARENT); gfx_bind(NULL); gfx_draw(m_boostpads, &id, pulse, pulse, pulse, 1);
          gfx_bind(tx_blob); gfx_draw(m_bouncepads, &id, 1, 1, 1, 0.9f);
          gfx_state(ST_SOLID); }
        gfx_state(ST_CUTOUT);
        for (int kd = 0; kd < g_nkind; kd++) { gfx_bind(g_kind[kd].tex); gfx_draw(g_kind[kd].mesh, &id, g_def->sun_r, g_def->sun_g, g_def->sun_b, 1); }
        if (tx_finale && m_finale) { gfx_camera(&vp, ldir, 1.0f, g_def->fog_r, g_def->fog_g, g_def->fog_b, 1e6f, 2e6f); gfx_state(ST_CUTOUT); gfx_bind(tx_finale); gfx_draw(m_finale, &id, 1, 1, 1, 1);
            gfx_camera(&vp, ldir, g_def->ambient, g_def->fog_r, g_def->fog_g, g_def->fog_b, g_def->fog_start, g_def->fog_end); }
        gfx_state(ST_TRANSPARENT); gfx_bind(NULL); gfx_draw(m_skid, &id, 1, 1, 1, 1);
        gfx_bind(tx_blob); gfx_draw(m_shadow, &id, 1, 1, 1, 1);
        /* item boxes: spinning cubes */
        gfx_state(ST_TRANSPARENT | ST_CULL); gfx_bind(NULL);
        for (int b = 0; b < r->nbox; b++) { ItemBox *bx = &r->box[b]; if (!bx->active) continue;
            Mat4 m = m4_mul(m4_translate(bx->x, bx->y + 0.15f * sinf(g_time * 2.0f + b), bx->z), m4_mul(m4_rotate_y(g_time * 1.5f + b), m4_rotate_x(0.6f)));
            gfx_draw(m_cube, &m, 0.3f, 0.6f, 1.0f, 0.55f);
            Mat4 m2 = m4_mul(m, m4_scale(0.5f, 0.5f, 0.5f)); gfx_draw(m_cube, &m2, 1.0f, 0.85f, 0.3f, 0.9f); }
        /* coins: spinning discs */
        gfx_state(ST_SOLID); gfx_bind(NULL);
        for (int c = 0; c < g_track.ncoin; c++) { if (r->coin_respawn[c] > 0) continue; const Coin *cn = &g_track.coin[c]; const TrackSample *cs = &g_track.s[cn->idx];
            float cx = cs->x + cs->nx * cn->lateral, cz = cs->z + cs->nz * cn->lateral; float ddx = cx - cam_x, ddz = cz - cam_z; if (ddx * ddx + ddz * ddz > 160.0f * 160.0f) continue;
            Mat4 m = m4_mul(m4_translate(cx, track_surface_height(&g_track, cn->idx, cn->lateral) + 1.0f + 0.1f * sinf(g_time * 3.0f + c), cz), m4_rotate_y(g_time * 3.0f + c * 0.7f));
            gfx_draw(m_coin, &m, 1, 1, 1, 1); }
        /* the karts */
        gfx_state(ST_SOLID); gfx_bind(NULL);
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; const Character *c = &CHARACTERS[r->character[i]];
            if (i != r->player) { float cdx = q->x - eye.x, cdz = q->z - eye.z; if (cdx * cdx + cdz * cdz < 3.2f * 3.2f) continue; }   /* a kart sitting on the camera would fill the screen */
            float pitch = -atan2f(q->hit.slope, 1.0f) * (q->air ? 0.3f : 1.0f) + (q->air ? -q->vy * 0.03f : 0.0f);
            float roll = q->hit.on_road ? -g_track.s[q->hit.idx].bank : 0.0f;
            float sq = 1.0f - q->squash * 0.25f;
            Mat4 m = m4_mul(m4_translate(q->x, q->y + q->hop * 0.4f, q->z), m4_mul(m4_rotate_y(q->yaw + q->slide + q->spin_vis), m4_mul(m4_rotate_x(pitch), m4_mul(m4_rotate_z(roll + q->steer_vis * -0.04f), m4_scale(1.0f + (1 - sq) * 0.5f, sq, 1.0f)))));
            int sty = r->style[i] >= 0 && r->style[i] < NSTYLES ? r->style[i] : 0;
            if (tx_kartback[r->character[i]]) continue;      /* drawn as a picture below */
            gfx_draw(m_body_style[sty], &m, c->r, c->g, c->b, 1);
            gfx_draw(m_rest_style[sty], &m, 1, 1, 1, 1);
            if (!tx_char[r->character[i]]) { gfx_draw(m_driver_box, &m, c->r, c->g, c->b, 1); gfx_draw(m_driver_head, &m, 1, 1, 1, 1); } }
        /* drivers drawn from Sean's character pictures: a standing cut-out in the seat, always facing the camera */
        gfx_state(ST_CUTOUT);
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; int ch = r->character[i];
            if (i != r->player) { float cdx = q->x - eye.x, cdz = q->z - eye.z; if (cdx * cdx + cdz * cdz < 3.2f * 3.2f) continue; }
            float hx = sinf(q->yaw + q->slide), hz = cosf(q->yaw + q->slide);
            int pic = tx_kartback[ch] != NULL;
            if (pic) {    /* Sean's kart as a standing picture: the view that best matches the angle the camera sees it from (N64 style) */
                float cdx = eye.x - q->x, cdz = eye.z - q->z;
                float phi = atan2f(cdx, cdz) - (q->yaw + q->slide + q->spin_vis + q->trick_vis) - 3.14159265f;      /* 0 = camera straight behind */
                while (phi > 3.14159265f) phi -= 6.2831853f; while (phi < -3.14159265f) phi += 6.2831853f;
                int mirror = phi > 0; float a = fabsf(phi) * 180.0f / 3.14159265f;
                int want = a < 22.5f ? 0 : (a < 67.5f ? 1 : (a < 112.5f ? 2 : (a < 157.5f ? 3 : 4)));
                int v = want; while (v > 0 && !tx_kartview[ch][v]) v--;                                /* fall back toward the back view when an angle is missing */
                if (want == 4 && !tx_kartview[ch][4]) v = tx_kartview[ch][3] ? 3 : (tx_kartview[ch][2] ? 2 : 0);
                float kw = v == 2 ? 3.4f : (v == 1 || v == 3 ? 3.0f : 2.6f), kh = kw / g_kartview_aspect[ch][v], klean = q->steer_vis * -0.08f + q->slide * 0.15f;
                Vec3 kc = v3(q->x, q->y + q->hop * 0.4f + kh * 0.5f - 0.1f, q->z);
                batch_clear(&g_b);
                float u0 = mirror ? 1.0f : 0.0f, u1 = mirror ? 0.0f : 1.0f;
                batch_quad(&g_b, kc, v3(ax.x * kw * 0.5f, 0, ax.z * kw * 0.5f), v3(ax.x * klean * kh * 0.5f, kh * 0.5f, ax.z * klean * kh * 0.5f), v3(0, 1, 0), u0, 0, u1, 1, 1, 1, 1, 1);
                gfx_mesh_update(m_kart_spr[i], g_b.v, g_b.n); gfx_bind(tx_kartview[ch][v]); float st = q->stun > 0 ? 0.6f : 1.0f; gfx_draw(m_kart_spr[i], &id, sr * st, sg * st, sb * st, 1); }
            if (!tx_char[ch]) continue;
            float hgt = pic ? 1.5f : 1.75f, hw = hgt * 0.5f * g_char_aspect[ch];
            Vec3 c = v3(q->x - hx * 0.25f, q->y + q->hop * 0.4f + (pic ? 0.75f : 0.55f) + hgt * 0.5f, q->z - hz * 0.25f);
            float lean = q->steer_vis * -0.12f;       /* leans into the turn */
            batch_clear(&g_b);
            batch_quad(&g_b, c, v3(ax.x * hw, 0, ax.z * hw), v3(ax.x * lean * hgt * 0.5f, hgt * 0.5f, ax.z * lean * hgt * 0.5f), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 1);
            gfx_mesh_update(m_driver_spr[i], g_b.v, g_b.n); gfx_bind(tx_char[ch]); gfx_draw(m_driver_spr[i], &id, sr, sg, sb, 1); }
        gfx_state(ST_SOLID); gfx_bind(NULL);
        if (g_mode == MODE_BATTLE) {
            Battle *b = &g_battle;
            /* stones on the ground and in the air, spears */
            for (int i = 0; i < BATTLE_STONES; i++) { if (b->stone_respawn[i] > 0) continue; TrackHit h; track_locate(&g_track, b->stone_x[i], b->stone_z[i], -1, &h);
                Mat4 m = m4_mul(m4_translate(b->stone_x[i], h.height + 0.4f + 0.1f * sinf(g_time * 3 + i), b->stone_z[i]), m4_rotate_y(g_time + i)); Mat4 m2 = m4_mul(m, m4_scale(0.5f, 0.5f, 0.5f)); gfx_draw(m_cube, &m2, 0.55f, 0.52f, 0.5f, 1); }
            for (int i = 0; i < BATTLE_SPEARS; i++) { Missile *ms = &b->stone[i]; if (!ms->alive) continue; Mat4 m = m4_mul(m4_translate(ms->x, ms->y, ms->z), m4_scale(0.35f, 0.35f, 0.35f)); gfx_draw(m_cube, &m, 0.55f, 0.52f, 0.5f, 1); }
            for (int i = 0; i < BATTLE_SPEARS; i++) { Missile *ms = &b->spear[i]; if (!ms->alive) continue;
                Mat4 m = m4_mul(m4_translate(ms->x, ms->y, ms->z), m4_mul(m4_rotate_y(atan2f(ms->vx, ms->vz)), m4_scale(0.12f, 0.12f, 2.2f))); gfx_draw(m_cube, &m, 0.55f, 0.35f, 0.2f, 1);
                Mat4 m2 = m4_mul(m4_translate(ms->x + ms->vx / 30.0f * 1.5f, ms->y, ms->z + ms->vz / 30.0f * 1.5f), m4_scale(0.25f, 0.25f, 0.25f)); gfx_draw(m_cube, &m2, 0.8f, 0.8f, 0.85f, 1); }
            /* Goliath himself: a giant standing picture, leaning into his charge, shaking when stunned */
            if (tx_goliath) { gfx_state(ST_CUTOUT); TrackHit h; track_locate(&g_track, b->gx, b->gz, -1, &h);
                float hgt = 7.5f, hw = hgt * 0.5f * g_goliath_aspect; float shake = b->gstate == GOL_STUNNED ? sinf(g_time * 40) * 0.3f : 0.0f;
                float lean = b->gstate == GOL_CHARGE ? 0.25f : (b->gstate == GOL_WINDUP ? -0.15f : 0.0f); float fx = sinf(b->gyaw), fz = cosf(b->gyaw);
                Vec3 c = v3(b->gx + shake + fx * lean * hgt * 0.3f, h.height + hgt * 0.5f * (b->gstate == GOL_DOWN ? 0.3f : 1.0f), b->gz + fz * lean * hgt * 0.3f);
                batch_clear(&g_b);
                if (b->gstate == GOL_DOWN) batch_quad(&g_b, c, v3(ax.x * hw, 0, ax.z * hw), v3(fx * hgt * 0.5f, hgt * 0.15f, fz * hgt * 0.5f), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 1);   /* fallen */
                else batch_quad(&g_b, c, v3(ax.x * hw, 0, ax.z * hw), v3(fx * lean * hgt * 0.5f, hgt * 0.5f, fz * lean * hgt * 0.5f), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 1);
                gfx_mesh_update(m_goliath, g_b.v, g_b.n); gfx_bind(tx_goliath);
                float tint = b->gstate == GOL_STUNNED ? 0.5f : 1.0f; gfx_draw(m_goliath, &id, sr, sg * tint, sb * tint, 1);
                gfx_state(ST_SOLID); gfx_bind(NULL); }
        }
        /* shields */
        gfx_state(ST_TRANSPARENT); gfx_bind(NULL);
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; if (q->shield <= 0) continue;
            Mat4 m = m4_mul(m4_translate(q->x, q->y, q->z), m4_rotate_y(g_time * 2.0f));
            gfx_draw(m_shield, &m, 0.4f, 0.8f, 1.0f, 0.25f + 0.1f * sinf(g_time * 8)); }
        gfx_bind(tx_blob); gfx_draw(m_items, &id, 1, 1, 1, 1);
        gfx_state(ST_TRANSPARENT | ST_ADD); gfx_draw(m_dust, &id, 1, 1, 1, 1); gfx_draw(m_spark, &id, 1, 1, 1, 1); gfx_draw(m_glow, &id, 1, 1, 1, 1);


        /* ---- HUD and menus */
        gfx_camera_2d(); gfx_state(ST_HUD);
        batch_clear(&g_b);
        int menu_screen = g_screen == SCR_TITLE || g_screen == SCR_MAIN || g_screen == SCR_CHAR || g_screen == SCR_KART || g_screen == SCR_TRACK || g_screen == SCR_GP;
        if (menu_screen) batch_rect(&g_b, 0, 0, 1280, 720, 0.02f, 0.03f, 0.1f, g_screen == SCR_TITLE ? 0.25f : 0.55f);
        switch (g_screen) {
            case SCR_TITLE: hud_title(); break;
            case SCR_MAIN: hud_main(); break;
            case SCR_CREDITS: hud_credits(); break;
            case SCR_OPTIONS: hud_options(); break;
            case SCR_CHAR: hud_char(); break;
            case SCR_KART: hud_kart(); break;
            case SCR_TRACK: hud_track(); break;
            case SCR_RACE: if (g_mode == MODE_BATTLE) hud_battle(); else hud_race(pad); break;
            case SCR_PAUSE: if (g_mode == MODE_BATTLE) hud_battle(); else hud_race(pad); hud_pause(); break;
            case SCR_RESULTS: if (g_mode == MODE_BATTLE) hud_battle_results(); else hud_results(); break;
            case SCR_GP: hud_gp(); break;
        }
        { float frac = clampf(g_frame_ms / 33.3f, 0.02f, 1.0f); int good = g_frame_ms <= 16.8f;
          batch_rect(&g_b, 20, 708, 300 * frac, 6, good ? 0.2f : 1.0f, good ? 1.0f : 0.2f, 0.2f, 0.8f);
          batch_rect(&g_b, 20 + 300 * (16.7f / 33.3f), 704, 2, 14, 1, 1, 1, 0.8f); }
        batch_rect(&g_b, 1220, 690, 20, 20, pad->source & 2 ? 1.0f : (pad->source & 1 ? 0.2f : 0.5f), pad->source & 2 ? 0.8f : (pad->source & 1 ? 0.5f : 0.5f), pad->source ? 0.2f : 0.5f, 0.9f);
        if (g_screen == SCR_TITLE && tx_title && m_title) { gfx_bind(tx_title); gfx_draw(m_title, &id, 1, 1, 1, 1); }
        gfx_mesh_update(m_hud, g_b.v, g_b.n);
        gfx_bind(tx_font); gfx_draw(m_hud, &id, 1, 1, 1, 1);
        if (g_screen == SCR_CHAR) draw_portraits();
        if (g_screen == SCR_TRACK) for (int d = 0; d < NUM_PLAY_TRACKS && d < 8; d++) { if (!tx_card[d] || !track_unlocked(d)) continue;
            if (!m_card[d]) { float x = 40 + d * 245 + 12, y = 110 + 12; batch_clear(&g_b); batch_rect_uv(&g_b, x, y, 200, 108, 0, 0, 1, 1, 1, 1, 1, 1); m_card[d] = gfx_mesh_static(g_b.v, g_b.n); }
            gfx_bind(tx_card[d]); gfx_draw(m_card[d], &id, 1, 1, 1, 1); }
        if (g_screen == SCR_KART) { draw_kart_models(); gfx_camera_2d(); gfx_state(ST_HUD);
            if (tx_kart[g_player_char]) { int two = tx_kartback[g_player_char] != NULL; float a = g_kart_aspect[g_player_char], ph = 220, pw = ph * a; if (pw > (two ? 330 : 500)) { pw = two ? 330 : 500; ph = pw / a; }
                float cx = two ? 470 : 640;
                batch_clear(&g_b); batch_rect_uv(&g_b, cx - pw / 2, 67 + (220 - ph) / 2, pw, ph, 0, 0, 1, 1, 1, 1, 1, 1); gfx_mesh_update(m_kartpic, g_b.v, g_b.n); gfx_bind(tx_kart[g_player_char]); gfx_draw(m_kartpic, &id, 1, 1, 1, 1);
                if (two) { static Mesh *m2 = NULL; if (!m2) m2 = gfx_mesh_dynamic(6); a = g_kartback_aspect[g_player_char]; ph = 220; pw = ph * a; if (pw > 330) { pw = 330; ph = pw / a; }
                    batch_clear(&g_b); batch_rect_uv(&g_b, 810 - pw / 2, 67 + (220 - ph) / 2, pw, ph, 0, 0, 1, 1, 1, 1, 1, 1); gfx_mesh_update(m2, g_b.v, g_b.n); gfx_bind(tx_kartback[g_player_char]); gfx_draw(m2, &id, 1, 1, 1, 1); } } }
        if ((g_screen == SCR_RACE || g_screen == SCR_PAUSE) && g_mode != MODE_TT) { int it = r->k[r->player].item;
            if (it > 0 && it < ITEM_COUNT && tx_item[it]) { static Mesh *im = NULL; if (!im) im = gfx_mesh_dynamic(6);
                batch_clear(&g_b); batch_rect_uv(&g_b, 48, 146, 134, 48, 0, 0, 1, 1, 1, 1, 1, 1);
                gfx_mesh_update(im, g_b.v, g_b.n); gfx_bind(tx_item[it]); gfx_draw(im, &id, 1, 1, 1, 1); } }
        gfx_view_end(drc);
    }
    gfx_frame_end();
}
int game_debug_screen(void) { return g_screen + 20 * g_mode + 100 * g_menu_sel + 1000 * (int)g_race.state; }
