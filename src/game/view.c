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
#include "wheels_gen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SKID 500
#define MAX_DUST 200
#define MAX_SPRITE_KINDS 16
#define SPRITE_DRAW_DIST 340.0f
#define NUM_GP_ROUNDS_MAX 6
#define NUM_PLAY_TRACKS (NUM_TRACK_DEFS - 3)     /* the last three are the Walls of Jericho, the battle arena and the developers' Test Loop, not roads to pick */
#define JERICHO_TRACK (NUM_TRACK_DEFS - 3)
/* the Grand Prix cups: the Gospel Cup runs the four story roads; Genesis to Glory runs every road in Bible order (earned with the Road to Heaven) */
typedef struct { const char *name, *sub; int roads[NUM_GP_ROUNDS_MAX]; int n; int needs_heaven; } Cup;
static const Cup CUPS[2] = { {"THE GOSPEL CUP", "THE STORY OF JESUS IN FOUR ROADS", {0, 1, 2, 3}, 4, 0}, {"GENESIS TO GLORY", "FROM THE GARDEN TO THE GATES: SIX ROADS", {5, 0, 1, 2, 3, 4}, 6, 1} };
static int g_cup = 0;
#define NUM_GP_TRACKS (CUPS[g_cup].n)
#ifndef KK_TOUR_ROAD
#define KK_TOUR_ROAD 1
#endif

static int g_status;
static Track g_track; static const TrackDef *g_def; static int g_track_idx = 0;
static Battle g_battle;            /* the battle holds the race; in the racing modes only its race is used */
#define g_race (g_battle.race)
#define ARENA_TRACK (NUM_TRACK_DEFS - 2)   /* the Valley of Elah sits just before the Test Loop */
static int g_player_char = 0, g_difficulty = DIFF_NORMAL, g_demo_char = 1;   /* demo_char: who drives the title-screen demo */
/* game flow */
enum { SCR_TITLE, SCR_MAIN, SCR_CHAR, SCR_KART, SCR_TRACK, SCR_RACE, SCR_PAUSE, SCR_RESULTS, SCR_GP, SCR_CREDITS, SCR_OPTIONS, SCR_OPENER, SCR_JOIN, SCR_MCHAR, SCR_CUP, SCR_TROPHY };
/* the opener video: frames content/video/opener_NNN.jpg, decoded one at a time as they come up */
static int g_op_frames = 0, g_op_fps = 12, g_op_cur = -1, g_op_w = 0, g_op_h = 0, g_op_vid = 0; static float g_op_buf = 0; static Texture *tx_op; static Mesh *m_op;   /* g_op_vid: the background decoder is running; g_op_buf: seconds spent filling it before the clock starts */
static int g_opt_sel = 0; static char g_pause_reason[48]; static float g_erase_hold = 0;
static void apply_volumes(void) { music_set_volume(g_save.music_vol / 10.0f); sfx_set_volume(g_save.sfx_vol / 10.0f); }
enum { MODE_GP, MODE_SINGLE, MODE_TT, MODE_BATTLE, MODE_VS, MODE_JERICHO };   /* VS: two to four people on a split screen; JERICHO: seven laps round the walls */
static int g_screen = SCR_TITLE, g_mode = MODE_SINGLE, g_menu_sel = 0, g_pause_sel = 0, g_attract = 1;
static int char_lock_bit(int c) { static const int bits[NCHARS] = {0, 0, 0, 0, 0, 0, 0, UNLOCK_SAMSON, UNLOCK_NOAH, UNLOCK_JOSEPH, UNLOCK_JONAH, UNLOCK_JOHN, UNLOCK_MICHAEL, UNLOCK_GABRIEL}; return c >= 0 && c < NCHARS ? bits[c] : 0; }
static int char_unlocked(int c) { return !CHARACTERS[c].bonus || (g_save.unlocks & char_lock_bit(c)); }
static int style_lock_bit(int sty) { return sty == 1 ? UNLOCK_KART_CHARIOT : (sty == 2 ? UNLOCK_KART_BOAT : (sty == 3 ? UNLOCK_KART_ARK : (sty == 4 ? UNLOCK_KART_SLING : 0))); }
static int style_unlocked(int sty) { return sty == 0 || (g_save.unlocks & style_lock_bit(sty)); }
static unsigned unlocked_styles(void) { unsigned m = 0; for (int i = 0; i < NSTYLES; i++) if (style_unlocked(i)) m |= 1u << i; return m; }
static int g_player_style = 0;
static int track_unlocked(int d) { return TRACK_DEFS[d].bonus != 1 || (g_save.unlocks & UNLOCK_HEAVEN); }   /* only the Road to Heaven has to be earned */
static unsigned unlocked_mask(void) { unsigned m = 0; for (int c = 0; c < NCHARS; c++) if (char_unlocked(c)) m |= 1u << c; return m; }
static int g_gp_round = 0, g_gp_points[NCHARS], g_gp_order[NUM_GP_ROUNDS_MAX];
static float g_best_time[8];                 /* best time trial time per road (loaded from the save file) */
static Texture *tx_portrait[NCHARS]; static float g_portrait_aspect[NCHARS];
static Texture *tx_char[NCHARS]; static float g_char_aspect[NCHARS];      /* full-body pictures: the drivers in the karts */
static Texture *tx_kart[NCHARS]; static float g_kart_aspect[NCHARS]; static Mesh *m_kartpic;   /* each character's own kart (kart-select screen) */
static Texture *tx_kartback[NCHARS]; static float g_kartback_aspect[NCHARS]; static Mesh *m_kart_spr[NKARTS];   /* the kart seen from behind: drives on the track */
/* the same kart from more angles (N64 style): 0 behind, 1 back-left 45, 2 left side, 3 front-left 45, 4 front; the right side is the left mirrored */
static Texture *tx_kartview[NCHARS][5]; static float g_kartview_aspect[NCHARS][5];
static Texture *tx_ride[NCHARS][5]; static float g_ride_aspect[NCHARS][5];
/* spinning wheels: the wheel discs cut from each picture (content/img/wheels_<picture>.png) and where they sit */
static const WheelDef *g_ride_wd[NCHARS][5], *g_view_wd[NCHARS][5]; static Texture *g_ride_wtex[NCHARS][5], *g_view_wtex[NCHARS][5];
static float g_wheel_ang[NKARTS], g_tread_ph[NKARTS]; static Mesh *m_wheel_spr[NKARTS];
static const TreadDef *g_ride_td[NCHARS][5]; static Texture *tx_tread, *tx_trophy, *tx_qbox; static void px_trophy(int x, int y, uint32_t *o); static void px_qbox(int x, int y, uint32_t *o); static Mesh *m_qface, *m_qframe; static Mesh *m_tread_spr[NKARTS];
static const TreadDef *tread_def(const char *picture) { for (int i = 0; i < NUM_TREAD_DEFS; i++) if (!strcmp(TREAD_DEFS[i].picture, picture)) return &TREAD_DEFS[i]; return NULL; }
static const WheelDef *wheel_def(const char *picture) { for (int i = 0; i < NUM_WHEEL_DEFS; i++) if (!strcmp(WHEEL_DEFS[i].picture, picture)) return &WHEEL_DEFS[i]; return NULL; }
static Texture *wheel_tex(const char *picture) { char n[80]; snprintf(n, sizeof n, "wheels_%s", picture); return asset_texture(n, 0, NULL, NULL); }   /* ride_<name>_<b|bl|l|fl|f>: the character sitting in the kart, one picture; used instead of kart + driver when present */
/* split screen: up to four people at once, each with a controller slot, a character, a camera and messages of their own */
typedef struct { float yaw, x, y, z, fov, shake, orbit, finish_t; } View;
static View g_view[MAX_HUMANS];
static int g_nplayers = 1;                      /* people in the race being shown (1 outside multiplayer) */
static int g_njoined = 0;                       /* people who joined on the join screen */
static int g_pslot[MAX_HUMANS] = {0, 1, 2, 3};  /* controller slot of each player (0 the GamePad, 1..4 the Wii controllers) */
static int g_pchar[MAX_HUMANS], g_plock[MAX_HUMANS];   /* multiplayer character select: each player's cursor, and whether they have locked in */
static int g_joined[KK_PADS];                   /* join screen: player number + 1 for each controller slot, 0 = not joined */
static Pad g_prev_pads[KK_PADS];
static char g_pside[MAX_HUMANS][40]; static float g_pside_t[MAX_HUMANS], g_coin_pop[MAX_HUMANS];   /* a small note that pops up beside your own kart ("BLESSED BY MARY") */
static char g_pmsg[MAX_HUMANS][40], g_pmsg2[MAX_HUMANS][64]; static float g_pmsg_t[MAX_HUMANS], g_proulette[MAX_HUMANS], g_prou_tick[MAX_HUMANS];
#define g_msg (g_pmsg[0])
#define g_msg2 (g_pmsg2[0])
#define g_msg_t (g_pmsg_t[0])
#define g_roulette (g_proulette[0])
#define g_rou_tick (g_prou_tick[0])
static View *cv = &g_view[0];                   /* the view being updated or drawn right now */
#define cam_yaw (cv->yaw)
#define cam_x (cv->x)
#define cam_y (cv->y)
#define cam_z (cv->z)
#define cam_fov (cv->fov)
#define g_shake (cv->shake)
#define g_orbit (cv->orbit)
static const float PLAYER_COL[MAX_HUMANS][3] = {{1.0f, 0.85f, 0.3f}, {0.35f, 0.65f, 1.0f}, {1.0f, 0.4f, 0.35f}, {0.4f, 0.9f, 0.45f}};   /* P1 gold, P2 blue, P3 red, P4 green */
static Texture *tx_goliath; static float g_goliath_aspect = 0.67f; static Mesh *m_goliath, *m_hud2;
static Texture *tx_bearer; static float g_bearer_aspect = 0.67f; static Mesh *m_bearer;   /* Goliath's shield-bearer (char_shieldbearer.png, or a drawn one) */
static Mesh *m_driver_box, *m_driver_head, *m_driver_spr[NKARTS];
static unsigned g_fresh_unlocks;             /* unlock bits earned by the race just finished, shown on the results */
static Texture *tx_finale; static Mesh *m_finale;
static Texture *tx_title, *tx_title_bg; static Mesh *m_title, *m_title_bg, *m_logo_small; static float g_title_h;      /* title_art.png: the logo on the title screen, when Sean has made it */
static Texture *tx_card[8]; static Mesh *m_card[8];   /* card_<prefix>.png: the road pictures on the road-select cards */
static Texture *tx_item[ITEM_COUNT]; static int g_item_sq[ITEM_COUNT]; static float g_item_asp[ITEM_COUNT];   /* item pictures for the HUD slot: Sean's files, or drawn ones (square) */
/* the drivers' voices: content/sfx/voice_<name>_<event>.wav for one character, or voice_<event>.wav shared by all; silent when missing */
enum { VO_START, VO_BOOST, VO_TRICK, VO_WIN, VO_OUCH, VO_BLESS, VO_FIRE, VO_COUNT };   /* start: at GO; boost: rocket start / super turbo / boost pad; fire: the Chariot of Fire */
static const char *VO_NAMES[VO_COUNT] = {"start", "boost", "trick", "win", "ouch", "bless", "fire"};
static int g_voice[NCHARS][VO_COUNT];
static Pad g_prev; static float g_nav_t, g_screen_t;
static const int GP_POINTS[8] = {10, 8, 6, 5, 4, 3, 2, 1};
typedef struct { float x[80], z[80]; int n; } Preview;
static Preview g_preview[8];
static float g_idle;                   /* seconds since the last real controller input */
static int g_demo;                     /* autopilot driving the player's kart */
static float g_time;
static float g_frame_ms = 16.7f; static int g_sting_due = 0;
static float g_results_t;

/* textures */
static Texture *tx_road, *tx_dirt, *tx_grass, *tx_wall, *tx_blob, *tx_font, *tx_flag, *tx_sky, *tx_star, *tx_vignette, *tx_bird;
static Mesh *m_scshadow;   /* soft shadows under the scenery */
/* meshes */
static Mesh *m_brook, *m_tunnel, *m_arch, *m_coin, *m_coins, *m_boostpads, *m_bouncepads, *m_road, *m_shoulder, *m_ground, *m_wall, *m_startline, *m_kart_body, *m_kart_rest, *m_body_style[NSTYLES], *m_rest_style[NSTYLES], *m_cube, *m_shield, *m_items, *m_skid, *m_glow, *m_dust, *m_hud, *m_shadow, *m_sky;
static Vertex g_vtmp[8192];
static Batch g_b;

/* camera state lives in the views (cam_x and friends are the view being handled) */
static float g_slowmo = 0;      /* finish-line slow motion (single player): seconds left */
/* Time Trial ghost: the best run is recorded ten times a second and replayed as a see-through kart */
#define GHOST_HZ 10
#define GHOST_MAX (GHOST_HZ * 180)
typedef struct { int16_t x, z, yaw; } GhostSample;        /* position in 1/16 m, yaw in 1/10000 rad */
static GhostSample g_ghost[GHOST_MAX], g_rec[GHOST_MAX]; static int g_ghost_n = 0, g_rec_n = 0, g_ghost_char = 0; static float g_rec_t = 0; static Mesh *m_ghost;
static void ghost_load(int road) {
    g_ghost_n = 0; char name[64], path[300]; snprintf(name, sizeof name, "kingdomkart_ghost3_%d.bin", road);
    FILE *f = fopen(save_dir_path(path, sizeof path, name), "rb"); if (!f) return;
    int hdr[2]; if (fread(hdr, sizeof hdr, 1, f) == 1 && hdr[0] > 0 && hdr[0] <= GHOST_MAX) { g_ghost_char = hdr[1]; g_ghost_n = (int)fread(g_ghost, sizeof(GhostSample), (size_t)hdr[0], f); }
    fclose(f); gfx_log("ghost: %d samples for road %d", g_ghost_n, road);
}
static void ghost_save(int road) {
    char name[64], path[300]; snprintf(name, sizeof name, "kingdomkart_ghost3_%d.bin", road);
    FILE *f = fopen(save_dir_path(path, sizeof path, name), "wb"); if (!f) return;
    int hdr[2] = {g_rec_n, g_player_char}; fwrite(hdr, sizeof hdr, 1, f); fwrite(g_rec, sizeof(GhostSample), (size_t)g_rec_n, f); fclose(f);
    memcpy(g_ghost, g_rec, sizeof(GhostSample) * (size_t)g_rec_n); g_ghost_n = g_rec_n; g_ghost_char = g_player_char; gfx_log("ghost: saved %d samples", g_rec_n);
}
/* skid marks: ring of quads */
typedef struct { Vec3 l0, r0, l1, r1; float a; int used; } Skid;
static Skid g_skid[MAX_SKID]; static int g_skid_n; static Vec3 g_last_l[NKARTS], g_last_r[NKARTS]; static int g_last_skid[NKARTS];
/* dust */
typedef struct { Vec3 p, v; float life, size; } Dust;
static Dust g_dust[MAX_DUST];
#define MAX_SPARK 400
typedef struct { Vec3 p, v; float life, size, r, g, b; } Spark;
static Spark g_spark[MAX_SPARK]; static Mesh *m_spark;
/* scenery: cut-out pictures standing beside the road, grouped by picture */
typedef struct { const char *img; Texture *tex; float aspect; Mesh *mesh; int placeholder, sway; } SpriteKind;   /* sway: 1 tree (slow), 2 cloth (quick) */
static SpriteKind g_kind[MAX_SPRITE_KINDS]; static int g_nkind;
typedef struct { Vec3 p; float h, w, glow; int kind; int purple, falls; int wall; float fall_t; } Placed;   /* wall: 0 = not a wall, else its place in the order the walls of Jericho fall (1..n); fall_t: seconds since it began to fall */
static int g_nwalls;
static Placed g_placed[MAX_SCENERY]; static int g_nplaced;
static int g_mover_kind[MAX_MOVERS];
static int g_was_air[NKARTS];
/* mini-map */
static float map_x0, map_z0, map_scale;

static uint32_t rng = 12345u;
static float frand01(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (rng & 0xFFFFFF) / 16777216.0f; }
static uint32_t rgba(int r, int g, int b, int a) { return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)a; }
static void px_tread(int x, int y, uint32_t *o) {   /* tire tread seen edge-on: dark grooves every 16 rows, soft-edged, repeating; a touch of highlight between */
    /* dark grooves and a lighter rubber block between them, so the roll shows on a black tire as well as a gray one */
    int r = y & 15; (void)x;
    if (r < 3) *o = rgba(0, 0, 0, 210); else if (r < 5) *o = rgba(0, 0, 0, 110);
    else if (r > 7 && r < 13) *o = rgba(185, 185, 185, r == 8 || r == 12 ? 70 : 120); else *o = rgba(0, 0, 0, 0); }

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
static void px_vignette(int x, int y, uint32_t *o) {    /* dark at the corners, clear in the middle; drawn over the whole screen */
    float dx = (x - 127.5f) / 127.5f, dy = (y - 127.5f) / 127.5f; float d = sqrtf(dx * dx + dy * dy) / 1.4142f; float a = d < 0.45f ? 0 : (d - 0.45f) / 0.55f; a = a * a * 0.6f; *o = rgba(0, 0, 0, (int)(a * 255)); }
