// view.c - draws the race: track, eight karts, items, effects, HUD, results. The rules live in sim/race.c.
#include "game.h"
#include "save.h"
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
#define NUM_PLAY_TRACKS (NUM_TRACK_DEFS - 1)     /* the last road is the developers' Test Loop, not offered to players */
#define NUM_GP_TRACKS 4                          /* the Grand Prix runs the four story roads; bonus roads are for Single Race and Time Trial */
#ifndef KK_TOUR_ROAD
#define KK_TOUR_ROAD 1
#endif

static int g_status;
static Track g_track; static const TrackDef *g_def; static int g_track_idx = 0;
static Race g_race;
static int g_player_char = 0, g_difficulty = DIFF_NORMAL;
/* game flow */
enum { SCR_TITLE, SCR_MAIN, SCR_CHAR, SCR_KART, SCR_TRACK, SCR_RACE, SCR_PAUSE, SCR_RESULTS, SCR_GP };
enum { MODE_GP, MODE_SINGLE, MODE_TT };
static int g_screen = SCR_TITLE, g_mode = MODE_SINGLE, g_menu_sel = 0, g_pause_sel = 0, g_attract = 1;
static int char_unlocked(int c) { return !CHARACTERS[c].bonus || (c == 8 && (g_save.unlocks & UNLOCK_NOAH)) || (c == 9 && (g_save.unlocks & UNLOCK_ELIJAH)); }
static int char_lock_bit(int c) { return c == 8 ? UNLOCK_NOAH : (c == 9 ? UNLOCK_ELIJAH : 0); }
static int style_lock_bit(int sty) { return sty == 1 ? UNLOCK_KART_CHARIOT : (sty == 2 ? UNLOCK_KART_BOAT : (sty == 3 ? UNLOCK_KART_ARK : 0)); }
static int style_unlocked(int sty) { return sty == 0 || (g_save.unlocks & style_lock_bit(sty)); }
static unsigned unlocked_styles(void) { unsigned m = 0; for (int i = 0; i < NSTYLES; i++) if (style_unlocked(i)) m |= 1u << i; return m; }
static int g_player_style = 0;
static int track_unlocked(int d) { return !TRACK_DEFS[d].bonus || (g_save.unlocks & UNLOCK_HEAVEN); }
static unsigned unlocked_mask(void) { unsigned m = 0; for (int c = 0; c < NCHARS; c++) if (char_unlocked(c)) m |= 1u << c; return m; }
static int g_gp_round = 0, g_gp_points[NCHARS], g_gp_order[NUM_GP_ROUNDS_MAX];
static float g_best_time[8];                 /* best time trial time per road (loaded from the save file) */
static Texture *tx_portrait[NCHARS];
static unsigned g_fresh_unlocks;             /* unlock bits earned by the race just finished, shown on the results */
static Texture *tx_finale; static Mesh *m_finale;
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
static Mesh *m_boostpads, *m_bouncepads, *m_road, *m_shoulder, *m_ground, *m_wall, *m_startline, *m_kart_body, *m_kart_rest, *m_body_style[NSTYLES], *m_rest_style[NSTYLES], *m_cube, *m_shield, *m_items, *m_skid, *m_glow, *m_dust, *m_hud, *m_shadow, *m_sky;
static Vertex g_vtmp[8192];
static Batch g_b;