static void px_bird(int x, int y, uint32_t *o) {        /* a small white bird: two wings in a shallow V */
    float fx = (x - 16) / 16.0f, fy = (y - 16) / 16.0f; float wing = 0.35f - fabsf(fx) * 0.5f;   /* the wing line, higher toward the tips */
    float d = fabsf(fy - wing); float a = d < 0.12f ? 1.0f : (d < 0.22f ? (0.22f - d) / 0.1f : 0); if (fabsf(fx) > 0.95f) a = 0;
    *o = rgba(250, 250, 255, (int)(a * 255)); }
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
    Mesh **ms[] = {&m_brook, &m_tunnel, &m_arch, &m_coin, &m_coins, &m_finale, &m_boostpads, &m_bouncepads, &m_road, &m_shoulder, &m_wall, &m_startline, &m_ground, &m_sky, &m_skid, &m_dust, &m_spark, &m_hud, &m_kart_body, &m_kart_rest, &m_cube, &m_qface, &m_qframe, &m_shield, &m_items, &m_shadow};
    for (size_t i = 0; i < sizeof ms / sizeof ms[0]; i++) { gfx_mesh_free(*ms[i]); *ms[i] = NULL; }
    for (int i = 1; i < NSTYLES; i++) { gfx_mesh_free(m_body_style[i]); gfx_mesh_free(m_rest_style[i]); m_body_style[i] = m_rest_style[i] = NULL; }
    gfx_mesh_free(m_driver_box); gfx_mesh_free(m_driver_head); m_driver_box = m_driver_head = NULL;
    for (int i = 0; i < NKARTS; i++) { gfx_mesh_free(m_driver_spr[i]); m_driver_spr[i] = NULL; }
}
/* a quad whose first two corners (the bottom) are darker than the last two (the top) */
static void batch_quad4_shade(Batch *b, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 n, float u0, float v0, float u1, float v1, float bottom, float top) {
    if (b->n + 6 > b->cap) return;
    Vec3 p[4] = {p0, p1, p2, p3}; float uv[4][2] = {{u0, v1}, {u1, v1}, {u1, v0}, {u0, v0}}; float sh[4] = {bottom, bottom, top, top};
    static const int idx[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; i++) { int k = idx[i]; Vertex q = {p[k].x, p[k].y, p[k].z, n.x, n.y, n.z, uv[k][0], uv[k][1], sh[k], sh[k], sh[k], 1}; b->v[b->n++] = q; }
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
            /* darker where the wall meets the ground (a baked contact shadow) */
            if (side < 0) batch_quad4_shade(&g_b, a, b, bt, at, n, v0, 0, v1, 1, 0.55f, 1.0f);
            else batch_quad4_shade(&g_b, b, a, at, bt, n, v0, 0, v1, 1, 0.55f, 1.0f);
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
        /* chevrons pointing the way the racers drive (tip ahead, wings behind), two in a row */
        for (int row = 0; row < 2; row++) { int i0 = pd->idx + row * 3;
        for (int bar = -1; bar <= 1; bar += 2) {
            Vec3 a = road_point(t, (i0 + 2) % t->n, pd->lateral), b = road_point(t, i0 % t->n, pd->lateral + bar * pd->halfw);
            Vec3 c = road_point(t, (i0 + 1) % t->n, pd->lateral + bar * pd->halfw), d = road_point(t, (i0 + 3) % t->n, pd->lateral);
            a.y += 0.04f; b.y += 0.04f; c.y += 0.04f; d.y += 0.04f;
            if (bar < 0) batch_quad4(&g_b, a, d, c, b, v3(0, 1, 0), 0, 0, 1, 1, 1, 0.85f, 0.2f, 1); else batch_quad4(&g_b, a, b, c, d, v3(0, 1, 0), 0, 0, 1, 1, 1, 0.85f, 0.2f, 1);
        } } }
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
    /* a coin: a thick gold medal - sixteen-sided rim, raised outer ring, and a cross standing out on both faces */
    batch_clear(&g_b);
    { const int N = 16; float rad = 0.6f, hw = 0.1f, rin = 0.47f;
      for (int q = 0; q < N; q++) { float a0 = q * 6.2831853f / N, a1 = (q + 1) * 6.2831853f / N, am = (a0 + a1) * 0.5f;
          Vec3 p0 = v3(cosf(a0) * rad, sinf(a0) * rad, hw), p1 = v3(cosf(a1) * rad, sinf(a1) * rad, hw), p2 = v3(cosf(a1) * rad, sinf(a1) * rad, -hw), p3 = v3(cosf(a0) * rad, sinf(a0) * rad, -hw);
          batch_quad4(&g_b, p0, p1, p2, p3, v3(cosf(am), sinf(am), 0), 0, 0, 1, 1, 0.85f, 0.6f, 0.12f, 1);   /* the milled edge, a deeper gold */
          for (int sd = -1; sd <= 1; sd += 2) { float z = hw * sd, zi = (hw - 0.035f) * sd;   /* raised ring on each face, then the sunken middle */
              Vec3 o0 = v3(cosf(a0) * rad, sinf(a0) * rad, z), o1 = v3(cosf(a1) * rad, sinf(a1) * rad, z), i0 = v3(cosf(a0) * rin, sinf(a0) * rin, z), i1 = v3(cosf(a1) * rin, sinf(a1) * rin, z);
              Vec3 d0 = v3(i0.x, i0.y, zi), d1 = v3(i1.x, i1.y, zi), n = v3(0, 0, (float)sd);
              if (sd > 0) { batch_quad4(&g_b, i0, o0, o1, i1, n, 0, 0, 1, 1, 1.0f, 0.86f, 0.35f, 1); batch_quad4(&g_b, d0, i0, i1, d1, v3(-cosf(am), -sinf(am), 0), 0, 0, 1, 1, 0.8f, 0.58f, 0.15f, 1); }
              else        { batch_quad4(&g_b, i1, o1, o0, i0, n, 0, 0, 1, 1, 1.0f, 0.86f, 0.35f, 1); batch_quad4(&g_b, d1, i1, i0, d0, v3(-cosf(am), -sinf(am), 0), 0, 0, 1, 1, 0.8f, 0.58f, 0.15f, 1); }
              Vertex c0 = {0, 0, zi, 0, 0, (float)sd, 0, 0, 0.95f, 0.74f, 0.24f, 1}, c1 = {d0.x, d0.y, zi, 0, 0, (float)sd, 0, 0, 0.95f, 0.74f, 0.24f, 1}, c2 = {d1.x, d1.y, zi, 0, 0, (float)sd, 0, 0, 0.95f, 0.74f, 0.24f, 1};
              if (g_b.n + 3 <= g_b.cap) { g_b.v[g_b.n++] = c0; if (sd > 0) { g_b.v[g_b.n++] = c1; g_b.v[g_b.n++] = c2; } else { g_b.v[g_b.n++] = c2; g_b.v[g_b.n++] = c1; } } } }
      batch_box(&g_b, v3(0, 0.03f, 0), v3(0.07f, 0.32f, hw - 0.005f), 1.0f, 0.93f, 0.55f);   /* the cross, standing proud of both faces */
      batch_box(&g_b, v3(0, 0.12f, 0), v3(0.22f, 0.07f, hw - 0.005f), 1.0f, 0.93f, 0.55f); }
    m_coin = gfx_mesh_static(g_b.v, g_b.n);
    m_coins = gfx_mesh_dynamic(MAX_COINS * 6);
    /* ground: a big grass plane under everything */
    batch_clear(&g_b);
    { float lowest = 1e9f; for (int i = 0; i < t->n; i++) { float y = track_surface_height(t, i, track_edge(t, i)); if (y < lowest) lowest = y; y = track_surface_height(t, i, -track_edge(t, i)); if (y < lowest) lowest = y; }
      batch_quad(&g_b, v3(0, lowest - 0.6f, 0), v3(600, 0, 0), v3(0, 0, -600), v3(0, 1, 0), 0, 0, 120, 120, 1, 1, 1, 1); }
    m_ground = gfx_mesh_static(g_b.v, g_b.n);
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
    /* the blessing box: six see-through faces with a question mark, inside a solid white frame of twelve rounded edges */
    batch_clear(&g_b); batch_box(&g_b, v3(0, 0, 0), v3(0.62f, 0.62f, 0.62f), 1, 1, 1); m_qface = gfx_mesh_static(g_b.v, g_b.n);
    batch_clear(&g_b);
    { const float e = 0.66f, t = 0.075f;
      for (int a = 0; a < 3; a++) for (int i = 0; i < 4; i++) { float u = (i & 1) ? e : -e, w = (i & 2) ? e : -e;
          Vec3 c = a == 0 ? v3(0, u, w) : a == 1 ? v3(u, 0, w) : v3(u, w, 0), h = a == 0 ? v3(e + t, t, t) : a == 1 ? v3(t, e + t, t) : v3(t, t, e + t);
          batch_box(&g_b, c, h, 1, 1, 1); } }
    m_qframe = gfx_mesh_static(g_b.v, g_b.n);
    /* shield: a bigger cube drawn transparent around the kart */
    batch_clear(&g_b); batch_box(&g_b, v3(0, 0.8f, 0), v3(1.4f, 1.2f, 1.6f), 1, 1, 1); m_shield = gfx_mesh_static(g_b.v, g_b.n);
    m_items = gfx_mesh_dynamic(MAX_PROJ * 12 + 80 * 12 + NKARTS * 12 + 64);
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
/* the Garden of Eden */
static void px_fruittree(int x, int y, uint32_t *o) {    /* a round green tree hung with red fruit */
    *o = 0;
    if (y > 58 && abs(x - 32) < 4) *o = rgba(105, 72, 40, 255);
    float dx = (x - 32) / 29.0f, dy = (y - 36) / 32.0f; float d = dx * dx + dy * dy; float n = smooth_noise(x / 5.0f, y / 5.0f);
    if (d < 1.0f && n > 0.18f) *o = rgba(40 + (int)(n * 50), 120 + (int)(n * 70), 40 + (int)(n * 30), 255);
    for (int f = 0; f < 7; f++) { static const int fx[7] = {18, 30, 44, 24, 40, 32, 14}, fy[7] = {30, 22, 32, 48, 50, 38, 44}; int ddx = x - fx[f], ddy = y - fy[f];
        if (ddx * ddx + ddy * ddy < 9) *o = rgba(220 + (ddx < 0 ? 30 : 0), 40, 40, 255); }
}
static void px_treeoflife(int x, int y, uint32_t *o) {   /* the Tree of Life: a great golden-leaved tree with twelve fruits and a glow */
    *o = 0;
    float t = y / 96.0f;
    if (y > 50 && abs(x - 32) < 3 + (int)(6 * (t - 0.5f))) *o = rgba(130, 95, 50, 255);                         /* a wide trunk */
    if (y > 44 && y < 62 && (abs(x - 20 - (y - 44)) < 2 || abs(x - 44 + (y - 44)) < 2)) *o = rgba(120, 88, 45, 255);   /* two boughs */
    float dx = (x - 32) / 31.0f, dy = (y - 30) / 28.0f; float d = dx * dx + dy * dy; float n = smooth_noise(x / 4.5f, y / 4.5f);
    if (d < 1.0f && n > 0.15f) *o = rgba(120 + (int)(n * 60), 170 + (int)(n * 60), 50 + (int)(n * 40), 255);
    if (d < 1.0f && n > 0.72f) *o = rgba(255, 235, 140, 255);                                                    /* golden leaves */
    for (int f = 0; f < 12; f++) { float a = f * 0.5236f; int fx = 32 + (int)(cosf(a) * 20), fy = 30 + (int)(sinf(a) * 17); int ddx = x - fx, ddy = y - fy;
        if (ddx * ddx + ddy * ddy < 8) *o = rgba(255, 200 + (f % 3) * 20, 60, 255); }
}
static void px_lion(int x, int y, uint32_t *o) {         /* a lion lying down, peaceful */
    *o = 0;
    float bx = (x - 36) / 24.0f, by = (y - 74) / 12.0f; if (bx * bx + by * by < 1.0f) *o = rgba(200, 160, 90, 255);     /* body */
    float mx = (x - 16) / 13.0f, my = (y - 62) / 13.0f; if (mx * mx + my * my < 1.0f) *o = rgba(150, 95, 40, 255);      /* mane */
    float hx = (x - 16) / 8.0f, hy = (y - 63) / 7.0f; if (hx * hx + hy * hy < 1.0f) *o = rgba(215, 175, 100, 255);      /* face */
    if (inrect(x, y, 12, 60, 14, 62) || inrect(x, y, 18, 60, 20, 62)) *o = rgba(40, 30, 20, 255);
    if (inrect(x, y, 6, 82, 62, 86)) *o = rgba(190, 150, 85, 255);                                                      /* paws and the ground line */
    if (y > 60 && y < 70 && abs(x - 58 - (y - 60)) < 2) *o = rgba(150, 95, 40, 255);                                    /* tail */
}
static void px_lamb(int x, int y, uint32_t *o) {         /* one white lamb */
    *o = 0; float dx = (x - 34) / 14.0f, dy = (y - 60) / 10.0f;
    if (dx * dx + dy * dy < 1.0f) *o = rgba(240, 240, 232, 255);
    if (inrect(x, y, 14, 48, 24, 60)) *o = rgba(40, 35, 30, 255);
    if (inrect(x, y, 24, 68, 28, 86) || inrect(x, y, 40, 68, 44, 86)) *o = rgba(40, 35, 30, 255);
}
static void px_deer(int x, int y, uint32_t *o) {         /* a deer with antlers */
    *o = 0; float dx = (x - 36) / 16.0f, dy = (y - 56) / 9.0f;
    if (dx * dx + dy * dy < 1.0f) *o = rgba(170, 120, 70, 255);
    if (inrect(x, y, 14, 30, 22, 54)) *o = rgba(160, 110, 60, 255);                                                     /* neck and head */
    if (inrect(x, y, 10, 28, 24, 36)) *o = rgba(165, 115, 65, 255);
    if (y > 10 && y < 30 && (abs(x - 12 - (30 - y) / 2) < 2 || abs(x - 22 + (30 - y) / 2) < 2)) *o = rgba(110, 80, 50, 255);   /* antlers */
    if (inrect(x, y, 24, 64, 28, 90) || inrect(x, y, 32, 64, 36, 90) || inrect(x, y, 42, 64, 46, 90) || inrect(x, y, 50, 64, 54, 90)) *o = rgba(140, 95, 55, 255);
}
static void px_flowers(int x, int y, uint32_t *o) {      /* a bank of flowers in many colors */
    *o = 0;
    for (int f = 0; f < 9; f++) { static const int fx[9] = {8, 18, 28, 38, 48, 58, 14, 34, 52}, fy[9] = {72, 62, 76, 60, 74, 66, 86, 88, 86};
        static const int fc[9][3] = {{255, 80, 90}, {255, 210, 60}, {120, 110, 255}, {255, 140, 200}, {255, 250, 250}, {255, 150, 40}, {160, 80, 230}, {255, 230, 100}, {90, 160, 255}};
        if (inrect(x, y, fx[f] - 1, fy[f], fx[f] + 1, 96)) *o = rgba(60, 130, 50, 255);
        int ddx = x - fx[f], ddy = y - fy[f]; if (ddx * ddx + ddy * ddy < 20) *o = rgba(fc[f][0], fc[f][1], fc[f][2], 255);
        if (ddx * ddx + ddy * ddy < 4) *o = rgba(255, 240, 150, 255); }
}
static void px_fern(int x, int y, uint32_t *o) {         /* a big fern */
    *o = 0;
    for (int f = 0; f < 5; f++) { float a = -1.3f + f * 0.65f; float dx = x - 32, dy = y - 92; float px2 = dx * sinf(a) - dy * cosf(a), py2 = dx * cosf(a) + dy * sinf(a);
        if (px2 > 0 && px2 < 60 && fabsf(py2) < 3.0f + (60 - px2) * 0.12f && ((int)(px2) % 6 < 4)) *o = rgba(40 + (int)(px2), 120 + (int)(px2 * 1.5f), 40, 255); }
}
static void px_path(int x, int y, uint32_t *o) {         /* a garden path: packed earth with moss and small stones */
    float n = smooth_noise(x / 11.0f, y / 11.0f) * 0.6f + smooth_noise(x / 4.0f, y / 4.0f) * 0.4f; float m = smooth_noise(x / 7.0f + 50, y / 7.0f);
    int r = 150 + (int)(n * 50), g = 125 + (int)(n * 45), b = 80 + (int)(n * 30);
    if (m > 0.68f) { r = 90 + (int)(n * 30); g = 140 + (int)(n * 40); b = 60; }         /* moss */
    if (smooth_noise(x / 2.5f + 9, y / 2.5f) > 0.82f) { r += 40; g += 35; b += 30; }      /* small stones */
    *o = rgba(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b, 255);
}
static void px_lush(int x, int y, uint32_t *o) { float n = smooth_noise(x / 14.0f, y / 14.0f) * 0.6f + smooth_noise(x / 4.0f, y / 4.0f) * 0.4f; *o = rgba(50 + (int)(n * 50), 150 + (int)(n * 80), 45 + (int)(n * 35), 255); }
static void px_hedge(int x, int y, uint32_t *o) { float n = smooth_noise(x / 6.0f, y / 6.0f) * 0.6f + smooth_noise(x / 2.5f, y / 2.5f) * 0.4f; int fl = smooth_noise(x / 3.0f + 20, y / 3.0f) > 0.86f; *o = fl ? rgba(255, 120, 150, 255) : rgba(25 + (int)(n * 50), 95 + (int)(n * 80), 30 + (int)(n * 30), 255); }
/* the Walls of Jericho */
static void px_jwall(int x, int y, uint32_t *o) {        /* a stretch of city wall, 128 x 64, battlements along the top */
    *o = 0;
    if (inrect(x, y, 0, 14, 128, 64)) *o = stone(x, y, 205, 180, 135);
    if (inrect(x, y, 0, 6, 128, 14) && ((x / 10) & 1)) *o = stone(x, y, 195, 170, 125);
    if (y >= 14 && ((y / 8 + (x / 16)) & 1) == 0 && (x % 16) < 1) *o = stone(x, y, 170, 148, 108);   /* joints between the stones */
    if (y >= 14 && (y % 8) == 0) *o = stone(x, y, 175, 152, 112);
}
static void px_jtower(int x, int y, uint32_t *o) {       /* a corner tower, taller, with a dark window */
    *o = 0;
    if (inrect(x, y, 8, 12, 56, 96)) *o = stone(x, y, 200, 175, 130);
    if (inrect(x, y, 6, 4, 58, 12) && ((x / 8) & 1)) *o = stone(x, y, 190, 165, 120);
    if ((y % 8) == 0 && y > 12) *o = stone(x, y, 172, 150, 110);
    if (inrect(x, y, 27, 36, 37, 50)) *o = rgba(40, 30, 20, 255);
}
static void px_rahab(int x, int y, uint32_t *o) {        /* Rahab's house on the wall: a window with the scarlet cord hanging from it (Joshua 2:18) */
    *o = 0;
    if (inrect(x, y, 4, 12, 60, 96)) *o = stone(x, y, 205, 180, 135);
    if (inrect(x, y, 2, 4, 62, 12) && ((x / 8) & 1)) *o = stone(x, y, 195, 170, 125);
    if (inrect(x, y, 24, 28, 40, 48)) *o = rgba(50, 35, 25, 255);                         /* the window */
    if (inrect(x, y, 30, 48, 34, 92)) *o = rgba(215, 30, 40, 255);                        /* the scarlet cord */
}
static void px_ark(int x, int y, uint32_t *o) {          /* the Ark of the Covenant on its poles, two priests carrying it */
    *o = 0;
    px_person(x, y, o, 240, 235, 220, 0); if (x > 32) { uint32_t t = 0; px_person(x - 30, y, &t, 240, 235, 220, 0); if (t) *o = t; }   /* two priests in white */
    if (inrect(x, y, 2, 50, 62, 54)) *o = rgba(200, 160, 60, 255);                        /* the poles */
    if (inrect(x, y, 18, 30, 46, 52)) *o = rgba(240, 195, 70, 255);                       /* the gold chest */
    if (inrect(x, y, 18, 28, 46, 31)) *o = rgba(255, 225, 120, 255);
    if (inrect(x, y, 24, 20, 30, 28) || inrect(x, y, 34, 20, 40, 28)) *o = rgba(255, 230, 140, 255);   /* the cherubim */
}
static void px_priest(int x, int y, uint32_t *o) {       /* a priest with a ram's horn to his lips */
    px_person(x, y, o, 240, 235, 220, 0);
    if (y > 14 && y < 24 && x > 36 && x < 36 + (y - 14) * 2 + 4) *o = rgba(190, 150, 90, 255);   /* the shofar, flaring out */
}
static void px_rubble(int x, int y, uint32_t *o) {       /* a heap of fallen stones */
    *o = 0;
    for (int b = 0; b < 6; b++) { static const int bx[6] = {14, 32, 50, 22, 42, 32}, by[6] = {80, 82, 80, 64, 66, 48}, br[6] = {13, 15, 13, 12, 12, 11};
        float dx = (float)(x - bx[b]) / br[b], dy = (float)(y - by[b]) / (br[b] * 0.8f); if (dx * dx + dy * dy < 1.0f) *o = stone(x, y, 190 - b * 6, 168 - b * 5, 125 - b * 4); }
}
/* drawn blessing icons, 64 x 64, until Sean's arrive */
static int g_icon_kind;
static float dist2(int x, int y, float cx, float cy, float rx, float ry) { float dx = (x - cx) / rx, dy = (y - cy) / ry; return dx * dx + dy * dy; }
static void px_icon(int x, int y, uint32_t *o) {
    *o = 0; float n = smooth_noise(x / 5.0f, y / 5.0f);
    switch (g_icon_kind) {
    case ITEM_MANNA:       /* a round loaf under rays of light */
        for (int r2 = 0; r2 < 7; r2++) { float a = -1.3f + r2 * 0.43f; float dx = x - 32, dy = y - 46; float along = dx * sinf(a) - dy * cosf(a), across = dx * cosf(a) + dy * sinf(a); if (along > 10 && along < 34 && fabsf(across) < 1.5f) *o = rgba(255, 240, 150, 255); }
        if (dist2(x, y, 32, 46, 22, 13) < 1) *o = rgba(205 + (int)(n * 40), 150 + (int)(n * 40), 80, 255);
        if (dist2(x, y, 32, 44, 20, 9) < 1 && y < 44) *o = rgba(235 + (int)(n * 20), 190 + (int)(n * 30), 110, 255); break;
    case ITEM_LOAVES:      /* a basket with loaves and two fish */
        if (y > 36 && y < 58 && abs(x - 32) < 24 - (y - 36) / 2) *o = rgba(150 + (int)(n * 40) + (((x + y) / 4) & 1) * 20, 100 + (int)(n * 30), 45, 255);
        for (int l = 0; l < 5; l++) if (dist2(x, y, 14 + l * 9, 34, 6, 5) < 1) *o = rgba(225, 175, 95, 255);
        for (int f = 0; f < 2; f++) { float cx = 20 + f * 24, cy = 16; if (dist2(x, y, cx, cy, 10, 4) < 1 || (x > cx + 8 && x < cx + 14 && abs(y - (int)cy) < (x - (int)cx - 7))) *o = rgba(90, 150, 220, 255); if (dist2(x, y, cx - 5, cy - 1, 1.2f, 1.2f) < 1) *o = rgba(20, 20, 40, 255); } break;
    case ITEM_SAMARITAN:   /* a flask of oil and a bandage */
        if (inrect(x, y, 6, 30, 58, 42) && ((x / 6) & 1)) *o = rgba(245, 240, 230, 255); else if (inrect(x, y, 6, 30, 58, 42)) *o = rgba(225, 220, 205, 255);
        if (dist2(x, y, 32, 44, 13, 15) < 1 || inrect(x, y, 27, 14, 37, 32)) *o = rgba(230 + (int)(n * 20), 150, 40, 255);
        if (inrect(x, y, 25, 10, 39, 16)) *o = rgba(120, 80, 40, 255); break;
    case ITEM_SHIELD:      /* a round shield with a gold cross */
        if (dist2(x, y, 32, 32, 27, 27) < 1) *o = rgba(120 + (int)(n * 30), 125 + (int)(n * 30), 140, 255);
        if (dist2(x, y, 32, 32, 27, 27) < 1 && dist2(x, y, 32, 32, 23, 23) > 1) *o = rgba(200, 160, 60, 255);
        if (inrect(x, y, 28, 12, 36, 52) || inrect(x, y, 16, 22, 48, 30)) *o = rgba(240, 200, 70, 255); break;
    case ITEM_PARTING:     /* two walls of water with the dry path between */
        if ((x < 22 || x > 42) && y > 8 + (int)(n * 6)) *o = rgba(40 + (int)(n * 40), 120 + (int)(n * 60), 210 + (int)(n * 40), 255);
        if ((x < 22 || x > 42) && y > 8 + (int)(n * 6) && y < 16 + (int)(n * 6)) *o = rgba(230, 245, 255, 255);
        if (x >= 22 && x <= 42 && y > 30) *o = rgba(200 + (int)(n * 30), 170 + (int)(n * 30), 110, 255); break;
    case ITEM_JUBILEE:     /* a ram's horn */
        { float t = (x - 8) / 48.0f; float cy = 46 - t * t * 30; float w = 3 + t * 9; if (x >= 8 && x < 58 && fabsf(y - cy) < w) *o = rgba(190 + (int)(n * 40), 150 + (int)(n * 30), 90, 255); } break;
    case ITEM_FIRE:        /* a chariot wheel in flames */
        if (dist2(x, y, 32, 40, 20, 20) < 1 && dist2(x, y, 32, 40, 15, 15) > 1) *o = rgba(80, 55, 30, 255);
        for (int sp = 0; sp < 4; sp++) { float a = sp * 0.785f; float dx = x - 32, dy = y - 40; float across = dx * cosf(a) + dy * sinf(a); float along = -dx * sinf(a) + dy * cosf(a); if (fabsf(across) < 1.6f && fabsf(along) < 18) *o = rgba(90, 60, 35, 255); }
        if (dist2(x, y, 32, 40, 4, 4) < 1) *o = rgba(200, 160, 60, 255);
        for (int f = 0; f < 5; f++) { float cx = 12 + f * 10, h = 14 + (f % 2) * 8 + n * 6; if (y < 26 && y > 26 - h && fabsf(x - cx) < (26 - y) * 0.35f + 1) *o = y < 26 - h * 0.6f ? rgba(255, 220, 80, 255) : rgba(255, 120 + (int)(n * 60), 30, 255); } break;
    case ITEM_WATER:       /* a jar pouring living water */
        if (dist2(x, y, 24, 30, 12, 16) < 1 || inrect(x, y, 16, 10, 32, 18)) *o = rgba(180 + (int)(n * 30), 120 + (int)(n * 30), 70, 255);
        if (inrect(x, y, 14, 8, 34, 12)) *o = rgba(140, 90, 50, 255);
        { float t = (y - 14) / 46.0f; float cx = 36 + t * t * 14; if (y > 14 && fabsf(x - cx) < 3 + t * 4) *o = rgba(120 + (int)(n * 60), 200 + (int)(n * 40), 255, 255); }
        for (int d = 0; d < 3; d++) if (dist2(x, y, 44 + d * 6, 52 + (d % 2) * 6, 2.5f, 3.5f) < 1) *o = rgba(200, 240, 255, 255); break;
    }
}
/* a "cute" item icon: the drawing, outlined in dark, on a round colored badge with a white rim and a shine */
static Texture *make_icon(int kind) {
    static const uint8_t badge[ITEM_COUNT][3] = {{0,0,0}, {255,214,120}, {240,180,110}, {150,215,140}, {140,170,240}, {110,190,240}, {230,170,110}, {250,130,80}, {130,210,240}};
    static uint32_t src[64 * 64], out[96 * 96];
    g_icon_kind = kind; for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) px_icon(x, y, &src[y * 64 + x]);
    for (int y = 0; y < 96; y++) for (int x = 0; x < 96; x++) {
        float dx = (x + 0.5f - 48) / 46.0f, dy = (y + 0.5f - 48) / 46.0f, d = sqrtf(dx * dx + dy * dy); uint32_t o = 0;
        if (d < 1.0f) { float sh = 1.08f - 0.35f * (dy + 1) * 0.5f; int r = (int)(badge[kind][0] * sh), g = (int)(badge[kind][1] * sh), b = (int)(badge[kind][2] * sh);
            if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255; o = d > 0.9f ? rgba(255, 255, 255, 255) : rgba(r, g, b, 255);
        }   /* badge with a white rim, lighter at the top */
        int sx = (x - 16), sy = (y - 16);                                                                                   /* the drawing sits in the middle 64 x 64 */
        if (sx >= 0 && sx < 64 && sy >= 0 && sy < 64 && (src[sy * 64 + sx] & 255) > 40) o = src[sy * 64 + sx] | 255;
        else if (d < 0.9f) { int near = 0; for (int oy = -2; oy <= 2 && !near; oy++) for (int ox = -2; ox <= 2; ox++) { int qx = sx + ox, qy = sy + oy;
                if (qx >= 0 && qx < 64 && qy >= 0 && qy < 64 && (src[qy * 64 + qx] & 255) > 40) { near = 1; break; } }
            if (near) o = rgba(40, 30, 50, 255); }                                                                        /* dark outline */
        out[y * 96 + x] = o; }
    for (int y = 6; y < 30; y++) for (int x = 22; x < 50; x++) { float dx = (x - 36) / 14.0f, dy = (y - 18) / 7.0f; if (dx * dx + dy * dy < 1 && (out[y * 96 + x] & 255)) {   /* a soft shine */
        uint32_t c = out[y * 96 + x]; int r = ((c >> 24) & 255) + 50, g = ((c >> 16) & 255) + 50, b = ((c >> 8) & 255) + 50; if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255; out[y * 96 + x] = rgba(r, g, b, 255); } }
    return gfx_texture_rgba(out, 96, 96, 0, 0);
}
static int human_kart(int p);
/* the item window: after a box it spins through the blessings like a rolodex, slows, and stops on the one you got */
#define ROU_N 14
#define ROU_T 1.4f
static uint8_t g_rou_seq[MAX_HUMANS][ROU_N + 1]; static float g_rou_land[MAX_HUMANS]; static int g_rou_last[MAX_HUMANS];
static Mesh *m_rou[MAX_HUMANS][2];
static void rou_start(int p, int item) {
    static uint32_t seed = 12345; int prev = 0;
    for (int i = 0; i < ROU_N; i++) { int v; do { seed = seed * 1664525u + 1013904223u; v = 1 + (int)((seed >> 16) % (ITEM_COUNT - 1)); } while (v == prev || (i == ROU_N - 1 && v == item)); g_rou_seq[p][i] = (uint8_t)v; prev = v; }
    g_rou_seq[p][ROU_N] = (uint8_t)item; g_rou_land[p] = 99; g_rou_last[p] = 0;
}
static float rou_phase(int p) { float u = 1.0f - g_proulette[p] / ROU_T; if (u < 0) u = 0; if (u > 1) u = 1; return (1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u)) * ROU_N; }
static void icon_rect_clipped(Batch *b, int item, float x, float top, float size, float clip0, float clip1, float scale) {
    float pad = size * 0.06f, avail = (size - 2 * pad) * scale, asp = g_item_asp[item] > 0 ? g_item_asp[item] : 1.0f;
    float iw = asp >= 1 ? avail : avail * asp, ih = asp >= 1 ? avail / asp : avail, ix = x + (size - iw) / 2, iy = top + (size - ih) / 2;
    float y0 = iy > clip0 ? iy : clip0, y1 = iy + ih < clip1 ? iy + ih : clip1; if (y1 <= y0) return;
    batch_rect_uv(b, ix, y0, iw, y1 - y0, 0, (y0 - iy) / ih, 1, (y1 - iy) / ih, 1, 1, 1, 1);
}
static void draw_item_window(int p, float x, float y, float size) {   /* the pictures in the window (the frame is in the HUD batch) */
    Kart *k = &g_race.k[human_kart(p)]; Mat4 id = m4_identity(); if (!k->item) return;
    for (int j = 0; j < 2; j++) if (!m_rou[p][j]) m_rou[p][j] = gfx_mesh_dynamic(6);
    if (g_proulette[p] > 0) { float f = rou_phase(p); int i0 = (int)f; float fr = f - i0;
        for (int j = 0; j < 2; j++) { int idx = i0 + j; if (idx > ROU_N) break; int it = g_rou_seq[p][idx]; if (!tx_item[it]) continue;
            batch_clear(&g_b); icon_rect_clipped(&g_b, it, x, y + (j - fr) * size, size, y, y + size, 1.0f);
            if (!g_b.n) continue; gfx_mesh_update(m_rou[p][j], g_b.v, g_b.n); gfx_bind(tx_item[it]); gfx_draw(m_rou[p][j], &id, 1, 1, 1, 1); } }
    else if (tx_item[k->item]) { float t = g_rou_land[p] < 99 ? g_rou_land[p] : 1, pop = 1.0f + 0.22f * expf(-t * 6.0f) * cosf(t * 18.0f);
        batch_clear(&g_b); icon_rect_clipped(&g_b, k->item, x, y, size, y - size, y + 2 * size, pop);
        gfx_mesh_update(m_rou[p][0], g_b.v, g_b.n); gfx_bind(tx_item[k->item]); gfx_draw(m_rou[p][0], &id, 1, 1, 1, 1); }
}
static void hud_item_window(int p, float x, float y, float size, int text_below) {   /* the frame and the name */
    Kart *k = &g_race.k[human_kart(p)];
    batch_rect(&g_b, x - 4, y - 4, size + 8, size + 8, 1, 0.82f, 0.35f, 0.95f);
    batch_rect(&g_b, x, y, size, size, 0.05f, 0.08f, 0.22f, 0.88f);
    batch_rect(&g_b, x, y, size, size * 0.12f, 1, 1, 1, 0.08f);
    if (!k->item) { const char *h = "ZL"; batch_text(&g_b, h, x + size / 2 - text_width(h, size * 0.22f) / 2, y + size / 2 - size * 0.11f, size * 0.22f, 0.6f, 0.7f, 0.9f, 0.6f); return; }
    if (g_proulette[p] > 0) return;
    const char *nm = item_name(k->item); float fs = size * 0.15f; if (fs < 11) fs = 11;
    if (text_below) { if (text_width(nm, fs) > size * 1.9f) fs *= 0.8f; batch_text(&g_b, nm, x + size / 2 - text_width(nm, fs) / 2, y + size + 10, fs, 1, 0.9f, 0.4f, 1);
        if (g_rou_land[p] < 3.0f) { const char *v = item_verse(k->item); batch_text(&g_b, v, x + size / 2 - text_width(v, fs * 0.72f) / 2, y + size + 14 + fs, fs * 0.72f, 0.8f, 0.85f, 1, clampf(3.0f - g_rou_land[p], 0, 1)); } }
    else batch_text(&g_b, nm, x + size + 10, y + size / 2 - fs / 2, fs, 1, 0.9f, 0.4f, 1);
}
static void px_shieldbearer(int x, int y, uint32_t *o) {   /* a Philistine soldier behind a tall bronze shield */
    px_person(x, y, o, 150, 40, 40, 1);
    float dx = (x - 30) / 20.0f, dy = (y - 52) / 34.0f; if (dx * dx + dy * dy < 1.0f) { float n = smooth_noise(x / 4.0f, y / 4.0f); *o = rgba(190 + (int)(n * 40), 140 + (int)(n * 30), 50, 255); }
    if (dx * dx + dy * dy < 1.0f && dx * dx + dy * dy > 0.8f) *o = rgba(120, 80, 30, 255);
    if (dx * dx + dy * dy < 0.05f) *o = rgba(235, 200, 90, 255);
}
static void px_desert(int x, int y, uint32_t *o) { float n = smooth_noise(x / 12.0f, y / 12.0f) * 0.6f + smooth_noise(x / 4.0f, y / 4.0f) * 0.4f; int d = smooth_noise(x / 6.0f + 30, y / 6.0f) > 0.75f; *o = d ? rgba(150, 140, 95, 255) : rgba(205 + (int)(n * 30), 180 + (int)(n * 30), 125 + (int)(n * 25), 255); }
static Texture *placeholder_sprite(const char *id, float *aspect) {
    *aspect = (float)PW / (float)PH;
    static const struct { const char *id; void (*fn)(int, int, uint32_t *); } table[] = {
        {"#hut", px_hut}, {"#net", px_net}, {"#rock", px_rock}, {"#boat", px_boat}, {"#cypress", px_cypress}, {"#pillar", px_pillar},
        {"#tower", px_tower}, {"#wallseg", px_wallseg}, {"#arch", px_arch}, {"#stall", px_stall}, {"#cart", px_cart}, {"#lily", px_lily},
        {"#tomb", px_tomb}, {"#crosses", px_crosses}, {"#stone", px_stone}, {"#cloud", px_cloudpuff}, {"#gpillar", px_gpillar}, {"#crystal", px_crystal},
        {"#israelite", px_israelite}, {"#philistine", px_philistine}, {"#banner_israel", px_banner_israel}, {"#banner_philistine", px_banner_philistine}, {"#tent", px_tent}, {"#camp", px_tent},
        {"#fruittree", px_fruittree}, {"#treeoflife", px_treeoflife}, {"#lion", px_lion}, {"#lamb", px_lamb}, {"#deer", px_deer}, {"#flowers", px_flowers}, {"#fern", px_fern} };
    if (!strcmp(id, "#waterfall")) { *aspect = 0.5f; return make_tex(64, 128, 1, 0, px_waterfall); }
    if (!strcmp(id, "#jwall")) { *aspect = 2.0f; return make_tex(128, 64, 0, 0, px_jwall); }
    if (!strcmp(id, "#jtower")) return make_tex(PW, PH, 0, 0, px_jtower);
    if (!strcmp(id, "#rahab")) return make_tex(PW, PH, 0, 0, px_rahab);
    if (!strcmp(id, "#ark")) return make_tex(PW, PH, 0, 0, px_ark);
    if (!strcmp(id, "#priest")) return make_tex(PW, PH, 0, 0, px_priest);
    if (!strcmp(id, "#rubble")) return make_tex(PW, PH, 0, 0, px_rubble);
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
    k->placeholder = k->tex == NULL;
    if (!k->tex) k->tex = placeholder_sprite(img, &k->aspect);
    k->sway = (strstr(img, "palm") || strstr(img, "olive") || strstr(img, "cypress") || strstr(img, "tree") || strstr(img, "fern") || strstr(img, "flowers")) ? 1 : ((strstr(img, "banner") || strstr(img, "flag") || strstr(img, "net") || strstr(img, "tent")) ? 2 : 0);
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
    int heaven = d->grid_idx > 0, eden = !strcmp(d->prefix, "eden"), desert = d->bonus == 3;
    tx_road = theme_tex(d->tex_road, 256, 256, heaven ? px_gold : (eden ? px_path : px_road));
    tx_dirt = theme_tex(d->tex_dirt, 128, 128, d->water ? px_sand : (heaven ? px_cloud : (eden ? px_lush : (desert ? px_sand : px_dirt))));
    tx_grass = theme_tex(d->tex_grass, 128, 128, d->water ? px_water : (eden ? px_lush : (desert ? px_desert : px_grass)));
    tx_wall = theme_tex(d->tex_wall, 64, 64, heaven ? px_cloud : (eden ? px_hedge : px_wall));
    g_skytop[0] = d->sky_r * 0.8f; g_skytop[1] = d->sky_g * 0.85f; g_skytop[2] = d->sky_b; g_skyhor[0] = d->fog_r; g_skyhor[1] = d->fog_g; g_skyhor[2] = d->fog_b;
    { int sw = 0, sh = 0; Texture *painted = asset_texture(d->tex_sky, 1, &sw, &sh);
      if (painted) { tx_sky = painted; g_sky_wrap = sw >= sh * 3 ? 1.0f : 3.0f; }
      else { tx_sky = make_tex(256, 256, 1, 0, d->night ? px_sky_night : px_sky); g_sky_wrap = 3.0f; } }
    /* sky: a big cylinder-ish ring far away (drawn without depth), built once the picture (and so the wrap count) is known.
       Going around, u runs forward continuously; the picture's bottom row (v = 1) sits at the horizon. */
    batch_clear(&g_b);
    for (int k = 0; k < 24; k++) {
        float a0 = k * 6.2831853f / 24, a1 = (k + 1) * 6.2831853f / 24, R = 900.0f;
        float top = g_sky_wrap < 1.5f ? -60 + 6.2831853f * R / 4.0f : 420;      /* a painted 4:1 sky goes around once, so the ring is a quarter of its circumference tall to keep the clouds in shape */
        float u0 = k / 24.0f * g_sky_wrap, u1 = (k + 1) / 24.0f * g_sky_wrap;
        Vec3 p0 = v3(cosf(a0) * R, -60, sinf(a0) * R), p1 = v3(cosf(a1) * R, -60, sinf(a1) * R), p2 = v3(cosf(a1) * R, top, sinf(a1) * R), p3 = v3(cosf(a0) * R, top, sinf(a0) * R);
        batch_quad4(&g_b, p1, p0, p3, p2, v3(0, 1, 0), u1, 0, u0, 1, 1, 1, 1, 1);   /* corners get (u1,1) (u0,1) (u0,0) (u1,0) */
    }
    gfx_mesh_free(m_sky); m_sky = gfx_mesh_static(g_b.v, g_b.n);
    gfx_texture_free(tx_finale); tx_finale = d->finale_img ? asset_texture(d->finale_img, 0, NULL, NULL) : NULL;
    tx_star = NULL;
    if (d->star_size > 0) { tx_star = asset_texture("beth_star", 0, NULL, NULL); if (!tx_star && g_sky_wrap > 1.5f) tx_star = make_tex(64, 64, 0, 0, px_star); }
    g_nkind = 0; g_nplaced = 0; g_nwalls = 0;
    for (int i = 0; i < g_track.nmover; i++) g_mover_kind[i] = sprite_kind(g_track.mover[i].img);
    for (int i = 0; i < d->nscenery && g_nplaced < MAX_SCENERY; i++) {
        const Scenery *sc = &d->scenery[i]; int idx = sc->idx % g_track.n;
        Placed *p = &g_placed[g_nplaced++];
        p->kind = sprite_kind(sc->img);
        float edge = track_edge(&g_track, idx);
        p->p = v3(g_track.s[idx].x + g_track.s[idx].nx * sc->side, 0, g_track.s[idx].z + g_track.s[idx].nz * sc->side);
        p->p.y = track_surface_height(&g_track, idx, (sc->side < 0 ? -1.0f : 1.0f) * edge) - 0.2f;   /* stands on the ground just outside the wall */
        p->h = sc->height; p->w = sc->height * g_kind[p->kind].aspect; p->glow = sc->glow; p->purple = !strcmp(sc->img, "#crystal"); p->falls = !strcmp(sc->img, "#waterfall");
        p->wall = (!strcmp(sc->img, "#jwall") || !strcmp(sc->img, "#jtower") || !strcmp(sc->img, "#rahab")) ? ++g_nwalls : 0; p->fall_t = 0;
    }
    m_glow = gfx_mesh_dynamic((MAX_SCENERY + NKARTS) * 6 + 6);
    /* a soft shadow blob under everything that stands by the road */
    batch_clear(&g_b);
    for (int i = 0; i < g_nplaced; i++) { Placed *pl = &g_placed[i]; if (pl->falls) continue;
        float w = pl->w * 0.55f, dz = pl->w * 0.3f;
        batch_quad(&g_b, v3(pl->p.x, pl->p.y + 0.26f, pl->p.z), v3(w, 0, 0), v3(0, 0, -dz), v3(0, 1, 0), 0, 0, 1, 1, 0, 0, 0, 0.45f); }
    gfx_mesh_free(m_scshadow); m_scshadow = gfx_mesh_static(g_b.v, g_b.n);
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
static int human_kart(int p);
static void reset_views(void) {   /* every player's camera starts just behind their kart */
    for (int p = 0; p < MAX_HUMANS; p++) { View *v = &g_view[p]; Kart *k = &g_race.k[p < g_nplayers ? human_kart(p) : g_race.player];
        v->yaw = k->yaw; v->x = k->x - sinf(k->yaw) * 8; v->z = k->z - cosf(k->yaw) * 8; v->y = k->y + 3.5f; v->fov = 62.0f; v->shake = 0; v->orbit = 0; v->finish_t = 0;
        g_pmsg_t[p] = 0; g_pmsg2[p][0] = 0; g_pside_t[p] = 0; g_proulette[p] = 0; }
    cv = &g_view[0];
}
static void start_race(int attract) {
    g_attract = attract; g_slowmo = 0; g_rec_n = 0; g_rec_t = 0;
    if (g_mode == MODE_TT && !attract) ghost_load(g_track_idx);
    int nk = (g_mode == MODE_TT && !attract) ? 1 : NKARTS;
    g_nplayers = (g_mode == MODE_VS && !attract && g_njoined >= 2) ? g_njoined : 1;
    if (g_nplayers > 1) race_init_multi(&g_race, &g_track, g_pchar, NULL, g_nplayers, g_difficulty);
    else race_init(&g_race, &g_track, attract && g_screen != SCR_TRACK ? g_demo_char : g_player_char, nk == 1 ? 0 : 5, g_difficulty, nk);
    if (g_def->bonus == 3) race_set_jericho(&g_race);
    if (attract) g_race.cd = 3.5f;   /* the demo skips the get-ready pause */
    for (int i = 0; i < g_nplaced; i++) g_placed[i].fall_t = 0;
    if (!attract) { g_sting_due = 1; sfx_play(SFX_ENGINE_START, 0.8f, 0); }   /* the recorded 3-2-1-GO starts later, timed so its last hit lands on GO */
    g_skid_n = 0; memset(g_skid, 0, sizeof g_skid); memset(g_dust, 0, sizeof g_dust); memset(g_last_skid, 0, sizeof g_last_skid);
    reset_views();
    g_results_t = 0; g_demo = attract; g_idle = attract ? 99 : 0;
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
static void draw_loading(const char *msg);
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
        tx_blob = make_tex(64, 64, 0, 0, px_blob); tx_vignette = make_tex(256, 256, 0, 0, px_vignette); tx_bird = make_tex(32, 32, 0, 0, px_bird);
        tx_flag = make_tex(32, 32, 1, 1, px_flag);
        { Texture *hd = asset_texture("font_hd", 0, NULL, NULL);
          if (hd) { tx_font = hd; g_font_hd = 1; }
          else { static uint32_t fpx[FONT_TEX_W * FONT_TEX_H]; font_build_texture(fpx); tx_font = gfx_texture_rgba(fpx, FONT_TEX_W, FONT_TEX_H, 0, 1); } }
        draw_loading("LOADING...");
        static const char *pn[NCHARS] = {"portrait_jesus", "portrait_moses", "portrait_david", "portrait_esther", "portrait_peter", "portrait_mary", "portrait_daniel", "portrait_samson", "portrait_noah", "portrait_joseph", "portrait_jonah", "portrait_john", "portrait_michael", "portrait_gabriel"};
        for (int i = 0; i < NCHARS; i++) { int w = 1, h = 1; tx_portrait[i] = asset_texture(pn[i], 0, &w, &h); g_portrait_aspect[i] = h > 0 ? (float)w / (float)h : 1.0f;
            char cn[48]; snprintf(cn, sizeof cn, "char_%s", pn[i] + 9); w = h = 1; tx_char[i] = asset_texture(cn, 0, &w, &h); g_char_aspect[i] = h > 0 ? (float)w / (float)h : 1.0f;
            snprintf(cn, sizeof cn, "kart_%s", pn[i] + 9); w = h = 1; tx_kart[i] = asset_texture(cn, 0, &w, &h); g_kart_aspect[i] = h > 0 ? (float)w / (float)h : 1.6f;
            snprintf(cn, sizeof cn, "kartback_%s", pn[i] + 9); w = h = 1; tx_kartback[i] = asset_texture(cn, 0, &w, &h); g_kartback_aspect[i] = h > 0 ? (float)w / (float)h : 1.6f;
            if (!tx_kartback[i]) { snprintf(cn, sizeof cn, "ride_%s_b", pn[i] + 9); w = h = 1; tx_kartback[i] = asset_texture(cn, 0, &w, &h); g_kartback_aspect[i] = h > 0 ? (float)w / (float)h : 1.4f; }   /* no kart-only behind picture: the riding one stands in */
            tx_kartview[i][0] = tx_kartback[i]; g_kartview_aspect[i][0] = g_kartback_aspect[i]; tx_kartview[i][4] = tx_kart[i]; g_kartview_aspect[i][4] = g_kart_aspect[i];
            static const char *suf[3] = {"bl", "l", "fl"};
            for (int v = 0; v < 3; v++) { snprintf(cn, sizeof cn, "kartview_%s_%s", pn[i] + 9, suf[v]); w = h = 1; tx_kartview[i][v + 1] = asset_texture(cn, 0, &w, &h); g_kartview_aspect[i][v + 1] = h > 0 ? (float)w / (float)h : 1.6f;
                if (tx_kartview[i][v + 1] && (g_view_wd[i][v + 1] = wheel_def(cn)) != NULL) g_view_wtex[i][v + 1] = wheel_tex(cn); }
            static const char *rsuf[5] = {"b", "bl", "l", "fl", "f"};
            for (int v = 0; v < 5; v++) { snprintf(cn, sizeof cn, "ride_%s_%s", pn[i] + 9, rsuf[v]); w = h = 1; tx_ride[i][v] = asset_texture(cn, 0, &w, &h); g_ride_aspect[i][v] = h > 0 ? (float)w / (float)h : 1.4f;
                if (tx_ride[i][v] && (g_ride_wd[i][v] = wheel_def(cn)) != NULL) g_ride_wtex[i][v] = wheel_tex(cn);
                if (tx_ride[i][v]) g_ride_td[i][v] = tread_def(cn); } }
        for (int i = 0; i < NKARTS; i++) { m_kart_spr[i] = gfx_mesh_dynamic(6); m_wheel_spr[i] = gfx_mesh_dynamic(18); m_tread_spr[i] = gfx_mesh_dynamic(18); }
        tx_tread = make_tex(16, 64, 1, 0, px_tread);
        tx_trophy = make_tex(128, 128, 0, 0, px_trophy);
        tx_qbox = make_tex(128, 128, 0, 0, px_qbox);
        m_kartpic = gfx_mesh_dynamic(6);
        static const char *in[ITEM_COUNT] = {"", "icon_manna", "icon_loaves", "icon_samaritan", "icon_shield", "icon_parting", "shofar", "icon_fire", "icon_water"};
        for (int i = 1; i < ITEM_COUNT; i++) { int w = 1, h = 1; tx_item[i] = asset_texture(in[i], 0, &w, &h); g_item_sq[i] = w == h; g_item_asp[i] = h > 0 ? (float)w / (float)h : 1.0f;
            if (!tx_item[i]) { tx_item[i] = make_icon(i); g_item_sq[i] = 1; g_item_asp[i] = 1.0f; } }
        for (int c = 0; c < NCHARS; c++) for (int e = 0; e < VO_COUNT; e++) { char vn[64]; snprintf(vn, sizeof vn, "voice_%s_%s", pn[c] + 9, VO_NAMES[e]); g_voice[c][e] = sfx_voice_load(vn);
            if (g_voice[c][e] < 0) { snprintf(vn, sizeof vn, "voice_%s", VO_NAMES[e]); g_voice[c][e] = sfx_voice_load(vn); } }
        { int w = 2, h = 3; tx_goliath = asset_texture("char_goliath", 0, &w, &h); g_goliath_aspect = (float)w / (float)h; m_goliath = gfx_mesh_dynamic(6); m_hud2 = gfx_mesh_dynamic(8192);
          w = 2; h = 3; tx_bearer = asset_texture("char_shieldbearer", 0, &w, &h); if (tx_bearer) g_bearer_aspect = (float)w / (float)h; else { tx_bearer = make_tex(PW, PH, 0, 0, px_shieldbearer); g_bearer_aspect = (float)PW / (float)PH; } m_bearer = gfx_mesh_dynamic(6); }
        tx_title = asset_texture("title_art", 0, NULL, NULL);
        for (int d = 0; d < NUM_TRACK_DEFS && d < 8; d++) { char cn[48]; snprintf(cn, sizeof cn, "card_%s", TRACK_DEFS[d].prefix); tx_card[d] = asset_texture(cn, 0, NULL, NULL); }
        if (tx_title) { int w = 1024, h = 437; gfx_texture_free(tx_title); tx_title = asset_texture("title_art", 0, &w, &h); float lw = 820, lh = lw * h / (float)w;
            batch_init(&g_b, g_vtmp, 8192); batch_clear(&g_b); batch_rect_uv(&g_b, 640 - lw / 2, 60, lw, lh, 0, 0, 1, 1, 1, 1, 1, 1); m_title = gfx_mesh_static(g_b.v, g_b.n); g_title_h = lh;
            float sw = 290, sh = sw * h / (float)w; batch_clear(&g_b); batch_rect_uv(&g_b, 640 - sw / 2, 6, sw, sh, 0, 0, 1, 1, 1, 1, 1, 1); m_logo_small = gfx_mesh_static(g_b.v, g_b.n); }
        tx_title_bg = asset_texture("title_bg", 0, NULL, NULL);
        if (tx_title_bg) { batch_init(&g_b, g_vtmp, 8192); batch_clear(&g_b); batch_rect_uv(&g_b, 0, 0, 1280, 720, 0, 0, 1, 1, 1, 1, 1, 1); m_title_bg = gfx_mesh_static(g_b.v, g_b.n); }
    }
    set_track(0);
    start_race(1);
    g_time = 0; g_screen = SCR_TITLE;
#if !defined(KK_TOUR) || KK_TOUR_ROAD == 7   /* the CI tour has a fixed timetable; only tour 7 watches the opener */
    if (status == GFX_OK && asset_exists("video/opener.txt")) {
        char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "video/opener.txt"); FILE *f = fopen(path, "r");
        if (f) { int w = 0, h = 0; if (fscanf(f, "%d %d %d %d", &g_op_frames, &g_op_fps, &w, &h) == 4 && g_op_frames > 0) { g_op_w = w; g_op_h = h;
            batch_init(&g_b, g_vtmp, 8192); batch_clear(&g_b); batch_rect_uv(&g_b, 0, 0, 1280, 720, 0, 0, 1, 1, 1, 1, 1, 1); m_op = gfx_mesh_static(g_b.v, g_b.n);
            g_screen = SCR_OPENER; g_screen_t = 0; gfx_log("opener: %d frames at %d fps", g_op_frames, g_op_fps); } fclose(f); } }