/* camera state */
static float cam_yaw, cam_x, cam_y, cam_z, cam_fov = 62.0f;
/* skid marks: ring of quads */
typedef struct { Vec3 l0, r0, l1, r1; float a; int used; } Skid;
static Skid g_skid[MAX_SKID]; static int g_skid_n; static Vec3 g_last_l[NKARTS], g_last_r[NKARTS]; static int g_last_skid[NKARTS];
/* dust */
typedef struct { Vec3 p, v; float life, size; } Dust;
static Dust g_dust[MAX_DUST];
/* scenery: cut-out pictures standing beside the road, grouped by picture */
typedef struct { const char *img; Texture *tex; float aspect; Mesh *mesh; } SpriteKind;
static SpriteKind g_kind[MAX_SPRITE_KINDS]; static int g_nkind;
typedef struct { Vec3 p; float h, w, glow; int kind; } Placed;
static Placed g_placed[MAX_SCENERY]; static int g_nplaced;
static int g_mover_kind[MAX_MOVERS];
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
    Mesh **ms[] = {&m_finale, &m_boostpads, &m_bouncepads, &m_road, &m_shoulder, &m_wall, &m_startline, &m_ground, &m_sky, &m_skid, &m_dust, &m_hud, &m_kart_body, &m_kart_rest, &m_cube, &m_shield, &m_items, &m_shadow};
    for (size_t i = 0; i < sizeof ms / sizeof ms[0]; i++) { gfx_mesh_free(*ms[i]); *ms[i] = NULL; }
    for (int i = 1; i < NSTYLES; i++) { gfx_mesh_free(m_body_style[i]); gfx_mesh_free(m_rest_style[i]); m_body_style[i] = m_rest_style[i] = NULL; }
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
    /* ground: a big grass plane under everything */
    batch_clear(&g_b);
    { float lowest = 1e9f; for (int i = 0; i < t->n; i++) { float y = track_surface_height(t, i, track_edge(t, i)); if (y < lowest) lowest = y; y = track_surface_height(t, i, -track_edge(t, i)); if (y < lowest) lowest = y; }
      batch_quad(&g_b, v3(0, lowest - 0.6f, 0), v3(600, 0, 0), v3(0, 0, -600), v3(0, 1, 0), 0, 0, 120, 120, 1, 1, 1, 1); }
    m_ground = gfx_mesh_static(g_b.v, g_b.n);
    /* sky: a big cylinder-ish ring far away (drawn without depth) */
    batch_clear(&g_b);
    for (int k = 0; k < 24; k++) {
        float a0 = k * 6.2831853f / 24, a1 = (k + 1) * 6.2831853f / 24, R = 900.0f;
        Vec3 p0 = v3(cosf(a0) * R, -60, sinf(a0) * R), p1 = v3(cosf(a1) * R, -60, sinf(a1) * R), p2 = v3(cosf(a1) * R, 420, sinf(a1) * R), p3 = v3(cosf(a0) * R, 420, sinf(a0) * R);
        batch_quad4(&g_b, p1, p0, p3, p2, v3(0, 1, 0), k / 24.0f * 3, 1, (k + 1) / 24.0f * 3, 0, 1, 1, 1, 1);
    }
    m_sky = gfx_mesh_static(g_b.v, g_b.n);
    m_skid = gfx_mesh_dynamic(MAX_SKID * 6);
    m_dust = gfx_mesh_dynamic(MAX_DUST * 6);
    m_hud = gfx_mesh_dynamic(8192);
    /* placeholder kart: boxes until the character art arrives. The body is white and tinted with the character's color. */
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.42f, 0), v3(0.75f, 0.17f, 1.05f), 1, 1, 1);                    /* body (tinted) */
    batch_box(&g_b, v3(0, 0.48f, 0.95f), v3(0.6f, 0.12f, 0.2f), 1, 1, 1);                  /* front bumper */
    batch_box(&g_b, v3(0, 1.0f, -0.25f), v3(0.22f, 0.3f, 0.2f), 0.9f, 0.9f, 0.9f);          /* driver robe (tinted) */
    m_kart_body = gfx_mesh_static(g_b.v, g_b.n); m_body_style[0] = m_kart_body;
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.62f, -0.15f), v3(0.55f, 0.08f, 0.55f), 0.2f, 0.2f, 0.25f);     /* seat */
    batch_box(&g_b, v3(0, 1.48f, -0.25f), v3(0.19f, 0.19f, 0.19f), 0.98f, 0.82f, 0.65f);   /* head */
    for (int wx = -1; wx <= 1; wx += 2) for (int wz = -1; wz <= 1; wz += 2)
        batch_box(&g_b, v3(wx * 0.85f, 0.32f, wz * 0.78f), v3(0.14f, 0.32f, 0.32f), 0.12f, 0.12f, 0.12f);
    m_kart_rest = gfx_mesh_static(g_b.v, g_b.n); m_rest_style[0] = m_kart_rest;
    /* CHARIOT: a low open car on two big back wheels, a rail in front */
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.45f, -0.2f), v3(0.6f, 0.12f, 0.8f), 1, 1, 1);
    batch_box(&g_b, v3(0, 0.85f, 0.55f), v3(0.6f, 0.3f, 0.06f), 1, 1, 1);                   /* the front wall */
    batch_box(&g_b, v3(0, 1.0f, -0.25f), v3(0.22f, 0.3f, 0.2f), 0.9f, 0.9f, 0.9f);
    m_body_style[1] = gfx_mesh_static(g_b.v, g_b.n);
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 1.48f, -0.25f), v3(0.19f, 0.19f, 0.19f), 0.98f, 0.82f, 0.65f);
    batch_box(&g_b, v3(0, 0.5f, 1.1f), v3(0.08f, 0.08f, 0.5f), 0.75f, 0.6f, 0.3f);           /* the shaft, gold */
    for (int wx = -1; wx <= 1; wx += 2) batch_box(&g_b, v3(wx * 0.8f, 0.5f, -0.5f), v3(0.1f, 0.5f, 0.5f), 0.85f, 0.65f, 0.2f);   /* big gold wheels */
    for (int wx = -1; wx <= 1; wx += 2) batch_box(&g_b, v3(wx * 0.7f, 0.25f, 0.8f), v3(0.1f, 0.25f, 0.25f), 0.12f, 0.12f, 0.12f);
    m_rest_style[1] = gfx_mesh_static(g_b.v, g_b.n);
    /* BOAT: a hull with a pointed bow and a little mast */
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.45f, -0.1f), v3(0.7f, 0.2f, 0.9f), 1, 1, 1);
    batch_box(&g_b, v3(0, 0.5f, 1.0f), v3(0.4f, 0.18f, 0.35f), 1, 1, 1);
    batch_box(&g_b, v3(0, 0.55f, 1.35f), v3(0.15f, 0.15f, 0.2f), 1, 1, 1);                   /* bow */
    batch_box(&g_b, v3(0, 1.0f, -0.25f), v3(0.22f, 0.3f, 0.2f), 0.9f, 0.9f, 0.9f);
    m_body_style[2] = gfx_mesh_static(g_b.v, g_b.n);
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 1.48f, -0.25f), v3(0.19f, 0.19f, 0.19f), 0.98f, 0.82f, 0.65f);
    batch_box(&g_b, v3(0, 1.3f, 0.5f), v3(0.05f, 0.7f, 0.05f), 0.5f, 0.35f, 0.2f);           /* mast */
    batch_box(&g_b, v3(0.3f, 1.4f, 0.5f), v3(0.3f, 0.45f, 0.02f), 0.95f, 0.93f, 0.85f);       /* sail */
    for (int wx = -1; wx <= 1; wx += 2) for (int wz = -1; wz <= 1; wz += 2)
        batch_box(&g_b, v3(wx * 0.8f, 0.3f, wz * 0.7f), v3(0.12f, 0.3f, 0.3f), 0.12f, 0.12f, 0.12f);
    m_rest_style[2] = gfx_mesh_static(g_b.v, g_b.n);
    /* ARK: a big wooden box with a roof */
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.6f, 0), v3(0.85f, 0.35f, 1.1f), 1, 1, 1);
    batch_box(&g_b, v3(0, 1.0f, -0.25f), v3(0.22f, 0.3f, 0.2f), 0.9f, 0.9f, 0.9f);
    m_body_style[3] = gfx_mesh_static(g_b.v, g_b.n);
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 1.48f, -0.25f), v3(0.19f, 0.19f, 0.19f), 0.98f, 0.82f, 0.65f);
    batch_box(&g_b, v3(0, 1.1f, 0.5f), v3(0.9f, 0.12f, 0.6f), 0.45f, 0.3f, 0.18f);           /* the roof over the front */
    batch_box(&g_b, v3(0, 1.4f, 0.5f), v3(0.5f, 0.2f, 0.4f), 0.5f, 0.35f, 0.2f);
    for (int wx = -1; wx <= 1; wx += 2) for (int wz = -1; wz <= 1; wz += 2)
        batch_box(&g_b, v3(wx * 0.95f, 0.32f, wz * 0.85f), v3(0.14f, 0.32f, 0.32f), 0.12f, 0.12f, 0.12f);
    m_rest_style[3] = gfx_mesh_static(g_b.v, g_b.n);
    /* item box: a cube */
    batch_clear(&g_b); batch_box(&g_b, v3(0, 0, 0), v3(0.65f, 0.65f, 0.65f), 1, 1, 1); m_cube = gfx_mesh_static(g_b.v, g_b.n);
    /* shield: a bigger cube drawn transparent around the kart */
    batch_clear(&g_b); batch_box(&g_b, v3(0, 0.8f, 0), v3(1.4f, 1.2f, 1.6f), 1, 1, 1); m_shield = gfx_mesh_static(g_b.v, g_b.n);
    m_items = gfx_mesh_dynamic(MAX_PROJ * 6 + 6);
    m_shadow = gfx_mesh_dynamic((NKARTS + MAX_MOVERS) * 6);
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
static void px_stone(int x, int y, uint32_t *o) {        /* a rolling round stone */
    *o = 0; float dx = (x - 32) / 30.0f, dy = (y - 64) / 30.0f;
    if (dx * dx + dy * dy < 1.0f) *o = stone(x, y, 150 - (int)(dy * 40), 140 - (int)(dy * 40), 130 - (int)(dy * 40));
}
static Texture *placeholder_sprite(const char *id, float *aspect) {
    *aspect = (float)PW / (float)PH;
    static const struct { const char *id; void (*fn)(int, int, uint32_t *); } table[] = {
        {"#hut", px_hut}, {"#net", px_net}, {"#rock", px_rock}, {"#boat", px_boat}, {"#cypress", px_cypress}, {"#pillar", px_pillar},
        {"#tower", px_tower}, {"#wallseg", px_wallseg}, {"#arch", px_arch}, {"#stall", px_stall}, {"#cart", px_cart}, {"#lily", px_lily},
        {"#tomb", px_tomb}, {"#crosses", px_crosses}, {"#stone", px_stone}, {"#cloud", px_cloudpuff}, {"#gpillar", px_gpillar} };
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
      k->tex = asset_texture(name, 0, &w, &h); if (k->tex) k->aspect = (float)w / (float)h; }
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
    tx_sky = theme_tex(d->tex_sky, 256, 256, d->night ? px_sky_night : px_sky);
    gfx_texture_free(tx_finale); tx_finale = d->finale_img ? asset_texture(d->finale_img, 0, NULL, NULL) : NULL;
    tx_star = NULL;
    if (d->star_size > 0) { tx_star = asset_texture("beth_star", 0, NULL, NULL); if (!tx_star) tx_star = make_tex(64, 64, 0, 0, px_star); }
    g_nkind = 0; g_nplaced = 0;
    for (int i = 0; i < g_track.nmover; i++) g_mover_kind[i] = sprite_kind(g_track.mover[i].img);
    for (int i = 0; i < d->nscenery && g_nplaced < MAX_SCENERY; i++) {
        const Scenery *sc = &d->scenery[i]; int idx = sc->idx % g_track.n;
        Placed *p = &g_placed[g_nplaced++];
        p->kind = sprite_kind(sc->img);
        float edge = track_edge(&g_track, idx);
        p->p = v3(g_track.s[idx].x + g_track.s[idx].nx * sc->side, 0, g_track.s[idx].z + g_track.s[idx].nz * sc->side);
        p->p.y = track_surface_height(&g_track, idx, (sc->side < 0 ? -1.0f : 1.0f) * edge) - 0.2f;   /* stands on the ground just outside the wall */
        p->h = sc->height; p->w = sc->height * g_kind[p->kind].aspect; p->glow = sc->glow;
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
    g_attract = attract;
    int nk = (g_mode == MODE_TT && !attract) ? 1 : NKARTS;
    race_init(&g_race, &g_track, g_player_char, nk == 1 ? 0 : 5, g_difficulty, nk);
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
    g_status = status;
    save_load(); save_check_unlocks(); save_store();
#ifdef KK_TOUR
    if (KK_TOUR_ROAD >= 4) g_save.unlocks = 0xFFFF;      /* the CI tour of the bonus road */
#endif
    for (int d = 0; d < 8 && d < NUM_TRACK_DEFS; d++) g_best_time[d] = g_save.best_ms[d] / 1000.0f;
    race_set_unlocked(unlocked_mask()); race_set_styles(0, unlocked_styles());
    build_previews();
    if (status == GFX_OK) {
        tx_blob = make_tex(64, 64, 0, 0, px_blob);
        tx_flag = make_tex(32, 32, 1, 1, px_flag);
        { static uint32_t fpx[FONT_TEX_W * FONT_TEX_H]; font_build_texture(fpx); tx_font = gfx_texture_rgba(fpx, FONT_TEX_W, FONT_TEX_H, 0, 1); }
        static const char *pn[NCHARS] = {"portrait_jesus", "portrait_moses", "portrait_david", "portrait_esther", "portrait_peter", "portrait_paul", "portrait_mary", "portrait_daniel", "portrait_noah", "portrait_elijah"};
        for (int i = 0; i < NCHARS; i++) tx_portrait[i] = asset_texture(pn[i], 0, NULL, NULL);
        static const char *in[ITEM_COUNT] = {"", "icon_manna", "icon_staff", "icon_stone", "icon_oil", "shofar", "icon_fire", "icon_locusts", "icon_dove"};
        for (int i = 1; i < ITEM_COUNT; i++) tx_item[i] = asset_texture(in[i], 0, NULL, NULL);
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
    menu_item("GRAND PRIX", 250, g_menu_sel == 0, 34);
    menu_item("SINGLE RACE", 310, g_menu_sel == 1, 34);
    menu_item("TIME TRIAL", 370, g_menu_sel == 2, 34);
    snprintf(buf, sizeof buf, "DIFFICULTY: %s", dn[g_difficulty]); menu_item(buf, 430, g_menu_sel == 3, 30);
    text_c("A: CHOOSE   B: BACK   LEFT/RIGHT: CHANGE", 520, 16, 0.8f, 0.85f, 1, 1);
}
static void char_card_pos(int i, float *x, float *y) { *x = 40 + (i % 5) * 245, *y = 95 + (i / 5) * 290; }
static void hud_char(void) {
    text_c("CHOOSE YOUR CHARACTER", 30, 40, 1, 0.85f, 0.3f, 1);
    for (int i = 0; i < NCHARS; i++) {
        const Character *c = &CHARACTERS[i]; int sel = i == g_player_char, open = char_unlocked(i);
        float x, y, w = 225, h = 262; char_card_pos(i, &x, &y);
        if (sel) batch_rect(&g_b, x - 8, y - 8, w + 16, h + 16, 1, 0.85f, 0.3f, 1);
        batch_rect(&g_b, x, y, w, h, 0.08f, 0.14f, 0.36f, 0.95f);
        if (!open) {
            batch_rect(&g_b, x + 10, y + 10, 120, 120, 0.2f, 0.22f, 0.3f, 1);
            batch_text(&g_b, "?", x + 70 - text_width("?", 80) / 2, y + 30, 80, 0.5f, 0.55f, 0.7f, 1);
            batch_text(&g_b, "LOCKED", x + 140, y + 20, 20, 0.8f, 0.8f, 0.9f, 1);
            /* the hint, wrapped onto a few short lines */
            const char *hint = unlock_hint(char_lock_bit(i)); char line[24]; int li = 0, row = 0;
            for (const char *q = hint; ; q++) { if (*q == ' ' || *q == 0) { if (li > 0 && (q - hint) > 0) { line[li] = 0; } 
                    if (*q == 0 || li >= 13) { line[li] = 0; batch_text(&g_b, line, x + 10, y + 150 + row * 20, 14, 1, 0.9f, 0.5f, 1); row++; li = 0; if (*q == 0) break; continue; } }
                if (li < 22) line[li++] = *q; }
            continue;
        }
        /* portrait or a colored card with the initial */
        batch_rect(&g_b, x + 10, y + 10, 120, 120, c->r, c->g, c->b, 1);
        if (!tx_portrait[i]) { char ini[2] = {c->short_name[0], 0}; batch_text(&g_b, ini, x + 70 - text_width(ini, 80) / 2, y + 30, 80, 0.1f, 0.1f, 0.15f, 0.8f); }
        batch_text(&g_b, c->short_name, x + 140, y + 20, 20, 1, 1, 1, 1);
        if (c->bonus) batch_text(&g_b, "BONUS", x + 140, y + 48, 14, 1, 0.85f, 0.3f, 1);
        stat_bar(x + 10, y + 150, "SPEED", c->st.top_speed);
        stat_bar(x + 10, y + 175, "ACCEL", c->st.accel);
        stat_bar(x + 10, y + 200, "HANDLE", c->st.handling);
        stat_bar(x + 10, y + 225, "WEIGHT", c->st.weight);
    }
    text_c("A: CHOOSE   B: BACK", 690, 16, 0.8f, 0.85f, 1, 1);
}
static void draw_portraits(void) {   /* the real portraits are separate textures, drawn after the HUD batch */
    Mat4 id = m4_identity();
    for (int i = 0; i < NCHARS; i++) { if (!tx_portrait[i] || !char_unlocked(i)) continue;
        float x, y; char_card_pos(i, &x, &y);
        batch_clear(&g_b); batch_rect_uv(&g_b, x + 10, y + 10, 120, 120, 0, 0, 1, 1, 1, 1, 1, 1);
        static Mesh *pm = NULL; if (!pm) pm = gfx_mesh_dynamic(6);
        gfx_mesh_update(pm, g_b.v, g_b.n); gfx_bind(tx_portrait[i]); gfx_draw(pm, &id, 1, 1, 1, 1); }
}
static void hud_kart(void) {
    text_c("CHOOSE YOUR KART", 30, 40, 1, 0.85f, 0.3f, 1);
    static const char *desc[NSTYLES][2] = {{"THE EVERYDAY KART", "BALANCED"}, {"BUILT FOR SPEED", "WIDER TURNS"}, {"LIGHT AND NIMBLE", "QUICK TURNS"}, {"HEAVY AND STURDY", "FAST OFF THE LINE"}};
    for (int i = 0; i < NSTYLES; i++) {
        int sel = i == g_player_style, open = style_unlocked(i); float x = 100 + i * 290, y = 150, w = 260, h = 360;
        if (sel) batch_rect(&g_b, x - 8, y - 8, w + 16, h + 16, 1, 0.85f, 0.3f, 1);
        batch_rect(&g_b, x, y, w, h, 0.08f, 0.14f, 0.36f, 0.95f);
        if (!open) {
            batch_rect(&g_b, x + 20, y + 20, 220, 160, 0.2f, 0.22f, 0.3f, 1);
            batch_text(&g_b, "?", x + 130 - text_width("?", 90) / 2, y + 55, 90, 0.5f, 0.55f, 0.7f, 1);
            batch_text(&g_b, "LOCKED", x + 20, y + 200, 20, 0.8f, 0.8f, 0.9f, 1);
            const char *hint = unlock_hint(style_lock_bit(i)); char line[24]; int li = 0, row = 0;
            for (const char *q = hint; ; q++) { if ((*q == ' ' || *q == 0) && li >= 12) { line[li] = 0; batch_text(&g_b, line, x + 20, y + 235 + row * 20, 14, 1, 0.9f, 0.5f, 1); row++; li = 0; if (*q == 0) break; continue; }
                if (*q == 0) { line[li] = 0; if (li) batch_text(&g_b, line, x + 20, y + 235 + row * 20, 14, 1, 0.9f, 0.5f, 1); break; }
                if (li < 22) line[li++] = *q; }
            continue;
        }
        batch_rect(&g_b, x + 20, y + 20, 220, 160, 0.05f, 0.08f, 0.2f, 1);       /* the kart itself is drawn in 3D over this window */
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
        float x = 100 + i * 290 + 130, y = 150 + 110;
        /* a tiny perspective view into the window: the kart at the origin, camera in front */
        Mat4 view = m4_look_at(v3(3.2f * sinf(g_time * 0.8f), 1.6f, 3.2f * cosf(g_time * 0.8f)), v3(0, 0.7f, 0), v3(0, 1, 0));
        Mat4 proj = m4_perspective(40.0f * 3.14159265f / 180.0f, 220.0f / 160.0f, 0.3f, 50.0f);
        Mat4 vp = m4_mul(proj, view);
        /* squeeze it into the window: scale and shift in clip space */
        Mat4 win = m4_identity(); win.m[0] = 220.0f / 1280.0f; win.m[5] = 160.0f / 720.0f; win.m[12] = (x / 640.0f) - 1.0f; win.m[13] = 1.0f - (y / 360.0f);
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
        draw_preview(&g_preview[d], x + 112, y + 112, 175, 0.95f, 0.95f, 0.95f);
        /* the name, split onto two lines when long */
        const char *nm = td->name; size_t len = strlen(nm);
        if (text_width(nm, 18) > w - 24) { size_t cut = len / 2; while (cut > 0 && nm[cut] != ' ') cut--; char l1[32], l2[32]; snprintf(l1, sizeof l1, "%.*s", (int)cut, nm); snprintf(l2, sizeof l2, "%s", nm + cut + 1);
            batch_text(&g_b, l1, x + 12, y + 225, 18, 1, 1, 1, 1); batch_text(&g_b, l2, x + 12, y + 248, 18, 1, 1, 1, 1); }
        else batch_text(&g_b, nm, x + 12, y + 225, 18, 1, 1, 1, 1);
        snprintf(buf, sizeof buf, td->laps == 1 ? "ONE RUN TO THE TOP" : "%d LAPS", td->laps); batch_text(&g_b, buf, x + 12, y + 280, 14, 0.8f, 0.9f, 1, 1);
        if (g_best_time[d] > 0) { char tb[32]; fmt_time(tb, sizeof tb, g_best_time[d]); snprintf(buf, sizeof buf, "RECORD %s", tb); batch_text(&g_b, buf, x + 12, y + 305, 14, 1, 0.9f, 0.5f, 1); }
        if (save_par_time(d) > 0) { char tb[32]; fmt_time(tb, sizeof tb, save_par_time(d)); snprintf(buf, sizeof buf, "TARGET %s", tb); batch_text(&g_b, buf, x + 12, y + 328, 14, 0.7f, 0.95f, 0.8f, 1); }
        if (td->bonus) batch_text(&g_b, "BONUS ROAD", x + 12, y + 360, 14, 1, 0.85f, 0.3f, 1);
    }
    text_c("A: RACE   B: BACK", 690, 16, 0.8f, 0.85f, 1, 1);
}
static void hud_pause(void) {
    batch_rect(&g_b, 0, 0, 1280, 720, 0, 0, 0, 0.45f);
    panel(390, 200, 500, 300);
    text_c("PAUSED", 220, 40, 1, 0.85f, 0.3f, 1);
    menu_item("CONTINUE", 300, g_pause_sel == 0, 32);
    menu_item("RESTART", 355, g_pause_sel == 1, 32);
    menu_item("QUIT TO MENU", 410, g_pause_sel == 2, 32);
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
    int ord[NKARTS]; for (int i = 0; i < NKARTS; i++) ord[i] = g_race.character[i];
    for (int a2 = 1; a2 < NKARTS; a2++) { int v = ord[a2], j = a2 - 1; while (j >= 0 && g_gp_points[ord[j]] < g_gp_points[v]) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = v; }
    for (int p = 0; p < NKARTS; p++) { int c = ord[p]; float y = 160 + p * 50; int me = c == g_player_char;
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
        {8, 'A'}, {11, 'D'}, {14, 'A'}, {17, 'R'}, {22, 'A'}, {26, 'A'},
        {27, KK_TOUR_ROAD >= 1 ? 'R' : 0}, {29, KK_TOUR_ROAD >= 2 ? 'R' : 0}, {31, KK_TOUR_ROAD >= 3 ? 'R' : 0}, {33, KK_TOUR_ROAD >= 4 ? 'R' : 0}, {36, 'A'},
        {70, 'P'}, {84, 'P'} };
    memset(p, 0, sizeof *p);
    for (unsigned i = 0; i < sizeof script / sizeof script[0]; i++) {
        if (t >= script[i].t && t < script[i].t + 0.15f) {
            switch (script[i].key) { case 'A': p->a = 1; break; case 'B': p->b = 1; break; case 'P': p->plus = 1; break;
                                     case 'D': p->sy = -1; break; case 'R': p->sx = 1; break; case 'L': p->sx = -1; break; }
        }
    }
}
#endif
static void begin_player_race(void) { set_track(g_mode == MODE_GP ? g_gp_order[g_gp_round] : g_track_idx); start_race(0); g_screen = SCR_RACE; }

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
    case SCR_TITLE: if (pa) { g_screen = SCR_MAIN; g_screen_t = 0; g_menu_sel = 0; } break;
    case SCR_MAIN:
        if (my) { g_menu_sel = (g_menu_sel - my + 4) % 4; }
        if (g_menu_sel == 3 && mx) g_difficulty = (g_difficulty + mx + 3) % 3;
        if (pa) { if (g_menu_sel == 3) g_difficulty = (g_difficulty + 1) % 3; else { g_mode = g_menu_sel == 0 ? MODE_GP : (g_menu_sel == 1 ? MODE_SINGLE : MODE_TT); g_screen = SCR_CHAR; g_screen_t = 0; } }
        if (pb) { g_screen = SCR_TITLE; g_screen_t = 0; }
        break;
    case SCR_CHAR:
        if (mx) g_player_char = (g_player_char / 5) * 5 + ((g_player_char % 5) + mx + 5) % 5;
        if (my) g_player_char = (g_player_char + 5) % NCHARS;
        if (pa && char_unlocked(g_player_char)) { g_screen = SCR_KART; g_screen_t = 0; }
        if (pb) { g_screen = SCR_MAIN; g_screen_t = 0; }
        break;
    case SCR_KART:
        if (mx) g_player_style = (g_player_style + mx + NSTYLES) % NSTYLES;
        if (pa && style_unlocked(g_player_style)) { race_set_styles(g_player_style, unlocked_styles());
            if (g_mode == MODE_GP) { memset(g_gp_points, 0, sizeof g_gp_points); g_gp_round = 0; for (int i = 0; i < NUM_GP_TRACKS; i++) g_gp_order[i] = i; begin_player_race(); g_screen_t = 0; } else { g_screen = SCR_TRACK; g_screen_t = 0; } }
        if (pb) { g_screen = SCR_CHAR; g_screen_t = 0; }
        break;
    case SCR_TRACK:
        if (mx) { int d = (g_track_idx + mx + NUM_PLAY_TRACKS) % NUM_PLAY_TRACKS; g_track_idx = d; if (track_unlocked(d)) { set_track(d); start_race(1); } }
        if (pa && track_unlocked(g_track_idx)) { begin_player_race(); g_screen_t = 0; }
        if (pb) { g_screen = SCR_KART; g_screen_t = 0; }
        break;
    case SCR_RACE:
        if (pplus && r->state != RACE_FINISHED && g_screen_t > 0.5f) { g_screen = SCR_PAUSE; g_pause_sel = 0; g_screen_t = 0; }
        if (r->state == RACE_FINISHED) {
            g_results_t += dt;
            if (g_results_t > 1.5f) {
                if (g_mode == MODE_GP) { race_rank(r); for (int p = 0; p < r->nk; p++) { Kart *q = &r->k[r->order[p]]; int pl = q->place ? q->place : p + 1; g_gp_points[r->character[r->order[p]]] += GP_POINTS[pl - 1]; } }
                if (g_mode == MODE_TT) { Kart *k = &r->k[r->player]; if (k->lap > g_track.laps && (g_best_time[g_track_idx] <= 0 || k->total_time < g_best_time[g_track_idx])) { g_best_time[g_track_idx] = k->total_time; if (g_track_idx < SAVE_MAX_ROADS) { g_save.best_ms[g_track_idx] = (int)(k->total_time * 1000); g_save.dirty = 1; } } }
                else if (!g_demo) { Kart *k = &r->k[r->player]; g_save.races++; if (k->place == 1) { g_save.wins++; g_save.win_chars |= 1u << g_player_char; } g_save.dirty = 1; }
                g_fresh_unlocks = 0;
                if (g_mode == MODE_GP && g_gp_round + 1 >= NUM_GP_TRACKS && !g_demo) {   /* the championship is decided */
                    int best = 0; for (int c = 1; c < NCHARS; c++) if (g_gp_points[c] > g_gp_points[best]) best = c;
                    if (best == g_player_char) { g_save.gp_wins[g_difficulty]++; g_save.dirty = 1; } }
                g_fresh_unlocks = save_check_unlocks(); save_store(); race_set_unlocked(unlocked_mask()); race_set_styles(g_player_style, unlocked_styles());
                g_screen = SCR_RESULTS; g_screen_t = 0;
            }
        }
        break;
    case SCR_PAUSE:
        sim_running = 0;
        if (my) g_pause_sel = (g_pause_sel - my + 3) % 3;
        if (pa || pplus) {
            if (g_pause_sel == 0 || pplus) { g_screen = SCR_RACE; g_screen_t = 0; }
            else if (g_pause_sel == 1) { start_race(0); g_screen = SCR_RACE; g_screen_t = 0; }
            else { start_race(1); g_screen = SCR_MAIN; g_screen_t = 0; }
        }
        if (pb) { g_screen = SCR_RACE; g_screen_t = 0; }
        break;
    case SCR_RESULTS:
        if (pa) { if (g_mode == MODE_GP) { g_gp_round++; g_screen = SCR_GP; g_screen_t = 0; } else { start_race(0); g_screen = SCR_RACE; g_screen_t = 0; } }
        if (pb && g_mode != MODE_GP) { start_race(1); g_screen = SCR_MAIN; g_screen_t = 0; }
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

    /* fixed-step simulation */
    static float acc = 0; if (sim_running) acc += dt; else acc = 0;
    KartInput in; memset(&in, 0, sizeof in);
    if (g_screen == SCR_RACE) { in.steer = -pad->sx;   /* the sim turns toward +x for positive steer, and +x is screen-left from behind the kart */ in.gas = pad->a; in.brake = pad->b; in.drift = pad->zr; in.item = pad->zl || pad->x || pad->y; }
    int steps = 0;
    while (acc >= SIM_DT && steps < 8) {
        race_step(r, &in, g_demo || g_attract, SIM_DT); acc -= SIM_DT; steps++;
        for (int e = 0; e < r->nev && !g_attract; e++) {
            RaceEvent *ev = &r->ev[e]; int mine = ev->kart == r->player;
            switch (ev->type) {
                case EV_GO: snprintf(g_msg, sizeof g_msg, "GO!"); g_msg_t = 1.0f; break;
                case EV_FINAL_LAP: if (mine) { snprintf(g_msg, sizeof g_msg, "FINAL LAP!"); g_msg_t = 2.0f; } break;
                case EV_FINISH: if (mine) { snprintf(g_msg, sizeof g_msg, g_def->grid_idx ? "WELCOME HOME!" : "FINISH!"); g_msg_t = 3.0f; } break;
                case EV_HIT: if (mine) { snprintf(g_msg, sizeof g_msg, "OUCH!"); g_msg_t = 0.8f; } add_dust(v3(r->k[ev->kart].x, r->k[ev->kart].y, r->k[ev->kart].z), 2.0f, 3.0f); break;
                case EV_BLOCK: if (mine) { snprintf(g_msg, sizeof g_msg, "BLOCKED!"); g_msg_t = 0.8f; } break;
                case EV_PICKUP: if (mine) { snprintf(g_msg, sizeof g_msg, "%s", item_name(r->k[ev->kart].item)); g_msg_t = 1.0f; } break;
                case EV_PAD_BOOST: if (mine) { snprintf(g_msg, sizeof g_msg, "BOOST!"); g_msg_t = 0.7f; } add_dust(v3(r->k[ev->kart].x, r->k[ev->kart].y, r->k[ev->kart].z), 1.0f, 2.0f); break;
                case EV_PAD_BOUNCE: if (mine) { snprintf(g_msg, sizeof g_msg, "BOING!"); g_msg_t = 0.7f; } break;
                case EV_SLOWED: if (mine) { snprintf(g_msg, sizeof g_msg, "LOCUSTS!"); g_msg_t = 1.5f; } break;
                case EV_USE_TRUMPET: for (int q = 0; q < 10; q++) add_dust(v3(r->k[ev->kart].x, r->k[ev->kart].y + 1, r->k[ev->kart].z), 6.0f, 2.5f); break;
                default: break;
            }
        }
        for (int i = 0; i < r->nk; i++) {
            Kart *q = &r->k[i];
            float hx = sinf(q->yaw + q->slide), hz = cosf(q->yaw + q->slide), rx = cosf(q->yaw + q->slide), rz = -sinf(q->yaw + q->slide);
            if (q->skid && !q->air) add_skid(i, v3(q->x - rx * 0.85f - hx * 0.8f, q->y + 0.04f, q->z - rz * 0.85f - hz * 0.8f), v3(q->x + rx * 0.85f - hx * 0.8f, q->y + 0.04f, q->z + rz * 0.85f - hz * 0.8f));
            else g_last_skid[i] = 0;
            if (i == r->player && q->mini_turbo_tier && !g_attract) { snprintf(g_msg, sizeof g_msg, q->mini_turbo_tier == 2 ? "SUPER TURBO!" : "MINI TURBO!"); g_msg_t = 1.2f; }
        }
    }
    if (steps == 8) acc = 0;
    Kart *k = &r->k[r->player];

    /* effects */
    for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i];
        float near = (q->x - k->x) * (q->x - k->x) + (q->z - k->z) * (q->z - k->z) < 60.0f * 60.0f;
        if (near && !q->air && (!q->hit.on_road || q->drifting) && q->speed > 5.0f && frand01() < 0.6f) add_dust(v3(q->x, q->y, q->z), 1.5f, 1.5f);
        if (near && q->boost_t > 0 && frand01() < 0.9f) add_dust(v3(q->x - sinf(q->yaw) * 1.2f, q->y, q->z - cosf(q->yaw) * 1.2f), 0.6f, 1.5f); }
    if (sim_running) {
        for (int i = 0; i < MAX_DUST; i++) if (g_dust[i].life > 0) { Dust *d = &g_dust[i]; d->life -= dt; d->p.x += d->v.x * dt; d->p.y += d->v.y * dt; d->p.z += d->v.z * dt; d->v.y -= 2.0f * dt; d->size += 1.2f * dt; }
        for (int i = 0; i < MAX_SKID; i++) if (g_skid[i].used) { g_skid[i].a -= dt * 0.02f; if (g_skid[i].a <= 0) g_skid[i].used = 0; }
    }

    /* chase camera: sits behind the kart, lags on turns, pulls back with speed */
    float want_yaw = k->yaw + k->slide * 0.3f;
    float dy = want_yaw - cam_yaw; while (dy > 3.14159f) dy -= 6.28318f; while (dy < -3.14159f) dy += 6.28318f;
    cam_yaw += dy * fminf(1.0f, 5.0f * dt);
    float back = 4.6f + k->speed * 0.12f, up = 2.7f + k->speed * 0.02f;   /* close when stopped (the kart behind on the grid is 5 m back), pulls back with speed */
    float tx = k->x - sinf(cam_yaw) * back, tz = k->z - cosf(cam_yaw) * back, ty = k->y + up;
    cam_x += (tx - cam_x) * fminf(1.0f, 12.0f * dt); cam_z += (tz - cam_z) * fminf(1.0f, 12.0f * dt); cam_y += (ty - cam_y) * fminf(1.0f, 6.0f * dt);
    float want_fov = 62.0f + (k->boost_t > 0 ? 10.0f : 0.0f) + k->speed * 0.15f;
    cam_fov += (want_fov - cam_fov) * fminf(1.0f, 4.0f * dt);
    Vec3 eye = v3(cam_x, cam_y, cam_z), at = v3(k->x + sinf(k->yaw) * 3.0f, k->y + 1.2f, k->z + cosf(k->yaw) * 3.0f);
    /* dynamic geometry shared by both screens: trees, skid marks, dust, shadows, items */
    float cy = cam_yaw; Vec3 ax = v3(cosf(cy), 0, -sinf(cy));
    {
        for (int kd = 0; kd < g_nkind; kd++) {
            batch_clear(&g_b);
            for (int i = 0; i < g_nplaced; i++) { Placed *pl = &g_placed[i]; if (pl->kind != kd) continue;
                float dx = pl->p.x - cam_x, dz = pl->p.z - cam_z; if (dx * dx + dz * dz > SPRITE_DRAW_DIST * SPRITE_DRAW_DIST) continue;
                float w = pl->w * 0.5f;
                batch_quad(&g_b, v3(pl->p.x, pl->p.y + pl->h * 0.5f, pl->p.z), v3(ax.x * w, 0, ax.z * w), v3(0, pl->h * 0.5f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 1); }
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
            batch_quad(&g_b, v3(pl->p.x, pl->p.y + pl->h * 0.55f, pl->p.z), v3(ax.x * s, 0, ax.z * s), v3(0, s, 0), v3(0, 1, 0), 0, 0, 1, 1, g_def->glow_r, g_def->glow_g, g_def->glow_b, 0.55f); }
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
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; TrackHit h2; track_locate(&g_track, q->x, q->z, q->hit.idx, &h2);
            float hx = sinf(q->yaw), hz = cosf(q->yaw);
            batch_quad(&g_b, v3(q->x, h2.height + 0.05f, q->z), v3(cosf(q->yaw) * 1.1f, 0, -sinf(q->yaw) * 1.1f), v3(hx * 1.3f, 0, hz * 1.3f), v3(0, 1, 0), 0, 0, 1, 1, 0, 0, 0, 0.45f * (1.0f - clampf((q->y - h2.height) / 6.0f, 0, 0.8f))); }
        for (int m = 0; m < g_track.nmover; m++) { const Mover *mv = &g_track.mover[m]; float mx, my, mz; track_mover_pos(&g_track, mv, r->race_t, &mx, &my, &mz);
            batch_quad(&g_b, v3(mx, my + 0.05f, mz), v3(mv->radius, 0, 0), v3(0, 0, -mv->radius), v3(0, 1, 0), 0, 0, 1, 1, 0, 0, 0, 0.4f); }
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
        gfx_bind(tx_flag); gfx_draw(m_startline, &id, sr, sg, sb, 1);
        gfx_bind(tx_wall); gfx_draw(m_wall, &id, sr, sg, sb, 1);
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
        /* the karts */
        gfx_state(ST_SOLID); gfx_bind(NULL);
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; const Character *c = &CHARACTERS[r->character[i]];
            if (i != r->player) { float cdx = q->x - eye.x, cdz = q->z - eye.z; if (cdx * cdx + cdz * cdz < 3.2f * 3.2f) continue; }   /* a kart sitting on the camera would fill the screen */
            float pitch = -atan2f(q->hit.slope, 1.0f) * (q->air ? 0.3f : 1.0f) + (q->air ? -q->vy * 0.03f : 0.0f);
            float roll = q->hit.on_road ? -g_track.s[q->hit.idx].bank : 0.0f;
            float sq = 1.0f - q->squash * 0.25f;
            Mat4 m = m4_mul(m4_translate(q->x, q->y + q->hop * 0.4f, q->z), m4_mul(m4_rotate_y(q->yaw + q->slide + q->spin_vis), m4_mul(m4_rotate_x(pitch), m4_mul(m4_rotate_z(roll + q->steer_vis * -0.04f), m4_scale(1.0f + (1 - sq) * 0.5f, sq, 1.0f)))));
            int sty = r->style[i] >= 0 && r->style[i] < NSTYLES ? r->style[i] : 0;
            gfx_draw(m_body_style[sty], &m, c->r, c->g, c->b, 1);
            gfx_draw(m_rest_style[sty], &m, 1, 1, 1, 1); }
        /* shields */
        gfx_state(ST_TRANSPARENT); gfx_bind(NULL);
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; if (q->shield <= 0) continue;
            Mat4 m = m4_mul(m4_translate(q->x, q->y, q->z), m4_rotate_y(g_time * 2.0f));
            gfx_draw(m_shield, &m, 0.4f, 0.8f, 1.0f, 0.25f + 0.1f * sinf(g_time * 8)); }
        gfx_bind(tx_blob); gfx_draw(m_items, &id, 1, 1, 1, 1);
        gfx_state(ST_TRANSPARENT | ST_ADD); gfx_draw(m_dust, &id, 1, 1, 1, 1); gfx_draw(m_glow, &id, 1, 1, 1, 1);


        /* ---- HUD and menus */
        gfx_camera_2d(); gfx_state(ST_HUD);
        batch_clear(&g_b);
        int menu_screen = g_screen == SCR_TITLE || g_screen == SCR_MAIN || g_screen == SCR_CHAR || g_screen == SCR_KART || g_screen == SCR_TRACK || g_screen == SCR_GP;
        if (menu_screen) batch_rect(&g_b, 0, 0, 1280, 720, 0.02f, 0.03f, 0.1f, g_screen == SCR_TITLE ? 0.25f : 0.55f);
        switch (g_screen) {
            case SCR_TITLE: hud_title(); break;
            case SCR_MAIN: hud_main(); break;
            case SCR_CHAR: hud_char(); break;
            case SCR_KART: hud_kart(); break;
            case SCR_TRACK: hud_track(); break;
            case SCR_RACE: hud_race(pad); break;
            case SCR_PAUSE: hud_race(pad); hud_pause(); break;
            case SCR_RESULTS: hud_results(); break;
            case SCR_GP: hud_gp(); break;
        }
        { float frac = clampf(g_frame_ms / 33.3f, 0.02f, 1.0f); int good = g_frame_ms <= 16.8f;
          batch_rect(&g_b, 20, 708, 300 * frac, 6, good ? 0.2f : 1.0f, good ? 1.0f : 0.2f, 0.2f, 0.8f);
          batch_rect(&g_b, 20 + 300 * (16.7f / 33.3f), 704, 2, 14, 1, 1, 1, 0.8f); }
        batch_rect(&g_b, 1220, 690, 20, 20, pad->source & 2 ? 1.0f : (pad->source & 1 ? 0.2f : 0.5f), pad->source & 2 ? 0.8f : (pad->source & 1 ? 0.5f : 0.5f), pad->source ? 0.2f : 0.5f, 0.9f);
        gfx_mesh_update(m_hud, g_b.v, g_b.n);
        gfx_bind(tx_font); gfx_draw(m_hud, &id, 1, 1, 1, 1);
        if (g_screen == SCR_CHAR) draw_portraits();
        if (g_screen == SCR_KART) { draw_kart_models(); gfx_camera_2d(); gfx_state(ST_HUD); }
        if ((g_screen == SCR_RACE || g_screen == SCR_PAUSE) && g_mode != MODE_TT) { int it = r->k[r->player].item;
            if (it > 0 && it < ITEM_COUNT && tx_item[it]) { static Mesh *im = NULL; if (!im) im = gfx_mesh_dynamic(6);
                batch_clear(&g_b); batch_rect_uv(&g_b, 48, 146, 134, 48, 0, 0, 1, 1, 1, 1, 1, 1);
                gfx_mesh_update(im, g_b.v, g_b.n); gfx_bind(tx_item[it]); gfx_draw(im, &id, 1, 1, 1, 1); } }
        gfx_view_end(drc);
    }
    gfx_frame_end();
}
int game_debug_screen(void) { return g_screen + 10 * g_mode + 100 * g_menu_sel + 1000 * (int)g_race.state; }