#endif
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
/* which kart a player drives, and the message / rumble helpers that go to one player's screen and controller */
static int human_kart(int p) { Race *r = &g_race; if (g_nplayers <= 1) return r->player; int n = 0; for (int i = 0; i < r->nk; i++) if (r->human[i]) { if (n == p) return i; n++; } return r->player; }
static void pmsg(int who, float t, const char *s) { if (who < 0 || who >= MAX_HUMANS) return; snprintf(g_pmsg[who], sizeof g_pmsg[who], "%s", s); g_pmsg_t[who] = t; }
static void pside(int who, float t, const char *s) { if (who < 0 || who >= MAX_HUMANS) return; snprintf(g_pside[who], sizeof g_pside[who], "%s", s); g_pside_t[who] = t; }
static void pmsg2(int who, float min_t, const char *s) { if (who < 0 || who >= MAX_HUMANS) return; snprintf(g_pmsg2[who], sizeof g_pmsg2[who], "%s", s); if (g_pmsg_t[who] < min_t) g_pmsg_t[who] = min_t; }
static void prumble(int who, float strength, float seconds) { if (who < 0 || !g_save.rumble) return; if (g_nplayers <= 1) platform_rumble(strength, seconds); else platform_rumble_pad(g_pslot[who], strength, seconds); }
/* a driver shouts: the player's own voice full on, a computer driver's only when close */
static void voice_at(int kart, int ev) {
    Race *r = &g_race; if (kart < 0 || kart >= r->nk || ev < 0 || ev >= VO_COUNT) return; int ch = r->character[kart]; int h = g_voice[ch][ev]; if (h < 0) return;
    Kart *q = &r->k[kart], *k = &r->k[r->player];
    if (race_human_index(r, kart) >= 0) { sfx_voice_play(h, 1.0f, 0); return; }
    float dx = q->x - k->x, dz = q->z - k->z, d = sqrtf(dx * dx + dz * dz); if (d > 45.0f || frand01() < 0.4f) return;
    float side = -(dx * cosf(cam_yaw) - dz * sinf(cam_yaw)); sfx_voice_play(h, 0.8f / (1.0f + d * 0.05f), clampf(side * 0.05f, -0.8f, 0.8f));
}
static void add_spark(Vec3 p, Vec3 v, float size, float life, float r, float g, float b) {
    for (int i = 0; i < MAX_SPARK; i++) if (g_spark[i].life <= 0) { Spark *s = &g_spark[i]; s->p = p; s->v = v; s->size = size; s->life = life; s->r = r; s->g = g; s->b = b; return; }
}
/* a burst of rainbow sparks and an expanding ring where a box was taken */
#define MAX_RING 8
typedef struct { Vec3 p; float t; } Ring; static Ring g_ring[MAX_RING];
static void rainbow_burst(Vec3 p) {
    for (int n = 0; n < 48; n++) { float h = n / 48.0f * 6.2831853f, a = n * 2.399f, up = 0.3f + frand01() * 0.7f;
        float r_ = 0.5f + 0.5f * sinf(h), g_ = 0.5f + 0.5f * sinf(h + 2.094f), b_ = 0.5f + 0.5f * sinf(h + 4.189f);
        float sp = 4.0f + frand01() * 6.0f;
        add_spark(v3(p.x, p.y + 0.6f, p.z), v3(cosf(a) * sp, up * 7.0f, sinf(a) * sp), 0.25f + frand01() * 0.2f, 0.6f + frand01() * 0.5f, r_, g_, b_); }
    for (int i = 0; i < MAX_RING; i++) if (g_ring[i].t <= 0) { g_ring[i].p = p; g_ring[i].t = 0.6f; break; }
}
/* a soft shower of golden sparkles over a kart that was just blessed */
static void bless_sparkle(const Kart *q, float r_, float g_, float b_) {
    for (int n = 0; n < 14; n++) { float a = frand01() * 6.2831853f, d = frand01() * 1.4f;
        add_spark(v3(q->x + cosf(a) * d, q->y + 1.2f + frand01() * 1.5f, q->z + sinf(a) * d), v3(0, 1.5f + frand01() * 2.0f, 0), 0.14f + frand01() * 0.1f, 0.8f + frand01() * 0.5f, r_, g_, b_); }
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
/* one frame with a message on both screens, shown while the pictures load (the game is otherwise black for a few seconds) */
static void draw_loading(const char *msg) {
    static Mesh *m = NULL; if (!m) m = gfx_mesh_dynamic(2048);
    batch_init(&g_b, g_vtmp, 8192); batch_clear(&g_b);
    checker_band(300, 20); text_c("KINGDOM KART", 340, 48, 1, 0.85f, 0.3f, 1); text_c(msg, 410, 24, 0.9f, 0.95f, 1, 1); checker_band(450, 20);
    gfx_mesh_update(m, g_b.v, g_b.n);
    Mat4 id = m4_identity();
    gfx_frame_begin();
    for (int drc = 0; drc < 2; drc++) { gfx_view_begin(drc, 0.02f, 0.04f, 0.12f); gfx_camera_2d(); gfx_state(ST_HUD); gfx_bind(tx_font); gfx_draw(m, &id, 1, 1, 1, 1); gfx_view_end(drc); }
    gfx_frame_end();
}
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
    else for (int p = 0; p < r->nk; p++) { int i = r->order[p]; const Character *c = &CHARACTERS[r->character[i]]; float y = 125 + p * 42; int hp = race_human_index(r, i), me = hp >= 0;
        if (me) batch_rect(&g_b, 790, y - 5, 460, 36, PLAYER_COL[hp][0], PLAYER_COL[hp][1], PLAYER_COL[hp][2], 0.25f);
        batch_text(&g_b, place_name(p + 1), 800, y, 22, 1, 1, 1, 1); batch_rect(&g_b, 880, y + 2, 22, 22, c->r, c->g, c->b, 1); batch_text(&g_b, c->short_name, 915, y, 22, 1, 1, 1, 1);
        if (me && g_nplayers > 1) { char tag[8]; snprintf(tag, sizeof tag, "P%d", hp + 1); batch_text(&g_b, tag, 1060, y, 18, PLAYER_COL[hp][0], PLAYER_COL[hp][1], PLAYER_COL[hp][2], 1); }
        if (r->k[i].lap >= 1) { snprintf(buf, sizeof buf, "LAP %d", r->k[i].lap > g_track.laps ? g_track.laps : r->k[i].lap); batch_text(&g_b, buf, 1120, y, 18, 0.8f, 0.9f, 1, 1); } }
    if (g_mode != MODE_TT && g_mode != MODE_BATTLE && g_nplayers <= 1) { Kart *k = &r->k[r->player]; snprintf(buf, sizeof buf, "ITEM: %s", k->item ? item_name(k->item) : "-"); batch_text(&g_b, buf, 800, 500, 22, 1, 0.9f, 0.4f, 1);
        snprintf(buf, sizeof buf, "COINS x%d", k->coins); batch_text(&g_b, buf, 800, 540, 22, 1, 0.9f, 0.5f, 1); }
    { char tb[32]; fmt_time(tb, sizeof tb, r->k[r->player].total_time); snprintf(buf, sizeof buf, "TIME %s", tb); batch_text(&g_b, buf, 800, 600, 22, 1, 1, 1, 1); }
    if (g_nplayers <= 1 && g_msg_t > 0) text_c(g_msg, 660, 30, 1, 0.95f, 0.4f, 1);
}
/* a verse for each road, shown during the countdown (King James text) */
static const char *road_verse(const char *prefix, const char **ref) {
    static const struct { const char *p, *v, *r; } V[] = {
        {"beth", "FOR UNTO YOU IS BORN THIS DAY IN THE CITY OF DAVID A SAVIOUR", "LUKE 2:11"},
        {"gal", "FOLLOW ME, AND I WILL MAKE YOU FISHERS OF MEN", "MATTHEW 4:19"},
        {"jeru", "BLESSED IS HE THAT COMETH IN THE NAME OF THE LORD", "MATTHEW 21:9"},
        {"tomb", "HE IS NOT HERE: FOR HE IS RISEN, AS HE SAID", "MATTHEW 28:6"},
        {"heaven", "BEHOLD, I MAKE ALL THINGS NEW", "REVELATION 21:5"},
        {"eden", "AND THE LORD GOD PLANTED A GARDEN EASTWARD IN EDEN", "GENESIS 2:8"},
        {"jericho", "SHOUT, FOR THE LORD HATH GIVEN YOU THE CITY", "JOSHUA 6:16"},
        {"elah", "THE BATTLE IS THE LORD'S", "1 SAMUEL 17:47"},
        {"test", "LET US RUN WITH PATIENCE THE RACE THAT IS SET BEFORE US", "HEBREWS 12:1"} };
    for (unsigned i = 0; i < sizeof V / sizeof V[0]; i++) if (!strcmp(V[i].p, prefix)) { *ref = V[i].r; return V[i].v; }
    *ref = "HEBREWS 12:1"; return "LET US RUN WITH PATIENCE THE RACE THAT IS SET BEFORE US";
}
static void hud_race(const Pad *pad) {
    Race *r = &g_race; Kart *k = &r->k[r->player]; char buf[64];
    int lap = k->lap < 1 ? 1 : (k->lap > g_track.laps ? g_track.laps : k->lap);
    snprintf(buf, sizeof buf, "LAP %d/%d", lap, g_track.laps); batch_text(&g_b, buf, 40, 30, 36, 1, 1, 1, 1);
    fmt_time(buf, sizeof buf, k->lap >= 1 ? k->lap_time : 0.0f); batch_text(&g_b, buf, 40, 72, 28, 1, 1, 0.6f, 1);
    if (k->best_lap > 0) { char tb[32]; fmt_time(tb, sizeof tb, k->best_lap); snprintf(buf, sizeof buf, "BEST %s", tb); batch_text(&g_b, buf, 40, 104, 20, 0.8f, 0.9f, 1, 1); }
    if (g_mode == MODE_TT) { fmt_time(buf, sizeof buf, k->total_time); batch_text(&g_b, buf, 1240 - text_width(buf, 40), 30, 40, 1, 0.9f, 0.3f, 1);
        if (g_ghost_n > 1) batch_text(&g_b, "VS YOUR BEST RUN", 1240 - text_width("VS YOUR BEST RUN", 14), 112, 14, 0.7f, 0.9f, 1, 0.9f);
        if (g_best_time[g_track_idx] > 0) { char tb[32]; fmt_time(tb, sizeof tb, g_best_time[g_track_idx]); snprintf(buf, sizeof buf, "RECORD %s", tb); batch_text(&g_b, buf, 1240 - text_width(buf, 20), 78, 20, 0.8f, 0.9f, 1, 1); } }
    else {
        int mypos = 0; for (int p = 0; p < r->nk; p++) if (r->order[p] == r->player) mypos = p + 1;
        if (k->finished && k->place) mypos = k->place;
        batch_text(&g_b, place_name(mypos), 1240 - text_width(place_name(mypos), 64), 24, 64, 1, 0.9f, 0.3f, 1);
    }
    snprintf(buf, sizeof buf, "%3.0f KM/H", fabsf(k->speed) * 3.6f); batch_text(&g_b, buf, 1240 - text_width(buf, 24), 96, 24, 1, 1, 1, 1);
    if (g_track.ncoin) { float cy2 = r->jericho ? 222 : 262, pop = 1.0f + 0.6f * fmaxf(g_coin_pop[0], 0) / 0.5f, cs2 = 22 * pop;   /* the coin counter grows for a moment on each pickup */
        batch_rect(&g_b, 44 - 1, cy2 - 1, cs2 + 2, cs2 + 2, 0.45f, 0.3f, 0.05f, 1); batch_rect(&g_b, 44, cy2, cs2, cs2, 1, 0.82f, 0.25f, 1); batch_rect(&g_b, 44 + cs2 * 0.25f, cy2 + cs2 * 0.2f, cs2 * 0.2f, cs2 * 0.6f, 1, 0.97f, 0.7f, 1);
        snprintf(buf, sizeof buf, k->coins >= 10 ? "x%d MAX" : "x%d", k->coins); batch_text(&g_b, buf, 44 + cs2 + 8, cy2 + 1, 22 * pop, 1, 0.9f, 0.5f, 1); }
    if (r->jericho) {   /* the shofars held, and the great shout on the last lap */
        int n = r->shofars[r->player], last = k->lap >= g_track.laps;
        batch_rect(&g_b, 40, 140, 150, 60, 0.05f, 0.1f, 0.25f, 0.7f); batch_rect(&g_b, 43, 143, 144, 54, last ? 0.5f : 0.15f, last ? 0.3f : 0.3f, 0.1f, 0.7f);
        for (int i = 0; i < 6; i++) batch_rect(&g_b, 50 + i * 22, 150, 16, 16, i < n ? 1.0f : 0.3f, i < n ? 0.8f : 0.33f, i < n ? 0.3f : 0.4f, 1);
        if (last) batch_text(&g_b, n > 0 ? "ZL: SHOUT!" : "SHOUTED!", 115 - text_width(n > 0 ? "ZL: SHOUT!" : "SHOUTED!", 16) / 2, 172, 16, 1, 0.9f, 0.4f, (int)(g_time * 4) & 1 || n == 0 ? 1.0f : 0.6f);
        else { snprintf(buf, sizeof buf, "SHOFAR x%d", n); batch_text(&g_b, buf, 115 - text_width(buf, 16) / 2, 172, 16, 0.9f, 0.95f, 1, 1); }
        batch_rect(&g_b, 40, 206, 150, 8, 0.1f, 0.1f, 0.1f, 0.6f); batch_rect(&g_b, 40, 206, 150 * r->wall_fall, 8, 0.85f, 0.6f, 0.3f, 1);   /* how far the walls have come down */
    }
    else if (g_mode != MODE_TT) hud_item_window(0, 44, 118, 104, 1);
    if (k->drifting) { float c = clampf(k->drift_charge / 1.6f, 0, 1); batch_rect(&g_b, 1040, 130, 200, 14, 0.1f, 0.1f, 0.1f, 0.6f);
        batch_rect(&g_b, 1040, 130, 200 * c, 14, c >= 1 ? 1.0f : 0.3f, c >= 0.5f ? 0.9f : 0.4f, 0.2f, 1); }
    { float mx = 1070, mz = 560;
      batch_rect(&g_b, mx - 105, mz - 105, 210, 210, 0.0f, 0.0f, 0.0f, 0.35f);
      for (int i = 0; i < g_track.n; i += 2) { TrackSample *s = &g_track.s[i];
          batch_rect(&g_b, mx + (s->x - map_x0) * map_scale - 2, mz - (s->z - map_z0) * map_scale - 2, 4, 4, 0.9f, 0.9f, 0.9f, 0.8f); }
      for (int p = r->nk - 1; p >= 0; p--) { int i = r->order[p]; Kart *q = &r->k[i]; const Character *c = &CHARACTERS[r->character[i]]; float sz = i == r->player ? 12 : 8;
          batch_rect(&g_b, mx + (q->x - map_x0) * map_scale - sz / 2, mz - (q->z - map_z0) * map_scale - sz / 2, sz, sz, c->r, c->g, c->b, 1);
          if (i == r->player) batch_rect(&g_b, mx + (q->x - map_x0) * map_scale - 3, mz - (q->z - map_z0) * map_scale - 3, 6, 6, 1, 1, 1, 1); } }
    if (r->state == RACE_COUNTDOWN) { text_c(g_track.name, 180, 34, 1, 1, 1, 1); int n = (int)ceilf(r->cd - 0.5f); if (n >= 1 && n <= 3) { snprintf(buf, sizeof buf, "%d", n); text_c(buf, 250, 110, 1, 0.9f, 0.3f, 1); }
        if (!r->jericho) { const char *ref, *v = road_verse(g_def->prefix, &ref); float a = clampf(r->cd * 1.2f, 0, 1); text_c(v, 404, text_width(v, 20) > 1100 ? 16 : 20, 1, 1, 1, a); text_c(ref, 434, 15, 1, 0.9f, 0.5f, a); }
        if (r->jericho) { text_c("SEVEN TIMES ROUND THE CITY. A SHOFAR EVERY LAP: ZL BLOWS IT.", 400, 18, 1, 1, 1, 1); text_c("SAVE THEM FOR THE GREAT SHOUT ON THE SEVENTH LAP!", 426, 18, 1, 0.9f, 0.5f, 1); } }
    if (g_msg_t > 0) { float sz = text_width(g_msg, 60) > 1100 ? 44 : 60; text_c(g_msg, 260, sz, 1, 0.95f, 0.4f, 1); if (g_msg2[0]) text_c(g_msg2, 330, 22, 0.9f, 0.95f, 1, 1); }
    if (k->wrongway_t > WRONGWAY_DELAY && ((int)(g_time * 4) & 1)) text_c("WRONG WAY", 330, 56, 1, 0.3f, 0.2f, 1);
    if (g_demo && !g_attract) text_c("DEMO - MOVE THE STICK TO DRIVE", 680, 20, 1, 1, 1, 0.8f);
    (void)pad;
}

// ------------------------------------------------------------------ screens
static void hud_title(void) {
    float u = g_screen_t < 0.7f ? g_screen_t / 0.7f : 1.0f; float ease = 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u);   /* the logo settles in */
    float sc = 1.6f - 0.6f * ease, al = ease;
    batch_text(&g_b, "VERSION " KK_VERSION, 20, 690, 11, 1, 1, 1, 0.5f);
    if (tx_title_bg) return;   /* Sean's whole title picture is drawn instead (it carries its own "press any button") */
    if (tx_title) {   /* Sean's logo is drawn separately (draw_title_logo); the words go under it */
        float y = 60 + g_title_h + 10;
        checker_band(y, 18); text_c("ROAD TO GLORY", y + 32, 40, 1, 0.85f, 0.3f, al); text_c("A RACE THROUGH THE STORY OF JESUS", y + 84, 18, 0.9f, 0.95f, 1, al); checker_band(y + 112, 18);
        if ((int)(g_time * 2) & 1 && u >= 1) text_c("PRESS A", 640, 32, 1, 1, 1, 1);
        return; }
    checker_band(150, 24);
    panel(240, 190, 800, 230);
    text_c("KINGDOM KART", 258 - 43 * sc, 86 * sc, 1, 0.85f, 0.3f, al);
    text_c("ROAD TO GLORY", 315, 44, 1, 1, 1, al);
    text_c("A RACE THROUGH THE STORY OF JESUS", 372, 18, 0.8f, 0.9f, 1, al);
    checker_band(430, 24);
    if ((int)(g_time * 2) & 1 && u >= 1) text_c("PRESS A", 520, 36, 1, 1, 1, 1);
}
/* the top three on stands (results of a race) */
static void hud_podium(void) {
    Race *r = &g_race; if (r->nk < 3) return;
    int rows[NKARTS]; for (int p = 0; p < r->nk; p++) rows[p] = r->order[p];
    for (int a2 = 1; a2 < r->nk; a2++) { int v = rows[a2], j = a2 - 1; while (j >= 0 && r->k[rows[j]].place > r->k[v].place) { rows[j + 1] = rows[j]; j--; } rows[j + 1] = v; }
    static const float sx[3] = {215, 95, 335}, sh[3] = {110, 75, 50}; static const float col[3][3] = {{1, 0.85f, 0.3f}, {0.8f, 0.82f, 0.88f}, {0.8f, 0.5f, 0.25f}};
    float base = 600;
    for (int p = 0; p < 3; p++) { int i = rows[p]; const Character *c = &CHARACTERS[r->character[i]];
        batch_rect(&g_b, sx[p] - 55, base - sh[p], 110, sh[p], col[p][0] * 0.6f, col[p][1] * 0.6f, col[p][2] * 0.6f, 1);
        batch_rect(&g_b, sx[p] - 55, base - sh[p], 110, 8, col[p][0], col[p][1], col[p][2], 1);
        char n[4]; snprintf(n, sizeof n, "%d", p + 1); batch_text(&g_b, n, sx[p] - text_width(n, 40) / 2, base - sh[p] + 22, 40, col[p][0], col[p][1], col[p][2], 1);
        batch_text(&g_b, c->short_name, sx[p] - text_width(c->short_name, 16) / 2, base + 8, 16, 1, 1, 1, 1); }
}
static void draw_podium_karts(void) {   /* the kart pictures stand on the podium; drawn after the HUD batch with their own textures */
    Race *r = &g_race; if (r->nk < 3) return;
    int rows[NKARTS]; for (int p = 0; p < r->nk; p++) rows[p] = r->order[p];
    for (int a2 = 1; a2 < r->nk; a2++) { int v = rows[a2], j = a2 - 1; while (j >= 0 && r->k[rows[j]].place > r->k[v].place) { rows[j + 1] = rows[j]; j--; } rows[j + 1] = v; }
    static Mesh *pm[3] = {0}; Mat4 id = m4_identity(); static const float sx[3] = {215, 95, 335}, sh[3] = {110, 75, 50};
    for (int p = 0; p < 3; p++) { int ch = r->character[rows[p]]; if (!tx_kart[ch]) continue; if (!pm[p]) pm[p] = gfx_mesh_dynamic(6);
        float w = p == 0 ? 150 : 110, h = w / g_kart_aspect[ch];
        batch_clear(&g_b); batch_rect_uv(&g_b, sx[p] - w / 2, 600 - sh[p] - h + 6, w, h, 0, 0, 1, 1, 1, 1, 1, 1);
        gfx_mesh_update(pm[p], g_b.v, g_b.n); gfx_bind(tx_kart[ch]); gfx_draw(pm[p], &id, 1, 1, 1, 1); }
}
static void hud_main(void) {
    panel(360, 150, 560, 420);
    text_c("MAIN MENU", 170, 40, 1, 0.85f, 0.3f, 1);
    static const char *dn[3] = {"EASY", "NORMAL", "HARD"}; char buf[64];
    menu_item("GRAND PRIX", 212, g_menu_sel == 0, 30);
    menu_item("SINGLE RACE", 254, g_menu_sel == 1, 30);
    menu_item("TIME TRIAL", 296, g_menu_sel == 2, 30);
    menu_item("DAVID VS GOLIATH", 338, g_menu_sel == 3, 30);   /* open from the start: it is one of the main stories */
    menu_item("MULTIPLAYER 2-4", 380, g_menu_sel == 4, 30);
    menu_item("THE WALLS OF JERICHO", 422, g_menu_sel == 5, 30);
    snprintf(buf, sizeof buf, "DIFFICULTY: %s", dn[g_difficulty]); menu_item(buf, 466, g_menu_sel == 6, 24);
    menu_item("OPTIONS AND RECORDS", 500, g_menu_sel == 7, 22);
    menu_item("CREDITS", 532, g_menu_sel == 8, 22);
    text_c("A: CHOOSE   B: BACK   LEFT/RIGHT: CHANGE", 560, 14, 0.8f, 0.85f, 1, 1);
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
    volume_row("MUSIC VOLUME", g_save.music_vol, 128, g_opt_sel == 0);
    volume_row("SOUND VOLUME", g_save.sfx_vol, 164, g_opt_sel == 1);
    { float x = 400, y = 200; if (g_opt_sel == 2) batch_rect(&g_b, x - 16, y - 6, 480, 36, 1, 0.85f, 0.3f, 0.25f);
      batch_text(&g_b, "CONTROLLER RUMBLE", x, y, 24, g_opt_sel == 2 ? 1 : 0.9f, g_opt_sel == 2 ? 0.85f : 0.95f, g_opt_sel == 2 ? 0.3f : 1, 1);
      batch_text(&g_b, g_save.rumble ? "ON" : "OFF", x + 270, y, 24, g_save.rumble ? 1 : 0.6f, g_save.rumble ? 0.85f : 0.65f, g_save.rumble ? 0.3f : 0.75f, 1); }
    { float x = 400, y = 236; if (g_opt_sel == 3) batch_rect(&g_b, x - 16, y - 6, 480, 30, 1, 0.5f, 0.3f, 0.25f);
      batch_text(&g_b, "ERASE ALL PROGRESS", x, y, 18, g_opt_sel == 3 ? 1 : 0.8f, g_opt_sel == 3 ? 0.6f : 0.75f, g_opt_sel == 3 ? 0.4f : 0.8f, 1);
      if (g_opt_sel == 3) { batch_text(&g_b, "HOLD A", x + 270, y, 18, 1, 0.9f, 0.5f, 1); if (g_erase_hold > 0) batch_rect(&g_b, x + 350, y + 4, 100 * clampf(g_erase_hold / 2.0f, 0, 1), 12, 1, 0.4f, 0.3f, 1); } }
    text_c("LEFT/RIGHT: CHANGE   B: BACK", 268, 14, 0.8f, 0.85f, 1, 1);
    batch_text(&g_b, "RECORDS", 280, 292, 22, 1, 0.85f, 0.3f, 1);
    batch_text(&g_b, "ROAD", 280, 322, 14, 0.8f, 0.85f, 1, 1); batch_text(&g_b, "BEST TIME", 720, 322, 14, 0.8f, 0.85f, 1, 1); batch_text(&g_b, "TARGET", 900, 322, 14, 0.8f, 0.85f, 1, 1);
    for (int d = 0; d < NUM_PLAY_TRACKS && d < SAVE_MAX_ROADS; d++) { float y = 344 + d * 27;
        batch_text(&g_b, TRACK_DEFS[d].name, 280, y, 18, 1, 1, 1, 1);
        if (g_save.best_ms[d] > 0) { fmt_time(buf, sizeof buf, g_save.best_ms[d] / 1000.0f); int beat = save_par_time(d) > 0 && g_save.best_ms[d] <= (int)(save_par_time(d) * 1000); batch_text(&g_b, buf, 720, y, 18, 1, beat ? 0.85f : 1, beat ? 0.3f : 1, 1); }
        else batch_text(&g_b, "--:--.--", 720, y, 18, 0.6f, 0.65f, 0.75f, 1);
        if (save_par_time(d) > 0) { fmt_time(buf, sizeof buf, save_par_time(d)); batch_text(&g_b, buf, 900, y, 18, 0.8f, 0.85f, 1, 1); } }
    float y = 344 + NUM_PLAY_TRACKS * 27 + 10;
    snprintf(buf, sizeof buf, "RACES FINISHED %d   RACES WON %d", g_save.races, g_save.wins); batch_text(&g_b, buf, 280, y, 18, 1, 1, 1, 1);
    snprintf(buf, sizeof buf, "GRAND PRIX WON: EASY %d  NORMAL %d  HARD %d", g_save.gp_wins[0], g_save.gp_wins[1], g_save.gp_wins[2]); batch_text(&g_b, buf, 280, y + 30, 18, 1, 1, 1, 1);
    snprintf(buf, sizeof buf, "GOLIATH: %d FIGHTS, %d WON", g_save.battles, g_save.battle_wins); batch_text(&g_b, buf, 280, y + 60, 18, 1, 1, 1, 1);
    if (g_save.jericho_ms) { char tb[32]; fmt_time(tb, sizeof tb, g_save.jericho_ms / 1000.0f); snprintf(buf, sizeof buf, "JERICHO: %d WON, BEST %s", g_save.jericho_wins, tb); } else snprintf(buf, sizeof buf, "JERICHO: %d WON", g_save.jericho_wins);
    batch_text(&g_b, buf, 720, y + 60, 18, 1, 1, 1, 1);
    int n = 0; for (int b = 0; b < UNLOCK_COUNT; b++) if (g_save.unlocks & (1u << b)) n++;
    snprintf(buf, sizeof buf, "UNLOCKED %d OF %d", n, UNLOCK_COUNT); batch_text(&g_b, buf, 280, y + 90, 18, 1, 0.85f, 0.3f, 1);
}
/* the credits roll up over the demo race; A or B skips */
typedef struct { const char *text; float size; int gold; float gap; } CreditLine;
static const CreditLine CREDITS[] = {
    {"KINGDOM KART", 56, 1, 0}, {"ROAD TO GLORY", 36, 1, 0}, {"A RACE THROUGH THE STORY OF JESUS", 18, 0, 60},
    {"CREATED BY", 18, 1, 0}, {"SEAN", 34, 0, 60},
    {"CHARACTERS, KARTS, ROADS AND ART", 18, 1, 0}, {"SEAN", 34, 0, 60},
    {"MUSIC, SOUND AND VOICES", 18, 1, 0}, {"SEAN", 34, 0, 30},
    {"RECORDED SOUND EFFECTS", 18, 1, 0}, {"JOSEPH SARDIN - BIGSOUNDBANK.COM", 22, 0, 0}, {"KENNEY - KENNEY.NL", 22, 0, 0}, {"ADDITIONAL VOICES AND SOUNDS - ELEVENLABS", 18, 0, 60},
    {"PROGRAMMING", 18, 1, 0}, {"CLAUDE", 34, 0, 60},
    {"THE ROADS", 18, 1, 0}, {"BETHLEHEM ROAD - LUKE 2", 24, 0, 0}, {"SEA OF GALILEE - MATTHEW 4 AND 14", 24, 0, 0},
    {"JERUSALEM ROAD - MATTHEW 21", 24, 0, 0}, {"SUNRISE AT THE EMPTY TOMB - MATTHEW 28", 24, 0, 0},
    {"ROAD TO HEAVEN - REVELATION 21 AND 22", 24, 0, 0}, {"THE GARDEN OF EDEN - GENESIS 2", 24, 0, 0}, {"THE WALLS OF JERICHO - JOSHUA 6", 24, 0, 0}, {"THE VALLEY OF ELAH - 1 SAMUEL 17", 24, 0, 60},
    {"THE RACERS", 18, 1, 0}, {"JESUS - MOSES - DAVID - ESTHER", 24, 0, 0}, {"PETER - MARY - DANIEL", 24, 0, 0},
    {"SAMSON - NOAH - JOSEPH - JONAH - JOHN THE BAPTIST", 24, 0, 0}, {"MICHAEL AND GABRIEL, THE ARCHANGELS", 24, 0, 60},
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
static void hud_char(int multi) {
    text_c(multi ? "EVERYONE: CHOOSE YOUR CHARACTER" : "CHOOSE YOUR CHARACTER", 22, 36, 1, 0.85f, 0.3f, 1);
    for (int i = 0; i < NCHARS; i++) {
        const Character *c = &CHARACTERS[i]; int sel = !multi && i == g_player_char, open = char_unlocked(i);
        float x, y, w = 225, h = 186; char_card_pos(i, &x, &y);
        if (sel) batch_rect(&g_b, x - 6, y - 6, w + 12, h + 12, 1, 0.85f, 0.3f, 1);
        if (multi) { int n = 0; for (int p = 0; p < g_njoined; p++) if (g_pchar[p] == i) {   /* each player's colored frame, nested when they sit on the same card */
                float in = 6.0f - n * 3.0f; batch_rect(&g_b, x - in, y - in, w + 2 * in, 3, PLAYER_COL[p][0], PLAYER_COL[p][1], PLAYER_COL[p][2], 1); batch_rect(&g_b, x - in, y + h + in - 3, w + 2 * in, 3, PLAYER_COL[p][0], PLAYER_COL[p][1], PLAYER_COL[p][2], 1);
                batch_rect(&g_b, x - in, y - in, 3, h + 2 * in, PLAYER_COL[p][0], PLAYER_COL[p][1], PLAYER_COL[p][2], 1); batch_rect(&g_b, x + w + in - 3, y - in, 3, h + 2 * in, PLAYER_COL[p][0], PLAYER_COL[p][1], PLAYER_COL[p][2], 1); n++; } }
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
    if (multi) {   /* the players' tags on the cards they sit on */
        for (int i = 0; i < NCHARS; i++) { float x, y; char_card_pos(i, &x, &y); int n = 0;
            for (int p = 0; p < g_njoined; p++) if (g_pchar[p] == i) { char tag[12]; snprintf(tag, sizeof tag, g_plock[p] ? "P%d OK" : "P%d", p + 1);
                float tw = text_width(tag, 14) + 10; batch_rect(&g_b, x + 106 + n * 58, y + 58, tw, 20, PLAYER_COL[p][0], PLAYER_COL[p][1], PLAYER_COL[p][2], 1); batch_text(&g_b, tag, x + 111 + n * 58, y + 61, 14, 0.05f, 0.05f, 0.1f, 1); n++; } }
        int all = 1; for (int p = 0; p < g_njoined; p++) if (!g_plock[p]) all = 0;
        text_c(all ? "EVERYONE IS READY!" : "A: LOCK IN   B: CHANGE   (P1: B TO GO BACK)", 692, 14, all ? 1 : 0.8f, all ? 0.9f : 0.85f, all ? 0.4f : 1, 1); }
    else text_c("A: CHOOSE   B: BACK", 692, 14, 0.8f, 0.85f, 1, 1);
}
/* the join screen: every controller that presses A gets a seat */
static void hud_join(const Pad *pads, int npads) {
    panel(190, 80, 900, 560);
    text_c("MULTIPLAYER", 100, 40, 1, 0.85f, 0.3f, 1);
    text_c("PRESS A ON EACH CONTROLLER TO JOIN (2 TO 4 PLAYERS)", 150, 18, 0.9f, 0.95f, 1, 1);
    static const char *kinds[] = {"GAMEPAD", "WII CONTROLLER 1", "WII CONTROLLER 2", "WII CONTROLLER 3", "WII CONTROLLER 4"};
    for (int sl = 0; sl < KK_PADS && sl < npads; sl++) { float y = 200 + sl * 62; int here = pads[sl].source != 0, j = g_joined[sl];
        const char *what = pads[sl].source & 2 ? "PRO CONTROLLER" : (pads[sl].source & 8 ? "CLASSIC CONTROLLER" : (pads[sl].source & 16 ? "REMOTE + NUNCHUK" : (pads[sl].source & 4 ? "WII REMOTE (SIDEWAYS)" : (pads[sl].source & 1 ? "GAMEPAD" : "NOT CONNECTED"))));
        if (j) batch_rect(&g_b, 220, y - 8, 840, 50, PLAYER_COL[j - 1][0], PLAYER_COL[j - 1][1], PLAYER_COL[j - 1][2], 0.3f);
        batch_text(&g_b, kinds[sl], 240, y + 2, 18, here ? 1 : 0.5f, here ? 1 : 0.55f, here ? 1 : 0.65f, 1);
        batch_text(&g_b, what, 600, y + 5, 13, 0.8f, 0.85f, 1, here ? 1 : 0.6f);
        if (j) { char tag[16]; snprintf(tag, sizeof tag, "PLAYER %d", j); batch_text(&g_b, tag, 880, y, 22, PLAYER_COL[j - 1][0], PLAYER_COL[j - 1][1], PLAYER_COL[j - 1][2], 1); }
        else if (here) batch_text(&g_b, (int)(g_time * 2) & 1 ? "PRESS A" : "", 880, y, 22, 1, 1, 1, 1); }
    if (g_njoined >= 2) text_c("PRESS + TO START", 540, 26, 1, 0.9f, 0.4f, (int)(g_time * 2) & 1 ? 1.0f : 0.7f);
    else text_c("WAITING FOR AT LEAST 2 PLAYERS", 540, 18, 0.8f, 0.85f, 1, 1);
    text_c("B: LEAVE   -   WII REMOTE SIDEWAYS: 2 GAS, 1 BRAKE, B DRIFT, A BLESSING", 600, 14, 0.8f, 0.85f, 1, 1);
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
/* the road cards across the screen: the six roads, plus the Walls of Jericho in multiplayer */
static int ncards(void) { return g_mode == MODE_VS ? NUM_PLAY_TRACKS + 1 : NUM_PLAY_TRACKS; }
static int card_track(int d) { return d < NUM_PLAY_TRACKS ? d : JERICHO_TRACK; }
static int track_card(int t) { return t < NUM_PLAY_TRACKS ? t : NUM_PLAY_TRACKS; }
#define CARD_STEP (1200.0f / ncards())
#define CARD_W (CARD_STEP - 10.0f)
#define CARD_IN (CARD_W - 24.0f)
static void hud_track(void) {
    text_c("CHOOSE A ROAD", 30, 40, 1, 0.85f, 0.3f, 1);
    for (int c2 = 0; c2 < ncards(); c2++) { int d = card_track(c2);
        int sel = d == g_track_idx, open = track_unlocked(d); float x = 40 + c2 * CARD_STEP, y = 110, w = CARD_W, h = 400, iw = CARD_IN;
        if (sel) batch_rect(&g_b, x - 8, y - 8, w + 16, h + 16, 1, 0.85f, 0.3f, 1);
        batch_rect(&g_b, x, y, w, h, 0.08f, 0.14f, 0.36f, 0.95f);
        const TrackDef *td = &TRACK_DEFS[d]; char buf[48]; float ty = y + 12 + iw + 14;
        if (!open) {
            batch_rect(&g_b, x + 12, y + 12, iw, iw, 0.2f, 0.22f, 0.3f, 1);
            batch_text(&g_b, "?", x + 12 + iw / 2 - text_width("?", 90) / 2, y + 12 + iw / 2 - 45, 90, 0.5f, 0.55f, 0.7f, 1);
            batch_text(&g_b, "BONUS ROAD", x + 12, ty, 14, 1, 0.85f, 0.3f, 1);
            batch_text(&g_b, "WIN THE", x + 12, ty + 30, 14, 0.9f, 0.9f, 1, 1); batch_text(&g_b, "GRAND PRIX", x + 12, ty + 50, 14, 0.9f, 0.9f, 1, 1); batch_text(&g_b, "ON NORMAL", x + 12, ty + 70, 14, 0.9f, 0.9f, 1, 1);
            continue;
        }
        batch_rect(&g_b, x + 12, y + 12, iw, iw, td->sky_r, td->sky_g, td->sky_b, 1);
        if (tx_card[d]) draw_preview(&g_preview[d], x + 12 + iw / 2, y + 12 + iw * 0.77f, iw * 0.4f, 0.95f, 0.95f, 0.95f);   /* the picture goes on top (drawn after the HUD batch), the map small under it */
        else draw_preview(&g_preview[d], x + 12 + iw / 2, y + 12 + iw / 2, iw * 0.875f, 0.95f, 0.95f, 0.95f);
        /* the name, split onto two lines when long */
        const char *nm = td->name; size_t len = strlen(nm);
        size_t cut = 0;    /* the space nearest the middle of the name, if any */
        for (size_t q = 0; q < len; q++) if (nm[q] == ' ' && (cut == 0 || labs((long)q - (long)len / 2) < labs((long)cut - (long)len / 2))) cut = q;
        float fit = iw * 0.78f;   /* the bold font runs wider than measured */
        if (text_width(nm, 18) > fit && cut > 0) { char l1[32], l2[32]; snprintf(l1, sizeof l1, "%.*s", (int)cut, nm); snprintf(l2, sizeof l2, "%s", nm + cut + 1);
            float sz = fmaxf(text_width(l1, 18), text_width(l2, 18)) > fit ? 14 : 18;
            batch_text(&g_b, l1, x + 12, ty, sz, 1, 1, 1, 1); batch_text(&g_b, l2, x + 12, ty + 23, sz, 1, 1, 1, 1); }
        else batch_text(&g_b, nm, x + 12, ty, text_width(nm, 18) > fit ? 14 : 18, 1, 1, 1, 1);
        snprintf(buf, sizeof buf, td->laps == 1 ? "ONE RUN TO THE TOP" : (td->bonus == 3 ? "7 LAPS - SHOFARS!" : "%d LAPS"), td->laps); batch_text(&g_b, buf, x + 12, ty + 55, text_width(buf, 14) > fit ? 11 : 14, 0.8f, 0.9f, 1, 1);
        if (g_best_time[d] > 0) { char tb[32]; fmt_time(tb, sizeof tb, g_best_time[d]); snprintf(buf, sizeof buf, "RECORD %s", tb); batch_text(&g_b, buf, x + 12, ty + 80, 14, 1, 0.9f, 0.5f, 1); }
        if (save_par_time(d) > 0) { char tb[32]; fmt_time(tb, sizeof tb, save_par_time(d)); snprintf(buf, sizeof buf, "TARGET %s", tb); batch_text(&g_b, buf, x + 12, ty + 103, 14, 0.7f, 0.95f, 0.8f, 1); }
        if (td->bonus) batch_text(&g_b, "BONUS ROAD", x + 12, ty + 135, 14, 1, 0.85f, 0.3f, 1);
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
    text_c("LEFT/RIGHT: VOLUME", 500, 14, 0.8f, 0.85f, 1, 1);
    text_c(g_race.jericho ? "A GAS   B BRAKE   ZR DRIFT   ZL SHOFAR" : "A GAS   B BRAKE   ZR HOLD TO DRIFT   ZL BLESSING", 526, 13, 0.7f, 0.8f, 1, 1);
    if (g_pause_reason[0]) text_c(g_pause_reason, 150, 18, 1, 0.5f, 0.4f, 1);
}
static void hud_battle(void) {
    Battle *b = &g_battle; char buf[64];
    text_c("THE VALLEY OF ELAH", 60, 16, 1, 0.85f, 0.3f, 1);
    batch_text(&g_b, "DAVID", 40, 30, 22, 1, 1, 1, 1);
    for (int i = 0; i < BATTLE_LIVES; i++) batch_rect(&g_b, 40 + i * 34, 60, 28, 24, i < BATTLE_LIVES - b->hits_on_david ? 0.95f : 0.25f, 0.2f, 0.25f, 1);
    snprintf(buf, sizeof buf, "STONES x%d", b->stones_held); batch_text(&g_b, buf, 40, 95, 18, 0.9f, 0.9f, 0.85f, 1);
    batch_text(&g_b, "GOLIATH", 1240 - text_width("GOLIATH", 22), 30, 22, 1, 1, 1, 1);
    for (int i = 0; i < BATTLE_WIN_HITS; i++) batch_rect(&g_b, 1240 - (i + 1) * 36, 60, 30, 24, 0.9f, i < BATTLE_WIN_HITS - b->hits_on_goliath ? 0.65f : 0.15f, 0.15f, 1);
    { char tb[32]; fmt_time(tb, sizeof tb, BATTLE_TIME_LIMIT - b->race.race_t > 0 ? BATTLE_TIME_LIMIT - b->race.race_t : 0); batch_text(&g_b, tb, 640 - text_width(tb, 26) / 2, 30, 26, 1, 0.9f, 0.5f, 1); }
    if (b->race.state == RACE_COUNTDOWN) { int n = (int)ceilf(b->race.cd - 0.5f); if (n >= 1 && n <= 3) { snprintf(buf, sizeof buf, "%d", n); text_c(buf, 250, 110, 1, 0.9f, 0.3f, 1); } text_c("PICK UP STONES, PRESS ZL TO SLING", 180, 18, 1, 1, 1, 1); text_c("HIS SHIELD-BEARER GUARDS HIS FRONT AS HE WALKS: SLING FROM THE SIDE, OR WHEN HE STOPS", 204, 15, 1, 0.9f, 0.5f, 1); }
    if (g_msg_t > 0) { float sz = text_width(g_msg, 60) > 1100 ? 44 : 60; text_c(g_msg, 260, sz, 1, 0.95f, 0.4f, 1); if (g_msg2[0]) text_c(g_msg2, 330, 22, 0.9f, 0.95f, 1, 1); }
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
    if (b->blocks) { snprintf(buf, sizeof buf, "STONES THE SHIELD-BEARER CAUGHT: %d", b->blocks); text_c(buf, 370, 18, 0.8f, 0.9f, 1, 1); }
    { char tb[32]; fmt_time(tb, sizeof tb, b->race.race_t); snprintf(buf, sizeof buf, "TIME %s", tb); text_c(buf, 340, 24, 1, 1, 1, 1); }
    if (b->won) text_c("THE BATTLE IS THE LORD'S", 400, 20, 0.9f, 0.95f, 1, 1);
    text_c("A: FIGHT AGAIN   B: MENU", 500, 20, 0.9f, 0.95f, 1, 1);
    if (g_fresh_unlocks) { float yb = 600; batch_rect(&g_b, 240, yb, 800, 60, 1, 0.85f, 0.3f, 0.95f); batch_rect(&g_b, 246, yb + 6, 788, 48, 0.08f, 0.14f, 0.36f, 1);
        char line[96] = "UNLOCKED: "; int first = 1; for (int q = 0; q < UNLOCK_COUNT; q++) if (g_fresh_unlocks & (1u << q)) { if (!first) strncat(line, ", ", sizeof line - strlen(line) - 1); strncat(line, unlock_name(1 << q), sizeof line - strlen(line) - 1); first = 0; }
        text_c(line, yb + 20, 20, 1, 0.95f, 0.5f, 1); }
}
static void hud_results(void) {
    Race *r = &g_race; char buf[64];
    int podium = g_mode != MODE_TT;
    if (podium) hud_podium();
    panel(podium ? 420 : 290, 90, podium ? 780 : 700, 540);
    text_c(g_mode == MODE_TT ? "TIME TRIAL" : (r->jericho ? "THE WALLS CAME DOWN" : "RESULTS"), 110, 44, 1, 0.85f, 0.3f, 1);
    int rows[NKARTS]; for (int p = 0; p < r->nk; p++) rows[p] = r->order[p];
    for (int a2 = 1; a2 < r->nk; a2++) { int v = rows[a2], j = a2 - 1; while (j >= 0 && r->k[rows[j]].place > r->k[v].place) { rows[j + 1] = rows[j]; j--; } rows[j + 1] = v; }
    for (int p = 0; p < r->nk; p++) { int i = rows[p]; Kart *q = &r->k[i]; const Character *c = &CHARACTERS[r->character[i]];
        float y = 170 + p * 50, ox = podium ? 130 : 0; int hp = race_human_index(r, i), me = hp >= 0;
        if (me) batch_rect(&g_b, 310 + ox, y - 6, 660, 44, PLAYER_COL[hp][0], PLAYER_COL[hp][1], PLAYER_COL[hp][2], 0.25f);
        if (me && g_nplayers > 1) { char tag[8]; snprintf(tag, sizeof tag, "P%d", hp + 1); batch_text(&g_b, tag, 1100 + ox, y + 4, 22, PLAYER_COL[hp][0], PLAYER_COL[hp][1], PLAYER_COL[hp][2], 1); }
        batch_text(&g_b, place_name(q->place ? q->place : p + 1), 330 + ox, y, 30, 1, 1, 1, 1);
        batch_rect(&g_b, 440 + ox, y + 2, 26, 26, c->r, c->g, c->b, 1);
        batch_text(&g_b, c->short_name, 480 + ox, y, 30, me ? 1.0f : 0.9f, me ? 0.95f : 0.9f, me ? 0.5f : 0.9f, 1);
        if (q->lap > g_track.laps) fmt_time(buf, sizeof buf, q->total_time); else snprintf(buf, sizeof buf, "--:--.--");
        batch_text(&g_b, buf, 720 + ox, y, 30, 1, 1, 1, 1);
        if (g_mode == MODE_GP) { int pl = q->place ? q->place : p + 1; snprintf(buf, sizeof buf, "+%d", GP_POINTS[pl - 1]); batch_text(&g_b, buf, 900 + ox, y, 30, 1, 0.9f, 0.4f, 1); }
        if (r->given[i] > 0) { snprintf(buf, sizeof buf, "%d", r->given[i]); batch_rect(&g_b, 1000 + ox + (g_mode == MODE_GP ? 60 : 0), y + 6, 14, 14, 1, 0.85f, 0.3f, 1); batch_text(&g_b, buf, 1020 + ox + (g_mode == MODE_GP ? 60 : 0), y + 2, 22, 1, 0.9f, 0.5f, 1); } }
    if (r->jericho) text_c("BY FAITH THE WALLS OF JERICHO FELL - HEBREWS 11:30", 548, 18, 1, 0.9f, 0.5f, 1);
    else if (g_mode != MODE_TT) {   /* the Most Generous: whoever blessed the others most */
        int best = -1, bg = 0; for (int i = 0; i < r->nk; i++) if (r->given[i] > bg) { bg = r->given[i]; best = i; }
        if (best >= 0) { snprintf(buf, sizeof buf, "MOST GENEROUS: %s (%d BLESSINGS)", CHARACTERS[r->character[best]].short_name, bg); text_c(buf, 548, 20, 1, 0.9f, 0.5f, 1); } }
    if (g_mode == MODE_TT) { Kart *k = &r->k[r->player]; fmt_time(buf, sizeof buf, k->best_lap); char line[96]; snprintf(line, sizeof line, "BEST LAP %s", buf); text_c(line, 240, 28, 0.9f, 0.95f, 1, 1);
        if (g_best_time[g_track_idx] > 0 && k->total_time <= g_best_time[g_track_idx] + 0.001f) text_c("NEW RECORD!", 300, 40, 1, 0.9f, 0.3f, 1); }
    text_c(g_mode == MODE_GP ? "A: NEXT ROAD   (+2 POINTS FOR THE MOST GENEROUS)" : "A: RACE AGAIN   B: MENU", 585, 18, 0.9f, 0.95f, 1, 1);
    if (g_fresh_unlocks) {      /* a banner for what this race unlocked */
        float yb = 640; batch_rect(&g_b, 240, yb, 800, 70, 1, 0.85f, 0.3f, 0.95f); batch_rect(&g_b, 246, yb + 6, 788, 58, 0.08f, 0.14f, 0.36f, 1);
        char line[96] = "UNLOCKED: "; int first = 1;
        for (int b = 0; b < UNLOCK_COUNT; b++) if (g_fresh_unlocks & (1u << b)) { if (!first) strncat(line, ", ", sizeof line - strlen(line) - 1); strncat(line, unlock_name(1 << b), sizeof line - strlen(line) - 1); first = 0; }
        float sz = text_width(line, 24) > 760 ? 16 : 24; text_c(line, yb + 35 - sz / 2, sz, 1, 0.95f, 0.5f, (int)(g_time * 3) & 1 ? 1.0f : 0.8f);
    }
}
static void hud_cup(void) {
    text_c("CHOOSE A CUP", 40, 40, 1, 0.85f, 0.3f, 1);
    for (int c = 0; c < 2; c++) { const Cup *cp = &CUPS[c]; int sel = c == g_cup, open = !cp->needs_heaven || (g_save.unlocks & UNLOCK_HEAVEN);
        float x = 90 + c * 580, y = 120, w = 520, h = 470;
        if (sel) batch_rect(&g_b, x - 8, y - 8, w + 16, h + 16, 1, 0.85f, 0.3f, 1);
        batch_rect(&g_b, x, y, w, h, 0.08f, 0.14f, 0.36f, 0.95f);
        batch_text(&g_b, cp->name, x + 20, y + 20, 30, 1, 0.95f, 0.5f, 1);
        batch_text(&g_b, cp->sub, x + 20, y + 62, 13, 0.8f, 0.9f, 1, 1);
        if (!open) { batch_text(&g_b, "?", x + w / 2 - text_width("?", 110) / 2, y + 140, 110, 0.5f, 0.55f, 0.7f, 1);
            batch_text(&g_b, "LOCKED", x + w / 2 - text_width("LOCKED", 24) / 2, y + 290, 24, 0.8f, 0.8f, 0.9f, 1);
            batch_text(&g_b, "WIN THE GOSPEL CUP ON NORMAL OR HARD", x + w / 2 - text_width("WIN THE GOSPEL CUP ON NORMAL OR HARD", 14) / 2, y + 330, 14, 1, 0.9f, 0.5f, 1); continue; }
        for (int i = 0; i < cp->n; i++) { const TrackDef *td = &TRACK_DEFS[cp->roads[i]]; float ry = y + 100 + i * 56; char nm[8]; snprintf(nm, sizeof nm, "%d.", i + 1);
            batch_rect(&g_b, x + 20, ry, 44, 44, td->sky_r, td->sky_g, td->sky_b, 1); draw_preview(&g_preview[cp->roads[i]], x + 42, ry + 22, 34, 0.95f, 0.95f, 0.95f);
            batch_text(&g_b, nm, x + 76, ry + 10, 20, 1, 0.85f, 0.3f, 1); batch_text(&g_b, td->name, x + 110, ry + 10, 20, 1, 1, 1, 1); }
        { char line[64]; snprintf(line, sizeof line, "%d ROADS   WON: EASY %d  NORMAL %d  HARD %d", cp->n, g_save.gp_wins[0], g_save.gp_wins[1], g_save.gp_wins[2]); if (c == 0) batch_text(&g_b, line, x + 20, y + h - 36, 13, 0.8f, 0.9f, 1, 1); else { snprintf(line, sizeof line, "%d ROADS - THE WHOLE STORY", cp->n); batch_text(&g_b, line, x + 20, y + h - 36, 13, 0.8f, 0.9f, 1, 1); } }
    }
    text_c("A: CHOOSE   B: BACK   LEFT/RIGHT: CHANGE", 650, 16, 0.8f, 0.85f, 1, 1);
}
static void hud_gp(void) {
    panel(290, 80, 700, 560); char buf[64];
    text_c(g_gp_round >= NUM_GP_TRACKS ? (g_cup == 0 ? "GOSPEL CUP CHAMPION" : "GENESIS TO GLORY CHAMPION") : "STANDINGS", 100, g_gp_round >= NUM_GP_TRACKS && g_cup == 1 ? 32 : 40, 1, 0.85f, 0.3f, 1);
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

// ------------------------------------------------------------------ the trophy ceremony at the end of a cup
static float seg_dist(float px, float py, float ax, float ay, float bx, float by) { float dx = bx - ax, dy = by - ay, t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy); t = t < 0 ? 0 : t > 1 ? 1 : t; float ex = px - ax - t * dx, ey = py - ay - t * dy; return sqrtf(ex * ex + ey * ey); }
static void px_qbox(int x, int y, uint32_t *o) {   /* one face of the blessing box: a frosted pane, brighter at the rim, with a smooth white question mark outlined in dark (128 x 128) */
    float fx = (x - 63.5f) / 64.0f, fy = (y - 63.5f) / 64.0f, edge = fmaxf(fabsf(fx), fabsf(fy));
    int a = 70 + (int)(edge * edge * 120.0f), v = 200 + (int)(edge * 55.0f);
    if (edge > 0.93f) { a = 235; v = 255; }   /* a bright border on each pane */
    /* the mark: an arc over the top, a short slant down to the stem, and the dot (distance to the stroke, so the edges are soft) */
    float px = x + 0.5f, py = y + 0.5f, cx = 64, cy = 44, R = 22, half = 8.0f;
    float ang = atan2f(py - cy, px - cx), d;   /* -pi..pi, screen y points down so the top of the circle is -pi/2 */
    float a0 = -3.4907f, a1 = 1.0472f;         /* from 200 degrees round over the top to 60 degrees */
    float aa = ang; if (aa > a1) aa -= 6.2831853f;
    if (aa >= a0) d = fabsf(sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy)) - R);
    else { float e0x = cx + R * cosf(a0), e0y = cy + R * sinf(a0), e1x = cx + R * cosf(a1), e1y = cy + R * sinf(a1);
           d = fminf(sqrtf((px - e0x) * (px - e0x) + (py - e0y) * (py - e0y)), sqrtf((px - e1x) * (px - e1x) + (py - e1y) * (py - e1y))); }
    d = fminf(d, seg_dist(px, py, cx + R * cosf(a1), cy + R * sinf(a1), 64, 80));
    d = fminf(d, seg_dist(px, py, 64, 80, 64, 83));
    float dd = sqrtf((px - 64) * (px - 64) + (py - 108) * (py - 108)) - 1.5f; if (dd < d) d = dd;
    float ink = clampf(half + 0.5f - d, 0, 1), rim = clampf(half + 5.5f - d, 0, 1);
    float br = v + (40 - v) * rim, bg = v + (25 - v) * rim, bb = v + (80 - v) * rim, ba = a + (235 - a) * rim;
    br += (255 - br) * ink; bg += (255 - bg) * ink; bb += (255 - bb) * ink; ba += (255 - ba) * ink;
    *o = rgba((int)br, (int)bg, (int)bb, (int)ba);
}
static void px_trophy(int x, int y, uint32_t *o) {   /* a gold cup with two handles, a stem, a base, and a cross on the bowl (128 x 128) */
    float fx = x + 0.5f, fy = y + 0.5f, hw = -1; int part = 0;
    if (fy >= 14 && fy < 74) { float u = (fy - 14) / 60.0f; hw = 40.0f * sqrtf(fmaxf(0, 1 - u * u)); if (hw < 9) hw = 9; part = 1; }   /* bowl */
    else if (fy >= 74 && fy < 94) { hw = 6 + (fy > 88 ? (fy - 88) * 1.5f : 0); part = 2; }                                       /* stem */
    else if (fy >= 94 && fy < 104) { hw = 24; part = 3; } else if (fy >= 104 && fy < 120) { hw = 32; part = 4; }                   /* base and plinth */
    int in = hw > 0 && fabsf(fx - 64) <= hw;
    if (!in) { for (int sd = -1; sd <= 1; sd += 2) { float cx = 64 + sd * 40, cy = 36, d = sqrtf((fx - cx) * (fx - cx) + (fy - cy) * (fy - cy));   /* handles */
                   if (d > 10 && d < 17 && (fx - cx) * sd > -2) { in = 1; hw = 17; part = 5; } } }
    if (!in) { *o = 0; return; }
    if (fy >= 14 && fy < 18 && part == 1) { *o = rgba(255, 236, 150, 255); return; }                                                /* bright rim */
    float t = part == 5 ? 0 : (fx - 64) / hw;                                                                                         /* -1 left edge .. 1 right edge */
    float shade = 0.62f + 0.38f * cosf(t * 1.4f) + (fabsf(t + 0.45f) < 0.12f ? 0.35f : 0);                                          /* round shading + a highlight */
    if (part == 4) shade *= 0.55f;                                                                                                   /* darker plinth */
    if (part == 1 && fabsf(fx - 64) < 3 && fy > 26 && fy < 58) shade = 1.25f;                                                        /* the cross on the bowl */
    if (part == 1 && fabsf(fy - 36) < 3 && fabsf(fx - 64) < 11) shade = 1.25f;
    float r = 236 * shade, g = 178 * shade, b = 52 * shade; if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    *o = rgba((int)r, (int)g, (int)b, 255);
}
static int cer_order(int *ord) {   /* the field of the cup, best first */
    int nk = g_race.nk; for (int i = 0; i < nk; i++) ord[i] = g_race.character[i];
    for (int a2 = 1; a2 < nk; a2++) { int v = ord[a2], j = a2 - 1; while (j >= 0 && g_gp_points[ord[j]] < g_gp_points[v]) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = v; }
    return nk;
}
static const float CER_X[3] = {640, 400, 880}, CER_H[3] = {150, 104, 70}, CER_T[3] = {2.4f, 1.4f, 0.6f};   /* 1st, 2nd, 3rd: where, how tall, when they arrive */
static float cer_drop(int p) { float u = clampf((g_screen_t - CER_T[p]) / 0.55f, 0, 1); float e = 1 - (1 - u) * (1 - u); return u >= 1 ? 0 : (1 - e) * -620; }
static void hud_trophy(void) {
    float t = g_screen_t; char buf[96]; int ord[NKARTS]; cer_order(ord);
    batch_rect(&g_b, 0, 0, 1280, 720, 0.03f, 0.04f, 0.12f, 0.82f);
    for (int k = 0; k < 9; k++) { float w = 120 + k * 60; batch_rect(&g_b, 640 - w / 2, 120 + k * 50, w, 520 - k * 50, 1, 0.85f, 0.4f, 0.035f * clampf(t - 2.2f, 0, 1)); }   /* the light on the winner */
    static const float col[3][3] = {{1, 0.82f, 0.25f}, {0.82f, 0.84f, 0.9f}, {0.82f, 0.52f, 0.28f}};
    for (int p = 0; p < 3; p++) { float x = CER_X[p], h = CER_H[p], top = 640 - h;
        batch_rect(&g_b, x - 110, top, 220, h, col[p][0] * 0.45f, col[p][1] * 0.45f, col[p][2] * 0.45f, 1);
        batch_rect(&g_b, x - 110, top, 220, 10, col[p][0], col[p][1], col[p][2], 1);
        snprintf(buf, sizeof buf, "%d", p + 1); batch_text(&g_b, buf, x - text_width(buf, 54) / 2, top + 26, 54, col[p][0], col[p][1], col[p][2], 1);
        if (t >= CER_T[p] + 0.4f) { const char *nm = CHARACTERS[ord[p]].short_name; batch_text(&g_b, nm, x - text_width(nm, 20) / 2, 652, 20, 1, 1, 1, clampf((t - CER_T[p] - 0.4f) * 3, 0, 1)); } }
    batch_rect(&g_b, 0, 640, 1280, 4, 1, 0.85f, 0.4f, 0.6f);
    const char *cup = g_cup == 0 ? "GOSPEL CUP" : "GENESIS TO GLORY CUP"; text_c(cup, 12, 36, 1, 0.85f, 0.3f, 1);
    if (t >= 3.4f) { snprintf(buf, sizeof buf, "CHAMPION: %s", CHARACTERS[ord[0]].name); text_c(buf, 56, 22, 1, 1, 1, clampf((t - 3.4f) * 2, 0, 1)); }
}
static void hud_trophy_top(void) {   /* drawn over the pictures: confetti, the player's place, the verse */
    float t = g_screen_t; char buf[96]; int ord[NKARTS]; int nk = cer_order(ord);
    if (t >= 3.4f) { static const float pal[6][3] = {{1, 0.85f, 0.3f}, {0.95f, 0.3f, 0.3f}, {0.3f, 0.6f, 1}, {0.4f, 0.9f, 0.4f}, {1, 1, 1}, {0.85f, 0.5f, 1}};
        for (int i = 0; i < 140; i++) { uint32_t hsh = (uint32_t)i * 2654435761u; float fx = (hsh & 1023) / 1023.0f, sp = 70 + ((hsh >> 10) & 127), ph = ((hsh >> 17) & 1023) / 1023.0f;
            float y = fmodf((t - 3.4f) * sp + ph * 760, 760) - 30, x = fx * 1280 + sinf(t * 2.2f + i) * 14; const float *c = pal[i % 6];
            batch_rect(&g_b, x, y, 7, (i & 1) ? 11 : 6, c[0], c[1], c[2], 0.95f); } }
    int me = -1; for (int i = 0; i < nk; i++) if (ord[i] == g_player_char) me = i;
    if (t >= 4.0f && me >= 0) { float a = clampf((t - 4.0f) * 2, 0, 1);
        if (me == 0) text_c("YOU WON THE CUP!", 86, 22, 1, 0.9f, 0.3f, a);
        else { snprintf(buf, sizeof buf, "YOU FINISHED %s", place_name(me + 1)); text_c(buf, 86, 20, 0.85f, 0.9f, 1, a); } }
    if (t >= 4.6f) { float a = clampf((t - 4.6f) * 2, 0, 1); text_c("I HAVE FOUGHT A GOOD FIGHT, I HAVE FINISHED MY COURSE, I HAVE KEPT THE FAITH", 684, 14, 1, 1, 1, a); text_c("2 TIMOTHY 4:7", 702, 12, 1, 0.85f, 0.4f, a); }
    if (t >= 5.5f && ((int)(g_time * 2) & 1)) batch_text(&g_b, "A: CONTINUE", 1260 - text_width("A: CONTINUE", 16), 690, 16, 0.9f, 0.95f, 1, 1);
}
static void draw_trophy_pictures(void) {   /* the three characters stand on the podium; the cup appears over the winner */
    int ord[NKARTS]; cer_order(ord); Mat4 id = m4_identity(); static Mesh *pm[4] = {0}; float t = g_screen_t;
    for (int p = 2; p >= 0; p--) { int ch = ord[p]; if (!tx_char[ch] || t < CER_T[p]) continue; if (!pm[p]) pm[p] = gfx_mesh_dynamic(6);
        float h = p == 0 ? 250 : 210, w = h * g_char_aspect[ch], x = CER_X[p], bot = 640 - CER_H[p] + 4 + cer_drop(p);
        if (w > 230) { w = 230; h = w / g_char_aspect[ch]; }
        float bounce = (p == 0 && t > CER_T[0] + 0.55f) ? sinf((t - CER_T[0]) * 3.0f) * 4 * expf(-(t - CER_T[0] - 0.55f) * 1.5f) : 0;
        batch_clear(&g_b); batch_rect_uv(&g_b, x - w / 2, bot - h + bounce, w, h, 0, 0, 1, 1, 1, 1, 1, 1); gfx_mesh_update(pm[p], g_b.v, g_b.n); gfx_bind(tx_char[ch]); gfx_draw(pm[p], &id, 1, 1, 1, 1); }
    if (t >= 3.4f && tx_trophy) { if (!pm[3]) pm[3] = gfx_mesh_dynamic(6);
        float u = clampf((t - 3.4f) / 0.5f, 0, 1), sz = 104 * (u < 1 ? (1.25f * u - 0.25f * u * u) : 1.0f) * (1 + 0.04f * sinf(t * 3)), top = 640 - CER_H[0] - 250 - sz - 4;
        if (top < 116) top = 116;
        batch_clear(&g_b); batch_rect_uv(&g_b, 640 - sz / 2 - (1 - u) * 0, top, sz, sz, 0, 0, 1, 1, 1, 1, 1, 1); gfx_mesh_update(pm[3], g_b.v, g_b.n); gfx_bind(tx_trophy); gfx_draw(pm[3], &id, 1, 1, 1, 1); }
}

// ------------------------------------------------------------------ per-frame update
#ifdef KK_TOUR
/* CI tour: the game presses its own buttons on a timer so the emulator test can photograph every screen */
static void tour_pads(Pad *pads, float t) {
    static const struct { float t; char key; int slot; } script[] = {
#if KK_TOUR_ROAD == 11
        /* the Grand Prix: title, GRAND PRIX, the second cup (Genesis to Glory), character, kart, the first road (Eden), then the standings */
        {8, 'A'}, {11, 'A'}, {14, 'R'}, {16, 'A'}, {19, 'A'}, {22, 'A'}, {70, 'P'}, {84, 'P'}, {130, 'A'}, {133, 'A'},
#elif KK_TOUR_ROAD == 12
        {60, 'A'}, {70, 'A'},                    /* the trophy ceremony plays out, then A, A: back to the main menu */
#elif KK_TOUR_ROAD == 10
        /* the Walls of Jericho: title, menu (down five times), character, kart, race */
        {8, 'A'}, {11, 'D'}, {13, 'D'}, {15, 'D'}, {17, 'D'}, {19, 'D'}, {21, 'A'}, {24, 'A'}, {27, 'A'},
#elif KK_TOUR_ROAD == 8
        /* four players: title, menu (down four times to MULTIPLAYER), everyone joins, + starts, everyone locks a character, P1 picks the road */
        {8, 'A'}, {11, 'D'}, {13, 'D'}, {15, 'D'}, {17, 'D'}, {19, 'A'},
        {22, 'A', 0}, {23, 'A', 1}, {24, 'A', 2}, {25, 'A', 3}, {28, 'P', 0},
        {31, 'R', 1}, {31, 'D', 2}, {31, 'L', 3}, {33, 'A', 0}, {34, 'A', 1}, {35, 'A', 2}, {36, 'A', 3},
        {40, 'A', 0}, {70, 'P', 0}, {84, 'P', 0},
#elif KK_TOUR_ROAD == 7
        {200, 0},                                /* the opener plays through, then the title screen */
#elif KK_TOUR_ROAD == 6
        {8, 'A'}, {11, 'U'}, {14, 'A'},          /* the credits, then (back at the title) options and records */
        {88, 'B'}, {91, 'A'}, {94, 'U'}, {96, 'U'}, {98, 'A'},
#else
        {8, 'A'}, {11, 'D'}, {13, KK_TOUR_ROAD == 5 ? 'D' : 0}, {15, KK_TOUR_ROAD == 5 ? 'D' : 0}, {18, KK_TOUR_ROAD == 5 ? 'A' : 0}, {14, KK_TOUR_ROAD == 5 ? 0 : 'A'}, {17, KK_TOUR_ROAD == 5 ? 0 : 'R'}, {22, KK_TOUR_ROAD == 5 ? 0 : 'A'}, {26, KK_TOUR_ROAD == 5 ? 0 : 'A'},
        {27, KK_TOUR_ROAD >= 1 ? 'R' : 0}, {29, KK_TOUR_ROAD >= 2 ? 'R' : 0}, {31, KK_TOUR_ROAD >= 3 ? 'R' : 0}, {33, KK_TOUR_ROAD >= 4 ? 'R' : 0}, {35, KK_TOUR_ROAD == 9 ? 'R' : 0}, {36, 'A'},   /* road 9: the Garden of Eden (sixth card) */
        {70, 'P'}, {84, 'P'}
#endif
    };
    memset(pads, 0, sizeof(Pad) * KK_PADS);
    for (int sl = 0; sl < KK_PADS; sl++) pads[sl].source = sl == 0 ? 1 : 2;   /* the tour pretends a GamePad and four Pro Controllers are connected */
    for (unsigned i = 0; i < sizeof script / sizeof script[0]; i++) {
        if (t >= script[i].t && t < script[i].t + 0.15f) { Pad *p = &pads[script[i].slot];
            switch (script[i].key) { case 'A': p->a = 1; break; case 'B': p->b = 1; break; case 'P': p->plus = 1; break;
                                     case 'D': p->sy = -1; break; case 'U': p->sy = 1; break; case 'R': p->sx = 1; break; case 'L': p->sx = -1; break; }
        }
    }
}
#endif
/* the split-screen layout: where player p's view sits on the TV (fractions of the screen) */
static void split_rect(int p, int n, float *fx, float *fy, float *fw, float *fh) {
    if (n <= 1) { *fx = 0; *fy = 0; *fw = 1; *fh = 1; }
    else if (n == 2) { *fx = 0; *fy = p * 0.5f; *fw = 1; *fh = 0.5f; }
    else { *fx = (p & 1) * 0.5f; *fy = (p >> 1) * 0.5f; *fw = 0.5f; *fh = 0.5f; }
}
/* one controller for the menus: every connected controller can press the buttons */
static Pad merge_pads(const Pad *pads, int npads) {
    Pad m; memset(&m, 0, sizeof m);
    for (int i = 0; i < npads; i++) { const Pad *p = &pads[i]; if (!p->source) continue;
        if (m.sx == 0) m.sx = p->sx;
        if (m.sy == 0) m.sy = p->sy;
        m.a |= p->a; m.b |= p->b; m.zl |= p->zl; m.zr |= p->zr; m.plus |= p->plus; m.x |= p->x; m.y |= p->y; m.source |= p->source; }
    return m;
}
static const Pad *player_pad(const Pad *pads, int npads, int p) { int sl = g_pslot[p]; static Pad none; return sl >= 0 && sl < npads ? &pads[sl] : &none; }
static void begin_player_race(void) { set_track(g_mode == MODE_GP ? g_gp_order[g_gp_round] : (g_mode == MODE_JERICHO ? JERICHO_TRACK : g_track_idx)); start_race(0); g_screen = SCR_RACE; }
static void begin_battle(void) {
    set_track(ARENA_TRACK); g_attract = 0;
    battle_init(&g_battle, &g_track, g_difficulty); g_nplayers = 1;
    g_skid_n = 0; memset(g_skid, 0, sizeof g_skid); memset(g_dust, 0, sizeof g_dust); memset(g_last_skid, 0, sizeof g_last_skid);
    reset_views();
    g_results_t = 0; g_demo = 0; g_idle = 0; g_screen = SCR_RACE;
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
    if (g_screen == SCR_TROPHY) return music_available("music_trophy") ? "music_trophy" : (music_available("music_results") ? "music_results" : "music_menu");
    if (g_screen == SCR_GP) {
        if (g_gp_round >= NUM_GP_TRACKS) { int rank = gp_player_rank();
            if (rank == 1 && music_available("music_trophy")) return "music_trophy";
            if (rank > 3 && music_available("music_lose")) return "music_lose"; }
        return music_available("music_results") ? "music_results" : "music_menu";
    }
    if ((g_screen == SCR_CHAR || g_screen == SCR_KART || g_screen == SCR_TRACK || g_screen == SCR_JOIN || g_screen == SCR_MCHAR || g_screen == SCR_CUP) && music_available("music_select")) return "music_select";
    if (g_screen == SCR_CREDITS && music_available("music_credits")) return "music_credits";
    if (g_screen == SCR_OPENER) return g_op_vid && g_op_buf < 2.0f ? NULL : (music_available("music_opener") ? "music_opener" : NULL);   /* silent while the video fills up */
    return "music_menu";
}

/* dynamic geometry for the view being drawn (cv): trees, skid marks, dust, shadows, items. Pictures face this view's camera. */
static void build_dynamic(void) {
    Race *r = &g_race;
    float cy = cam_yaw; Vec3 ax = v3(-cosf(cy), 0, sinf(cy));   /* screen-right in world space, so pictures are not mirrored */
    {
        for (int kd = 0; kd < g_nkind; kd++) {
            batch_clear(&g_b);
            for (int i = 0; i < g_nplaced; i++) { Placed *pl = &g_placed[i]; if (pl->kind != kd) continue;
                float dx = pl->p.x - cam_x, dz = pl->p.z - cam_z; float sdd = g_nplayers >= 3 ? SPRITE_DRAW_DIST * 0.6f : SPRITE_DRAW_DIST; if (dx * dx + dz * dz > sdd * sdd) continue;
                float w = pl->w * 0.5f; int scroll = pl->falls && g_kind[kd].placeholder;
                float v0 = scroll ? g_time * 1.5f : 0.0f, v1 = v0 + (scroll ? pl->h / 3.0f : 1.0f);
                float sw = g_kind[kd].sway == 1 ? sinf(g_time * 0.9f + i * 1.3f) * 0.025f * pl->h : (g_kind[kd].sway == 2 ? sinf(g_time * 3.0f + i * 2.1f) * 0.03f * pl->h : 0);   /* the top of trees and cloth moves in the wind */
                float hh = pl->h;
                if (pl->wall && pl->fall_t > 0) { float f = clampf(pl->fall_t / 1.2f, 0, 1); f = f * f; hh = pl->h * (1.0f - 0.78f * f); sw += sinf(pl->fall_t * 30.0f) * (1 - f) * 0.04f * pl->h;   /* it shudders, sinks into its own rubble, a quarter of it left standing */
                    v0 = 1.0f - hh / pl->h; v1 = 1.0f; }   /* the bottom of the picture stays on the ground */
                Vec3 c = v3(pl->p.x, pl->p.y + hh * 0.5f, pl->p.z), r_ = v3(ax.x * w, 0, ax.z * w), u_ = v3(ax.x * sw, hh * 0.5f, ax.z * sw);
                batch_quad4(&g_b, v3(c.x - r_.x - u_.x, c.y - u_.y, c.z - r_.z - u_.z), v3(c.x + r_.x - u_.x, c.y - u_.y, c.z + r_.z - u_.z), v3(c.x + r_.x + u_.x, c.y + u_.y, c.z + r_.z + u_.z), v3(c.x - r_.x + u_.x, c.y + u_.y, c.z - r_.z + u_.z), v3(0, 1, 0), 0, v0, 1, v1, 1, 1, 1, 1); }
            for (int m = 0; m < g_track.nmover; m++) { if (g_mover_kind[m] != kd) continue; const Mover *mv = &g_track.mover[m];
                if (r->jericho && mv->img[1] == 'r' && mv->img[2] == 'u' && r->wall_fall < 0.3f) continue;   /* the rubble appears once the walls start to fall */
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
        if (g_def->night) {   /* each kart's lantern lights the road ahead of it */
            for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; float dx = q->x - cam_x, dz = q->z - cam_z; if (dx * dx + dz * dz > 80.0f * 80.0f) continue;
                float fx = sinf(q->yaw), fz = cosf(q->yaw), big = r->human[i] ? 1.0f : 0.7f;
                Vec3 c = v3(q->x + fx * 4.5f * big, q->y + 0.08f, q->z + fz * 4.5f * big);
                batch_quad(&g_b, c, v3(fz * 3.2f * big, 0, -fx * 3.2f * big), v3(fx * 5.0f * big, 0, fz * 5.0f * big), v3(0, 1, 0), 0, 0, 1, 1, 1.0f, 0.85f, 0.55f, 0.28f); } }
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
        /* blessings on the road (additive glow sprites): manna, fire trails, the parted sea, the Samaritan's light, rainbow rings */
        batch_clear(&g_b);
        for (int p = 0; p < MAX_PROJ; p++) { Proj *pr = &r->proj[p]; if (!pr->alive) continue;
            float fade = pr->life < 1.0f ? pr->life : 1.0f;
            if (pr->type == TRAIL_MANNA) { float pul = 0.8f + 0.2f * sinf(g_time * 6.0f + p), sz = 0.9f * pul;
                batch_quad(&g_b, v3(pr->x, pr->y + 0.5f, pr->z), v3(ax.x * sz, 0, ax.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0, 0, 1, 1, 1.0f, 0.9f, 0.5f, 0.9f * fade);
                batch_quad(&g_b, v3(pr->x, pr->y + 0.5f, pr->z), v3(ax.x * sz * 2.2f, 0, ax.z * sz * 2.2f), v3(0, sz * 2.2f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1.0f, 0.8f, 0.3f, 0.35f * fade); }
            else { float fl = 0.8f + 0.2f * sinf(g_time * 14.0f + p * 1.3f), sz = 1.1f * fl;
                batch_quad(&g_b, v3(pr->x, pr->y + 0.7f, pr->z), v3(ax.x * sz, 0, ax.z * sz), v3(0, sz * 1.4f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1.0f, 0.55f, 0.1f, 0.85f * fade);
                batch_quad(&g_b, v3(pr->x, pr->y + 0.4f, pr->z), v3(ax.x * sz * 0.6f, 0, ax.z * sz * 0.6f), v3(0, sz * 0.9f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1.0f, 0.95f, 0.5f, 0.9f * fade); } }
        if (r->parting_t > 0) {   /* walls of water standing along both shoulders of the parted stretch */
            float a = r->parting_t > 9.0f ? (10.0f - r->parting_t) : (r->parting_t < 1.5f ? r->parting_t / 1.5f : 1.0f);
            for (int n = 0; n < r->parting_len; n += 2) { int i0 = (r->parting_idx + n) % g_track.n, i1 = (r->parting_idx + n + 2) % g_track.n;
                float e0 = track_edge(&g_track, i0) + 1.0f, e1 = track_edge(&g_track, i1) + 1.0f, hgt = 5.0f + 1.5f * sinf(g_time * 3.0f + n * 0.4f);
                for (int side = -1; side <= 1; side += 2) { Vec3 p0 = road_point(&g_track, i0, side * e0), p1 = road_point(&g_track, i1, side * e1);
                    batch_quad4(&g_b, p0, p1, v3(p1.x, p1.y + hgt * a, p1.z), v3(p0.x, p0.y + hgt * a, p0.z), v3(0, 1, 0), 0, 0, 1, 1, 0.35f, 0.7f, 1.0f, 0.45f * a);
                    batch_quad4(&g_b, v3(p0.x, p0.y + hgt * a * 0.8f, p0.z), v3(p1.x, p1.y + hgt * a * 0.8f, p1.z), v3(p1.x, p1.y + hgt * a, p1.z), v3(p0.x, p0.y + hgt * a, p0.z), v3(0, 1, 0), 0, 0, 1, 1, 0.9f, 0.97f, 1.0f, 0.6f * a); } } }
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i];
            if (q->carried_t > 0) { float sz = 2.6f + 0.2f * sinf(g_time * 5.0f);   /* a pillar of light carries the kart */
                batch_quad(&g_b, v3(q->x, q->y + 2.2f, q->z), v3(ax.x * sz * 0.6f, 0, ax.z * sz * 0.6f), v3(0, sz * 1.6f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1.0f, 0.97f, 0.8f, 0.5f); }
            if (q->shield > 0 && q->shield < 8.5f && q->shield > 7.0f) { float sz = 2.4f; batch_quad(&g_b, v3(q->x, q->y + 1.0f, q->z), v3(ax.x * sz, 0, ax.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.6f, 0.85f, 1.0f, 0.35f); } }
        for (int i = 0; i < MAX_RING; i++) if (g_ring[i].t > 0) { Ring *rg = &g_ring[i]; float u = 1.0f - rg->t / 0.6f, sz = 0.5f + u * 5.0f;
            batch_quad(&g_b, v3(rg->p.x, rg->p.y + 0.8f, rg->p.z), v3(ax.x * sz, 0, ax.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.6f + 0.4f * sinf(u * 6), 0.6f + 0.4f * sinf(u * 6 + 2), 0.6f + 0.4f * sinf(u * 6 + 4), 0.7f * (1 - u)); }
        gfx_mesh_update(m_items, g_b.v, g_b.n);
    }
}
/* the 3D world as seen from the view cv, for the player driving kart vk; aspect is the viewport's width / height, fov_mul trims the lens for a wide half-screen view */
static void draw_scene(int vk, float aspect, float fov_mul, int lite) {
    Race *r = &g_race;
    float cy = cam_yaw; Vec3 ax = v3(-cosf(cy), 0, sinf(cy));
    Kart *k = &r->k[vk];
    float shk = g_shake * g_shake * 0.6f;
    Vec3 eye = v3(cam_x + sinf(g_time * 37.0f) * shk, cam_y + sinf(g_time * 53.0f) * shk, cam_z + cosf(g_time * 41.0f) * shk), at = v3(k->x + sinf(k->yaw) * 3.0f, k->y + 1.2f, k->z + cosf(k->yaw) * 3.0f);
    {
        Mat4 view = m4_look_at(eye, at, v3(0, 1, 0));
        Mat4 proj = m4_perspective(cam_fov * fov_mul * 3.14159265f / 180.0f, aspect, 0.3f, 1200.0f);
        Mat4 vp = m4_mul(proj, view);
        Mat4 id = m4_identity();
        Vec3 ldir = v3(g_def->light_x, g_def->light_y, g_def->light_z);
        { /* hemisphere light: the sky's color from above, a bounce color from below, the sun's color on lit faces */
          float mx = fmaxf(g_def->sky_r, fmaxf(g_def->sky_g, g_def->sky_b)); if (mx < 0.05f) mx = 0.05f;
          Vec3 skyc = v3(0.3f + 0.7f * g_def->sky_r / mx, 0.3f + 0.7f * g_def->sky_g / mx, 0.3f + 0.7f * g_def->sky_b / mx);
          Vec3 gnd = g_def->night ? v3(0.5f, 0.45f, 0.5f) : (g_def->water ? v3(0.75f, 0.85f, 0.95f) : (g_def->grid_idx ? v3(1.0f, 0.95f, 0.9f) : v3(0.95f, 0.85f, 0.7f)));
          gfx_light(v3(g_def->sun_r, g_def->sun_g, g_def->sun_b), skyc, gnd, eye); }
        gfx_camera(&vp, ldir, 1.0f, g_def->fog_r, g_def->fog_g, g_def->fog_b, 1e6f, 2e6f);
        gfx_state(ST_BLEND); gfx_bind(tx_sky); { Mat4 sm = m4_translate(cam_x, -30.0f, cam_z); gfx_draw_ex(m_sky, &sm, 1, 1, 1, 1, 0, 1, 0, 0); }
        if (tx_star) { gfx_state(ST_BLEND | ST_ADD); gfx_bind(tx_star);
            batch_clear(&g_b); float ss = g_def->star_size * (0.95f + 0.05f * sinf(g_time * 3.0f));
            Vec3 sp = v3(cam_x + g_def->star_x, g_def->star_y, cam_z + g_def->star_z);
            batch_quad(&g_b, sp, v3(ax.x * ss, 0, ax.z * ss), v3(0, ss, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 1);
            static Mesh *m_star = NULL; if (!m_star) m_star = gfx_mesh_dynamic(12); gfx_mesh_update(m_star, g_b.v, g_b.n); gfx_draw_ex(m_star, &id, 1, 1, 1, 1, 0, 1, 0, 0); }
        gfx_camera(&vp, ldir, g_def->ambient, g_def->fog_r, g_def->fog_g, g_def->fog_b, g_def->fog_start, g_def->fog_end);
        gfx_state(ST_SOLID);
        float sr = 1, sg = 1, sb = 1;    /* the sun's color is applied by the shader now */
        /* the "grass" plane is the sea on a water road: it glints in the sun and drifts slowly */
        gfx_bind(tx_grass); if (g_def->water) gfx_draw_ex(m_ground, &id, 1, 1, 1, 1, 0.9f, 0, g_time * 0.012f, g_time * 0.008f); else gfx_draw(m_ground, &id, sr, sg, sb, 1);
        gfx_bind(tx_dirt); gfx_draw(m_shoulder, &id, sr, sg, sb, 1);
        gfx_bind(tx_road); gfx_draw_ex(m_road, &id, 1, 1, 1, 1, g_def->night ? 0.35f : 0.18f, 0, 0, 0);   /* a faint sheen on the road, wetter at night */
        if (g_def->bonus != 2) { gfx_bind(tx_flag); gfx_draw(m_startline, &id, sr, sg, sb, 1); }
        gfx_bind(tx_wall); gfx_draw(m_wall, &id, sr, sg, sb, 1);
        gfx_bind(tx_wall); gfx_draw(m_tunnel, &id, sr * 0.8f, sg * 0.8f, sb * 0.8f, 1);
        if (g_def->bonus != 2) { gfx_bind(tx_flag); gfx_draw(m_arch, &id, sr, sg, sb, 1); }
        if (g_def->bonus == 2 && m_brook) { static Texture *tx_brook = NULL; if (!tx_brook) tx_brook = make_tex(128, 128, 1, 0, px_water); gfx_state(ST_TRANSPARENT); gfx_bind(tx_brook);
            Mat4 flow = m4_translate(sinf(g_time * 2.0f) * 0.3f, 0, 0); gfx_draw_ex(m_brook, &flow, 1, 1, 1, 0.75f, 0.8f, 0, g_time * 0.05f, 0); gfx_state(ST_SOLID); }
        { float pulse = 0.75f + 0.25f * sinf(g_time * 6.0f);
          gfx_state(ST_TRANSPARENT); gfx_bind(NULL); gfx_draw(m_boostpads, &id, pulse, pulse, pulse, 1);
          gfx_bind(tx_blob); gfx_draw(m_bouncepads, &id, 1, 1, 1, 0.9f);
          gfx_state(ST_SOLID); }
        gfx_state(ST_CUTOUT);
        for (int kd = 0; kd < g_nkind; kd++) { gfx_bind(g_kind[kd].tex); gfx_draw(g_kind[kd].mesh, &id, 1, 1, 1, 1); }
        if (tx_finale && m_finale) { gfx_state(ST_CUTOUT); gfx_bind(tx_finale); gfx_draw_ex(m_finale, &id, 1, 1, 1, 1, 0, 1, 0, 0); }
        if (!g_def->night && !lite) {   /* doves crossing the sky, and the sun's glare */
            static Mesh *m_birds = NULL, *m_sun = NULL; if (!m_birds) { m_birds = gfx_mesh_dynamic(6 * 6); m_sun = gfx_mesh_dynamic(12); }
            batch_clear(&g_b);
            for (int bd = 0; bd < 6; bd++) { float t = g_time * 0.25f + bd * 1.05f, rad = 90.0f + bd * 17.0f;
                Vec3 bp = v3(cam_x + cosf(t) * rad, 26.0f + bd * 3.0f + sinf(g_time * 0.7f + bd) * 2.0f, cam_z + sinf(t) * rad);
                float flap = 0.35f + 0.65f * fabsf(sinf(g_time * 7.0f + bd * 1.7f)), bw = 1.1f;
                batch_quad(&g_b, bp, v3(ax.x * bw, 0, ax.z * bw), v3(0, bw * 0.5f * flap, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 0.9f); }
            gfx_mesh_update(m_birds, g_b.v, g_b.n); gfx_state(ST_CUTOUT); gfx_bind(tx_bird); gfx_draw_ex(m_birds, &id, 1, 1, 1, 1, 0, 1, 0, 0);
            batch_clear(&g_b);
            { Vec3 sd = v3_norm(ldir); float dist = 700.0f; Vec3 sp = v3(cam_x + sd.x * dist, cam_y + sd.y * dist * 0.55f, cam_z + sd.z * dist);
              float ss = 95.0f; batch_quad(&g_b, sp, v3(ax.x * ss, 0, ax.z * ss), v3(0, ss, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 0.95f, 0.8f, 0.55f); }
            gfx_mesh_update(m_sun, g_b.v, g_b.n); gfx_state(ST_BLEND | ST_ADD); gfx_bind(tx_blob); gfx_draw_ex(m_sun, &id, 1, 1, 1, 1, 0, 1, 0, 0); }
        gfx_state(ST_TRANSPARENT); gfx_bind(NULL); gfx_draw(m_skid, &id, 1, 1, 1, 1);
        gfx_bind(tx_blob); gfx_draw(m_scshadow, &id, 1, 1, 1, 1); gfx_draw(m_shadow, &id, 1, 1, 1, 1);
        /* item boxes: spinning cubes */
        /* the boxes float a metre up, turn slowly, shimmer through the rainbow and glow */
        { static Mesh *m_boxglow = NULL; if (!m_boxglow) m_boxglow = gfx_mesh_dynamic(MAX_BOX * 12);
          batch_clear(&g_b);
          for (int b = 0; b < r->nbox; b++) { ItemBox *bx = &r->box[b]; if (!bx->active) continue; float yb = bx->y + 1.1f + 0.25f * sinf(g_time * 2.0f + b);
              float pul = 0.85f + 0.15f * sinf(g_time * 4.0f + b * 0.7f), sz = 1.9f * pul;
              batch_quad(&g_b, v3(bx->x, yb, bx->z), v3(ax.x * sz, 0, ax.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.75f, 0.85f, 1.0f, 0.3f);
              batch_quad(&g_b, v3(bx->x, bx->y + 0.05f, bx->z), v3(1.2f, 0, 0), v3(0, 0, -1.2f), v3(0, 1, 0), 0, 0, 1, 1, 0.6f, 0.8f, 1.0f, 0.3f); }
          gfx_mesh_update(m_boxglow, g_b.v, g_b.n); gfx_state(ST_TRANSPARENT | ST_ADD); gfx_bind(tx_blob); gfx_draw(m_boxglow, &id, 1, 1, 1, 1); }
        for (int pass = 0; pass < 2; pass++)   /* inside faces first (seen through the front), then the frame and the front faces */
        for (int b = 0; b < r->nbox; b++) { ItemBox *bx = &r->box[b]; if (!bx->active) continue; float yb = bx->y + 1.1f + 0.25f * sinf(g_time * 2.0f + b);
            float ddx = bx->x - cam_x, ddz = bx->z - cam_z; if (ddx * ddx + ddz * ddz > 170.0f * 170.0f) continue;
            float hue = g_time * 1.2f + b * 0.9f; float cr = 0.6f + 0.4f * sinf(hue), cg = 0.6f + 0.4f * sinf(hue + 2.094f), cb = 0.6f + 0.4f * sinf(hue + 4.189f);
            Mat4 m = m4_mul(m4_translate(bx->x, yb, bx->z), m4_mul(m4_rotate_y(g_time * 1.3f + b), m4_rotate_x(0.45f + 0.15f * sinf(g_time + b))));
            if (pass == 0) { Mat4 inside = m4_mul(m, m4_scale(-1, 1, 1)); gfx_state(ST_TRANSPARENT | ST_CULL); gfx_bind(tx_qbox); gfx_draw_ex(m_qface, &inside, cr, cg, cb, 0.55f, 0.2f, 0, 0, 0); continue; }
            gfx_state(ST_SOLID); gfx_bind(NULL); gfx_draw_ex(m_qframe, &m, 1.0f, 0.97f, 0.9f, 1, 1.0f, 0, 0, 0);
            gfx_state(ST_TRANSPARENT | ST_CULL); gfx_bind(tx_qbox); gfx_draw_ex(m_qface, &m, cr, cg, cb, 0.85f, 1.0f, 0, 0, 0); }
        gfx_bind(NULL);
        /* coins: spinning discs */
        gfx_state(ST_SOLID); gfx_bind(NULL);
        for (int c = 0; c < g_track.ncoin; c++) { if (r->coin_respawn[c] > 0) continue; const Coin *cn = &g_track.coin[c]; const TrackSample *cs = &g_track.s[cn->idx];
            float cx = cs->x + cs->nx * cn->lateral, cz = cs->z + cs->nz * cn->lateral; float ddx = cx - cam_x, ddz = cz - cam_z; if (ddx * ddx + ddz * ddz > 160.0f * 160.0f) continue;
            Mat4 m = m4_mul(m4_translate(cx, track_surface_height(&g_track, cn->idx, cn->lateral) + 1.0f + 0.1f * sinf(g_time * 3.0f + c), cz), m4_rotate_y(g_time * 3.0f + c * 0.7f));
            gfx_draw_ex(m_coin, &m, 1, 1, 1, 1, 1.2f, 0, 0, 0); }
        /* the karts */
        gfx_state(ST_SOLID); gfx_bind(NULL);
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; const Character *c = &CHARACTERS[r->character[i]];
            if (i != vk) { float cdx = q->x - eye.x, cdz = q->z - eye.z; if (cdx * cdx + cdz * cdz < 3.2f * 3.2f) continue; }   /* a kart sitting on the camera would fill the screen */
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
        if (g_mode == MODE_TT && !g_attract && g_ghost_n > 1 && r->state != RACE_COUNTDOWN) {   /* the ghost of the best run */
            float t = r->race_t * GHOST_HZ; int i0 = (int)t; if (i0 >= g_ghost_n - 1) i0 = g_ghost_n - 2; float f = clampf(t - i0, 0, 1);
            GhostSample *a = &g_ghost[i0], *b = &g_ghost[i0 + 1];
            float gx = (a->x + (b->x - a->x) * f) / 16.0f, gz = (a->z + (b->z - a->z) * f) / 16.0f, gyaw = a->yaw / 10000.0f;
            TrackHit gh; track_locate(&g_track, gx, gz, -1, &gh);
            int ch = g_ghost_char; Texture *gt = tx_ride[ch][1] ? tx_ride[ch][1] : (tx_kartback[ch] ? tx_kartback[ch] : NULL); float gasp = tx_ride[ch][1] ? g_ride_aspect[ch][1] : g_kartback_aspect[ch];
            if (gt) { if (!m_ghost) m_ghost = gfx_mesh_dynamic(6); float kw = 2.8f, kh = kw / gasp; (void)gyaw;
                batch_clear(&g_b); batch_quad(&g_b, v3(gx, gh.height + kh * 0.5f - 0.1f, gz), v3(ax.x * kw * 0.5f, 0, ax.z * kw * 0.5f), v3(0, kh * 0.5f, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.7f, 0.9f, 1.0f, 0.45f);
                gfx_mesh_update(m_ghost, g_b.v, g_b.n); gfx_state(ST_TRANSPARENT); gfx_bind(gt); gfx_draw_ex(m_ghost, &id, 0.7f, 0.9f, 1.0f, 0.45f, 0, 1, 0, 0); gfx_state(ST_CUTOUT); } }
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; int ch = r->character[i];
            if (i != vk) { float cdx = q->x - eye.x, cdz = q->z - eye.z; if (cdx * cdx + cdz * cdz < 3.2f * 3.2f) continue; }
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
                /* a picture with the driver already in the kart wins over kart + separate driver */
                int rv = (want == 0 && !tx_ride[ch][0] && tx_ride[ch][1]) ? 1 : want;   /* straight behind: the behind-left picture is close enough */
                Texture *ktex = tx_kartview[ch][v]; float kasp = g_kartview_aspect[ch][v]; int ride = 0;
                const WheelDef *wd = g_view_wd[ch][v]; Texture *wtex = g_view_wtex[ch][v]; const TreadDef *td = NULL;
                if (tx_ride[ch][rv]) { ktex = tx_ride[ch][rv]; kasp = g_ride_aspect[ch][rv]; v = rv; ride = 1; wd = g_ride_wd[ch][rv]; wtex = g_ride_wtex[ch][rv]; td = g_ride_td[ch][rv]; }
                float kw = v == 2 ? 3.4f : (v == 1 || v == 3 ? 3.0f : 2.6f), kh = kw / kasp, klean = q->steer_vis * -0.08f + q->slide * 0.15f;
                Vec3 kc = v3(q->x, q->y + q->hop * 0.4f + kh * 0.5f - 0.1f, q->z);
                batch_clear(&g_b);
                float u0 = mirror ? 1.0f : 0.0f, u1 = mirror ? 0.0f : 1.0f;
                batch_quad(&g_b, kc, v3(ax.x * kw * 0.5f, 0, ax.z * kw * 0.5f), v3(ax.x * klean * kh * 0.5f, kh * 0.5f, ax.z * klean * kh * 0.5f), v3(0, 1, 0), u0, 0, u1, 1, 1, 1, 1, 1);
                gfx_mesh_update(m_kart_spr[i], g_b.v, g_b.n); gfx_bind(ktex); float st = q->stun > 0 ? 0.6f : 1.0f; gfx_draw(m_kart_spr[i], &id, sr * st, sg * st, sb * st, 1);
                if (wd && wtex) {   /* the wheels, cut from the same picture, turned by the distance driven */
                    float ang = g_wheel_ang[i] * (float)wd->spin * (mirror ? -1.0f : 1.0f), ca = cosf(ang), sa = sinf(ang);
                    Vec3 R = v3(ax.x * kw * 0.5f, 0, ax.z * kw * 0.5f), U = v3(ax.x * klean * kh * 0.5f, kh * 0.5f, ax.z * klean * kh * 0.5f);
                    batch_clear(&g_b);
                    for (int n = 0; n < wd->n; n++) {
                        float cx = mirror ? 1.0f - wd->cx[n] : wd->cx[n], cy = wd->cy[n], fx = (cx - 0.5f) * 2.0f, fy = (0.5f - cy) * 2.0f;
                        Vec3 c = v3(kc.x + R.x * fx + U.x * fy, kc.y + R.y * fx + U.y * fy + 0.01f, kc.z + R.z * fx + U.z * fy);
                        float sx = wd->rx[n] * 2.0f / 0.84f, sy = wd->ry[n] * 2.0f / 0.84f;   /* the disc fills 84% of its cell */
                        Vec3 pts[4]; float uu[4] = {-1, 1, 1, -1}, vv[4] = {-1, -1, 1, 1};
                        for (int k2 = 0; k2 < 4; k2++) { float u = uu[k2] * ca - vv[k2] * sa, w2 = uu[k2] * sa + vv[k2] * ca;
                            pts[k2] = v3(c.x + R.x * u * sx + U.x * w2 * sy, c.y + R.y * u * sx + U.y * w2 * sy, c.z + R.z * u * sx + U.z * w2 * sy); }
                        float u0 = (float)n / wd->n, u1 = (float)(n + 1) / wd->n;
                        Vertex *o = &g_b.v[g_b.n]; if (g_b.n + 6 <= g_b.cap) {
                            float tu[4] = {u0, u1, u1, u0}, tv[4] = {1, 1, 0, 0}; static const int idx[6] = {0, 1, 2, 0, 2, 3};
                            if (mirror) { tu[0] = u1; tu[1] = u0; tu[2] = u0; tu[3] = u1; }
                            for (int k2 = 0; k2 < 6; k2++) { int kk = idx[k2]; Vertex vq = {pts[kk].x, pts[kk].y, pts[kk].z, 0, 1, 0, tu[kk], tv[kk], 1, 1, 1, 1}; o[k2] = vq; } g_b.n += 6; } }
                    gfx_mesh_update(m_wheel_spr[i], g_b.v, g_b.n); gfx_bind(wtex); gfx_draw(m_wheel_spr[i], &id, sr * st, sg * st, sb * st, 1); }
                if (td) {   /* edge-on tires: a tread pattern rolls over them */
                    float roll = g_tread_ph[i];   /* tread position, moved at most part of a groove per frame (see game_frame) so it reads as rolling instead of flickering */
                    Vec3 R = v3(ax.x * kw * 0.5f, 0, ax.z * kw * 0.5f), U = v3(ax.x * klean * kh * 0.5f, kh * 0.5f, ax.z * klean * kh * 0.5f);
                    batch_clear(&g_b);
                    for (int n = 0; n < td->n; n++) { float cx = mirror ? 1.0f - td->cx[n] : td->cx[n], fx = (cx - 0.5f) * 2.0f, fy = (0.5f - td->cy[n]) * 2.0f, hw = td->hw[n] * 2.0f, hh = td->hh[n] * 2.0f;
                        Vec3 c = v3(kc.x + R.x * fx + U.x * fy, kc.y + R.y * fx + U.y * fy + 0.01f, kc.z + R.z * fx + U.z * fy);
                        float v0 = -roll * 4.0f;
                        batch_quad(&g_b, c, v3(R.x * hw, R.y * hw, R.z * hw), v3(U.x * hh, U.y * hh, U.z * hh), v3(0, 1, 0), 0, v0, 1, v0 + 2.0f, 1, 1, 1, 0.8f); }
                    gfx_mesh_update(m_tread_spr[i], g_b.v, g_b.n); gfx_state(ST_TRANSPARENT); gfx_bind(tx_tread); gfx_draw_ex(m_tread_spr[i], &id, 1, 1, 1, 1, 0, 1, 0, 0); gfx_state(ST_CUTOUT); }
                if (ride) continue; }
            if (!tx_char[ch]) continue;
            /* the driver sits: only the top part of the standing picture shows (head, shoulders, chest), sunk into the seat */
            float part = pic ? 0.56f : 1.0f;
            float hgt = (pic ? 1.5f : 1.75f) * part, hw = (pic ? 1.5f : 1.75f) * 0.5f * g_char_aspect[ch];
            float bob = sinf(g_time * 11.0f + i) * 0.015f * fminf(fabsf(q->speed) / 20.0f, 1.0f);   /* a little engine shake */
            hgt *= 1.0f - q->squash * 0.18f + (q->air && q->hop <= 0 ? -0.08f : 0);                 /* lands with a bounce, tucks in on jumps */
            Vec3 c = v3(q->x - hx * 0.35f, q->y + q->hop * 0.4f + bob + (pic ? 0.85f : 0.55f) + hgt * 0.5f, q->z - hz * 0.35f);
            float lean = q->steer_vis * -0.12f;       /* leans into the turn */
            batch_clear(&g_b);
            batch_quad(&g_b, c, v3(ax.x * hw, 0, ax.z * hw), v3(ax.x * lean * hgt * 0.5f, hgt * 0.5f, ax.z * lean * hgt * 0.5f), v3(0, 1, 0), 0, 0, 1, part, 1, 1, 1, 1);
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
                if (tx_bearer && b->gstate != GOL_DOWN) {   /* the shield-bearer walks before him */
                    float bx = b->gx + sinf(b->sb_yaw) * 3.2f, bz = b->gz + cosf(b->sb_yaw) * 3.2f; TrackHit bh; track_locate(&g_track, bx, bz, -1, &bh); float bhg = 4.2f, bhw = bhg * 0.5f * g_bearer_aspect;
                    batch_clear(&g_b); batch_quad(&g_b, v3(bx, bh.height + bhg * 0.5f, bz), v3(ax.x * bhw, 0, ax.z * bhw), v3(0, bhg * 0.5f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 1);
                    gfx_mesh_update(m_bearer, g_b.v, g_b.n); gfx_bind(tx_bearer); gfx_draw(m_bearer, &id, sr, sg, sb, 1); }
                gfx_state(ST_SOLID); gfx_bind(NULL); }
        }
        /* shields */
        gfx_state(ST_TRANSPARENT); gfx_bind(NULL);
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; if (q->shield <= 0) continue;
            Mat4 m = m4_mul(m4_translate(q->x, q->y, q->z), m4_rotate_y(g_time * 2.0f));
            gfx_draw(m_shield, &m, 0.4f, 0.8f, 1.0f, 0.25f + 0.1f * sinf(g_time * 8)); }
        gfx_bind(tx_blob); gfx_draw(m_items, &id, 1, 1, 1, 1);
        gfx_state(ST_TRANSPARENT | ST_ADD); gfx_draw(m_dust, &id, 1, 1, 1, 1); gfx_draw(m_spark, &id, 1, 1, 1, 1); gfx_draw(m_glow, &id, 1, 1, 1, 1);
    }
}
/* the HUD of one split-screen view, laid out on a virtual screen of vw x vh pixels */
static const char *item_short(int item) {
    switch (item) { case ITEM_MANNA: return "MANNA"; case ITEM_LOAVES: return "LOAVES"; case ITEM_SAMARITAN: return "SAMARITAN"; case ITEM_SHIELD: return "SHIELD";
                    case ITEM_PARTING: return "PARTING"; case ITEM_JUBILEE: return "JUBILEE"; case ITEM_FIRE: return "FIRE"; case ITEM_WATER: return "WATER"; default: return ""; }
}
static void hud_split(int p, float vw, float vh) {
    Race *r = &g_race; int vk = human_kart(p); Kart *k = &r->k[vk]; char buf[64]; float sc = vw < 1000 ? 0.75f : 1.0f;
    int lap = k->lap < 1 ? 1 : (k->lap > g_track.laps ? g_track.laps : k->lap);
    snprintf(buf, sizeof buf, "LAP %d/%d", lap, g_track.laps); batch_text(&g_b, buf, 16, 12, 26 * sc, 1, 1, 1, 1);
    fmt_time(buf, sizeof buf, k->lap >= 1 ? k->lap_time : 0.0f); batch_text(&g_b, buf, 16, 12 + 30 * sc, 18 * sc, 1, 1, 0.6f, 1);
    { char tag[8]; snprintf(tag, sizeof tag, "P%d", p + 1); float tw = text_width(tag, 18 * sc) + 12; batch_rect(&g_b, vw / 2 - tw / 2, 8, tw, 24 * sc, PLAYER_COL[p][0], PLAYER_COL[p][1], PLAYER_COL[p][2], 0.9f); batch_text(&g_b, tag, vw / 2 - tw / 2 + 6, 10, 18 * sc, 0.05f, 0.05f, 0.1f, 1);
      batch_text(&g_b, CHARACTERS[r->character[vk]].short_name, vw / 2 - text_width(CHARACTERS[r->character[vk]].short_name, 12 * sc) / 2, 12 + 26 * sc, 12 * sc, 1, 1, 1, 0.9f); }
    int mypos = 0; for (int q = 0; q < r->nk; q++) if (r->order[q] == vk) mypos = q + 1;
    if (k->finished && k->place) mypos = k->place;
    batch_text(&g_b, place_name(mypos), vw - 16 - text_width(place_name(mypos), 48 * sc), 10, 48 * sc, 1, 0.9f, 0.3f, 1);
    snprintf(buf, sizeof buf, "%3.0f KM/H", fabsf(k->speed) * 3.6f); batch_text(&g_b, buf, vw - 16 - text_width(buf, 16 * sc), 12 + 52 * sc, 16 * sc, 1, 1, 1, 1);
    /* the blessing slot, bottom left */
    { float bw = 120 * sc, bh = 44 * sc, bx = 16, by = vh - bh - 12;
      if (r->jericho) { batch_rect(&g_b, bx, by, bw, bh, 0.05f, 0.1f, 0.25f, 0.7f); batch_rect(&g_b, bx + 2, by + 2, bw - 4, bh - 4, 0.15f, 0.3f, 0.6f, 0.7f); }
      if (r->jericho) { int n = r->shofars[vk], last = k->lap >= g_track.laps; if (last) snprintf(buf, sizeof buf, n > 0 ? "ZL: SHOUT!" : "SHOUTED!"); else snprintf(buf, sizeof buf, "SHOFAR x%d", n);
          batch_text(&g_b, buf, bx + bw / 2 - text_width(buf, 12 * sc) / 2, by + bh / 2 - 6 * sc, 12 * sc, 1, 0.9f, 0.4f, 1); }
      else hud_item_window(p, 16, vh - 64 * sc - 14, 64 * sc, 0);
      if (g_track.ncoin) { batch_rect(&g_b, bx + bw + 10, by + bh - 16 * sc, 12 * sc, 12 * sc, 1, 0.85f, 0.3f, 1); snprintf(buf, sizeof buf, "x%d", k->coins); batch_text(&g_b, buf, bx + bw + 26 * sc, by + bh - 18 * sc, 14 * sc, 1, 0.9f, 0.5f, 1); } }
    if (k->drifting) { float c = clampf(k->drift_charge / 1.6f, 0, 1), w = 140 * sc; batch_rect(&g_b, vw - 16 - w, vh - 24, w, 10, 0.1f, 0.1f, 0.1f, 0.6f);
        batch_rect(&g_b, vw - 16 - w, vh - 24, w * c, 10, c >= 1 ? 1.0f : 0.3f, c >= 0.5f ? 0.9f : 0.4f, 0.2f, 1); }
    if (r->state == RACE_COUNTDOWN) { int n = (int)ceilf(r->cd - 0.5f); if (n >= 1 && n <= 3) { snprintf(buf, sizeof buf, "%d", n); batch_text(&g_b, buf, vw / 2 - text_width(buf, 80 * sc) / 2, vh * 0.3f, 80 * sc, 1, 0.9f, 0.3f, 1); }
        else if (r->cd > 3.5f) { float a = clampf((r->cd - 3.5f) * 2.0f, 0, 1) * clampf((3.5f + RACE_READY_T - r->cd) * 3.0f, 0, 1); batch_text(&g_b, "GET READY", vw / 2 - text_width("GET READY", 44 * sc) / 2, vh * 0.3f, 44 * sc, 1, 0.95f, 0.6f, a); }
        if (p == 0 && !r->jericho) { const char *ref, *v = road_verse(g_def->prefix, &ref); float fs = 12 * sc; if (text_width(v, fs) > vw - 30) fs = 9 * sc; batch_text(&g_b, v, vw / 2 - text_width(v, fs) / 2, vh * 0.62f, fs, 1, 1, 1, clampf(r->cd * 1.2f, 0, 1)); } }
    if (g_pmsg_t[p] > 0) { float sz = 30 * sc; if (text_width(g_pmsg[p], sz) > vw - 40) sz = 22 * sc; float my = vh * 0.15f;   /* up over the sky, so the road stays in view */
        batch_text(&g_b, g_pmsg[p], vw / 2 - text_width(g_pmsg[p], sz) / 2, my, sz, 1, 0.95f, 0.4f, 1);
        if (g_pmsg2[p][0]) batch_text(&g_b, g_pmsg2[p], vw / 2 - text_width(g_pmsg2[p], 13 * sc) / 2, my + sz + 4, 13 * sc, 0.9f, 0.95f, 1, 1); }
    if (g_pside_t[p] > 0) { /* small note beside your kart: rises a little and fades */
        float sz = 14 * sc, a = clampf(g_pside_t[p] / 0.4f, 0, 1), rise = (1.8f - g_pside_t[p]) * 10 * sc, x = vw * 0.5f + 175 * sc, y = vh * 0.58f - rise, w = text_width(g_pside[p], sz);
        if (x + w > vw - 10) x = vw - 10 - w;
        batch_rect(&g_b, x - 6, y - 4, w + 12, sz + 8, 0.05f, 0.08f, 0.2f, 0.55f * a); batch_text(&g_b, g_pside[p], x, y, sz, 1, 0.92f, 0.5f, a); }
    if (k->wrongway_t > WRONGWAY_DELAY && ((int)(g_time * 4) & 1)) batch_text(&g_b, "WRONG WAY", vw / 2 - text_width("WRONG WAY", 36 * sc) / 2, vh * 0.5f, 36 * sc, 1, 0.3f, 0.2f, 1);
}
/* the fourth quarter of a three-player screen: the map and the standings */
static void hud_quarter_map(void) {
    Race *r = &g_race; float vw = 640, vh = 360;
    batch_rect(&g_b, 0, 0, vw, vh, 0.04f, 0.07f, 0.18f, 1);
    float mx = 170, mz = 180, sc = map_scale * 1.5f;
    for (int i = 0; i < g_track.n; i += 2) { TrackSample *sm = &g_track.s[i]; batch_rect(&g_b, mx + (sm->x - map_x0) * sc - 3, mz - (sm->z - map_z0) * sc - 3, 6, 6, 0.85f, 0.85f, 0.85f, 1); }
    for (int q = r->nk - 1; q >= 0; q--) { int i = r->order[q]; Kart *kk = &r->k[i]; const Character *c = &CHARACTERS[r->character[i]]; float sz = r->human[i] ? 14 : 9;
        batch_rect(&g_b, mx + (kk->x - map_x0) * sc - sz / 2, mz - (kk->z - map_z0) * sc - sz / 2, sz, sz, c->r, c->g, c->b, 1);
        if (r->human[i]) { int hp = race_human_index(r, i); batch_rect(&g_b, mx + (kk->x - map_x0) * sc - 4, mz - (kk->z - map_z0) * sc - 4, 8, 8, PLAYER_COL[hp][0], PLAYER_COL[hp][1], PLAYER_COL[hp][2], 1); } }
    batch_text(&g_b, "STANDINGS", 370, 16, 18, 1, 0.85f, 0.3f, 1);
    for (int q = 0; q < r->nk; q++) { int i = r->order[q]; const Character *c = &CHARACTERS[r->character[i]]; float y = 46 + q * 36; int hp = race_human_index(r, i);
        if (hp >= 0) batch_rect(&g_b, 362, y - 4, 270, 30, PLAYER_COL[hp][0], PLAYER_COL[hp][1], PLAYER_COL[hp][2], 0.3f);
        batch_text(&g_b, place_name(q + 1), 370, y, 18, 1, 1, 1, 1); batch_rect(&g_b, 430, y + 2, 16, 16, c->r, c->g, c->b, 1); batch_text(&g_b, c->short_name, 456, y, 18, 1, 1, 1, 1);
        if (hp >= 0) { char tag[8]; snprintf(tag, sizeof tag, "P%d", hp + 1); batch_text(&g_b, tag, 580, y, 16, PLAYER_COL[hp][0], PLAYER_COL[hp][1], PLAYER_COL[hp][2], 1); } }
}
void game_frame(const Pad *pads, int npads, float dt) {
    if (dt > 0.1f) dt = 0.1f;
    if (npads > KK_PADS) npads = KK_PADS;
#ifdef KK_TOUR
    static Pad tour[KK_PADS]; tour_pads(tour, g_time); pads = tour; npads = KK_PADS;
#endif
    Pad merged = merge_pads(pads, npads); const Pad *pad = &merged;     /* menus listen to every controller; a lone player can use any of them */
    g_frame_ms = dt * 1000.0f;
#if defined(KK_TOUR) && KK_TOUR_ROAD == 12
    { static int done = 0; if (!done && g_time > 2.0f) { done = 1;   /* CI: straight to a trophy ceremony, the field behind it is the demo race */
        g_mode = MODE_GP; g_cup = 0; g_gp_round = NUM_GP_TRACKS; memset(g_gp_points, 0, sizeof g_gp_points);
        for (int i = 0; i < g_race.nk; i++) g_gp_points[g_race.character[i]] = 60 - 7 * i;
        g_player_char = g_race.character[1]; g_screen = SCR_TROPHY; g_screen_t = 0; } }
#endif
    if (g_sting_due && g_race.state == RACE_COUNTDOWN && g_race.cd <= 4.85f) { g_sting_due = 0; sfx_play(SFX_COUNTDOWN, 1, 0); }   /* the sting's closing hit is 4.85 s in */
    if (g_race.state != RACE_COUNTDOWN) g_sting_due = 0;
    /* tire treads seen edge-on: at racing speed the real roll (dozens of grooves a second) would jump a whole groove or more
       between frames and look frozen, so the drawn roll is capped at 0.35 of a groove per frame - always visibly rolling */
    for (int i = 0; i < NKARTS && i < g_race.nk; i++) { float rate = g_race.k[i].speed / 0.42f / 6.2831853f * dt, cap = 0.35f * 0.25f / 4.0f;
        if (rate > cap) rate = cap; if (rate < -cap) rate = -cap; g_tread_ph[i] += rate; if (g_tread_ph[i] > 1000.0f) g_tread_ph[i] -= 1000.0f; }
    { /* speed report for the log file (the real Wii U's frame rate is only visible here): every 10 seconds */
      static float acc = 0, worst = 0; static int nf = 0, slow = 0; acc += dt; nf++; if (g_frame_ms > worst) worst = g_frame_ms; if (g_frame_ms > 17.5f) slow++;
      if (acc >= 10.0f) { gfx_log("speed: %.1f fps average, slowest frame %.1f ms, %d of %d frames over 1/60 s (screen %d)", nf / acc, worst, slow, nf, (int)g_screen); acc = 0; worst = 0; nf = 0; slow = 0; } }
    g_time += dt; g_screen_t += dt;
    Race *r = &g_race;
    cv = &g_view[0];
    /* button edges per controller slot (the join and character screens listen to each one) */
    int pa_s[KK_PADS], pb_s[KK_PADS], pplus_s[KK_PADS], mx_s[KK_PADS], my_s[KK_PADS];
    for (int sl = 0; sl < KK_PADS; sl++) { const Pad *q = sl < npads ? &pads[sl] : &g_prev_pads[sl]; Pad *pv = &g_prev_pads[sl];
        pa_s[sl] = q->a && !pv->a; pb_s[sl] = q->b && !pv->b; pplus_s[sl] = q->plus && !pv->plus;
        int qx = (q->sx > 0.5f) - (q->sx < -0.5f), qy = (q->sy > 0.5f) - (q->sy < -0.5f), vx = (pv->sx > 0.5f) - (pv->sx < -0.5f), vy = (pv->sy > 0.5f) - (pv->sy < -0.5f);
        mx_s[sl] = qx != vx ? qx : 0; my_s[sl] = qy != vy ? qy : 0;
        if (sl < npads) *pv = *q; }
    if (g_screen_t < 0.3f) for (int sl = 0; sl < KK_PADS; sl++) pa_s[sl] = pb_s[sl] = pplus_s[sl] = mx_s[sl] = my_s[sl] = 0;
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
    case SCR_TITLE: if (pa || pb || pplus || pad->x || pad->y) { g_screen = SCR_MAIN; g_screen_t = 0; g_menu_sel = 0; sfx_play(SFX_MENU_OK, 1, 0); }
#ifndef KK_TOUR
        else if (g_screen_t > 90.0f && g_op_frames > 0 && m_op) { g_screen = SCR_OPENER; g_screen_t = 0; g_op_cur = -1; }   /* left alone, the title screen plays the opener again */
#endif
        break;
    case SCR_OPENER: {
        if (g_op_cur == -1 && !g_op_vid && g_op_buf == 0) { g_op_vid = vid_open("video/opener_", g_op_frames, g_op_w, g_op_h); g_op_buf = 0.0001f; }
        if (g_op_vid && g_op_buf < 2.0f) {   /* fill the decoder a little before the clock (and the music) starts */
            int ready = vid_update(0); g_op_buf += dt; g_screen_t = 0; if (ready >= 6 || g_op_buf >= 2.0f) g_op_buf = 2.0f; }
        int want = (int)(g_screen_t * g_op_fps);
        if (pa || pb || pplus || pad->x || pad->y || want >= g_op_frames) { g_screen = SCR_TITLE; g_screen_t = 0; gfx_texture_free(tx_op); tx_op = NULL; g_op_cur = -1; if (g_op_vid) vid_close(); g_op_vid = 0; g_op_buf = 0; break; }
        if (g_op_vid) { vid_update(want); g_op_cur = want; break; }
        if (want != g_op_cur) { char rel[64]; snprintf(rel, sizeof rel, "video/opener_%03d.jpg", want); Texture *nt = asset_texture_file(rel, 0, NULL, NULL);
            if (nt) { gfx_texture_free(tx_op); tx_op = nt; } g_op_cur = want; }
        break; }
    case SCR_OPTIONS:
        if (my) { g_opt_sel = (g_opt_sel - my + 4) % 4; sfx_play(SFX_MENU_MOVE, 1, 0); g_erase_hold = 0; }
        if (g_opt_sel == 3) { if (pad->a) { g_erase_hold += dt; if (g_erase_hold >= 2.0f) { int mv = g_save.music_vol, sv = g_save.sfx_vol, rb = g_save.rumble; memset(&g_save, 0, sizeof g_save); g_save.music_vol = mv; g_save.sfx_vol = sv; g_save.rumble = rb; g_save.dirty = 1; save_store();
                    for (int d = 0; d < 8; d++) g_best_time[d] = 0; g_ghost_n = 0; race_set_unlocked(unlocked_mask()); race_set_styles(0, unlocked_styles()); g_erase_hold = 0; g_opt_sel = 0; sfx_play(SFX_WRONG_WAY, 1, 0); } } else g_erase_hold = 0; pa = 0; }
        if (mx && g_opt_sel < 2) { int *v = g_opt_sel == 0 ? &g_save.music_vol : &g_save.sfx_vol; *v = *v + mx < 0 ? 0 : (*v + mx > 10 ? 10 : *v + mx); g_save.dirty = 1; apply_volumes(); sfx_play(SFX_MENU_MOVE, 1, 0); }
        if ((mx || pa) && g_opt_sel == 2) { g_save.rumble = !g_save.rumble; g_save.dirty = 1; sfx_play(SFX_MENU_MOVE, 1, 0); if (g_save.rumble) platform_rumble(0.6f, 0.25f); }
        if (pb || (pa && g_opt_sel != 2)) { save_store(); g_screen = SCR_MAIN; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_CREDITS:
        if (pa || pb || g_screen_t * CREDITS_SPEED > credits_height() + 760) { g_screen = SCR_TITLE; g_screen_t = 0; if (pa || pb) sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_MAIN:
        if (my) { g_menu_sel = (g_menu_sel - my + 9) % 9; sfx_play(SFX_MENU_MOVE, 1, 0); }
        if (g_menu_sel == 6 && mx) g_difficulty = (g_difficulty + mx + 3) % 3;
        if (pa) {
            if (g_menu_sel == 8) { g_screen = SCR_CREDITS; g_screen_t = 0; sfx_play(SFX_MENU_OK, 1, 0); }
            else if (g_menu_sel == 7) { g_screen = SCR_OPTIONS; g_opt_sel = 0; g_screen_t = 0; sfx_play(SFX_MENU_OK, 1, 0); }
            else if (g_menu_sel == 6) { g_difficulty = (g_difficulty + 1) % 3; sfx_play(SFX_MENU_OK, 1, 0); }
            else if (g_menu_sel == 5) { sfx_play(SFX_MENU_OK, 1, 0); g_mode = MODE_JERICHO; g_screen = SCR_CHAR; g_screen_t = 0; }
            else if (g_menu_sel == 4) { sfx_play(SFX_MENU_OK, 1, 0); g_mode = MODE_VS; memset(g_joined, 0, sizeof g_joined); g_njoined = 0; g_screen = SCR_JOIN; g_screen_t = 0; }
            else if (g_menu_sel == 3) { sfx_play(SFX_MENU_OK, 1, 0); g_mode = MODE_BATTLE; g_player_char = 2; begin_battle(); g_screen_t = 0; }
            else { sfx_play(SFX_MENU_OK, 1, 0); g_mode = g_menu_sel == 0 ? MODE_GP : (g_menu_sel == 1 ? MODE_SINGLE : MODE_TT); g_screen = g_mode == MODE_GP ? SCR_CUP : SCR_CHAR; g_screen_t = 0; } }
        if (pb) { g_screen = SCR_TITLE; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_JOIN:
        for (int sl = 0; sl < npads; sl++) {
            if (pa_s[sl] && !g_joined[sl] && g_njoined < MAX_HUMANS && pads[sl].source) { g_joined[sl] = ++g_njoined; sfx_play(SFX_MENU_OK, 1, 0); }
            else if (pb_s[sl] && g_joined[sl]) { int gone = g_joined[sl]; g_joined[sl] = 0; g_njoined--; for (int o = 0; o < KK_PADS; o++) if (g_joined[o] > gone) g_joined[o]--; sfx_play(SFX_MENU_BACK, 1, 0); }
            else if (pb_s[sl] && g_njoined == 0) { g_mode = MODE_SINGLE; g_screen = SCR_MAIN; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
            else if (pplus_s[sl] && g_joined[sl] && g_njoined >= 2) { sfx_play(SFX_MENU_OK, 1, 0);
                for (int o = 0; o < KK_PADS; o++) if (g_joined[o]) g_pslot[g_joined[o] - 1] = o;
                for (int p = 0; p < g_njoined; p++) { g_pchar[p] = p % NMAIN; g_plock[p] = 0; }
                g_screen = SCR_MCHAR; g_screen_t = 0; break; } }
        break;
    case SCR_MCHAR: {
        int all = g_njoined >= 2;
        for (int p = 0; p < g_njoined; p++) { int sl = g_pslot[p]; int qx = mx_s[sl], qy = my_s[sl];
            if (!g_plock[p] && qx) { int row = g_pchar[p] / 5, ncol = row == 2 ? NCHARS - 10 : 5; g_pchar[p] = row * 5 + ((g_pchar[p] % 5) + qx + ncol) % ncol; sfx_play(SFX_MENU_MOVE, 1, 0); }
            if (!g_plock[p] && qy) { int nc = g_pchar[p] - qy * 5; if (nc < 0) nc += 15; if (nc >= 15) nc -= 15; if (nc >= NCHARS) nc = qy < 0 ? nc - 5 : nc % 5; g_pchar[p] = nc; sfx_play(SFX_MENU_MOVE, 1, 0); }
            if (pa_s[sl] && !g_plock[p]) { if (char_unlocked(g_pchar[p])) { g_plock[p] = 1; sfx_play(SFX_MENU_OK, 1, 0); sfx_voice_play(g_voice[g_pchar[p]][VO_START], 1, 0); } else sfx_play(SFX_WRONG_WAY, 1, 0); }
            else if (pb_s[sl] && g_plock[p]) { g_plock[p] = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
            else if (pb_s[sl] && p == 0) { g_screen = SCR_JOIN; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
            if (!g_plock[p]) all = 0; }
        if (all && g_screen == SCR_MCHAR && g_screen_t > 0.6f) { g_screen = SCR_TRACK; g_screen_t = 0; sfx_play(SFX_MENU_OK, 1, 0); }
        break; }
    case SCR_CUP: {
        int open = !CUPS[g_cup].needs_heaven || (g_save.unlocks & UNLOCK_HEAVEN);
        if (mx) { g_cup = (g_cup + mx + 2) % 2; sfx_play(SFX_MENU_MOVE, 1, 0); }
        if (pa) { if (open) { g_screen = SCR_CHAR; g_screen_t = 0; sfx_play(SFX_MENU_OK, 1, 0); } else sfx_play(SFX_WRONG_WAY, 1, 0); }
        if (pb) { g_screen = SCR_MAIN; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        break; }
    case SCR_CHAR:
        if (mx) { int row = g_player_char / 5, ncol = row == 2 ? NCHARS - 10 : 5; g_player_char = row * 5 + ((g_player_char % 5) + mx + ncol) % ncol; sfx_play(SFX_MENU_MOVE, 1, 0); }
        if (my) { int nc = g_player_char - my * 5; if (nc < 0) nc += 15; if (nc >= 15) nc -= 15; if (nc >= NCHARS) nc = my < 0 ? nc - 5 : nc % 5; g_player_char = nc; sfx_play(SFX_MENU_MOVE, 1, 0); }
        if (pa) sfx_play(char_unlocked(g_player_char) ? SFX_MENU_OK : SFX_WRONG_WAY, 1, 0);
        if (pa && char_unlocked(g_player_char)) { g_screen = SCR_KART; g_screen_t = 0; sfx_voice_play(g_voice[g_player_char][VO_START], 1, 0); }
        if (pb) { g_screen = g_mode == MODE_GP ? SCR_CUP : SCR_MAIN; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_KART:
        if (mx) { g_player_style = (g_player_style + mx + NSTYLES) % NSTYLES; sfx_play(SFX_MENU_MOVE, 1, 0); }
        if (pa) sfx_play(style_unlocked(g_player_style) ? SFX_MENU_OK : SFX_WRONG_WAY, 1, 0);
        if (pa && style_unlocked(g_player_style)) { race_set_styles(g_player_style, unlocked_styles());
            if (g_mode == MODE_GP) { memset(g_gp_points, 0, sizeof g_gp_points); g_gp_round = 0; for (int i = 0; i < NUM_GP_TRACKS; i++) g_gp_order[i] = CUPS[g_cup].roads[i]; begin_player_race(); g_screen_t = 0; }
            else if (g_mode == MODE_JERICHO) { begin_player_race(); g_screen_t = 0; } else { if (g_track_idx >= NUM_PLAY_TRACKS) { set_track(0); start_race(1); } g_screen = SCR_TRACK; g_screen_t = 0; } }
        if (pb) { g_screen = SCR_CHAR; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_TRACK:
        if (mx) { sfx_play(SFX_MENU_MOVE, 1, 0); int n = ncards(), d = card_track((track_card(g_track_idx) + mx + n) % n); g_track_idx = d; if (track_unlocked(d)) { set_track(d); start_race(1); } }
        if (pa) sfx_play(track_unlocked(g_track_idx) ? SFX_MENU_OK : SFX_WRONG_WAY, 1, 0);
        if (pa && track_unlocked(g_track_idx)) { begin_player_race(); g_screen_t = 0; }
        if (pb) { g_screen = SCR_KART; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_RACE:
        if (pplus && r->state != RACE_FINISHED && g_screen_t > 0.5f) { g_screen = SCR_PAUSE; g_pause_sel = 0; g_screen_t = 0; g_pause_reason[0] = 0; }
        if (g_nplayers > 1 && r->state != RACE_FINISHED && g_screen_t > 2.0f) for (int p = 0; p < g_nplayers; p++) { int sl = g_pslot[p]; if (sl < npads && !pads[sl].source) {   /* a controller went away */
            g_screen = SCR_PAUSE; g_pause_sel = 0; g_screen_t = 0; snprintf(g_pause_reason, sizeof g_pause_reason, "PLAYER %d'S CONTROLLER DISCONNECTED", p + 1); break; } }
        if (r->state == RACE_FINISHED) {
            g_results_t += dt;
            if (g_results_t > 1.5f) {
                if (g_mode == MODE_GP) { race_rank(r); for (int p = 0; p < r->nk; p++) { Kart *q = &r->k[r->order[p]]; int pl = q->place ? q->place : p + 1; g_gp_points[r->character[r->order[p]]] += GP_POINTS[pl - 1]; }
                    int best = -1, bg = 0; for (int i = 0; i < r->nk; i++) if (r->given[i] > bg) { bg = r->given[i]; best = i; } if (best >= 0) g_gp_points[r->character[best]] += 2; }
                if (g_mode == MODE_TT) { Kart *k = &r->k[r->player]; if (k->lap > g_track.laps && (g_best_time[g_track_idx] <= 0 || k->total_time < g_best_time[g_track_idx])) { g_best_time[g_track_idx] = k->total_time; if (g_track_idx < SAVE_MAX_ROADS) { g_save.best_ms[g_track_idx] = (int)(k->total_time * 1000); g_save.dirty = 1; } if (g_rec_n > 0) ghost_save(g_track_idx); } }
                else if (!g_demo && g_mode != MODE_BATTLE && g_mode != MODE_VS) { Kart *k = &r->k[r->player]; g_save.races++; if (k->place == 1) { g_save.wins++; g_save.win_chars |= 1u << g_player_char; } g_save.dirty = 1; }
                g_fresh_unlocks = 0;
                if (g_mode == MODE_BATTLE && !g_demo) { g_save.battles++; if (g_battle.won) g_save.battle_wins++; g_save.dirty = 1; }
                if (g_mode == MODE_JERICHO && !g_demo) { Kart *k = &r->k[r->player]; if (k->place == 1) { g_save.jericho_wins++; if (k->lap > g_track.laps && (g_save.jericho_ms == 0 || (int)(k->total_time * 1000) < g_save.jericho_ms)) g_save.jericho_ms = (int)(k->total_time * 1000); } g_save.dirty = 1; }
                if (g_mode == MODE_GP && g_gp_round + 1 >= NUM_GP_TRACKS && !g_demo) {   /* the championship is decided */
                    int best = 0; for (int c = 1; c < NCHARS; c++) if (g_gp_points[c] > g_gp_points[best]) best = c;
                    if (best == g_player_char) { g_save.gp_wins[g_difficulty]++; if (g_cup == 1) g_save.glory_wins++; g_save.dirty = 1; } }
                g_fresh_unlocks = save_check_unlocks(); save_store(); race_set_unlocked(unlocked_mask()); race_set_styles(g_player_style, unlocked_styles());
                { int hwon = 0; for (int i = 0; i < r->nk; i++) if (r->human[i] && r->k[i].place == 1) hwon = 1;
                  if (g_fresh_unlocks) sfx_play(SFX_UNLOCK, 1, 0); else if (!g_demo && hwon) sfx_play(SFX_WIN, 1, 0); }
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
            else { if (g_mode == MODE_BATTLE || g_mode == MODE_JERICHO || g_track_idx >= NUM_PLAY_TRACKS) set_track(0); if (g_mode == MODE_VS || g_mode == MODE_JERICHO) g_mode = MODE_SINGLE; start_race(1); g_screen = SCR_MAIN; g_screen_t = 0; }
        }
        if (pb) { g_screen = SCR_RACE; g_screen_t = 0; save_store(); }
        break;
    case SCR_RESULTS:
        if (pa) { if (g_mode == MODE_GP) { g_gp_round++; g_screen = SCR_GP; g_screen_t = 0; } else if (g_mode == MODE_BATTLE) { begin_battle(); g_screen_t = 0; } else { start_race(0); g_screen = SCR_RACE; g_screen_t = 0; } }
        if (pb && g_mode != MODE_GP) { set_track(0); if (g_mode == MODE_VS || g_mode == MODE_JERICHO) g_mode = MODE_SINGLE; start_race(1); g_screen = SCR_MAIN; g_screen_t = 0; }
        break;
    case SCR_GP:
        if (pa) { if (g_gp_round >= NUM_GP_TRACKS) { g_screen = SCR_TROPHY; g_screen_t = 0; } else { begin_player_race(); g_screen_t = 0; } }
        break;
    case SCR_TROPHY: {
        static float last_t = 99; float t0 = last_t; last_t = g_screen_t; int ord[NKARTS]; cer_order(ord);
        if (t0 < CER_T[0] + 0.5f && g_screen_t >= CER_T[0] + 0.5f) { sfx_play(SFX_WIN, 1, 0); int hv = g_voice[ord[0]][VO_WIN]; if (hv >= 0) sfx_voice_play(hv, 1.0f, 0); }
        if (t0 < 3.4f && g_screen_t >= 3.4f) sfx_play(SFX_UNLOCK, 1, 0);
        for (int p = 1; p < 3; p++) if (t0 < CER_T[p] + 0.5f && g_screen_t >= CER_T[p] + 0.5f) sfx_play(SFX_LAND, 0.8f, 0);
        if (pa && g_screen_t > 5.5f) { start_race(1); g_screen = SCR_MAIN; g_screen_t = 0; }
        else if (pa && g_screen_t < 5.5f) g_screen_t = 5.5f; }   /* A skips ahead to the end of the show */
        break;
    }
    /* the attract-mode race restarts itself when it ends */
    if (g_attract && r->state == RACE_FINISHED) { g_results_t += dt; if (g_results_t > 3.0f) {
        if (g_screen == SCR_TITLE || g_screen == SCR_MAIN || g_screen == SCR_CREDITS || g_screen == SCR_OPTIONS) {   /* the demo moves on to another road with another driver */
            int d = g_track_idx; for (int tries = 0; tries < NUM_PLAY_TRACKS; tries++) { d = (d + 1) % NUM_PLAY_TRACKS; if (track_unlocked(d)) break; }
            g_demo_char = (g_demo_char + 5) % NCHARS; while (!char_unlocked(g_demo_char)) g_demo_char = (g_demo_char + 1) % NCHARS;
            set_track(d); }
        start_race(1); } }
    int in_race = g_screen == SCR_RACE || g_screen == SCR_PAUSE || g_screen == SCR_RESULTS || g_screen == SCR_GP;
    if (in_race && !g_attract && g_mode != MODE_VS) {
        int real_input = pad->source && (fabsf(pad->sx) > 0.15f || pad->a || pad->b || pad->zr || pad->zl);
        if (real_input) { g_idle = 0; g_demo = 0; } else { g_idle += dt; if (g_idle > 8.0f) g_demo = 1; }
    }
    if (g_screen == SCR_RACE && r->state == RACE_RUNNING && !g_attract && g_mode != MODE_BATTLE) {   /* taking (or losing) the lead */
        static int last_pos[MAX_HUMANS]; for (int p = 0; p < g_nplayers; p++) { int vk = human_kart(p), pos = 0; for (int q = 0; q < r->nk; q++) if (r->order[q] == vk) pos = q + 1;
            if (r->race_t < 3.0f) last_pos[p] = pos;
            else if (pos != last_pos[p] && r->nk > 1 && !r->k[vk].finished) { if (pos == 1) { pmsg(p, 1.2f, "YOU TOOK THE LEAD!"); sfx_play(SFX_LAP, 0.7f, 0); } else if (last_pos[p] == 1) pmsg(p, 1.0f, "YOU LOST THE LEAD"); last_pos[p] = pos; } } }
    for (int p = 0; p < g_nplayers; p++) {
        if (g_pmsg_t[p] > 0) { g_pmsg_t[p] -= dt; if (g_pmsg_t[p] <= 0) g_pmsg2[p][0] = 0; }
        if (g_pside_t[p] > 0) g_pside_t[p] -= dt;
        if (g_coin_pop[p] > 0) g_coin_pop[p] -= dt;
        if (g_rou_land[p] < 99) g_rou_land[p] += dt;
        if (g_proulette[p] > 0) { g_proulette[p] -= dt;
            int pos = (int)rou_phase(p); if (pos != g_rou_last[p]) { g_rou_last[p] = pos; sfx_play(SFX_MENU_MOVE, 0.45f, 0); }   /* a tick as each picture rolls past; they slow as it settles */
            if (g_proulette[p] <= 0) { g_proulette[p] = 0; g_rou_land[p] = 0; Kart *pk = &g_race.k[human_kart(p)]; if (pk->item) sfx_play(SFX_MENU_OK, 0.9f, 0); } } }
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
    KartInput hin[NKARTS]; memset(hin, 0, sizeof hin); int all_auto = g_demo || g_attract;
    if (g_nplayers > 1 && g_screen == SCR_RACE) for (int p = 0; p < g_nplayers; p++) { const Pad *pp = player_pad(pads, npads, p); KartInput *h = &hin[human_kart(p)];
        h->steer = -pp->sx; h->gas = pp->a; h->brake = pp->b; h->drift = pp->zr; h->item = pp->zl || pp->x || pp->y; }
#if defined(KK_TOUR) && KK_TOUR_ROAD == 8
    all_auto = 1;   /* the CI tour has nobody holding the controllers: the four people drive themselves */
#endif
    int steps = 0;
    while (acc >= SIM_DT && steps < 8) {
        if (g_mode == MODE_BATTLE) {
            battle_step(&g_battle, &in, g_demo || g_attract, SIM_DT);
            for (int e = 0; e < g_battle.nev; e++) switch (g_battle.ev[e]) {
                case BEV_STONE_PICKED: sfx_play(SFX_COIN, 0.8f, 0); break;
                case BEV_STONE_THROWN: sfx_play(SFX_THROW, 1, 0); break;
                case BEV_SHIELD: snprintf(g_msg, sizeof g_msg, "THE SHIELD-BEARER!"); snprintf(g_msg2, sizeof g_msg2, "HIT HIM FROM THE SIDE - 1 SAMUEL 17:41"); g_msg_t = 1.2f; sfx_play(SFX_BLOCK, 1, 0); break;
                case BEV_GOLIATH_HIT: snprintf(g_msg, sizeof g_msg, "HIT!"); g_msg_t = 0.8f; sfx_play(SFX_HIT, 1, 0); add_dust(v3(g_battle.gx, 2, g_battle.gz), 3, 3); break;
                case BEV_DAVID_HIT: snprintf(g_msg, sizeof g_msg, "OUCH!"); g_msg_t = 0.8f; sfx_play(SFX_HIT, 1, 0); platform_rumble(1.0f, 0.45f); break;
                case BEV_SPEAR: sfx_play(SFX_THROW, 0.7f, 0); break;
                case BEV_ROAR: sfx_play(SFX_SHOFAR, 0.6f, 0); break;
                case BEV_CHARGE: snprintf(g_msg, sizeof g_msg, "HE CHARGES!"); g_msg_t = 1.0f; sfx_play(SFX_FIRE, 0.7f, 0); break;
                case BEV_WIN: snprintf(g_msg, sizeof g_msg, "GIANT SLAYER!"); g_msg_t = 3.0f; sfx_play(SFX_WIN, 1, 0); break;
                case BEV_STEP: { float ddx = g_battle.gx - r->k[0].x, ddz = g_battle.gz - r->k[0].z, dd = sqrtf(ddx * ddx + ddz * ddz); g_shake = fmaxf(g_shake, clampf(1.2f - dd / 50.0f, 0.1f, 1.0f) * (g_battle.gstate == GOL_CHARGE ? 0.5f : 0.3f)); sfx_play(SFX_BUMP, clampf(1.5f - dd / 60.0f, 0.2f, 1.0f), 0); platform_rumble(clampf(1.2f - dd / 50.0f, 0.1f, 0.8f), 0.15f); } break;
                case BEV_LOSE: snprintf(g_msg, sizeof g_msg, "GOLIATH WINS"); g_msg_t = 3.0f; sfx_play(SFX_WRONG_WAY, 1, 0); break;
                default: break; }
        } else { if (g_nplayers > 1) race_step_multi(r, hin, all_auto, SIM_DT); else race_step(r, &in, all_auto, SIM_DT);
            if (g_mode == MODE_TT && !g_attract && r->state == RACE_RUNNING) { g_rec_t += SIM_DT;   /* the ghost recording */
                if (g_rec_t >= 1.0f / GHOST_HZ && g_rec_n < GHOST_MAX) { g_rec_t -= 1.0f / GHOST_HZ; Kart *pk = &r->k[r->player]; GhostSample *gs = &g_rec[g_rec_n++];
                    gs->x = (int16_t)(pk->x * 16.0f); gs->z = (int16_t)(pk->z * 16.0f); gs->yaw = (int16_t)(pk->yaw * 10000.0f); } } }
        acc -= SIM_DT; steps++;
        for (int e = 0; e < r->nev && !g_attract; e++) {
            RaceEvent *ev = &r->ev[e]; int who = ev->kart >= 0 ? race_human_index(r, ev->kart) : -1;   /* which player's screen gets the message (-1: a computer driver) */
            Kart *ek = ev->kart >= 0 ? &r->k[ev->kart] : NULL;
            switch (ev->type) {
                case EV_COUNT_TICK: sfx_play(SFX_COUNT_TICK, 1, 0); break;
                case EV_GO: for (int p = 0; p < g_nplayers; p++) { pmsg(p, 1.0f, "GO!"); if (p == 0 || frand01() < 0.5f) voice_at(human_kart(p), VO_START); } sfx_play(SFX_GO, 1, 0); break;
                case EV_LAP: if (who >= 0) sfx_play(SFX_LAP, 1, 0); break;
                case EV_FINAL_LAP: if (who >= 0) { pmsg(who, 2.0f, "FINAL LAP!"); sfx_play(SFX_FINAL_LAP, 1, 0); } break;
                case EV_FINISH: if (ek->place == 1) voice_at(ev->kart, VO_WIN);
                    if (who >= 0) { pmsg(who, 3.0f, g_def->grid_idx ? "WELCOME HOME!" : "FINISH!"); sfx_play(g_def->grid_idx ? SFX_WELCOME_HOME : SFX_FINISH, 1, 0);
                    if (!g_attract && !g_demo) { if (g_nplayers <= 1) { g_slowmo = 1.1f; g_view[0].orbit = 0; } g_view[who].finish_t = 0.001f; g_view[who].orbit = 0; } } break;
                case EV_HIT: voice_at(ev->kart, VO_OUCH); if (who >= 0) { pmsg(who, 0.8f, "OUCH!"); prumble(who, 1.0f, 0.45f); } add_dust(v3(ek->x, ek->y, ek->z), 2.0f, 3.0f); sfx_at(SFX_HIT, ek, 1.0f); break;
                case EV_BLOCK: if (who >= 0) pmsg(who, 0.8f, "BLOCKED!"); sfx_at(SFX_BLOCK, ek, 0.9f); break;
                case EV_BUMP: sfx_at(SFX_BUMP, ek, 0.7f); if (who >= 0) prumble(who, 0.4f, 0.12f); break;
                case EV_TRICK: if (who >= 0) { pmsg(who, 0.6f, "TRICK!"); sfx_play(SFX_MINI_TURBO, 0.5f, 0); } break;
                case EV_TRICK_LAND: voice_at(ev->kart, VO_TRICK); if (who >= 0) { pmsg(who, 0.9f, "NICE LANDING!"); sfx_play(SFX_MINI_TURBO, 0.9f, 0); prumble(who, 0.5f, 0.15f); } add_dust(v3(ek->x, ek->y, ek->z), 1.0f, 2.0f); break;
                case EV_ROCKET_START: voice_at(ev->kart, VO_BOOST); if (who >= 0) { pmsg(who, 1.2f, "ROCKET START!"); sfx_play(SFX_SUPER_TURBO, 1, 0); prumble(who, 0.7f, 0.35f); } break;
                case EV_WHEELSPIN: if (who >= 0) { pmsg(who, 1.2f, "TOO EARLY!"); sfx_play(SFX_WRONG_WAY, 0.8f, 0); prumble(who, 0.3f, 0.5f); } add_dust(v3(ek->x, ek->y, ek->z), 1.5f, 2.5f); break;
                case EV_PICKUP: { rainbow_burst(v3(ek->x, ek->y, ek->z));
                    if (who >= 0) { g_proulette[who] = ROU_T; g_prou_tick[who] = 0; rou_start(who, g_race.k[human_kart(who)].item); sfx_play(SFX_PICKUP, 1, 0); } else sfx_at(SFX_PICKUP, ek, 0.5f); } break;
                case EV_USE_MANNA: sfx_at(SFX_USE_ITEM, ek, 0.9f); if (who >= 0) pmsg(who, 0.8f, "MANNA!"); break;
                case EV_USE_LOAVES: voice_at(ev->kart, VO_BLESS); sfx_at(SFX_COIN, ek, 1.0f); if (who >= 0) pmsg(who, 1.2f, "BREAD FOR EVERYONE!"); for (int q2 = 0; q2 < r->nk; q2++) bless_sparkle(&r->k[q2], 1, 0.85f, 0.3f); break;
                case EV_USE_SAMARITAN: sfx_at(SFX_UNLOCK, ek, 0.8f); break;
                case EV_CARRIED: if (who >= 0) { pmsg(who, 1.5f, "CARRIED BY THE SAMARITAN!"); prumble(who, 0.5f, 0.3f); } bless_sparkle(ek, 1, 1, 1); break;
                case EV_USE_SHIELD: sfx_at(SFX_BLOCK, ek, 0.7f); if (who >= 0) pmsg(who, 1.0f, "SHIELD OF FAITH!"); break;
                case EV_USE_PARTING: voice_at(ev->kart, VO_BLESS); sfx_at(SFX_SPLASH, ek, 1.0f); sfx_play(SFX_SHOFAR, 0.4f, 0); if (who >= 0) pmsg(who, 1.5f, "THE SEA PARTS!"); for (int p = 0; p < g_nplayers; p++) g_view[p].shake = fmaxf(g_view[p].shake, 0.4f); break;
                case EV_USE_JUBILEE: voice_at(ev->kart, VO_BLESS); sfx_at(SFX_SHOFAR, ek, 1.0f); if (who >= 0) pmsg(who, 1.5f, "JUBILEE!"); for (int q2 = 0; q2 < r->nk; q2++) bless_sparkle(&r->k[q2], 1, 0.95f, 0.6f); break;
                case EV_USE_FIRE: voice_at(ev->kart, VO_FIRE); sfx_at(SFX_FIRE, ek, 1.0f); if (who >= 0) { pmsg(who, 1.5f, "CHARIOT OF FIRE!"); prumble(who, 0.6f, 0.4f); } break;
                case EV_USE_WATER: sfx_at(SFX_SPLASH, ek, 0.8f); if (who >= 0) pmsg(who, 1.0f, "LIVING WATER!"); bless_sparkle(ek, 0.5f, 0.8f, 1); break;
                case EV_BLESSED: { int by = r->blessed_by[ev->kart]; if (who >= 0 && by >= 0 && by < r->nk) { char line[64]; snprintf(line, sizeof line, "BLESSED BY %s", CHARACTERS[r->character[by]].short_name); pside(who, 1.8f, line); } bless_sparkle(ek, 1, 0.9f, 0.5f); } break;
                case EV_TRAIL_BOOST: if (who >= 0) pmsg(who, 1.0f, "BLESSING ON THE ROAD!"); sfx_at(SFX_BOOST_PAD, ek, 0.7f); break;
                case EV_CHEEK: if (who >= 0) pmsg(who, 1.0f, "THE OTHER CHEEK!"); break;
                case EV_PAD_BOOST: if (frand01() < 0.5f) voice_at(ev->kart, VO_BOOST); if (who >= 0) { pmsg(who, 0.7f, "BOOST!"); prumble(who, 0.35f, 0.2f); } add_dust(v3(ek->x, ek->y, ek->z), 1.0f, 2.0f); sfx_at(SFX_BOOST_PAD, ek, 0.9f); break;
                case EV_PAD_BOUNCE: if (who >= 0) pmsg(who, 0.7f, "BOING!"); sfx_at(SFX_BOUNCE, ek, 0.9f); break;
                case EV_COIN: if (!ek) break; bless_sparkle(ek, 1, 0.85f, 0.25f); if (who >= 0) { sfx_play(SFX_COIN, 1.0f, 0); g_coin_pop[who] = 0.5f; char cl[32]; if (ek->coins >= 10) snprintf(cl, sizeof cl, "COINS FULL - TOP SPEED +10%%"); else snprintf(cl, sizeof cl, "+1 COIN  (%d/10)", ek->coins); pside(who, 0.9f, cl); } break;
                case EV_SLOWED: if (who >= 0) pmsg(who, 1.0f, "SLOWED!"); break;
                case EV_SHOFAR: sfx_at(SFX_SHOFAR, ek, 0.9f); if (who >= 0) { pmsg(who, 0.9f, "SHOFAR!"); prumble(who, 0.3f, 0.2f); } break;
                case EV_SHOUT: sfx_at(SFX_SHOUT, ek, 1.0f); if (who >= 0) { pmsg(who, 1.6f, "THE GREAT SHOUT!"); pmsg2(who, 1.6f, "JOSHUA 6:20"); prumble(who, 0.8f, 0.5f); } for (int p = 0; p < g_nplayers; p++) g_view[p].shake = fmaxf(g_view[p].shake, 0.5f); break;
                case EV_WALLS_DOWN: sfx_play(SFX_CRUMBLE, 1, 0); for (int p = 0; p < g_nplayers; p++) { pmsg(p, 3.0f, "THE WALLS CAME DOWN!"); pmsg2(p, 3.0f, "AND THE WALL FELL DOWN FLAT - JOSHUA 6:20"); g_view[p].shake = fmaxf(g_view[p].shake, 0.9f); prumble(p, 1.0f, 0.8f); } break;
                default: break;
            }
        }
        for (int i = 0; i < r->nk; i++) {
            Kart *q = &r->k[i]; int who = race_human_index(r, i);
            float hx = sinf(q->yaw + q->slide), hz = cosf(q->yaw + q->slide), rx = cosf(q->yaw + q->slide), rz = -sinf(q->yaw + q->slide);
            if (q->skid && !q->air) add_skid(i, v3(q->x - rx * 0.85f - hx * 0.8f, q->y + 0.04f, q->z - rz * 0.85f - hz * 0.8f), v3(q->x + rx * 0.85f - hx * 0.8f, q->y + 0.04f, q->z + rz * 0.85f - hz * 0.8f));
            else g_last_skid[i] = 0;
            if (who >= 0 && q->mini_turbo_tier && !g_attract) { if (q->mini_turbo_tier == 2) voice_at(i, VO_BOOST); pmsg(who, 1.2f, q->mini_turbo_tier == 2 ? "SUPER TURBO!" : "MINI TURBO!"); sfx_play(q->mini_turbo_tier == 2 ? SFX_SUPER_TURBO : SFX_MINI_TURBO, 1, 0); }
            if (who >= 0 && !g_attract && g_was_air[i] && !q->air) { sfx_play(g_track.water && !q->hit.on_road ? SFX_SPLASH : SFX_LAND, 0.8f, 0); prumble(who, 0.3f, 0.1f); }
            g_was_air[i] = q->air;
            g_wheel_ang[i] += q->speed / 0.42f * SIM_DT;   /* the wheels turn with the distance driven (drawn spinning in every view) */
        }
    }
    if (steps == 8) acc = 0;
    Kart *k = &r->k[r->player];
    if (sim_running) for (int i = 0; i < MAX_RING; i++) if (g_ring[i].t > 0) g_ring[i].t -= dt;
    if (r->jericho && sim_running) {   /* the walls of Jericho come down one stretch after another as wall_fall climbs */
        for (int i = 0; i < g_nplaced; i++) { Placed *pl = &g_placed[i]; if (!pl->wall) continue;
            float due = (float)(pl->wall - 1) / (float)(g_nwalls > 0 ? g_nwalls : 1);
            if (pl->fall_t <= 0 && r->wall_fall > due && r->wall_fall > 0.001f) { pl->fall_t = 0.001f;
                for (int n = 0; n < 10; n++) add_dust(v3(pl->p.x + (frand01() - 0.5f) * pl->w, pl->p.y + frand01() * pl->h * 0.5f, pl->p.z), 3.0f, 2.5f);
                float dx = pl->p.x - k->x, dz = pl->p.z - k->z, d = sqrtf(dx * dx + dz * dz); float near = clampf(1.3f - d / 90.0f, 0.15f, 1.0f);
                sfx_play(SFX_CRUMBLE, near, 0); for (int p = 0; p < g_nplayers; p++) g_view[p].shake = fmaxf(g_view[p].shake, 0.35f * near); if (near > 0.6f) prumble(0, near * 0.6f, 0.4f); }
            else if (pl->fall_t > 0 && pl->fall_t < 2.0f) { pl->fall_t += dt; if (pl->fall_t < 1.2f && frand01() < 0.5f) add_dust(v3(pl->p.x + (frand01() - 0.5f) * pl->w, pl->p.y + 0.3f, pl->p.z), 3.0f, 2.0f); } }
    }

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
        float near = 0; for (int p = 0; p < g_nplayers; p++) { Kart *hk = &r->k[human_kart(p)]; if ((q->x - hk->x) * (q->x - hk->x) + (q->z - hk->z) * (q->z - hk->z) < 60.0f * 60.0f) near = 1; }
        if (near && !q->air && (!q->hit.on_road || q->drifting) && q->speed > 5.0f && frand01() < 0.6f) add_dust(v3(q->x, q->y, q->z), 1.5f, 1.5f);
        if (near && q->boost_t > 0 && frand01() < 0.9f) add_dust(v3(q->x - sinf(q->yaw) * 1.2f, q->y, q->z - cosf(q->yaw) * 1.2f), 0.6f, 1.5f); }
    if (sim_running) {
        for (int i = 0; i < MAX_DUST; i++) if (g_dust[i].life > 0) { Dust *d = &g_dust[i]; d->life -= dt; d->p.x += d->v.x * dt; d->p.y += d->v.y * dt; d->p.z += d->v.z * dt; d->v.y -= 2.0f * dt; d->size += 1.2f * dt; }
        for (int i = 0; i < MAX_SKID; i++) if (g_skid[i].used) { g_skid[i].a -= dt * 0.02f; if (g_skid[i].a <= 0) g_skid[i].used = 0; }
        for (int i = 0; i < MAX_SPARK; i++) if (g_spark[i].life > 0) { Spark *s = &g_spark[i]; s->life -= dt; s->p.x += s->v.x * dt; s->p.y += s->v.y * dt; s->p.z += s->v.z * dt; s->v.y -= 9.0f * dt; }
    }

    /* every player's camera */
    for (int p = 0; p < g_nplayers; p++) { cv = &g_view[p]; k = &r->k[human_kart(p)];
    if (cv->finish_t > 0 && cv->finish_t < 1.2f) { cv->finish_t += dt; if (g_nplayers > 1) cv->orbit += dt * 1.6f; }   /* after the finish the camera swings round the kart (the sim slows too when playing alone) */
    /* the race intro: during the countdown the camera sweeps around the grid and settles in behind the player */
    if (r->state == RACE_COUNTDOWN && !g_attract && g_mode != MODE_BATTLE && r->cd > 0.6f) {
        float u = clampf((3.5f + RACE_READY_T - r->cd) / (2.9f + RACE_READY_T), 0, 1), e = u * u * (3 - 2 * u);         /* 0 at the start of the countdown, 1 just before GO */
        float a = k->yaw + 3.14159f * (1.0f - e) * 0.9f + 0.6f * (1.0f - e);               /* starts in front and to the side, swings round to behind */
        float dist = 9.0f - 4.4f * e, hgt = 4.5f - 1.8f * e;
        cam_yaw = k->yaw; cam_x = k->x - sinf(a) * dist; cam_z = k->z - cosf(a) * dist; cam_y = k->y + hgt; cam_fov = 58.0f + 4.0f * e;
    } else {
    /* chase camera: sits behind the kart, lags on turns, pulls back with speed */
    int orbiting = g_nplayers > 1 ? (cv->finish_t > 0 && cv->finish_t < 1.2f) : (g_slowmo > 0);
    float want_yaw = k->yaw + k->slide * 0.3f + (orbiting ? g_orbit : 0);
    float dy = want_yaw - cam_yaw; while (dy > 3.14159f) dy -= 6.28318f; while (dy < -3.14159f) dy += 6.28318f;
    cam_yaw += dy * fminf(1.0f, 5.0f * dt);
    float back = 4.6f + k->speed * 0.12f, up = 2.7f + k->speed * 0.02f;   /* close when stopped (the kart behind on the grid is 5 m back), pulls back with speed */
    float tx = k->x - sinf(cam_yaw) * back, tz = k->z - cosf(cam_yaw) * back, ty = k->y + up;
    cam_x += (tx - cam_x) * fminf(1.0f, 12.0f * dt); cam_z += (tz - cam_z) * fminf(1.0f, 12.0f * dt); cam_y += (ty - cam_y) * fminf(1.0f, 6.0f * dt);
    float want_fov = 62.0f + (k->boost_t > 0 ? 10.0f : 0.0f) + k->speed * 0.15f;
    cam_fov += (want_fov - cam_fov) * fminf(1.0f, 4.0f * dt);
    }
    if (g_shake > 0) g_shake = fmaxf(0, g_shake - dt * 1.5f);
    }
    cv = &g_view[0]; k = &r->k[r->player];
    /* ---- draw both screens */
    int split = g_nplayers > 1 && (g_screen == SCR_RACE || g_screen == SCR_PAUSE);
    if (!split) { cv = &g_view[0]; build_dynamic(); }
    gfx_frame_begin();
    for (int drc = 0; drc < 2; drc++) {
        if (g_status == GFX_NO_COMPILER) { gfx_view_begin(drc, 1.0f, 0.5f, 0.0f); gfx_view_end(drc); continue; }
        if (g_status == GFX_SHADER_FAILED) { gfx_view_begin(drc, 0.8f, 0.05f, 0.05f); gfx_view_end(drc); continue; }
        int gamepad_map = drc && (g_screen == SCR_RACE || g_screen == SCR_PAUSE) && !g_attract;
#ifdef KK_TOUR
        if (!drc && g_time > 98.0f && g_time < 103.0f && g_screen == SCR_RACE && !split) gamepad_map = 1;   /* the CI camera only sees the TV: show the GamePad layout there for one photo */
#endif
        if (gamepad_map) { gfx_view_begin(drc, 0.04f, 0.07f, 0.18f); gfx_camera_2d(); gfx_state(ST_HUD); Mat4 id2 = m4_identity();
            batch_clear(&g_b); hud_gamepad(); if (g_screen == SCR_PAUSE) hud_pause(); gfx_mesh_update(m_hud2, g_b.v, g_b.n); gfx_bind(tx_font); gfx_draw(m_hud2, &id2, 1, 1, 1, 1); gfx_view_end(drc); continue; }
        if (g_screen == SCR_OPENER) { gfx_view_begin(drc, 0, 0, 0); gfx_camera_2d(); gfx_state(ST_HUD); Mat4 id2 = m4_identity();
            { Texture *vt = g_op_vid ? vid_texture() : tx_op; if (vt && m_op) { gfx_bind(vt); gfx_draw(m_op, &id2, 1, 1, 1, 1); } }
            gfx_view_end(drc); continue; }
        if (split) {   /* the TV in pieces: one view per player, each with its own HUD; the whole-screen pause menu on top */
            gfx_view_begin(drc, 0.02f, 0.03f, 0.08f); Mat4 id = m4_identity();
            for (int p = 0; p < g_nplayers; p++) { float fx, fy, fw, fh; split_rect(p, g_nplayers, &fx, &fy, &fw, &fh);
                gfx_viewport(drc, fx, fy, fw, fh); cv = &g_view[p]; build_dynamic();
                float vw = fw * 1280.0f, vh = fh * 720.0f;
                draw_scene(human_kart(p), vw / vh, g_nplayers == 2 ? 0.62f : 1.0f, g_nplayers >= 3);
                gfx_camera_2d_virtual(vw, vh); gfx_state(ST_HUD); batch_clear(&g_b);
                hud_split(p, vw, vh);
                gfx_mesh_update(m_hud, g_b.v, g_b.n); gfx_bind(tx_font); gfx_draw(m_hud, &id, 1, 1, 1, 1);
                int it = r->k[human_kart(p)].item;
                if (it > 0 && it < ITEM_COUNT && !r->jericho) { float sc = vw < 1000 ? 0.75f : 1.0f; draw_item_window(p, 16, vh - 64 * sc - 14, 64 * sc); } }
            if (g_nplayers == 3) { gfx_viewport(drc, 0.5f, 0.5f, 0.5f, 0.5f); gfx_camera_2d_virtual(640, 360); gfx_state(ST_HUD); batch_clear(&g_b); hud_quarter_map(); gfx_mesh_update(m_hud, g_b.v, g_b.n); gfx_bind(tx_font); gfx_draw(m_hud, &id, 1, 1, 1, 1); }
            gfx_viewport(drc, 0, 0, 1, 1); gfx_camera_2d(); gfx_state(ST_HUD); batch_clear(&g_b);
            batch_rect(&g_b, 0, 358, 1280, 4, 0.02f, 0.03f, 0.08f, 1); if (g_nplayers >= 3) batch_rect(&g_b, 638, 0, 4, 720, 0.02f, 0.03f, 0.08f, 1);   /* the bars between the views */
            if (g_screen == SCR_PAUSE) hud_pause();
            if (g_screen_t < 0.3f && g_screen == SCR_RACE) batch_rect(&g_b, 0, 0, 1280, 720, 0, 0, 0, 1.0f - g_screen_t / 0.3f);
            gfx_mesh_update(m_hud2, g_b.v, g_b.n); gfx_bind(tx_font); gfx_draw(m_hud2, &id, 1, 1, 1, 1);
            cv = &g_view[0]; gfx_view_end(drc); continue; }
        cv = &g_view[0];
        gfx_view_begin(drc, g_def->sky_r, g_def->sky_g, g_def->sky_b);
        Mat4 id = m4_identity();
        draw_scene(g_mode == MODE_BATTLE ? 0 : r->player, gfx_aspect(drc), 1.0f, 0);
        /* ---- HUD and menus */
        gfx_camera_2d(); gfx_state(ST_HUD);
        batch_clear(&g_b);
        { /* a touch of the road's color over the whole picture, and darker corners */
          static Mesh *m_tone = NULL; if (!m_tone) { batch_rect_uv(&g_b, 0, 0, 1280, 720, 0, 0, 1, 1, 1, 1, 1, 1); m_tone = gfx_mesh_static(g_b.v, g_b.n); batch_clear(&g_b); }
          gfx_bind(NULL); gfx_draw(m_tone, &id, g_def->fog_r, g_def->fog_g, g_def->fog_b, 0.07f);
          gfx_bind(tx_vignette); gfx_draw(m_tone, &id, 1, 1, 1, 1); }
        int menu_screen = g_screen == SCR_TITLE || g_screen == SCR_MAIN || g_screen == SCR_CHAR || g_screen == SCR_KART || g_screen == SCR_TRACK || g_screen == SCR_GP || g_screen == SCR_JOIN || g_screen == SCR_MCHAR || g_screen == SCR_CUP;
        if (menu_screen) batch_rect(&g_b, 0, 0, 1280, 720, 0.02f, 0.03f, 0.1f, g_screen == SCR_TITLE ? 0.25f : 0.55f);
        switch (g_screen) {
            case SCR_TITLE: hud_title(); break;
            case SCR_MAIN: hud_main(); break;
            case SCR_CREDITS: hud_credits(); break;
            case SCR_OPTIONS: hud_options(); break;
            case SCR_CHAR: hud_char(0); break;
            case SCR_CUP: hud_cup(); break;
            case SCR_MCHAR: hud_char(1); break;
            case SCR_JOIN: hud_join(pads, npads); break;
            case SCR_KART: hud_kart(); break;
            case SCR_TRACK: hud_track(); break;
            case SCR_RACE: if (g_mode == MODE_BATTLE) hud_battle(); else hud_race(pad); break;
            case SCR_PAUSE: if (g_mode == MODE_BATTLE) hud_battle(); else hud_race(pad); hud_pause(); break;
            case SCR_RESULTS: if (g_mode == MODE_BATTLE) hud_battle_results(); else hud_results(); break;
            case SCR_GP: hud_gp(); break;
            case SCR_TROPHY: hud_trophy(); break;
        }
        if (g_screen == SCR_TITLE && tx_title_bg && m_title_bg) { float u = g_screen_t < 0.7f ? g_screen_t / 0.7f : 1.0f; gfx_bind(tx_title_bg); gfx_draw(m_title_bg, &id, 1, 1, 1, u); }
        else if (g_screen == SCR_TITLE && tx_title && m_title) { float u = g_screen_t < 0.7f ? g_screen_t / 0.7f : 1.0f; float ease = 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u);
            gfx_bind(tx_title); gfx_draw(m_title, &id, 1, 1, 1, ease); }
        /* a quick fade from black when a new screen opens */
        if (g_screen_t < 0.3f && (g_screen == SCR_RACE || g_screen == SCR_RESULTS || g_screen == SCR_TITLE || g_screen == SCR_CREDITS || g_screen == SCR_GP || g_screen == SCR_TROPHY)) batch_rect(&g_b, 0, 0, 1280, 720, 0, 0, 0, 1.0f - g_screen_t / 0.3f);
        gfx_mesh_update(m_hud, g_b.v, g_b.n);
        gfx_bind(tx_font); gfx_draw(m_hud, &id, 1, 1, 1, 1);
        if (g_screen == SCR_MAIN && tx_title && m_logo_small) { gfx_bind(tx_title); gfx_draw(m_logo_small, &id, 1, 1, 1, 1); }
        if (g_screen == SCR_CHAR || g_screen == SCR_MCHAR) draw_portraits();
        if (g_screen == SCR_RESULTS && g_mode != MODE_TT && g_mode != MODE_BATTLE) draw_podium_karts();
        if (g_screen == SCR_TROPHY) { draw_trophy_pictures(); static Mesh *m_top = NULL; if (!m_top) m_top = gfx_mesh_dynamic(8192); batch_clear(&g_b); hud_trophy_top(); gfx_mesh_update(m_top, g_b.v, g_b.n); gfx_bind(tx_font); gfx_draw(m_top, &id, 1, 1, 1, 1); }
        if (g_screen == SCR_TRACK) for (int c2 = 0; c2 < ncards() && c2 < 8; c2++) { int d = card_track(c2); if (d >= 8 || !tx_card[d] || !track_unlocked(d)) continue;
            if (!m_card[c2]) m_card[c2] = gfx_mesh_dynamic(6);
            { float x = 40 + c2 * CARD_STEP + 12, y = 110 + 12; batch_clear(&g_b); batch_rect_uv(&g_b, x, y, CARD_IN, CARD_IN * 0.54f, 0, 0, 1, 1, 1, 1, 1, 1); gfx_mesh_update(m_card[c2], g_b.v, g_b.n); }
            gfx_bind(tx_card[d]); gfx_draw(m_card[c2], &id, 1, 1, 1, 1); }
        if (g_screen == SCR_KART) { draw_kart_models(); gfx_camera_2d(); gfx_state(ST_HUD);
            if (tx_kart[g_player_char]) { int two = tx_kartback[g_player_char] != NULL; float a = g_kart_aspect[g_player_char], ph = 220, pw = ph * a; if (pw > (two ? 330 : 500)) { pw = two ? 330 : 500; ph = pw / a; }
                float cx = two ? 470 : 640;
                batch_clear(&g_b); batch_rect_uv(&g_b, cx - pw / 2, 67 + (220 - ph) / 2, pw, ph, 0, 0, 1, 1, 1, 1, 1, 1); gfx_mesh_update(m_kartpic, g_b.v, g_b.n); gfx_bind(tx_kart[g_player_char]); gfx_draw(m_kartpic, &id, 1, 1, 1, 1);
                if (two) { static Mesh *m2 = NULL; if (!m2) m2 = gfx_mesh_dynamic(6); a = g_kartback_aspect[g_player_char]; ph = 220; pw = ph * a; if (pw > 330) { pw = 330; ph = pw / a; }
                    batch_clear(&g_b); batch_rect_uv(&g_b, 810 - pw / 2, 67 + (220 - ph) / 2, pw, ph, 0, 0, 1, 1, 1, 1, 1, 1); gfx_mesh_update(m2, g_b.v, g_b.n); gfx_bind(tx_kartback[g_player_char]); gfx_draw(m2, &id, 1, 1, 1, 1); } } }
        if ((g_screen == SCR_RACE || g_screen == SCR_PAUSE) && g_mode != MODE_TT) { int it = r->k[r->player].item;
            if (it > 0 && it < ITEM_COUNT && !r->jericho) draw_item_window(0, 44, 118, 104); }
        gfx_view_end(drc);
    }
    gfx_frame_end();
}
int game_debug_screen(void) { return g_screen + 20 * g_mode + 100 * g_menu_sel + 1000 * (int)g_race.state; }
const char *game_debug_track(void) { return g_track.name; }
