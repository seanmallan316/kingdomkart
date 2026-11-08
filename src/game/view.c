// view.c - draws the race: track, eight karts, items, effects, HUD, results. The rules live in sim/race.c.
#include "game.h"
#include "models.h"
#include "kart3d.h"
#include "../net/net.h"
#include "anim3d.h"
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
enum { SCR_TITLE, SCR_MAIN, SCR_CHAR, SCR_KART, SCR_TRACK, SCR_RACE, SCR_PAUSE, SCR_RESULTS, SCR_GP, SCR_CREDITS, SCR_OPTIONS, SCR_OPENER, SCR_JOIN, SCR_MCHAR, SCR_CUP, SCR_TROPHY, SCR_ONLINE, SCR_SCENE };
/* the opener video: frames content/video/opener_NNN.jpg, decoded one at a time as they come up */
static int g_op_frames = 0, g_op_fps = 12, g_op_cur = -1, g_op_w = 0, g_op_h = 0, g_op_vid = 0; static float g_op_buf = 0; static Texture *tx_op; static Mesh *m_op;   /* g_op_vid: the background decoder is running; g_op_buf: seconds spent filling it before the clock starts */
static int g_multi = 0;   /* 2 to 4 people are playing together (any mode) */
static int g_res_sel = 0;   /* the results screen's choice: 0 next road, 1 race again, 2 choose road, 3 main menu */
static int results_first_choice(void);
static float g_title_press = 0;   /* seconds since a button was pressed on the title screen (0 = not yet) */
static float g_track_wait = 0; static int g_loaded_track = -1;
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
static int cer_order(int *ord);
static int g_gp_kp[NKARTS];   /* cup points per kart seat (the field is the same every round), so two karts can never pool their points */
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
static int g_menu_slot = -1, g_pause_slot = -1;   /* player 1's controller for the menus; who paused (-1 = anyone) */
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
static Anim3D g_gol3d; static Mesh *m_spear3d;
static Texture *tx_goliath; static float g_goliath_aspect = 0.67f; static Mesh *m_goliath, *m_hud2;
static Kart3D g_k3d[NCHARS];   /* racers with a real 3D kart (content/models/<name>.kkm) */
static Texture *tx_flame; static Texture *tx_emblem[NSTYLES]; static int g_emb_kind; static void px_emblem(int x, int y, uint32_t *o); static Texture *tx_bearer; static float g_bearer_aspect = 0.67f; static Mesh *m_bearer;   /* Goliath's shield-bearer (char_shieldbearer.png, or a drawn one) */
static Mesh *m_driver_box, *m_driver_head, *m_driver_spr[NKARTS];
static unsigned g_unl_bits; static float g_unl_t;   /* the "UNLOCKED!" screens still to show after this race, one per prize, and how long the current one has been up */
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
static Mesh *m_shsquare;
static int g_pad_race = 1;   /* the GamePad during a race: 1 = the race itself (the TV picture), 0 = the big map and standings; tap its corner button to switch */   /* the patch of road a kart's shadow is darkened through (see ST_MULDA) */
static uint32_t g_px[256 * 256];
#define PROP_CHUNKS 12
static Mesh *m_props[MAT_N][PROP_CHUNKS], *m_curbs; static int m_nprops[MAT_N]; static Texture *tx_mat[MAT_N];
static Vertex *g_matv[MAT_N]; static Batch g_matb[MAT_N];
#define MATB_CAP 24000
static void mat_buckets(ModelOut *mo) {   /* the buckets the models are built into, made once */
    for (int m = 0; m < MAT_N; m++) { if (!g_matv[m]) g_matv[m] = malloc(sizeof(Vertex) * MATB_CAP); batch_init(&g_matb[m], g_matv[m], g_matv[m] ? MATB_CAP : 0); mo->b[m] = &g_matb[m]; }
    if (!tx_mat[0]) for (int m = 0; m < MAT_N; m++) { model_texture(m, g_px, 256); tx_mat[m] = gfx_texture_rgba_ex(g_px, 256, 256, 1, 0, 1); } }
static void mat_flush(ModelOut *mo) { for (int m = 0; m < MAT_N; m++) { if (mo->b[m]->n && m_nprops[m] < PROP_CHUNKS) m_props[m][m_nprops[m]++] = gfx_mesh_static(mo->b[m]->v, mo->b[m]->n); batch_clear(mo->b[m]); } }
static void draw_mat_meshes(Mesh *const *ms, int n, int m, const Mat4 *mx) { if (!n) return; gfx_bind(tx_mat[m]); for (int c = 0; c < n; c++) if (ms[c]) gfx_draw(ms[c], mx, 1, 1, 1, 1); }   /* 3D buildings, columns, tents and rocks where there is no picture; raised kerbs along the road */
/* meshes */
static Mesh *m_fallsw, *m_foam; static Texture *tx_fallsw, *tx_poolw;
static Mesh *m_hills, *m_mountains, *m_clouds, *m_boostglow; static Texture *tx_padglow; static Texture *tx_white; static Mesh *m_field, *m_gem, *m_pebble, *m_whale, *m_whale_jaw; static Texture *tx_whale[4]; static float g_whale_asp[4];   /* Sean's whale: front, front-side, side, back */ static float g_ground_y; static Mesh *m_brook, *m_tunnel, *m_arch, *m_coin, *m_coins, *m_boostpads, *m_bouncepads, *m_road, *m_shoulder, *m_ground, *m_wall, *m_startline, *m_kart_body, *m_kart_rest, *m_body_style[NSTYLES], *m_rest_style[NSTYLES], *m_cube, *m_shield, *m_items, *m_skid, *m_glow, *m_dust, *m_hud, *m_shadow, *m_sky;
static Vertex g_vtmp[32768];   /* scratch for building meshes (the walls of a long road need over 9000 corners) */
static Batch g_b;

/* camera state lives in the views (cam_x and friends are the view being handled) */
#define KART_DRAW 0.82f      /* the karts are drawn this much smaller than first made (Sean: more room on the road) */
static float g_slowmo = 0;      /* finish-line slow motion (single player): seconds left */
#define CELE_T 4.5f            /* the victory lap of the camera round a kart that has finished */
static float cele_bounce(const Race *r, int i);
/* Time Trial ghost: the best run is recorded ten times a second and replayed as a see-through kart */
#define GHOST_HZ 10
#define GHOST_MAX (GHOST_HZ * 420)   /* seven minutes: the roads are long now */
typedef struct { int16_t x, z, yaw; } GhostSample;        /* position in 1/16 m, yaw in 1/10000 rad */
static GhostSample g_ghost[GHOST_MAX], g_rec[GHOST_MAX]; static int g_ghost_n = 0, g_rec_n = 0, g_ghost_char = 0; static float g_rec_t = 0; static Mesh *m_ghost;
static void ghost_load(int road) {
    g_ghost_n = 0; char name[64], path[300]; snprintf(name, sizeof name, "kingdomkart_ghost4_%d.bin", road);
    FILE *f = fopen(save_dir_path(path, sizeof path, name), "rb"); if (!f) return;
    int hdr[2]; if (fread(hdr, sizeof hdr, 1, f) == 1 && hdr[0] > 0 && hdr[0] <= GHOST_MAX) { g_ghost_char = hdr[1]; g_ghost_n = (int)fread(g_ghost, sizeof(GhostSample), (size_t)hdr[0], f); }
    fclose(f); gfx_log("ghost: %d samples for road %d", g_ghost_n, road);
}
static void ghost_save(int road) {
    char name[64], path[300]; snprintf(name, sizeof name, "kingdomkart_ghost4_%d.bin", road);
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
typedef struct { Vec3 p; float h, w, glow; int kind; int purple, falls; int wall; float fall_t; int solid; float yaw; Mesh *m3; Vec3 t3, o3; } Placed;   /* m3: a wall of Jericho as its own 3D model; t3, o3: along the road and outward, for its fall */   /* solid: drawn as a real 3D model instead of a picture; yaw: which way it faces */   /* wall: 0 = not a wall, else its place in the order the walls of Jericho fall (1..n); fall_t: seconds since it began to fall */
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
static void px_flame(int x, int y, uint32_t *o) {   /* one tongue of fire, 64 x 128: a soft teardrop, hot white in the middle, licking up to a point */
    float u = (x - 31.5f) / 32.0f, t = 1.0f - y / 127.0f;   /* t: 0 at the base, 1 at the tip */
    float w = 0.95f * powf(sinf(3.14159f * fminf(1.0f, powf(t, 0.55f) * 0.98f + 0.02f)), 0.8f) * (1.0f - 0.55f * t), e = fabsf(u) / (w + 1e-3f);
    float a = e > 1 ? 0 : (1 - e * e) * fminf(1.0f, t * 6.0f) * (1.0f - t * t * 0.5f); float core = fmaxf(0, 1 - e * 1.8f) * (1 - t * 0.9f);
    int v = (int)(fminf(1.0f, 0.55f + core) * 255); *o = rgba(v, v, v, (int)(clampf(a, 0, 1) * 255)); }
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
    batch_quad4(b, p0, p1, p2, p3, n, l0 * vscale, v0, l1 * vscale, v1, r, g, bl, 1);   /* square tiles: across the road at the same scale as along it (it was one picture stretched over the whole width) */
}
static void free_track_meshes(void) {
    gfx_mesh_free(m_hills); m_hills = NULL;
    Mesh **ms[] = {&m_fallsw, &m_foam, &m_boostglow, &m_mountains, &m_clouds, &m_brook, &m_tunnel, &m_arch, &m_coin, &m_coins, &m_finale, &m_boostpads, &m_bouncepads, &m_road, &m_shoulder, &m_wall, &m_startline, &m_ground, &m_sky, &m_skid, &m_dust, &m_spark, &m_hud, &m_kart_body, &m_kart_rest, &m_cube, &m_qface, &m_qframe, &m_field, &m_gem, &m_pebble, &m_whale, &m_whale_jaw, &m_shield, &m_items, &m_shadow};
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
static float ground_level(const Track *t) {   /* the height of the ground plane (the sea on a water road) */
    float lowest = 1e9f; for (int i = 0; i < t->n; i++) for (int sd = -1; sd <= 1; sd += 2) { float y = track_surface_height(t, i, sd * track_bound(t, i, sd)); if (y < lowest) lowest = y; }
    return t->water && t->water_y > -1e8f ? t->water_y + 0.35f : lowest - 0.6f;   /* on the sea the karts sit a little way into the water: wading */
}

/* ---- the far world (Sean: the whole world was still flat pictures): real 3D mountain ranges standing in a ring round the
   road, and real 3D clouds drifting overhead. They are lit and hazed when built (the shader's fog is off while they are
   drawn, like the sky), and they stay put in the world, so they slide past each other as you drive. */
#define FAR_CAP 40000
static Vertex *far_buf(void) { static Vertex *b = NULL; if (!b) b = malloc(sizeof(Vertex) * FAR_CAP); return b; }
static void px_padglow(int x, int y, uint32_t *o) { float dx = (x - 31.5f) / 32.0f, dy = (y - 31.5f) / 32.0f, d = sqrtf(dx * dx + dy * dy), a = fmaxf(0, 1 - d); a = a * a * 0.85f;
    *o = rgba((int)(255 * a), (int)(235 * a), (int)(160 * a), (int)(255 * a)); }
static void px_white(int x, int y, uint32_t *o) { (void)x; (void)y; *o = rgba(255, 255, 255, 255); }
static void far_vert(Batch *b, Vec3 p, Vec3 n, float u, float v, float r, float g, float bl) {
    if (b->n >= b->cap) return; Vertex q = {p.x, p.y, p.z, n.x, n.y, n.z, u, v, r, g, bl, 1}; b->v[b->n++] = q; }
static void build_far_world(const Track *t) {
    gfx_mesh_free(m_mountains); m_mountains = NULL; gfx_mesh_free(m_clouds); m_clouds = NULL;
    if (!tx_white) tx_white = make_tex(4, 4, 1, 0, px_white);
    const TrackDef *d = g_def; if (!d || d->finale_img) return;   /* the road to Heaven is above the clouds already */
    float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
    for (int i = 0; i < t->n; i++) { x0 = fminf(x0, t->s[i].x); x1 = fmaxf(x1, t->s[i].x); z0 = fminf(z0, t->s[i].z); z1 = fmaxf(z1, t->s[i].z); }
    float cx = (x0 + x1) / 2, cz = (z0 + z1) / 2, rad = sqrtf((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0)) / 2;
    Vec3 L = v3(d->light_x, d->light_y, d->light_z); { float l = sqrtf(L.x * L.x + L.y * L.y + L.z * L.z); if (l < 1e-4f) l = 1; L = v3(L.x / l, L.y / l, L.z / l); }
    float night = d->night ? 0.42f : 1.0f, fr = d->fog_r, fg = d->fog_g, fb = d->fog_b;
    /* rock and land colors by road: green-brown hills, pale desert stone, or the sunrise's warm rock */
    int desert = d->bonus == 3 || !strcmp(d->prefix, "tomb") || !strcmp(d->prefix, "elah"), lush = !strcmp(d->prefix, "eden") || d->water;
    float lr = desert ? 0.78f : (lush ? 0.42f : 0.62f), lg = desert ? 0.66f : (lush ? 0.58f : 0.6f), lb = desert ? 0.48f : (lush ? 0.32f : 0.4f);
    Batch b; batch_init(&b, far_buf(), FAR_CAP); batch_clear(&b);
    /* two ranges: a nearer row of rolling hills and a taller, hazier range behind it */
    enum { NA = 120, NR = 7 };
    for (int layer = 0; layer < 2; layer++) {
        float r0 = rad + (d->water ? 330.0f : 260.0f) + layer * 230.0f, span = 260.0f + layer * 120.0f;
        float hmax = layer ? 230.0f : 95.0f, haze = layer ? 0.62f : 0.36f; if (d->bonus == 2) { r0 = rad + 140.0f + layer * 200.0f; }
        static float hy[NA + 1][NR + 1]; static Vec3 P[NA + 1][NR + 1];
        for (int a = 0; a <= NA; a++) for (int r = 0; r <= NR; r++) {
            float ang = a * 6.2831853f / NA, rr = (float)r / NR, R = r0 + rr * span;
            float ridge = sinf(rr * 3.14159265f); ridge = powf(fmaxf(ridge, 0), 0.7f);   /* up from the near foot to the crest, down the far side */
            float n = smooth_noise(a * 0.11f + layer * 40, rr * 2.0f) * 0.6f + smooth_noise(a * 0.37f + layer * 17, rr * 4.0f + 3) * 0.3f + smooth_noise(a * 1.1f, rr * 9.0f) * 0.1f;
            float h = hmax * (0.25f + 0.95f * n * n) * ridge;
            hy[a][r] = h; P[a][r] = v3(cx + cosf(ang) * R, g_ground_y - 2.0f + h, cz + sinf(ang) * R); }
        for (int a = 0; a < NA; a++) for (int r = 0; r < NR; r++) {
            Vec3 q[4] = {P[a][r], P[a + 1][r], P[a + 1][r + 1], P[a][r + 1]}; float c[4][3]; Vec3 nn[4];
            int ia[4] = {a, a + 1, a + 1, a}, ir[4] = {r, r, r + 1, r + 1};
            for (int k = 0; k < 4; k++) { int A = ia[k], Rr = ir[k];
                Vec3 ta = v3_sub(P[A < NA ? A + 1 : 1][Rr], P[A > 0 ? A - 1 : NA - 1][Rr]), tr = v3_sub(P[A][Rr < NR ? Rr + 1 : Rr], P[A][Rr > 0 ? Rr - 1 : Rr]);
                Vec3 n = v3_norm(v3_cross(tr, ta)); if (n.y < 0) n = v3(-n.x, -n.y, -n.z); nn[k] = n;
                float lit = 0.55f + 0.45f * fmaxf(0, n.x * L.x + n.y * L.y + n.z * L.z), up = clampf(hy[A][Rr] / hmax, 0, 1);
                float snow = (!desert && !lush && layer && up > 0.72f) ? (up - 0.72f) * 3.0f : 0;   /* a little snow on the high tops */
                float rr2 = lr + (0.85f - lr) * up * 0.5f, gg = lg + (0.8f - lg) * up * 0.4f, bb2 = lb + (0.75f - lb) * up * 0.5f;
                rr2 += (1 - rr2) * snow; gg += (1 - gg) * snow; bb2 += (1 - bb2) * snow;
                float hz = haze * (1.0f - 0.35f * up);   /* hazier at the foot, clearer on the crests */
                c[k][0] = (rr2 * lit * (1 - hz) + fr * hz) * night * d->sun_r; c[k][1] = (gg * lit * (1 - hz) + fg * hz) * night * d->sun_g; c[k][2] = (bb2 * lit * (1 - hz) + fb * hz) * night * d->sun_b; }
            static const int ix[6] = {0, 1, 2, 0, 2, 3};
            for (int k = 0; k < 6; k++) { int j = ix[k]; far_vert(&b, q[j], nn[j], q[j].x / 25.0f, q[j].z / 25.0f, c[j][0], c[j][1], c[j][2]); } }
    }
    /* a far-off town on the hills: little flat-roofed houses (not on the sea or in the desert) */
    if (!d->water && !desert) for (int h = 0; h < 46; h++) {
        float ang = 0.6f + h * 0.137f + noise2(h, 7) * 0.08f, R = rad + 300.0f + noise2(h, 3) * 60.0f;
        float px = cx + cosf(ang) * R, pz = cz + sinf(ang) * R, w = 7 + noise2(h, 1) * 8, hh = 6 + noise2(h, 2) * 7, base = g_ground_y - 2 + 30 + noise2(h, 5) * 25;
        float shade[6] = {1.0f, 0.62f, 0.8f, 0.7f, 0.9f, 0.5f}; float wr = 0.92f, wg = 0.86f, wb = 0.74f;
        Vec3 c0 = v3(px, base, pz); float ca = cosf(ang * 3), sa = sinf(ang * 3);
        Vec3 ax = v3(ca * w, 0, sa * w), az = v3(-sa * w, 0, ca * w);
        Vec3 corner[8]; for (int k = 0; k < 8; k++) corner[k] = v3(c0.x + ax.x * ((k & 1) ? 0.5f : -0.5f) + az.x * ((k & 2) ? 0.5f : -0.5f), c0.y + ((k & 4) ? hh : 0.0f), c0.z + ax.z * ((k & 1) ? 0.5f : -0.5f) + az.z * ((k & 2) ? 0.5f : -0.5f));
        static const int face[6][4] = {{4, 5, 7, 6}, {0, 1, 5, 4}, {1, 3, 7, 5}, {3, 2, 6, 7}, {2, 0, 4, 6}, {0, 2, 3, 1}};
        for (int f = 0; f < 5; f++) { float sh = shade[f] * night, hz = 0.45f; float r = (wr * sh) * (1 - hz) + fr * hz, g = (wg * sh) * (1 - hz) + fg * hz, bl = (wb * sh) * (1 - hz) + fb * hz;
            Vec3 n = v3_norm(v3_cross(v3_sub(corner[face[f][1]], corner[face[f][0]]), v3_sub(corner[face[f][2]], corner[face[f][0]])));
            static const int ix[6] = {0, 1, 2, 0, 2, 3}; for (int k = 0; k < 6; k++) far_vert(&b, corner[face[f][ix[k]]], n, 0.5f, 0.5f, r, g, bl); }
    }
    m_mountains = b.n ? gfx_mesh_static(b.v, b.n) : NULL;
    /* clouds: each a bunch of soft, lumpy balls, bright on top and grey-blue underneath */
    batch_clear(&b);
    int nc = d->night ? 6 : 16;
    for (int cl = 0; cl < nc; cl++) {
        float ang = cl * 6.2831853f / nc + noise2(cl, 11) * 0.3f, R = rad * 0.4f + 150.0f + noise2(cl, 12) * (rad + 250.0f);
        Vec3 cc = v3(cx + cosf(ang) * R, g_ground_y + 150.0f + noise2(cl, 13) * 90.0f, cz + sinf(ang) * R);
        int np = 5 + (int)(noise2(cl, 14) * 4);
        for (int p = 0; p < np; p++) {
            float pr = 18 + noise2(cl * 7 + p, 15) * 16; Vec3 pc = v3(cc.x + (p - np / 2.0f) * 16 + noise2(cl, p) * 10, cc.y + noise2(p, cl) * 10 - fabsf(p - np / 2.0f) * 3, cc.z + (noise2(cl + p, 9) - 0.5f) * 26);
            enum { SU = 10, SV = 6 };
            for (int i = 0; i < SU; i++) for (int j = 0; j < SV; j++) {
                Vec3 q[4]; Vec3 nn[4]; float cr[4];
                int ii[4] = {i, i + 1, i + 1, i}, jj[4] = {j, j, j + 1, j + 1};
                for (int k = 0; k < 4; k++) { float th = ii[k] * 6.2831853f / SU, ph = jj[k] * 3.14159265f / SV - 1.5707963f;
                    Vec3 n = v3(cosf(ph) * cosf(th), sinf(ph), cosf(ph) * sinf(th)); nn[k] = n;
                    float sq = n.y < 0 ? 0.45f : 0.8f;   /* flat-bottomed */
                    q[k] = v3(pc.x + n.x * pr * 1.25f, pc.y + n.y * pr * sq, pc.z + n.z * pr);
                    float lit = 0.72f + 0.28f * fmaxf(0, n.x * L.x + n.y * L.y + n.z * L.z); lit *= 0.8f + 0.2f * (n.y * 0.5f + 0.5f); cr[k] = lit; }
                static const int ix[6] = {0, 2, 1, 0, 3, 2};
                for (int k = 0; k < 6; k++) { int jx = ix[k]; float l = cr[jx], hz = 0.18f;
                    far_vert(&b, q[jx], nn[jx], 0.5f, 0.5f, (l * (1 - hz) + fr * hz) * night * (0.9f + 0.1f * d->sun_r), (l * (1 - hz) + fg * hz) * night * (0.9f + 0.1f * d->sun_g), (l * 1.03f * (1 - hz) + fb * hz) * night * (0.9f + 0.1f * d->sun_b)); } } } }
    m_clouds = b.n ? gfx_mesh_static(b.v, b.n) : NULL;
}
static void build_track_meshes(void) {
    const Track *t = &g_track;
    batch_init(&g_b, g_vtmp, 16384);
    /* road */
    batch_clear(&g_b);
    if (g_def->bonus == 2) {   /* the battle valley: one flat disc of grass out to the edge, from the middle (a road strip that wide would fold over itself there) */
        float cx = 0, cz = 0; for (int i = 0; i < t->n; i++) { cx += t->s[i].x; cz += t->s[i].z; } cx /= t->n; cz /= t->n;
        for (int i = 0; i < t->n; i++) { int j = (i + 1) % t->n; float h = t->s[i].width * 0.5f, hj = t->s[j].width * 0.5f;
            Vec3 a = road_point(t, i, h), b = road_point(t, j, hj), a2 = road_point(t, i, -h);
            if ((a2.x - cx) * (a2.x - cx) + (a2.z - cz) * (a2.z - cz) > (a.x - cx) * (a.x - cx) + (a.z - cz) * (a.z - cz)) { a = a2; b = road_point(t, j, -hj); }
            Vec3 c = v3(cx, a.y, cz), p0 = a, p1 = b;
            if ((p0.x - c.x) * (p1.z - c.z) - (p0.z - c.z) * (p1.x - c.x) > 0) { Vec3 q = p0; p0 = p1; p1 = q; }   /* counter-clockwise seen from above */
            Vertex v[3] = { {c.x, c.y, c.z, 0, 1, 0, c.x / 10.0f, c.z / 10.0f, 1, 1, 1, 1}, {p1.x, p1.y, p1.z, 0, 1, 0, p1.x / 10.0f, p1.z / 10.0f, 1, 1, 1, 1}, {p0.x, p0.y, p0.z, 0, 1, 0, p0.x / 10.0f, p0.z / 10.0f, 1, 1, 1, 1} };
            if (g_b.n + 3 <= g_b.cap) for (int k = 0; k < 3; k++) g_b.v[g_b.n++] = v[k]; }
    } else
    for (int i = 0; i < t->n; i++) strip_quad(&g_b, t, i, -t->s[i].width * 0.5f, t->s[i].width * 0.5f, 1.0f / 7.0f, 1, 1, 1);
    m_road = gfx_mesh_static(g_b.v, g_b.n);
    /* shoulders (dirt); the battle valley has only an outside edge (its inside edge is the middle of the disc) */
    int skip_side = 0;
    if (g_def->bonus == 2) { float cx = 0, cz = 0; for (int i = 0; i < t->n; i++) { cx += t->s[i].x; cz += t->s[i].z; } cx /= t->n; cz /= t->n;
        Vec3 r = road_point(t, 0, 1.0f); skip_side = (r.x - cx) * (r.x - cx) + (r.z - cz) * (r.z - cz) < (t->s[0].x - cx) * (t->s[0].x - cx) + (t->s[0].z - cz) * (t->s[0].z - cz) ? 1 : -1; }
    batch_clear(&g_b);
    for (int i = 0; i < t->n; i++) {
        float h = t->s[i].width * 0.5f;
        if (skip_side != -1) strip_quad(&g_b, t, i, -h - SHOULDER_W, -h, 1.0f / 6.0f, 1, 1, 1);
        if (skip_side != 1) strip_quad(&g_b, t, i, h, h + SHOULDER_W, 1.0f / 6.0f, 1, 1, 1);
    }
    m_shoulder = gfx_mesh_static(g_b.v, g_b.n);
    /* the ground level everything stands on (the sea on a water road) */
    g_ground_y = ground_level(t);
    /* low stone walls at the outer edge of the open land (at the shoulder where there is none), with a bank of earth
       below them down to the ground so a raised road never floats */
    batch_clear(&g_b);
    for (int i = 0; i < t->n; i++) {
        int j = (i + 1) % t->n;
        for (int side = -1; side <= 1; side += 2) { if (side == skip_side) continue;
            float e = track_bound(t, i, side), ej = track_bound(t, j, side);
            Vec3 a = road_point(t, i, side * e), b = road_point(t, j, side * ej);
            Vec3 at = v3(a.x, a.y + 0.9f, a.z), bt = v3(b.x, b.y + 0.9f, b.z);
            Vec3 n = v3(-t->s[i].nx * side, 0, -t->s[i].nz * side);
            float v0 = t->s[i].dist / 3.0f, v1 = v0 + 0.7f;
            /* darker where the wall meets the ground (a baked contact shadow) */
            if (side < 0) batch_quad4_shade(&g_b, a, b, bt, at, n, v0, 0, v1, 1, 0.55f, 1.0f);
            else batch_quad4_shade(&g_b, b, a, at, bt, n, v0, 0, v1, 1, 0.55f, 1.0f);
            /* the outside face, down to the ground */
            Vec3 ag = v3(a.x, g_ground_y - 0.2f, a.z), bg = v3(b.x, g_ground_y - 0.2f, b.z), no = v3(-n.x, 0, -n.z);
            if (a.y + 0.9f > g_ground_y) { if (side < 0) batch_quad4_shade(&g_b, bg, ag, at, bt, no, v0, 0, v1, 1, 0.5f, 0.9f); else batch_quad4_shade(&g_b, ag, bg, bt, at, no, v0, 0, v1, 1, 0.5f, 0.9f); }
        }
    }
    m_wall = gfx_mesh_static(g_b.v, g_b.n);
    /* the open land: grass from the shoulder out to the wall, textured in world space so it matches the ground plane */
    batch_clear(&g_b);
    if (!t->water) for (int i = 0; i < t->n; i++) { int j = (i + 1) % t->n;
        for (int side = -1; side <= 1; side += 2) { int sd = side > 0;
            if (t->field[i][sd] <= 0 && t->field[j][sd] <= 0) continue;
            float e0 = track_edge(t, i), e1 = track_edge(t, j), b0 = track_bound(t, i, side), b1 = track_bound(t, j, side);
            Vec3 p[4] = { road_point(t, i, side * e0), road_point(t, i, side * b0), road_point(t, j, side * b1), road_point(t, j, side * e1) };
            if (side < 0) { Vec3 q = p[1]; p[1] = p[3]; p[3] = q; }   /* counter-clockwise seen from above */
            if (g_b.n + 6 > g_b.cap) break;
            static const int ix[6] = {0, 1, 2, 0, 2, 3};
            for (int k = 0; k < 6; k++) { Vec3 v = p[ix[k]]; Vertex q = {v.x, v.y + 0.01f, v.z, 0, 1, 0, v.x / 10.0f, v.z / 10.0f, 1, 1, 1, 1}; g_b.v[g_b.n++] = q; } } }
    m_field = gfx_mesh_static(g_b.v, g_b.n);
    /* a hidden gem: an eight-faced crystal */
    batch_clear(&g_b);
    { Vec3 top = v3(0, 0.75f, 0), bot = v3(0, -0.75f, 0);
      for (int q = 0; q < 4; q++) { float a0 = q * 1.5707963f, a1 = (q + 1) * 1.5707963f; Vec3 r0 = v3(cosf(a0) * 0.55f, 0, sinf(a0) * 0.55f), r1 = v3(cosf(a1) * 0.55f, 0, sinf(a1) * 0.55f);
          Vec3 nt = face_normal(top, r1, r0), nb = face_normal(bot, r0, r1);
          Vertex v[6] = { {top.x, top.y, top.z, nt.x, nt.y, nt.z, 0, 0, 1, 1, 1, 1}, {r1.x, r1.y, r1.z, nt.x, nt.y, nt.z, 0, 0, 0.8f, 0.8f, 0.8f, 1}, {r0.x, r0.y, r0.z, nt.x, nt.y, nt.z, 0, 0, 0.8f, 0.8f, 0.8f, 1},
                          {bot.x, bot.y, bot.z, nb.x, nb.y, nb.z, 0, 0, 0.6f, 0.6f, 0.6f, 1}, {r0.x, r0.y, r0.z, nb.x, nb.y, nb.z, 0, 0, 0.9f, 0.9f, 0.9f, 1}, {r1.x, r1.y, r1.z, nb.x, nb.y, nb.z, 0, 0, 0.9f, 0.9f, 0.9f, 1} };
          for (int k = 0; k < 6; k++) g_b.v[g_b.n++] = v[k]; } }
    m_gem = gfx_mesh_static(g_b.v, g_b.n);
    /* a smooth stone from the brook: a squashed ball */
    batch_clear(&g_b);
    for (int ri = 0; ri < 6; ri++) for (int si = 0; si < 10; si++) {
        float t0 = 3.14159265f * ri / 6, t1 = 3.14159265f * (ri + 1) / 6, p0 = 6.2831853f * si / 10, p1 = 6.2831853f * (si + 1) / 10;
        Vec3 n00 = v3(sinf(t0) * cosf(p0), cosf(t0), sinf(t0) * sinf(p0)), n01 = v3(sinf(t0) * cosf(p1), cosf(t0), sinf(t0) * sinf(p1)), n10 = v3(sinf(t1) * cosf(p0), cosf(t1), sinf(t1) * sinf(p0)), n11 = v3(sinf(t1) * cosf(p1), cosf(t1), sinf(t1) * sinf(p1));
        Vec3 q[4] = {n00, n10, n11, n01}; static const int ix[6] = {0, 1, 2, 0, 2, 3};
        if (g_b.n + 6 > g_b.cap) break;
        for (int k = 0; k < 6; k++) { Vec3 n = q[ix[k]]; float sh = 0.85f + 0.15f * n.y; Vertex v = {n.x * 0.5f, n.y * 0.32f, n.z * 0.42f, n.x, n.y, n.z, 0, 0, sh, sh, sh, 1}; g_b.v[g_b.n++] = v; } }
    m_pebble = gfx_mesh_static(g_b.v, g_b.n);
    /* the great fish (a whale, about 10 m long, nose along +z): dark back, pale grooved belly, flippers, tail flukes,
       eyes, and a dark mouth inside; the lower jaw is its own piece, hinged so it can open */
    batch_clear(&g_b);
    { const int R = 10, S = 14; const float rx = 1.7f, ry = 1.6f, rz = 5.0f;
      for (int ri = 0; ri < R; ri++) for (int si = 0; si < S; si++) {
          float t0 = 3.14159265f * ri / R, t1 = 3.14159265f * (ri + 1) / R, p0 = 6.2831853f * si / S, p1 = 6.2831853f * (si + 1) / S;
          Vec3 q[4]; float shade[4];
          float tt[4] = {t0, t1, t1, t0}, pp[4] = {p0, p0, p1, p1};
          for (int k = 0; k < 4; k++) { float st = sinf(tt[k]), ct = cosf(tt[k]); Vec3 n = v3(st * cosf(pp[k]), st * sinf(pp[k]), ct);   /* z is the long axis */
              float taper = n.z < 0 ? 1.0f - 0.45f * n.z * n.z : 1.0f;   /* thinner toward the tail */
              q[k] = v3(n.x * rx * taper, n.y * ry * taper, n.z * rz); shade[k] = n.y; }
          Vec3 nn = face_normal(q[0], q[1], q[2]); if (nn.x * q[0].x + nn.y * q[0].y + nn.z * q[0].z < 0) { Vec3 tmp = q[1]; q[1] = q[3]; q[3] = tmp; nn = v3(-nn.x, -nn.y, -nn.z); }
          static const int ix[6] = {0, 1, 2, 0, 2, 3};
          for (int k = 0; k < 6; k++) { int c = ix[k]; float belly = shade[c] < -0.35f ? 1.0f : 0.0f, groove = belly > 0 && ((si & 1) == 0) ? 0.85f : 1.0f;
              float r_ = belly ? 0.86f * groove : 0.22f + 0.08f * shade[c], g_ = belly ? 0.86f * groove : 0.31f + 0.08f * shade[c], b_ = belly ? 0.8f * groove : 0.45f + 0.08f * shade[c];
              Vertex v = {q[c].x, q[c].y, q[c].z, nn.x, nn.y, nn.z, 0, 0, r_, g_, b_, 1}; if (g_b.n < g_b.cap) g_b.v[g_b.n++] = v; } }
      /* flippers and tail flukes: flat, both sides drawn */
      Vec3 fl[3][3] = { {v3(1.4f, -0.6f, 1.6f), v3(3.4f, -1.3f, 0.2f), v3(1.5f, -0.7f, 0.2f)}, {v3(-1.4f, -0.6f, 1.6f), v3(-1.5f, -0.7f, 0.2f), v3(-3.4f, -1.3f, 0.2f)},
                        {v3(0, 0, -4.6f), v3(2.6f, 0.3f, -6.4f), v3(-2.6f, 0.3f, -6.4f)} };
      for (int f = 0; f < 3; f++) for (int sd = 0; sd < 2; sd++) { Vec3 a = fl[f][0], b = sd ? fl[f][2] : fl[f][1], c = sd ? fl[f][1] : fl[f][2]; Vec3 n = face_normal(a, b, c);
          Vertex v[3] = { {a.x, a.y, a.z, n.x, n.y, n.z, 0, 0, 0.24f, 0.32f, 0.46f, 1}, {b.x, b.y, b.z, n.x, n.y, n.z, 0, 0, 0.2f, 0.28f, 0.42f, 1}, {c.x, c.y, c.z, n.x, n.y, n.z, 0, 0, 0.2f, 0.28f, 0.42f, 1} };
          for (int k = 0; k < 3; k++) if (g_b.n < g_b.cap) g_b.v[g_b.n++] = v[k]; }
      for (int sd = -1; sd <= 1; sd += 2) { batch_box(&g_b, v3(sd * 1.25f, 0.35f, 3.3f), v3(0.08f, 0.22f, 0.22f), 0.95f, 0.95f, 0.95f); batch_box(&g_b, v3(sd * 1.31f, 0.33f, 3.36f), v3(0.04f, 0.12f, 0.12f), 0.05f, 0.05f, 0.08f); }   /* eyes */
      batch_box(&g_b, v3(0, -0.75f, 2.6f), v3(1.2f, 0.05f, 2.2f), 0.35f, 0.06f, 0.1f);   /* the roof of the mouth, dark red */
    }
    m_whale = gfx_mesh_static(g_b.v, g_b.n);
    batch_clear(&g_b);
    { /* the lower jaw: a long scoop from the hinge (z = 0) forward, pale outside, a dark red tongue inside */
      const int N = 10;
      for (int i = 0; i < N; i++) { float a0 = 3.14159265f * i / N, a1 = 3.14159265f * (i + 1) / N;
          Vec3 p0 = v3(cosf(a0) * 1.45f, -sinf(a0) * 0.75f, 0), p1 = v3(cosf(a1) * 1.45f, -sinf(a1) * 0.75f, 0);
          Vec3 f0 = v3(p0.x * 0.35f, p0.y * 0.5f, 4.9f), f1 = v3(p1.x * 0.35f, p1.y * 0.5f, 4.9f);
          batch_quad4(&g_b, p1, p0, f0, f1, v3(cosf((a0 + a1) / 2), -sinf((a0 + a1) / 2), 0), 0, 0, 1, 1, 0.84f, 0.84f, 0.78f, 1);
          batch_quad4(&g_b, p0, p1, f1, f0, v3(0, 1, 0), 0, 0, 1, 1, 0.55f, 0.1f, 0.14f, 1); }
    }
    m_whale_jaw = gfx_mesh_static(g_b.v, g_b.n);
    /* start line across the road */
    batch_clear(&g_b);
    { Vec3 p0 = road_point(t, 0, -t->s[0].width * 0.5f), p1 = road_point(t, 0, t->s[0].width * 0.5f), p2 = road_point(t, 2, t->s[2].width * 0.5f), p3 = road_point(t, 2, -t->s[2].width * 0.5f);
      p0.y += 0.03f; p1.y += 0.03f; p2.y += 0.03f; p3.y += 0.03f;
      batch_quad4(&g_b, p0, p1, p2, p3, v3(0, 1, 0), 0, 0, 4, 1, 1, 1, 1, 1); }
    m_startline = gfx_mesh_static(g_b.v, g_b.n);
    /* boost pads (Sean): a raised, glowing golden Jesus fish - the ichthys, the early Christians' sign - its head pointing the
       way the racers drive, on a pool of soft light. Three solid pieces: the body (the two arcs meeting at the tail) and the
       two points of the tail where the arcs cross and carry on. */
    Batch keep_b = g_b; batch_init(&g_b, far_buf(), FAR_CAP); batch_clear(&g_b);   /* a road can have 30 of them: the big buffer */
    {
      enum { DN = 3 }; static int dn[DN]; static float dp[DN][32][2];
      { int n = 0; for (int k = 0; k <= 13; k++) { float u = (float)k / 13; dp[0][n][0] = 0.4f * sinf(3.14159265f * u); dp[0][n][1] = 1.0f - 1.42f * u; n++; }   /* one arc, head to tail */
        for (int k = 12; k >= 1; k--) { float u = (float)k / 13; dp[0][n][0] = -0.4f * sinf(3.14159265f * u); dp[0][n][1] = 1.0f - 1.42f * u; n++; }   /* and back along the other */
        dn[0] = n;
        float tl[3][2] = {{0.0f, -0.42f}, {0.44f, -0.98f}, {0.16f, -0.86f}};
        for (int q = 0; q < 3; q++) { dp[1][q][0] = tl[q][0]; dp[1][q][1] = tl[q][1]; dp[2][2 - q][0] = -tl[q][0]; dp[2][2 - q][1] = tl[q][1]; }
        dn[1] = dn[2] = 3; }
      for (int p = 0; p < t->npad; p++) { const RoadPad *pd = &t->pad[p]; if (pd->kind != PAD_BOOST) continue;
        int ci = (pd->idx + 3) % t->n; const TrackSample *cs = &t->s[ci]; float spacing = t->length / (float)t->n, W = pd->halfw * 1.05f, F = pd->halfw * 1.25f;
        for (int part = 0; part < DN; part++) {
            int pn = dn[part], wingp = 0, tailp = part > 0;
            float th = part == 0 ? 0.38f : 0.26f;
            Vec3 top[32], bot[32];
            for (int k = 0; k < pn; k++) { float lx = dp[part][k][0], lz = dp[part][k][1];
                float lat = pd->lateral + lx * W, fwd = lz * F; int si = (ci + (int)lroundf(fwd / spacing) + t->n) % t->n; const TrackSample *ss = &t->s[si];
                float along = fwd - (float)lroundf(fwd / spacing) * spacing;
                Vec3 base = road_point(t, si, lat); base.x += ss->tx * along; base.z += ss->tz * along; base.y += 0.06f;
                float lift = 0; (void)wingp; (void)tailp;
                float h = th * (part == 0 ? (0.55f + 0.45f * (1.0f - fabsf(lx) / 0.42f)) : 1.0f);   /* the body domed along its middle */
                bot[k] = v3(base.x, base.y + lift * 0.5f, base.z); top[k] = v3(base.x, base.y + lift + h, base.z); }
            (void)cs;
            /* the top: bright white-gold */
            Vec3 mid = v3(0, 0, 0); for (int k = 0; k < pn; k++) { mid.x += top[k].x / pn; mid.y += top[k].y / pn; mid.z += top[k].z / pn; }
            for (int k = 0; k < pn; k++) { Vec3 tri[3] = {mid, top[k], top[(k + 1) % pn]}; for (int q = 0; q < 3; q++) far_vert(&g_b, tri[q], v3(0, 1, 0), 0.5f, 0.5f, 1.0f, 0.97f, 0.78f); }
            /* the edges: deeper gold, lit from one side */
            for (int k = 0; k < pn; k++) { int k2 = (k + 1) % pn; Vec3 e = v3_sub(top[k2], top[k]); Vec3 n = v3_norm(v3(e.z, 0, -e.x));
                float sh = 0.7f + 0.3f * (n.x * 0.5f + n.z * 0.5f + 0.5f); batch_quad4(&g_b, bot[k], bot[k2], top[k2], top[k], n, 0, 0, 1, 1, 1.0f * sh, 0.72f * sh, 0.22f * sh, 1); }
        }
      } }
    m_boostpads = gfx_mesh_static(g_b.v, g_b.n); g_b = keep_b;
    batch_clear(&g_b);
    for (int p = 0; p < t->npad; p++) { const RoadPad *pd = &t->pad[p]; if (pd->kind != PAD_BOOST) continue;
        int ci = (pd->idx + 3) % t->n; const TrackSample *cs = &t->s[ci]; Vec3 c = road_point(t, ci, pd->lateral); c.y += 0.05f; float r = pd->halfw * 1.9f;
        batch_quad(&g_b, c, v3(cs->nx * r, 0, cs->nz * r), v3(cs->tx * r * 1.15f, 0, cs->tz * r * 1.15f), v3(0, 1, 0), 0, 0, 1, 1, 1.0f, 0.85f, 0.4f, 1); }
    gfx_mesh_free(m_boostglow); m_boostglow = gfx_mesh_static(g_b.v, g_b.n);
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
        /* nothing over the road (Sean): the two halves of the picture stand as gates flung open, one each side, swung out toward the racers */
        float ew = track_edge(t, fi), open = 1.15f;
        for (int sd = -1; sd <= 1; sd += 2) { Vec3 hinge = v3(c.x + fs->nx * sd * (ew + 0.5f), c.y, c.z + fs->nz * sd * (ew + 0.5f));
            Vec3 dir = v3(fs->nx * sd * cosf(open) - fs->tx * sinf(open), 0, fs->nz * sd * cosf(open) - fs->tz * sinf(open));   /* from the hinge out along the gate */
            Vec3 mid = v3(hinge.x + dir.x * hw * 0.5f, c.y + hh * 0.5f, hinge.z + dir.z * hw * 0.5f);
            float u0 = sd < 0 ? 0.5f : 0.5f, u1 = sd < 0 ? 0.0f : 1.0f;   /* each half of the picture, its outer edge at the far end */
            batch_quad(&g_b, mid, v3(dir.x * hw * 0.5f, 0, dir.z * hw * 0.5f), v3(0, hh * 0.5f, 0), v3(-fs->tx, 0, -fs->tz), u0, 0, u1, 1, 1, 1, 1, 1); } }
    m_finale = gfx_mesh_static(g_b.v, g_b.n);
    /* tunnels: a roof over the road and walls up to it, with an arch ring at each end */
    batch_clear(&g_b);
    for (int tn = 0; tn < 0 /* no roofs over the road any more (Sean) */ && tn < t->ntunnel; tn++) { const Tunnel *tu = &t->tunnel[tn]; int len = (tu->to - tu->from + t->n) % t->n;
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
      /* no banner across the road: a checkered flag flies from the top of each pillar instead, out over the shoulder */
      for (int sd = -1; sd <= 1; sd += 2) { Vec3 top = road_point(t, i, sd * (e + 0.5f)); for (int fx = 0; fx < 4; fx++) for (int fy = 0; fy < 3; fy++) { float c = ((fx + fy) & 1) ? 0.05f : 1.0f;
          batch_box(&g_b, v3(top.x + t->s[i].nx * sd * (0.45f + 0.35f + fx * 0.7f), top.y + 5.6f + fy * 0.5f, top.z + t->s[i].nz * sd * (0.45f + 0.35f + fx * 0.7f)), v3(0.35f, 0.25f, 0.05f), c, c, c); } } }
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
    { batch_quad(&g_b, v3(0, g_ground_y, 0), v3(600, 0, 0), v3(0, 0, -600), v3(0, 1, 0), 0, 0, 120, 120, 1, 1, 1, 1); }
    m_ground = gfx_mesh_static(g_b.v, g_b.n);
    /* real hills beyond the walls (Bethlehem first): a grid of land that stays flat near the road and rises into rolling
       hills further out, fading back down at the edge of the grid where the flat ground carries on */
    if (!t->water && g_def && !g_def->finale_img) {   /* (Sean: the whole world was flat) every land road now */
        enum { GN = 72 }; static float hy[GN + 1][GN + 1]; float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
        for (int i = 0; i < t->n; i++) { if (t->s[i].x < x0) x0 = t->s[i].x; if (t->s[i].x > x1) x1 = t->s[i].x; if (t->s[i].z < z0) z0 = t->s[i].z; if (t->s[i].z > z1) z1 = t->s[i].z; }
        x0 -= 220; x1 += 220; z0 -= 220; z1 += 220; float cx = (x0 + x1) / 2, cz = (z0 + z1) / 2, half = fmaxf(x1 - x0, z1 - z0) / 2, cell = 2 * half / GN;
        for (int a = 0; a <= GN; a++) for (int b = 0; b <= GN; b++) { float x = cx - half + a * cell, z = cz - half + b * cell, dmin = 1e9f;
            for (int i = 0; i < t->n; i += 2) { float dx = x - t->s[i].x, dz = z - t->s[i].z, d = sqrtf(dx * dx + dz * dz) - track_bound(t, i, 1) * 0.5f - track_bound(t, i, -1) * 0.5f; if (d < dmin) dmin = d; }
            float rise = clampf((dmin - 12.0f) / 45.0f, 0, 1); rise = rise * rise * (3 - 2 * rise);
            float edge = fminf(fminf(a, GN - a), fminf(b, GN - b)) / 6.0f; edge = clampf(edge, 0, 1);
            float n = smooth_noise(x * 0.012f, z * 0.012f) * 0.65f + smooth_noise(x * 0.03f + 7, z * 0.03f) * 0.35f;
            hy[a][b] = g_ground_y + rise * edge * (6.0f + 30.0f * n * n); }
        Batch save = g_b; batch_init(&g_b, far_buf(), FAR_CAP); batch_clear(&g_b);
        for (int a = 0; a < GN; a++) for (int b = 0; b < GN; b++) { if (g_b.n + 6 > g_b.cap) break;
            float xa = cx - half + a * cell, za = cz - half + b * cell; int ia[4] = {a, a + 1, a + 1, a}, ib[4] = {b, b, b + 1, b + 1}; Vertex q[4];
            if (hy[a][b] <= g_ground_y + 0.01f && hy[a + 1][b] <= g_ground_y + 0.01f && hy[a][b + 1] <= g_ground_y + 0.01f && hy[a + 1][b + 1] <= g_ground_y + 0.01f) continue;   /* flat: the ground plane is already there */
            for (int k = 0; k < 4; k++) { int A = ia[k], B = ib[k]; float hxp = hy[A < GN ? A + 1 : A][B] - hy[A > 0 ? A - 1 : A][B], hzp = hy[A][B < GN ? B + 1 : B] - hy[A][B > 0 ? B - 1 : B];
                Vec3 n = v3(-hxp, 2 * cell, -hzp); float l = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z); n = v3(n.x / l, n.y / l, n.z / l);
                float x = xa + (A - a) * cell, z = za + (B - b) * cell, y = hy[A][B], sh = 0.8f + 0.2f * n.y, dry = clampf((y - g_ground_y) / 40.0f, 0, 1);   /* higher up: drier, sandier */
                Vertex v = {x, y - 0.05f, z, n.x, n.y, n.z, x / 10.0f, z / 10.0f, sh * (1 + 0.25f * dry), sh * (1 + 0.1f * dry), sh * (1 - 0.15f * dry), 1}; q[k] = v; }
            static const int ix[6] = {0, 2, 1, 0, 3, 2}; for (int k = 0; k < 6; k++) g_b.v[g_b.n++] = q[ix[k]]; }
        m_hills = g_b.n ? gfx_mesh_static(g_b.v, g_b.n) : NULL; g_b = save; }
    build_far_world(t);
    m_skid = gfx_mesh_dynamic(MAX_SKID * 6);
    m_dust = gfx_mesh_dynamic(MAX_DUST * 6);
    m_spark = gfx_mesh_dynamic(MAX_SPARK * 6);
    m_hud = gfx_mesh_dynamic(32768);   /* the 3D menu plaques take a lot of corners */
    batch_init(&g_b, g_vtmp, 8192);   /* back to the size every per-frame mesh is made for */
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
    /* the blessing box: six see-through faces with a cross, inside a solid white frame of twelve rounded edges */
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
    { Vertex q[6]; memset(q, 0, sizeof q); const float qx[6] = { -1, 1, 1, -1, 1, -1 }, qz[6] = { -1, -1, 1, -1, 1, 1 };   /* a flat square on the ground, 2 wide */
      for (int i = 0; i < 6; i++) { q[i].px = qx[i]; q[i].pz = qz[i]; q[i].ny = 1; q[i].u = 0.5f; q[i].v = 0.5f; q[i].r = q[i].g = q[i].b = q[i].a = 1; }
      gfx_mesh_free(m_shsquare); m_shsquare = gfx_mesh_static(q, 6); }
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
/* ---- the 3D menus (Sean: the menus were plain and retro) ----
   Menu pieces are thick bevelled plaques - a deep blue or gold face with a gloss on top, a lit gold rim, sides you see
   when they tilt, and a soft shadow - that can lift toward you, tilt and turn. Everything in the menu batch is laid out
   in the usual 1280 x 720 screen, with z (in pixels, toward the viewer) kept in the vertex; ui_end() turns a run of
   vertices by a matrix in screen pixels, and the whole batch is drawn through gfx_camera_ui3d(). Only the main TV and
   GamePad passes do this (g_ui3d); anywhere else the same calls draw flat. */
#define UI_EYE 1100.0f
static int g_ui3d = 0; static uint32_t g_ui_conv = 0; static Mat4 g_ui_screen; static int g_mi = 0;
static float ui_ease(float t) { t = clampf(t, 0, 1); return 1 - (1 - t) * (1 - t) * (1 - t); }
static void ui_flush(void) {   /* everything not yet moved into screen pixels goes there now, flat */
    if (!g_ui3d) return; for (uint32_t i = g_ui_conv; i < g_b.n; i++) { Vertex *v = &g_b.v[i]; v->px = (v->px + 1.0f) * 640.0f; v->py = (1.0f - v->py) * 360.0f; } g_ui_conv = g_b.n; }
static uint32_t ui_begin(void) { ui_flush(); return g_b.n; }
static void ui_end(uint32_t mark, const Mat4 *m) {
    if (!g_ui3d) return; ui_flush();
    for (uint32_t i = mark; i < g_b.n; i++) { Vertex *v = &g_b.v[i]; float x = v->px, y = v->py, z = v->pz;
        v->px = m->m[0] * x + m->m[4] * y + m->m[8] * z + m->m[12]; v->py = m->m[1] * x + m->m[5] * y + m->m[9] * z + m->m[13]; v->pz = m->m[2] * x + m->m[6] * y + m->m[10] * z + m->m[14]; } }
static Mat4 ui_xf(float cx, float cy, float lift, float rx, float ry, float rz, float s) {   /* about the point (cx, cy) on the screen: scale, turn, then lift toward the viewer */
    Mat4 m = m4_translate(-cx, -cy, 0); m = m4_mul(m4_scale(s, s, s), m); m = m4_mul(m4_rotate_z(rz), m); m = m4_mul(m4_rotate_x(rx), m); m = m4_mul(m4_rotate_y(ry), m);
    return m4_mul(m4_translate(cx, cy, lift), m); }
static void ui_v(float x, float y, float z, float r, float g, float b, float a) {   /* one corner, in screen pixels */
    if (g_b.n >= g_b.cap) return; Vertex v = {x / 640.0f - 1.0f, 1.0f - y / 360.0f, g_ui3d ? z : 0, 0, 0, 1, WHITE_U, WHITE_V, r, g, b, a}; g_b.v[g_b.n++] = v; }
static void ui_tri(const float *p0, const float *p1, const float *p2) {   /* x, y, z, r, g, b, a each */
    if (g_b.n + 3 > g_b.cap) return; ui_v(p0[0], p0[1], p0[2], p0[3], p0[4], p0[5], p0[6]); ui_v(p1[0], p1[1], p1[2], p1[3], p1[4], p1[5], p1[6]); ui_v(p2[0], p2[1], p2[2], p2[3], p2[4], p2[5], p2[6]); }
static int ui_round(float x, float y, float w, float h, float r, float *ox, float *oy, float *nx, float *ny) {   /* a rounded rectangle's outline, clockwise from the top left; with each point's outward direction */
    int n = 0; static const float cxs[4] = {0, 1, 1, 0}, cys[4] = {0, 0, 1, 1}; r = fminf(r, fminf(w, h) * 0.5f);
    for (int c = 0; c < 4; c++) { float ccx = x + (cxs[c] ? w - r : r), ccy = y + (cys[c] ? h - r : r), a0 = 3.14159265f + c * 1.5707963f;
        for (int k = 0; k <= 4; k++) { float a = a0 + k * 1.5707963f / 4; ox[n] = ccx + cosf(a) * r; oy[n] = ccy + sinf(a) * r; nx[n] = cosf(a); ny[n] = sinf(a); n++; } }
    return n; }
typedef struct { float top[3], bot[3], rim[3], side[3]; } UiLook;
static const UiLook UI_BLUE = {{0.16f, 0.27f, 0.62f}, {0.05f, 0.09f, 0.27f}, {0.95f, 0.76f, 0.32f}, {0.42f, 0.3f, 0.1f}};
static const UiLook UI_GOLD = {{1.0f, 0.93f, 0.6f}, {0.92f, 0.6f, 0.16f}, {1.0f, 0.97f, 0.82f}, {0.55f, 0.36f, 0.08f}};
static const UiLook UI_DIM  = {{0.18f, 0.2f, 0.3f}, {0.07f, 0.08f, 0.14f}, {0.55f, 0.58f, 0.66f}, {0.22f, 0.23f, 0.28f}};
/* a plaque: shadow, glow (0..1), sides going back 'depth', a lit bevelled rim, a graded face, a gloss, and a shine that
   sweeps across when 'shine' >= 0 (its place, 0..1) */
static void ui_plaque(float x, float y, float w, float h, float rad, float depth, const UiLook *L, float alpha, float glow, float shine) {
    float ox[20], oy[20], nx[20], ny[20], ix[20], iy[20]; int n = ui_round(x, y, w, h, rad, ox, oy, nx, ny); float bev = fminf(7.0f, fminf(w, h) * 0.12f);
    { float tx[20], ty[20], tnx[20], tny[20]; ui_round(x + bev, y + bev, w - 2 * bev, h - 2 * bev, fmaxf(rad - bev, 2.0f), tx, ty, tnx, tny); for (int i = 0; i < n; i++) { ix[i] = tx[i]; iy[i] = ty[i]; } }
    const float Lx = -0.55f, Ly = -0.8f;   /* the light comes from the top left */
    for (int i = 0; i < n; i++) { int j = (i + 1) % n;   /* soft shadow below and to the right */
        float a0[7] = {ox[i] + 7, oy[i] + 11, -depth - 1, 0, 0, 0.1f, 0.42f * alpha}, a1[7] = {ox[j] + 7, oy[j] + 11, -depth - 1, 0, 0, 0.1f, 0.42f * alpha};
        float b0[7] = {ox[i] + 7 + nx[i] * 16, oy[i] + 11 + ny[i] * 16, -depth - 1, 0, 0, 0.1f, 0}, b1[7] = {ox[j] + 7 + nx[j] * 16, oy[j] + 11 + ny[j] * 16, -depth - 1, 0, 0, 0.1f, 0};
        float c0[7] = {x + w / 2 + 7, y + h / 2 + 11, -depth - 1, 0, 0, 0.1f, 0.42f * alpha};
        ui_tri(c0, a0, a1); ui_tri(a0, b0, b1); ui_tri(a0, b1, a1); }
    if (glow > 0.01f) for (int i = 0; i < n; i++) { int j = (i + 1) % n; float gr = 1.0f, gg = 0.85f, gb = 0.4f, ga = 0.6f * glow * alpha, R = 26 + 6 * glow;
        float a0[7] = {ox[i], oy[i], -depth, gr, gg, gb, ga}, a1[7] = {ox[j], oy[j], -depth, gr, gg, gb, ga}, b0[7] = {ox[i] + nx[i] * R, oy[i] + ny[i] * R, -depth, gr, gg, gb, 0}, b1[7] = {ox[j] + nx[j] * R, oy[j] + ny[j] * R, -depth, gr, gg, gb, 0};
        ui_tri(a0, b0, b1); ui_tri(a0, b1, a1); }
    for (int i = 0; i < n; i++) { int j = (i + 1) % n; float sh = 0.55f + 0.35f * (nx[i] * Lx + ny[i] * Ly) * -1.0f; sh = clampf(0.35f + 0.3f * (1 - (nx[i] * Lx + ny[i] * Ly)), 0.3f, 0.95f);   /* the sides */
        float a0[7] = {ox[i], oy[i], -depth, L->side[0] * sh, L->side[1] * sh, L->side[2] * sh, alpha}, a1[7] = {ox[j], oy[j], -depth, L->side[0] * sh, L->side[1] * sh, L->side[2] * sh, alpha};
        float b0[7] = {ox[i], oy[i], 0, L->side[0] * sh * 1.2f, L->side[1] * sh * 1.2f, L->side[2] * sh * 1.2f, alpha}, b1[7] = {ox[j], oy[j], 0, L->side[0] * sh * 1.2f, L->side[1] * sh * 1.2f, L->side[2] * sh * 1.2f, alpha};
        ui_tri(a0, b0, b1); ui_tri(a0, b1, a1); }
    for (int i = 0; i < n; i++) { int j = (i + 1) % n;   /* the bevelled rim: bright where it faces the light, darker away from it */
        float si = clampf(0.78f + 0.42f * (nx[i] * Lx + ny[i] * Ly), 0.45f, 1.25f), sj = clampf(0.78f + 0.42f * (nx[j] * Lx + ny[j] * Ly), 0.45f, 1.25f);
        float a0[7] = {ox[i], oy[i], 0, L->rim[0] * si, L->rim[1] * si, L->rim[2] * si, alpha}, a1[7] = {ox[j], oy[j], 0, L->rim[0] * sj, L->rim[1] * sj, L->rim[2] * sj, alpha};
        float b0[7] = {ix[i], iy[i], 5, L->rim[0] * si * 0.85f, L->rim[1] * si * 0.85f, L->rim[2] * si * 0.85f, alpha}, b1[7] = {ix[j], iy[j], 5, L->rim[0] * sj * 0.85f, L->rim[1] * sj * 0.85f, L->rim[2] * sj * 0.85f, alpha};
        ui_tri(a0, b0, b1); ui_tri(a0, b1, a1); }
    { float fy0 = y + bev, fh = h - 2 * bev; if (fh < 1) fh = 1;   /* the face: lighter at the top */
      float cy = y + h / 2, t = (cy - fy0) / fh; float c0[7] = {x + w / 2, cy, 5, L->top[0] + (L->bot[0] - L->top[0]) * t, L->top[1] + (L->bot[1] - L->top[1]) * t, L->top[2] + (L->bot[2] - L->top[2]) * t, alpha};
      for (int i = 0; i < n; i++) { int j = (i + 1) % n; float ti = clampf((iy[i] - fy0) / fh, 0, 1), tj = clampf((iy[j] - fy0) / fh, 0, 1);
          float a0[7] = {ix[i], iy[i], 5, L->top[0] + (L->bot[0] - L->top[0]) * ti, L->top[1] + (L->bot[1] - L->top[1]) * ti, L->top[2] + (L->bot[2] - L->top[2]) * ti, alpha};
          float a1[7] = {ix[j], iy[j], 5, L->top[0] + (L->bot[0] - L->top[0]) * tj, L->top[1] + (L->bot[1] - L->top[1]) * tj, L->top[2] + (L->bot[2] - L->top[2]) * tj, alpha};
          ui_tri(c0, a0, a1); }
      /* the gloss over the top half */
      float gx0 = x + bev + 6, gx1 = x + w - bev - 6, gy0 = fy0 + 2, gy1 = fy0 + fh * 0.48f, ga = 0.2f * alpha;
      if (gx1 > gx0) { float p0[7] = {gx0, gy0, 5.2f, 1, 1, 1, ga}, p1[7] = {gx1, gy0, 5.2f, 1, 1, 1, ga}, p2[7] = {gx1, gy1, 5.2f, 1, 1, 1, 0}, p3[7] = {gx0, gy1, 5.2f, 1, 1, 1, 0}; ui_tri(p0, p1, p2); ui_tri(p0, p2, p3); }
      if (shine >= 0 && gx1 > gx0) {   /* a bright streak sliding across */
          float bw = fminf(w * 0.18f, 70), sx = gx0 - bw * 2 + (gx1 - gx0 + bw * 3) * shine, k = 0.45f, fy1 = fy0 + fh;
          float xs[4] = {sx, sx + bw, sx + bw + k * fh, sx + k * fh}, ys[4] = {fy0, fy0, fy1, fy1};
          for (int q = 0; q < 4; q++) xs[q] = clampf(xs[q], gx0 - 4, gx1 + 4);
          if (xs[1] > xs[0] + 1 || xs[2] > xs[3] + 1) { float m0 = (xs[0] + xs[1]) / 2, m1 = (xs[3] + xs[2]) / 2, sa = 0.38f * alpha;
              float A[7] = {xs[0], ys[0], 5.4f, 1, 1, 1, 0}, B[7] = {m0, ys[0], 5.4f, 1, 1, 1, sa}, C[7] = {xs[1], ys[1], 5.4f, 1, 1, 1, 0}, D[7] = {xs[2], ys[2], 5.4f, 1, 1, 1, 0}, E[7] = {m1, ys[2], 5.4f, 1, 1, 1, sa}, F[7] = {xs[3], ys[3], 5.4f, 1, 1, 1, 0};
              ui_tri(A, B, E); ui_tri(A, E, F); ui_tri(B, C, D); ui_tri(B, D, E); } } }
}
static void ui_zrun(uint32_t from, float z) { if (!g_ui3d) return; for (uint32_t i = from; i < g_b.n; i++) g_b.v[i].pz = z; }   /* lift a run of flat pieces (words) onto a plaque's face */
static const UiLook UI_SEL  = {{0.24f, 0.4f, 0.82f}, {0.07f, 0.13f, 0.38f}, {1.0f, 0.93f, 0.6f}, {0.55f, 0.4f, 0.12f}};
static Mat4 g_cardm[16]; static uint32_t g_card_face;
/* a card on the character, kart or road screens: they flip in one after another; the chosen one lifts toward you,
   grows a little and sways */
static Mat4 card_xf(int i, float x, float y, float w, float h, int sel) {
    float t = ui_ease((g_screen_t - 0.08f - i * 0.045f) / 0.45f), s2 = sel ? 1.0f : 0.0f, wob = g_time * 1.9f + i * 0.7f;
    float lift = 55 * s2 - (1 - t) * 220, cx = x + w / 2, cy = y + h / 2;
    Mat4 m = ui_xf(cx, cy, lift, 0.05f * s2 * sinf(wob) + 0.012f * (1 - s2) * sinf(wob * 0.7f), 0.08f * s2 * sinf(wob * 0.8f) + (1 - t) * 1.4f, 0, 1 + 0.045f * s2);
    if (lift > 0) m = m4_mul(m4_translate((640 - cx) * lift / UI_EYE, (360 - cy) * lift / UI_EYE, 0), m);   /* lifting toward you also spreads a card out from the middle: keep it where it was */
    return m; }
static void card_face(float x, float y, float w, float h, int sel, int open) {   /* the card itself; what is drawn on it after this sits on its face */
    ui_plaque(x, y, w, h, 16, 12, open ? (sel ? &UI_SEL : &UI_BLUE) : &UI_DIM, 0.98f, sel ? 0.75f + 0.25f * sinf(g_time * 4) : 0, sel ? fmodf(g_time * 0.45f, 1.5f) : -1); g_card_face = g_b.n; }
static void ui_px(Vertex *v, uint32_t n, float z) { for (uint32_t i = 0; i < n; i++) { v[i].px = (v[i].px + 1.0f) * 640.0f; v[i].py = (1.0f - v[i].py) * 360.0f; v[i].pz = z; } }   /* a picture laid out flat, moved into screen pixels to ride on a card */
static Mat4 ui_card_model(int i) { return m4_mul(g_ui_screen, g_cardm[i & 15]); }
static void card_done(uint32_t mk, int i, float x, float y, float w, float h, int sel) { ui_zrun(g_card_face, 6); Mat4 m = card_xf(i, x, y, w, h, sel); ui_end(mk, &m); if (i < 16) g_cardm[i] = m; }
static void panel(float x, float y, float w, float h) { ui_plaque(x, y, w, h, 18, 14, &UI_BLUE, 0.96f, 0, -1); }
#define ROU_T 1.4f
static uint8_t g_rou_seq[MAX_HUMANS][ROU_N + 1]; static float g_rou_land[MAX_HUMANS]; static int g_rou_last[MAX_HUMANS];
static Mesh *m_rou[MAX_HUMANS][8];
static void rou_start(int p, int item) {
    static uint32_t seed = 12345; int prev = 0;
    for (int i = 0; i < ROU_N; i++) { int v; do { seed = seed * 1664525u + 1013904223u; v = 1 + (int)((seed >> 16) % (ITEM_COUNT - 1)); } while (v == prev || (i == ROU_N - 1 && v == item)); g_rou_seq[p][i] = (uint8_t)v; prev = v; }
    g_rou_seq[p][ROU_N] = (uint8_t)item; g_rou_land[p] = 99; g_rou_last[p] = 0;
}
static float rou_phase(int p) { float u = 1.0f - g_proulette[p] / ROU_T; if (u < 0) u = 0; if (u > 1) u = 1; return (1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u)) * ROU_N; }
static void icon_rect_3d(Batch *b, int item, float cx, float cy, float size, float sx, float sy, float clip0, float clip1, float shade) {   /* an icon squashed by sx, sy (the turn of the drum, the flip of the coin), shaded, clipped top and bottom */
    float asp = g_item_asp[item] > 0 ? g_item_asp[item] : 1.0f, avail = size * 0.88f;
    float iw = (asp >= 1 ? avail : avail * asp) * sx, ih = (asp >= 1 ? avail / asp : avail) * sy, ix = cx - iw / 2, iy = cy - ih / 2;
    if (ih < 1 || iw < 0.5f) return;
    float y0 = iy > clip0 ? iy : clip0, y1 = iy + ih < clip1 ? iy + ih : clip1; if (y1 <= y0) return;
    batch_rect_uv(b, ix, y0, iw, y1 - y0, 0, (y0 - iy) / ih, 1, (y1 - iy) / ih, shade, shade, shade, 1);
}
/* (Sean: the item roulette was flat) a real 3D slot-machine drum: eight faces round a turning drum, seen in perspective
   and tipped a little toward you, with gold bands at both ends; it spins up, slows and clicks onto your item, which then
   flips out of the drum like a gold coin and keeps turning gently. Single-player screen; the split screens keep the
   flat version. Corners are in screen pixels with z toward the viewer (gfx_camera_ui3d). */
static void rv(Batch *b, Vec3 p, Vec3 n, float u, float v, float sh, float r, float g, float bl) { if (b->n >= b->cap) return; Vertex q = {p.x, p.y, p.z, n.x, n.y, n.z, u, v, r * sh, g * sh, bl * sh, 1}; b->v[b->n++] = q; }
static Vec3 rot_y_about(Vec3 p, Vec3 c, float a) { float dx = p.x - c.x, dz = p.z - c.z, ca = cosf(a), sa = sinf(a); return v3(c.x + dx * ca + dz * sa, p.y, c.z - dx * sa + dz * ca); }
/* ---- the item slot in 3D, in every mode (Sean: the box was flat - "needs to be 3 dimensional") ---- */
static Mesh *m_slot[MAX_HUMANS][3]; static Vertex g_slotv[900];
static Vec3 xf_about(Vec3 p, Vec3 c, const Mat4 *m) { Vec3 d = v3(p.x - c.x, p.y - c.y, p.z - c.z); return v3(c.x + m->m[0] * d.x + m->m[4] * d.y + m->m[8] * d.z, c.y + m->m[1] * d.x + m->m[5] * d.y + m->m[9] * d.z, c.z + m->m[2] * d.x + m->m[6] * d.y + m->m[10] * d.z); }
/* a box with its own turn, each face shaded by how much it faces the light (up-left, toward you); faces turned away are left out */
static void slot_box(Batch *b, Vec3 c, Vec3 h, const Mat4 *rot, Vec3 pivot, float r, float g, float bl, float a) {
    static const int F[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {4, 5, 1, 0}, {3, 2, 6, 7}};
    Vec3 P[8];
    const float sx[8] = {-1, 1, 1, -1, -1, 1, 1, -1}, sy[8] = {-1, -1, 1, 1, -1, -1, 1, 1}, sz[8] = {1, 1, 1, 1, -1, -1, -1, -1};
    for (int i = 0; i < 8; i++) P[i] = xf_about(v3(c.x + sx[i] * h.x, c.y + sy[i] * h.y, c.z + sz[i] * h.z), pivot, rot);
    for (int f = 0; f < 6; f++) { Vec3 A = P[F[f][0]], B = P[F[f][1]], C = P[F[f][2]];
        Vec3 n = v3_norm(v3_cross(v3_sub(B, A), v3_sub(C, A))); if (n.z < 0.02f) continue;   /* z toward you, y down: a face you can see points at you */
        float sh = 0.45f + 0.35f * n.z + 0.3f * fmaxf(0, -n.y) + 0.12f * fmaxf(0, -n.x);
        Vec3 Q[4] = {P[F[f][0]], P[F[f][1]], P[F[f][2]], P[F[f][3]]}; static const int T[6] = {0, 1, 2, 0, 2, 3};
        for (int k = 0; k < 6 && b->n < b->cap; k++) { Vertex v = {Q[T[k]].x, Q[T[k]].y, Q[T[k]].z, 0, 0, 1, WHITE_U, WHITE_V, r * sh, g * sh, bl * sh, a}; b->v[b->n++] = v; } } }
/* the frame of the slot, for the split-screen views (the full screen has its own 3D plaque): a gold rim with depth and a deep blue well */
static void slot_frame(Batch *b, float x, float y, float size, float sway) {
    Vec3 c = v3(x + size / 2, y + size / 2, 0); Mat4 rot = m4_mul(m4_rotate_y(sway), m4_rotate_x(-0.12f));
    float H = size / 2 + 7, t = 6, d = 9;
    slot_box(b, v3(c.x, c.y, -d - 2), v3(H - 2, H - 2, 1), &rot, c, 0.06f, 0.1f, 0.3f, 0.92f);                 /* the well */
    slot_box(b, v3(c.x, c.y - H + t / 2, 0), v3(H, t / 2, d), &rot, c, 1, 0.8f, 0.32f, 1);                    /* top, bottom, left, right rims */
    slot_box(b, v3(c.x, c.y + H - t / 2, 0), v3(H, t / 2, d), &rot, c, 1, 0.8f, 0.32f, 1);
    slot_box(b, v3(c.x - H + t / 2, c.y, 0), v3(t / 2, H - t, d), &rot, c, 1, 0.8f, 0.32f, 1);
    slot_box(b, v3(c.x + H - t / 2, c.y, 0), v3(t / 2, H - t, d), &rot, c, 1, 0.8f, 0.32f, 1); }
/* the empty slot: a little glass blessing box with a gold cross inside, turning and bobbing, glowing now and then */
static void slot_empty(Batch *b, float cx, float cy, float size) {
    float s = size * 0.27f, bob = sinf(g_time * 2.4f) * size * 0.04f, glow = 0.5f + 0.5f * sinf(g_time * 3.1f);
    Vec3 c = v3(cx, cy + bob, size * 0.12f); Mat4 rot = m4_mul(m4_rotate_x(0.42f + 0.08f * sinf(g_time * 1.7f)), m4_rotate_y(g_time * 1.3f));
    slot_box(b, c, v3(s * 0.16f, s * 0.78f, s * 0.16f), &rot, c, 1, 0.85f, 0.35f, 1);                                  /* the cross */
    slot_box(b, v3(c.x, c.y - s * 0.22f, c.z), v3(s * 0.52f, s * 0.16f, s * 0.16f), &rot, c, 1, 0.85f, 0.35f, 1);
    slot_box(b, c, v3(s, s, s), &rot, c, 0.55f + 0.3f * glow, 0.75f + 0.2f * glow, 1, 0.32f + 0.1f * glow); }       /* the glass box round it */
static void draw_item_drum(int p, float x, float y, float size, float vw, float vh, int frame) {
    Kart *k = &g_race.k[human_kart(p)]; Mat4 id = m4_identity();
    float cx = x + size / 2, cy = y + size / 2, R = size * 0.62f, W = size * 0.86f; const int NF = 8; const float step = 6.2831853f / NF;
    float tilt = 0.32f + 0.06f * sinf(g_time * 1.3f); Vec3 ctr = v3(cx, cy, -R);
    gfx_camera_ui3d_v(UI_EYE, vw, vh); gfx_state(ST_HUD); int mi = 0; Vertex vb[64]; Batch b;
    for (int j = 0; j < 3; j++) if (!m_slot[p][j]) m_slot[p][j] = gfx_mesh_dynamic(900);
    if (frame) { batch_init(&b, g_slotv, 900); slot_frame(&b, x, y, size, 0.16f * sinf(g_time * 0.9f + p)); gfx_mesh_update(m_slot[p][0], b.v, b.n); gfx_bind(tx_font); gfx_draw(m_slot[p][0], &id, 1, 1, 1, 1); }
    if (!k->item && g_proulette[p] <= 0) {
        batch_init(&b, g_slotv, 900); slot_empty(&b, cx, cy, size); gfx_mesh_update(m_slot[p][1], b.v, b.n); gfx_bind(tx_font); gfx_draw(m_slot[p][1], &id, 1, 1, 1, 1);
        gfx_camera_2d_virtual(vw, vh); return; }
    if (g_proulette[p] > 0) {
        float f = rou_phase(p); int i0 = (int)floorf(f);
        /* the drum's body: dark wood-and-gold faces behind the pictures, and the gold end bands */
        batch_init(&b, vb, 64);
        for (int s2 = -1; s2 <= 1; s2 += 2) for (int q = 0; q < NF; q++) { float a0 = (q - fmodf(f, 1.0f)) * step - 0.5f * step, a1 = a0 + step; if (cosf(a0 + step * 0.5f) < -0.1f) continue;
            Vec3 e0 = v3(cx + s2 * W * 0.5f, cy + sinf(a0) * R, -R + cosf(a0) * R), e1 = v3(cx + s2 * W * 0.5f, cy + sinf(a1) * R, -R + cosf(a1) * R);
            Vec3 o0 = v3(e0.x + s2 * 6, cy + sinf(a0) * (R + 4), -R + cosf(a0) * (R + 4)), o1 = v3(e1.x + s2 * 6, cy + sinf(a1) * (R + 4), -R + cosf(a1) * (R + 4));
            e0 = rot_y_about(e0, ctr, tilt); e1 = rot_y_about(e1, ctr, tilt); o0 = rot_y_about(o0, ctr, tilt); o1 = rot_y_about(o1, ctr, tilt);
            float sh = 0.55f + 0.45f * fmaxf(cosf(a0 + step * 0.5f), 0); Vec3 n = v3(0, 0, 1);
            rv(&b, e0, n, WHITE_U, WHITE_V, sh, 1, 0.8f, 0.3f); rv(&b, e1, n, WHITE_U, WHITE_V, sh, 1, 0.8f, 0.3f); rv(&b, o1, n, WHITE_U, WHITE_V, sh * 1.2f, 1, 0.88f, 0.45f);
            rv(&b, e0, n, WHITE_U, WHITE_V, sh, 1, 0.8f, 0.3f); rv(&b, o1, n, WHITE_U, WHITE_V, sh * 1.2f, 1, 0.88f, 0.45f); rv(&b, o0, n, WHITE_U, WHITE_V, sh * 1.2f, 1, 0.88f, 0.45f); }
        gfx_mesh_update(m_rou[p][mi], b.v, b.n); gfx_bind(tx_font); gfx_draw(m_rou[p][mi], &id, 1, 1, 1, 1); mi++;
        /* the faces, back ones first */
        int order[6], no = 0; for (int j = -2; j <= 3; j++) order[no++] = j;
        for (int a = 0; a < no; a++) for (int c = a + 1; c < no; c++) { float ta = fabsf((i0 + order[a] - f) * step), tc = fabsf((i0 + order[c] - f) * step); if (tc > ta) { int t2 = order[a]; order[a] = order[c]; order[c] = t2; } }
        for (int oi = 0; oi < no && mi < 8; oi++) { int idx = i0 + order[oi]; if (idx < 0 || idx > ROU_N) continue; int it = g_rou_seq[p][idx]; if (!tx_item[it]) continue;
            float th = (idx - f) * step; if (cosf(th) < 0.05f) continue;
            float a0 = th - step * 0.46f, a1 = th + step * 0.46f, asp = g_item_asp[it] > 0 ? g_item_asp[it] : 1, fw = W * 0.92f * (asp < 1 ? asp : 1);
            Vec3 c[4] = {v3(cx - fw / 2, cy + sinf(a0) * R, -R + cosf(a0) * R), v3(cx + fw / 2, cy + sinf(a0) * R, -R + cosf(a0) * R), v3(cx + fw / 2, cy + sinf(a1) * R, -R + cosf(a1) * R), v3(cx - fw / 2, cy + sinf(a1) * R, -R + cosf(a1) * R)};
            for (int q = 0; q < 4; q++) c[q] = rot_y_about(c[q], ctr, tilt);
            float sh = 0.35f + 0.65f * cosf(th); batch_init(&b, vb, 64); Vec3 n = v3(0, 0, 1);
            rv(&b, c[0], n, 0, 0, sh, 1, 1, 1); rv(&b, c[1], n, 1, 0, sh, 1, 1, 1); rv(&b, c[2], n, 1, 1, sh, 1, 1, 1); rv(&b, c[0], n, 0, 0, sh, 1, 1, 1); rv(&b, c[2], n, 1, 1, sh, 1, 1, 1); rv(&b, c[3], n, 0, 1, sh, 1, 1, 1);
            gfx_mesh_update(m_rou[p][mi], b.v, b.n); gfx_bind(tx_item[it]); gfx_draw(m_rou[p][mi], &id, 1, 1, 1, 1); mi++; } }
    else if (tx_item[k->item]) {   /* landed: a gold coin with the picture on it flips out and keeps turning */
        float t = g_rou_land[p] < 99 ? g_rou_land[p] : 1, spin = t < 0.7f ? (1.0f - (1.0f - t / 0.7f) * (1.0f - t / 0.7f)) * 12.566f : 0.45f * sinf(g_time * 2.2f);
        float pop = 1.0f + 0.22f * expf(-t * 6.0f) * cosf(t * 18.0f), bob = 2.5f * sinf(g_time * 3.0f) * (t > 0.7f), rr = size * 0.5f * pop, lift = 30 * expf(-t * 4.0f);
        Vec3 cc = v3(cx, cy + bob, lift); batch_init(&b, vb, 64); Vec3 n = v3(0, 0, 1);
        for (int q = 0; q < 16; q++) { float a0 = q * 6.2831853f / 16, a1 = (q + 1) * 6.2831853f / 16;   /* the gold coin behind the picture */
            Vec3 p0 = rot_y_about(v3(cx + cosf(a0) * rr, cc.y + sinf(a0) * rr, -3 + lift), cc, spin), p1 = rot_y_about(v3(cx + cosf(a1) * rr, cc.y + sinf(a1) * rr, -3 + lift), cc, spin);
            float sh = 0.75f + 0.25f * sinf(a0 - 2.4f); rv(&b, rot_y_about(v3(cx, cc.y, -3 + lift), cc, spin), n, WHITE_U, WHITE_V, 1.05f, 1, 0.82f, 0.32f); rv(&b, p0, n, WHITE_U, WHITE_V, sh, 1, 0.78f, 0.28f); rv(&b, p1, n, WHITE_U, WHITE_V, sh, 1, 0.78f, 0.28f); }
        gfx_mesh_update(m_rou[p][mi], b.v, b.n); gfx_bind(tx_font); gfx_draw(m_rou[p][mi], &id, 1, 1, 1, 1); mi++;
        float asp = g_item_asp[k->item] > 0 ? g_item_asp[k->item] : 1, iw = size * 0.72f * pop * (asp >= 1 ? 1 : asp), ih = size * 0.72f * pop / (asp >= 1 ? asp : 1);
        Vec3 c[4] = {v3(cx - iw / 2, cc.y - ih / 2, lift), v3(cx + iw / 2, cc.y - ih / 2, lift), v3(cx + iw / 2, cc.y + ih / 2, lift), v3(cx - iw / 2, cc.y + ih / 2, lift)};
        for (int q = 0; q < 4; q++) c[q] = rot_y_about(c[q], cc, spin);
        float sh = 0.55f + 0.45f * fabsf(cosf(spin)); batch_init(&b, vb, 64);
        rv(&b, c[0], n, 0, 0, sh, 1, 1, 1); rv(&b, c[1], n, 1, 0, sh, 1, 1, 1); rv(&b, c[2], n, 1, 1, sh, 1, 1, 1); rv(&b, c[0], n, 0, 0, sh, 1, 1, 1); rv(&b, c[2], n, 1, 1, sh, 1, 1, 1); rv(&b, c[3], n, 0, 1, sh, 1, 1, 1);
        gfx_mesh_update(m_rou[p][mi], b.v, b.n); gfx_bind(tx_item[k->item]); gfx_draw(m_rou[p][mi], &id, 1, 1, 1, 1); }
    gfx_camera_2d_virtual(vw, vh);
}
static void draw_item_window(int p, float x, float y, float size, float vw, float vh) {   /* the slot in 3D: frame (split screen), empty box, spinning drum or the item's coin */
    for (int j = 0; j < 8; j++) if (!m_rou[p][j]) m_rou[p][j] = gfx_mesh_dynamic(64);
    draw_item_drum(p, x, y, size, vw, vh, g_nplayers > 1);
}
static float text3d(const char *str, float x, float y, float size, float r, float g, float b, float a, int gold);
static void hud_item_window(int p, float x, float y, float size, int text_below) {   /* the frame and the name */
    Kart *k = &g_race.k[human_kart(p)];
    if (g_ui3d) ui_plaque(x - 8, y - 8, size + 16, size + 16, 16, 12, &UI_BLUE, 0.95f, g_proulette[p] > 0 ? 0.6f + 0.4f * sinf(g_time * 14) : 0.25f, g_proulette[p] > 0 ? fmodf(g_time * 1.6f, 1.4f) : -1);
    /* (split screen: the frame is drawn in 3D with the slot, see draw_item_drum) */
    if (!k->item) { if (g_proulette[p] <= 0) { const char *h = "ZL"; float fs = size * 0.16f; if (fs < 9) fs = 9; if (g_ui3d) batch_text(&g_b, h, x + size - text_width(h, fs) - 2, y + size - fs - 1, fs, 1, 0.9f, 0.55f, 0.85f); else text3d(h, x + size + 12, y + size * 0.5f - fs * 0.5f, fs * 1.2f, 1, 0.9f, 0.55f, 0.9f, 0); } return; }   /* split screen: beside the 3D frame, not behind it */
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
/* ---- 3D scenery: where a road has no picture for a building, column, tent or rock, it is built as a real model ---- */
static int prop_shape(const char *img) {
    static const char *names[] = {"#house1", "#house2", "#inn", "#stall", "#hut", "#tower", "#pillar", "#gpillar", "#tent", "#rock", "#well", "#wallseg"};
    static const int shape[] = {0, 0, 0, 0, 1, 2, 3, 3, 4, 5, 6, 7};
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) if (!strcmp(img, names[i])) return shape[i];
    return -1; }
static void prop_place(int n0, Vec3 at, float yaw) {   /* turn the model's corners (built round 0,0,0) to face yaw and stand them at 'at' */
    float c = cosf(yaw), s_ = sinf(yaw);
    for (int i = n0; i < (int)g_b.n; i++) { Vertex *v = &g_b.v[i]; float x = v->px, z = v->pz, nx = v->nx, nz = v->nz;
        v->px = at.x + x * c + z * s_; v->pz = at.z - x * s_ + z * c; v->py += at.y; v->nx = nx * c + nz * s_; v->nz = -nx * s_ + nz * c; } }
static void prop_tri(Vec3 a, Vec3 b, Vec3 c, float r, float g, float bl) { Vec3 n = face_normal(a, b, c);
    Vertex v[3] = { {a.x, a.y, a.z, n.x, n.y, n.z, 0, 0, r, g, bl, 1}, {b.x, b.y, b.z, n.x, n.y, n.z, 0, 0, r, g, bl, 1}, {c.x, c.y, c.z, n.x, n.y, n.z, 0, 0, r, g, bl, 1} };
    for (int k = 0; k < 3; k++) if (g_b.n < g_b.cap) g_b.v[g_b.n++] = v[k]; }
static void prop_cyl(float r0, float r1, float y0, float y1, int n, float r, float g, float bl) {   /* a column section, sides only */
    for (int i = 0; i < n; i++) { float a0 = i * 6.2831853f / n, a1 = (i + 1) * 6.2831853f / n, am = (a0 + a1) / 2;
        batch_quad4(&g_b, v3(sinf(a0) * r0, y0, cosf(a0) * r0), v3(sinf(a1) * r0, y0, cosf(a1) * r0), v3(sinf(a1) * r1, y1, cosf(a1) * r1), v3(sinf(a0) * r1, y1, cosf(a0) * r1), v3(sinf(am), 0, cosf(am)), 0, 0, 1, 1, r, g, bl, 1);
        prop_tri(v3(0, y1, 0), v3(sinf(a0) * r1, y1, cosf(a0) * r1), v3(sinf(a1) * r1, y1, cosf(a1) * r1), r * 1.05f, g * 1.05f, bl * 1.05f); } }
static void build_props(void) {
    batch_init(&g_b, g_vtmp, 16384);   /* the kerbs of a long road need over 13000 corners */
    int heaven = g_def->finale_img != NULL; float sr = 0.86f, sg = 0.78f, sb = 0.62f;   /* sun-baked limestone */
    if (heaven) { sr = 0.97f; sg = 0.95f; sb = 0.9f; } else if (!strcmp(g_def->prefix, "tomb")) { sr = 0.7f; sg = 0.68f; sb = 0.64f; } else if (!strcmp(g_def->prefix, "eden")) { sr = 0.62f; sg = 0.5f; sb = 0.36f; }
    /* every piece of scenery is a 3D model now: its corners go into one bucket per surface (plaster, stone, bark, leaves...),
       and full buckets go to the GPU in pieces, each drawn with its surface's picture */
    for (int m = 0; m < MAT_N; m++) { for (int c = 0; c < m_nprops[m]; c++) { gfx_mesh_free(m_props[m][c]); m_props[m][c] = NULL; } m_nprops[m] = 0; }
    ModelOut mo; mat_buckets(&mo);
    for (int i = 0; i < g_nplaced; i++) { Placed *pl = &g_placed[i]; if (!pl->solid) continue;
        const char *img = g_kind[pl->kind].img; if (!model_known(img)) continue;
        unsigned hsh = (unsigned)i * 2654435761u; float tint = 0.92f + 0.08f * ((hsh >> 8) & 255) / 255.0f;
        if (model_build(&mo, img, pl->h, pl->w, (unsigned)i, pl->p, pl->yaw, sr * tint, sg * tint, sb * tint) < 0) { mat_flush(&mo); model_build(&mo, img, pl->h, pl->w, (unsigned)i, pl->p, pl->yaw, sr * tint, sg * tint, sb * tint); } }
    mat_flush(&mo);
    /* the walls of Jericho: each stretch its own 3D model (stone courses, battlements), so it can fall flat */
    for (int i = 0; i < g_nplaced; i++) { Placed *pl = &g_placed[i]; gfx_mesh_free(pl->m3); pl->m3 = NULL; if (!pl->wall) continue;
        const char *img = g_kind[pl->kind].img; int tower = !strcmp(img, "#jtower"), rahab = !strcmp(img, "#rahab"); float L = fmaxf(pl->w, 4.0f), H = pl->h, T = tower ? L * 0.8f : 2.2f;
        batch_clear(&g_b);
        (void)tower; model_jwall(&g_b, L, H, T, rahab, (unsigned)i);
        pl->m3 = gfx_mesh_static(g_b.v, g_b.n); }
    /* raised kerbs along both edges of the road: red and white blocks, a hand's height, on the roads of the earth */
    batch_clear(&g_b);
    if (g_def->bonus != 2) for (int i = 0; i < g_track.n; i++) { int j = (i + 1) % g_track.n; const TrackSample *a = &g_track.s[i], *bq = &g_track.s[j];
        int stripe = (int)(a->dist / 3.0f) & 1; float cr = heaven ? 1.0f : (stripe ? 0.85f : 0.95f), cg = heaven ? (stripe ? 0.85f : 0.95f) : (stripe ? 0.2f : 0.95f), cb = heaven ? (stripe ? 0.4f : 0.9f) : (stripe ? 0.2f : 0.95f);
        /* chunky blocks (Sean: depth everywhere): a bevel up from the road, a flat top, the outer face, and a dark joint between blocks */
        int joint = (int)(a->dist / 3.0f) != (int)(bq->dist / 3.0f); float jk = joint ? 0.35f : 1.0f; cr *= jk; cg *= jk; cb *= jk;
        for (int sd = -1; sd <= 1; sd += 2) { float ha = a->width * 0.5f, hb = bq->width * 0.5f;
            #define KP(idx, h, lat, up) ({ Vec3 q_ = road_point(&g_track, idx, sd * ((h) + (lat))); v3(q_.x, q_.y + (up), q_.z); })
            Vec3 f0 = KP(i, ha, -0.7f, 0.02f), f1 = KP(j, hb, -0.7f, 0.02f);       /* foot, on the road */
            Vec3 e0 = KP(i, ha, -0.5f, 0.24f), e1 = KP(j, hb, -0.5f, 0.24f);       /* top of the bevel */
            Vec3 t0 = KP(i, ha, 0.05f, 0.26f), t1 = KP(j, hb, 0.05f, 0.26f);       /* outer top edge */
            Vec3 b0 = KP(i, ha, 0.05f, -0.3f), b1 = KP(j, hb, 0.05f, -0.3f);      /* down to the shoulder */
            #undef KP
            Vec3 nb = v3(-a->nx * sd * 0.7f, 0.7f, -a->nz * sd * 0.7f), nt = v3(-a->nx * sd * 0.08f, 1, -a->nz * sd * 0.08f), no = v3(a->nx * sd, 0, a->nz * sd);
            if (sd > 0) { batch_quad4(&g_b, f0, e0, e1, f1, nb, 0, 0, 1, 1, cr * 0.82f, cg * 0.82f, cb * 0.82f, 1); batch_quad4(&g_b, e0, t0, t1, e1, nt, 0, 0, 1, 1, cr, cg, cb, 1); batch_quad4(&g_b, b0, b1, t1, t0, no, 0, 0, 1, 1, cr * 0.62f, cg * 0.62f, cb * 0.62f, 1); }
            else        { batch_quad4(&g_b, e0, f0, f1, e1, nb, 0, 0, 1, 1, cr * 0.82f, cg * 0.82f, cb * 0.82f, 1); batch_quad4(&g_b, t0, e0, e1, t1, nt, 0, 0, 1, 1, cr, cg, cb, 1); batch_quad4(&g_b, b1, b0, t0, t1, no, 0, 0, 1, 1, cr * 0.62f, cg * 0.62f, cb * 0.62f, 1); } } }
    gfx_mesh_free(m_curbs); m_curbs = g_b.n ? gfx_mesh_static(g_b.v, g_b.n) : NULL;
    batch_init(&g_b, g_vtmp, 8192);
}
static void load_theme(void) {
    const TrackDef *d = g_def;
    int heaven = d->grid_idx > 0, eden = !strcmp(d->prefix, "eden"), desert = d->bonus == 3;
    tx_road = d->bonus == 2 ? theme_tex(d->tex_grass, 128, 128, px_grass) : theme_tex(d->tex_road, 256, 256, heaven ? px_gold : (eden ? px_path : px_road));   /* the valley floor is grass, not a paved road */
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
    for (int i = 0; i < 4; i++) { gfx_texture_free(tx_whale[i]); tx_whale[i] = NULL; }
    if (g_track.water) { static const char *wn[4] = {"whale_f", "whale_fl", "whale_l", "whale_b"};
        for (int i = 0; i < 4; i++) { int w = 0, h = 0; tx_whale[i] = asset_texture(wn[i], 0, &w, &h); g_whale_asp[i] = h ? (float)w / h : 1.3f; } }
    g_nkind = 0; g_nplaced = 0; g_nwalls = 0; g_ground_y = ground_level(&g_track);
    for (int i = 0; i < g_track.nmover; i++) g_mover_kind[i] = sprite_kind(g_track.mover[i].img);
    for (int i = 0; i < d->nscenery && g_nplaced < MAX_SCENERY; i++) {
        const Scenery *sc = &d->scenery[i]; int idx = track_def_idx(&g_track, sc->idx);
        Placed *p = &g_placed[g_nplaced++];
        p->kind = sprite_kind(sc->img);
        float sgn = sc->side < 0 ? -1.0f : 1.0f, bound = track_bound(&g_track, idx, (int)sgn);
        int jw = !strcmp(sc->img, "#jwall") || !strcmp(sc->img, "#jtower") || !strcmp(sc->img, "#rahab");
        float lat = track_scenery_lat(&g_track, idx, sc->side, jw ? 0 : track_scenery_radius(sc->img, sc->height));   /* the same place the race makes solid */
        p->p = v3(g_track.s[idx].x + g_track.s[idx].nx * lat, 0, g_track.s[idx].z + g_track.s[idx].nz * lat);
        p->p.y = fmaxf(track_surface_height(&g_track, idx, fabsf(lat) < bound ? lat : sgn * bound), g_ground_y) - 0.2f;   /* stands on the ground */
        p->h = sc->height; p->w = sc->height * g_kind[p->kind].aspect; p->glow = sc->glow; p->purple = !strcmp(sc->img, "#crystal"); p->falls = !strcmp(sc->img, "#waterfall");
        p->wall = (!strcmp(sc->img, "#jwall") || !strcmp(sc->img, "#jtower") || !strcmp(sc->img, "#rahab")) ? ++g_nwalls : 0; p->fall_t = 0;
        p->yaw = atan2f(g_track.s[idx].tx, g_track.s[idx].tz) + i * 0.37f;   /* roughly along the road, each a little turned */
        p->solid = !p->wall && (model_known(sc->img) || prop_shape(sc->img) >= 0);   /* everything is a 3D model now (Sean), except the waterfalls */
        { const TrackSample *ts = &g_track.s[idx]; p->t3 = v3(ts->tx, 0, ts->tz); p->o3 = v3(ts->nx * sgn, 0, ts->nz * sgn); }
        if (p->falls) p->yaw = atan2f(-p->o3.x, -p->o3.z);   /* the waterfall's front (+z) faces the road */
    }
    m_glow = gfx_mesh_dynamic((MAX_SCENERY + NKARTS) * 6 + 6);
    /* a soft shadow blob under everything that stands by the road */
    batch_clear(&g_b);
    for (int i = 0; i < g_nplaced; i++) { Placed *pl = &g_placed[i]; if (pl->falls) continue;
        float w = pl->w * 0.55f, dz = pl->w * 0.3f;
        batch_quad(&g_b, v3(pl->p.x, pl->p.y + 0.26f, pl->p.z), v3(w, 0, 0), v3(0, 0, -dz), v3(0, 1, 0), 0, 0, 1, 1, 0, 0, 0, 0.45f); }
    gfx_mesh_free(m_scshadow); m_scshadow = gfx_mesh_static(g_b.v, g_b.n);
    build_props();
    /* the water of each waterfall (Sean: animated and 3D): a sheet pouring over the lip of its rock, curving out and down
       into a round pool, drawn every frame with its picture sliding down; and white foam where it lands */
    { Batch keep = g_b; batch_init(&g_b, far_buf(), FAR_CAP); batch_clear(&g_b); Batch fb; static Vertex fv[4096]; batch_init(&fb, fv, 4096);
      for (int i = 0; i < g_nplaced; i++) { Placed *pl = &g_placed[i]; if (!pl->falls) continue;
        float h = pl->h, W = h * 0.9f, c = cosf(pl->yaw), sn = sinf(pl->yaw);
        #define WP(X_, Y_, Z_) v3(pl->p.x + (X_) * c + (Z_) * sn, pl->p.y + (Y_), pl->p.z - (X_) * sn + (Z_) * c)
        enum { NT = 12, NX = 5 }; Vec3 P[NT + 1][NX + 1]; float V[NT + 1];
        for (int t = 0; t <= NT; t++) { float u = (float)t / NT, y = h * 0.97f * (1 - u) + 0.3f * u, z = -W * 0.22f + W * 0.5f * powf(u, 0.55f), hw = W * 0.15f * (1 + 0.45f * u);
            for (int x = 0; x <= NX; x++) { float f = (float)x / NX * 2 - 1; P[t][x] = WP(f * hw, y, z + 0.12f * (1 - f * f)); }
            V[t] = t ? V[t - 1] + sqrtf((P[t][0].y - P[t - 1][0].y) * (P[t][0].y - P[t - 1][0].y) + 0.25f) / 3.0f : 0; }
        Vec3 nf = v3(sn, 0.25f, c);   /* facing out, toward the road */
        for (int t = 0; t < NT; t++) for (int x = 0; x < NX; x++) { float u0 = (float)x / NX, u1 = (float)(x + 1) / NX, sh = 0.92f + 0.08f * (x & 1);
            batch_quad4(&g_b, P[t + 1][x], P[t + 1][x + 1], P[t][x + 1], P[t][x], nf, u0, V[t + 1], u1, V[t], sh, sh, 1.0f, 1); }
        Vec3 pc = WP(0, 0.28f, W * 0.3f); float pr = W * 0.46f;   /* the pool */
        for (int k = 0; k < 18; k++) { float a0 = k * 6.2831853f / 18, a1 = (k + 1) * 6.2831853f / 18;
            Vec3 q0 = v3(pc.x + cosf(a0) * pr, pc.y, pc.z + sinf(a0) * pr * 0.8f), q1 = v3(pc.x + cosf(a1) * pr, pc.y, pc.z + sinf(a1) * pr * 0.8f);
            Vertex tri[3] = {{pc.x, pc.y, pc.z, 0, 1, 0, pc.x / 4, pc.z / 4, 0.75f, 0.9f, 1, 1}, {q1.x, q1.y, q1.z, 0, 1, 0, q1.x / 4, q1.z / 4, 0.7f, 0.85f, 1, 1}, {q0.x, q0.y, q0.z, 0, 1, 0, q0.x / 4, q0.z / 4, 0.7f, 0.85f, 1, 1}};
            if (g_b.n + 3 <= g_b.cap) for (int q = 0; q < 3; q++) g_b.v[g_b.n++] = tri[q]; }
        Vec3 fc = WP(0, 0.36f, W * 0.28f);   /* foam where it lands */
        for (int k = 0; k < 3; k++) { float R = W * (0.22f + 0.1f * k); batch_quad(&fb, v3(fc.x, fc.y + k * 0.03f, fc.z), v3(R * c, 0, -R * sn), v3(R * sn, 0, R * c), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 0.55f - 0.12f * k); }
        #undef WP
      }
      gfx_mesh_free(m_fallsw); m_fallsw = g_b.n ? gfx_mesh_static(g_b.v, g_b.n) : NULL; gfx_mesh_free(m_foam); m_foam = fb.n ? gfx_mesh_static(fb.v, fb.n) : NULL; g_b = keep; }
}

// ------------------------------------------------------------------ setup
static void set_track(int idx) {
    idx %= NUM_TRACK_DEFS;
    if (idx == g_loaded_track && g_status == GFX_OK) { g_track_idx = idx; return; }   /* already loaded: nothing to do */
    g_track_idx = idx; g_def = &TRACK_DEFS[g_track_idx]; g_loaded_track = idx;
    track_from_def(&g_track, g_def);
    float minx = 1e9f, maxx = -1e9f, minz = 1e9f, maxz = -1e9f;
    for (int i = 0; i < g_track.n; i++) { TrackSample *sm = &g_track.s[i]; if (sm->x < minx) minx = sm->x; if (sm->x > maxx) maxx = sm->x; if (sm->z < minz) minz = sm->z; if (sm->z > maxz) maxz = sm->z; }
    float span = fmaxf(maxx - minx, maxz - minz) + 20.0f; map_scale = 190.0f / span; map_x0 = (minx + maxx) * 0.5f; map_z0 = (minz + maxz) * 0.5f;
    if (g_status == GFX_OK) { free_theme(); free_track_meshes(); load_theme(); build_track_meshes(); }
}
static int human_kart(int p);
static float cele_bounce(const Race *r, int i) {   /* a finished player's kart hops for joy while the camera goes round */
    if (g_attract || i < 0 || i >= r->nk || !r->human[i] || !r->k[i].finished) return 0;
    for (int p = 0; p < g_nplayers; p++) if (human_kart(p) == i && g_view[p].finish_t > 0.6f && g_view[p].finish_t < CELE_T + 3.0f) return fabsf(sinf((g_view[p].finish_t - 0.6f) * 5.5f)) * 0.55f;
    return 0; }
static void reset_views(void) {   /* every player's camera starts just behind their kart */
    for (int p = 0; p < MAX_HUMANS; p++) { View *v = &g_view[p]; Kart *k = &g_race.k[p < g_nplayers ? human_kart(p) : g_race.player];
        v->yaw = k->yaw; v->x = k->x - sinf(k->yaw) * 8; v->z = k->z - cosf(k->yaw) * 8; v->y = k->y + 3.5f; v->fov = 62.0f; v->shake = 0; v->orbit = 0; v->finish_t = 0;
        g_pmsg_t[p] = 0; g_pmsg2[p][0] = 0; g_pside_t[p] = 0; g_proulette[p] = 0; }
    cv = &g_view[0];
}
static void drivers_reset(void);
static void start_race(int attract) {
    drivers_reset(); g_attract = attract; g_slowmo = 0; g_rec_n = 0; g_rec_t = 0;
    if (g_mode == MODE_TT && !attract && !g_multi) ghost_load(g_track_idx); else g_ghost_n = 0;
    int nk = (g_mode == MODE_TT && !attract) ? 1 : NKARTS;
    g_nplayers = (g_multi && !attract && g_njoined >= 2) ? g_njoined : 1;
    int cup = g_mode == MODE_GP && !attract;   /* a cup: the same rivals in the same seats every round, so each kart's points add up */
    if (!cup || g_gp_round == 0) race_set_field(NULL);
    if (g_nplayers > 1 && g_mode == MODE_TT) race_init_tt_multi(&g_race, &g_track, g_pchar, g_nplayers, g_difficulty);
    else if (g_nplayers > 1) race_init_multi(&g_race, &g_track, g_pchar, NULL, g_nplayers, g_difficulty);
    else race_init(&g_race, &g_track, attract && g_screen != SCR_TRACK ? g_demo_char : g_player_char, nk == 1 ? 0 : 5, g_difficulty, nk);
    if (cup && g_gp_round == 0) race_set_field(g_race.character);
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
        tx_blob = make_tex(64, 64, 0, 0, px_blob); tx_flame = make_tex(64, 128, 0, 0, px_flame); for (int e = 0; e < NSTYLES; e++) { g_emb_kind = e; tx_emblem[e] = make_tex(128, 96, 0, 0, px_emblem); } tx_vignette = make_tex(256, 256, 0, 0, px_vignette); tx_bird = make_tex(32, 32, 0, 0, px_bird);
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
            if (!g_k3d[i].ok) kart3d_load(&g_k3d[i], pn[i] + 9);
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
        { if (!g_gol3d.ok) anim3d_load(&g_gol3d, "goliath");
          int w = 2, h = 3; tx_goliath = asset_texture("char_goliath", 0, &w, &h); g_goliath_aspect = (float)w / (float)h; m_goliath = gfx_mesh_dynamic(6); m_hud2 = gfx_mesh_dynamic(16384);
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
static void checker_band(float y, float h) {
    for (float x = 0; x < 1280; x += h) { int c = ((int)(x / h)) & 1; batch_rect(&g_b, x, y, h, h, c ? 0.95f : 0.1f, c ? 0.95f : 0.1f, c ? 0.95f : 0.1f, 0.9f); }
}
static void text_c(const char *s, float y, float size, float r, float g, float b, float a) { batch_text(&g_b, s, 640 - text_width(s, size) / 2, y, size, r, g, b, a); }
static void text_c_in(const char *s, float vw, float y, float size, float r, float g, float b, float a) { batch_text(&g_b, s, vw / 2 - text_width(s, size) / 2, y, size, r, g, b, a); }   /* centred in a view of width vw */

/* ---- words that move (Sean: the messages in the middle and the box at the bottom "need to be animated and more interactive") ---- */
static float ease_back(float t) { t = clampf(t, 0, 1); float u = t - 1; return 1 + 2.70158f * u * u * u + 1.70158f * u * u; }
/* big words: the letters drop in one after another and bounce, thick 3D letters with a dark edge below, a shine running
   across, a gentle wave while they stay, and a burst outward as they go. age = seconds since shown, left = seconds still to show */
static void fx_words(const char *s, float cx, float y, float size, float age, float left, int gold, float r, float g, float b, float amul) {
    int n = (int)strlen(s); if (n <= 0 || n > 60) { if (n > 0) batch_text(&g_b, s, cx - text_width(s, size) / 2, y, size, r, g, b, amul); return; }
    char pre[64], one[2] = {0, 0}; float x0 = cx - text_width(s, size) / 2, prev = 0;
    float out = left < 0.28f ? clampf((0.28f - left) / 0.28f, 0, 1) : 0, sweep = fmodf(age * 0.9f, 2.0f) * (n + 6) - 3;
    for (int i = 0; i < n; i++) {
        memcpy(pre, s, (size_t)(i + 1)); pre[i + 1] = 0; float next = text_width(pre, size) + (i + 1 < n ? 0 : 0); float adv = next - prev; float lx = x0 + prev + adv * 0.5f; prev = next;
        if (s[i] == ' ') continue;
        float la = age - i * 0.035f; if (la <= 0) continue;
        float e = clampf(la / 0.42f, 0, 1), sc = ease_back(e) * (1 + 0.45f * out);
        float dy = -(1 - e) * (1 - e) * size * 1.3f + sinf(g_time * 5.0f - i * 0.6f) * size * 0.035f * e - out * size * 0.35f;
        float al = amul * clampf(la * 7, 0, 1) * (1 - out), sh = expf(-(i - sweep) * (i - sweep) * 0.3f);
        one[0] = s[i]; float sz = size * sc, px = lx - text_width(one, sz) * 0.5f - (lx - cx) * 0.18f * out, py = y + size * 0.5f - sz * 0.5f + dy;
        for (int d = 5; d >= 1; d--) { float o = d * sz * 0.016f; batch_text_g(&g_b, one, px + o * 0.55f, py + o, sz, gold ? 0.32f : r * 0.3f, gold ? 0.16f : g * 0.25f, gold ? 0.05f : b * 0.2f, al, 0); }   /* the letter's side, going back */
        if (gold) batch_text_g(&g_b, one, px, py, sz, 1, 1, 1, al, 1); else batch_text_g(&g_b, one, px, py, sz, r, g, b, al, 0);
        if (sh > 0.04f) batch_text_g(&g_b, one, px, py, sz, 1, 1, 1, al * sh * 0.55f, 0); } }   /* the shine passing over */
/* the small line under them: it slides up and types itself out, just after the big words land */
static void fx_line(const char *s, float cx, float y, float size, float age, float left, float r, float g, float b, float amul) {
    float t = age - 0.3f; if (t <= 0) return; int n = (int)strlen(s), show = (int)(t * 70); if (show > n) show = n; if (show <= 0) return;
    char buf[160]; if (n > 158) n = 158; memcpy(buf, s, (size_t)n); buf[n] = 0;
    float x = cx - text_width(buf, size) / 2, e = clampf(t / 0.25f, 0, 1), a = amul * e * (left < 0.2f ? clampf(left / 0.2f, 0, 1) : 1);
    buf[show] = 0; batch_text(&g_b, buf, x, y + (1 - e) * 16, size, r, g, b, a); }
/* how long a player's message has been showing: restarts when the words change or are shown again */
/* HUD words with a solid 3D edge (a few darker copies stepping down-right), so they stand off the picture */
static float text3d(const char *str, float x, float y, float size, float r, float g, float b, float a, int gold) {
    int layers = size >= 40 ? 5 : size >= 24 ? 4 : 3;
    for (int d = layers; d >= 1; d--) { float o = d * size * 0.022f; batch_text_g(&g_b, str, x + o * 0.6f, y + o, size, r * 0.18f + 0.05f, g * 0.14f + 0.03f, b * 0.12f + 0.06f, a, 0); }
    return batch_text_g(&g_b, str, x, y, size, r, g, b, a, gold); }
static float msg_age(int p) {
    static char last[MAX_HUMANS][40]; static float t0[MAX_HUMANS], prevleft[MAX_HUMANS];
    if (p < 0 || p >= MAX_HUMANS) return 1;
    if (strcmp(last[p], g_pmsg[p]) || g_pmsg_t[p] > prevleft[p] + 0.05f) { snprintf(last[p], sizeof last[p], "%s", g_pmsg[p]); t0[p] = g_time; }
    prevleft[p] = g_pmsg_t[p]; return g_time - t0[p]; }
/* the big countdown number: it slams in each second and shrinks away */
static void fx_count(int nnum, float cd, float cx, float y, float size) {
    char buf[4]; snprintf(buf, sizeof buf, "%d", nnum); float frac = nnum - (cd - 0.5f); frac = clampf(frac, 0, 1);
    float sc = frac < 0.18f ? 2.2f - 1.2f * ease_back(frac / 0.18f) : 1.0f - 0.25f * (frac - 0.18f), al = frac < 0.7f ? 1 : clampf((1 - frac) / 0.3f, 0, 1);
    float sz = size * sc, px = cx - text_width(buf, sz) / 2, py = y + size * 0.5f - sz * 0.5f;
    for (int d = 7; d >= 1; d--) { float o = d * sz * 0.014f; batch_text_g(&g_b, buf, px + o * 0.55f, py + o, sz, 0.32f, 0.16f, 0.05f, al, 0); }
    batch_text_g(&g_b, buf, px, py, sz, 1, 1, 1, al, 1); }
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
/* the GamePad touch screen: each menu marks where its buttons and cards are as it draws (TV pass only, same layout on
   both screens); a tap on one picks it and confirms it, like moving there and pressing A */
enum { TZ_SEL, TZ_A, TZ_B, TZ_PLUS, TZ_LEFT, TZ_RIGHT };
typedef struct { float x, y, w, h; int kind, val; } TZone;
static TZone g_tz[64], g_tz_prev[64]; static int g_ntz, g_ntz_prev, g_tz_rec;
static void tzone(float x, float y, float w, float h, int kind, int val) { if (!g_tz_rec || g_ntz >= 64) return; TZone *z = &g_tz[g_ntz++]; z->x = x; z->y = y; z->w = w; z->h = h; z->kind = kind; z->val = val; }
static void back_button(void) {   /* a BACK button to tap, bottom left */
    batch_rect(&g_b, 20, 652, 150, 50, 1, 0.85f, 0.3f, 0.9f); batch_rect(&g_b, 23, 655, 144, 44, 0.08f, 0.14f, 0.36f, 1);
    batch_text(&g_b, "< BACK", 95 - text_width("< BACK", 22) / 2, 666, 22, 1, 1, 1, 1); tzone(10, 640, 175, 75, TZ_B, 0); }
/* a menu choice: its own plaque. They fly in one after another when the menu opens; the one you are on turns gold,
   lifts toward you, sways a little and has a shine running across it */
static void menu_item(const char *s, float y, int selected, float size) {
    int i = g_mi++; float t = ui_ease((g_screen_t - 0.05f - i * 0.05f) / 0.4f), w = fmaxf(470, text_width(s, size) * 1.12f + 60), h = size + 14, x = 640 - w / 2, py = y - 7;
    uint32_t mk = ui_begin();
    ui_plaque(x, py, w, h, h * 0.35f, 10, selected ? &UI_GOLD : &UI_BLUE, 0.97f, selected ? 0.7f + 0.3f * sinf(g_time * 4) : 0, selected ? fmodf(g_time * 0.55f, 1.4f) : -1);
    uint32_t tk = g_b.n; float tw = text_width(s, size);
    if (selected) batch_text(&g_b, s, 640 - tw / 2, y + 1, size, 0.1f, 0.12f, 0.3f, 1); else { batch_text(&g_b, s, 640 - tw / 2 + 2, y + 3, size, 0, 0, 0.05f, 0.5f); batch_text(&g_b, s, 640 - tw / 2, y + 1, size, 0.95f, 0.96f, 1, 1); }
    ui_zrun(tk, 6);
    float sel = selected ? 1.0f : 0.0f, wob = g_time * 2.3f + i;
    Mat4 m = ui_xf(640, py + h / 2, 34 * sel - 380 * (1 - t), 0.05f * sel * sinf(wob) + (1 - t) * 0.9f, 0.07f * sel * sinf(wob * 0.8f) - (1 - t) * 0.5f, 0, 1 + 0.05f * sel);
    ui_end(mk, &m);
}
static void stat_bar(float x, float y, const char *label, float v) {
    batch_text(&g_b, label, x, y, 12, 0.85f, 0.9f, 1, 1);
    batch_rect(&g_b, x + 92, y + 2, 92, 10, 0.05f, 0.08f, 0.2f, 0.9f);
    float f = clampf((v - 0.8f) / 0.5f, 0.1f, 1.0f);
    batch_rect(&g_b, x + 92, y + 2, 92 * f, 10, 0.3f + 0.6f * f, 0.85f, 0.3f, 1);
}
static void draw_preview(const Preview *pv, float cx, float cy, float size, float r, float g, float b) {
    for (int i = 0; i < pv->n; i++) batch_rect(&g_b, cx + pv->x[i] * size - 2.5f, cy - pv->z[i] * size - 2.5f, 5, 5, r, g, b, 1);
}

// ------------------------------------------------------------------ the race HUD
static void pad_corner_button(const char *label) {   /* the GamePad's MAP / RACE switch, bottom right (tap it) */
    float x = 1084, y = 634, w = 176, h = 66;
    batch_rect(&g_b, x - 3, y - 3, w + 6, h + 6, 1, 0.82f, 0.35f, 0.95f); batch_rect(&g_b, x, y, w, h, 0.07f, 0.1f, 0.28f, 0.92f);
    batch_text(&g_b, label, x + w * 0.5f - text_width(label, 28) * 0.5f, y + 19, 28, 1, 0.9f, 0.5f, 1); }
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
    if (g_mode == MODE_BATTLE) { Battle *b = &g_battle; for (int i = 0; i < BATTLE_STONES; i++) if (b->stone_respawn[i] <= 0) batch_rect(&g_b, mx + (b->stone_x[i] - map_x0) * sc - 8, mz - (b->stone_z[i] - map_z0) * sc - 8, 16, 16, 1, 0.9f, 0.5f, 1); }
    if (g_mode == MODE_BATTLE) { Battle *b = &g_battle; batch_rect(&g_b, mx + (b->gx - map_x0) * sc - 20, mz - (b->gz - map_z0) * sc - 20, 40, 40, 0.9f, 0.2f, 0.15f, 1); batch_text(&g_b, "G", mx + (b->gx - map_x0) * sc - 8, mz - (b->gz - map_z0) * sc - 10, 20, 1, 1, 1, 1); }
    /* standings down the right side */
    batch_text(&g_b, g_mode == MODE_BATTLE ? "THE FIGHT" : "STANDINGS", 800, 80, 24, 1, 0.85f, 0.3f, 1);
    if (g_mode == MODE_BATTLE) { Battle *b = &g_battle; snprintf(buf, sizeof buf, b->gstate == GOL_DAZED ? "FINAL SHOT - HIS FOREHEAD!" : "HEAD HITS %d OF %d", b->hits_on_goliath, battle_need(b)); batch_text(&g_b, buf, 800, 130, 20, 1, 1, 1, 1);
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
    snprintf(buf, sizeof buf, "LAP %d/%d", lap, g_track.laps); text3d(buf, 40, 30, 36, 1, 1, 1, 1, 0);
    fmt_time(buf, sizeof buf, k->lap >= 1 ? k->lap_time : 0.0f); text3d(buf, 40, 72, 28, 1, 1, 0.6f, 1, 0);
    if (k->best_lap > 0) { char tb[32]; fmt_time(tb, sizeof tb, k->best_lap); snprintf(buf, sizeof buf, "BEST %s", tb); batch_text(&g_b, buf, 40, 104, 20, 0.8f, 0.9f, 1, 1); }
    if (g_mode == MODE_TT) { fmt_time(buf, sizeof buf, k->total_time); batch_text(&g_b, buf, 1240 - text_width(buf, 40), 30, 40, 1, 0.9f, 0.3f, 1);
        if (g_ghost_n > 1) batch_text(&g_b, "VS YOUR BEST RUN", 1240 - text_width("VS YOUR BEST RUN", 14), 112, 14, 0.7f, 0.9f, 1, 0.9f);
        if (g_best_time[g_track_idx] > 0) { char tb[32]; fmt_time(tb, sizeof tb, g_best_time[g_track_idx]); snprintf(buf, sizeof buf, "RECORD %s", tb); batch_text(&g_b, buf, 1240 - text_width(buf, 20), 78, 20, 0.8f, 0.9f, 1, 1); } }
    else {
        int mypos = 0; for (int p = 0; p < r->nk; p++) if (r->order[p] == r->player) mypos = p + 1;
        if (k->finished && k->place) mypos = k->place;
        { static int lastpos = -1; static float pt0 = 0; if (mypos != lastpos) { lastpos = mypos; pt0 = g_time; }   /* the place jumps when it changes */
          float pa = g_time - pt0, ps = 64 * (1 + 0.45f * expf(-pa * 7) * cosf(pa * 14)); text3d(place_name(mypos), 1240 - text_width(place_name(mypos), ps), 24 + (64 - ps) * 0.5f, ps, 1, 1, 1, 1, 1); }
    }
    snprintf(buf, sizeof buf, "%3.0f KM/H", fabsf(k->speed) * 3.6f); text3d(buf, 1240 - text_width(buf, 24), 96, 24, 1, 1, 1, 1, 0);
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
    if (r->state == RACE_COUNTDOWN) { static float cd0 = 0, cdl = -1; if (r->cd > cdl + 0.01f) cd0 = r->cd; cdl = r->cd; float ca = cd0 - r->cd;   /* seconds since the countdown began */
        fx_words(g_track.name, 640, 180, 34, ca, r->cd, 0, 1, 1, 1, 1); int n = (int)ceilf(r->cd - 0.5f); if (n >= 1 && n <= 3) fx_count(n, r->cd, 640, 250, 110);
        if (!r->jericho) { const char *ref, *v = road_verse(g_def->prefix, &ref); float a = clampf(r->cd * 1.2f, 0, 1); fx_line(v, 640, 404, text_width(v, 20) > 1100 ? 16 : 20, ca - 0.6f, r->cd, 1, 1, 1, a); fx_line(ref, 640, 434, 15, ca - 1.4f, r->cd, 1, 0.9f, 0.5f, a); }
        if (r->jericho) { text_c("SEVEN TIMES ROUND THE CITY. A SHOFAR EVERY LAP: ZL BLOWS IT.", 400, 18, 1, 1, 1, 1); text_c("SAVE THEM FOR THE GREAT SHOUT ON THE SEVENTH LAP!", 426, 18, 1, 0.9f, 0.5f, 1); } }
    if (g_msg_t > 0) { float sz = text_width(g_msg, 60) > 1100 ? 44 : 60, ag = msg_age(0); fx_words(g_msg, 640, 260, sz, ag, g_msg_t, 1, 1, 1, 1, 1); if (g_msg2[0]) fx_line(g_msg2, 640, 334, 22, ag, g_msg_t, 0.9f, 0.95f, 1, 1); }
    if (k->wrongway_t > WRONGWAY_DELAY && ((int)(g_time * 4) & 1)) text_c("WRONG WAY", 330, 56, 1, 0.3f, 0.2f, 1);
    if (g_demo && !g_attract) text_c("DEMO - MOVE THE STICK TO DRIVE", 680, 20, 1, 1, 1, 0.8f);
    (void)pad;
}

// ------------------------------------------------------------------ screens
/* "PRESS ANY BUTTON" (painted into Sean's title picture): beams of light turn slowly behind it, it glows and a shine
   runs across it; pressed, it sinks in with a flash before the menu opens */
/* a quad in screen pixels (center, two half-axes) for the 2D layer, which works in -1..1 units with y up */
static void q2d(Batch *b, Vec3 c, Vec3 ax, Vec3 ay, Vec3 n, float u0, float v0, float u1, float v1, float r, float g, float bl, float a) {
    batch_quad(b, scr(c.x, c.y), v3(ax.x / 640.0f, -ax.y / 360.0f, 0), v3(ay.x / 640.0f, -ay.y / 360.0f, 0), n, u0, v0, u1, v1, r, g, bl, a); }
static void draw_title_button(const Mat4 *id, float fade) {
    static Vertex tv[160]; Batch tb; batch_init(&tb, tv, 160);   /* its own little batch: the HUD batch is still being filled */
    static Mesh *m_rays = NULL, *m_btn = NULL, *m_shine = NULL; if (!m_rays) { m_rays = gfx_mesh_dynamic(40 * 6); m_btn = gfx_mesh_dynamic(4 * 6); m_shine = gfx_mesh_dynamic(2 * 6); }
    const float cx = 640, cy = 597, u0 = 412.0f / 1280, u1 = 868.0f / 1280, v0 = 566.0f / 720, v1 = 629.0f / 720;
    float pr = g_title_press, push = pr > 0 ? (pr < 0.12f ? pr / 0.12f : fmaxf(0.0f, 1.0f - (pr - 0.12f) / 0.25f)) : 0;   /* 0..1..0: down, then back up */
    float flash = pr > 0 ? fmaxf(0.0f, 1.0f - pr / 0.45f) : 0, beat = 0.5f + 0.5f * sinf(g_time * 3.2f);
    /* golden beams turning slowly round the words, and a warm glow (blended, not added: the sky behind is nearly white) */
    for (int i = 0; i < 16; i++) {
        float a = g_time * 0.3f + i * 6.2831853f / 16, len = 230 + 50 * sinf(g_time * 1.3f + i * 1.7f) + 160 * flash, wid = 13;
        float dx = cosf(a), dy = sinf(a) * 0.5f, dl = sqrtf(dx * dx + dy * dy); dx /= dl; dy /= dl;
        Vec3 c = v3(cx + dx * len * 0.5f, cy + dy * len * 0.5f, 0);
        q2d(&tb, c, v3(dx * len * 0.5f, dy * len * 0.5f, 0), v3(-dy * wid, dx * wid, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 0.78f, 0.25f, (0.55f + 0.25f * beat + 0.4f * flash) * fade); }
    q2d(&tb, v3(cx, cy, 0), v3(290 + 25 * beat, 0, 0), v3(0, 58 + 6 * beat, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 0.82f, 0.3f, (0.6f + 0.25f * beat + 0.5f * flash) * fade);
    gfx_mesh_update(m_rays, tb.v, tb.n); gfx_state(ST_HUD); gfx_bind(tx_blob); gfx_draw(m_rays, id, 1, 1, 1, 1);
    /* the words themselves, cut from the picture and drawn again on top: they breathe in and out, and sink in when pressed */
    float sc = (1.0f + 0.03f * beat) * (1.0f - 0.12f * push), w = (u1 - u0) * 1280 * sc, h = (v1 - v0) * 720 * sc, y = cy + 6 * push;
    static Texture *tx_letters = NULL; static int tried = 0;
    if (!tried) { tried = 1; tx_letters = asset_texture_letters("title_bg", u0 + 0.025f, v0, u1 - 0.025f, v1, NULL, NULL); }
    batch_clear(&tb);
    { float lu0 = tx_letters ? 0 : u0, lu1 = tx_letters ? 1 : u1, lv0 = tx_letters ? 0 : v0, lv1 = tx_letters ? 1 : v1, ww = tx_letters ? w * (1.0f - 0.05f / (u1 - u0)) : w;
      if (push > 0) batch_rect_uv(&tb, cx - ww * 0.5f, y - h * 0.5f + 5, ww, h, lu0, lv0, lu1, lv1, 0, 0, 0, 0.4f * push);   /* its shadow, while pressed */
      batch_rect_uv(&tb, cx - ww * 0.5f, y - h * 0.5f, ww, h, lu0, lv0, lu1, lv1, 1, 1, 1, fade); }
    gfx_mesh_update(m_btn, tb.v, tb.n); gfx_bind(tx_letters ? tx_letters : tx_title_bg); gfx_draw(m_btn, id, 1 - 0.15f * push, 1 - 0.15f * push, 1 - 0.15f * push, 1);
    /* a shine sweeping across, and the white flash of the press */
    batch_clear(&tb);
    { float t = fmodf(g_time, 2.6f) / 0.9f; if (t < 1) q2d(&tb, v3(400 + t * 480, cy, 0), v3(20, 0, 0), v3(-14, 30, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 1, 1, 0.6f * fade); }
    if (flash > 0) q2d(&tb, v3(cx, cy, 0), v3(260, 0, 0), v3(0, 46, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 1, 1, flash);
    if (tb.n) { gfx_mesh_update(m_shine, tb.v, tb.n); gfx_state(ST_HUD | ST_ADD); gfx_bind(tx_blob); gfx_draw(m_shine, id, 1, 1, 1, 1); gfx_state(ST_HUD); }
}
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
    for (int p = 0; p < 3; p++) { int ch = r->character[rows[p]]; Texture *kt = tx_ride[ch][3] ? tx_ride[ch][3] : tx_kart[ch]; float ka = tx_ride[ch][3] ? g_ride_aspect[ch][3] : g_kart_aspect[ch];   /* the kart they really raced in */
        if (!kt) continue; if (!pm[p]) pm[p] = gfx_mesh_dynamic(6);
        float w = p == 0 ? 150 : 110, h = w / ka;
        batch_clear(&g_b); batch_rect_uv(&g_b, sx[p] - w / 2, 600 - sh[p] - h + 6, w, h, 0, 0, 1, 1, 1, 1, 1, 1);
        gfx_mesh_update(pm[p], g_b.v, g_b.n); gfx_bind(kt); gfx_draw(pm[p], &id, 1, 1, 1, 1); }
}
static void ui_backdrop(void) {   /* behind every menu: slow golden beams fanning from above and motes of light drifting up */
    for (int i = 0; i < 9; i++) { float a = 1.5707963f + (i - 4) * 0.22f + 0.05f * sinf(g_time * 0.3f + i), len = 900, w0 = 6, w1 = 70 + 20 * (i & 1), al = 0.05f + 0.03f * sinf(g_time * 0.7f + i * 1.7f);
        float dx = cosf(a), dy = sinf(a), px = -dy, py = dx, x0 = 640, y0 = -80;
        float A[7] = {x0 - px * w0, y0 - py * w0, -40, 1, 0.88f, 0.55f, al}, B[7] = {x0 + px * w0, y0 + py * w0, -40, 1, 0.88f, 0.55f, al};
        float C[7] = {x0 + dx * len + px * w1, y0 + dy * len + py * w1, -40, 1, 0.88f, 0.55f, 0}, D[7] = {x0 + dx * len - px * w1, y0 + dy * len - py * w1, -40, 1, 0.88f, 0.55f, 0};
        ui_tri(A, B, C); ui_tri(A, C, D); }
    for (int k = 0; k < 40; k++) { float h1 = noise2(k, 31), h2 = noise2(k, 32), ph = fmodf(g_time * (0.04f + 0.05f * h2) + h1, 1.0f), x = h1 * 1280 + 30 * sinf(g_time * 0.5f + k), y = 760 - ph * 820, r = 2 + 3 * h2, al = 0.55f * sinf(ph * 3.14159f);
        float A[7] = {x - r, y, -20, 1, 0.92f, 0.6f, al}, B[7] = {x, y - r, -20, 1, 0.92f, 0.6f, al}, C[7] = {x + r, y, -20, 1, 0.92f, 0.6f, al}, D[7] = {x, y + r, -20, 1, 0.92f, 0.6f, al}; ui_tri(A, B, C); ui_tri(A, C, D); }
}
static void hud_main(void) {
    static const char *dn[3] = {"EASY", "NORMAL", "HARD"}; char buf[64];
    snprintf(buf, sizeof buf, "DIFFICULTY: %s", dn[g_difficulty]);
    const char *items[9] = {"GRAND PRIX", "SINGLE RACE", "TIME TRIAL", "DAVID VS GOLIATH", "MULTIPLAYER 2-4", "THE WALLS OF JERICHO", buf, "OPTIONS AND RECORDS", "CREDITS"};   /* David vs Goliath is open from the start: it is one of the main stories */
    /* (online play is built but switched off for now - Sean: no paid server. The ONLINE TEST screen, SCR_ONLINE, src/net and
       server/ stay in the project for later; to bring it back, add "ONLINE" after THE WALLS OF JERICHO and open SCR_ONLINE.) */
    for (int i = 0; i < 9; i++) { float y = 190 + i * 49; menu_item(items[i], y, g_menu_sel == i, i < 6 ? 27 : 22); tzone(400, y - 10, 480, 47, TZ_SEL, i); }
    text_c("A: CHOOSE   B: BACK   LEFT/RIGHT: CHANGE", 690, 14, 0.8f, 0.85f, 1, 1);
}
static void hud_online(void) {   /* the ONLINE TEST screen */
    const NetStatus *n = net_status(); char buf[96]; int ok = n->state == NET_CONNECTED;
    int bad = n->state == NET_NO_ADDRESS || n->state == NET_NO_ANSWER || n->state == NET_REFUSED || n->state == NET_NO_SOCKET;
    panel(240, 70, 800, 580);
    text_c("ONLINE TEST", 96, 36, 1, 0.85f, 0.3f, 1);
    text_c("ONLINE RACING IS ON ITS WAY.", 146, 14, 0.85f, 0.9f, 1, 1); text_c("THIS CHECKS YOUR WII U CAN REACH THE KINGDOM KART SERVER.", 166, 14, 0.85f, 0.9f, 1, 1);
    batch_text(&g_b, "SERVER", 300, 200, 18, 0.8f, 0.85f, 1, 1);
    { char hu[64]; int i = 0; for (; n->host[i] && i < 63; i++) hu[i] = (char)(n->host[i] >= 'a' && n->host[i] <= 'z' ? n->host[i] - 32 : n->host[i]); hu[i] = 0; batch_text(&g_b, hu, 520, 200, 18, 1, 1, 1, 1); }   /* the font has capitals only */
    batch_text(&g_b, "STATUS", 300, 240, 18, 0.8f, 0.85f, 1, 1);
    { float pulse = 0.7f + 0.3f * sinf(g_time * 6); batch_text(&g_b, net_state_words(n->state), 520, 236, 24, ok ? 0.45f : bad ? 1 : pulse, ok ? 1 : bad ? 0.45f : pulse * 0.9f, ok ? 0.5f : bad ? 0.4f : 0.4f, 1); }
    if (ok || n->answered > 0) {
        batch_text(&g_b, "PING", 300, 290, 18, 0.8f, 0.85f, 1, 1);
        if (n->ping_ms >= 0) snprintf(buf, sizeof buf, "%d MS", n->ping_ms); else snprintf(buf, sizeof buf, "...");
        float g = n->ping_ms < 0 ? 1 : n->ping_ms < 80 ? 1 : n->ping_ms < 160 ? 0.85f : 0.5f;
        batch_text(&g_b, buf, 520, 284, 30, n->ping_ms < 0 || n->ping_ms < 160 ? 0.5f : 1, g, 0.45f, 1);
        if (n->best_ms >= 0) { snprintf(buf, sizeof buf, "BEST %d   AVERAGE %d   WORST %d", n->best_ms, n->avg_ms, n->worst_ms); batch_text(&g_b, buf, 520, 324, 14, 0.85f, 0.9f, 1, 1); }
        batch_text(&g_b, "PACKETS", 300, 360, 18, 0.8f, 0.85f, 1, 1);
        int lost = n->sent - n->answered - (ok ? 1 : 0); if (lost < 0) lost = 0;   /* the newest ping may still be on its way */
        snprintf(buf, sizeof buf, "%d SENT, %d BACK, %d LOST", n->sent, n->answered, lost);
        batch_text(&g_b, buf, 520, 360, 18, 1, 1, 1, 1);
        batch_text(&g_b, "ONLINE NOW", 300, 400, 18, 0.8f, 0.85f, 1, 1);
        snprintf(buf, sizeof buf, "%d KINGDOM KART %s", n->online, n->online == 1 ? "PLAYER (YOU)" : "PLAYERS"); batch_text(&g_b, buf, 520, 400, 18, 1, 1, 1, 1);
        if (n->message[0]) { char up[40]; int i = 0; for (; n->message[i] && i < 39; i++) up[i] = (char)(n->message[i] >= 'a' && n->message[i] <= 'z' ? n->message[i] - 32 : n->message[i]); up[i] = 0; text_c(up, 460, 18, 1, 0.9f, 0.55f, 1); } }
    if (n->state == NET_NO_ADDRESS || n->state == NET_NO_SOCKET) text_c("CHECK THE WII U IS ON THE INTERNET (SYSTEM SETTINGS)", 300, 15, 1, 0.8f, 0.6f, 1);
    if (n->state == NET_NO_ANSWER) text_c("THE SERVER MAY BE OFF RIGHT NOW. TRY AGAIN IN A MINUTE.", 300, 15, 1, 0.8f, 0.6f, 1);
    if (n->state == NET_REFUSED) text_c(n->refused_why == 1 ? "THIS GAME IS A DIFFERENT VERSION FROM THE SERVER. GET THE NEWEST BUILD." : "THE SERVER IS BUSY. TRY AGAIN SOON.", 300, 15, 1, 0.8f, 0.6f, 1);
    text_c("A: TRY AGAIN   B: BACK", 610, 14, 0.8f, 0.85f, 1, 1);
}
static void volume_row(const char *label, int vol, float y, int selected) {
    float x = 400;
    if (selected) ui_plaque(x - 16, y - 6, 480, 36, 12, 8, &UI_SEL, 0.95f, 0.5f, fmodf(g_time * 0.5f, 1.4f));
    batch_text(&g_b, label, x, y, 24, selected ? 1 : 0.9f, selected ? 0.85f : 0.95f, selected ? 0.3f : 1, 1);
    for (int i = 0; i < 10; i++) batch_rect(&g_b, x + 270 + i * 18, y + 2, 12, 22, i < vol ? 1 : 0.3f, i < vol ? 0.85f : 0.33f, i < vol ? 0.3f : 0.45f, 1);
}
static void hud_options(void) {
    panel(240, 60, 800, 600); char buf[64];
    text_c("OPTIONS AND RECORDS", 80, 36, 1, 0.85f, 0.3f, 1);
    volume_row("MUSIC VOLUME", g_save.music_vol, 128, g_opt_sel == 0);
    volume_row("SOUND VOLUME", g_save.sfx_vol, 164, g_opt_sel == 1);
    for (int r2 = 0; r2 < 2; r2++) { tzone(384, 122 + r2 * 36, 340, 36, TZ_LEFT, r2); tzone(724, 122 + r2 * 36, 180, 36, TZ_RIGHT, r2); }   /* tap the left of a volume bar for quieter, the right for louder */
    tzone(384, 194, 480, 36, TZ_SEL, 2);
    { float x = 400, y = 200; if (g_opt_sel == 2) ui_plaque(x - 16, y - 6, 480, 36, 12, 8, &UI_SEL, 0.95f, 0.5f, fmodf(g_time * 0.5f, 1.4f));
      batch_text(&g_b, "CONTROLLER RUMBLE", x, y, 24, g_opt_sel == 2 ? 1 : 0.9f, g_opt_sel == 2 ? 0.85f : 0.95f, g_opt_sel == 2 ? 0.3f : 1, 1);
      batch_text(&g_b, g_save.rumble ? "ON" : "OFF", x + 270, y, 24, g_save.rumble ? 1 : 0.6f, g_save.rumble ? 0.85f : 0.65f, g_save.rumble ? 0.3f : 0.75f, 1); }
    { float x = 400, y = 236; if (g_opt_sel == 3) ui_plaque(x - 16, y - 6, 480, 30, 10, 8, &UI_SEL, 0.95f, 0.5f, -1);
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
    {"RECORDED SOUND EFFECTS", 18, 1, 0}, {"JOSEPH SARDIN - BIGSOUNDBANK.COM", 22, 0, 0}, {"KENNEY - KENNEY.NL", 22, 0, 0}, {"CROWD CHEERING - GREGOR QUENDEL (CC-BY 4.0)", 18, 0, 0}, {"ADDITIONAL VOICES AND SOUNDS - ELEVENLABS", 18, 0, 60},
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
static void char_card(int i, int multi) {
        const Character *c = &CHARACTERS[i]; int sel = !multi && i == g_player_char, open = char_unlocked(i);
        float x, y, w = 225, h = 186; char_card_pos(i, &x, &y); tzone(x, y, w, h, TZ_SEL, i);
        if (multi) { int n = 0; for (int p = 0; p < g_njoined; p++) if (g_pchar[p] == i) {   /* each player's colored frame, nested when they sit on the same card */
                float in = 6.0f - n * 3.0f; batch_rect(&g_b, x - in, y - in, w + 2 * in, 3, PLAYER_COL[p][0], PLAYER_COL[p][1], PLAYER_COL[p][2], 1); batch_rect(&g_b, x - in, y + h + in - 3, w + 2 * in, 3, PLAYER_COL[p][0], PLAYER_COL[p][1], PLAYER_COL[p][2], 1);
                batch_rect(&g_b, x - in, y - in, 3, h + 2 * in, PLAYER_COL[p][0], PLAYER_COL[p][1], PLAYER_COL[p][2], 1); batch_rect(&g_b, x + w + in - 3, y - in, 3, h + 2 * in, PLAYER_COL[p][0], PLAYER_COL[p][1], PLAYER_COL[p][2], 1); n++; } }
        card_face(x, y, w, h, sel, open);
        if (!open) {
            batch_rect(&g_b, x + 8, y + 8, 90, 90, 0.2f, 0.22f, 0.3f, 1);
            batch_text(&g_b, "?", x + 53 - text_width("?", 60) / 2, y + 22, 60, 0.5f, 0.55f, 0.7f, 1);
            batch_text(&g_b, "LOCKED", x + 106, y + 14, 16, 0.8f, 0.8f, 0.9f, 1);
            const char *hint = unlock_hint(char_lock_bit(i)); char line[24]; int li = 0, row = 0;
            for (const char *q = hint; ; q++) { if ((*q == ' ' || *q == 0) && li >= 11) { line[li] = 0; batch_text(&g_b, line, x + 8, y + 108 + row * 17, 12, 1, 0.9f, 0.5f, 1); row++; li = 0; if (*q == 0) break; continue; }
                if (*q == 0) { line[li] = 0; if (li) batch_text(&g_b, line, x + 8, y + 108 + row * 17, 12, 1, 0.9f, 0.5f, 1); break; }
                if (li < 22) line[li++] = *q; }
            return;
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
static void hud_char(int multi) {
    text_c(multi ? "EVERYONE: CHOOSE YOUR CHARACTER" : "CHOOSE YOUR CHARACTER", 22, 36, 1, 0.85f, 0.3f, 1);
    int order[NCHARS], no = 0; for (int i = 0; i < NCHARS; i++) if (multi || i != g_player_char) order[no++] = i; if (!multi) order[no++] = g_player_char;   /* the chosen card last, on top */
    for (int k = 0; k < no; k++) { int i = order[k]; float x, y; char_card_pos(i, &x, &y); uint32_t mk = ui_begin(); char_card(i, multi); card_done(mk, i, x, y, 225, 186, !multi && i == g_player_char); }
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
    if (g_njoined >= 2) { text_c("PRESS + TO START (OR TAP HERE)", 540, 26, 1, 0.9f, 0.4f, (int)(g_time * 2) & 1 ? 1.0f : 0.7f); tzone(240, 525, 800, 60, TZ_PLUS, 0); }
    else if (g_mode != MODE_VS) { text_c("EVERYONE PLAYING: PRESS A ON YOUR CONTROLLER", 540, 18, 0.8f, 0.85f, 1, 1); text_c("JUST YOU? PRESS + ON THE GAMEPAD (OR TAP HERE)", 570, 18, 1, 0.9f, 0.4f, 1); tzone(240, 560, 800, 40, TZ_PLUS, 0); }
    else text_c("WAITING FOR AT LEAST 2 PLAYERS", 540, 18, 0.8f, 0.85f, 1, 1);
    tzone(220, 192, 840, 54, TZ_A, 0);   /* the GamePad's own row: tap to join */
    text_c("B: LEAVE   -   WII REMOTE SIDEWAYS: 2 GAS, 1 BRAKE, B DRIFT, A BLESSING", 600, 14, 0.8f, 0.85f, 1, 1);
}
static void draw_portraits(void) {   /* the real portraits are separate textures, drawn after the HUD batch; one mesh each (a dynamic mesh can only be filled once per frame) */
    static Mesh *pm[NCHARS]; Mat4 id = m4_identity();
    for (int i = 0; i < NCHARS; i++) { if (!tx_portrait[i] || !char_unlocked(i)) continue;
        if (!pm[i]) { float x, y; char_card_pos(i, &x, &y);
            float a = g_portrait_aspect[i], pw = a < 1 ? 90 * a : 90, ph = a < 1 ? 90 : 90 / a;
            batch_clear(&g_b); batch_rect_uv(&g_b, x + 8 + (90 - pw) / 2, y + 8 + (90 - ph) / 2, pw, ph, 0, 0, 1, 1, 1, 1, 1, 1); ui_px(g_b.v, g_b.n, 7);
            pm[i] = gfx_mesh_static(g_b.v, g_b.n); }
        gfx_camera_ui3d(UI_EYE); Mat4 cm = ui_card_model(i); gfx_bind(tx_portrait[i]); gfx_draw(pm[i], &cm, 1, 1, 1, 1); }
    gfx_camera_2d(); (void)id;
}
#define KCARD_X(i) (36.0f + (i) * 242.0f)
#define KCARD_W 226.0f
#define KCARD_Y 324.0f
static float fitsz(const char *s, float size, float maxw) { float w = text_width(s, size); return w > maxw ? size * maxw / w : size; }   /* shrinks words that would run off a card */
static void kart_card(int i, const char *const (*desc)[2]) {
        int sel = i == g_player_style, open = style_unlocked(i); float x = KCARD_X(i), y = KCARD_Y, w = KCARD_W, h = 300; tzone(x, y, w, h, TZ_SEL, i);
        card_face(x, y, w, h, sel, open);
        if (!open) {
            batch_rect(&g_b, x + 16, y + 16, w - 32, 120, 0.2f, 0.22f, 0.3f, 1);
            batch_text(&g_b, "?", x + w / 2 - text_width("?", 80) / 2, y + 36, 80, 0.5f, 0.55f, 0.7f, 1);
            batch_text(&g_b, "LOCKED", x + 16, y + 150, 20, 0.8f, 0.8f, 0.9f, 1);
            const char *hint = unlock_hint(style_lock_bit(i)); char line[24]; int li = 0, row = 0;
            for (const char *q = hint; ; q++) { if ((*q == ' ' || *q == 0) && li >= 12) { line[li] = 0; batch_text(&g_b, line, x + 16, y + 182 + row * 20, 14, 1, 0.9f, 0.5f, 1); row++; li = 0; if (*q == 0) break; continue; }
                if (*q == 0) { line[li] = 0; if (li) batch_text(&g_b, line, x + 16, y + 182 + row * 20, 14, 1, 0.9f, 0.5f, 1); break; }
                if (li < 22) line[li++] = *q; }
            return;
        }
        batch_rect(&g_b, x + 16, y + 16, w - 32, 120, 0.05f, 0.08f, 0.2f, 1);       /* the emblem is drawn over this window */
        { float mw = w - 32; batch_text(&g_b, KART_STYLES[i].name, x + 16, y + 146, fitsz(KART_STYLES[i].name, 22, mw), 1, 1, 1, 1);
          batch_text(&g_b, desc[i][0], x + 16, y + 176, fitsz(desc[i][0], 13, mw), 0.8f, 0.9f, 1, 1); batch_text(&g_b, desc[i][1], x + 16, y + 194, fitsz(desc[i][1], 13, mw), 0.8f, 0.9f, 1, 1); }
        const KartStats *c = &CHARACTERS[g_player_char].st, *m = &KART_STYLES[i].mul;
        stat_bar(x + 16, y + 222, "SPEED", c->top_speed * m->top_speed);
        stat_bar(x + 16, y + 240, "ACCEL", c->accel * m->accel);
        stat_bar(x + 16, y + 258, "HANDLE", c->handling * m->handling);
        stat_bar(x + 16, y + 276, "WEIGHT", c->weight * m->weight);
    }
static void hud_kart(void) {
    char ttl[64]; snprintf(ttl, sizeof ttl, "%s'S KART", CHARACTERS[g_player_char].short_name); text_c(ttl, 20, 34, 1, 0.85f, 0.3f, 1);
    text_c("CHOOSE A BODY", 296, 20, 1, 0.85f, 0.3f, 1);
    static const char *desc[NSTYLES][2] = {{"THE EVERYDAY KART", "BALANCED"}, {"BUILT FOR SPEED", "WIDER TURNS"}, {"LIGHT AND NIMBLE", "QUICK TURNS"}, {"HEAVY AND STURDY", "FAST OFF THE LINE"}, {"DAVID'S SLING CART", "NIMBLE AND QUICK"}};
    for (int k = 0; k <= NSTYLES; k++) { int i = k < NSTYLES ? k : g_player_style; if (k < NSTYLES && i == g_player_style) continue;   /* the chosen card last, on top */
        uint32_t mk = ui_begin(); kart_card(i, desc); card_done(mk, i, KCARD_X(i), KCARD_Y, KCARD_W, 300, i == g_player_style); }
    text_c("A: CHOOSE   B: BACK", 690, 16, 0.8f, 0.85f, 1, 1);
}
/* "UNLOCKED!" after a race: the word bursts in over a dark screen with golden beams turning and flashing behind it,
   then it rises and the picture of the prize grows in under it (the character, the kart, the road or Goliath), with its name */
/* the kart-body emblems (tx_emblem) are made once at start-up: making a picture in the middle of drawing a frame scrambled it on the GamePad */
static void unlock_prize(int bit, Texture **pic, float *asp) {   /* the picture of what was won */
    *pic = NULL; *asp = 1;
    for (int c = 0; c < NCHARS; c++) if (char_lock_bit(c) == bit) { if (tx_char[c]) { *pic = tx_char[c]; *asp = g_char_aspect[c]; } else if (tx_portrait[c]) { *pic = tx_portrait[c]; *asp = g_portrait_aspect[c]; } }
    for (int sty = 1; sty < NSTYLES; sty++) if (style_lock_bit(sty) == bit && tx_emblem[sty]) { *pic = tx_emblem[sty]; *asp = 128.0f / 96.0f; }
    if (bit == UNLOCK_HEAVEN) for (int d = 0; d < 8; d++) if (TRACK_DEFS[d].bonus == 1 && tx_card[d]) { *pic = tx_card[d]; *asp = 1.85f; }
    if (bit == UNLOCK_GOLIATH && tx_goliath) { *pic = tx_goliath; *asp = g_goliath_aspect; } }
/* "UNLOCKED!" after a race, in 3D: the screen dims, the word bursts in with golden beams turning and flashing behind it,
   then the prize arrives on a thick gold medallion that spins in fast and slows to a stop facing you, with a burst of
   sparks and tumbling confetti flying out round it. Every mesh here is kept per screen (TV, GamePad). */
static void draw_unlock(const Mat4 *id, int drc) {
    static Vertex tv[2048]; Batch tb; batch_init(&tb, tv, 2048);
    static Mesh *m_back[2], *m_rays[2], *m_pic[2], *m_txt[2], *m_frame[2], *m_fx[2], *m_conf[2]; static int frame_bit[2] = {-1, -1};
    if (!m_back[drc]) { m_back[drc] = gfx_mesh_dynamic(12); m_rays[drc] = gfx_mesh_dynamic(64 * 6); m_pic[drc] = gfx_mesh_dynamic(12); m_txt[drc] = gfx_mesh_dynamic(4096); m_fx[drc] = gfx_mesh_dynamic(160 * 6); m_conf[drc] = gfx_mesh_dynamic(160 * 6); }
    int bit = (int)(g_unl_bits & (0u - g_unl_bits)); float t = g_unl_t, in = clampf(t / 0.35f, 0, 1), rise = clampf((t - 1.1f) / 0.5f, 0, 1), ease = 1 - (1 - rise) * (1 - rise);
    float flash = 0.5f + 0.5f * sinf(t * 9.0f), pop = t < 0.35f ? 0.4f + 0.6f * in + 0.25f * sinf(in * 3.14159f) : 1.0f, ty = 330 - 240 * ease;
    gfx_camera_2d(); gfx_state(ST_HUD);
    batch_rect(&tb, 0, 0, 1280, 720, 0.02f, 0.03f, 0.1f, 0.9f * in); gfx_mesh_update(m_back[drc], tb.v, tb.n); gfx_bind(tx_font); gfx_draw(m_back[drc], id, 1, 1, 1, 1);
    batch_clear(&tb);   /* beams turning behind everything, in time with the flashing */
    for (int i = 0; i < 28; i++) { float a = t * 0.45f + i * 6.2831853f / 28, len = (560 + 100 * sinf(t * 2.1f + i * 1.3f)) * in, wid = 18 + 10 * (i & 1), cy = 380 - 40 * ease;
        float dx = cosf(a), dy = sinf(a); Vec3 c = v3(640 + dx * len * 0.5f, cy + dy * len * 0.5f, 0);
        q2d(&tb, c, v3(dx * len * 0.5f, dy * len * 0.5f, 0), v3(-dy * wid, dx * wid, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 0.8f, 0.3f, (0.25f + 0.35f * flash) * in); }
    q2d(&tb, v3(640, ty + 20, 0), v3(440 + 40 * flash, 0, 0), v3(0, 110 + 20 * flash, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 0.9f, 0.5f, (0.45f + 0.45f * flash) * in);
    q2d(&tb, v3(640, 400, 0), v3(380, 0, 0), v3(0, 320, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 0.85f, 0.45f, 0.5f * clampf((t - 1.2f) * 2, 0, 1));   /* the glow behind the medallion */
    if (t < 0.5f) q2d(&tb, v3(640, 360, 0), v3(1400, 0, 0), v3(0, 800, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 1, 1, 1 - t / 0.5f);
    gfx_mesh_update(m_rays[drc], tb.v, tb.n); gfx_state(ST_HUD | ST_ADD); gfx_bind(tx_blob); gfx_draw(m_rays[drc], id, 1, 1, 1, 1); gfx_state(ST_HUD);
    /* the medallion, in 3D */
    Texture *pic; float asp; unlock_prize(bit, &pic, &asp); float W = clampf(asp, 0.55f, 1.7f), H = 1.0f;
    float arrive = clampf((t - 1.2f) / 1.6f, 0, 1), e3 = 1 - powf(1 - arrive, 3);
    if (arrive > 0) {
        if (frame_bit[drc] != bit || !m_frame[drc]) { batch_clear(&tb); float fw = 0.13f, d = 0.12f;   /* a gold frame with a deep blue plate, a gold back with a cross, a jewel at each corner */
            batch_box(&tb, v3(0, H + fw / 2, 0), v3(W + fw, fw / 2, d), 1.0f, 0.82f, 0.36f); batch_box(&tb, v3(0, -H - fw / 2, 0), v3(W + fw, fw / 2, d), 0.9f, 0.72f, 0.3f);
            batch_box(&tb, v3(-W - fw / 2, 0, 0), v3(fw / 2, H, d), 0.95f, 0.78f, 0.33f); batch_box(&tb, v3(W + fw / 2, 0, 0), v3(fw / 2, H, d), 0.95f, 0.78f, 0.33f);
            batch_box(&tb, v3(0, 0, -0.02f), v3(W, H, 0.05f), 0.08f, 0.14f, 0.38f); batch_box(&tb, v3(0, 0, -0.09f), v3(W + fw, H + fw, 0.03f), 0.85f, 0.68f, 0.28f);
            batch_box(&tb, v3(0, 0.1f, -0.14f), v3(0.07f, H * 0.7f, 0.03f), 1.0f, 0.86f, 0.4f); batch_box(&tb, v3(0, 0.35f, -0.14f), v3(W * 0.45f, 0.07f, 0.03f), 1.0f, 0.86f, 0.4f);
            for (int k = 0; k < 4; k++) batch_box(&tb, v3((k & 1 ? 1 : -1) * (W + fw / 2), (k & 2 ? 1 : -1) * (H + fw / 2), d + 0.02f), v3(0.07f, 0.07f, 0.04f), k & 1 ? 0.9f : 0.2f, 0.15f, k & 1 ? 0.2f : 0.85f);
            gfx_mesh_free(m_frame[drc]); m_frame[drc] = gfx_mesh_static(tb.v, tb.n); frame_bit[drc] = bit; }
        gfx_clear_depth(drc);   /* nothing of the race behind may cut into it */
        float aspect = drc ? 854.0f / 480.0f : 1280.0f / 720.0f; Mat4 vp = m4_mul(m4_perspective(0.62f, aspect, 0.5f, 60.0f), m4_look_at(v3(0, 0, 7.5f), v3(0, 0, 0), v3(0, 1, 0)));
        gfx_camera(&vp, v3(0.35f, 0.55f, 0.75f), 0.5f, 0, 0, 0, 1e6f, 2e6f);
        float spin = (1 - e3) * 5.0f * 3.14159265f + sinf(t * 1.3f) * 0.18f * e3, scale = 0.3f + 0.7f * e3, yy = -0.32f + (1 - e3) * -2.5f + 0.05f * sinf(t * 2.0f);
        Mat4 m = m4_mul(m4_translate(0, yy, 0), m4_mul(m4_rotate_y(spin), m4_mul(m4_rotate_x(0.12f * sinf(t * 1.7f)), m4_scale(scale * 1.25f, scale * 1.25f, scale * 1.25f))));
        gfx_state(ST_DEPTH | ST_DEPTH_WRITE); gfx_bind(NULL); gfx_draw_ex(m_frame[drc], &m, 1, 1, 1, 1, 0.9f, 0, 0, 0);
        if (pic && cosf(spin) > 0) { float pw = W * 0.94f, ph = H * 0.94f; if (pw / ph > asp) pw = ph * asp; else ph = pw / asp;
            batch_clear(&tb); batch_quad(&tb, v3(0, 0, 0.045f), v3(pw, 0, 0), v3(0, ph, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 1, 1, 1);
            gfx_mesh_update(m_pic[drc], tb.v, tb.n); gfx_state(ST_TRANSPARENT); gfx_bind(pic); gfx_draw_ex(m_pic[drc], &m, 1, 1, 1, 1, 0, 1, 0, 0); }
        /* sparks bursting out when it lands, and confetti tumbling down */
        float bt = t - 2.0f; batch_clear(&tb);
        if (bt > 0) for (int k = 0; k < 70; k++) { float h1 = noise2(k, 5), h2 = noise2(k, 9), h3 = noise2(k, 13), a = h1 * 6.2831853f, el = (h2 - 0.4f) * 2.2f, sp = 2.0f + h3 * 3.5f, lt = bt * (0.9f + h2 * 0.4f);
            if (lt > 1.6f) continue; float px2 = cosf(a) * cosf(el) * sp * lt, py2 = sinf(el) * sp * lt - 1.6f * lt * lt + yy, pz2 = sinf(a) * cosf(el) * sp * lt * 0.6f + 0.5f, s2 = 0.07f * (1.6f - lt);
            batch_quad(&tb, v3(px2, py2, pz2), v3(s2, 0, 0), v3(0, s2, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 0.8f + 0.2f * h3, 0.35f + 0.4f * h2, 1 - lt / 1.6f); }
        if (tb.n) { gfx_mesh_update(m_fx[drc], tb.v, tb.n); gfx_state(ST_TRANSPARENT | ST_ADD); gfx_bind(tx_blob); Mat4 i2 = m4_identity(); gfx_draw_ex(m_fx[drc], &i2, 1, 1, 1, 1, 0, 1, 0, 0); }
        batch_clear(&tb);
        if (bt > 0) for (int k = 0; k < 90; k++) { float h1 = noise2(k, 21), h2 = noise2(k, 22), h3 = noise2(k, 23), fall = fmodf(bt * (0.5f + 0.3f * h2) + h3 * 1.5f, 3.0f);
            float x = (h1 - 0.5f) * 7.5f + sinf(bt * 2 + k) * 0.3f, y = 2.6f - fall * 2.0f, z = (h2 - 0.5f) * 2.0f, ang = bt * (3 + h3 * 4) + k, cs = 0.06f;
            static const float cc[6][3] = {{1, 0.85f, 0.3f}, {0.95f, 0.3f, 0.35f}, {0.35f, 0.6f, 1}, {0.4f, 0.85f, 0.45f}, {1, 1, 1}, {0.75f, 0.45f, 0.95f}}; int ci = k % 6;
            batch_quad(&tb, v3(x, y, z), v3(cosf(ang) * cs * 1.6f, sinf(ang * 0.7f) * cs, sinf(ang) * cs), v3(0, cosf(ang * 1.3f) * cs, cs * 0.3f), v3(0, 0, 1), 0, 0, 1, 1, cc[ci][0], cc[ci][1], cc[ci][2], clampf(bt * 3, 0, 1)); }
        if (tb.n) { gfx_mesh_update(m_conf[drc], tb.v, tb.n); gfx_state(ST_TRANSPARENT); gfx_bind(NULL); Mat4 i2 = m4_identity(); gfx_draw_ex(m_conf[drc], &i2, 1, 1, 1, 1, 0, 1, 0, 0); }
        gfx_camera_2d(); gfx_state(ST_HUD); }
    /* the words on top */
    Batch save = g_b; static Vertex txv[2][4096]; batch_init(&g_b, txv[drc], 4096);
    float sz = 62 * pop, gl = 0.75f + 0.25f * flash;
    text_c("UNLOCKED!", ty + 4 - sz * 0.5f + 31, sz, 0, 0, 0, 0.6f * in);
    text_c("UNLOCKED!", ty - sz * 0.5f + 31, sz, 1, 0.85f * gl + 0.15f, 0.3f * gl, in);
    float ga = clampf((t - 2.4f) * 2, 0, 1);
    if (ga > 0) { const char *nm = unlock_name(bit); text_c(nm, 618, 40, 1, 1, 1, ga);
        if (t > 2.8f) text_c(bit == UNLOCK_HEAVEN || bit == UNLOCK_GOLIATH ? "NOW OPEN - PRESS A" : "NOW YOU CAN CHOOSE IT - PRESS A", 668, 20, 1, 0.85f, 0.3f, 0.6f + 0.4f * sinf(t * 4)); }
    gfx_mesh_update(m_txt[drc], g_b.v, g_b.n); gfx_bind(tx_font); gfx_draw(m_txt[drc], id, 1, 1, 1, 1);
    g_b = save;
}
static void draw_kart_models(void) {    /* the kart-body cards: an emblem for each kind of kart (the 3D stand-ins looked like an early build) */
    static Texture *tx_emb[NSTYLES]; static Mesh *m_emb[NSTYLES]; Mat4 id = m4_identity();
    gfx_camera_2d(); gfx_state(ST_HUD);
    for (int i = 0; i < NSTYLES; i++) { if (!style_unlocked(i)) continue;
        if (!m_emb[i]) m_emb[i] = gfx_mesh_dynamic(6); tx_emb[i] = tx_emblem[i];
        float x = KCARD_X(i) + 16, y = KCARD_Y + 16, bob = (i == g_player_style) ? 4.0f * sinf(g_time * 4.0f) : 0, ew = 150, eh = 112;
        Vertex tv[6]; Batch tb; batch_init(&tb, tv, 6); batch_rect_uv(&tb, x + (KCARD_W - 32 - ew) / 2, y + 4 + bob, ew, eh, 0, 0, 1, 1, 1, 1, 1, 1); ui_px(tb.v, tb.n, 7);
        gfx_mesh_update(m_emb[i], tb.v, tb.n); gfx_camera_ui3d(UI_EYE); Mat4 cm = ui_card_model(i); gfx_bind(tx_emb[i]); gfx_draw(m_emb[i], &cm, 1, 1, 1, i == g_player_style ? 1.0f : 0.75f); }
    gfx_camera_2d(); (void)id;
}
/* the road cards across the screen: the six roads, plus the Walls of Jericho in multiplayer */
static int ncards(void) { return g_mode == MODE_VS ? NUM_PLAY_TRACKS + 1 : NUM_PLAY_TRACKS; }
static int card_track(int d) { return d < NUM_PLAY_TRACKS ? d : JERICHO_TRACK; }
static int track_card(int t) { return t < NUM_PLAY_TRACKS ? t : NUM_PLAY_TRACKS; }
#define CARD_STEP (1200.0f / ncards())
#define CARD_W (CARD_STEP - 10.0f)
#define CARD_IN (CARD_W - 24.0f)
static void track_card_draw(int c2) { int d = card_track(c2);
        int sel = d == g_track_idx, open = track_unlocked(d); float x = 40 + c2 * CARD_STEP, y = 110, w = CARD_W, h = 400, iw = CARD_IN; tzone(x, y, w, h, TZ_SEL, d);
        card_face(x, y, w, h, sel, open);
        const TrackDef *td = &TRACK_DEFS[d]; char buf[48]; float ty = y + 12 + iw + 14;
        if (!open) {
            batch_rect(&g_b, x + 12, y + 12, iw, iw, 0.2f, 0.22f, 0.3f, 1);
            batch_text(&g_b, "?", x + 12 + iw / 2 - text_width("?", 90) / 2, y + 12 + iw / 2 - 45, 90, 0.5f, 0.55f, 0.7f, 1);
            batch_text(&g_b, "BONUS ROAD", x + 12, ty, 14, 1, 0.85f, 0.3f, 1);
            batch_text(&g_b, "WIN THE", x + 12, ty + 30, 14, 0.9f, 0.9f, 1, 1); batch_text(&g_b, "GRAND PRIX", x + 12, ty + 50, 14, 0.9f, 0.9f, 1, 1); batch_text(&g_b, "ON NORMAL", x + 12, ty + 70, 14, 0.9f, 0.9f, 1, 1);
            return;
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
static void hud_track(void) {
    text_c("CHOOSE A ROAD", 30, 40, 1, 0.85f, 0.3f, 1);
    int selc = track_card(g_track_idx);
    for (int k = 0; k <= ncards(); k++) { int c2 = k < ncards() ? k : selc; if (k < ncards() && c2 == selc) continue;   /* the chosen card last, on top */
        uint32_t mk = ui_begin(); track_card_draw(c2); card_done(mk, c2, 40 + c2 * CARD_STEP, 110, CARD_W, 400, c2 == selc); }
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
    for (int i = 0; i < 3; i++) tzone(400, 254 + i * 50, 480, 48, TZ_SEL, i);
    for (int r2 = 0; r2 < 2; r2++) { tzone(384, 414 + r2 * 40, 340, 40, TZ_LEFT, 3 + r2); tzone(724, 414 + r2 * 40, 180, 40, TZ_RIGHT, 3 + r2); }
    text_c("LEFT/RIGHT: VOLUME", 500, 14, 0.8f, 0.85f, 1, 1);
    text_c(g_race.jericho ? "A GAS   B BRAKE   ZR DRIFT   ZL SHOFAR" : "A GAS   B BRAKE   ZR HOLD TO DRIFT   ZL BLESSING", 526, 13, 0.7f, 0.8f, 1, 1);
    if (g_pause_reason[0]) text_c(g_pause_reason, 150, 18, 1, 0.5f, 0.4f, 1);
}
static void hud_battle(void) {
    Battle *b = &g_battle; char buf[64];
    text_c("THE VALLEY OF ELAH", 60, 16, 1, 0.85f, 0.3f, 1);
    batch_text(&g_b, "DAVID", 40, 30, 22, 1, 1, 1, 1);
    for (int i = 0; i < BATTLE_LIVES; i++) batch_rect(&g_b, 40 + i * 34, 60, 28, 24, i < BATTLE_LIVES - b->hits_on_david ? 0.95f : 0.25f, 0.2f, 0.25f, 1);
    snprintf(buf, sizeof buf, "STONES x%d", b->stones_held); batch_text(&g_b, buf, 40, 95, 22, 0.95f, 0.95f, 0.85f, 1);
    if (b->stones_held == 0 && b->race.state == RACE_RUNNING && fmodf(g_time, 1.0f) < 0.7f) batch_text(&g_b, "GET A STONE FROM THE BROOK!", 40, 124, 18, 1, 0.85f, 0.3f, 1);
    else if (b->stones_held > 0 && b->race.state == RACE_RUNNING) batch_text(&g_b, "ZL: SLING", 40, 124, 18, 0.8f, 0.9f, 1, 1);
    batch_text(&g_b, "GOLIATH", 1240 - text_width("GOLIATH", 22), 30, 22, 1, 1, 1, 1);
    for (int i = 0; i < battle_need(b); i++) batch_rect(&g_b, 1240 - (i + 1) * 36, 60, 30, 24, 0.9f, i < battle_need(b) - b->hits_on_goliath ? 0.65f : 0.15f, 0.15f, 1);
    if (b->gstate == GOL_DAZED) text_c("HE IS REELING - SLING AT HIS FOREHEAD!", 96, 22, 1, 0.9f, 0.3f, 0.6f + 0.4f * sinf(g_time * 8));
    { char tb[32]; fmt_time(tb, sizeof tb, BATTLE_TIME_LIMIT - b->race.race_t > 0 ? BATTLE_TIME_LIMIT - b->race.race_t : 0); batch_text(&g_b, tb, 640 - text_width(tb, 26) / 2, 30, 26, 1, 0.9f, 0.5f, 1); }
    if (b->race.state == RACE_COUNTDOWN) { int n = (int)ceilf(b->race.cd - 0.5f); if (n >= 1 && n <= 3) fx_count(n, b->race.cd, 640, 250, 110); text_c("PICK UP STONES, PRESS ZL TO SLING", 180, 18, 1, 1, 1, 1); text_c("HIS SHIELD-BEARER GUARDS HIS FRONT AS HE WALKS: SLING FROM THE SIDE, OR WHEN HE STOPS", 204, 15, 1, 0.9f, 0.5f, 1); }
    if (g_msg_t > 0) { float sz = text_width(g_msg, 60) > 1100 ? 44 : 60, ag = msg_age(0); fx_words(g_msg, 640, 260, sz, ag, g_msg_t, 1, 1, 1, 1, 1); if (g_msg2[0]) fx_line(g_msg2, 640, 334, 22, ag, g_msg_t, 0.9f, 0.95f, 1, 1); }
    { static int lastq = -1; static float qt0 = 0; if (b->quote != lastq) { lastq = b->quote; qt0 = g_time; }
    if (b->quote >= 0 && b->quote < BATTLE_NQUOTES) {      /* the words of 1 Samuel 17: the panel springs up from the bottom, the speaker's name on a gold tab, the words type themselves out */
        const BattleQuote *q = &BATTLE_QUOTES[b->quote]; float qa = g_time - qt0, e = ease_back(clampf(qa / 0.45f, 0, 1)), y0 = 560 + (1 - e) * 190, glow = 0.6f + 0.4f * sinf(g_time * 4);
        int goli = !strncmp(q->who, "GOLIATH", 7); float tr = goli ? 0.55f : 0.12f, tg = goli ? 0.08f : 0.16f, tb = goli ? 0.06f : 0.42f;   /* Goliath's words on red, the others on blue */
        batch_rect(&g_b, 146, y0 + 8, 1000, 120, 0, 0, 0, 0.35f);                                     /* its shadow */
        batch_rect(&g_b, 137, y0 - 3, 1006, 126, 1, 0.82f, 0.32f, 0.55f + 0.45f * glow);              /* gold edge, glowing */
        batch_rect(&g_b, 140, y0, 1000, 120, tr * 0.5f, tg * 0.5f, tb * 0.5f, 0.92f); batch_rect(&g_b, 140, y0, 1000, 40, tr, tg, tb, 0.55f);
        batch_rect(&g_b, 140 + fmodf(qa * 520, 1300) - 150, y0, 60, 120, 1, 1, 1, 0.05f);              /* a shine sweeping across */
        char head[64]; snprintf(head, sizeof head, "%s%s%s", q->who, q->who[0] ? "  -  " : "", q->ref); float hw = text_width(head, 16) + 28;
        batch_rect(&g_b, 160, y0 - 20, hw, 30, 1, 0.82f, 0.32f, 1); batch_rect(&g_b, 162, y0 - 18, hw - 4, 26, 0.18f, 0.1f, 0.04f, 1);   /* the name tab */
        batch_text(&g_b, head, 174, y0 - 12, 16, 1, 0.85f, 0.3f, 1);
        int shown = (int)((qa - 0.25f) * 48), total = 0; char line[80]; int li = 0, row = 0; const char *p = q->line;
        for (;; p++) { int brk = (*p == ' ' || *p == 0) && text_width(line, 19) > 860, end = *p == 0;
            if (brk || end) { line[li] = 0; int vis = shown - total; if (vis > 0) { if (vis < li) line[vis] = 0; batch_text(&g_b, line, 166, y0 + 36 + row * 26, 19, 1, 1, 1, 1); } total += li; row++; li = 0; if (end) break; continue; }
            if (li < 78) { line[li++] = *p; line[li] = 0; } }
        if (shown >= total && fmodf(g_time, 0.8f) < 0.5f) batch_rect(&g_b, 1110, y0 + 92 + 3 * sinf(g_time * 6), 14, 10, 1, 0.85f, 0.3f, 1);   /* done: a little marker bobs */
    } }
    if (g_demo && !g_attract) text_c("DEMO - MOVE THE STICK TO DRIVE", 700, 16, 1, 1, 1, 0.8f);
}
static void hud_battle_results(void) {
    Battle *b = &g_battle; char buf[64];
    panel(290, 150, 700, 400);
    text_c(b->won ? "GIANT SLAYER!" : "GOLIATH WINS", 175, 44, 1, 0.85f, 0.3f, 1);
    snprintf(buf, sizeof buf, "HEAD HITS ON GOLIATH: %d OF %d", b->hits_on_goliath, battle_need(b)); text_c(buf, 260, 24, 1, 1, 1, 1);
    snprintf(buf, sizeof buf, "HITS ON DAVID: %d OF %d", b->hits_on_david, BATTLE_LIVES); text_c(buf, 300, 24, 1, 1, 1, 1);
    if (b->blocks) { snprintf(buf, sizeof buf, "STONES THE SHIELD-BEARER CAUGHT: %d", b->blocks); text_c(buf, 370, 18, 0.8f, 0.9f, 1, 1); }
    { char tb[32]; fmt_time(tb, sizeof tb, b->race.race_t); snprintf(buf, sizeof buf, "TIME %s", tb); text_c(buf, 340, 24, 1, 1, 1, 1); }
    if (b->won) text_c("THE BATTLE IS THE LORD'S", 400, 20, 0.9f, 0.95f, 1, 1);
    text_c("A: FIGHT AGAIN   B: MENU", 500, 20, 0.9f, 0.95f, 1, 1);
    if (g_fresh_unlocks) { float yb = 600; batch_rect(&g_b, 240, yb, 800, 60, 1, 0.85f, 0.3f, 0.95f); batch_rect(&g_b, 246, yb + 6, 788, 48, 0.08f, 0.14f, 0.36f, 1);
        char line[96] = "UNLOCKED: "; int first = 1; for (int q = 0; q < UNLOCK_COUNT; q++) if (g_fresh_unlocks & (1u << q)) { if (!first) strncat(line, ", ", sizeof line - strlen(line) - 1); strncat(line, unlock_name(1 << q), sizeof line - strlen(line) - 1); first = 0; }
        text_c(line, yb + 20, 20, 1, 0.95f, 0.5f, 1); }
}
static int results_first_choice(void) { return (g_mode == MODE_JERICHO || g_mode == MODE_BATTLE) ? 1 : 0; }   /* one-of-a-kind roads have no 'next' */
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
    if (g_mode == MODE_GP) text_c("A: NEXT ROAD   (+2 POINTS FOR THE MOST GENEROUS)", 585, 18, 0.9f, 0.95f, 1, 1);
    else {   /* what next: a row of choices (left / right, A), each one tappable on the GamePad */
        static const char *lab[4] = {"NEXT ROAD", "RACE AGAIN", "CHOOSE ROAD", "MAIN MENU"};
        int first = results_first_choice(); float x = 640 - (4 - first) * 150;
        for (int i = first; i < 4; i++, x += 300) { int sel = i == g_res_sel; float w = 280, y = 572;
            if (sel) batch_rect(&g_b, x - 4, y - 4, w + 8, 48, 1, 0.85f, 0.3f, 1);
            batch_rect(&g_b, x, y, w, 40, sel ? 0.16f : 0.08f, sel ? 0.26f : 0.14f, sel ? 0.6f : 0.36f, 0.95f);
            batch_text(&g_b, lab[i], x + w / 2 - text_width(lab[i], 22) / 2, y + 9, 22, 1, sel ? 0.95f : 0.85f, sel ? 0.5f : 0.9f, 1);
            tzone(x, y - 6, w, 52, TZ_SEL, i); } }
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
        uint32_t mk = ui_begin(); card_face(x, y, w, h, sel, open); tzone(x, y, w, h, TZ_SEL, c);
        batch_text(&g_b, cp->name, x + 20, y + 20, 30, 1, 0.95f, 0.5f, 1);
        batch_text(&g_b, cp->sub, x + 20, y + 62, 13, 0.8f, 0.9f, 1, 1);
        if (!open) { batch_text(&g_b, "?", x + w / 2 - text_width("?", 110) / 2, y + 140, 110, 0.5f, 0.55f, 0.7f, 1);
            batch_text(&g_b, "LOCKED", x + w / 2 - text_width("LOCKED", 24) / 2, y + 290, 24, 0.8f, 0.8f, 0.9f, 1);
            batch_text(&g_b, "WIN THE GOSPEL CUP ON NORMAL OR HARD", x + w / 2 - text_width("WIN THE GOSPEL CUP ON NORMAL OR HARD", 14) / 2, y + 330, 14, 1, 0.9f, 0.5f, 1); card_done(mk, c, x, y, w, h, sel); continue; }
        for (int i = 0; i < cp->n; i++) { const TrackDef *td = &TRACK_DEFS[cp->roads[i]]; float ry = y + 100 + i * 56; char nm[8]; snprintf(nm, sizeof nm, "%d.", i + 1);
            batch_rect(&g_b, x + 20, ry, 44, 44, td->sky_r, td->sky_g, td->sky_b, 1); draw_preview(&g_preview[cp->roads[i]], x + 42, ry + 22, 34, 0.95f, 0.95f, 0.95f);
            batch_text(&g_b, nm, x + 76, ry + 10, 20, 1, 0.85f, 0.3f, 1); batch_text(&g_b, td->name, x + 110, ry + 10, 20, 1, 1, 1, 1); }
        { char line[64]; snprintf(line, sizeof line, "%d ROADS   WON: EASY %d  NORMAL %d  HARD %d", cp->n, g_save.gp_wins[0], g_save.gp_wins[1], g_save.gp_wins[2]); if (c == 0) batch_text(&g_b, line, x + 20, y + h - 36, 13, 0.8f, 0.9f, 1, 1); else { snprintf(line, sizeof line, "%d ROADS - THE WHOLE STORY", cp->n); batch_text(&g_b, line, x + 20, y + h - 36, 13, 0.8f, 0.9f, 1, 1); } }
        card_done(mk, c, x, y, w, h, sel);
    }
    text_c("A: CHOOSE   B: BACK   LEFT/RIGHT: CHANGE", 650, 16, 0.8f, 0.85f, 1, 1);
}
static void hud_gp(void) {
    panel(290, 80, 700, 560); char buf[64];
    text_c(g_gp_round >= NUM_GP_TRACKS ? (g_cup == 0 ? "GOSPEL CUP CHAMPION" : "GENESIS TO GLORY CHAMPION") : "STANDINGS", 100, g_gp_round >= NUM_GP_TRACKS && g_cup == 1 ? 32 : 40, 1, 0.85f, 0.3f, 1);
    int ord[NKARTS]; int nk = cer_order(ord);
    for (int p = 0; p < nk; p++) { int c = ord[p]; float y = 160 + p * 50; int me = c == g_player_char;
        if (me) ui_plaque(310, y - 6, 660, 44, 12, 8, &UI_SEL, 0.95f, 0.45f, fmodf(g_time * 0.4f, 1.4f));
        batch_text(&g_b, place_name(p + 1), 330, y, 30, 1, 1, 1, 1);
        batch_rect(&g_b, 440, y + 2, 26, 26, CHARACTERS[c].r, CHARACTERS[c].g, CHARACTERS[c].b, 1);
        batch_text(&g_b, CHARACTERS[c].short_name, 480, y, 30, me ? 1.0f : 0.9f, me ? 0.95f : 0.9f, me ? 0.5f : 0.9f, 1);
        snprintf(buf, sizeof buf, "%d PTS", g_gp_points[c]); batch_text(&g_b, buf, 780, y, 30, 1, 0.9f, 0.4f, 1); }
    if (g_gp_round >= NUM_GP_TRACKS) { if (ord[0] == g_player_char) text_c("YOU ARE THE CHAMPION!", 570, 28, 1, 0.9f, 0.3f, 1); text_c("A: MENU", 610, 20, 0.9f, 0.95f, 1, 1); }
    else { snprintf(buf, sizeof buf, "A: ROAD %d OF %d", g_gp_round + 1, NUM_GP_TRACKS); text_c(buf, 600, 20, 0.9f, 0.95f, 1, 1); }
}

// ------------------------------------------------------------------ the trophy ceremony at the end of a cup
static float seg_dist(float px, float py, float ax, float ay, float bx, float by) { float dx = bx - ax, dy = by - ay, t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy); t = t < 0 ? 0 : t > 1 ? 1 : t; float ex = px - ax - t * dx, ey = py - ay - t * dy; return sqrtf(ex * ex + ey * ey); }
/* emblems for the five kart bodies (128 x 96, gold on clear): what kind of kart it is, at a glance */

static float emb_ring(float px, float py, float cx, float cy, float r) { return fabsf(sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy)) - r); }
static float emb_wheel(float px, float py, float cx, float cy, float r, int spokes) {
    float d = emb_ring(px, py, cx, cy, r) - 3.5f, hub = sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy)) - 6.0f; if (hub < d) d = hub;
    for (int k = 0; k < spokes; k++) { float a = k * 3.14159265f / spokes; float sd = seg_dist(px, py, cx - cosf(a) * r, cy - sinf(a) * r, cx + cosf(a) * r, cy + sinf(a) * r) - 1.8f; if (sd < d) d = sd; }
    return d; }
static float emb_box(float px, float py, float x0, float y0, float x1, float y1) { float dx = fmaxf(x0 - px, px - x1), dy = fmaxf(y0 - py, py - y1); return fmaxf(dx, dy); }
static void px_emblem(int x, int y, uint32_t *o) {
    float px = x + 0.5f, py = y + 0.5f, d = 99;
    switch (g_emb_kind) {
        case 0: d = emb_wheel(px, py, 64, 48, 32, 3); break;                                         /* standard: one good wheel */
        case 1: d = emb_wheel(px, py, 46, 52, 30, 4);                                                 /* chariot: a big wheel, racing */
            for (int k = 0; k < 3; k++) { float sd = seg_dist(px, py, 86, 34 + k * 16, 120 - k * 8, 34 + k * 16) - 3.0f; if (sd < d) d = sd; }
            { float sd = seg_dist(px, py, 20, 26, 74, 18) - 3.0f; if (sd < d) d = sd; } break;
        case 2: { float hull = fmaxf(emb_box(px, py, 18, 56, 110, 72), (py - 56) * 0.9f - (px - 18) * 0.0f - 999); hull = fmaxf(emb_box(px, py, 18, 56, 110, 72), fmaxf(28 - px + (py - 56) * 0.75f, px - 100 - (72 - py) * 0.0f + (py - 56) * 0.75f)); d = hull;   /* boat: hull, mast and sail over a wave */
            float m = seg_dist(px, py, 62, 16, 62, 56) - 2.0f; if (m < d) d = m;
            float sail = fmaxf(fmaxf(66 - px, py - 50), (px - 66) * 1.2f - (py - 18)); if (sail < d) d = sail;
            float wv = fabsf(py - (82 + 4 * sinf(px * 0.2f))) - 2.5f; if (wv < d) d = wv; } break;
        case 3: { d = fmaxf(emb_box(px, py, 12, 52, 116, 74), fmaxf(20 - px + (74 - py) * 0.4f, px - 108 + (74 - py) * 0.4f) - 999);   /* ark: long hull, house, roof */
            d = emb_box(px, py, 14, 54, 114, 72); float h2 = emb_box(px, py, 40, 36, 88, 54); if (h2 < d) d = h2;
            float roof = fmaxf(py - 36, fabsf(px - 64) * 0.55f - (py - 18)); if (roof < d) d = roof; } break;
        case 4: { d = 99; for (int k = 0; k < 2; k++) { float sx = k ? 112 : 16; for (int q = 0; q < 8; q++) {   /* sling: two cords curving down to the pouch, with its stone */
                float t0 = q / 8.0f, t1 = (q + 1) / 8.0f; float ax = sx + (64 - sx) * t0, ay = 14 + 56 * sqrtf(t0), bx = sx + (64 - sx) * t1, by = 14 + 56 * sqrtf(t1);
                float sd = seg_dist(px, py, ax, ay, bx, by) - 2.0f; if (sd < d) d = sd; } }
            float pouch = sqrtf((px - 64) * (px - 64) / 1.8f + (py - 70) * (py - 70)) - 11; if (pouch < d) d = pouch;
            float stone = sqrtf((px - 64) * (px - 64) + (py - 66) * (py - 66)) - 7; if (stone < 0) { *o = rgba(225, 220, 205, 255); return; } } break;
    }
    float a = clampf(0.5f - d, 0, 1), rim = clampf(2.5f - d, 0, 1);   /* soft edge, with a dark outline round the gold */
    int r = (int)(255 * a + 50 * (1 - a)), g = (int)(205 * a + 30 * (1 - a)), b = (int)(80 * a + 20 * (1 - a));
    *o = rgba(r, g, b, (int)(255 * fmaxf(a, rim * 0.85f)));
}
static void px_qbox(int x, int y, uint32_t *o) {   /* one face of the blessing box: a frosted pane, brighter at the rim, with a white cross outlined in dark (128 x 128) */
    float fx = (x - 63.5f) / 64.0f, fy = (y - 63.5f) / 64.0f, edge = fmaxf(fabsf(fx), fabsf(fy));
    int a = 70 + (int)(edge * edge * 120.0f), v = 200 + (int)(edge * 55.0f);
    if (edge > 0.93f) { a = 235; v = 255; }   /* a bright border on each pane */
    /* the mark: a cross, upright bar longer than the arms (distance to the strokes, so the edges are soft) */
    float px = x + 0.5f, py = y + 0.5f, half = 9.0f;
    float d = fminf(seg_dist(px, py, 64, 22, 64, 106), seg_dist(px, py, 34, 50, 94, 50));
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
    int nk = 0; for (int i = 0; i < g_race.nk; i++) { int c = g_race.character[i], dup = 0; for (int j = 0; j < nk; j++) if (ord[j] == c) dup = 1; if (!dup) ord[nk++] = c; }   /* each character once */
    for (int a2 = 1; a2 < nk; a2++) { int v = ord[a2], j = a2 - 1; while (j >= 0 && g_gp_points[ord[j]] < g_gp_points[v]) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = v; }
    return nk;
}
/* The cup ceremony, in 3D. Three stands rise out of the floor while every character's points count up from 0 together,
   slowly at first and then faster; each stand grows with its points, so the lowest score stops first: 3rd place locks in
   with a burst of light, then 2nd, then 1st. Then a spotlight falls on the champion, the gold cup spins down over their
   head, fireworks go off and confetti tumbles, while the camera slowly swings round the stands. */
#define CER_T0 1.0f   /* the counting starts */
#define CER_D 5.5f    /* and the top score is reached this long after */
static float cer_ease(float u) { u = clampf(u, 0, 1); return powf(u, 1.6f); }
static float cer_uinv(float f) { return powf(clampf(f, 0, 1), 1.0f / 1.6f); }
static int cer_max(const int *ord) { int m = 1; for (int p = 0; p < 3; p++) if (g_gp_points[ord[p]] > m) m = g_gp_points[ord[p]]; return m; }
static float cer_lock_t(int p, const int *ord) { return CER_T0 + CER_D * cer_uinv((float)g_gp_points[ord[p]] / cer_max(ord)); }   /* when stand p stops growing */
static float cer_end(void) { int ord[NKARTS]; cer_order(ord); return cer_lock_t(0, ord); }
static float cer_shown(int p, const int *ord) { float f = cer_ease((g_screen_t - CER_T0) / CER_D) * cer_max(ord); float tot = (float)g_gp_points[ord[p]]; return f < tot ? f : tot; }
static const float CER_SX[3] = {0, -2.3f, 2.3f};   /* 1st in the middle, 2nd left, 3rd right */
static float g_cer_lab[3][4];   /* where each stand's points, place and name go on the screen (worked out in the 3D pass) */
static void hud_trophy(void) {
    float t = g_screen_t;
    batch_rect(&g_b, 0, 0, 1280, 720, 0.02f, 0.03f, 0.1f, 0.88f);
    for (int k = 0; k < 6; k++) batch_rect(&g_b, 0, 360 + k * 60, 1280, 60, 0.12f, 0.1f, 0.25f, 0.06f * k * clampf(t, 0, 1));   /* a warm floor glow */
}
static void hud_trophy_top(void) {   /* drawn over the 3D: the points, places and names, the title, the verse */
    float t = g_screen_t; char buf[96]; int ord[NKARTS]; int nk = cer_order(ord); float te = cer_lock_t(0, ord);
    static const float col[3][3] = {{1, 0.85f, 0.3f}, {0.85f, 0.88f, 0.95f}, {0.95f, 0.6f, 0.32f}};
    for (int p = 0; p < 3 && p < nk; p++) { float sx = g_cer_lab[p][0], sy = g_cer_lab[p][1], ny = g_cer_lab[p][3], lock = cer_lock_t(p, ord), a = clampf(t * 2, 0, 1);
        int pts = (int)floorf(cer_shown(p, ord) + 0.001f); if (t >= lock) pts = g_gp_points[ord[p]];
        float pop = t >= lock ? 1.0f + 0.5f * expf(-(t - lock) * 6.0f) : 1.0f, sz = 34 * pop;
        snprintf(buf, sizeof buf, "%d", pts); batch_text(&g_b, buf, sx - text_width(buf, sz) / 2 + 2, sy - sz / 2 + 2, sz, 0, 0, 0, 0.6f * a);
        batch_text(&g_b, buf, sx - text_width(buf, sz) / 2, sy - sz / 2, sz, t >= lock ? col[p][0] : 1, t >= lock ? col[p][1] : 1, t >= lock ? col[p][2] : 1, a);
        if (t >= lock) { const char *pl = p == 0 ? "1ST" : p == 1 ? "2ND" : "3RD"; float la = clampf((t - lock) * 3, 0, 1), ps = 30 * (1 + 0.6f * expf(-(t - lock) * 5));
            batch_text(&g_b, pl, sx - text_width(pl, ps) / 2, g_cer_lab[p][2] - ps / 2, ps, col[p][0], col[p][1], col[p][2], la); }
        const char *nm = CHARACTERS[ord[p]].short_name; batch_text(&g_b, nm, sx - text_width(nm, 20) / 2, ny, 20, 1, 1, 1, a); }
    const char *cup = g_cup == 0 ? "GOSPEL CUP" : "GENESIS TO GLORY CUP"; text_c(cup, 14, 36, 1, 0.85f, 0.3f, clampf(t * 2, 0, 1));
    if (t >= te + 0.9f) { snprintf(buf, sizeof buf, "CHAMPION: %s", CHARACTERS[ord[0]].name); text_c(buf, 58, 24, 1, 1, 1, clampf((t - te - 0.9f) * 2, 0, 1)); }
    int me = -1; for (int i = 0; i < nk; i++) if (ord[i] == g_player_char) me = i;
    if (t >= te + 1.4f && me >= 0) { float a = clampf((t - te - 1.4f) * 2, 0, 1);
        if (me == 0) text_c("YOU WON THE CUP!", 88, 22, 1, 0.9f, 0.3f, a); else { snprintf(buf, sizeof buf, "YOU FINISHED %s", place_name(me + 1)); text_c(buf, 88, 20, 0.85f, 0.9f, 1, a); } }
    if (t >= te + 2.0f) { float a = clampf((t - te - 2.0f) * 2, 0, 1); text_c("I HAVE FOUGHT A GOOD FIGHT, I HAVE FINISHED MY COURSE, I HAVE KEPT THE FAITH", 684, 14, 1, 1, 1, a); text_c("2 TIMOTHY 4:7", 702, 12, 1, 0.85f, 0.4f, a); }
    if (t >= te + 3.0f && ((int)(g_time * 2) & 1)) batch_text(&g_b, "A: CONTINUE", 1260 - text_width("A: CONTINUE", 16), 690, 16, 0.9f, 0.95f, 1, 1);
}
static void draw_trophy_pictures(int drc) {
    int ord[NKARTS]; int nk = cer_order(ord); float t = g_screen_t, te = cer_lock_t(0, ord); Mat4 id = m4_identity();
    static Mesh *m_col, *m_trim, *m_cup, *m_floor, *m_chr[2][3], *m_fx[2], *m_conf[2];
    if (!m_col) { Vertex *vv = g_vtmp; Batch tb; batch_init(&tb, vv, 8192);
        batch_box(&tb, v3(0, 0.5f, 0), v3(0.85f, 0.5f, 0.85f), 1, 1, 1); m_col = gfx_mesh_static(tb.v, tb.n); batch_clear(&tb);
        batch_box(&tb, v3(0, 0.06f, 0), v3(0.95f, 0.06f, 0.95f), 1, 1, 1); batch_box(&tb, v3(0, -0.02f, 0.9f), v3(0.95f, 0.04f, 0.04f), 1, 1, 1); m_trim = gfx_mesh_static(tb.v, tb.n); batch_clear(&tb);
        model_cup(&tb); m_cup = gfx_mesh_static(tb.v, tb.n); batch_clear(&tb);
        for (int k = 0; k < 3; k++) { float r0 = 3.5f + k * 2.2f; for (int q = 0; q < 32; q++) { float a0 = q * 0.19635f, a1 = a0 + 0.19635f, sh = (q & 1) ? 0.22f : 0.18f;   /* round stepped floor */
                batch_quad4(&tb, v3(cosf(a0) * r0, -0.02f * k, sinf(a0) * r0), v3(cosf(a1) * r0, -0.02f * k, sinf(a1) * r0), v3(cosf(a1) * (r0 + 2.2f), -0.02f * k, sinf(a1) * (r0 + 2.2f)), v3(cosf(a0) * (r0 + 2.2f), -0.02f * k, sinf(a0) * (r0 + 2.2f)), v3(0, 1, 0), 0, 0, 1, 1, sh + 0.05f * k, sh, sh + 0.12f, 1); } }
        for (int q = 0; q < 32; q++) { float a0 = q * 0.19635f, a1 = a0 + 0.19635f; batch_quad4(&tb, v3(0, 0.01f, 0), v3(cosf(a0) * 3.5f, 0.01f, sinf(a0) * 3.5f), v3(cosf(a1) * 3.5f, 0.01f, sinf(a1) * 3.5f), v3(0, 0.01f, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.35f, 0.3f, 0.5f, 1); }
        m_floor = gfx_mesh_static(tb.v, tb.n); }
    gfx_clear_depth(drc);
    float aspect = drc ? 854.0f / 480.0f : 1280.0f / 720.0f, sw = clampf(t / 1.6f, 0, 1), swe = 1 - (1 - sw) * (1 - sw) * (1 - sw);
    float orbit = sinf(t * 0.22f) * 0.38f * clampf(t - 1.0f, 0, 1), dist = 13.0f + 9.0f * (1 - swe), ey = 3.0f + 3.0f * (1 - swe);
    Vec3 eye = v3(sinf(orbit) * dist, ey, cosf(orbit) * dist), at = v3(0, 2.6f, 0);
    Mat4 vp = m4_mul(m4_perspective(0.72f, aspect, 0.3f, 200.0f), m4_look_at(eye, at, v3(0, 1, 0)));
    gfx_camera(&vp, v3(0.3f, 0.8f, 0.55f), 0.55f, 0, 0, 0, 1e6f, 2e6f);
    Vec3 R = v3_norm(v3(eye.z, 0, -eye.x)); if (eye.z < 0) R = v3(-R.x, 0, -R.z); R = v3_norm(v3_cross(v3(0, 1, 0), v3_norm(v3_sub(eye, at)))); R = v3(R.x, 0, R.z);
    gfx_state(ST_DEPTH | ST_DEPTH_WRITE); gfx_bind(NULL); gfx_draw(m_floor, &id, 1, 1, 1, 1);
    static const float col[3][3] = {{1, 0.8f, 0.3f}, {0.8f, 0.83f, 0.92f}, {0.85f, 0.5f, 0.25f}};
    float mx = (float)cer_max(ord), top[3];
    for (int p = 0; p < 3 && p < nk; p++) { float h = 0.25f + 2.6f * cer_shown(p, ord) / mx, x = CER_SX[p], lock = cer_lock_t(p, ord); top[p] = h;
        float pulse = t >= lock ? 1.0f + 0.4f * expf(-(t - lock) * 4) : 1.0f;
        Mat4 m = m4_mul(m4_translate(x, 0, 0), m4_scale(1, h, 1)); gfx_draw_ex(m_col, &m, col[p][0] * 1.15f * pulse, col[p][1] * 1.15f * pulse, col[p][2] * 1.15f * pulse, 1, 0.9f, 0, 0, 0);
        Mat4 mt = m4_translate(x, h, 0); gfx_draw_ex(m_trim, &mt, col[p][0], col[p][1], col[p][2], 1, 1.0f, 0, 0, 0);
        /* where the labels go: the points on the stand's face, the place above the head, the name at the foot */
        Vec3 pts[3] = {v3(x, h * 0.42f, 0.9f), v3(x, h * 0.42f + 0.62f, 0.9f), v3(x, -0.05f, 0.95f)};
        for (int k = 0; k < 3; k++) { const float *M = vp.m; float cx = M[0] * pts[k].x + M[4] * pts[k].y + M[8] * pts[k].z + M[12], cy = M[1] * pts[k].x + M[5] * pts[k].y + M[9] * pts[k].z + M[13], cw = M[3] * pts[k].x + M[7] * pts[k].y + M[11] * pts[k].z + M[15];
            if (cw < 0.01f) cw = 0.01f; float sx2 = (cx / cw * 0.5f + 0.5f) * 1280, sy2 = (1 - (cy / cw * 0.5f + 0.5f)) * 720;
            if (k == 0) { g_cer_lab[p][0] = sx2; g_cer_lab[p][1] = sy2; } else if (k == 1) g_cer_lab[p][2] = sy2; else g_cer_lab[p][3] = sy2 + 8; } }
    for (int p = 2; p >= 0; p--) { if (p >= nk) continue; int ch = ord[p]; Texture *ct = tx_char[ch]; float ca = g_char_aspect[ch]; if (!ct) continue;   /* the characters stand on their stands, facing us */
        if (!m_chr[drc][p]) m_chr[drc][p] = gfx_mesh_dynamic(6); float lock = cer_lock_t(p, ord), hop = t >= lock ? fabsf(sinf((t - lock) * 7.0f)) * 0.35f * expf(-(t - lock) * 1.8f) : 0;
        if (p == 0 && t > te + 0.4f) hop += fabsf(sinf(t * 3.2f)) * 0.12f;   /* the champion keeps jumping for joy */
        float hh = 2.3f, ww = hh * ca; Vertex cv[6]; Batch cb; batch_init(&cb, cv, 6);
        batch_quad(&cb, v3(CER_SX[p], top[p] + 0.12f + hh / 2 + hop, 0.1f), v3(R.x * ww / 2, 0, R.z * ww / 2), v3(0, hh / 2, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 1, 1, 1);
        gfx_mesh_update(m_chr[drc][p], cb.v, cb.n); gfx_state(ST_TRANSPARENT | ST_DEPTH_WRITE); gfx_bind(ct); gfx_draw_ex(m_chr[drc][p], &id, 1, 1, 1, 1, 0, 1, 0, 0); }
    gfx_bind(NULL); gfx_state(ST_DEPTH | ST_DEPTH_WRITE);
    if (t > te + 0.3f && nk > 0) { float u = clampf((t - te - 0.3f) / 1.4f, 0, 1), e = 1 - (1 - u) * (1 - u) * (1 - u);   /* the cup spins down over the champion */
        float y = top[0] + 2.55f + 6.0f * (1 - e) + 0.08f * sinf(t * 2.2f), sc = 0.75f * (0.4f + 0.6f * e);
        Mat4 m = m4_mul(m4_translate(0, y, 0.1f), m4_mul(m4_rotate_y(t * 1.6f + (1 - e) * 12.0f), m4_scale(sc, sc, sc))); gfx_draw_ex(m_cup, &m, 1, 1, 1, 1, 1.2f, 0, 0, 0); }
    /* light: a beam on each stand as it locks in, sparks bursting off its top, then fireworks over the champion */
    if (!m_fx[drc]) { m_fx[drc] = gfx_mesh_dynamic(700 * 6); m_conf[drc] = gfx_mesh_dynamic(200 * 6); }
    Batch fb; static Vertex fv[700 * 6]; batch_init(&fb, fv, 700 * 6);
    for (int p = 0; p < 3 && p < nk; p++) { float lock = cer_lock_t(p, ord), x = CER_SX[p]; if (t < lock) continue; float since = t - lock;
        float beam = p == 0 ? 0.55f : 0.3f * expf(-since * 0.5f) + 0.1f; batch_quad(&fb, v3(x, top[p] + 4.0f, 0), v3(R.x * 1.1f, 0, R.z * 1.1f), v3(0, 4.5f, 0), v3(0, 0, 1), 0, 0, 1, 1, col[p][0], col[p][1], col[p][2], beam);
        batch_quad(&fb, v3(x, top[p] + 0.3f, 0.3f), v3(R.x * 2.2f, 0, R.z * 2.2f), v3(0, 1.4f, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 0.95f, 0.8f, 0.8f * expf(-since * 3));   /* the flash */
        if (since < 1.4f) for (int k = 0; k < 40; k++) { float h1 = noise2(k, p * 7 + 1), h2 = noise2(k, p * 7 + 2), a = h1 * 6.2831853f, sp = 2.5f + h2 * 3, el = 0.3f + h2 * 0.9f;
            float px2 = x + cosf(a) * cosf(el) * sp * since, py2 = top[p] + sinf(el) * sp * since - 3.0f * since * since, pz2 = sinf(a) * cosf(el) * sp * since, s2 = 0.09f * (1.4f - since);
            batch_quad(&fb, v3(px2, py2, pz2), v3(R.x * s2, 0, R.z * s2), v3(0, s2, 0), v3(0, 0, 1), 0, 0, 1, 1, col[p][0], col[p][1], col[p][2], 1 - since / 1.4f); } }
    if (t > te + 0.6f) for (int b = 0; b < 6; b++) { float bt = fmodf(t - te - 0.6f + b * 0.55f, 3.3f); if (bt > 1.8f) continue; float h1 = noise2(b, (int)((t - te + b * 0.55f) / 3.3f) + 40);   /* fireworks */
        Vec3 c = v3((h1 - 0.5f) * 9.0f, 5.0f + noise2(b, 3) * 2.0f, -2.0f - noise2(b, 4) * 3.0f); float hue = h1 * 6.0f, cr = clampf(fabsf(hue - 3) - 1, 0, 1) * 0.7f + 0.3f, cg = clampf(2 - fabsf(hue - 2), 0, 1) * 0.7f + 0.3f, cb2 = clampf(2 - fabsf(hue - 4), 0, 1) * 0.7f + 0.3f;
        for (int k = 0; k < 42; k++) { float a = k * 0.1496f, el = noise2(k, b) * 2 - 1, sp = 2.4f; Vec3 d = v3(cosf(a) * sqrtf(1 - el * el), el, sinf(a) * sqrtf(1 - el * el)); float s2 = 0.08f * (1.8f - bt);
            batch_quad(&fb, v3(c.x + d.x * sp * bt, c.y + d.y * sp * bt - 0.9f * bt * bt, c.z + d.z * sp * bt), v3(R.x * s2, 0, R.z * s2), v3(0, s2, 0), v3(0, 0, 1), 0, 0, 1, 1, cr, cg, cb2, clampf(1.8f - bt, 0, 1)); } }
    if (fb.n) { gfx_mesh_update(m_fx[drc], fb.v, fb.n); gfx_state(ST_TRANSPARENT | ST_ADD); gfx_bind(tx_blob); gfx_draw_ex(m_fx[drc], &id, 1, 1, 1, 1, 0, 1, 0, 0); }
    batch_clear(&fb);
    if (t > te + 0.5f) for (int k = 0; k < 160; k++) { float h1 = noise2(k, 61), h2 = noise2(k, 62), h3 = noise2(k, 63), ct = t - te - 0.5f, fall = fmodf(ct * (0.6f + 0.4f * h2) + h3 * 6.0f, 6.0f);   /* confetti */
        float x = (h1 - 0.5f) * 12.0f + sinf(ct * 1.5f + k) * 0.4f, y = 7.0f - fall * 1.4f, z = (h2 - 0.5f) * 6.0f, ang = ct * (2 + h3 * 4) + k, cs = 0.07f;
        static const float cc[6][3] = {{1, 0.85f, 0.3f}, {0.95f, 0.3f, 0.35f}, {0.35f, 0.6f, 1}, {0.4f, 0.85f, 0.45f}, {1, 1, 1}, {0.75f, 0.45f, 0.95f}}; int ci = k % 6;
        batch_quad(&fb, v3(x, y, z), v3(cosf(ang) * cs * 1.5f, sinf(ang * 0.7f) * cs, sinf(ang) * cs), v3(0, cosf(ang * 1.3f) * cs, cs * 0.4f), v3(0, 0, 1), 0, 0, 1, 1, cc[ci][0], cc[ci][1], cc[ci][2], 1); }
    if (fb.n) { gfx_mesh_update(m_conf[drc], fb.v, fb.n); gfx_state(ST_TRANSPARENT); gfx_bind(NULL); gfx_draw_ex(m_conf[drc], &id, 1, 1, 1, 1, 0, 1, 0, 0); }
    gfx_camera_2d(); gfx_state(ST_HUD); gfx_bind(NULL);
}

/* ---- the cup winner's Bible story (Sean: "after the trophy ceremony you click the A button and then it automatically
   starts a scene that centers around the winner of the cup. The scene should be biblical and a teaching of scripture.")
   The winner's own 3D kart drives into a little 3D set of their story; the camera moves round them while the story is
   told in five beats: the title, two lines of the story, the verse, and what it teaches us. A finishes the line being
   typed, or moves on; the beats also move on by themselves. The words are in cupstory.h. ---- */
#include "cupstory.h"
static void sc_load_props(void);
typedef char sc_fourteen_stories[(NCHARS == 14) ? 1 : -1];
enum { SB_TITLE, SB_STORY1, SB_STORY2, SB_VERSE, SB_LESSON, SB_COUNT };
static int g_sc_char, g_sc_beat; static float g_sc_bt, g_sc_out, g_sc_roll;   /* who, which beat, seconds into it, the fade-out (0 = staying), how far the kart has rolled */
static Vec3 g_sc_eye, g_sc_at, g_sc_eye0, g_sc_at0;                         /* the camera, and where it was when the beat changed */
#define SC_TYPE 38.0f   /* letters a second as the words type themselves out */
static const char *sc_text(int beat) { const CupStory *s = &CUP_STORIES[g_sc_char];
    switch (beat) { case SB_TITLE: return s->place; case SB_STORY1: return s->story[0]; case SB_STORY2: return s->story[1]; case SB_VERSE: return s->verse; default: return s->lesson; } }
static float sc_typed_t(int beat) { return beat == SB_TITLE ? 2.2f : 0.25f + (float)strlen(sc_text(beat)) / SC_TYPE; }
static float sc_beat_len(int beat) {
#ifdef KK_TOUR
    return 4.0f;   /* the CI photos: every story goes by quickly */
#endif
    return beat == SB_TITLE ? 6.0f : sc_typed_t(beat) + (beat == SB_VERSE ? 5.0f : 4.0f); }
typedef struct { float top[3], hor[3], gnd[3], sun[3], amb[3]; Vec3 focus; int night; } SetLook;
static const SetLook SET_LOOK[SET_COUNT] = {
    /* MOUNT */    {{0.22f, 0.45f, 0.85f}, {0.78f, 0.86f, 0.95f}, {0.42f, 0.5f, 0.28f}, {1.0f, 0.95f, 0.85f}, {0.65f, 0.72f, 0.85f}, {-7, 0.8f, 1.5f}, 0},
    /* REDSEA */   {{0.12f, 0.10f, 0.30f}, {0.95f, 0.55f, 0.38f}, {0.80f, 0.66f, 0.44f}, {1.0f, 0.78f, 0.6f}, {0.55f, 0.5f, 0.7f}, {0, 3.5f, -18}, 0},
    /* ELAH */     {{0.28f, 0.52f, 0.88f}, {0.86f, 0.86f, 0.80f}, {0.50f, 0.58f, 0.30f}, {1.0f, 0.95f, 0.85f}, {0.65f, 0.7f, 0.8f}, {-4.5f, 0.8f, -8}, 0},
    /* PALACE */   {{0.10f, 0.04f, 0.14f}, {0.34f, 0.18f, 0.30f}, {0.86f, 0.82f, 0.74f}, {1.0f, 0.85f, 0.65f}, {0.6f, 0.45f, 0.55f}, {0, 2.2f, -10}, 0},
    /* STORM */    {{0.02f, 0.03f, 0.07f}, {0.12f, 0.15f, 0.22f}, {0.10f, 0.20f, 0.30f}, {0.55f, 0.6f, 0.75f}, {0.3f, 0.35f, 0.5f}, {-4, 1.2f, -7}, 1},
    /* NAZARETH */ {{0.14f, 0.18f, 0.42f}, {0.95f, 0.70f, 0.52f}, {0.70f, 0.60f, 0.43f}, {1.0f, 0.8f, 0.62f}, {0.55f, 0.55f, 0.72f}, {-3.5f, 1.8f, -5}, 0},
    /* LIONS */    {{0.04f, 0.035f, 0.03f}, {0.20f, 0.15f, 0.10f}, {0.46f, 0.38f, 0.29f}, {1.0f, 0.9f, 0.7f}, {0.4f, 0.33f, 0.28f}, {3, 0.6f, -4}, 1},
    /* TEMPLE */   {{0.18f, 0.13f, 0.10f}, {0.62f, 0.46f, 0.30f}, {0.60f, 0.55f, 0.47f}, {1.0f, 0.85f, 0.65f}, {0.55f, 0.45f, 0.4f}, {3.2f, 4, -2}, 0},
    /* ARARAT */   {{0.34f, 0.54f, 0.85f}, {0.90f, 0.90f, 0.86f}, {0.35f, 0.55f, 0.30f}, {1.0f, 0.97f, 0.9f}, {0.65f, 0.72f, 0.85f}, {6, 4, -16}, 0},
    /* EGYPT */    {{0.32f, 0.58f, 0.90f}, {0.98f, 0.88f, 0.66f}, {0.88f, 0.72f, 0.46f}, {1.0f, 0.92f, 0.75f}, {0.7f, 0.68f, 0.7f}, {-8, 4, -30}, 0},
    /* SHORE */    {{0.30f, 0.55f, 0.86f}, {0.86f, 0.90f, 0.95f}, {0.86f, 0.78f, 0.58f}, {1.0f, 0.95f, 0.85f}, {0.65f, 0.72f, 0.85f}, {6, 1.5f, -15}, 0},
    /* JORDAN */   {{0.30f, 0.55f, 0.90f}, {0.90f, 0.90f, 0.80f}, {0.52f, 0.55f, 0.34f}, {1.0f, 0.95f, 0.85f}, {0.65f, 0.72f, 0.85f}, {-3, 1.5f, -4}, 0},
    /* HEAVEN */   {{0.50f, 0.62f, 0.95f}, {1.00f, 0.95f, 0.85f}, {0.95f, 0.93f, 0.90f}, {1.0f, 0.95f, 0.8f}, {0.85f, 0.85f, 0.9f}, {0, 8, -30}, 0},
};
/* where the guest stands (Jesus on the water, Gabriel over Nazareth, Mary...) and which way they face */
static Vec3 sc_guest_pos(int set) { return set == SET_STORM ? v3(-4, 0, -7) : set == SET_JORDAN ? v3(-3.2f, 0, -4.5f) : v3(-3.5f, 0, -5); }
static float sc_wave(float x, float z, float t, float amp) { return amp * (sinf(x * 0.55f + t * 1.3f) + 0.7f * sinf(z * 0.43f - t * 1.05f) + 0.3f * sinf((x + z) * 1.1f + t * 2.1f)); }
static float sc_flash(void) { float ph = fmodf(g_time, 6.3f); return ph < 0.1f ? 1.0f : (ph > 0.2f && ph < 0.32f) ? 0.6f : 0.0f; }   /* lightning in the storm */
static float sc_kart_h(void) { const Kart3D *k = &g_k3d[g_sc_char]; return k->ok ? clampf(k->hi.y, 0.8f, 3.0f) : 1.6f; }
static float sc_smooth(float u) { u = clampf(u, 0, 1); return u * u * (3 - 2 * u); }
static Vec3 sc_lerp(Vec3 a, Vec3 b, float u) { return v3(a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, a.z + (b.z - a.z) * u); }
static void sc_shot(int beat, float u, Vec3 *eye, Vec3 *at) {   /* where the camera wants to be in each beat (u: 0..1 through it) */
    float hy = sc_kart_h(); Vec3 F = SET_LOOK[CUP_STORIES[g_sc_char].set].focus;
    switch (beat) {
    case SB_TITLE: { float e = sc_smooth(u * 1.4f); *eye = sc_lerp(v3(0, 10, 30), v3(3.5f, 2.6f, 11), e); *at = v3(0, hy * 0.6f, 0); } break;
    case SB_STORY1: { float a = 0.55f + 0.3f * u; *eye = v3(sinf(a) * 8.5f, 2.0f + 0.5f * u, cosf(a) * 8.5f); *at = v3(0, hy * 0.7f, 0); } break;
    case SB_STORY2: { Vec3 d = v3_norm(v3(F.x, 0, F.z)), side = v3(-d.z, 0, d.x); if (side.x < 0) side = v3(-side.x, 0, -side.z);   /* over the kart's shoulder toward the guest or the prop */
        float back = 7.5f - 1.5f * u; *eye = v3(-d.x * back + side.x * 3.0f, 2.6f + F.y * 0.25f, -d.z * back + side.z * 3.0f); *at = sc_lerp(v3(0, hy * 0.7f, 0), F, 0.6f); } break;
    case SB_VERSE: *eye = v3(0.8f - 1.6f * u, 0.8f, 5.0f - 0.6f * u); *at = v3(0, hy + 0.3f, 0); break;   /* low, in front, looking up at them */
    default: { float e = sc_smooth(u); *eye = sc_lerp(v3(0, 2.4f, 8), v3(0, 7.5f, 19), e); *at = v3(0, hy * 0.8f + 1.5f * e, 0); } break; } }
static void sc_keep_clear(Vec3 *eye, Vec3 at);
static void scene_start(int ch) {
    g_sc_char = ch >= 0 && ch < NCHARS ? ch : 0; g_sc_beat = SB_TITLE; g_sc_bt = 0; g_sc_out = 0; g_sc_roll = 0; g_screen = SCR_SCENE; g_screen_t = 0;
    g_sc_eye = g_sc_eye0 = v3(0, 10, 30); g_sc_at = g_sc_at0 = v3(0, 1, 0);
    sc_load_props();
    if (CUP_STORIES[g_sc_char].set == SET_ELAH && !g_gol3d.ok) anim3d_load(&g_gol3d, "goliath");   /* the giant, fallen on his face */
    gfx_log("cup story: %s (%s)", CHARACTERS[g_sc_char].name, CUP_STORIES[g_sc_char].title); }
static void sc_next(void) {
    g_sc_eye0 = g_sc_eye; g_sc_at0 = g_sc_at; g_sc_beat++; g_sc_bt = 0; int set = CUP_STORIES[g_sc_char].set;
    if (g_sc_beat == SB_STORY2) { if (set == SET_JORDAN) sfx_play(SFX_DOVE, 0.8f, 0); else if (set == SET_SHORE) sfx_play(SFX_WHALE_ROAR, 0.6f, 0.3f); else if (set == SET_STORM || set == SET_REDSEA) sfx_play(SFX_SPLASH, 0.7f, 0); else if (set == SET_HEAVEN) sfx_play(SFX_SHOFAR, 0.6f, 0); }
    if (g_sc_beat == SB_VERSE) sfx_play(SFX_UNLOCK, 0.55f, 0);
    if (g_sc_beat == SB_LESSON) { int hv = g_voice[g_sc_char][VO_WIN]; if (hv >= 0) sfx_voice_play(hv, 1.0f, 0); } }
static void scene_leave(void) {
#ifdef KK_TOUR
    scene_start((g_sc_char + 1) % NCHARS); return;   /* the CI photos: on to the next story */
#endif
    g_multi = 0; start_race(1); g_screen = SCR_MAIN; g_screen_t = 0; }
static void scene_update(float dt, int pa) {
    if (g_sc_out > 0) { g_sc_out += dt; if (g_sc_out > 0.7f) scene_leave(); return; }
    g_sc_bt += dt; int last = g_sc_beat == SB_COUNT - 1;
    if (g_sc_beat == SB_TITLE) { float u = clampf(g_sc_bt / 3.2f, 0, 1), z = -16.0f * (1 - u) * (1 - u); static float lastz; if (g_sc_bt < dt * 1.5f) lastz = -16; g_sc_roll += z - lastz; lastz = z; }
    if (pa && g_screen_t > 0.6f) { sfx_play(SFX_MENU_OK, 0.5f, 0);
        if (g_sc_beat != SB_TITLE && g_sc_bt < sc_typed_t(g_sc_beat)) g_sc_bt = sc_typed_t(g_sc_beat);   /* finish the line */
        else if (last) g_sc_out = 0.001f; else sc_next(); }
#ifdef KK_TOUR
    else if (g_sc_bt > sc_beat_len(g_sc_beat)) { if (last) g_sc_out = 0.001f; else sc_next(); }
#else
    else if (!last && g_sc_bt > sc_beat_len(g_sc_beat)) sc_next();
#endif
    Vec3 e, a; sc_shot(g_sc_beat, clampf(g_sc_bt / sc_beat_len(g_sc_beat), 0, 1), &e, &a);
    float bl = g_sc_beat == SB_TITLE ? 1.0f : sc_smooth(g_sc_bt / 1.5f); g_sc_eye = sc_lerp(g_sc_eye0, e, bl); g_sc_at = sc_lerp(g_sc_at0, a, bl);
    sc_keep_clear(&g_sc_eye, g_sc_at); }

/* ---- the sets, all solid 3D (Sean: "Everything needs to be in 3d"): Meshy models for the things in the stories
   (content/models/prop_*.kkm, made by tools/model3d/meshy_props.py + prop_pack.py), and built shapes for the rest:
   a sky dome with a round sun and real clouds, textured ground with hills and river banks, beams of light as cones,
   the rainbow as a curved tube, the Red Sea as standing walls of water, the pillar of fire as turning flame cones ---- */
enum { PR_ARK, PR_LION, PR_SHEEP, PR_PALM, PR_BOAT, PR_PYRAMID, PR_PILLAR, PR_THRONE, PR_HOUSE, PR_VSIT, PR_VSTAND, PR_DOVE, PR_GATE, PR_OLIVE, PR_SACKS, PR_BOULDER, PR_CLOUD, PR_COUNT };
static const char *PROP_FILE[PR_COUNT] = {"prop_ark", "prop_lion", "prop_sheep", "prop_palm", "prop_boat", "prop_pyramid", "prop_pillar", "prop_throne", "prop_house",
    "prop_villager_sit", "prop_villager", "prop_dove", "prop_heaven_gate", "prop_olive", "prop_sacks", "prop_boulder", "prop_cloud"};
static Kart3D g_prop[PR_COUNT]; static int g_props_tried;
static void sc_load_props(void) { if (g_props_tried) return; g_props_tried = 1; for (int i = 0; i < PR_COUNT; i++) kart3d_load(&g_prop[i], PROP_FILE[i]); }
typedef struct { int p; float x, y, z, yaw, s, r, g, b; } Place;
static int sc_put(Place *o, int n, int p, float x, float y, float z, float yaw, float s, float r, float g, float b) { if (n < 96) { Place q = {p, x, y, z, yaw, s, r, g, b}; o[n] = q; } return n < 96 ? n + 1 : n; }
#define PUT(p, x, y, z, yaw, s) n = sc_put(o, n, p, x, y, z, yaw, s, 1, 1, 1)
#define PUTC(p, x, y, z, yaw, s, r, g, b) n = sc_put(o, n, p, x, y, z, yaw, s, r, g, b)
static int sc_places(int set, Place *o) {   /* where every model stands in each story */
    int n = 0;
    switch (set) {
    case SET_MOUNT:   /* the people sat on the grass round Him (Matthew 5:1) */
        for (int row = 0; row < 3; row++) for (int k = 0; k < 7; k++) { float h1 = noise2(k, row + 5), x = -5.0f - row * 1.6f - h1 * 0.4f, z = -3.5f + k * 1.3f + row * 0.4f;
            PUTC(PR_VSIT, x, 0, z, atan2f(-x, -z) , 1.0f, 0.75f + 0.25f * h1, 0.75f + 0.25f * noise2(k, row + 9), 0.75f + 0.25f * noise2(k, row + 13)); }
        PUT(PR_VSTAND, -10.5f, 0, -2, 1.3f, 1); PUT(PR_VSTAND, -10, 0, 3, 1.9f, 1); PUT(PR_OLIVE, 6, 0, -5, 0.4f, 1.1f); PUT(PR_OLIVE, 9, 0, 2, 2.0f, 0.9f); PUT(PR_OLIVE, -13, 0, -7, 1.0f, 1.2f); PUT(PR_OLIVE, 4, 0, -13, 3.0f, 1.0f);
        PUT(PR_SHEEP, 7, 0, 6, 2.5f, 1); break;
    case SET_REDSEA:
        for (int k = 0; k < 12; k++) PUTC(PR_VSTAND, ((k & 1) ? 2.4f : -2.4f) + (noise2(k, 33) - 0.5f) * 0.8f, 0, -3.0f - (k / 2) * 1.5f, 0, 1, 0.8f + 0.2f * noise2(k, 34), 0.8f + 0.2f * noise2(k, 35), 0.8f + 0.2f * noise2(k, 36));
        for (int k = 0; k < 10; k++) PUT(PR_BOULDER, (k & 1 ? 4.3f : -4.3f) + (noise2(k, 37) - 0.5f), 0, -40 + k * 7.0f, k * 1.3f, 0.25f + 0.2f * noise2(k, 38));
        PUT(PR_SHEEP, 1.5f, 0, -12, 0, 1); PUT(PR_SHEEP, -1.2f, 0, -13, 0.3f, 1); break;
    case SET_ELAH:
        PUT(PR_SHEEP, 9, 0, -3, 1.4f, 1); PUT(PR_SHEEP, 10.5f, 0, -1, 2.2f, 1); PUT(PR_SHEEP, 8, 0, 1.5f, 0.6f, 1); PUT(PR_SHEEP, 11, 0, 3, 3.9f, 1);
        PUT(PR_OLIVE, -10, 0, 4, 0, 1); PUT(PR_OLIVE, 14, 0, -10, 1, 1.2f);
        for (int k = 0; k < 6; k++) PUT(PR_BOULDER, -9 + k * 4.0f + noise2(k, 39) * 2, 0, -14 - noise2(k, 40) * 4, k, 0.4f + 0.4f * noise2(k, 41)); break;
    case SET_PALACE:
        for (int k = 0; k < 5; k++) for (int s = -1; s <= 1; s += 2) PUTC(PR_PILLAR, s * 6.0f, 0, 6 - k * 5.0f, 0, 9.0f / 9.0f, 1.0f, 0.95f, 0.85f);
        PUT(PR_THRONE, 0, 0, -10.5f, 0, 1); break;
    case SET_STORM:
        PUTC(PR_CLOUD, -20, 16, -50, 0, 4, 0.35f, 0.38f, 0.45f); PUTC(PR_CLOUD, 15, 18, -60, 1, 5, 0.3f, 0.33f, 0.4f); PUTC(PR_CLOUD, 0, 22, -40, 2, 4.5f, 0.32f, 0.35f, 0.42f); PUTC(PR_CLOUD, -40, 20, -30, 3, 5, 0.3f, 0.32f, 0.4f); break;
    case SET_NAZARETH:
        PUT(PR_HOUSE, 4.5f, 0, -7, -0.4f, 1); PUT(PR_HOUSE, -8, 0, -9, 0.5f, 1.1f); PUT(PR_HOUSE, 9, 0, -13, -0.2f, 1.2f); PUT(PR_HOUSE, -2, 0, -16, 0.1f, 1); PUT(PR_HOUSE, 12, 0, -4, -1.2f, 0.9f); PUT(PR_HOUSE, -12, 0, -2, 1.3f, 0.95f);
        PUT(PR_PALM, 7, 0, -2, 0, 1); PUT(PR_PALM, -10, 0, -13, 1, 1.1f); PUT(PR_OLIVE, 2, 0, -11, 0, 0.9f); PUT(PR_SACKS, 6, 0, -4.5f, 0.3f, 1); break;
    case SET_LIONS:   /* a pit walled with great rocks, its mouth open toward us */
        for (int row = 0; row < 3; row++) for (int k = 0; k < 22; k++) { float a = k * 0.2856f + row * 0.14f; if (sinf(a) > 0.5f) continue;
            PUT(PR_BOULDER, cosf(a) * (9.5f - row * 0.4f), row * 2.3f - 0.3f, sinf(a) * (9.5f - row * 0.4f) - 2, k * 1.7f + row, 1.3f + 0.4f * noise2(k, row + 43)); }
        PUT(PR_LION, 3.2f, 0, -4, -1.2f, 1); PUT(PR_LION, -4, 0, -3.5f, 1.6f, 1); PUT(PR_LION, 5.5f, 0, 1, -2.2f, 1); PUT(PR_LION, -2, 0, -7.5f, 0.9f, 1); break;
    case SET_TEMPLE:
        PUTC(PR_PILLAR, -4.0f, 0, -3, 0, 1.15f, 1, 0.95f, 0.88f); PUTC(PR_PILLAR, 4.0f, 0, -3, 0.5f, 1.15f, 1, 0.95f, 0.88f);   /* the two middle pillars (Judges 16:29) */
        for (int k = 0; k < 4; k++) for (int s = -1; s <= 1; s += 2) PUTC(PR_PILLAR, s * 10.0f, 0, -2 - k * 7.0f, k, 1.05f, 0.9f, 0.85f, 0.78f);
        for (int k = 0; k < 6; k++) PUT(PR_BOULDER, (noise2(k, 45) - 0.5f) * 14, 0, -8 - noise2(k, 46) * 10, k, 0.15f + 0.12f * noise2(k, 47)); break;
    case SET_ARARAT:
        PUT(PR_ARK, 7, 3.6f, -19, 0.25f, 1); PUT(PR_SHEEP, -5, 0, -3, 1, 1); PUT(PR_SHEEP, -6.5f, 0, -1.2f, 2, 1); PUT(PR_SHEEP, 4, 0, -6, 4, 1); PUT(PR_OLIVE, -9, 0, -8, 0, 0.8f);
        PUTC(PR_CLOUD, -30, 26, -80, 0, 4, 1, 1, 1); PUTC(PR_CLOUD, 35, 30, -90, 1, 5, 1, 1, 1); PUTC(PR_CLOUD, -60, 22, -40, 2, 3.5f, 1, 1, 1); break;
    case SET_EGYPT:
        PUT(PR_PYRAMID, -14, 0, -42, 0.2f, 15.0f / 6.0f); PUT(PR_PYRAMID, 14, 0, -58, -0.1f, 19.0f / 6.0f); PUT(PR_PYRAMID, 32, 0, -38, 0.4f, 9.0f / 6.0f);
        PUT(PR_PALM, -9, 0, -12, 0, 1); PUT(PR_PALM, 8, 0, -14, 1, 1.15f); PUT(PR_PALM, 13, 0, -10, 2, 0.9f); PUT(PR_SACKS, -4.5f, 0, -4, 0.3f, 1); PUT(PR_SACKS, -6, 0, -2.5f, 1.5f, 0.8f);
        PUTC(PR_CLOUD, -50, 30, -100, 0, 4, 1, 1, 1); break;
    case SET_SHORE:
        PUT(PR_PALM, -7, 0, 2, 0, 1); PUT(PR_PALM, 9, 0, 4, 1, 1.1f); PUT(PR_BOULDER, -4, 0, -2.5f, 0, 0.5f); PUT(PR_BOULDER, 3, 0, -3, 1, 0.35f);
        for (int k = 0; k < 7; k++) PUTC(PR_HOUSE, -40 + k * 5.5f, 0.2f, -62, k * 0.7f, 1.4f, 0.9f, 0.85f, 0.8f);   /* Nineveh, far off */
        PUTC(PR_CLOUD, -30, 26, -90, 0, 4, 1, 1, 1); PUTC(PR_CLOUD, 30, 22, -80, 1, 3.5f, 1, 1, 1); break;
    case SET_JORDAN:
        for (int k = 0; k < 6; k++) PUTC(PR_VSTAND, 3 + (k % 3) * 1.1f, 0, -1 + (k / 3) * 1.3f, -0.6f + k * 0.2f, 1, 0.8f + 0.2f * noise2(k, 48), 0.8f, 0.8f + 0.2f * noise2(k, 49));
        PUT(PR_PALM, -9, 0, -2, 0, 1); PUT(PR_OLIVE, 9, 0, -3, 1, 1); PUT(PR_PALM, 12, 0, -16, 2, 1.1f); PUT(PR_SHEEP, -6, 0, 2, 1, 1);
        PUTC(PR_CLOUD, -30, 26, -90, 0, 4, 1, 1, 1); PUTC(PR_CLOUD, 35, 30, -95, 1, 4.5f, 1, 1, 1); break;
    case SET_HEAVEN:
        PUT(PR_GATE, 0, -0.6f, -26, 0, 1.4f);
        for (int k = 0; k < 10; k++) { float a = -0.3f - k * 0.28f; PUTC(PR_PILLAR, cosf(a) * 20, -1, sinf(a) * 20 - 8, k, 1.6f, 1.1f, 0.95f, 0.6f); }
        for (int k = 0; k < 44; k++) { float a = noise2(k, 50) * 6.283f, rr = 4.0f + noise2(k, 51) * 36, sc = 0.45f + noise2(k, 53) * 0.5f; PUT(PR_CLOUD, cosf(a) * rr, -3.6f * sc - 0.4f + noise2(k, 52) * 0.3f, sinf(a) * rr - 6, k, sc); }
        break; }
    return n; }

static void sc_keep_clear(Vec3 *eye, Vec3 at) {   /* slide the camera in toward what it looks at until it is out of every big model */
    Place pl[96]; int set = CUP_STORIES[g_sc_char].set, np = sc_places(set, pl);
    for (int step = 0; step < 20; step++) { int hit = 0;
        for (int i = 0; i < np && !hit; i++) { Place *q = &pl[i]; const Kart3D *k = &g_prop[q->p]; if (!k->ok || q->p == PR_CLOUD || q->p == PR_VSIT || q->p == PR_VSTAND || q->p == PR_SHEEP || q->p == PR_DOVE) continue;
            float r = 0.5f * fmaxf(k->hi.x - k->lo.x, k->hi.z - k->lo.z) * q->s + 0.7f, top = q->y + (k->hi.y - k->lo.y) * q->s + 0.5f, dx = eye->x - q->x, dz = eye->z - q->z;
            if (dx * dx + dz * dz < r * r && eye->y < top) hit = 1; }
        if (set == SET_PALACE && fabsf(eye->x) > 7.6f) hit = 1;   /* the hangings along the walls */
        if (!hit) return; *eye = sc_lerp(*eye, at, 0.12f); } }
/* ---- built shapes ---- */
static void sc_v(Batch *b, Vec3 p, Vec3 n, float u, float v, float r, float g, float bl, float a) { if (b->n < b->cap) { Vertex q = {p.x, p.y, p.z, n.x, n.y, n.z, u, v, r, g, bl, a}; b->v[b->n++] = q; } }
static void sc_quadw(Batch *b, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, float r, float g, float bl, float tile) {   /* a quad whose picture is laid on the ground in world units */
    Vec3 n = face_normal(p0, p1, p2); if (n.y < 0) n = v3(-n.x, -n.y, -n.z); Vec3 p[6] = {p0, p1, p2, p0, p2, p3};
    for (int i = 0; i < 6; i++) sc_v(b, p[i], n, p[i].x / tile, p[i].z / tile, r, g, bl, 1); }
static void sc_quadn(Batch *b, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 n0, Vec3 n1, Vec3 n2, Vec3 n3, float r, float g, float bl, float a) {   /* smooth-shaded */
    sc_v(b, p0, n0, 0.5f, 0.5f, r, g, bl, a); sc_v(b, p1, n1, 0.5f, 0.5f, r, g, bl, a); sc_v(b, p2, n2, 0.5f, 0.5f, r, g, bl, a);
    sc_v(b, p0, n0, 0.5f, 0.5f, r, g, bl, a); sc_v(b, p2, n2, 0.5f, 0.5f, r, g, bl, a); sc_v(b, p3, n3, 0.5f, 0.5f, r, g, bl, a); }
static float sc_hill_y(float x, float z, float cx, float cz, float rad, float h) { float d = sqrtf((x - cx) * (x - cx) + (z - cz) * (z - cz)) / rad; return d >= 1 ? 0 : h * (0.5f + 0.5f * cosf(d * 3.14159f)); }
static void sc_ground(Batch *b, int set, float R) {   /* the ground: a grid near the middle that rises into rolling hills, then rings out to the fog */
    const float *G = SET_LOOK[set].gnd; static const float hl[SET_COUNT][3][4] = {   /* up to three hills: x, z, radius, height */
        {{-30, -40, 30, 9}, {25, -55, 35, 12}, {0, -75, 45, 6}}, {{0}}, {{-40, -20, 32, 10}, {40, -25, 34, 11}, {0, -70, 40, 6}}, {{0}}, {{0}},
        {{0, -45, 45, 8}, {-35, -30, 20, 5}, {0}}, {{0}}, {{0}}, {{8, -22, 20, 4.2f}, {-40, -75, 45, 18}, {45, -85, 50, 22}}, {{0}}, {{0}}, {{-35, -40, 32, 8}, {30, -45, 34, 10}, {0}}, {{0}}};
    const int N = 26; const float S = 4.0f;   /* 104 m square of grid */
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) { Vec3 p[4]; float xs[4] = {0, 0, 1, 1}, zs[4] = {0, 1, 1, 0};
        for (int c = 0; c < 4; c++) { float x = (i + xs[c] - N / 2) * S, z = (j + zs[c] - N + 6) * S, y = 0; for (int h = 0; h < 3; h++) if (hl[set][h][2] > 0) y += sc_hill_y(x, z, hl[set][h][0], hl[set][h][1], hl[set][h][2], hl[set][h][3]);
            y += (noise2((int)(x * 3) + 999, (int)(z * 3)) - 0.5f) * 0.12f * (fabsf(x) > 6 || z < -14 || z > 8); p[c] = v3(x, y, z); }
        float pa = 0.5f * (noise2(i / 3, j / 3 + 70) + noise2((i + 1) / 3, (j + 2) / 3 + 70)), sh = 0.78f + 0.3f * pa + 0.06f * noise2(i, j), dry = noise2(i / 4 + 9, j / 4 + 90) > 0.65f ? 1.15f : 1.0f;   /* patches: darker, lighter, drier */
        sc_quadw(b, p[0], p[1], p[2], p[3], G[0] * sh * dry, G[1] * sh, G[2] * sh / dry, 5); }
    float in = N / 2 * S;   /* past the grid: rings out to the fog, flat */
    for (int q = 0; q < 48; q++) { float a0 = q * 0.1309f, a1 = a0 + 0.1309f; for (int k = 0; k < 2; k++) { float r0 = k ? in * 1.6f : in * 0.99f, r1 = k ? R : in * 1.6f;
        Vec3 p0 = v3(cosf(a0) * r0, -0.05f, sinf(a0) * r0 - 30), p1 = v3(cosf(a0) * r1, -0.05f, sinf(a0) * r1 - 30), p2 = v3(cosf(a1) * r1, -0.05f, sinf(a1) * r1 - 30), p3 = v3(cosf(a1) * r0, -0.05f, sinf(a1) * r0 - 30);
        sc_quadw(b, p0, p1, p2, p3, G[0], G[1], G[2], 5); } } }
static void sc_tube(Batch *b, Vec3 a, Vec3 c, float ra, float rc, int seg, float r, float g, float bl, float al, float ac) {   /* a cylinder or cone from a to c */
    Vec3 d = v3_norm(v3_sub(c, a)), u = fabsf(d.y) < 0.9f ? v3_norm(v3_cross(d, v3(0, 1, 0))) : v3_norm(v3_cross(d, v3(1, 0, 0))), w = v3_cross(d, u);
    for (int i = 0; i < seg; i++) { float t0 = 6.2831853f * i / seg, t1 = 6.2831853f * (i + 1) / seg;
        Vec3 n0 = v3(u.x * cosf(t0) + w.x * sinf(t0), u.y * cosf(t0) + w.y * sinf(t0), u.z * cosf(t0) + w.z * sinf(t0)), n1 = v3(u.x * cosf(t1) + w.x * sinf(t1), u.y * cosf(t1) + w.y * sinf(t1), u.z * cosf(t1) + w.z * sinf(t1));
        Vec3 p0 = v3(a.x + n0.x * ra, a.y + n0.y * ra, a.z + n0.z * ra), p1 = v3(a.x + n1.x * ra, a.y + n1.y * ra, a.z + n1.z * ra), p2 = v3(c.x + n1.x * rc, c.y + n1.y * rc, c.z + n1.z * rc), p3 = v3(c.x + n0.x * rc, c.y + n0.y * rc, c.z + n0.z * rc);
        sc_v(b, p0, n0, 0.5f, 0.5f, r, g, bl, al); sc_v(b, p1, n1, 0.5f, 0.5f, r, g, bl, al); sc_v(b, p2, n1, 0.5f, 0.5f, r, g, bl, ac);
        sc_v(b, p0, n0, 0.5f, 0.5f, r, g, bl, al); sc_v(b, p2, n1, 0.5f, 0.5f, r, g, bl, ac); sc_v(b, p3, n0, 0.5f, 0.5f, r, g, bl, ac); } }
static void sc_ball(Batch *b, Vec3 c, float rad, int seg, float r, float g, float bl, float a) {   /* a sphere */
    for (int i = 0; i < seg / 2; i++) for (int j = 0; j < seg; j++) { float t0 = 3.14159265f * i / (seg / 2), t1 = 3.14159265f * (i + 1) / (seg / 2), p0 = 6.2831853f * j / seg, p1 = 6.2831853f * (j + 1) / seg;
        Vec3 n[4] = {v3(sinf(t0) * cosf(p0), cosf(t0), sinf(t0) * sinf(p0)), v3(sinf(t1) * cosf(p0), cosf(t1), sinf(t1) * sinf(p0)), v3(sinf(t1) * cosf(p1), cosf(t1), sinf(t1) * sinf(p1)), v3(sinf(t0) * cosf(p1), cosf(t0), sinf(t0) * sinf(p1))};
        Vec3 p[4]; for (int k = 0; k < 4; k++) p[k] = v3(c.x + n[k].x * rad, c.y + n[k].y * rad, c.z + n[k].z * rad);
        sc_quadn(b, p[0], p[1], p[2], p[3], n[0], n[1], n[2], n[3], r, g, bl, a); } }
static void sc_octa(Batch *b, Vec3 c, float s, float r, float g, float bl, float a) {   /* a tiny solid spark of light */
    Vec3 P[6] = {v3(c.x + s, c.y, c.z), v3(c.x - s, c.y, c.z), v3(c.x, c.y + s, c.z), v3(c.x, c.y - s, c.z), v3(c.x, c.y, c.z + s), v3(c.x, c.y, c.z - s)};
    static const int F[8][3] = {{0, 2, 4}, {2, 1, 4}, {1, 3, 4}, {3, 0, 4}, {2, 0, 5}, {1, 2, 5}, {3, 1, 5}, {0, 3, 5}};
    for (int f = 0; f < 8; f++) { Vec3 n = face_normal(P[F[f][0]], P[F[f][1]], P[F[f][2]]); for (int k = 0; k < 3; k++) sc_v(b, P[F[f][k]], n, 0.5f, 0.5f, r, g, bl, a); } }
static void sc_bank(Batch *b, float x0, float x1, float z, float rad, float r, float g, float bl, int alongz) {   /* a rounded earth bank beside a river */
    for (int k = 0; k < 20; k++) { float u0 = x0 + (x1 - x0) * k / 20, u1 = x0 + (x1 - x0) * (k + 1) / 20;
        for (int s = 0; s < 6; s++) { float a0 = 3.14159f * s / 6, a1 = 3.14159f * (s + 1) / 6; Vec3 n0 = v3(cosf(a0), sinf(a0), 0), n1 = v3(cosf(a1), sinf(a1), 0);
            Vec3 p[4] = {v3(u0, n0.y * rad * 0.28f, z + n0.x * rad), v3(u1, n0.y * rad * 0.28f, z + n0.x * rad), v3(u1, n1.y * rad * 0.28f, z + n1.x * rad), v3(u0, n1.y * rad * 0.28f, z + n1.x * rad)};
            Vec3 m0 = v3(0, n0.y, n0.x), m1 = v3(0, n1.y, n1.x);
            if (alongz) { for (int c = 0; c < 4; c++) p[c] = v3(p[c].z, p[c].y, p[c].x); m0 = v3(m0.z, m0.y, m0.x); m1 = v3(m1.z, m1.y, m1.x); }
            sc_quadn(b, p[0], p[1], p[2], p[3], m0, m0, m1, m1, r * (0.9f + 0.1f * (s & 1)), g, bl, 1); } } }
static void sc_drape(Batch *b, float x, float z, float face, float w, float h, float r, float g, float bl) {   /* a hanging cloth in deep folds (Esther 1:6) */
    for (int k = 0; k < 8; k++) { float z0 = z - w / 2 + w * k / 8, z1 = z0 + w / 8, d0 = (k & 1) ? 0.35f : 0, d1 = (k & 1) ? 0 : 0.35f;
        Vec3 n = v3_norm(v3(face, 0, (k & 1) ? 0.6f : -0.6f)); float sh = (k & 1) ? 1.0f : 0.75f;
        sc_quadn(b, v3(x + face * d0, 0.2f, z0), v3(x + face * d1, 0.2f, z1), v3(x + face * d1 * 0.4f, h, z1), v3(x + face * d0 * 0.4f, h, z0), n, n, n, n, r * sh, g * sh, bl * sh, 1); }
    sc_tube(b, v3(x, h + 0.1f, z - w / 2 - 0.2f), v3(x, h + 0.1f, z + w / 2 + 0.2f), 0.12f, 0.12f, 8, 0.95f, 0.78f, 0.3f, 1, 1); }
static void sc_build_terrain(Batch *b, int set) {   /* what has a picture laid on it: ground, hills, floors */
    if (set == SET_STORM || set == SET_HEAVEN) return;
    if (set == SET_PALACE || set == SET_TEMPLE) {   /* a paved floor */
        const float *G = SET_LOOK[set].gnd; for (int i = -10; i < 10; i++) for (int j = -24; j < 8; j++) { float x = i * 2.0f, z = j * 2.0f, sh = 0.9f + 0.1f * noise2(i + 50, j);
            sc_quadw(b, v3(x, 0, z), v3(x, 0, z + 2), v3(x + 2, 0, z + 2), v3(x + 2, 0, z), G[0] * sh, G[1] * sh, G[2] * sh, 4); }
        return; }
    sc_ground(b, set, 175); }
static void sc_tufts(Batch *b, int set, int n) {   /* clumps of grass blades standing up out of the ground near the kart */
    const float *G = SET_LOOK[set].gnd;
    for (int k = 0; k < n; k++) { float x = (noise2(k, 201) - 0.5f) * 36, z = (noise2(k, 202) - 0.5f) * 30 - 3; if (fabsf(x) < 2.0f && z > -18 && z < 3) continue;   /* not on the kart's path */
        if (set == SET_JORDAN && z < -5 && z > -13.5f) continue; if (set == SET_ELAH && x > 3.4f && x < 7.6f) continue;
        for (int c = 0; c < 4; c++) { float a = c * 1.57f + noise2(k, c + 203) * 1.2f, h = 0.25f + noise2(k, c + 207) * 0.35f, lean = 0.12f + 0.1f * noise2(k, c + 211), sh = 0.8f + 0.35f * noise2(k, c + 215);
            sc_tube(b, v3(x + cosf(a) * 0.05f, 0, z + sinf(a) * 0.05f), v3(x + cosf(a) * lean, h, z + sinf(a) * lean), 0.035f, 0.004f, 3, G[0] * sh * 0.9f, G[1] * sh * 1.05f, G[2] * sh * 0.8f, 1, 1); } } }
static void sc_build_solid(Batch *b, int set) {   /* the built shapes without a picture */
    if (set == SET_MOUNT || set == SET_ELAH || set == SET_ARARAT || set == SET_JORDAN) sc_tufts(b, set, 260);
    switch (set) {
    case SET_PALACE:
        for (int k = 0; k < 5; k++) for (int s = -1; s <= 1; s += 2) sc_drape(b, s * 8.6f, 6 - k * 5.0f - 2.5f, -s, 4.4f, 9.0f, 0.5f, 0.14f, 0.55f);
        batch_box(b, v3(0, 0.3f, -10), v3(3.2f, 0.3f, 2.2f), 0.9f, 0.76f, 0.4f); batch_box(b, v3(0, 9.4f, -4), v3(9, 0.4f, 14), 0.3f, 0.16f, 0.24f);
        for (int k = 0; k < 6; k++) batch_box(b, v3(0, 8.9f, 8 - k * 5.0f), v3(9, 0.25f, 0.3f), 0.85f, 0.68f, 0.3f);   /* gilded beams */
        for (int k = 0; k < 5; k++) for (int s = -1; s <= 1; s += 2) sc_tube(b, v3(s * 5.3f, 4.2f, 6 - k * 5.0f), v3(s * 5.3f, 4.55f, 6 - k * 5.0f), 0.18f, 0.32f, 10, 0.95f, 0.75f, 0.3f, 1, 1);   /* lamp bowls */
        break;
    case SET_TEMPLE: batch_box(b, v3(0, 11.6f, -12), v3(13, 0.8f, 18), 0.5f, 0.42f, 0.34f); for (int k = 0; k < 7; k++) batch_box(b, v3(0, 10.6f, 4 - k * 5.0f), v3(13, 0.3f, 0.45f), 0.55f, 0.45f, 0.35f); break;
    case SET_ELAH: sc_bank(b, -40, 30, 4.1f, 1.0f, 0.45f, 0.38f, 0.25f, 1); sc_bank(b, -40, 30, 6.9f, 1.0f, 0.45f, 0.38f, 0.25f, 1); break;
    case SET_EGYPT: sc_bank(b, -80, 80, -17.6f, 0.9f, 0.7f, 0.58f, 0.36f, 0); sc_bank(b, -80, 80, -24.4f, 0.9f, 0.7f, 0.58f, 0.36f, 0); break;
    case SET_JORDAN:
        sc_bank(b, -60, 60, -5.8f, 1.3f, 0.42f, 0.42f, 0.25f, 0); sc_bank(b, -60, 60, -12.6f, 1.3f, 0.42f, 0.42f, 0.25f, 0);
        for (int k = 0; k < 60; k++) { float x = -30 + k * 1.0f + noise2(k, 51), z = (k & 1) ? -6.3f - noise2(k, 52) * 0.4f : -12.1f + noise2(k, 52) * 0.4f, h = 1.0f + noise2(k, 53) * 0.9f;
            sc_tube(b, v3(x, 0, z), v3(x + 0.1f, h, z), 0.04f, 0.015f, 4, 0.42f, 0.58f, 0.25f, 1, 1); } break;   /* reeds */
    case SET_SHORE: for (int k = 0; k < 40; k++) { float x = -40 + k * 2.0f; sc_bank(b, x, x + 2, -4.3f, 0.5f, 0.86f, 0.78f, 0.58f, 0); } break;
    case SET_HEAVEN: sc_tube(b, v3(0, -1.6f, -26), v3(0, -0.9f, -26), 9, 8.5f, 32, 1, 0.92f, 0.6f, 1, 1); break;   /* a golden floor before the gate */
    } }
static void sc_build_sky(Batch *b, int set) {   /* a dome round the camera: colour from the top down to the horizon; stars when it is dark */
    const SetLook *L = &SET_LOOK[set]; const float R = 330;
    for (int i = 0; i < 10; i++) for (int j = 0; j < 32; j++) { float e0 = -0.25f + (1.82f) * i / 10, e1 = -0.25f + 1.82f * (i + 1) / 10, a0 = 6.2831853f * j / 32, a1 = 6.2831853f * (j + 1) / 32;
        float c0 = clampf(powf(fmaxf(sinf(e0), 0), 0.55f), 0, 1), c1 = clampf(powf(fmaxf(sinf(e1), 0), 0.55f), 0, 1);
        Vec3 p[4] = {v3(cosf(e0) * cosf(a0) * R, sinf(e0) * R, cosf(e0) * sinf(a0) * R), v3(cosf(e0) * cosf(a1) * R, sinf(e0) * R, cosf(e0) * sinf(a1) * R), v3(cosf(e1) * cosf(a1) * R, sinf(e1) * R, cosf(e1) * sinf(a1) * R), v3(cosf(e1) * cosf(a0) * R, sinf(e1) * R, cosf(e1) * sinf(a0) * R)};
        float k0 = e0 < 0 ? 0.85f : 1, k1 = e1 < 0 ? 0.85f : 1; Vec3 n = v3(0, -1, 0);
        sc_v(b, p[0], n, 0.5f, 0.5f, (L->hor[0] + (L->top[0] - L->hor[0]) * c0) * k0, (L->hor[1] + (L->top[1] - L->hor[1]) * c0) * k0, (L->hor[2] + (L->top[2] - L->hor[2]) * c0) * k0, 1);
        sc_v(b, p[1], n, 0.5f, 0.5f, (L->hor[0] + (L->top[0] - L->hor[0]) * c0) * k0, (L->hor[1] + (L->top[1] - L->hor[1]) * c0) * k0, (L->hor[2] + (L->top[2] - L->hor[2]) * c0) * k0, 1);
        sc_v(b, p[2], n, 0.5f, 0.5f, (L->hor[0] + (L->top[0] - L->hor[0]) * c1) * k1, (L->hor[1] + (L->top[1] - L->hor[1]) * c1) * k1, (L->hor[2] + (L->top[2] - L->hor[2]) * c1) * k1, 1);
        sc_v(b, p[0], n, 0.5f, 0.5f, (L->hor[0] + (L->top[0] - L->hor[0]) * c0) * k0, (L->hor[1] + (L->top[1] - L->hor[1]) * c0) * k0, (L->hor[2] + (L->top[2] - L->hor[2]) * c0) * k0, 1);
        sc_v(b, p[2], n, 0.5f, 0.5f, (L->hor[0] + (L->top[0] - L->hor[0]) * c1) * k1, (L->hor[1] + (L->top[1] - L->hor[1]) * c1) * k1, (L->hor[2] + (L->top[2] - L->hor[2]) * c1) * k1, 1);
        sc_v(b, p[3], n, 0.5f, 0.5f, (L->hor[0] + (L->top[0] - L->hor[0]) * c1) * k1, (L->hor[1] + (L->top[1] - L->hor[1]) * c1) * k1, (L->hor[2] + (L->top[2] - L->hor[2]) * c1) * k1, 1); }
    if (set == SET_NAZARETH || set == SET_LIONS) for (int k = 0; k < 140; k++) { float a = noise2(k, 91) * 6.283f, e = 0.35f + noise2(k, 92) * 1.1f, s = 0.6f + noise2(k, 93) * 0.9f;
        sc_octa(b, v3(cosf(e) * cosf(a) * (R - 10), sinf(e) * (R - 10), cosf(e) * sinf(a) * (R - 10)), s, 1, 1, 0.92f, 1); } }
static Vec3 sc_sun_dir(int set) { return set == SET_REDSEA || set == SET_NAZARETH ? v3_norm(v3(-0.5f, 0.12f, -1)) : set == SET_HEAVEN ? v3_norm(v3(0, 0.25f, -1)) : v3_norm(v3(-0.35f, 0.55f, -1)); }
static void sc_build_rainbow(Batch *b) {   /* the bow in the cloud (Genesis 9:13): seven round bands, a real arch you could walk under */
    static const float rc[7][3] = {{1, 0.18f, 0.18f}, {1, 0.55f, 0.12f}, {1, 0.95f, 0.2f}, {0.2f, 0.85f, 0.3f}, {0.2f, 0.5f, 1}, {0.3f, 0.22f, 0.85f}, {0.6f, 0.28f, 0.85f}};
    for (int band = 0; band < 7; band++) { float rr = 46 - band * 1.5f; for (int q = 0; q < 30; q++) { float a0 = 3.14159f * q / 30, a1 = 3.14159f * (q + 1) / 30;
        sc_tube(b, v3(cosf(a0) * rr, sinf(a0) * rr - 12, -72), v3(cosf(a1) * rr, sinf(a1) * rr - 12, -72), 0.78f, 0.78f, 6, rc[band][0], rc[band][1], rc[band][2], 1, 1); } } }

static Vertex g_scv[3][12000];   /* the moving parts: solid, see-through, glowing */
static void sc_draw_prop(int p, float x, float y, float z, float yaw, float s, float r, float g, float b) {
    if (p < 0 || p >= PR_COUNT || !g_prop[p].ok) return; Mat4 m = m4_mul(m4_translate(x, y, z), m4_mul(m4_rotate_y(yaw), m4_scale(s, s, s))); kart3d_draw(&g_prop[p], &m, 0, 0, r, g, b, 1); }
static void draw_story_scene(int drc) {
    const CupStory *S = &CUP_STORIES[g_sc_char]; int set = S->set; const SetLook *L = &SET_LOOK[set]; Mat4 id = m4_identity(); float t = g_time, fl = set == SET_STORM ? sc_flash() : 0;
    static Mesh *m_sky[SET_COUNT], *m_gnd[SET_COUNT], *m_solid[SET_COUNT], *m_rainbow, *m_ball, *m_cone, *m_dyn[2][3]; static Texture *tx_gnd[SET_COUNT]; static int gtried[SET_COUNT];
    if (!m_dyn[drc][0]) for (int k = 0; k < 3; k++) m_dyn[drc][k] = gfx_mesh_dynamic(12000);
    if (!m_sky[set]) { Batch tb; batch_init(&tb, g_vtmp, 32768); sc_build_sky(&tb, set); m_sky[set] = gfx_mesh_static(tb.v, tb.n);
        batch_clear(&tb); sc_build_terrain(&tb, set); m_gnd[set] = tb.n ? gfx_mesh_static(tb.v, tb.n) : NULL;
        batch_clear(&tb); sc_build_solid(&tb, set); m_solid[set] = tb.n ? gfx_mesh_static(tb.v, tb.n) : NULL; }
    if (!m_ball) { Batch tb; batch_init(&tb, g_vtmp, 32768); sc_ball(&tb, v3(0, 0, 0), 1, 20, 1, 1, 1, 1); m_ball = gfx_mesh_static(tb.v, tb.n);
        batch_clear(&tb); sc_tube(&tb, v3(0, 1, 0), v3(0, 0, 0), 0.45f, 1.0f, 16, 1, 1, 1, 1, 0.0f); m_cone = gfx_mesh_static(tb.v, tb.n);   /* a beam: bright at the top, fading out at the ground */
        batch_clear(&tb); sc_build_rainbow(&tb); m_rainbow = gfx_mesh_static(tb.v, tb.n); }
    if (!gtried[set]) { gtried[set] = 1; static const char *gn[SET_COUNT] = {"eden_grass", "gal_sand", "elah_grass", "jeru_road", NULL, "beth_dirt", "elah_dirt", "jericho_road", "jericho_grass", "gal_sand", "gal_sand", "jeru_grass", NULL};
        if (gn[set]) tx_gnd[set] = asset_texture(gn[set], 1, NULL, NULL); }
    float aspect = drc ? 854.0f / 480.0f : 1280.0f / 720.0f; Vec3 eye = g_sc_eye, at = g_sc_at;
    Mat4 vp = m4_mul(m4_perspective(0.8f, aspect, 0.3f, 400.0f), m4_look_at(eye, at, v3(0, 1, 0)));
    { Mat4 lift = m4_identity(); lift.m[13] = 0.2f; vp = m4_mul(lift, vp); }   /* the picture sits above the panel the words are on */
    float fb = 1 + fl * 1.5f; Vec3 ldir = v3_norm(v3(0.4f, 0.85f, 0.5f));
    gfx_light(v3(L->sun[0] * fb, L->sun[1] * fb, L->sun[2] * fb), v3(L->amb[0] * fb, L->amb[1] * fb, L->amb[2] * fb), v3(L->gnd[0] * 0.5f, L->gnd[1] * 0.5f, L->gnd[2] * 0.5f), eye);
    /* the sky: the dome, the sun, the clouds far off, all without fog, behind everything */
    gfx_clear_depth(drc); gfx_camera(&vp, ldir, 1.0f, 0, 0, 0, 1e6f, 2e6f);
    { Mat4 sm = m4_translate(eye.x, eye.y, eye.z); gfx_state(ST_BLEND); gfx_bind(NULL); float f = 1 + fl * 0.8f; gfx_draw_ex(m_sky[set], &sm, f, f, f, 1, 0, 1, 0, 0);
      if (!L->night) { Vec3 sd = sc_sun_dir(set); float big = set == SET_HEAVEN ? 26 : 13, sr = set == SET_REDSEA || set == SET_NAZARETH ? 1.0f : 1.0f, sg = set == SET_REDSEA || set == SET_NAZARETH ? 0.72f : 0.97f, sbb = set == SET_REDSEA || set == SET_NAZARETH ? 0.45f : 0.85f;
          Mat4 um = m4_mul(m4_translate(eye.x + sd.x * 300, eye.y + sd.y * 300, eye.z + sd.z * 300), m4_scale(big, big, big)); gfx_draw_ex(m_ball, &um, sr, sg, sbb, 1, 0, 1, 0, 0);
          gfx_state(ST_BLEND | ST_ADD); for (int k = 1; k <= 10; k++) { float gs = big * (1 + k * 0.28f); Mat4 gm = m4_mul(m4_translate(eye.x + sd.x * 300, eye.y + sd.y * 300, eye.z + sd.z * 300), m4_scale(gs, gs, gs)); gfx_draw_ex(m_ball, &gm, sr, sg * 0.95f, sbb * 0.8f, 0.035f, 0, 1, 0, 0); } } }   /* a soft glow: many faint shells */
    gfx_clear_depth(drc);
    gfx_camera(&vp, ldir, L->night ? 0.42f : 0.55f, L->hor[0], L->hor[1], L->hor[2], set == SET_PALACE || set == SET_LIONS || set == SET_TEMPLE ? 30.0f : 60.0f, set == SET_PALACE || set == SET_LIONS ? 90.0f : 180.0f);
    /* the ground with its picture and a little relief, then the built shapes */
    gfx_state(ST_DEPTH | ST_DEPTH_WRITE);
    if (m_gnd[set]) { gfx_bind(tx_gnd[set] ? tx_gnd[set] : tx_white); gfx_bump(2); gfx_draw_ex(m_gnd[set], &id, 1, 1, 1, 1, 0.1f, 0, 0, 0); gfx_bump(0); }
    gfx_bind(NULL); if (m_solid[set]) gfx_draw_ex(m_solid[set], &id, 1, 1, 1, 1, 0.35f, 0, 0, 0);
    /* the models: everything in the story */
    Place pl[96]; int np = sc_places(set, pl);
    for (int i = 0; i < np; i++) { Place *q = &pl[i]; float y = q->y; if (set == SET_HEAVEN && q->p == PR_CLOUD) y += 0.15f * sinf(t * 0.4f + i);
        if (set == SET_STORM && q->p == PR_CLOUD) { float c = 1 + fl * 1.2f; sc_draw_prop(q->p, q->x + sinf(t * 0.1f + i) * 2, y, q->z, q->yaw, q->s, q->r * c, q->g * c, q->b * c); continue; }
        sc_draw_prop(q->p, q->x, y, q->z, q->yaw, q->s, q->r, q->g, q->b); }
    if (set == SET_STORM) sc_draw_prop(PR_BOAT, 5.5f, -0.35f + sc_wave(5.5f, -10, t, 0.35f), -10, 0.5f + 0.06f * sinf(t * 0.9f), 1, 0.9f, 0.9f, 0.95f);
    if (set == SET_ARARAT) { float a = t * 0.5f; sc_draw_prop(PR_DOVE, 7 + cosf(a) * 6, 9 + sinf(t * 1.3f) * 0.6f, -19 + sinf(a) * 4, a + 1.57f, 1, 1, 1, 1); }   /* the dove sent out from the ark (Genesis 8:11) */
    gfx_bind(NULL);
    /* the moving solid parts: water */
    Batch db; batch_init(&db, g_scv[0], 12000);
    { float wx0[3], wx1[3], wz0[3], wz1[3], wy[3], wa[3]; int wn[3][2], nw = 0; float wr = set == SET_STORM ? 0.08f : 0.22f, wg = set == SET_STORM ? 0.17f : 0.48f, wb = set == SET_STORM ? 0.26f : 0.68f;
      #define WATER(a, b2, c, d, y, amp, nx, nz) do { wx0[nw] = a; wx1[nw] = b2; wz0[nw] = c; wz1[nw] = d; wy[nw] = y; wa[nw] = amp; wn[nw][0] = nx; wn[nw][1] = nz; nw++; } while (0)
      if (set == SET_STORM) WATER(-60, 60, -90, 20, 0, 0.35f, 36, 30);
      if (set == SET_SHORE) WATER(-60, 60, -90, -4.3f, 0.12f, 0.1f, 36, 26);
      if (set == SET_JORDAN) WATER(-60, 60, -12.6f, -5.8f, 0.08f, 0.04f, 40, 5);
      if (set == SET_EGYPT) WATER(-80, 80, -24.4f, -17.6f, 0.15f, 0.05f, 40, 4);
      if (set == SET_ELAH) WATER(4.1f, 6.9f, -40, 30, 0.08f, 0.03f, 3, 40);
      #undef WATER
      for (int w = 0; w < nw; w++) for (int i = 0; i < wn[w][0]; i++) for (int j = 0; j < wn[w][1]; j++) {
          float ax = wx0[w] + (wx1[w] - wx0[w]) * i / wn[w][0], bx = wx0[w] + (wx1[w] - wx0[w]) * (i + 1) / wn[w][0], az = wz0[w] + (wz1[w] - wz0[w]) * j / wn[w][1], bz = wz0[w] + (wz1[w] - wz0[w]) * (j + 1) / wn[w][1], y0 = wy[w], amp = wa[w];
          float xs[4] = {ax, ax, bx, bx}, zs[4] = {az, bz, bz, az}; Vec3 p[4], nn[4]; float cc[4];
          for (int c = 0; c < 4; c++) { float h = sc_wave(xs[c], zs[c], t, amp), hx = sc_wave(xs[c] + 0.3f, zs[c], t, amp), hz = sc_wave(xs[c], zs[c] + 0.3f, t, amp);
              p[c] = v3(xs[c], y0 + h, zs[c]); nn[c] = v3_norm(v3(-(hx - h) / 0.3f, 1, -(hz - h) / 0.3f)); cc[c] = 0.88f + 0.25f * h / (amp * 2 + 0.01f); }
          Vec3 q[6] = {p[0], p[1], p[2], p[0], p[2], p[3]}, qn[6] = {nn[0], nn[1], nn[2], nn[0], nn[2], nn[3]}; float qc[6] = {cc[0], cc[1], cc[2], cc[0], cc[2], cc[3]};
          for (int c = 0; c < 6; c++) sc_v(&db, q[c], qn[c], 0.5f, 0.5f, wr * qc[c], wg * qc[c], wb * qc[c], 1); } }
    if (db.n) { gfx_mesh_update(m_dyn[drc][0], db.v, db.n); gfx_draw_ex(m_dyn[drc][0], &id, 1, 1, 1, 1, 1.4f, 0, 0, 0); }
    if (set == SET_ELAH && g_gol3d.ok && anim3d_clip(&g_gol3d, "fall") >= 0) { Mat4 gm = m4_mul(m4_translate(-4.5f, 0, -9), m4_rotate_y(1.9f)); anim3d_draw(&g_gol3d, anim3d_clip(&g_gol3d, "fall"), 99.0f, &gm, 1, 1, 1, 1); gfx_bind(NULL); }
    if (set == SET_SHORE && m_whale) {   /* the great fish, at the edge of the sea, mouth open: it has just let Jonah go (Jonah 2:10) */
        float jaw = 0.35f + 0.1f * sinf(t * 0.8f); Mat4 m = m4_mul(m4_translate(6, -0.4f + 0.15f * sinf(t * 0.7f), -14), m4_mul(m4_rotate_y(-2.3f), m4_scale(0.9f, 0.9f, 0.9f)));
        gfx_bind(NULL); gfx_draw_ex(m_whale, &m, 1, 1, 1, 1, 0.8f, 0, 0, 0); Mat4 mj = m4_mul(m, m4_mul(m4_translate(0, -0.55f, -0.4f), m4_rotate_x(jaw))); gfx_draw_ex(m_whale_jaw, &mj, 1, 1, 1, 1, 0.6f, 0, 0, 0); }
    if (set == SET_ELAH && m_pebble) for (int k = 0; k < 5; k++) { Mat4 pm = m4_mul(m4_translate(4.6f + k * 0.4f, 0.12f, 2.0f + k * 0.6f), m4_scale(0.4f, 0.4f, 0.4f)); gfx_draw_ex(m_pebble, &pm, 0.85f, 0.83f, 0.8f, 1, 0.9f, 0, 0, 0); }   /* five smooth stones out of the brook */
    /* the winner's kart: it drives in, stops, and the driver acts out the story */
    float hy = sc_kart_h(), bt = g_sc_bt, ky = 0, kz = 0;
    if (g_sc_beat == SB_TITLE) { float u = clampf(bt / 3.2f, 0, 1); kz = -16.0f * (1 - u) * (1 - u); }
    if (set == SET_STORM) ky = sc_wave(0, kz, t, 0.35f) - 0.15f; else if (set == SET_HEAVEN) ky = 0.25f + 0.12f * sinf(t * 1.4f);
    K3Pose P; memset(&P, 0, sizeof P); P.t = t; Vec3 F = L->focus;
    float fyaw = clampf(atan2f(F.x, F.z), -1.4f, 1.4f), ramp = clampf(bt / 0.8f, 0, 1);
    switch (g_sc_beat) {
    case SB_TITLE: P.look_yaw = bt > 3.2f ? 0.6f * sinf((bt - 3.2f) * 0.9f) : 0; P.bob = 0.02f * sinf(t * 7); break;
    case SB_STORY1: P.look_yaw = 0.35f * sinf(t * 0.6f); P.look_pitch = 0.08f * sinf(t * 1.7f); P.twist = 0.1f * sinf(t * 0.6f); P.lean = 0.04f * sinf(t * 0.8f); break;
    case SB_STORY2: P.look_yaw = fyaw * ramp; P.twist = fyaw * 0.25f * ramp; P.look_pitch = clampf(atan2f(F.y - hy, sqrtf(F.x * F.x + F.z * F.z)), -0.3f, 0.5f) * ramp;
        P.arm[F.x > 0 ? 0 : 1] = K3A_POINT; P.arm_w[F.x > 0 ? 0 : 1] = clampf((bt - 0.8f) / 0.6f, 0, 1) * (bt < 4.5f ? 1 : clampf(5.2f - bt, 0, 1)); break;
    case SB_VERSE: P.look_pitch = 0.42f * ramp; P.bow = -0.08f * ramp; P.arm[0] = P.arm[1] = K3A_UP; P.arm_w[0] = P.arm_w[1] = clampf((bt - 0.4f) / 0.9f, 0, 1); P.bob = 0.015f * sinf(t * 2.0f); break;
    default: { float ang = atan2f(eye.x, eye.z); P.look_yaw = clampf(ang, -1.2f, 1.2f); P.look_pitch = 0.1f; P.arm[1] = K3A_WAVE; P.arm_w[1] = ramp; P.bob = 0.03f * fabsf(sinf(t * 3.0f)); } break; }
    Mat4 km = m4_translate(0, ky, kz); if (set == SET_STORM) km = m4_mul(km, m4_rotate_z(0.05f * sinf(t * 1.1f)));
    if (g_k3d[g_sc_char].ok) { gfx_state(ST_DEPTH | ST_DEPTH_WRITE); kart3d_draw_pose(&g_k3d[g_sc_char], &km, g_sc_roll / 0.33f, 0, &P, 1, 1, 1, 1); }
    /* the guest: Jesus on the water, Jesus coming to the Jordan, Gabriel over Nazareth, Mary looking up */
    Vec3 gp = sc_guest_pos(set); int gu = S->guest; Mat4 gmx = id; K3Pose Q; memset(&Q, 0, sizeof Q); int guest_on = gu >= 0 && gu < NCHARS && gu != g_sc_char && g_k3d[gu].ok;
    float gy = set == SET_STORM ? sc_wave(gp.x, gp.z, t, 0.35f) - 0.1f : (gu == 13 ? 1.6f + 0.15f * sinf(t * 1.6f) : 0), gz = gp.z;
    if (guest_on) {
        if (set == SET_JORDAN && g_sc_beat < SB_STORY2) gz = gp.z - 5.0f + 5.0f * clampf((g_sc_beat == SB_STORY1 ? bt : 0) / 6.0f, 0, 1);   /* John 1:29: seeth Jesus coming unto him */
        gmx = m4_mul(m4_translate(gp.x, gy, gz), m4_rotate_y(atan2f(-gp.x, -gz))); Q.t = t; Q.look_pitch = gu == 5 ? 0.3f : 0.05f;
        if (g_sc_beat >= SB_STORY2) { int sd = gu == 0 ? 0 : 1; Q.arm[sd] = gu == 0 ? K3A_POINT : K3A_UP; Q.arm_w[sd] = g_sc_beat > SB_STORY2 ? 1 : clampf(bt, 0, 1); }
        float gl = gu == 13 ? 1.25f : 1.0f; gfx_state(ST_DEPTH | ST_DEPTH_WRITE); kart3d_draw_pose(&g_k3d[gu], &gmx, 0, 0, &Q, gl, gl, gl, 1); }
    /* shadows on the ground: the kart, the guest, the people and animals (one even shade, as on the roads) */
    if (set != SET_STORM && set != SET_HEAVEN && m_shsquare) {
        float lx = ldir.x, ly = ldir.y, lz = ldir.z; Mat4 sh = m4_identity(); sh.m[4] = -lx / ly; sh.m[5] = 0.0f; sh.m[6] = -lz / ly; sh.m[13] = 0.03f;
        Mat4 pm = m4_mul(m4_translate(0, 0.03f, -4), m4_scale(26, 1, 26)); gfx_bind(NULL);
        gfx_state(ST_AMASK); gfx_draw_ex(m_shsquare, &pm, 0, 0, 0, 1, 0, 1, 0, 0);
        gfx_state(ST_AMASK | ST_DEPTH);
        if (g_k3d[g_sc_char].ok) { Mat4 s1 = m4_mul(sh, km); kart3d_draw_flat(&g_k3d[g_sc_char], &s1, 0, 0, &P, 0, 0, 0, 0.55f); }
        if (guest_on && gy < 0.5f) { Mat4 s2 = m4_mul(sh, gmx); kart3d_draw_flat(&g_k3d[gu], &s2, 0, 0, &Q, 0, 0, 0, 0.55f); }
        for (int i = 0; i < np; i++) { Place *q = &pl[i]; if (q->y > 0.5f || fabsf(q->x) > 24 || q->z < -28 || q->z > 20 || !g_prop[q->p].ok) continue;
            Mat4 m = m4_mul(sh, m4_mul(m4_translate(q->x, 0, q->z), m4_mul(m4_rotate_y(q->yaw), m4_scale(q->s, q->s, q->s)))); kart3d_draw_flat(&g_prop[q->p], &m, 0, 0, NULL, 0, 0, 0, 0.55f); }
        gfx_state(ST_MULDA); gfx_draw_ex(m_shsquare, &pm, 1, 1, 1, 1, 0, 1, 0, 0); }
    if (set == SET_ARARAT) { float a = clampf(g_sc_beat >= SB_STORY2 ? 1 : (g_sc_beat == SB_STORY1 ? g_sc_bt / 4 : 0.2f), 0, 1); gfx_state(ST_DEPTH | ST_BLEND); gfx_bind(NULL); gfx_draw_ex(m_rainbow, &id, 1, 1, 1, 0.55f * a, 0, 1, 0, 0); }
    /* see-through: the walls of water standing up on either side (Exodus 14:22), with rolling white tops */
    batch_init(&db, g_scv[1], 12000);
    if (set == SET_REDSEA) for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 36; k++) {
        float z0 = -80 + k * 3.0f, z1 = z0 + 3.0f, x = s * 6.0f, h0 = 9 + 0.4f * sinf(z0 * 0.4f + t * 1.6f), h1 = 9 + 0.4f * sinf(z1 * 0.4f + t * 1.6f), b0 = 0.5f * sinf(z0 * 0.7f + t * 2.2f), b1 = 0.5f * sinf(z1 * 0.7f + t * 2.2f);
        for (int r = 0; r < 6; r++) { float y0 = 9.0f * r / 6, y1 = 9.0f * (r + 1) / 6, bu0 = b0 * sinf(r * 0.5f), bu1 = b1 * sinf(r * 0.5f), bv0 = b0 * sinf((r + 1) * 0.5f), bv1 = b1 * sinf((r + 1) * 0.5f);
            float ya = r == 5 ? h0 : y1, yb = r == 5 ? h1 : y1; Vec3 n = v3((float)-s, 0, 0);
            float c0 = r / 6.0f, c1 = (r + 1) / 6.0f;   /* deep blue at the foot to bright sea-green at the top */
            sc_v(&db, v3(x + bu0 * s, y0, z0), n, 0.5f, 0.5f, 0.05f + 0.2f * c0, 0.22f + 0.5f * c0, 0.45f + 0.35f * c0, 0.78f); sc_v(&db, v3(x + bu1 * s, y0, z1), n, 0.5f, 0.5f, 0.05f + 0.2f * c0, 0.22f + 0.5f * c0, 0.45f + 0.35f * c0, 0.78f); sc_v(&db, v3(x + bv1 * s, yb, z1), n, 0.5f, 0.5f, 0.05f + 0.2f * c1, 0.22f + 0.5f * c1, 0.45f + 0.35f * c1, 0.78f);
            sc_v(&db, v3(x + bu0 * s, y0, z0), n, 0.5f, 0.5f, 0.05f + 0.2f * c0, 0.22f + 0.5f * c0, 0.45f + 0.35f * c0, 0.78f); sc_v(&db, v3(x + bv1 * s, yb, z1), n, 0.5f, 0.5f, 0.05f + 0.2f * c1, 0.22f + 0.5f * c1, 0.45f + 0.35f * c1, 0.78f); sc_v(&db, v3(x + bv0 * s, ya, z0), n, 0.5f, 0.5f, 0.05f + 0.2f * c1, 0.22f + 0.5f * c1, 0.45f + 0.35f * c1, 0.78f); }
        sc_quadn(&db, v3(x + b0 * s, h0, z0), v3(x + b1 * s, h1, z1), v3(x + s * 30, h1 + 0.3f, z1), v3(x + s * 30, h0 + 0.3f, z0), v3(0, 1, 0), v3(0, 1, 0), v3(0, 1, 0), v3(0, 1, 0), 0.12f, 0.35f, 0.6f, 0.92f);
        sc_tube(&db, v3(x + b0 * s, h0, z0), v3(x + b1 * s, h1, z1), 0.45f, 0.45f, 6, 0.92f, 0.96f, 1, 0.9f, 0.9f); }   /* the foaming crest */
    if (set == SET_STORM) for (int k = 0; k < 160; k++) { float h1 = noise2(k, 61), x = eye.x + (h1 - 0.5f) * 30, z = eye.z - 2 - noise2(k, 62) * 24, y = 14 - fmodf(t * 16 + noise2(k, 63) * 14, 14);   /* rain */
        sc_tube(&db, v3(x, y, z), v3(x + 0.15f, y - 0.9f, z), 0.025f, 0.01f, 3, 0.7f, 0.8f, 1, 0.45f, 0.1f); }
    if (db.n) { gfx_mesh_update(m_dyn[drc][1], db.v, db.n); gfx_state(ST_TRANSPARENT); gfx_bind(NULL); gfx_draw_ex(m_dyn[drc][1], &id, 1, 1, 1, 1, 1.2f, set == SET_STORM || set == SET_REDSEA, 0, 0); }
    /* light: beams from above as cones, sparks, the pillar of fire, the dove, halos */
    gfx_state(ST_TRANSPARENT | ST_ADD); gfx_bind(NULL);
    int rays = set == SET_MOUNT || set == SET_NAZARETH || set == SET_LIONS || set == SET_TEMPLE || set == SET_ARARAT || set == SET_HEAVEN || set == SET_JORDAN;
    float rayv = g_sc_beat == SB_VERSE ? 1.6f : 1.0f;
    if (rays) for (int k = 0; k < 5; k++) { float h1 = noise2(k, 71), x = (h1 - 0.5f) * (set == SET_LIONS ? 2 : 14), z = -3 - noise2(k, 72) * 6, a = 0.07f * rayv * (0.6f + 0.4f * sinf(t * (0.5f + h1) + k)), w = 1.0f + h1;
        Mat4 m = m4_mul(m4_translate(x, 0, z + (set == SET_LIONS ? -1 : 0)), m4_mul(m4_rotate_z(-0.18f), m4_mul(m4_rotate_x(0.12f), m4_scale(w, 20, w)))); gfx_draw_ex(m_cone, &m, 1, 0.92f, 0.7f, a, 0, 1, 0, 0); }
    if (g_sc_beat == SB_VERSE) { float a = clampf(bt / 1.2f, 0, 1); Mat4 m = m4_mul(m4_translate(0, 0, kz), m4_scale(1.9f, 18, 1.9f)); gfx_draw_ex(m_cone, &m, 1, 0.9f, 0.6f, 0.3f * a, 0, 1, 0, 0); }   /* light on them while the verse is read */
    if (set == SET_REDSEA) for (int k = 0; k < 70; k++) { float h1 = noise2(k, 41), life = fmodf(t * (0.35f + h1 * 0.25f) + h1, 1.0f), y = life * 16, sw = 0.9f * (1 - life * 0.5f);   /* the pillar of fire (Exodus 14:19-20): balls of flame boiling upward */
        float s = (1.7f - life * 1.0f) * (0.7f + 0.5f * noise2(k, 42)), a2 = h1 * 6.283f + t * 1.5f;
        Mat4 m = m4_mul(m4_translate(cosf(a2) * sw, y, -18 + sinf(a2) * sw), m4_scale(s, s * 1.3f, s)); gfx_draw_ex(m_ball, &m, 1, 0.75f - life * 0.5f, 0.25f - life * 0.2f, 0.28f * sinf(life * 3.14159f) + 0.02f, 0, 1, 0, 0); }
    if (gu == 13 || set == SET_LIONS || (set == SET_STORM && gu == 0) || set == SET_JORDAN) {   /* the angel's light; the light round Jesus in the dark; the light where the dove comes down */
        Vec3 c = set == SET_LIONS ? v3(0, 3.0f, -5) : v3(gp.x, (gu == 13 ? 1.6f : 0.2f) + 1.2f, gz); float s = 1.6f + 0.15f * sinf(t * 2);
        for (int k = 1; k <= 8; k++) { float r2 = s * (0.35f + k * 0.17f); Mat4 m = m4_mul(m4_translate(c.x, c.y, c.z), m4_scale(r2, r2, r2)); gfx_draw_ex(m_ball, &m, 1, 0.92f, 0.7f, set == SET_LIONS ? 0.022f : 0.03f, 0, 1, 0, 0); } }
    batch_init(&db, g_scv[2], 12000);
    for (int k = 0; k < 44; k++) { float h1 = noise2(k, 81), ph = fmodf(t * (0.12f + h1 * 0.1f) + h1, 1.0f), a = h1 * 6.283f + t * 0.2f, rr = 2.5f + noise2(k, 82) * 6, s = 0.04f + noise2(k, 83) * 0.05f;
        sc_octa(&db, v3(cosf(a) * rr, ph * 7, sinf(a) * rr - 1), s, 1, 0.9f, 0.6f, sinf(ph * 3.14159f) * (L->night ? 0.9f : 0.6f)); }
    if (set == SET_PALACE) for (int k = 0; k < 10; k++) { float x = (k & 1 ? 5.3f : -5.3f), z = 6 - (k / 2) * 5.0f, s = 0.22f + 0.04f * sinf(t * 11 + k);   /* the flames in the lamp bowls */
        sc_tube(&db, v3(x, 4.5f, z), v3(x + 0.05f * sinf(t * 6 + k), 5.2f + 0.1f * sinf(t * 9 + k), z), s, 0.0f, 8, 1, 0.65f, 0.2f, 0.9f, 0.2f); }
    if (set == SET_PALACE && g_sc_beat >= SB_STORY2) { float a = clampf(bt / 1.0f, 0, 1) * (0.6f + 0.4f * sinf(t * 3)); sc_ball(&db, v3(1.3f, 2.6f, -9.6f), 0.35f, 10, 1, 0.85f, 0.3f, a); }   /* the golden sceptre shines (Esther 5:2) */
    if (db.n) { gfx_mesh_update(m_dyn[drc][2], db.v, db.n); gfx_draw_ex(m_dyn[drc][2], &id, 1, 1, 1, 1, 0, 1, 0, 0); }
    if (set == SET_JORDAN && g_sc_beat >= SB_STORY2) { float u = clampf(bt / 4.0f, 0, 1), dy = g_sc_beat == SB_STORY2 ? 12 - 8.5f * sc_smooth(u) : 3.5f;   /* the Spirit like a dove, coming down on Him (Matthew 3:16) */
        gfx_state(ST_DEPTH | ST_DEPTH_WRITE); sc_draw_prop(PR_DOVE, gp.x, dy + 0.15f * sinf(t * 2), gz, 3.14159f + 0.3f * sinf(t * 0.7f), 1, 1.2f, 1.2f, 1.2f); }
    gfx_bind(NULL); gfx_camera_2d(); gfx_state(ST_HUD); }

/* the words over the scene: film bars, the title, then each line typing itself out in the panel at the bottom */
static int sc_wrap(const char *s, float size, float maxw, char out[5][140]) {
    int n = 0, len = (int)strlen(s), i = 0; char line[140]; line[0] = 0;
    while (i < len && n < 5) { int j = i; while (j < len && s[j] != ' ') j++; char word[140]; int wl = j - i < 139 ? j - i : 139; memcpy(word, s + i, (size_t)wl); word[wl] = 0;
        char trial[300]; snprintf(trial, sizeof trial, line[0] ? "%s %s" : "%s%s", line, word);
        if (line[0] && text_width(trial, size) > maxw) { snprintf(out[n++], 140, "%s", line); snprintf(line, sizeof line, "%s", word); } else snprintf(line, sizeof line, "%s", trial);
        i = j + 1; }
    if (line[0] && n < 5) snprintf(out[n++], 140, "%s", line);
    return n; }
static void hud_scene_top(void) {
    const CupStory *S = &CUP_STORIES[g_sc_char]; float bt = g_sc_bt, st = g_screen_t, bars = sc_smooth(st / 0.9f);
    if (S->set == SET_STORM) { float f = sc_flash(); if (f > 0) batch_rect(&g_b, 0, 0, 1280, 720, 0.8f, 0.85f, 1, 0.22f * f); }
    static const UiLook UI_SCENE = {{0.17f, 0.14f, 0.24f}, {0.05f, 0.04f, 0.09f}, {0.95f, 0.76f, 0.32f}, {0.36f, 0.25f, 0.1f}};
    ui_plaque(-30, -40 - (1 - bars) * 70, 1340, 98, 16, 20, &UI_SCENE, 0.96f, 0, -1);   /* the bar along the top, a solid slab with a gold edge */
    float py = 720 - 176 * bars;
    ui_plaque(14, py, 1252, 720 - py + 30, 22, 26, &UI_SCENE, 0.95f, 0.25f, -1);         /* the panel the words stand on */
    { char buf[64]; snprintf(buf, sizeof buf, "%s - CHAMPION", CHARACTERS[g_sc_char].name); for (char *c = buf; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
      text3d(buf, 26, 18, 18, 1, 0.85f, 0.4f, bars, 1); }
    for (int b = 0; b < SB_COUNT; b++) { float s = b == g_sc_beat ? 9 + 1.5f * sinf(g_time * 5) : 7;   /* where we are in the story: little gold studs */
        ui_plaque(1150 + b * 24 - s, 27 - s, s * 2, s * 2, s * 0.9f, 6, b <= g_sc_beat ? &UI_GOLD : &UI_SCENE, bars, b == g_sc_beat ? 0.6f : 0, -1); }
    if (g_sc_beat == SB_TITLE) {
        if (bt > 1.0f) { fx_words(S->title, 640, 232, strlen(S->title) > 22 ? 40 : 48, bt - 1.0f, 99, 1, 1, 1, 1, 1); fx_line(S->place, 640, 300, 24, bt - 1.3f, 99, 1, 1, 1, 1); }
        if (bt > 2.0f) { const char *cup = g_cup == 0 ? "A STORY FOR THE WINNER OF THE GOSPEL CUP" : "A STORY FOR THE WINNER OF THE GENESIS TO GLORY CUP"; fx_line(cup, 640, py + 70, 20, bt - 2.0f, 99, 0.85f, 0.9f, 1, 1); } }
    else {
        const char *hdr = g_sc_beat == SB_VERSE ? "THE BIBLE SAYS" : g_sc_beat == SB_LESSON ? "WHAT THIS TEACHES US" : S->title; float ha = clampf(bt * 3, 0, 1);
        text3d(hdr, 44 + (1 - ha) * 30, py + 18, 20, 1, 0.82f, 0.38f, ha, 1);
        char lines[5][140]; float sz = g_sc_beat == SB_VERSE ? 26 : 24; int n = sc_wrap(sc_text(g_sc_beat), sz, 1180, lines), left = (int)((bt - 0.25f) * SC_TYPE);
        float lh = sz + 8, y0 = py + 50 + (n < 3 ? (3 - n) * lh * 0.3f : 0);
        float cr = g_sc_beat == SB_VERSE ? 1.0f : 1, cg = g_sc_beat == SB_VERSE ? 0.9f : 1, cb = g_sc_beat == SB_VERSE ? 0.6f : 1;
        for (int i = 0; i < n && left > 0; i++) { char part[140]; int l = (int)strlen(lines[i]), sh = left < l ? left : l; memcpy(part, lines[i], (size_t)sh); part[sh] = 0; left -= l + 1;
            float x = 640 - text_width(lines[i], sz) / 2; text3d(part, x, y0 + i * lh, sz, cr, cg, cb, 1, 0); }
        float done = bt - sc_typed_t(g_sc_beat);
        if (g_sc_beat == SB_VERSE && done > 0) { char rb[48]; snprintf(rb, sizeof rb, "- %s", S->ref); float a = clampf(done * 3, 0, 1); text3d(rb, 1236 - text_width(rb, 20) - (1 - a) * 30, 686, 20, 1, 0.82f, 0.38f, a, 1); } }
    { int last = g_sc_beat == SB_COUNT - 1; float done = bt - sc_typed_t(g_sc_beat);   /* the A prompt, once the words are all out */
      if (done > 0.3f && g_sc_out <= 0) { const char *p = last ? "A: CONTINUE" : "A: NEXT"; float a = 0.55f + 0.45f * sinf(g_time * 4); text3d(p, (g_sc_beat == SB_VERSE ? 44 : 1236 - text_width(p, 18)), 688, 18, 0.9f, 0.95f, 1, a, 0); } }
    float fade = st < 0.8f ? 1 - st / 0.8f : 0; if (g_sc_out > 0) fade = clampf(g_sc_out / 0.6f, 0, 1);
    if (fade > 0) batch_rect(&g_b, 0, 0, 1280, 720, 0, 0, 0, fade); }
#ifdef KK_TOUR
static void scene_check_words(void) {   /* the CI log: every story's lines fit the panel */
    int worst = 0, who = -1; for (int c = 0; c < NCHARS; c++) { int keep = g_sc_char; g_sc_char = c;
        for (int b = SB_STORY1; b < SB_COUNT; b++) { char l[5][140]; int n = sc_wrap(sc_text(b), b == SB_VERSE ? 26 : 24, 1180, l); if (n > worst) { worst = n; who = c; } }
        if (text_width(CUP_STORIES[c].title, strlen(CUP_STORIES[c].title) > 22 ? 40 : 48) > 1240) gfx_log("cup story title too wide: %s", CUP_STORIES[c].title);
        g_sc_char = keep; }
    gfx_log("cup story words: at most %d lines (%s)%s", worst, who >= 0 ? CHARACTERS[who].name : "-", worst > 4 ? " TOO MANY" : ""); }
#endif

// ------------------------------------------------------------------ per-frame update
#ifdef KK_TOUR
/* CI tour: the game presses its own buttons on a timer so the emulator test can photograph every screen */
static void tour_pads(Pad *pads, float t) {
    static const struct { float t; char key; int slot; } script[] = {
#if KK_TOUR_ROAD == 11
        /* the Grand Prix: title, GRAND PRIX, the second cup (Genesis to Glory), character, kart, the first road (Eden), then the standings */
        {8, 'A'}, {11, 'A'}, {14, 'R'}, {16, 'A'}, {19, 'A'}, {22, 'A'}, {70, 'P'}, {84, 'P'}, {130, 'A'}, {133, 'A'},
#elif KK_TOUR_ROAD >= 13
        {900, 'A'},                              /* the Bible stories run by themselves: no buttons */
#elif KK_TOUR_ROAD == 12
        {20, 'A'},                               /* the trophy ceremony plays out, then A: the winner's Bible story, which then runs through the stories by itself */
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
        {8, 'A'}, {11, 'D'}, {13, KK_TOUR_ROAD == 5 ? 'D' : 0}, {15, KK_TOUR_ROAD == 5 ? 'D' : 0}, {18, KK_TOUR_ROAD == 5 ? 'A' : 0}, {14, KK_TOUR_ROAD == 5 ? 0 : 'A'}, {17, (KK_TOUR_ROAD == 5 || KK_TOUR_ROAD == 0) ? 0 : 'R'}, {22, KK_TOUR_ROAD == 5 ? 0 : 'A'}, {26, KK_TOUR_ROAD == 5 ? 0 : 'A'},
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
static void begin_battle(void);
static void begin_mode_alone(void) {   /* the single-player start of the chosen mode */
    g_multi = 0; g_screen_t = 0;
    if (g_mode == MODE_BATTLE) { g_player_char = 2; begin_battle(); }
    else if (g_mode == MODE_VS) { g_mode = MODE_SINGLE; g_screen = SCR_CHAR; }
    else g_screen = g_mode == MODE_GP ? SCR_CUP : SCR_CHAR; }
static void begin_battle(void) {
    set_track(ARENA_TRACK); g_attract = 0;
    if (g_multi && g_njoined >= 2) { battle_init_multi(&g_battle, &g_track, g_difficulty, g_njoined, g_pchar); g_nplayers = g_njoined; }   /* together: every player is a David */
    else { battle_init(&g_battle, &g_track, g_difficulty); g_nplayers = 1; }
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
    if (g_screen == SCR_SCENE) return music_available("music_scene") ? "music_scene" : (music_available("music_credits") ? "music_credits" : "music_menu");   /* Sean can drop in content/music/music_scene.mp3 */
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
            for (int i = 0; i < g_nplaced; i++) { Placed *pl = &g_placed[i]; if (pl->kind != kd || pl->solid || pl->m3) continue;
                float dx = pl->p.x - cam_x, dz = pl->p.z - cam_z; float sdd = g_nplayers >= 3 ? SPRITE_DRAW_DIST * 0.6f : (g_nplayers == 2 ? SPRITE_DRAW_DIST * 0.8f : SPRITE_DRAW_DIST); if (dx * dx + dz * dz > sdd * sdd) continue;
                float w = pl->w * 0.5f; int scroll = pl->falls && g_kind[kd].placeholder;
                float v0 = scroll ? g_time * 1.5f : 0.0f, v1 = v0 + (scroll ? pl->h / 3.0f : 1.0f);
                float sw = g_kind[kd].sway == 1 ? sinf(g_time * 0.9f + i * 1.3f) * 0.025f * pl->h : (g_kind[kd].sway == 2 ? sinf(g_time * 3.0f + i * 2.1f) * 0.03f * pl->h : 0);   /* the top of trees and cloth moves in the wind */
                float hh = pl->h;
                Vec3 c = v3(pl->p.x, pl->p.y + hh * 0.5f, pl->p.z), r_ = v3(ax.x * w, 0, ax.z * w), u_ = v3(ax.x * sw, hh * 0.5f, ax.z * sw);
                if (pl->wall && pl->fall_t > 0) {   /* "the wall fell down flat": it shudders, then topples over away from the road, faster and faster, and lies there */
                    float f = clampf((pl->fall_t - 0.15f) / 0.95f, 0, 1), ang = 1.5f * f * f, shud = pl->fall_t < 0.3f ? sinf(pl->fall_t * 60.0f) * 0.05f * pl->h : 0;
                    TrackHit wh; track_locate(&g_track, pl->p.x, pl->p.z, -1, &wh); const TrackSample *ws = &g_track.s[wh.idx]; float sgn = wh.lateral > 0 ? 1.0f : -1.0f;
                    Vec3 out = v3(ws->nx * sgn, 0, ws->nz * sgn);   /* away from the road */
                    float ca = cosf(ang), sa = sinf(ang); hh = pl->h * 0.5f; r_ = v3(ws->tx * w, 0, ws->tz * w);   /* lying along the road, not facing the camera, so it never shrinks to a sliver */
                    u_ = v3(out.x * sa * hh + ax.x * shud, ca * hh, out.z * sa * hh + ax.z * shud);
                    c = v3(pl->p.x + u_.x, pl->p.y + 0.15f + u_.y, pl->p.z + u_.z); }
                if (g_kind[kd].sway == 1 && !pl->wall) {   /* trees and plants: two crossed pictures fixed in the ground, so they have depth as you pass, not a card turning to face you */
                    for (int q = 0; q < 2; q++) { float a = pl->yaw + q * 1.5707963f; Vec3 rr = v3(cosf(a) * w, 0, sinf(a) * w), uu = v3(cosf(a) * sw, hh * 0.5f, sinf(a) * sw);
                        batch_quad4(&g_b, v3(c.x - rr.x - uu.x, c.y - uu.y, c.z - rr.z - uu.z), v3(c.x + rr.x - uu.x, c.y - uu.y, c.z + rr.z - uu.z), v3(c.x + rr.x + uu.x, c.y + uu.y, c.z + rr.z + uu.z), v3(c.x - rr.x + uu.x, c.y + uu.y, c.z - rr.z + uu.z), v3(0, 1, 0), 0, v1, 1, v0, 1, 1, 1, 1); }
                    continue; }
                batch_quad4(&g_b, v3(c.x - r_.x - u_.x, c.y - u_.y, c.z - r_.z - u_.z), v3(c.x + r_.x - u_.x, c.y - u_.y, c.z + r_.z - u_.z), v3(c.x + r_.x + u_.x, c.y + u_.y, c.z + r_.z + u_.z), v3(c.x - r_.x + u_.x, c.y + u_.y, c.z - r_.z + u_.z), v3(0, 1, 0), 0, v0, 1, v1, 1, 1, 1, 1); }
            for (int m = 0; m < g_track.nmover; m++) { if (g_mover_kind[m] != kd || model_known(g_track.mover[m].img)) continue; const Mover *mv = &g_track.mover[m];
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
            for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; if (r->swallow_t[i] > 0) continue; float dx = q->x - cam_x, dz = q->z - cam_z; if (dx * dx + dz * dz > 80.0f * 80.0f) continue;
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
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; if (r->swallow_t[i] > 0) continue;   /* inside the great fish */ TrackHit h2; track_locate(&g_track, q->x, q->z, q->hit.idx, &h2);
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
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; if (r->swallow_t[i] > 0) continue;   /* inside the great fish */
            if (q->carried_t > 0) { float sz = 2.6f + 0.2f * sinf(g_time * 5.0f);   /* a pillar of light carries the kart */
                batch_quad(&g_b, v3(q->x, q->y + 2.2f, q->z), v3(ax.x * sz * 0.6f, 0, ax.z * sz * 0.6f), v3(0, sz * 1.6f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1.0f, 0.97f, 0.8f, 0.5f); }
            if (q->shield > 0 && q->shield < 8.5f && q->shield > 7.0f) { float sz = 2.4f; batch_quad(&g_b, v3(q->x, q->y + 1.0f, q->z), v3(ax.x * sz, 0, ax.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.6f, 0.85f, 1.0f, 0.35f); } }
        for (int i = 0; i < MAX_RING; i++) if (g_ring[i].t > 0) { Ring *rg = &g_ring[i]; float u = 1.0f - rg->t / 0.6f, sz = 0.5f + u * 5.0f;
            batch_quad(&g_b, v3(rg->p.x, rg->p.y + 0.8f, rg->p.z), v3(ax.x * sz, 0, ax.z * sz), v3(0, sz, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.6f + 0.4f * sinf(u * 6), 0.6f + 0.4f * sinf(u * 6 + 2), 0.6f + 0.4f * sinf(u * 6 + 4), 0.7f * (1 - u)); }
        gfx_mesh_update(m_items, g_b.v, g_b.n);
    }
}
/* the 3D world as seen from the view cv, for the player driving kart vk; aspect is the viewport's width / height, fov_mul trims the lens for a wide half-screen view */
static Mat4 kart_mat(const Race *r, int i) {   /* where a kart is drawn: on the road's slope and bank, leaning into turns, squashing on landings */
    const Kart *q = &r->k[i];
    float pitch = -atan2f(q->hit.slope, 1.0f) * (q->air ? 0.3f : 1.0f) + (q->air ? -q->vy * 0.03f : 0.0f);
    float roll = q->hit.on_road ? -g_track.s[q->hit.idx].bank : 0.0f, sq = 1.0f - q->squash * 0.25f;
    return m4_mul(m4_translate(q->x, q->y + q->hop * 0.4f + cele_bounce(r, i), q->z), m4_mul(m4_rotate_y(q->yaw + q->slide + q->spin_vis), m4_mul(m4_rotate_x(pitch), m4_mul(m4_rotate_z(roll + q->steer_vis * -0.06f), m4_scale(1.0f + (1 - sq) * 0.5f, sq, 1.0f))))); }
/* ---- the drivers come alive (Sean: "move their heads, arms... so it looks like they are interacting with one another"):
   heads turn to the racer alongside or behind, bodies lean into the turns, a wave for the one you just passed or who just
   blessed you, a guard when hit, a cheer at the finish. Drawn by kart3d_draw_pose for karts whose driver moves. ---- */
enum { GEST_NONE, GEST_WAVE, GEST_CHEER, GEST_PUMP, GEST_GUARD, GEST_POINT };
typedef struct { K3Pose p; int gest, side, look_at, last_rank; float gest_t, gest_len, look_hold, wave_cd, shake, idle_cd; } DrvState;
static DrvState g_drv[NKARTS];
static void drv_gesture(int i, int g, int side, float len) {   /* side: 0 left arm, 1 right arm, 2 both */
    if (i < 0 || i >= NKARTS) return; DrvState *d = &g_drv[i];
    if (d->gest == GEST_GUARD && g != GEST_GUARD && d->gest_t < d->gest_len) return;   /* a hit outranks a wave */
    d->gest = g; d->side = side; d->gest_t = 0; d->gest_len = len; }
static void drivers_reset(void) { memset(g_drv, 0, sizeof g_drv); for (int i = 0; i < NKARTS; i++) { g_drv[i].look_at = -1; g_drv[i].idle_cd = 0.5f + 0.4f * i; } }
static void drv_look(int i, int at, float hold) { if (i >= 0 && i < NKARTS) { g_drv[i].look_at = at; g_drv[i].look_hold = hold; } }
static float drv_side_of(const Race *r, int i, int j, float *ang) {   /* where kart j is, seen from kart i's seat: + = to the left */
    const Kart *a = &r->k[i], *b = &r->k[j]; float dx = b->x - a->x, dz = b->z - a->z, yw = a->yaw + a->slide + a->spin_vis, c = cosf(yw), s = sinf(yw);
    float lx = c * dx - s * dz, lz = s * dx + c * dz; if (ang) *ang = atan2f(lx, lz); return lx; }
static void drv_event(const Race *r, const RaceEvent *ev) {
    int i = ev->kart; if (i < 0 || i >= r->nk) return;
    switch (ev->type) {
    case EV_HIT: drv_gesture(i, GEST_GUARD, 2, 1.1f); g_drv[i].shake = 1.0f; break;
    case EV_BLOCK: drv_gesture(i, GEST_PUMP, 1, 1.2f); break;
    case EV_BUMP: g_drv[i].shake = fmaxf(g_drv[i].shake, 0.5f); { int best = -1; float bd = 36; for (int j = 0; j < r->nk; j++) if (j != i) { float dx = r->k[j].x - r->k[i].x, dz = r->k[j].z - r->k[i].z, dd = dx * dx + dz * dz; if (dd < bd) { bd = dd; best = j; } } if (best >= 0) drv_look(i, best, 1.4f); } break;
    case EV_BLESSED: { int by = r->blessed_by[i]; if (by >= 0 && by < r->nk && by != i) {   /* thank the giver, and the giver waves back */
        float a; int sd = drv_side_of(r, i, by, &a) > 0 ? 0 : 1; drv_look(i, by, 2.0f); drv_gesture(i, GEST_WAVE, sd, 1.8f);
        sd = drv_side_of(r, by, i, &a) > 0 ? 0 : 1; drv_look(by, i, 2.0f); if (g_drv[by].gest != GEST_GUARD) drv_gesture(by, GEST_WAVE, sd, 1.5f); } } break;
    case EV_USE_LOAVES: case EV_USE_JUBILEE: drv_gesture(i, GEST_CHEER, 2, 1.3f); break;
    case EV_USE_FIRE: case EV_USE_PARTING: case EV_USE_MANNA: case EV_USE_WATER: drv_gesture(i, GEST_POINT, 1, 0.9f); break;
    case EV_TRICK: drv_gesture(i, GEST_CHEER, 2, 0.7f); break;
    case EV_TRICK_LAND: case EV_ROCKET_START: case EV_PAD_BOOST: if (g_drv[i].gest == GEST_NONE) drv_gesture(i, GEST_PUMP, (i & 1), 0.9f); break;
    case EV_FINISH: drv_gesture(i, r->k[i].place <= 3 ? GEST_CHEER : GEST_WAVE, r->k[i].place <= 3 ? 2 : 1, r->k[i].place <= 3 ? 3.5f : 2.5f); break;
    default: break; } }
static void drivers_update(const Race *r, float dt) {
    int rank[NKARTS]; for (int i = 0; i < NKARTS; i++) rank[i] = i; for (int n = 0; n < r->nk; n++) if (r->order[n] >= 0 && r->order[n] < NKARTS) rank[r->order[n]] = n;
    for (int i = 0; i < r->nk && i < NKARTS; i++) { const Kart *q = &r->k[i]; DrvState *d = &g_drv[i]; K3Pose *p = &d->p; p->t += dt;
        float h = noise2(i * 31 + 7, 5);
        /* passing someone close: a friendly wave as you go by */
        if (r->state == RACE_RUNNING && !q->finished) {
            if (d->last_rank > 0 && rank[i] < d->last_rank && d->wave_cd <= 0 && q->stun <= 0) {
                int passed = -1; for (int j = 0; j < r->nk; j++) if (rank[j] == rank[i] + 1) passed = j;
                if (passed >= 0) { float dx = r->k[passed].x - q->x, dz = r->k[passed].z - q->z;
                    if (dx * dx + dz * dz < 18 * 18 && (h > 0.35f || r->human[i])) { float a; int sd = drv_side_of(r, i, passed, &a) > 0 ? 0 : 1;
                        drv_look(i, passed, 1.6f); drv_gesture(i, GEST_WAVE, sd, 1.4f); d->wave_cd = 5.0f + 4.0f * h;
                        drv_look(passed, i, 1.4f); } } }
            d->last_rank = rank[i]; }
        else d->last_rank = rank[i];
        if (d->wave_cd > 0) d->wave_cd -= dt;
        /* before the start and after the finish: look about and wave to the crowd now and then */
        if ((r->state == RACE_COUNTDOWN || q->finished) && d->gest == GEST_NONE) { d->idle_cd -= dt;
            if (d->idle_cd <= 0) { int tk = (int)(p->t * 10.0f); d->idle_cd = 2.5f + 4.0f * noise2(i, tk); float u = noise2(tk, i * 7 + 3);
                if (u < 0.35f) drv_gesture(i, GEST_WAVE, u < 0.17f ? 0 : 1, 1.6f); else if (q->finished && u < 0.5f && q->place <= 3) drv_gesture(i, GEST_PUMP, 1, 1.4f);
                else { int j = (int)(noise2(i + 11, tk) * r->nk) % (r->nk > 0 ? r->nk : 1); if (j != i) drv_look(i, j, 2.0f); } } }
        if (q->stun > 0 && d->gest != GEST_GUARD) drv_gesture(i, GEST_GUARD, 2, q->stun + 0.2f);
        /* the gesture: arms out to it and back to the wheel */
        float tw[2] = { 0, 0 }; int ta[2] = { K3A_WHEEL, K3A_WHEEL };
        if (d->gest != GEST_NONE) { d->gest_t += dt; if (d->gest_t >= d->gest_len) d->gest = GEST_NONE;
            else { int a = d->gest == GEST_WAVE ? K3A_WAVE : d->gest == GEST_CHEER ? K3A_UP : d->gest == GEST_PUMP ? K3A_PUMP : d->gest == GEST_GUARD ? K3A_GUARD : K3A_POINT;
                for (int sd = 0; sd < 2; sd++) if (d->side == 2 || d->side == sd) { ta[sd] = a; tw[sd] = 1; } } }
        for (int sd = 0; sd < 2; sd++) { if (tw[sd] > 0) p->arm[sd] = ta[sd];
            float rate = tw[sd] > p->arm_w[sd] ? 7.0f : 4.0f; p->arm_w[sd] += clampf(tw[sd] - p->arm_w[sd], -rate * dt, rate * dt);
            if (p->arm_w[sd] <= 0) p->arm[sd] = K3A_WHEEL; }
        /* the head: at whoever it is watching, else at the racer alongside, else into the bend */
        if (d->look_hold > 0) d->look_hold -= dt; else d->look_at = -1;
        if (d->look_at < 0 && r->state != RACE_FINISHED) { int best = -1; float bd = 9.0f * 9.0f;
            for (int j = 0; j < r->nk; j++) if (j != i && r->swallow_t[j] <= 0) { float dx = r->k[j].x - q->x, dz = r->k[j].z - q->z, dd = dx * dx + dz * dz; if (dd < bd) { bd = dd; best = j; } }
            if (best >= 0 && noise2(i * 17 + 1, (int)(p->t * 0.5f)) > 0.4f) drv_look(i, best, 1.2f + h); }
        float ly = -q->steer_vis * 0.3f, lp = q->air ? 0.15f : -0.04f;
        if (d->look_at >= 0 && d->look_at < r->nk) { float a; drv_side_of(r, i, d->look_at, &a); ly = clampf(a, -1.45f, 1.45f); lp = 0.05f; }
        float sk = d->shake; d->shake = fmaxf(0, d->shake - dt * 1.4f);
        float k = 1 - expf(-dt * 7.0f);
        p->look_yaw += (ly - p->look_yaw) * k; p->look_pitch += (lp - p->look_pitch) * k;
        p->tilt = sk * 0.35f * sinf(p->t * 28.0f) + (q->drifting ? 0.08f * (float)q->drift_dir : 0);
        /* the body: into the turn, back in the seat on a boost, a twist to look far round, a bump over the ground */
        float lean = q->steer_vis * (q->drifting ? 0.22f : 0.14f), tw2 = -q->steer_vis * 0.08f + (fabsf(p->look_yaw) > 1.0f ? (p->look_yaw > 0 ? 0.25f : -0.25f) : 0);
        float bow = q->boost_t > 0 ? -0.12f : (q->speed < 3 ? 0.04f : 0); if (p->arm[0] == K3A_UP && p->arm_w[0] > 0.5f) bow -= 0.08f;
        p->lean += (lean - p->lean) * k; p->twist += (tw2 - p->twist) * k; p->bow += (bow - p->bow) * k;
        float sp = clampf(fabsf(q->speed) / 25.0f, 0, 1); p->bob = 0.012f * sp * sinf(p->t * 14.0f + i) + q->squash * -0.05f; } }
/* ---- the boost blur (Sean: "a motion blur when the racer gets a boost"): the picture is copied, and stretched copies
   of it are laid over the edges, more at the corners and none in the middle, so the sides streak toward you ---- */
static float g_blur[4]; static Mesh *m_blur; static Vertex g_blurv[3600];
static void blur_update(float dt) {
    for (int p = 0; p < 4; p++) { float want = 0;
        if (p < g_nplayers && (g_screen == SCR_RACE)) { const Kart *q = &g_race.k[human_kart(p)]; if (q->boost_t > 0 && q->speed > 8) want = 1; }
        float rate = want > g_blur[p] ? 5.0f : 2.2f; g_blur[p] += clampf(want - g_blur[p], -rate * dt, rate * dt); } }
static void draw_boost_blur(int drc, int p, float fx, float fy, float fw, float fh, float vw, float vh) {
    float k = p >= 0 && p < 4 ? g_blur[p] : 0; if (k < 0.02f) return;
    Texture *tx = gfx_grab(drc); gfx_viewport(drc, fx, fy, fw, fh); if (!tx) return;
    if (!m_blur) m_blur = gfx_mesh_dynamic(3600);
    Batch b; batch_init(&b, g_blurv, 3600);
    const int NX = 12, NY = 8, NL = 3; float uc = fx + fw * 0.5f, vc = fy + fh * 0.5f;
    for (int l = 1; l <= NL; l++) { float z = 1.0f + 0.016f * l * k, la = 0.3f * k;
        for (int j = 0; j < NY; j++) for (int i = 0; i < NX; i++) {
            Vertex c4[4]; const int ci[4][2] = {{i, j + 1}, {i + 1, j + 1}, {i + 1, j}, {i, j}};
            for (int n = 0; n < 4; n++) { float gx = (float)ci[n][0] / NX, gy = (float)ci[n][1] / NY, dx = gx * 2 - 1, dy = gy * 2 - 1;
                float r = sqrtf(dx * dx * 0.8f + dy * dy * (dy > 0 ? 0.55f : 1.2f))   /* less at the bottom, where your own kart is */, m = clampf((r - 0.42f) / 0.55f, 0, 1); m = m * m * (3 - 2 * m);
                Vec3 sp = scr(gx * vw, gy * vh); float u = fx + gx * fw, v = fy + gy * fh;
                Vertex q = { sp.x, sp.y, 0, 0, 0, 1, uc + (u - uc) / z, vc + (v - vc) / z, 1, 1, 1, la * m }; c4[n] = q; }
            static const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int n = 0; n < 6 && b.n < b.cap; n++) b.v[b.n++] = c4[idx[n]]; } }
    gfx_mesh_update(m_blur, b.v, b.n); Mat4 id = m4_identity();
    gfx_camera_2d_virtual(vw, vh); gfx_state(ST_HUD); gfx_bind(tx); gfx_draw_ex(m_blur, &id, 1, 1, 1, 1, 0, 1, 0, 0); gfx_bind(NULL); }
/* the shadow pass: everything that casts a sun shadow, seen from the sun, around player 1 */
static void draw_shadow_casters(void) {
    const Race *r = &g_race; if (!g_def || g_nplayers > 1) { gfx_shadow_none(); return; }
    const Kart *k = &r->k[r->player]; Vec3 L = v3(g_def->light_x, g_def->light_y, g_def->light_z); float ll = sqrtf(L.x * L.x + L.y * L.y + L.z * L.z); L = v3(L.x / ll, L.y / ll, L.z / ll);
    Vec3 c = v3(k->x + sinf(k->yaw) * 22.0f, k->y, k->z + cosf(k->yaw) * 22.0f); const float half = 60.0f, dep = 260.0f;
    c.x = floorf(c.x / (2 * half / 1024)) * (2 * half / 1024); c.z = floorf(c.z / (2 * half / 1024)) * (2 * half / 1024);   /* steady edges as the kart moves */
    Mat4 view = m4_look_at(v3(c.x + L.x * dep * 0.5f, c.y + L.y * dep * 0.5f, c.z + L.z * dep * 0.5f), c, fabsf(L.y) > 0.95f ? v3(0, 0, 1) : v3(0, 1, 0));
    Mat4 proj = m4_identity(); proj.m[0] = 1.0f / half; proj.m[5] = 1.0f / half; proj.m[10] = -2.0f / dep; proj.m[14] = -1.0f;   /* square, straight-on (orthographic) */
    Mat4 lvp = m4_mul(proj, view), id = m4_identity();
    if (!gfx_shadow_begin(&lvp)) return;
    for (int m = 0; m < MAT_N; m++) { if (m == MAT_PLAIN && 0) continue; draw_mat_meshes(m_props[m], m_nprops[m], m, &id); }
    /* the karts are not in the sun's shadow map: each casts its own crisp shadow (a second, blocky one doubled it up) */
    gfx_shadow_end(); }
static void draw_scene(int vk, float aspect, float fov_mul, int lite) {
    Race *r = &g_race;
    float cy = cam_yaw; Vec3 ax = v3(-cosf(cy), 0, sinf(cy));
    Kart *k = &r->k[vk];
    float shk = g_shake * g_shake * 0.6f;
    Vec3 eye = v3(cam_x + sinf(g_time * 37.0f) * shk, cam_y + sinf(g_time * 53.0f) * shk, cam_z + cosf(g_time * 41.0f) * shk), at = v3(k->x + sinf(k->yaw) * 3.0f, k->y + 1.2f, k->z + cosf(k->yaw) * 3.0f);
    if (cv->finish_t > 0 && !g_attract && g_mode != MODE_BATTLE) at = v3(k->x, k->y + 1.0f, k->z);   /* the celebration looks at the kart itself */
    {
        Mat4 view = m4_look_at(eye, at, v3(0, 1, 0));
        Mat4 proj = m4_perspective(cam_fov * fov_mul * 3.14159265f / 180.0f, aspect, 0.3f, 2400.0f);   /* far enough for the mountain ranges */
        Mat4 vp = m4_mul(proj, view);
        Mat4 id = m4_identity();
        Vec3 ldir = v3(g_def->light_x, g_def->light_y, g_def->light_z);
        { /* hemisphere light: the sky's color from above, a bounce color from below, the sun's color on lit faces */
          float mx = fmaxf(g_def->sky_r, fmaxf(g_def->sky_g, g_def->sky_b)); if (mx < 0.05f) mx = 0.05f;
          Vec3 skyc = v3(0.3f + 0.7f * g_def->sky_r / mx, 0.3f + 0.7f * g_def->sky_g / mx, 0.3f + 0.7f * g_def->sky_b / mx);
          Vec3 gnd = g_def->night ? v3(0.5f, 0.45f, 0.5f) : (g_def->water ? v3(0.75f, 0.85f, 0.95f) : (g_def->grid_idx ? v3(1.0f, 0.95f, 0.9f) : v3(0.95f, 0.85f, 0.7f)));
          gfx_light(v3(g_def->sun_r, g_def->sun_g, g_def->sun_b), skyc, gnd, eye); }
        gfx_set_time(g_time); gfx_camera(&vp, ldir, 1.0f, g_def->fog_r, g_def->fog_g, g_def->fog_b, 1e6f, 2e6f);
        gfx_state(ST_BLEND); gfx_bind(tx_sky); { Mat4 sm = m4_translate(cam_x, -30.0f, cam_z); gfx_draw_ex(m_sky, &sm, 1, 1, 1, 1, 0, 1, 0, 0); }
        { /* the sun's glow in the sky: a soft halo where the light comes from (a cool moon-glow at night) */
          Vec3 L = v3_norm(ldir); if (L.y > -0.05f) { Vec3 sc = v3(eye.x + L.x * 850, eye.y + fmaxf(L.y, 0.12f) * 850, eye.z + L.z * 850);
            Vec3 fwd = v3_norm(v3_sub(sc, eye)), rt = v3_norm(v3_cross(fwd, v3(0, 1, 0))), up = v3_cross(rt, fwd);
            static Mesh *m_sun = NULL; if (!m_sun) m_sun = gfx_mesh_dynamic(18); Vertex sv[18]; Batch sb; batch_init(&sb, sv, 18);
            float nr = g_def->night ? 0.55f : 1.0f, ng = g_def->night ? 0.65f : 0.9f, nb = g_def->night ? 0.95f : 0.7f;
            for (int k = 0; k < 3; k++) { float R = k == 0 ? 330 : (k == 1 ? 130 : 45), al = k == 0 ? 0.22f : (k == 1 ? 0.35f : 0.75f);
                batch_quad(&sb, sc, v3(rt.x * R, rt.y * R, rt.z * R), v3(up.x * R, up.y * R, up.z * R), v3(0, 0, 1), 0, 0, 1, 1, nr, ng, nb, al * (g_def->night ? 0.5f : 1.0f)); }
            gfx_mesh_update(m_sun, sb.v, sb.n); gfx_state(ST_BLEND | ST_ADD); gfx_bind(tx_blob); gfx_draw_ex(m_sun, &id, 1, 1, 1, 1, 0, 1, 0, 0); gfx_state(ST_BLEND); } }
        if (m_mountains || m_clouds) { gfx_state(ST_DEPTH | ST_DEPTH_WRITE); gfx_bind(tx_white);   /* the 3D far world: lit and hazed when it was built */
            if (m_mountains) { gfx_bind(tx_grass); gfx_draw_ex(m_mountains, &id, 1, 1, 1, 1, 0, 1, 0, 0); gfx_bind(tx_white); }
            if (m_clouds) { Mat4 cm = m4_translate(fmodf(g_time * 1.5f, 400.0f) - 200.0f, 0, 0); gfx_draw_ex(m_clouds, &cm, 1, 1, 1, 1, 0, 1, 0, 0); }
            gfx_state(ST_SOLID); }
        if (tx_star) { gfx_state(ST_BLEND | ST_ADD); gfx_bind(tx_star);
            batch_clear(&g_b); float ss = g_def->star_size * (0.95f + 0.05f * sinf(g_time * 3.0f));
            Vec3 sp = v3(cam_x + g_def->star_x, g_def->star_y, cam_z + g_def->star_z);
            batch_quad(&g_b, sp, v3(ax.x * ss, 0, ax.z * ss), v3(0, ss, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 1);
            static Mesh *m_star = NULL; if (!m_star) m_star = gfx_mesh_dynamic(12); gfx_mesh_update(m_star, g_b.v, g_b.n); gfx_draw_ex(m_star, &id, 1, 1, 1, 1, 0, 1, 0, 0); }
        gfx_shadow_use(g_nplayers <= 1); gfx_camera(&vp, ldir, g_def->ambient, g_def->fog_r, g_def->fog_g, g_def->fog_b, g_def->fog_start, g_def->fog_end); gfx_shadow_use(0);   /* sun shadows in 1-player */
        gfx_state(ST_SOLID);
        float sr = 1, sg = 1, sb = 1;    /* the sun's color is applied by the shader now */
        /* the "grass" plane is the sea on a water road: it glints in the sun and drifts slowly */
        /* depth on every ground surface (Sean: "they need to be 3 dimensional"): bumps that catch the sun - see gfx_bump */
        gfx_bind(tx_grass); if (g_def->water) { gfx_bump(2); gfx_draw_ex(m_ground, &id, 1, 1, 1, 1, 0.9f, 0, g_time * 0.012f, g_time * 0.008f); } else { gfx_bump(2); gfx_draw(m_ground, &id, sr, sg, sb, 1); }   /* the lake's ripples catch the sun as they move */
        gfx_bump(2); if (m_field) gfx_draw(m_field, &id, sr, sg, sb, 1);   /* the open land, same grass as the ground */
        if (m_hills) { gfx_state(ST_DEPTH | ST_DEPTH_WRITE); gfx_draw(m_hills, &id, sr, sg, sb, 1); gfx_state(ST_SOLID); }   /* the hills beyond the walls */
        gfx_bump(3); gfx_bind(tx_dirt); gfx_draw(m_shoulder, &id, sr, sg, sb, 1);
        gfx_bump(4); gfx_bind(tx_road); gfx_draw_ex(m_road, &id, 1, 1, 1, 1, g_def->night ? 0.35f : 0.18f, 0, 0, 0);   /* a faint sheen on the road, wetter at night */
        gfx_bump(0);
        if (g_def->bonus != 2) { gfx_bind(tx_flag); gfx_draw(m_startline, &id, sr, sg, sb, 1); }
        gfx_bump(3); gfx_bind(tx_wall); gfx_draw(m_wall, &id, sr, sg, sb, 1);
        gfx_bind(tx_wall); gfx_draw(m_tunnel, &id, sr * 0.8f, sg * 0.8f, sb * 0.8f, 1); gfx_bump(0);
        if (g_def->bonus != 2) { gfx_bind(tx_flag); gfx_draw(m_arch, &id, sr, sg, sb, 1); }
        if (m_fallsw) { if (!tx_fallsw) tx_fallsw = make_tex(64, 128, 1, 0, px_waterfall);   /* the waterfalls: water sliding down, foam pulsing where it lands */
            gfx_state(ST_TRANSPARENT); gfx_bind(tx_fallsw); gfx_draw_ex(m_fallsw, &id, 1, 1, 1, 0.92f, 0.7f, 0, 0, -g_time * 1.6f);
            if (m_foam) { float fp = 0.8f + 0.2f * sinf(g_time * 7.0f); gfx_state(ST_TRANSPARENT | ST_ADD); gfx_bind(tx_blob); gfx_draw_ex(m_foam, &id, fp, fp, fp, 1, 0, 1, 0, 0); }
            gfx_state(ST_SOLID); }
        if (g_def->bonus == 2 && m_brook) { static Texture *tx_brook = NULL; if (!tx_brook) tx_brook = make_tex(128, 128, 1, 0, px_water); gfx_state(ST_TRANSPARENT); gfx_bind(tx_brook);
            Mat4 flow = m4_translate(sinf(g_time * 2.0f) * 0.3f, 0, 0); gfx_draw_ex(m_brook, &flow, 1, 1, 1, 0.75f, 0.8f, 0, g_time * 0.05f, 0); gfx_state(ST_SOLID); }
        { float pulse = 0.75f + 0.25f * sinf(g_time * 6.0f);
          if (!tx_padglow) tx_padglow = make_tex(64, 64, 0, 0, px_padglow);
          gfx_state(ST_DEPTH | ST_BLEND | ST_ADD); gfx_bind(tx_padglow); gfx_draw_ex(m_boostglow, &id, pulse, pulse, pulse, 1, 0, 1, 0, 0);   /* the pool of light */
          gfx_state(ST_DEPTH | ST_DEPTH_WRITE); gfx_bind(tx_white); { float gl = 1.05f + 0.35f * sinf(g_time * 6.0f); gfx_draw_ex(m_boostpads, &id, gl, gl, gl, 1, 0.4f, 1, 0, 0); }   /* the dove, shining */
          gfx_state(ST_TRANSPARENT);
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
        gfx_state(ST_DEPTH | ST_DEPTH_WRITE); gfx_bind(NULL); for (int m = 0; m < MAT_N; m++) draw_mat_meshes(m_props[m], m_nprops[m], m, &id); gfx_bind(NULL); if (m_curbs) gfx_draw(m_curbs, &id, 1, 1, 1, 1);
        for (int i = 0; i < g_nplaced; i++) { Placed *pl = &g_placed[i]; if (!pl->m3) continue;   /* the walls of Jericho, falling flat outward when their time comes */
            float f = pl->fall_t > 0 ? clampf((pl->fall_t - 0.15f) / 0.95f, 0, 1) : 0, a = 1.5707963f * f * f, ca = cosf(a), sa = sinf(a), shud = pl->fall_t > 0 && pl->fall_t < 0.3f ? sinf(pl->fall_t * 60.0f) * 0.15f : 0;
            Vec3 e1 = pl->t3, up = v3(pl->o3.x * sa, ca, pl->o3.z * sa), out = v3(pl->o3.x * ca, -sa, pl->o3.z * ca);
            Mat4 m = m4_identity(); m.m[0] = e1.x; m.m[1] = e1.y; m.m[2] = e1.z; m.m[4] = up.x; m.m[5] = up.y; m.m[6] = up.z; m.m[8] = out.x; m.m[9] = out.y; m.m[10] = out.z;
            m.m[12] = pl->p.x + e1.x * shud; m.m[13] = pl->p.y + 0.15f; m.m[14] = pl->p.z + e1.z * shud; gfx_bind(tx_mat[MAT_STONE]); gfx_draw(pl->m3, &m, 1, 1, 1, 1); } gfx_bind(NULL);
        {   /* the obstacles that cross the road, as 3D models: sheep, carts, nets, rolling stones, rubble */
            static Mesh *m_mov[MAX_MOVERS][MAT_N]; static int has_mov[MAX_MOVERS]; static const Track *built_for = NULL; static int built_n = -1;
            if (built_for != &g_track || built_n != g_loaded_track) { ModelOut mo; mat_buckets(&mo);
                for (int m = 0; m < MAX_MOVERS; m++) { for (int q = 0; q < MAT_N; q++) { gfx_mesh_free(m_mov[m][q]); m_mov[m][q] = NULL; } has_mov[m] = 0; }
                for (int m = 0; m < g_track.nmover; m++) if (model_known(g_track.mover[m].img)) { for (int q = 0; q < MAT_N; q++) batch_clear(mo.b[q]);
                    model_build(&mo, g_track.mover[m].img, g_track.mover[m].height, g_track.mover[m].height, (unsigned)m * 7u, v3(0, 0, 0), 0, 0.8f, 0.72f, 0.58f);
                    for (int q = 0; q < MAT_N; q++) if (mo.b[q]->n) m_mov[m][q] = gfx_mesh_static(mo.b[q]->v, mo.b[q]->n); has_mov[m] = 1;
                    for (int q = 0; q < MAT_N; q++) batch_clear(mo.b[q]); }
                built_for = &g_track; built_n = g_loaded_track; }
            for (int m = 0; m < g_track.nmover; m++) { const Mover *mv = &g_track.mover[m]; if (!has_mov[m]) continue;
                if (r->jericho && mv->img[1] == 'r' && mv->img[2] == 'u' && r->wall_fall < 0.3f) continue;
                float mx, my, mz, mx2, my2, mz2; track_mover_pos(&g_track, mv, r->race_t, &mx, &my, &mz); track_mover_pos(&g_track, mv, r->race_t + 0.05f, &mx2, &my2, &mz2);
                float dxm = mx2 - mx, dzm = mz2 - mz, face = (dxm * dxm + dzm * dzm > 1e-6f) ? atan2f(dxm, dzm) : 0;   /* it faces the way it is going */
                Mat4 mm = m4_mul(m4_translate(mx, my, mz), m4_rotate_y(face));
                if (!strcmp(mv->img, "#stone")) { float roll = r->race_t * mv->speed * 2.0f; mm = m4_mul(mm, m4_mul(m4_translate(0, mv->height * 0.5f, 0), m4_mul(m4_rotate_x(roll), m4_translate(0, -mv->height * 0.5f, 0)))); }
                for (int q = 0; q < MAT_N; q++) if (m_mov[m][q]) { gfx_bind(tx_mat[q]); gfx_draw(m_mov[m][q], &mm, 1, 1, 1, 1); } } gfx_bind(NULL); }
        gfx_state(ST_SOLID);   /* both sides drawn: no chance of a model showing inside out */
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
        /* hidden gems out in the open land: spinning crystals */
        for (int g = 0; g < g_track.ngem; g++) { if (r->gem_respawn[g] > 0) continue; const Gem *gm = &g_track.gem[g]; const TrackSample *gs = &g_track.s[gm->idx];
            float gx = gs->x + gs->nx * gm->lateral, gz = gs->z + gs->nz * gm->lateral; float ddx = gx - cam_x, ddz = gz - cam_z; if (ddx * ddx + ddz * ddz > 170.0f * 170.0f) continue;
            static const float GC[3][3] = {{1.0f, 0.25f, 0.4f}, {0.25f, 1.0f, 0.5f}, {0.35f, 0.6f, 1.0f}};   /* ruby, emerald, sapphire */
            Mat4 m = m4_mul(m4_translate(gx, track_surface_height(&g_track, gm->idx, gm->lateral) + 1.3f + 0.2f * sinf(g_time * 2.5f + g), gz), m4_rotate_y(g_time * 2.0f + g));
            gfx_draw_ex(m_gem, &m, GC[gm->color][0], GC[gm->color][1], GC[gm->color][2], 1, 1.5f, 0, 0, 0); }
        /* the great fish */
        if (r->whale.active && g_track.water) { float wt = r->whale.t, wy = g_track.water_y + 0.35f;
            if (wt < 1.3f) {   /* its shadow under the water, growing, and bubbles breaking the surface */
                Vertex sv[12]; Batch sb; batch_init(&sb, sv, 12); float gr = 0.4f + 0.6f * (wt / 1.3f);
                batch_quad(&sb, v3(r->whale.x, wy + 0.04f, r->whale.z), v3(r->whale.dz * 2.2f * gr, 0, -r->whale.dx * 2.2f * gr), v3(r->whale.dx * 5.5f * gr, 0, r->whale.dz * 5.5f * gr), v3(0, 1, 0), 0, 0, 1, 1, 0.02f, 0.06f, 0.12f, 0.55f * gr);
                static Mesh *m_wsh = NULL; if (!m_wsh) m_wsh = gfx_mesh_dynamic(12); gfx_mesh_update(m_wsh, sb.v, sb.n); gfx_state(ST_TRANSPARENT); gfx_bind(tx_blob); Mat4 idm = m4_identity(); gfx_draw(m_wsh, &idm, 1, 1, 1, 1); gfx_state(ST_SOLID); gfx_bind(NULL);
                if (frand01() < 0.5f) add_dust(v3(r->whale.x + (frand01() - 0.5f) * 5, wy, r->whale.z + (frand01() - 0.5f) * 5), 1.0f, 1.5f); }
            else if (wt < 3.0f) {   /* the leap: up out of the water, over, and down on the spot with its jaws snapping shut */
                float u = (wt - 1.3f) / 1.7f, al = -10.0f + 17.9f * u, yy = wy - 3.0f + 10.0f * sinf(3.14159265f * clampf(u / 0.72f, 0, 1)) - (u > 0.72f ? (u - 0.72f) * 14.0f : 0);
                float u2 = fminf(u + 0.01f, 1.0f), al2 = -10.0f + 17.9f * u2, yy2 = wy - 3.0f + 10.0f * sinf(3.14159265f * clampf(u2 / 0.72f, 0, 1)) - (u2 > 0.72f ? (u2 - 0.72f) * 14.0f : 0);
                float pitch = -atan2f(yy2 - yy, al2 - al), yaw = atan2f(r->whale.dx, r->whale.dz);
                float jaw = u < 0.48f ? 0.65f : (u < 0.56f ? 0.65f * (0.56f - u) / 0.08f : 0);
                Mat4 m = m4_mul(m4_translate(r->whale.x + r->whale.dx * al, yy, r->whale.z + r->whale.dz * al), m4_mul(m4_rotate_y(yaw), m4_rotate_x(pitch)));
                static Kart3D whale3d; static int whale_tried = 0; if (!whale_tried) { whale_tried = 1; kart3d_load(&whale3d, "whale"); }
                if (whale3d.ok) {   /* the 3D great fish, made from Sean's picture: it leaps nose-up and dives nose-down */
                    Mat4 mm = m4_mul(m, m4_translate(0, -3.5f, 0)); gfx_state(ST_DEPTH | ST_DEPTH_WRITE); kart3d_draw(&whale3d, &mm, 0, 0, 1, 1, 1, 1); gfx_state(ST_SOLID); gfx_bind(NULL); (void)jaw; }
                else if (tx_whale[2]) {   /* Sean's pictures: the one for the angle we see it from, tipped nose-up as it rises and nose-down as it falls */
                    float wx = r->whale.x + r->whale.dx * al, wz = r->whale.z + r->whale.dz * al;
                    float rel = atan2f(cam_x - wx, cam_z - wz) - yaw; while (rel > 3.14159265f) rel -= 6.2831853f; while (rel < -3.14159265f) rel += 6.2831853f;
                    float ar = fabsf(rel); int v = ar < 0.45f ? 0 : (ar < 1.15f ? 1 : (ar < 2.3f ? 2 : 3)); if (!tx_whale[v]) v = 2;
                    Vec3 ax2 = v3(-cosf(cam_yaw), 0, sinf(cam_yaw)); float head = r->whale.dx * ax2.x + r->whale.dz * ax2.z; int mirror = head < 0;
                    float hh = 7.5f, ww = hh * g_whale_asp[v], th = (v == 1 || v == 2) ? -pitch * (mirror ? -1.0f : 1.0f) * (v == 1 ? 0.6f : 1.0f) : 0, ct = cosf(th), st = sinf(th);
                    Vec3 R = v3(ax2.x * ct, st, ax2.z * ct), U = v3(-ax2.x * st, ct, -ax2.z * st), c = v3(wx, yy, wz);
                    Vertex wv[6]; Batch wb; batch_init(&wb, wv, 6); float u0 = mirror ? 1.0f : 0.0f, u1 = 1.0f - u0;
                    batch_quad4(&wb, v3(c.x - R.x * ww / 2 - U.x * hh / 2, c.y - R.y * ww / 2 - U.y * hh / 2, c.z - R.z * ww / 2 - U.z * hh / 2), v3(c.x + R.x * ww / 2 - U.x * hh / 2, c.y + R.y * ww / 2 - U.y * hh / 2, c.z + R.z * ww / 2 - U.z * hh / 2),
                                v3(c.x + R.x * ww / 2 + U.x * hh / 2, c.y + R.y * ww / 2 + U.y * hh / 2, c.z + R.z * ww / 2 + U.z * hh / 2), v3(c.x - R.x * ww / 2 + U.x * hh / 2, c.y - R.y * ww / 2 + U.y * hh / 2, c.z - R.z * ww / 2 + U.z * hh / 2), v3(0, 1, 0), u0, 0, u1, 1, 1, 1, 1, 1);   /* same corner order as the scenery pictures */
                    static Mesh *m_wspr = NULL; if (!m_wspr) m_wspr = gfx_mesh_dynamic(6); gfx_mesh_update(m_wspr, wb.v, wb.n);
                    Mat4 idm = m4_identity(); gfx_state(ST_CUTOUT); gfx_bind(tx_whale[v]); gfx_draw(m_wspr, &idm, 1, 1, 1, 1); gfx_state(ST_SOLID); gfx_bind(NULL); (void)jaw; (void)m; }
                else { gfx_state(ST_DEPTH | ST_DEPTH_WRITE); gfx_bind(NULL); gfx_draw_ex(m_whale, &m, 1, 1, 1, 1, 0.8f, 0, 0, 0);
                    Mat4 mj = m4_mul(m, m4_mul(m4_translate(0, -0.55f, -0.4f), m4_rotate_x(jaw))); gfx_draw_ex(m_whale_jaw, &mj, 1, 1, 1, 1, 0.6f, 0, 0, 0); gfx_state(ST_SOLID); }
                static float last_u = 1; if ((last_u < 0.02f || last_u > u) && u >= 0) { for (int n = 0; n < 70; n++) add_dust(v3(r->whale.x - r->whale.dx * 10 + (frand01() - 0.5f) * 4, wy, r->whale.z - r->whale.dz * 10 + (frand01() - 0.5f) * 4), 3.0f, 6.0f); }
                if (last_u < 0.72f && u >= 0.72f) for (int n = 0; n < 80; n++) add_dust(v3(r->whale.x + r->whale.dx * 3 + (frand01() - 0.5f) * 5, wy, r->whale.z + r->whale.dz * 3 + (frand01() - 0.5f) * 5), 4.0f, 7.0f);
                last_u = u; } }

        /* the karts */
        gfx_state(ST_SOLID); gfx_bind(NULL);
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; if (r->swallow_t[i] > 0) continue;   /* inside the great fish */ const Character *c = &CHARACTERS[r->character[i]];
            if (i != vk) { float cdx = q->x - eye.x, cdz = q->z - eye.z; if (cdx * cdx + cdz * cdz < 3.2f * 3.2f) continue; }   /* a kart sitting on the camera would fill the screen */
            float pitch = -atan2f(q->hit.slope, 1.0f) * (q->air ? 0.3f : 1.0f) + (q->air ? -q->vy * 0.03f : 0.0f);
            float roll = q->hit.on_road ? -g_track.s[q->hit.idx].bank : 0.0f;
            float sq = 1.0f - q->squash * 0.25f;
            Mat4 m = kart_mat(r, i); (void)pitch; (void)roll; (void)sq;
            int sty = r->style[i] >= 0 && r->style[i] < NSTYLES ? r->style[i] : 0;
            if (g_k3d[r->character[i]].ok) {   /* a real 3D kart: body and driver, wheels that roll and steer, and its shadow on the road */
                const Kart3D *k3 = &g_k3d[r->character[i]]; float st = q->stun > 0 ? 0.6f : 1.0f;
                gfx_state(ST_DEPTH | ST_DEPTH_WRITE); kart3d_draw_pose(k3, &m, g_wheel_ang[i], -q->steer_vis * 0.42f, &g_drv[i].p, sr * st, sg * st, sb * st, 1);
                if (!q->air || q->hop < 2.5f) {   /* the shadow: the kart squashed flat onto the road along the sun */
                    float gy = q->hit.height + 0.04f, lx = ldir.x, ly = ldir.y < 0.25f ? 0.25f : ldir.y, lz = ldir.z;
                    Mat4 sh = m4_identity(); sh.m[4] = -lx / ly; sh.m[5] = 0.0f; sh.m[6] = -lz / ly; sh.m[12] = lx / ly * gy; sh.m[13] = gy; sh.m[14] = lz / ly * gy;
                    /* one even shade (Sean: the inside of the shadows was squiggly - the flattened layers fought each other):
                       mark the patch 'no shadow' (alpha 1), mark the shadow's spots in the alpha alone (every layer writes the same
                       0.58, so overlaps can't add up), then darken the patch once by what is marked */
                    Mat4 smx = m4_mul(sh, m), pm = m4_mul(m4_translate(q->x - lx / ly * 1.3f, gy, q->z - lz / ly * 1.3f), m4_scale(2.0f + fabsf(lx / ly) * 1.5f, 1, 2.0f + fabsf(lz / ly) * 1.5f)); gfx_bind(NULL);
                    gfx_state(ST_AMASK); gfx_draw_ex(m_shsquare, &pm, 0, 0, 0, 1, 0, 1, 0, 0);
                    gfx_state(ST_AMASK | ST_DEPTH); kart3d_draw_flat(k3, &smx, 0, 0, &g_drv[i].p, 0.0f, 0.0f, 0.0f, 0.58f);
                    gfx_state(ST_MULDA); gfx_draw_ex(m_shsquare, &pm, 1, 1, 1, 1, 0, 1, 0, 0); }
                gfx_state(ST_SOLID); gfx_bind(NULL); continue; }
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
            if (gt) { if (!m_ghost) m_ghost = gfx_mesh_dynamic(6); float kw = 2.8f * KART_DRAW, kh = kw / gasp; (void)gyaw;
                batch_clear(&g_b); batch_quad(&g_b, v3(gx, gh.height + kh * 0.5f - 0.1f, gz), v3(ax.x * kw * 0.5f, 0, ax.z * kw * 0.5f), v3(0, kh * 0.5f, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.7f, 0.9f, 1.0f, 0.45f);
                gfx_mesh_update(m_ghost, g_b.v, g_b.n); gfx_state(ST_TRANSPARENT); gfx_bind(gt); gfx_draw_ex(m_ghost, &id, 0.7f, 0.9f, 1.0f, 0.45f, 0, 1, 0, 0); gfx_state(ST_CUTOUT); } }
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; if (r->swallow_t[i] > 0) continue;   /* inside the great fish */ int ch = r->character[i];
            if (i != vk) { float cdx = q->x - eye.x, cdz = q->z - eye.z; if (cdx * cdx + cdz * cdz < 3.2f * 3.2f) continue; }
            float hx = sinf(q->yaw + q->slide), hz = cosf(q->yaw + q->slide);
            if (g_k3d[ch].ok) continue;   /* the driver is part of the 3D kart */
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
                float kw = (v == 2 ? 3.4f : (v == 1 || v == 3 ? 3.0f : 2.6f)) * KART_DRAW, kh = kw / kasp, klean = q->steer_vis * -0.08f + q->slide * 0.15f;
                Vec3 kc = v3(q->x, q->y + q->hop * 0.4f + cele_bounce(r, i) + kh * 0.5f - 0.1f, q->z);
                batch_clear(&g_b);
                float u0 = mirror ? 1.0f : 0.0f, u1 = mirror ? 0.0f : 1.0f;
                batch_quad(&g_b, kc, v3(ax.x * kw * 0.5f, 0, ax.z * kw * 0.5f), v3(ax.x * klean * kh * 0.5f, kh * 0.5f, ax.z * klean * kh * 0.5f), v3(0, 1, 0), u0, 0, u1, 1, 1, 1, 1, 1);
                gfx_mesh_update(m_kart_spr[i], g_b.v, g_b.n); gfx_bind(ktex); float st = q->stun > 0 ? 0.6f : 1.0f; gfx_draw(m_kart_spr[i], &id, sr * st, sg * st, sb * st, 1);
                if (wd || td) {   /* real 3D tires over the painted ones: rubber with tread blocks and a gold spoked rim, rolling with the kart */
                    static Mesh *m_tire = NULL; if (!m_tire) { static Vertex tv3[3072]; Batch tb3; batch_init(&tb3, tv3, 3072); model_tire(&tb3); m_tire = gfx_mesh_static(tb3.v, tb3.n); }
                    Vec3 R = v3(ax.x * kw * 0.5f, 0, ax.z * kw * 0.5f), U = v3(ax.x * klean * kh * 0.5f, kh * 0.5f, ax.z * klean * kh * 0.5f);
                    float yk = q->yaw + q->slide + q->spin_vis + q->trick_vis, ang = g_wheel_ang[i];
                    float tx[6], ty[6], trad[6], twid[6]; int nt = 0;
                    if (wd) for (int n = 0; n < wd->n && nt < 6; n++) { tx[nt] = mirror ? 1.0f - wd->cx[n] : wd->cx[n]; ty[nt] = wd->cy[n]; trad[nt] = wd->ry[n] * kh * 1.06f; twid[nt] = trad[nt] * 0.8f; nt++; }
                    if (td) for (int n = 0; n < td->n && nt < 6; n++) { tx[nt] = mirror ? 1.0f - td->cx[n] : td->cx[n]; ty[nt] = td->cy[n]; trad[nt] = td->hh[n] * kh * 0.98f; twid[nt] = fmaxf(td->hw[n] * kw * 2.1f, trad[nt] * 0.5f); nt++; }
                    gfx_state(ST_DEPTH | ST_DEPTH_WRITE); gfx_bind(NULL);
                    for (int n = 0; n < nt; n++) { float fx = (tx[n] - 0.5f) * 2.0f, fy = (0.5f - ty[n]) * 2.0f;
                        Vec3 c = v3(kc.x + R.x * fx + U.x * fy, kc.y + R.y * fx + U.y * fy, kc.z + R.z * fx + U.z * fy);
                        float ex = eye.x - c.x, ez = eye.z - c.z, el = sqrtf(ex * ex + ez * ez) + 1e-4f, push = twid[n] * 0.5f + 0.06f;   /* stands just in front of the picture */
                        c.x += ex / el * push; c.z += ez / el * push;
                        Mat4 tm = m4_mul(m4_translate(c.x, c.y, c.z), m4_mul(m4_rotate_y(yk), m4_mul(m4_rotate_x(ang), m4_scale(twid[n], trad[n], trad[n]))));
                        gfx_draw(m_tire, &tm, sr * st, sg * st, sb * st, 1); }
                    gfx_state(ST_CUTOUT); (void)wtex; }
                if (ride) continue; }
            if (!tx_char[ch]) continue;
            /* the driver sits: only the top part of the standing picture shows (head, shoulders, chest), sunk into the seat */
            float part = pic ? 0.56f : 1.0f;
            float hgt = (pic ? 1.5f : 1.75f) * part, hw = (pic ? 1.5f : 1.75f) * 0.5f * g_char_aspect[ch];
            float bob = sinf(g_time * 11.0f + i) * 0.015f * fminf(fabsf(q->speed) / 20.0f, 1.0f);   /* a little engine shake */
            hgt *= 1.0f - q->squash * 0.18f + (q->air && q->hop <= 0 ? -0.08f : 0);                 /* lands with a bounce, tucks in on jumps */
            Vec3 c = v3(q->x - hx * 0.35f, q->y + q->hop * 0.4f + cele_bounce(r, i) + bob + (pic ? 0.85f : 0.55f) + hgt * 0.5f, q->z - hz * 0.35f);
            float lean = q->steer_vis * -0.12f;       /* leans into the turn */
            batch_clear(&g_b);
            batch_quad(&g_b, c, v3(ax.x * hw, 0, ax.z * hw), v3(ax.x * lean * hgt * 0.5f, hgt * 0.5f, ax.z * lean * hgt * 0.5f), v3(0, 1, 0), 0, 0, 1, part, 1, 1, 1, 1);
            gfx_mesh_update(m_driver_spr[i], g_b.v, g_b.n); gfx_bind(tx_char[ch]); gfx_draw(m_driver_spr[i], &id, sr, sg, sb, 1); }
        gfx_state(ST_SOLID); gfx_bind(NULL);
        if (g_mode == MODE_BATTLE) {
            Battle *b = &g_battle;
            /* stones on the ground and in the air, spears */
            for (int i = 0; i < BATTLE_STONES; i++) { if (b->stone_respawn[i] > 0) continue; TrackHit h; track_locate(&g_track, b->stone_x[i], b->stone_z[i], -1, &h);
                Mat4 m = m4_mul(m4_translate(b->stone_x[i], h.height + 0.55f + 0.12f * sinf(g_time * 3 + i), b->stone_z[i]), m4_mul(m4_rotate_y(g_time * 0.8f + i), m4_scale(1.6f, 1.6f, 1.6f)));
                gfx_draw_ex(m_pebble, &m, 0.92f, 0.9f, 0.85f, 1, 0.9f, 0, 0, 0); }
            { batch_clear(&g_b);   /* a golden ring on the ground and a soft beam of light over each stone, so they can be found from across the valley */
              for (int i = 0; i < BATTLE_STONES; i++) { if (b->stone_respawn[i] > 0) continue; float pul = 1.0f + 0.15f * sinf(g_time * 4 + i);
                  batch_quad(&g_b, v3(b->stone_x[i], 0.08f, b->stone_z[i]), v3(2.4f * pul, 0, 0), v3(0, 0, -2.4f * pul), v3(0, 1, 0), 0, 0, 1, 1, 1, 0.85f, 0.35f, 0.9f);
                  batch_quad(&g_b, v3(b->stone_x[i], 6.0f, b->stone_z[i]), v3(ax.x * 1.1f, 0, ax.z * 1.1f), v3(0, 6.0f, 0), v3(0, 1, 0), 0.3f, 0, 0.7f, 1, 1, 0.9f, 0.6f, 0.35f); }
              static Mesh *m_stoneglow = NULL; if (!m_stoneglow) m_stoneglow = gfx_mesh_dynamic(BATTLE_STONES * 12);
              gfx_mesh_update(m_stoneglow, g_b.v, g_b.n); gfx_state(ST_TRANSPARENT | ST_ADD); gfx_bind(tx_blob); gfx_draw(m_stoneglow, &id, 1, 1, 1, 1); gfx_state(ST_SOLID); gfx_bind(NULL); }
            for (int i = 0; i < BATTLE_SPEARS; i++) { Missile *ms = &b->stone[i]; if (!ms->alive) continue; Mat4 m = m4_mul(m4_translate(ms->x, ms->y, ms->z), m4_scale(0.9f, 0.9f, 0.9f)); gfx_draw(m_pebble, &m, 0.55f, 0.52f, 0.5f, 1); }
            if (!m_spear3d) {   /* a real spear: a long wooden shaft, a leather grip, a broad bronze head and a bronze butt-spike, pointing down +z */
                batch_clear(&g_b); const int NS = 8;
                for (int q = 0; q < NS; q++) { float a0 = q * 6.2831853f / NS, a1 = (q + 1) * 6.2831853f / NS, am = (a0 + a1) / 2; float rr = 0.07f;
                    for (int seg = 0; seg < 3; seg++) { float z0 = seg == 0 ? -2.3f : (seg == 1 ? -0.6f : 0.0f), z1 = seg == 0 ? -0.6f : (seg == 1 ? 0.0f : 1.9f), cr = seg == 1 ? 0.35f : 0.55f, cg = seg == 1 ? 0.22f : 0.38f, cb = seg == 1 ? 0.15f : 0.22f;
                        batch_quad4(&g_b, v3(cosf(a0) * rr, sinf(a0) * rr, z0), v3(cosf(a1) * rr, sinf(a1) * rr, z0), v3(cosf(a1) * rr, sinf(a1) * rr, z1), v3(cosf(a0) * rr, sinf(a0) * rr, z1), v3(cosf(am), sinf(am), 0), 0, 0, 1, 1, cr, cg, cb, 1); }
                    float hr = 0.24f; Vec3 tip = v3(0, 0, 2.9f), b0 = v3(cosf(a0) * hr * (q & 1 ? 0.35f : 1.0f), sinf(a0) * hr * 0.25f, 1.9f), b1 = v3(cosf(a1) * hr * (q & 1 ? 1.0f : 0.35f), sinf(a1) * hr * 0.25f, 1.9f);
                    Vertex tv[3] = {{tip.x, tip.y, tip.z, cosf(am), sinf(am), 0.3f, 0, 0, 0.95f, 0.72f, 0.35f, 1}, {b1.x, b1.y, b1.z, cosf(am), sinf(am), 0.3f, 0, 0, 0.85f, 0.6f, 0.28f, 1}, {b0.x, b0.y, b0.z, cosf(am), sinf(am), 0.3f, 0, 0, 0.85f, 0.6f, 0.28f, 1}};
                    for (int k2 = 0; k2 < 3; k2++) if (g_b.n < g_b.cap) g_b.v[g_b.n++] = tv[k2];
                    Vertex sv2[3] = {{0, 0, -2.6f, cosf(am), sinf(am), -0.3f, 0, 0, 0.8f, 0.58f, 0.28f, 1}, {cosf(a0) * 0.09f, sinf(a0) * 0.09f, -2.3f, cosf(am), sinf(am), 0, 0, 0, 0.8f, 0.58f, 0.28f, 1}, {cosf(a1) * 0.09f, sinf(a1) * 0.09f, -2.3f, cosf(am), sinf(am), 0, 0, 0, 0.8f, 0.58f, 0.28f, 1}};
                    for (int k2 = 0; k2 < 3; k2++) if (g_b.n < g_b.cap) g_b.v[g_b.n++] = sv2[k2]; }
                m_spear3d = gfx_mesh_static(g_b.v, g_b.n); }
            for (int i = 0; i < BATTLE_SPEARS; i++) { Missile *ms = &b->spear[i]; if (!ms->alive) continue;   /* flying point first along its arc, turning a little as it goes */
                float hv = sqrtf(ms->vx * ms->vx + ms->vz * ms->vz) + 1e-3f, pitch = -atan2f(ms->vy, hv);
                Mat4 m = m4_mul(m4_translate(ms->x, ms->y, ms->z), m4_mul(m4_rotate_y(atan2f(ms->vx, ms->vz)), m4_mul(m4_rotate_x(pitch), m4_rotate_z(g_time * 6 + i))));
                gfx_bind(NULL); gfx_draw_ex(m_spear3d, &m, 1, 1, 1, 1, 0.6f, 0, 0, 0); }
            if (g_gol3d.ok) {   /* Goliath in 3D, really moving: walking, charging head down, roaring, the over-the-shoulder spear throw,
                                   reeling when struck, holding his head when dazed, and falling on his face (1 Samuel 17:49) */
                const char *cn = "walk"; float ct = b->gstate_t;
                switch (b->gstate) { case GOL_STALK: cn = "walk"; ct = b->gstate_t * 1.6f; break; case GOL_CHARGE: cn = "run"; ct = b->gstate_t * 1.2f; break; case GOL_WINDUP: cn = "roar"; ct = 0.6f + b->gstate_t * 2.5f; break;
                    case GOL_THROW: { int ci = anim3d_clip(&g_gol3d, "throw"); float d = ci >= 0 ? g_gol3d.clip[ci].dur : 4.3f; cn = "throw"; ct = b->gstate_t / 0.9f * d * 0.6f; } break;
                    case GOL_STUNNED: cn = "hit"; ct = b->gstate_t * 1.8f; break; case GOL_DAZED: cn = "daze"; break; case GOL_DOWN: cn = "fall"; ct = b->gstate_t * 1.1f; break; }
                Mat4 gm = m4_mul(m4_translate(b->gx, b->gy - 0.05f, b->gz), m4_rotate_y(b->gyaw));
                float tint = b->gstate == GOL_STUNNED ? 0.75f + 0.25f * sinf(g_time * 40) : 1.0f;
                gfx_state(ST_DEPTH | ST_DEPTH_WRITE); anim3d_draw(&g_gol3d, anim3d_clip(&g_gol3d, cn), ct, &gm, tint, tint, tint, 1); gfx_bind(NULL);
                if (b->gstate == GOL_DAZED) {   /* stars circling his head */
                    batch_clear(&g_b); for (int k = 0; k < 5; k++) { float a2 = g_time * 3 + k * 1.2566f; Vec3 sp = v3(b->gx + cosf(a2) * 1.1f, b->gy + battle_head_y(b) + 0.8f + 0.15f * sinf(g_time * 5 + k), b->gz + sinf(a2) * 1.1f);
                        batch_quad(&g_b, sp, v3(ax.x * 0.28f, 0, ax.z * 0.28f), v3(0, 0.28f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 0.9f, 0.3f, 1); }
                    static Mesh *m_stars = NULL; if (!m_stars) m_stars = gfx_mesh_dynamic(30); gfx_mesh_update(m_stars, g_b.v, g_b.n); gfx_state(ST_TRANSPARENT | ST_ADD); gfx_bind(tx_blob); gfx_draw(m_stars, &id, 1, 1, 1, 1); gfx_state(ST_SOLID); gfx_bind(NULL); }
                if (tx_bearer && b->gstate != GOL_DOWN) {   /* the shield-bearer walks before him */
                    gfx_state(ST_CUTOUT); float bx = b->gx + sinf(b->sb_yaw) * 3.2f, bz = b->gz + cosf(b->sb_yaw) * 3.2f; TrackHit bh; track_locate(&g_track, bx, bz, -1, &bh); float bhg = 4.2f, bhw = bhg * 0.5f * g_bearer_aspect;
                    batch_clear(&g_b); batch_quad(&g_b, v3(bx, bh.height + bhg * 0.5f, bz), v3(ax.x * bhw, 0, ax.z * bhw), v3(0, bhg * 0.5f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 1);
                    gfx_mesh_update(m_bearer, g_b.v, g_b.n); gfx_bind(tx_bearer); gfx_draw(m_bearer, &id, sr, sg, sb, 1); gfx_state(ST_SOLID); gfx_bind(NULL); } }
            /* Goliath himself: a giant standing picture, leaning into his charge, shaking when stunned */
            else if (tx_goliath) { gfx_state(ST_CUTOUT); TrackHit h; track_locate(&g_track, b->gx, b->gz, -1, &h);
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
#if defined(KK_TOUR) && KK_TOUR_ROAD == 0
        if (r->race_t > 2) r->k[r->player].shield = 8.0f;   /* the CI photos of the Holy Ghost fire */
#endif
        {   /* the Shield of Faith: a ring of rainbow fire (Holy Ghost fire, Acts 2:3) burning round the racer: tongues of flame
               in every color rise, flicker and fade all the way round, with a glow on the ground and sparks spiralling up */
            static Vertex fv[4 * 150 * 6]; static Mesh *m_fire = NULL; if (!m_fire) m_fire = gfx_mesh_dynamic(4 * 150 * 6);
            Batch fb; batch_init(&fb, fv, 4 * 150 * 6); int any = 0;
            for (int i = 0; i < r->nk && i < 4 * 8; i++) { Kart *q = &r->k[i]; if (q->shield <= 0 || r->swallow_t[i] > 0 || fb.n + 150 * 6 > fb.cap) continue;
                float dx = q->x - eye.x, dz = q->z - eye.z, dl = sqrtf(dx * dx + dz * dz) + 1e-3f; if (dl > 120) continue; any = 1;
                Vec3 R = v3(dz / dl, 0, -dx / dl), cU = v3(0, 1, 0);
                float fade = clampf(q->shield * 2.0f, 0, 1), born = clampf((8.5f - q->shield) * 3.0f, 0, 1);   /* it bursts up when taken and dies down at the end */
                for (int k = 0; k < 44; k++) { float h1 = noise2(k, i * 13) , ang = k * 6.2831853f / 44 + g_time * 0.7f + h1 * 0.3f, ph = fmodf(g_time * (1.3f + h1 * 0.6f) + h1 * 7.0f, 1.0f);
                    float rad = 1.45f + 0.18f * sinf(g_time * 3 + k) + ph * 0.15f, y = q->y + 0.05f + ph * 1.7f * born, sz = (0.5f + 0.35f * h1) * (1.0f - ph * 0.55f) * born, a = sinf(3.14159f * ph) * fade;
                    float hue = fmodf(ang / 6.2831853f + g_time * 0.22f + 1.0f, 1.0f) * 6.0f, cr = clampf(fabsf(hue - 3) - 1, 0, 1), cg = clampf(2 - fabsf(hue - 2), 0, 1), cb = clampf(2 - fabsf(hue - 4), 0, 1);
                    Vec3 c = v3(q->x + cosf(ang) * rad * 1.05f, y + sz, q->z + sinf(ang) * rad * 1.25f);
                    batch_quad(&fb, c, v3(R.x * sz * 0.42f, 0, R.z * sz * 0.42f), v3(0, sz, 0), cU, 0, 0, 1, 1, cr, cg, cb, a * 0.9f);
                    batch_quad(&fb, v3(c.x, c.y - sz * 0.35f, c.z), v3(R.x * sz * 0.2f, 0, R.z * sz * 0.2f), v3(0, sz * 0.55f, 0), cU, 0, 0, 1, 1, 0.6f + cr * 0.4f, 0.6f + cg * 0.4f, 0.6f + cb * 0.4f, a * 0.7f); }   /* the hot white heart of each tongue */
                for (int k = 0; k < 14; k++) { float h1 = noise2(k + 99, i), ph = fmodf(g_time * 0.6f + h1, 1.0f), ang = h1 * 6.28f + ph * 4.0f, rad = 1.2f + ph * 0.6f, s2 = 0.09f * (1 - ph);   /* sparks rising in a spiral */
                    float hue = fmodf(h1 + g_time * 0.3f, 1.0f) * 6.0f; batch_quad(&fb, v3(q->x + cosf(ang) * rad, q->y + 0.4f + ph * 3.2f, q->z + sinf(ang) * rad), v3(R.x * s2, 0, R.z * s2), v3(0, s2 * 2, 0), cU, 0, 0, 1, 1,
                        clampf(fabsf(hue - 3) - 1, 0, 1) * 0.5f + 0.5f, clampf(2 - fabsf(hue - 2), 0, 1) * 0.5f + 0.5f, clampf(2 - fabsf(hue - 4), 0, 1) * 0.5f + 0.5f, (1 - ph) * fade); } }
            if (any) { gfx_mesh_update(m_fire, fb.v, fb.n); gfx_state(ST_TRANSPARENT | ST_ADD); gfx_bind(tx_flame); gfx_draw(m_fire, &id, 1, 1, 1, 1);
                batch_clear(&fb);   /* the glow on the ground under the fire, shifting through the colors */
                for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; if (q->shield <= 0 || r->swallow_t[i] > 0 || fb.n + 12 > fb.cap) continue; float hue = fmodf(g_time * 0.22f, 1.0f) * 6.0f, f = clampf(q->shield * 2.0f, 0, 1);
                    batch_quad(&fb, v3(q->x, q->y + 0.08f, q->z), v3(2.4f, 0, 0), v3(0, 0, 2.6f), v3(0, 1, 0), 0, 0, 1, 1, clampf(fabsf(hue - 3) - 1, 0, 1) * 0.6f + 0.4f, clampf(2 - fabsf(hue - 2), 0, 1) * 0.6f + 0.4f, clampf(2 - fabsf(hue - 4), 0, 1) * 0.6f + 0.4f, 0.55f * f); }
                static Mesh *m_fglow = NULL; if (!m_fglow) m_fglow = gfx_mesh_dynamic(64 * 6); gfx_mesh_update(m_fglow, fb.v, fb.n); gfx_bind(tx_blob); gfx_draw(m_fglow, &id, 1, 1, 1, 1); }
            gfx_state(ST_TRANSPARENT); gfx_bind(NULL); }
        gfx_bind(tx_blob); gfx_draw(m_items, &id, 1, 1, 1, 1);
        gfx_state(ST_TRANSPARENT | ST_ADD); gfx_draw(m_dust, &id, 1, 1, 1, 1); gfx_draw(m_spark, &id, 1, 1, 1, 1); gfx_draw(m_glow, &id, 1, 1, 1, 1);
    }
}
/* the HUD of one split-screen view, laid out on a virtual screen of vw x vh pixels */
static const char *item_short(int item) {
    switch (item) { case ITEM_MANNA: return "MANNA"; case ITEM_LOAVES: return "LOAVES"; case ITEM_SAMARITAN: return "SAMARITAN"; case ITEM_SHIELD: return "SHIELD";
                    case ITEM_PARTING: return "PARTING"; case ITEM_JUBILEE: return "JUBILEE"; case ITEM_FIRE: return "FIRE"; case ITEM_WATER: return "WATER"; default: return ""; }
}
static void hud_battle_split(int p, float vw, float vh) {   /* one David's corner of the screen when several fight Goliath together */
    Battle *b = &g_battle; char buf[64]; float sc = vw < 1000 ? 0.75f : 1.0f; int d = human_kart(p);
    { char tag[8]; snprintf(tag, sizeof tag, "P%d", p + 1); float tw = text_width(tag, 18 * sc) + 12; batch_rect(&g_b, vw / 2 - tw / 2, 8, tw, 24 * sc, PLAYER_COL[p][0], PLAYER_COL[p][1], PLAYER_COL[p][2], 0.9f); batch_text(&g_b, tag, vw / 2 - text_width(tag, 18 * sc) / 2, 11, 18 * sc, 1, 1, 1, 1); }
    for (int i = 0; i < BATTLE_LIVES; i++) batch_rect(&g_b, 16 + i * 26 * sc, 14, 20 * sc, 18 * sc, i < BATTLE_LIVES - b->dhits[d] ? 0.95f : 0.25f, 0.2f, 0.25f, 1);
    snprintf(buf, sizeof buf, "STONES x%d", b->dstones[d]); batch_text(&g_b, buf, 16, 40 * sc, 18 * sc, 0.95f, 0.95f, 0.85f, 1);
    snprintf(buf, sizeof buf, b->gstate == GOL_DAZED ? "FINAL SHOT!" : "GOLIATH %d/%d", b->hits_on_goliath, battle_need(b)); batch_text(&g_b, buf, vw - 16 - text_width(buf, 18 * sc), 14, 18 * sc, 1, 0.85f, 0.4f, 1);
    { int left = (int)fmaxf(0.0f, BATTLE_TIME_LIMIT - b->race.race_t); snprintf(buf, sizeof buf, "%d:%02d", left / 60, left % 60); batch_text(&g_b, buf, vw - 16 - text_width(buf, 16 * sc), 40 * sc, 16 * sc, 1, 1, 1, 1); }
    if (battle_out(b, d)) text_c_in("OUT - CHEER THEM ON!", vw, vh * 0.45f, 24 * sc, 1, 0.5f, 0.4f, 1);
    else if (b->race.state == RACE_RUNNING) { const char *h = b->dstones[d] ? "ZL: SLING" : "GET A STONE FROM THE BROOK!"; batch_text(&g_b, h, 16, vh - 30 * sc, 15 * sc, 1, 0.9f, 0.5f, (b->dstones[d] || fmodf(g_time, 1.0f) < 0.7f) ? 1.0f : 0.0f); }
    if (b->race.state == RACE_COUNTDOWN) { int n = (int)ceilf(b->race.cd - 0.5f); if (n >= 1 && n <= 3) { snprintf(buf, sizeof buf, "%d", n); text_c_in(buf, vw, vh * 0.35f, 80 * sc, 1, 1, 1, 1); } }
    if (p == 0 && b->quote >= 0 && b->quote_t > 0) { const BattleQuote *q = &BATTLE_QUOTES[b->quote]; float fs = 11 * sc; char line[200]; snprintf(line, sizeof line, "%s%s%s", q->who, q->who[0] ? ": " : "", q->line);
        if (text_width(line, fs) > vw - 20) fs = 8 * sc; text_c_in(line, vw, vh - 60 * sc, fs, 1, 0.95f, 0.7f, 1); }
}
static void hud_split(int p, float vw, float vh) {
    Race *r = &g_race; int vk = human_kart(p); Kart *k = &r->k[vk]; char buf[64]; float sc = vw < 1000 ? 0.75f : 1.0f;
    int lap = k->lap < 1 ? 1 : (k->lap > g_track.laps ? g_track.laps : k->lap);
    snprintf(buf, sizeof buf, "LAP %d/%d", lap, g_track.laps); text3d(buf, 16, 12, 26 * sc, 1, 1, 1, 1, 0);
    fmt_time(buf, sizeof buf, k->lap >= 1 ? k->lap_time : 0.0f); text3d(buf, 16, 12 + 30 * sc, 18 * sc, 1, 1, 0.6f, 1, 0);
    { char tag[8]; snprintf(tag, sizeof tag, "P%d", p + 1); float tw = text_width(tag, 18 * sc) + 12; batch_rect(&g_b, vw / 2 - tw / 2, 8, tw, 24 * sc, PLAYER_COL[p][0], PLAYER_COL[p][1], PLAYER_COL[p][2], 0.9f); batch_text(&g_b, tag, vw / 2 - tw / 2 + 6, 10, 18 * sc, 0.05f, 0.05f, 0.1f, 1);
      batch_text(&g_b, CHARACTERS[r->character[vk]].short_name, vw / 2 - text_width(CHARACTERS[r->character[vk]].short_name, 12 * sc) / 2, 12 + 26 * sc, 12 * sc, 1, 1, 1, 0.9f); }
    int mypos = 0; for (int q = 0; q < r->nk; q++) if (r->order[q] == vk) mypos = q + 1;
    if (k->finished && k->place) mypos = k->place;
    { static int lastpos[MAX_HUMANS] = {-1, -1, -1, -1}; static float pt0[MAX_HUMANS]; int pp = p < MAX_HUMANS ? p : 0; if (mypos != lastpos[pp]) { lastpos[pp] = mypos; pt0[pp] = g_time; }
      float pa = g_time - pt0[pp], ps = 48 * sc * (1 + 0.45f * expf(-pa * 7) * cosf(pa * 14)); text3d(place_name(mypos), vw - 16 - text_width(place_name(mypos), ps), 10 + (48 * sc - ps) * 0.5f, ps, 1, 1, 1, 1, 1); }
    snprintf(buf, sizeof buf, "%3.0f KM/H", fabsf(k->speed) * 3.6f); text3d(buf, vw - 16 - text_width(buf, 16 * sc), 12 + 52 * sc, 16 * sc, 1, 1, 1, 1, 0);
    /* the blessing slot, bottom left */
    { float bw = 120 * sc, bh = 44 * sc, bx = 16, by = vh - bh - 12;
      if (r->jericho) { batch_rect(&g_b, bx, by, bw, bh, 0.05f, 0.1f, 0.25f, 0.7f); batch_rect(&g_b, bx + 2, by + 2, bw - 4, bh - 4, 0.15f, 0.3f, 0.6f, 0.7f); }
      if (r->jericho) { int n = r->shofars[vk], last = k->lap >= g_track.laps; if (last) snprintf(buf, sizeof buf, n > 0 ? "ZL: SHOUT!" : "SHOUTED!"); else snprintf(buf, sizeof buf, "SHOFAR x%d", n);
          batch_text(&g_b, buf, bx + bw / 2 - text_width(buf, 12 * sc) / 2, by + bh / 2 - 6 * sc, 12 * sc, 1, 0.9f, 0.4f, 1); }
      else hud_item_window(p, 16, vh - 64 * sc - 14, 64 * sc, 0);
      if (g_track.ncoin) { batch_rect(&g_b, bx + bw + 10, by + bh - 16 * sc, 12 * sc, 12 * sc, 1, 0.85f, 0.3f, 1); snprintf(buf, sizeof buf, "x%d", k->coins); batch_text(&g_b, buf, bx + bw + 26 * sc, by + bh - 18 * sc, 14 * sc, 1, 0.9f, 0.5f, 1); } }
    if (k->drifting) { float c = clampf(k->drift_charge / 1.6f, 0, 1), w = 140 * sc; batch_rect(&g_b, vw - 16 - w, vh - 24, w, 10, 0.1f, 0.1f, 0.1f, 0.6f);
        batch_rect(&g_b, vw - 16 - w, vh - 24, w * c, 10, c >= 1 ? 1.0f : 0.3f, c >= 0.5f ? 0.9f : 0.4f, 0.2f, 1); }
    if (r->state == RACE_COUNTDOWN) { int n = (int)ceilf(r->cd - 0.5f); if (n >= 1 && n <= 3) fx_count(n, r->cd, vw / 2, vh * 0.3f, 80 * sc);
        else if (r->cd > 3.5f) { float a = clampf((r->cd - 3.5f) * 2.0f, 0, 1); fx_words("GET READY", vw / 2, vh * 0.3f, 44 * sc, 3.5f + RACE_READY_T - r->cd, r->cd - 3.5f, 1, 1, 1, 1, a); }
        if (p == 0 && !r->jericho) { const char *ref, *v = road_verse(g_def->prefix, &ref); float fs = 12 * sc; if (text_width(v, fs) > vw - 30) fs = 9 * sc; batch_text(&g_b, v, vw / 2 - text_width(v, fs) / 2, vh * 0.62f, fs, 1, 1, 1, clampf(r->cd * 1.2f, 0, 1)); } }
    if (g_pmsg_t[p] > 0) { float sz = 30 * sc; if (text_width(g_pmsg[p], sz) > vw - 40) sz = 22 * sc; float my = vh * 0.15f;   /* up over the sky, so the road stays in view */
        float ag = msg_age(p); fx_words(g_pmsg[p], vw / 2, my, sz, ag, g_pmsg_t[p], 1, 1, 1, 1, 1);
        if (g_pmsg2[p][0]) fx_line(g_pmsg2[p], vw / 2, my + sz + 6, 13 * sc, ag, g_pmsg_t[p], 0.9f, 0.95f, 1, 1); }
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
    Pad merged = merge_pads(pads, npads); const Pad *pad = &merged;
    /* (Sean) only player 1 works the menus: the controller that pressed a button on the title screen (or player 1 of a
       group). In a race anyone can pause, and whoever paused works the pause menu. The title, join and group character
       screens still listen to everyone; if player 1's controller goes away, everyone can use the menus again. */
    { int sl = -1;
      if (g_screen == SCR_PAUSE) sl = g_pause_slot;
      else if (g_screen != SCR_TITLE && g_screen != SCR_RACE && g_screen != SCR_JOIN && g_screen != SCR_MCHAR && g_screen != SCR_OPENER) sl = g_menu_slot;
      if (sl >= 0 && sl < npads && pads[sl].source) pad = &pads[sl]; }
    g_frame_ms = dt * 1000.0f;
#if defined(KK_TOUR) && KK_TOUR_ROAD == 12
    { static int done = 0; if (!done && g_time > 2.0f) { done = 1;   /* CI: straight to a trophy ceremony, the field behind it is the demo race */
        g_mode = MODE_GP; g_cup = 0; g_gp_round = NUM_GP_TRACKS; memset(g_gp_points, 0, sizeof g_gp_points); memset(g_gp_kp, 0, sizeof g_gp_kp);
        for (int i = 0; i < g_race.nk; i++) g_gp_points[g_race.character[i]] = 60 - 7 * i;
        g_player_char = g_race.character[1]; scene_check_words(); scene_start(0); } }   /* CI: the trophy screen is unchanged; photograph the Bible stories from Jesus on (tour 13 goes from Samson) */
#endif
#if defined(KK_TOUR) && KK_TOUR_ROAD >= 13
    { static int done = 0; static const int first[7] = {7, 2, 4, 6, 9, 11, 13}; if (!done && g_time > 2.0f) { done = 1; g_cup = 1; scene_check_words(); scene_start(first[(KK_TOUR_ROAD - 13) % 7]); } }   /* CI: straight into the cup winners' Bible stories, from Samson on */
#endif
#if defined(KK_TOUR) && KK_TOUR_ROAD == 1
    if (g_screen == SCR_RACE && g_track.water && g_race.state == RACE_RUNNING && g_time > 55.0f) {   /* CI: the great fish held mid-leap just ahead of the player, for the photos */
        Kart *k = &g_race.k[g_race.player]; const TrackSample *ws = &g_track.s[(k->hit.idx + 7) % g_track.n]; float lat = track_edge(&g_track, (k->hit.idx + 7) % g_track.n) + 6.0f;
        g_race.whale.active = 1; g_race.whale.t = 1.75f; g_race.whale.target = g_race.player; g_race.whale.x = ws->x + ws->nx * lat; g_race.whale.z = ws->z + ws->nz * lat; g_race.whale.dx = ws->tx; g_race.whale.dz = ws->tz; }
#endif
    if (g_sting_due && g_race.state == RACE_COUNTDOWN && g_race.cd <= 4.85f) { g_sting_due = 0; sfx_play(SFX_COUNTDOWN, 1, 0); }   /* the sting's closing hit is 4.85 s in */
    if (g_race.state != RACE_COUNTDOWN) g_sting_due = 0;
    /* tire treads seen edge-on: at racing speed the real roll (dozens of grooves a second) would jump a whole groove or more
       between frames and look frozen, so the drawn roll is capped at 0.35 of a groove per frame - always visibly rolling */
    for (int i = 0; i < NKARTS && i < g_race.nk; i++) { float rate = g_race.k[i].speed / 0.42f / 6.2831853f * dt, cap = 0.35f * 0.25f / 4.0f;
        if (rate > cap) rate = cap; if (rate < -cap) rate = -cap; g_tread_ph[i] += rate; if (g_tread_ph[i] > 1000.0f) g_tread_ph[i] -= 1000.0f;
        /* the wheels seen from the side: the real turn rate (ten turns a second at speed) strobes on a 60 Hz screen and the
           hubcaps whirl like a cartoon, so they turn with the kart's speed but never more than about two turns a second */
        float wr = g_race.k[i].speed / 0.42f * dt, wcap = 12.0f * dt; if (wr > wcap) wr = wcap; if (wr < -wcap) wr = -wcap;
        g_wheel_ang[i] += wr; if (g_wheel_ang[i] > 6283.0f) g_wheel_ang[i] -= 6283.1853f; }
    if (g_screen == SCR_RACE || g_screen == SCR_RESULTS) drivers_update(&g_race, dt);
    blur_update(dt);
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
    {   /* the GamePad touch screen */
        static int was_touch = 0, tz_screen = -1; const Pad *gp = npads > 0 ? &pads[0] : NULL; int tap = gp && gp->touch && !was_touch; was_touch = gp && gp->touch;
        if (tz_screen != g_screen) { g_ntz_prev = 0; tz_screen = g_screen; }   /* buttons from the screen before do not count */
        if (tap && (g_screen == SCR_RACE || g_screen == SCR_PAUSE) && !g_attract && gp->tx > 1060 && gp->ty > 610) {   /* the MAP / RACE corner button */
            g_pad_race = !g_pad_race; sfx_play(SFX_MENU_MOVE, 1, 0); tap = 0; }
        if (tap && g_screen_t > 0.3f) { int hit = -1;
            for (int z = 0; z < g_ntz_prev; z++) { const TZone *q = &g_tz_prev[z]; if (gp->tx >= q->x && gp->tx < q->x + q->w && gp->ty >= q->y && gp->ty < q->y + q->h) hit = z; }
            if (hit >= 0) { const TZone *q = &g_tz_prev[hit];
                if (q->kind == TZ_A) { pa = 1; pa_s[0] = 1; }
                else if (q->kind == TZ_B) { pb = 1; pb_s[0] = 1; }
                else if (q->kind == TZ_PLUS) { pplus = 1; pplus_s[0] = 1; }
                else {   /* pick the card or line, then left/right or confirm */
                    switch (g_screen) {
                        case SCR_MAIN: g_menu_sel = q->val; break;
                        case SCR_CUP: g_cup = q->val; break;
                        case SCR_CHAR: g_player_char = q->val; break;
                        case SCR_MCHAR: if (!g_plock[0]) g_pchar[0] = q->val; break;
                        case SCR_KART: g_player_style = q->val; break;
                        case SCR_TRACK: g_track_idx = q->val; break;
                        case SCR_OPTIONS: g_opt_sel = q->val; break;
                        case SCR_PAUSE: g_pause_sel = q->val; break;
                        case SCR_RESULTS: g_res_sel = q->val; break;
                    }
                    if (q->kind == TZ_LEFT) mx = -1; else if (q->kind == TZ_RIGHT) mx = 1;
                    else if (g_screen == SCR_MCHAR) pa_s[0] = 1; else pa = 1; }
                sfx_play(SFX_MENU_MOVE, 0.6f, 0); }
            else if (g_screen == SCR_TITLE || (g_screen == SCR_RESULTS && (g_mode == MODE_GP || g_mode == MODE_BATTLE || g_unl_bits)) || g_screen == SCR_GP || g_screen == SCR_TROPHY || g_screen == SCR_SCENE || g_screen == SCR_CREDITS || g_screen == SCR_OPENER) pa = 1;   /* a tap anywhere carries on */
        }
        memcpy(g_tz_prev, g_tz, sizeof g_tz); g_ntz_prev = g_ntz; g_ntz = 0;
    }

    int sim_running = 1;
    switch (g_screen) {
    case SCR_TITLE:
        if (g_title_press > 0) { g_title_press += dt; if (g_title_press > 0.5f) { g_title_press = 0; g_screen = SCR_MAIN; g_screen_t = 0; g_menu_sel = 0; } break; }   /* the button shows being pressed, then the menu */
        if (pa || pb || pplus || pad->x || pad->y) { g_menu_slot = -1; for (int sl = 0; sl < npads && sl < KK_PADS; sl++) if (pads[sl].source && (pa_s[sl] || pb_s[sl] || pplus_s[sl] || pads[sl].x || pads[sl].y)) { g_menu_slot = sl; break; }
            g_title_press = 0.001f; sfx_play(SFX_MENU_OK, 1, 0); sfx_play(SFX_UNLOCK, 0.55f, 0); platform_rumble(0.5f, 0.12f); }
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
    case SCR_ONLINE:   /* Phase 1 of online play: does this Wii U reach the Kingdom Kart server? */
        net_update(dt);
        if (pa) { net_test_start(NULL, 0); sfx_play(SFX_MENU_OK, 1, 0); }
        if (pb) { net_stop(); g_screen = SCR_MAIN; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
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
            else { sfx_play(SFX_MENU_OK, 1, 0); g_multi = 0;
                static const int MODE_OF[6] = {MODE_GP, MODE_SINGLE, MODE_TT, MODE_BATTLE, MODE_VS, MODE_JERICHO}; g_mode = MODE_OF[g_menu_sel];
                /* every mode can be played together: with a Wii controller switched on (or MULTIPLAYER chosen) the players join first */
                int others = 0; for (int sl = 1; sl < npads; sl++) if (pads[sl].source) others = 1;
#ifdef KK_TOUR
                others = 0;   /* the CI tours drive the single-player screens */
#endif
                if (others || g_mode == MODE_VS) { memset(g_joined, 0, sizeof g_joined); g_njoined = 0; g_joined[0] = 1; g_njoined = 1; g_screen = SCR_JOIN; g_screen_t = 0; }
                else begin_mode_alone(); } }
        if (pb) { g_screen = SCR_TITLE; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_JOIN:
        for (int sl = 0; sl < npads; sl++) {
            /* player numbers follow the controllers' own order, not who pressed first: the GamePad is always player 1, then
               the Wii controllers in the order the Wii U numbered them when they were switched on (their player lights) */
            { int n = 0; for (int o = 0; o < KK_PADS; o++) if (g_joined[o]) g_joined[o] = ++n; }
            if (pa_s[sl] && !g_joined[sl] && g_njoined < MAX_HUMANS && pads[sl].source) { g_joined[sl] = ++g_njoined; sfx_play(SFX_MENU_OK, 1, 0); }
            else if (pb_s[sl] && g_joined[sl]) { g_joined[sl] = 0; g_njoined--; sfx_play(SFX_MENU_BACK, 1, 0); }
            else if (pb_s[sl] && g_njoined == 0) { g_mode = MODE_SINGLE; g_screen = SCR_MAIN; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
            else if (pplus_s[sl] && g_njoined <= 1 && sl == 0 && g_mode != MODE_VS) { sfx_play(SFX_MENU_OK, 1, 0); g_multi = 0; g_njoined = 0; memset(g_joined, 0, sizeof g_joined); begin_mode_alone(); break; }   /* just the GamePad: play alone */
            else if (pplus_s[sl] && g_joined[sl] && g_njoined >= 2) { sfx_play(SFX_MENU_OK, 1, 0);
                for (int o = 0; o < KK_PADS; o++) if (g_joined[o]) g_pslot[g_joined[o] - 1] = o;
                g_menu_slot = g_pslot[0];   /* player 1 of the group works the menus */
                for (int p = 0; p < g_njoined; p++) { g_pchar[p] = p % NMAIN; g_plock[p] = 0; }
                g_multi = 1; g_screen = SCR_MCHAR; g_screen_t = 0; break; } }
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
        if (all && g_screen == SCR_MCHAR && g_screen_t > 0.6f) { sfx_play(SFX_MENU_OK, 1, 0); g_player_char = g_pchar[0]; g_screen_t = 0;
            if (g_mode == MODE_GP) g_screen = SCR_CUP;                                     /* then the cup, then the races */
            else if (g_mode == MODE_JERICHO) begin_player_race();
            else if (g_mode == MODE_BATTLE) begin_battle();
            else g_screen = SCR_TRACK; }
        break; }
    case SCR_CUP: {
        int open = !CUPS[g_cup].needs_heaven || (g_save.unlocks & UNLOCK_HEAVEN);
        if (mx) { g_cup = (g_cup + mx + 2) % 2; sfx_play(SFX_MENU_MOVE, 1, 0); }
        if (pa) { if (open) { sfx_play(SFX_MENU_OK, 1, 0); g_screen_t = 0;
                if (g_multi) { memset(g_gp_points, 0, sizeof g_gp_points); memset(g_gp_kp, 0, sizeof g_gp_kp); g_gp_round = 0; for (int i = 0; i < NUM_GP_TRACKS; i++) g_gp_order[i] = CUPS[g_cup].roads[i]; begin_player_race(); }   /* together: everyone has picked already */
                else g_screen = SCR_CHAR; } else sfx_play(SFX_WRONG_WAY, 1, 0); }
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
            if (g_mode == MODE_GP) { memset(g_gp_points, 0, sizeof g_gp_points); memset(g_gp_kp, 0, sizeof g_gp_kp); g_gp_round = 0; for (int i = 0; i < NUM_GP_TRACKS; i++) g_gp_order[i] = CUPS[g_cup].roads[i]; begin_player_race(); g_screen_t = 0; }
            else if (g_mode == MODE_JERICHO) { begin_player_race(); g_screen_t = 0; } else { if (g_track_idx >= NUM_PLAY_TRACKS) { set_track(0); start_race(1); } g_screen = SCR_TRACK; g_screen_t = 0; } }
        if (pb) { g_screen = SCR_CHAR; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_TRACK:
        if (mx) { sfx_play(SFX_MENU_MOVE, 1, 0); int n = ncards(), d = card_track((track_card(g_track_idx) + mx + n) % n); g_track_idx = d; g_track_wait = g_multi ? 0 : 0.6f; }
        /* the road behind the cards changes only once the cursor rests (loading a road takes a moment; doing it on every
           move made the cursor stick); in multiplayer it does not change at all */
        if (g_track_wait > 0) { g_track_wait -= dt; if (g_track_wait <= 0 && track_unlocked(g_track_idx) && g_track_idx != g_loaded_track) { set_track(g_track_idx); start_race(1); } }
        if (pa) sfx_play(track_unlocked(g_track_idx) ? SFX_MENU_OK : SFX_WRONG_WAY, 1, 0);
        if (pa && track_unlocked(g_track_idx)) { begin_player_race(); g_screen_t = 0; }
        if (pb) { g_screen = SCR_KART; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        break;
    case SCR_RACE:
        if (pplus && r->state != RACE_FINISHED && g_screen_t > 0.5f) { g_screen = SCR_PAUSE; g_pause_sel = 0; g_screen_t = 0; g_pause_reason[0] = 0;
            g_pause_slot = -1; for (int sl = 0; sl < npads && sl < KK_PADS; sl++) if (pads[sl].source && pads[sl].plus) { g_pause_slot = sl; break; } }
        static float gone_t[MAX_HUMANS];   /* a controller that drops out for a moment (a Wii Remote does) must not pause the race: only one gone for 2 s */
        for (int p = 0; p < MAX_HUMANS; p++) { int sl = p < g_nplayers ? g_pslot[p] : -1; if (sl >= 0 && sl < npads && !pads[sl].source) gone_t[p] += dt; else gone_t[p] = 0; }
        if (g_nplayers > 1 && r->state != RACE_FINISHED && g_screen_t > 2.0f) for (int p = 0; p < g_nplayers; p++) { if (gone_t[p] > 2.0f) { gone_t[p] = 0;
            g_screen = SCR_PAUSE; g_pause_sel = 0; g_screen_t = 0; g_pause_slot = -1; snprintf(g_pause_reason, sizeof g_pause_reason, "PLAYER %d'S CONTROLLER DISCONNECTED", p + 1); break; } }
        if (r->state == RACE_FINISHED) {
            g_results_t += dt;
            if (g_results_t > 0.6f) {
                if (g_mode == MODE_GP) { race_rank(r); for (int p = 0; p < r->nk; p++) { Kart *q = &r->k[r->order[p]]; int pl = q->place ? q->place : p + 1; g_gp_kp[r->order[p]] += GP_POINTS[pl - 1]; }
                    int best = -1, bg = 0; for (int i = 0; i < r->nk; i++) if (r->given[i] > bg) { bg = r->given[i]; best = i; } if (best >= 0) g_gp_kp[best] += 2;
                    memset(g_gp_points, 0, sizeof g_gp_points); for (int i = 0; i < r->nk; i++) { int c = r->character[i]; if (g_gp_kp[i] > g_gp_points[c]) g_gp_points[c] = g_gp_kp[i]; } }   /* each character's points = their own kart's */
                if (g_mode == MODE_TT && !g_multi) { Kart *k = &r->k[r->player]; if (k->lap > g_track.laps && (g_best_time[g_track_idx] <= 0 || k->total_time < g_best_time[g_track_idx])) { g_best_time[g_track_idx] = k->total_time; if (g_track_idx < SAVE_MAX_ROADS) { g_save.best_ms[g_track_idx] = (int)(k->total_time * 1000); g_save.dirty = 1; } if (g_rec_n > 0) ghost_save(g_track_idx); } }
                else if (!g_demo && g_mode != MODE_BATTLE && g_mode != MODE_VS && !g_multi) { Kart *k = &r->k[r->player]; g_save.races++; if (k->place == 1) { g_save.wins++; g_save.win_chars |= 1u << g_player_char; } g_save.dirty = 1; }
                g_fresh_unlocks = 0;
                if (g_mode == MODE_BATTLE && !g_demo) { g_save.battles++; if (g_battle.won) g_save.battle_wins++; g_save.dirty = 1; }
                if (g_mode == MODE_JERICHO && !g_demo) { Kart *k = &r->k[r->player]; if (k->place == 1) { g_save.jericho_wins++; if (k->lap > g_track.laps && (g_save.jericho_ms == 0 || (int)(k->total_time * 1000) < g_save.jericho_ms)) g_save.jericho_ms = (int)(k->total_time * 1000); } g_save.dirty = 1; }
                if (g_mode == MODE_GP && g_gp_round + 1 >= NUM_GP_TRACKS && !g_demo) {   /* the championship is decided */
                    int best = 0; for (int c = 1; c < NCHARS; c++) if (g_gp_points[c] > g_gp_points[best]) best = c;
                    if (best == g_player_char) { g_save.gp_wins[g_difficulty]++; if (g_cup == 1) g_save.glory_wins++; g_save.dirty = 1; } }
                g_fresh_unlocks = save_check_unlocks(); save_store(); race_set_unlocked(unlocked_mask()); race_set_styles(g_player_style, unlocked_styles());
                { int hwon = 0; for (int i = 0; i < r->nk; i++) if (r->human[i] && r->k[i].place == 1) hwon = 1;
                  if (g_fresh_unlocks) sfx_play(SFX_UNLOCK, 1, 0); else if (!g_demo && hwon) sfx_play(SFX_WIN, 1, 0); }
                g_unl_bits = g_demo ? 0 : g_fresh_unlocks; g_unl_t = 0;
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
            else { if (g_mode == MODE_BATTLE || g_mode == MODE_JERICHO || g_track_idx >= NUM_PLAY_TRACKS) set_track(0); if (g_mode == MODE_VS || g_mode == MODE_JERICHO) g_mode = MODE_SINGLE; g_multi = 0; start_race(1); g_screen = SCR_MAIN; g_screen_t = 0; }
        }
        if (pb) { g_screen = SCR_RACE; g_screen_t = 0; save_store(); }
        break;
    case SCR_RESULTS:
        if (g_unl_bits) {   /* the unlock screens come first: A (or a tap) moves on once the picture is up */
            g_unl_t += dt; if ((pa || pb) && g_unl_t > 2.2f) { g_unl_bits &= g_unl_bits - 1; g_unl_t = 0; sfx_play(g_unl_bits ? SFX_UNLOCK : SFX_MENU_OK, 1, 0); g_screen_t = 0.3f; }
            break; }
        if (g_mode != MODE_GP && g_mode != MODE_BATTLE) {
            int first = results_first_choice(); if (g_res_sel < first) g_res_sel = first;
            if (mx) { g_res_sel += mx; if (g_res_sel < first) g_res_sel = 3; if (g_res_sel > 3) g_res_sel = first; sfx_play(SFX_MENU_MOVE, 1, 0); }
            if (pb) { pa = 1; g_res_sel = 3; }
            if (pa) { sfx_play(SFX_MENU_OK, 1, 0);
                if (g_res_sel == 0) {   /* the next open road, same character, straight into the race */
                    int d = g_track_idx; for (int tries = 0; tries < NUM_PLAY_TRACKS; tries++) { d = (d + 1) % NUM_PLAY_TRACKS; if (track_unlocked(d)) break; }
                    g_track_idx = d; begin_player_race(); g_screen_t = 0; }
                else if (g_res_sel == 1) { start_race(0); g_screen = SCR_RACE; g_screen_t = 0; }
                else if (g_res_sel == 2) { g_screen = SCR_TRACK; g_screen_t = 0; }
                else { set_track(0); if (g_mode == MODE_VS || g_mode == MODE_JERICHO) g_mode = MODE_SINGLE; g_multi = 0; start_race(1); g_screen = SCR_MAIN; g_screen_t = 0; }
                g_res_sel = 0; }
            break; }
        if (pa) { if (g_mode == MODE_GP) { g_gp_round++; g_screen = SCR_GP; g_screen_t = 0; } else if (g_mode == MODE_BATTLE) { begin_battle(); g_screen_t = 0; } }
        if (pb && g_mode == MODE_BATTLE) { set_track(0); g_mode = MODE_SINGLE; g_multi = 0; start_race(1); g_screen = SCR_MAIN; g_screen_t = 0; }
        break;
    case SCR_GP:
        if (pa) { if (g_gp_round >= NUM_GP_TRACKS) { g_screen = SCR_TROPHY; g_screen_t = 0; } else { begin_player_race(); g_screen_t = 0; } }
        break;
    case SCR_SCENE: scene_update(dt, pa); break;
    case SCR_TROPHY: {
        static float last_t = 99, tick_t = 0; float t0 = last_t; last_t = g_screen_t; int ord[NKARTS]; cer_order(ord); float te = cer_lock_t(0, ord);
        for (int p = 0; p < 3; p++) { float lk = cer_lock_t(p, ord); if (t0 < lk && g_screen_t >= lk) sfx_play(p == 0 ? SFX_WIN : SFX_LAND, p == 0 ? 1.0f : 0.9f, 0); }
        if (t0 < te + 0.3f && g_screen_t >= te + 0.3f) { int hv = g_voice[ord[0]][VO_WIN]; if (hv >= 0) sfx_voice_play(hv, 1.0f, 0); }
        if (t0 < te + 1.6f && g_screen_t >= te + 1.6f) sfx_play(SFX_UNLOCK, 1, 0);
        if (g_screen_t > CER_T0 && g_screen_t < te && g_screen_t - tick_t > 0.09f) { tick_t = g_screen_t; sfx_play(SFX_COIN, 0.35f, 0); }   /* the points ticking up */
        if (pa && g_screen_t > cer_end() + 3.0f) scene_start(ord[0]);   /* on to the winner's Bible story */
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
    if (in_race && !g_attract && g_mode != MODE_VS && !g_multi) {
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
    static float ipx[NKARTS][4]; static int ip_ok = 0; if (!sim_running || g_mode == MODE_BATTLE) ip_ok = 0;   /* where each kart was one step ago: the picture is drawn between the last two steps (smooth even when the frame time wobbles) */
    while (acc >= SIM_DT && steps < 8) {
        if (g_mode != MODE_BATTLE) { for (int i = 0; i < r->nk && i < NKARTS; i++) { ipx[i][0] = r->k[i].x; ipx[i][1] = r->k[i].y; ipx[i][2] = r->k[i].z; ipx[i][3] = r->k[i].yaw; } ip_ok = 1; }
        if (g_mode == MODE_BATTLE) {
            if (g_nplayers > 1) { KartInput bin[MAX_HUMANS]; for (int p = 0; p < g_nplayers; p++) bin[p] = hin[human_kart(p)]; battle_step(&g_battle, bin, 0, SIM_DT); }
            else battle_step(&g_battle, &in, g_demo || g_attract, SIM_DT);
            for (int e = 0; e < g_battle.nev; e++) switch (g_battle.ev[e]) {
                case BEV_STONE_PICKED: sfx_play(SFX_PICKUP, 0.9f, 0); snprintf(g_msg, sizeof g_msg, "A SMOOTH STONE!"); snprintf(g_msg2, sizeof g_msg2, "PRESS ZL TO SLING IT - 1 SAMUEL 17:40"); g_msg_t = 1.2f; break;
                case BEV_EMPTY: snprintf(g_msg, sizeof g_msg, "NO STONES!"); snprintf(g_msg2, sizeof g_msg2, "DRIVE TO THE GLOWING STONES IN THE BROOK"); g_msg_t = 1.4f; sfx_play(SFX_WRONG_WAY, 0.6f, 0); break;
                case BEV_STONE_THROWN: sfx_play(SFX_THROW, 1, 0); break;
                case BEV_SHIELD: snprintf(g_msg, sizeof g_msg, "THE SHIELD-BEARER!"); snprintf(g_msg2, sizeof g_msg2, "HIT HIM FROM THE SIDE - 1 SAMUEL 17:41"); g_msg_t = 1.2f; sfx_play(SFX_BLOCK, 1, 0); break;
                case BEV_GOLIATH_HIT: snprintf(g_msg, sizeof g_msg, "RIGHT ON HIS HEAD!"); snprintf(g_msg2, sizeof g_msg2, "%d OF %d", g_battle.hits_on_goliath, battle_need(&g_battle)); g_msg_t = 0.9f; sfx_play(SFX_HIT, 1, 0); sfx_play(SFX_BUMP, 0.8f, 0); add_dust(v3(g_battle.gx, g_battle.gy + battle_head_y(&g_battle), g_battle.gz), 2, 2); break;
                case BEV_ARMOUR: snprintf(g_msg, sizeof g_msg, "CLANG!"); snprintf(g_msg2, sizeof g_msg2, "HIS ARMOUR - AIM FOR HIS HEAD"); g_msg_t = 0.9f; sfx_play(SFX_BLOCK, 0.9f, 0); break;
                case BEV_DAZED: snprintf(g_msg, sizeof g_msg, "HE IS REELING!"); snprintf(g_msg2, sizeof g_msg2, "ONE STONE TO HIS FOREHEAD - NOW!"); g_msg_t = 2.5f; sfx_play(SFX_SHOUT, 0.8f, 0); platform_rumble(0.6f, 0.3f); break;
                case BEV_RECOVER: snprintf(g_msg, sizeof g_msg, "HE SHOOK IT OFF!"); g_msg_t = 1.5f; break;
                case BEV_FOREHEAD: snprintf(g_msg2, sizeof g_msg2, "THE STONE SUNK INTO HIS FOREHEAD - 1 SAMUEL 17:49"); g_slowmo = 1.6f; g_shake = fmaxf(g_shake, 1.0f); platform_rumble(1.0f, 0.6f); sfx_play(SFX_CRUMBLE, 1, 0); break;
                case BEV_DAVID_HIT: snprintf(g_msg, sizeof g_msg, "OUCH!"); g_msg_t = 0.8f; sfx_play(SFX_HIT, 1, 0); platform_rumble(1.0f, 0.45f); break;
                case BEV_SPEAR: sfx_play(SFX_THROW, 0.7f, 0); break;
                case BEV_ROAR: sfx_play(SFX_WHALE_ROAR, 0.7f, 0); break;   /* a giant's roar */
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
        for (int e = 0; e < r->nev; e++) drv_event(r, &r->ev[e]);
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
                case EV_WHALE_WARN: if (who >= 0) { pmsg(who, 1.6f, "THE GREAT FISH!"); pmsg2(who, 1.6f, "GET OUT OF THE WATER!"); prumble(who, 0.3f, 0.6f); } sfx_play(SFX_SPLASH, who >= 0 ? 0.6f : 0.25f, 0); break;
                case EV_WHALE_UP: { float d2 = 1e9f; for (int p = 0; p < g_nplayers; p++) { const Kart *hk = &r->k[human_kart(p)]; float dx = hk->x - r->whale.x, dz = hk->z - r->whale.z; d2 = fminf(d2, dx * dx + dz * dz); }
                    float nearv = clampf(1.2f - sqrtf(d2) / 80.0f, 0, 1); nearv = fmaxf(nearv, 0.55f);   /* (Sean) a loud roar you hear from anywhere on the sea */
                    if (nearv > 0) { sfx_play(SFX_WHALE_ROAR, 1.0f, 0); sfx_play(SFX_SPLASH, nearv, 0); for (int p = 0; p < g_nplayers; p++) g_view[p].shake = fmaxf(g_view[p].shake, 0.5f * nearv); } } break;
                case EV_SWALLOWED: if (ek) for (int n = 0; n < 16; n++) add_dust(v3(ek->x + (frand01() - 0.5f) * 3, ek->y, ek->z + (frand01() - 0.5f) * 3), 3.0f, 5.0f);
                    sfx_play(SFX_HIT, who >= 0 ? 1.0f : 0.4f, 0); if (who >= 0) { pmsg(who, 2.2f, "SWALLOWED!"); pmsg2(who, 2.2f, "INSIDE THE GREAT FISH - JONAH 1:17"); prumble(who, 0.8f, 0.5f); } break;
                case EV_SPAT_OUT: if (ek) for (int n = 0; n < 12; n++) add_dust(v3(ek->x, ek->y, ek->z), 2.5f, 4.0f);
                    sfx_play(SFX_SPLASH, who >= 0 ? 0.9f : 0.3f, 0); if (who >= 0) { pmsg(who, 1.8f, "OUT ON DRY LAND!"); pmsg2(who, 1.8f, "JONAH 2:10"); } break;
                case EV_GEM: if (!ek) break; bless_sparkle(ek, 0.6f, 1.0f, 0.9f); bless_sparkle(ek, 1, 0.85f, 0.3f); sfx_at(SFX_UNLOCK, ek, 0.9f); sfx_at(SFX_BOOST_PAD, ek, 0.6f);
                    if (who >= 0) { g_coin_pop[who] = 0.6f; pmsg(who, 1.4f, "HIDDEN GEM!"); pside(who, 1.4f, "+3 COINS AND A BOOST"); prumble(who, 0.4f, 0.25f); } break;
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
            (void)q;
        }
    }
    if (steps == 8) acc = 0;
    Kart *k = &r->k[r->player];
    if (sim_running) for (int i = 0; i < MAX_RING; i++) if (g_ring[i].t > 0) g_ring[i].t -= dt;
    if (r->jericho && sim_running && r->collapse_t > 0) {   /* the walls of Jericho fall: starting by your own kart and running away round the city */
        int first = 1;
        for (int i = 0; i < g_nplaced; i++) { Placed *pl = &g_placed[i]; if (!pl->wall) continue;
            float dx = pl->p.x - k->x, dz = pl->p.z - k->z, d = sqrtf(dx * dx + dz * dz);
            float due = fminf(d / 90.0f, 2.6f);   /* the ripple travels 90 m a second, so the far side has fallen within three seconds */
            if (pl->fall_t <= 0 && r->collapse_t >= due) { pl->fall_t = 0.001f;
                for (int n = 0; n < 6; n++) add_dust(v3(pl->p.x + (frand01() - 0.5f) * pl->w, pl->p.y + frand01() * pl->h * 0.6f, pl->p.z), 3.0f, 2.5f);
                if (first && d < 120.0f) { first = 0; sfx_play(SFX_CRUMBLE, clampf(1.3f - d / 120.0f, 0.4f, 1.0f), 0); } }   /* one crash per frame at most, only for walls you can see */
            else if (pl->fall_t > 0 && pl->fall_t < 3.0f) { float before = pl->fall_t; pl->fall_t += dt;
                if (before < 1.1f && pl->fall_t >= 1.1f) {   /* it hits the ground: a cloud of dust along its length, and the ground shakes */
                    for (int n = 0; n < 14; n++) add_dust(v3(pl->p.x + (frand01() - 0.5f) * pl->w * 1.4f, pl->p.y + 0.4f, pl->p.z + (frand01() - 0.5f) * pl->w * 0.8f), 4.0f, 3.0f);
                    float near = clampf(1.2f - d / 100.0f, 0.0f, 1.0f); for (int p = 0; p < g_nplayers; p++) g_view[p].shake = fmaxf(g_view[p].shake, 0.6f * near); if (near > 0.5f) prumble(0, near * 0.7f, 0.35f); }
                else if (pl->fall_t < 1.1f && frand01() < 0.4f) add_dust(v3(pl->p.x + (frand01() - 0.5f) * pl->w, pl->p.y + 0.3f, pl->p.z), 3.0f, 2.0f); } }
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

    /* draw the karts where they are between the last two steps of the simulation */
    float ipcur[NKARTS][4]; int ip_n = 0;
    if (ip_ok && sim_running && g_slowmo <= 0) { float al = clampf(acc / SIM_DT, 0, 1); ip_n = r->nk < NKARTS ? r->nk : NKARTS;
        for (int i = 0; i < ip_n; i++) { Kart *q = &r->k[i]; ipcur[i][0] = q->x; ipcur[i][1] = q->y; ipcur[i][2] = q->z; ipcur[i][3] = q->yaw;
            float dx = q->x - ipx[i][0], dz = q->z - ipx[i][2]; if (dx * dx + dz * dz > 25.0f) continue;   /* a jump back onto the road: no blending */
            float dy = q->yaw - ipx[i][3]; while (dy > 3.14159265f) dy -= 6.2831853f; while (dy < -3.14159265f) dy += 6.2831853f;
            q->x = ipx[i][0] + dx * al; q->y = ipx[i][1] + (q->y - ipx[i][1]) * al; q->z = ipx[i][2] + dz * al; q->yaw = q->yaw - dy * (1 - al); } }
    /* every player's camera */
    for (int p = 0; p < g_nplayers; p++) { cv = &g_view[p]; k = &r->k[human_kart(p)];
    if (cv->finish_t > 0) cv->finish_t += dt;   /* after the finish: the celebration, the camera all the way round the kart */
    /* the race intro: during the countdown the camera sweeps around the grid and settles in behind the player */
    if (r->state == RACE_COUNTDOWN && !g_attract && g_mode != MODE_BATTLE && r->cd > 0.6f) {
        float u = clampf((3.5f + RACE_READY_T - r->cd) / (2.9f + RACE_READY_T), 0, 1), e = u * u * (3 - 2 * u);         /* 0 at the start of the countdown, 1 just before GO */
        float a = k->yaw + 3.14159f * (1.0f - e) * 0.9f + 0.6f * (1.0f - e);               /* starts in front and to the side, swings round to behind */
        float dist = 9.0f - 4.4f * e, hgt = 4.5f - 1.8f * e;
        cam_yaw = k->yaw; cam_x = k->x - sinf(a) * dist; cam_z = k->z - cosf(a) * dist; cam_y = k->y + hgt; cam_fov = 58.0f + 4.0f * e;
    } else if (cv->finish_t > 0 && !g_attract && g_mode != MODE_BATTLE) {
        /* the celebration: in close and low, swinging once all the way round the kart, then drifting on slowly */
        float u = clampf(cv->finish_t / CELE_T, 0, 1), e = u * u * (3 - 2 * u), a = k->yaw + 6.2831853f * e + (cv->finish_t > CELE_T ? (cv->finish_t - CELE_T) * 0.4f : 0);
        float in = clampf(cv->finish_t / 0.9f, 0, 1), dist = 6.0f - 2.4f * in, hgt = 2.6f - 0.9f * in;
        float tx = k->x - sinf(a) * dist, tz = k->z - cosf(a) * dist, ty = k->y + hgt;
        float f = fminf(1.0f, 8.0f * dt); cam_x += (tx - cam_x) * f; cam_z += (tz - cam_z) * f; cam_y += (ty - cam_y) * f;
        cam_yaw = atan2f(k->x - cam_x, k->z - cam_z); cam_fov += (55.0f - cam_fov) * fminf(1.0f, 3.0f * dt);
        if ((int)(cv->finish_t * 2.5f) != (int)((cv->finish_t - dt) * 2.5f) && cv->finish_t < CELE_T) { const Character *ch = &CHARACTERS[r->character[human_kart(p)]]; bless_sparkle(k, 1, 0.85f, 0.3f); bless_sparkle(k, ch->r, ch->g, ch->b); }   /* confetti */
    } else {
    /* chase camera: sits behind the kart, lags on turns, pulls back with speed */
    int orbiting = g_nplayers > 1 ? (cv->finish_t > 0 && cv->finish_t < 1.2f) : (g_slowmo > 0);
    float want_yaw = k->yaw + k->slide * 0.3f + (orbiting ? g_orbit : 0);
    float dy = want_yaw - cam_yaw; while (dy > 3.14159f) dy -= 6.28318f; while (dy < -3.14159f) dy += 6.28318f;
    cam_yaw += dy * fminf(1.0f, 5.0f * dt);
    float sp = fminf(fmaxf(k->speed, 0), 26.0f);   /* past cruising speed (boosts) the camera stays put, so the racers stay big on screen (Sean) */
    float back = 4.6f + sp * 0.07f, up = 2.7f + sp * 0.015f;   /* close when stopped (the kart behind on the grid is 5 m back), a little further back at speed */
    float tx = k->x - sinf(cam_yaw) * back, tz = k->z - cosf(cam_yaw) * back, ty = k->y + up;
    cam_x += (tx - cam_x) * fminf(1.0f, 22.0f * dt); cam_z += (tz - cam_z) * fminf(1.0f, 22.0f * dt); cam_y += (ty - cam_y) * fminf(1.0f, 6.0f * dt);
    float want_fov = 62.0f + (k->boost_t > 0 ? 3.0f : 0.0f) + sp * 0.1f;
    cam_fov += (want_fov - cam_fov) * fminf(1.0f, 4.0f * dt);
    }
    if (g_shake > 0) g_shake = fmaxf(0, g_shake - dt * 1.5f);
    }
    cv = &g_view[0]; k = &r->k[r->player];
    /* ---- draw both screens */
    int split = g_nplayers > 1 && (g_screen == SCR_RACE || g_screen == SCR_PAUSE);
    if (!split) { cv = &g_view[0]; build_dynamic(); }
    gfx_frame_begin();
    if (g_status == GFX_OK && g_def && (g_screen == SCR_RACE || g_screen == SCR_PAUSE || g_screen == SCR_RESULTS)) draw_shadow_casters(); else gfx_shadow_none();
    for (int drc = 0; drc < 2; drc++) {
        if (g_status == GFX_NO_COMPILER) { gfx_view_begin(drc, 1.0f, 0.5f, 0.0f); gfx_view_end(drc); continue; }
        if (g_status == GFX_SHADER_FAILED) { gfx_view_begin(drc, 0.8f, 0.05f, 0.05f); gfx_view_end(drc); continue; }
        int gamepad_map = drc && (g_screen == SCR_RACE || g_screen == SCR_PAUSE) && !g_attract;
#ifdef KK_TOUR
        if (!drc && g_time > 98.0f && g_time < 103.0f && g_screen == SCR_RACE && !split) gamepad_map = 1;   /* the CI camera only sees the TV: show the GamePad layout there for one photo */
#endif
        if (gamepad_map && g_pad_race && drc) {   /* the GamePad shows the race: the TV's finished picture, copied over (no second drawing of the race) */
            gfx_view_begin(drc, 0, 0, 0); Texture *tv = gfx_grab_ex(0, drc); gfx_viewport(drc, 0, 0, 1, 1); gfx_camera_2d(); gfx_state(ST_HUD); Mat4 id2 = m4_identity();
            batch_init(&g_b, g_vtmp, 16384); batch_clear(&g_b);
            static Mesh *m_padview; if (!m_padview) { batch_rect_uv(&g_b, 0, 0, 1280, 720, 0, 0, 1, 1, 1, 1, 1, 1); m_padview = gfx_mesh_static(g_b.v, g_b.n); batch_clear(&g_b); }
            if (tv) { gfx_bind(tv); gfx_draw_ex(m_padview, &id2, 1, 1, 1, 1, 0, 1, 0, 0); }
            else { batch_rect(&g_b, 0, 0, 1280, 720, 0.04f, 0.07f, 0.18f, 1); text_c("LOOK AT THE TV", 340, 30, 1, 0.85f, 0.3f, 1); }
            pad_corner_button("MAP");   /* tap: the big map instead */
            gfx_mesh_update(m_hud2, g_b.v, g_b.n); gfx_bind(tx_font); gfx_draw(m_hud2, &id2, 1, 1, 1, 1); gfx_view_end(drc); continue; }
        if (gamepad_map) { gfx_view_begin(drc, 0.04f, 0.07f, 0.18f); gfx_camera_2d(); gfx_state(ST_HUD); Mat4 id2 = m4_identity();
            batch_init(&g_b, g_vtmp, 16384); batch_clear(&g_b); hud_gamepad(); if (g_screen == SCR_PAUSE) hud_pause(); else pad_corner_button("RACE"); gfx_mesh_update(m_hud2, g_b.v, g_b.n); gfx_bind(tx_font); gfx_draw(m_hud2, &id2, 1, 1, 1, 1); gfx_view_end(drc); continue; }
        if (g_screen == SCR_OPENER) { gfx_view_begin(drc, 0, 0, 0); gfx_camera_2d(); gfx_state(ST_HUD); Mat4 id2 = m4_identity();
            { Texture *vt = g_op_vid ? vid_texture() : tx_op; if (vt && m_op) { gfx_bind(vt); gfx_draw(m_op, &id2, 1, 1, 1, 1); } }
            gfx_view_end(drc); continue; }
        if (split) {   /* the TV in pieces: one view per player, each with its own HUD; the whole-screen pause menu on top */
            gfx_view_begin(drc, 0.02f, 0.03f, 0.08f); Mat4 id = m4_identity();
            for (int p = 0; p < g_nplayers; p++) { float fx, fy, fw, fh; split_rect(p, g_nplayers, &fx, &fy, &fw, &fh);
                gfx_viewport(drc, fx, fy, fw, fh); cv = &g_view[p]; build_dynamic();
                float vw = fw * 1280.0f, vh = fh * 720.0f;
                draw_scene(human_kart(p), vw / vh, g_nplayers == 2 ? 0.62f : 1.0f, 1);
                if (!drc) draw_boost_blur(drc, p, fx, fy, fw, fh, vw, vh);
                gfx_camera_2d_virtual(vw, vh); gfx_state(ST_HUD); batch_clear(&g_b);
                if (g_mode == MODE_BATTLE) hud_battle_split(p, vw, vh); else hud_split(p, vw, vh);
                gfx_mesh_update(m_hud, g_b.v, g_b.n); gfx_bind(tx_font); gfx_draw(m_hud, &id, 1, 1, 1, 1);
                int it = r->k[human_kart(p)].item;
                if (it >= 0 && it < ITEM_COUNT && !r->jericho && g_mode != MODE_BATTLE && g_mode != MODE_TT) { float sc = vw < 1000 ? 0.75f : 1.0f; draw_item_window(p, 16, vh - 64 * sc - 14, 64 * sc, vw, vh); } }
            if (g_nplayers == 3) { gfx_viewport(drc, 0.5f, 0.5f, 0.5f, 0.5f); gfx_camera_2d_virtual(640, 360); gfx_state(ST_HUD); batch_clear(&g_b); hud_quarter_map(); gfx_mesh_update(m_hud, g_b.v, g_b.n); gfx_bind(tx_font); gfx_draw(m_hud, &id, 1, 1, 1, 1); }
            gfx_viewport(drc, 0, 0, 1, 1); gfx_camera_2d(); gfx_state(ST_HUD); batch_clear(&g_b);
            batch_rect(&g_b, 0, 358, 1280, 4, 0.02f, 0.03f, 0.08f, 1); if (g_nplayers >= 3) batch_rect(&g_b, 638, 0, 4, 720, 0.02f, 0.03f, 0.08f, 1);   /* the bars between the views */
            if (g_screen == SCR_PAUSE) hud_pause();
            if (g_screen_t < 0.3f && g_screen == SCR_RACE) batch_rect(&g_b, 0, 0, 1280, 720, 0, 0, 0, 1.0f - g_screen_t / 0.3f);
            gfx_mesh_update(m_hud2, g_b.v, g_b.n); gfx_bind(tx_font); gfx_draw(m_hud2, &id, 1, 1, 1, 1);
            cv = &g_view[0]; gfx_view_end(drc); continue; }
        cv = &g_view[0];
        /* on the GamePad the menus sit on a plain background: drawing the 3D road behind them a second time halved the
           frame rate of every menu (the log from the real Wii U: 30 a second instead of 60) */
        int menu_bg = g_screen == SCR_TITLE || g_screen == SCR_MAIN || g_screen == SCR_CHAR || g_screen == SCR_KART || g_screen == SCR_TRACK || g_screen == SCR_JOIN || g_screen == SCR_MCHAR || g_screen == SCR_CUP || g_screen == SCR_OPTIONS || g_screen == SCR_CREDITS || g_screen == SCR_GP;
        int plain_bg = g_screen == SCR_SCENE || (drc && menu_bg);   /* the GamePad's menus show the TV's moving race behind them (copied, not drawn twice) */
        if (plain_bg) gfx_view_begin(drc, 0.06f, 0.09f, 0.24f); else gfx_view_begin(drc, g_def->sky_r, g_def->sky_g, g_def->sky_b);
        Mat4 id = m4_identity();
        if (!plain_bg) draw_scene(g_mode == MODE_BATTLE ? 0 : r->player, gfx_aspect(drc), 1.0f, 0);
        { static Texture *menubg; static Mesh *m_menubg;
          if (!drc && menu_bg) menubg = gfx_grab_ex(0, 0);   /* the TV's race behind the menu, before the menu is drawn on it */
          if (drc && menu_bg && menubg) { if (!m_menubg) { static Vertex mv[6]; Batch mb; batch_init(&mb, mv, 6); batch_rect_uv(&mb, 0, 0, 1280, 720, 0, 0, 1, 1, 1, 1, 1, 1); m_menubg = gfx_mesh_static(mb.v, mb.n); }
              gfx_viewport(drc, 0, 0, 1, 1); gfx_camera_2d(); gfx_state(ST_HUD); gfx_bind(menubg); gfx_draw_ex(m_menubg, &id, 1, 1, 1, 1, 0, 1, 0, 0); } }
        if (!plain_bg && g_mode != MODE_BATTLE && g_screen == SCR_RACE) draw_boost_blur(drc, 0, 0, 0, 1, 1, 1280, 720);
        /* ---- HUD and menus */
        gfx_camera_2d(); gfx_state(ST_HUD);
        batch_init(&g_b, g_vtmp, 32768); batch_clear(&g_b);
        { /* a touch of the road's color over the whole picture, and darker corners */
          static Mesh *m_tone = NULL; if (!m_tone) { batch_rect_uv(&g_b, 0, 0, 1280, 720, 0, 0, 1, 1, 1, 1, 1, 1); m_tone = gfx_mesh_static(g_b.v, g_b.n); batch_clear(&g_b); }
          gfx_bind(NULL); gfx_draw(m_tone, &id, g_def->fog_r, g_def->fog_g, g_def->fog_b, 0.07f);
          gfx_bind(tx_vignette); gfx_draw(m_tone, &id, 1, 1, 1, 1); }
        int menu_screen = g_screen == SCR_TITLE || g_screen == SCR_MAIN || g_screen == SCR_CHAR || g_screen == SCR_KART || g_screen == SCR_TRACK || g_screen == SCR_GP || g_screen == SCR_JOIN || g_screen == SCR_MCHAR || g_screen == SCR_CUP;
        if (menu_screen) batch_rect(&g_b, 0, 0, 1280, 720, 0.02f, 0.03f, 0.1f, g_screen == SCR_TITLE ? 0.25f : 0.45f);
        if (menu_screen && g_screen != SCR_TITLE) ui_backdrop();
        g_tz_rec = !drc; g_ui3d = 1; g_mi = 0;
        uint32_t smk = ui_begin();
        switch (g_screen) {
            case SCR_TITLE: hud_title(); break;
            case SCR_MAIN: hud_main(); break;
            case SCR_CREDITS: hud_credits(); break;
            case SCR_ONLINE: hud_online(); break;
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
        { /* the whole menu swings in when it opens, and leans a little with the stick */
          static float tlx = 0, tly = 0; tlx += (g_prev.sx - tlx) * 0.08f; tly += (g_prev.sy - tly) * 0.08f;
          int menu3d = menu_screen && g_screen != SCR_TITLE; float e = ui_ease(g_screen_t / 0.45f);
          g_ui_screen = menu3d ? ui_xf(640, 380, -(1 - e) * 260, (1 - e) * 0.32f + tly * 0.035f, tlx * 0.06f, 0, 1) : m4_identity();
          ui_end(smk, &g_ui_screen); }
        if (drc && (g_screen == SCR_MAIN || g_screen == SCR_CUP || g_screen == SCR_CHAR || g_screen == SCR_KART || g_screen == SCR_TRACK || g_screen == SCR_OPTIONS || g_screen == SCR_JOIN || g_screen == SCR_MCHAR)) back_button();
        else if (!drc && (g_screen == SCR_MAIN || g_screen == SCR_CUP || g_screen == SCR_CHAR || g_screen == SCR_KART || g_screen == SCR_TRACK || g_screen == SCR_OPTIONS || g_screen == SCR_JOIN || g_screen == SCR_MCHAR)) tzone(10, 640, 175, 75, TZ_B, 0);
        g_tz_rec = 0;
        if (g_screen == SCR_TITLE && tx_title_bg && m_title_bg) { float u = g_screen_t < 0.7f ? g_screen_t / 0.7f : 1.0f; gfx_bind(tx_title_bg); gfx_draw(m_title_bg, &id, 1, 1, 1, u); draw_title_button(&id, u); }
        else if (g_screen == SCR_TITLE && tx_title && m_title) { float u = g_screen_t < 0.7f ? g_screen_t / 0.7f : 1.0f; float ease = 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u);
            gfx_bind(tx_title); gfx_draw(m_title, &id, 1, 1, 1, ease); }
        /* a quick fade from black when a new screen opens */
        if (g_screen_t < 0.3f && (g_screen == SCR_RACE || g_screen == SCR_RESULTS || g_screen == SCR_TITLE || g_screen == SCR_CREDITS || g_screen == SCR_GP || g_screen == SCR_TROPHY)) batch_rect(&g_b, 0, 0, 1280, 720, 0, 0, 0, 1.0f - g_screen_t / 0.3f);
        ui_flush(); gfx_camera_ui3d(UI_EYE);
        gfx_mesh_update(m_hud, g_b.v, g_b.n);
        gfx_bind(tx_font); gfx_draw(m_hud, &id, 1, 1, 1, 1);
        g_ui3d = 0; gfx_camera_2d();
        if (g_screen == SCR_MAIN && tx_title && m_logo_small) { gfx_bind(tx_title); gfx_draw(m_logo_small, &id, 1, 1, 1, 1); }
        if (g_screen == SCR_CHAR || g_screen == SCR_MCHAR) draw_portraits();
        if (g_screen == SCR_RESULTS && g_mode != MODE_TT && g_mode != MODE_BATTLE) draw_podium_karts();
        if (g_screen == SCR_SCENE) { draw_story_scene(drc); static Mesh *m_sct[2]; if (!m_sct[drc]) m_sct[drc] = gfx_mesh_dynamic(16384);   /* the words on solid 3D slabs */
            batch_clear(&g_b); g_ui3d = 1; g_ui_conv = 0; hud_scene_top(); ui_flush(); g_ui3d = 0; gfx_mesh_update(m_sct[drc], g_b.v, g_b.n);
            gfx_camera_ui3d(UI_EYE); gfx_state(ST_HUD); gfx_bind(tx_font); gfx_draw(m_sct[drc], &id, 1, 1, 1, 1); gfx_camera_2d(); }
        if (g_screen == SCR_TROPHY) { draw_trophy_pictures(drc); static Mesh *m_top[2]; if (!m_top[drc]) m_top[drc] = gfx_mesh_dynamic(8192); batch_clear(&g_b); hud_trophy_top(); gfx_mesh_update(m_top[drc], g_b.v, g_b.n); gfx_bind(tx_font); gfx_draw(m_top[drc], &id, 1, 1, 1, 1); }
        if (g_screen == SCR_TRACK) for (int c2 = 0; c2 < ncards() && c2 < 8; c2++) { int d = card_track(c2); if (d >= 8 || !tx_card[d] || !track_unlocked(d)) continue;
            if (!m_card[c2]) m_card[c2] = gfx_mesh_dynamic(6);
            { float x = 40 + c2 * CARD_STEP + 12, y = 110 + 12; batch_clear(&g_b); batch_rect_uv(&g_b, x, y, CARD_IN, CARD_IN * 0.54f, 0, 0, 1, 1, 1, 1, 1, 1); ui_px(g_b.v, g_b.n, 7); gfx_mesh_update(m_card[c2], g_b.v, g_b.n); }
            gfx_camera_ui3d(UI_EYE); Mat4 cm = ui_card_model(c2); gfx_bind(tx_card[d]); gfx_draw(m_card[c2], &cm, 1, 1, 1, 1); gfx_camera_2d(); }
        if (g_screen == SCR_KART) { draw_kart_models(); gfx_camera_2d(); gfx_state(ST_HUD);
            {   /* the character in the very kart they race in, turning slowly on a glowing stand under a spotlight */
                int ch = g_player_char; static Mesh *m_tt[2], *m_ttfx[2]; if (!m_tt[drc]) { m_tt[drc] = gfx_mesh_dynamic(6); m_ttfx[drc] = gfx_mesh_dynamic(40 * 6); }
                float ang = g_time * 0.9f; int oct = ((int)floorf(ang / 0.7853982f + 0.5f)) & 7;   /* eight directions: back, back-left, left, front-left, front, then the left ones mirrored */
                static const int view8[8] = {4, 3, 2, 1, 0, 1, 2, 3}; static const int mir8[8] = {0, 0, 0, 0, 0, 1, 1, 1};
                int v = view8[oct], mir = mir8[oct]; if (!tx_ride[ch][v]) { v = 3; mir = 0; } Texture *rt = tx_ride[ch][v] ? tx_ride[ch][v] : tx_kart[ch]; float a2 = tx_ride[ch][v] ? g_ride_aspect[ch][v] : g_kart_aspect[ch];
                float frac = ang / 0.7853982f - floorf(ang / 0.7853982f + 0.5f), squash = 1.0f - 0.12f * fabsf(frac);   /* a little squeeze as it turns between pictures */
                float ph = 205, pw = ph * a2 * squash, cx = 640, by = 282 + 3 * sinf(g_time * 2.2f);
                batch_init(&g_b, g_vtmp, 8192); batch_clear(&g_b);
                q2d(&g_b, v3(640, 150, 0), v3(170, 0, 0), v3(0, 150, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 0.9f, 0.6f, 0.35f);   /* the spotlight */
                for (int k = 0; k < 3; k++) q2d(&g_b, v3(640, 290, 0), v3(230 - k * 50, 0, 0), v3(0, 26 - k * 6, 0), v3(0, 0, 1), 0, 0, 1, 1, 0.5f + k * 0.2f, 0.75f, 1, 0.5f);   /* the stand's glow */
                for (int k = 0; k < 24; k++) { float h1 = noise2(k, 77), ph2 = fmodf(g_time * 0.35f + h1, 1.0f), a3 = h1 * 6.28f + g_time * 0.8f;   /* motes of light rising round it */
                    q2d(&g_b, v3(640 + cosf(a3) * 210, 290 - ph2 * 220, 0), v3(5, 0, 0), v3(0, 5, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 0.9f, 0.6f, sinf(ph2 * 3.14159f)); }
                gfx_mesh_update(m_ttfx[drc], g_b.v, g_b.n); gfx_state(ST_HUD | ST_ADD); gfx_bind(tx_blob); gfx_draw(m_ttfx[drc], &id, 1, 1, 1, 1); gfx_state(ST_HUD);
                if (g_k3d[ch].ok) {   /* the real 3D kart turning on the stand */
                    gfx_clear_depth(drc); float aspect = drc ? 854.0f / 480.0f : 1280.0f / 720.0f;
                    Mat4 vp = m4_mul(m4_perspective(0.55f, aspect, 0.5f, 80.0f), m4_look_at(v3(0, 3.6f, 15.5f), v3(0, 0.9f, 0), v3(0, 1, 0)));
                    /* the turntable sits in the window between the title and the cards: shift the picture up to it */
                    Mat4 shift = m4_identity(); shift.m[13] = 0.36f; vp = m4_mul(shift, vp);
                    gfx_light(v3(0.85f, 0.82f, 0.78f), v3(0.75f, 0.8f, 0.9f), v3(0.45f, 0.4f, 0.38f), v3(0, 3.6f, 15.5f));   /* plain studio light, not the road's */
                    gfx_camera(&vp, v3(0.4f, 0.8f, 0.6f), 0.55f, 0, 0, 0, 1e6f, 2e6f); gfx_state(ST_DEPTH | ST_DEPTH_WRITE);
                    Mat4 m = m4_mul(m4_translate(0, 0.35f + 0.05f * sinf(g_time * 2.2f), 0), m4_mul(m4_rotate_y(g_time * 0.8f), m4_scale(0.9f, 0.9f, 0.9f)));   /* up clear of the words under it */
                    { K3Pose tp; memset(&tp, 0, sizeof tp); tp.t = g_time;   /* the driver looks at you and waves as the stand turns round */
                      float ang = fmodf(-g_time * 0.8f, 6.2831853f); if (ang < -3.14159f) ang += 6.2831853f; if (ang > 3.14159f) ang -= 6.2831853f;
                      tp.look_yaw = clampf(ang, -1.3f, 1.3f); tp.twist = clampf(ang * 0.25f, -0.3f, 0.3f); tp.look_pitch = 0.08f;
                      float wv = fabsf(ang) < 1.3f ? clampf((1.3f - fabsf(ang)) * 3.0f, 0, 1) : 0; tp.arm[ang > 0 ? 0 : 1] = K3A_WAVE; tp.arm_w[ang > 0 ? 0 : 1] = wv;
                      kart3d_draw_pose(&g_k3d[ch], &m, g_time * 6.0f, 0.3f * sinf(g_time * 0.9f), &tp, 1, 1, 1, 1); }
                    gfx_camera_2d(); gfx_state(ST_HUD); rt = NULL; }
                if (rt) { batch_clear(&g_b); if (mir) batch_rect_uv(&g_b, cx - pw / 2, by - ph, pw, ph, 1, 0, 0, 1, 1, 1, 1, 1); else batch_rect_uv(&g_b, cx - pw / 2, by - ph, pw, ph, 0, 0, 1, 1, 1, 1, 1, 1);
                    gfx_mesh_update(m_tt[drc], g_b.v, g_b.n); gfx_bind(rt); gfx_draw(m_tt[drc], &id, 1, 1, 1, 1); } } }
#if defined(KK_TOUR) && KK_TOUR_ROAD == 2
        if ((g_screen == SCR_RACE || g_screen == SCR_PAUSE) && g_race.race_t < 5 && gfx_shadow_texture()) {   /* the CI photos: the sun's shadow map itself, top right */
            static Mesh *m_dbg = NULL; if (!m_dbg) m_dbg = gfx_mesh_dynamic(6); Vertex dv[6]; Batch db; batch_init(&db, dv, 6); batch_rect_uv(&db, 1000, 120, 260, 260, 0, 0, 1, 1, 1, 1, 1, 1);
            gfx_mesh_update(m_dbg, db.v, db.n); gfx_camera_2d(); gfx_state(ST_HUD); gfx_bind(gfx_shadow_texture()); gfx_draw_ex(m_dbg, &id, 1, 1, 1, 1, 0, 1, 0, 0); gfx_bind(NULL); }
#endif
#if defined(KK_TOUR) && KK_TOUR_ROAD == 2
        if ((g_screen == SCR_RACE || g_screen == SCR_PAUSE) && g_race.race_t > 5) { g_unl_bits = g_race.race_t > 12 ? UNLOCK_KART_CHARIOT : UNLOCK_SAMSON; g_unl_t = g_race.race_t - (g_race.race_t > 12 ? 10 : 5); draw_unlock(&id, drc); }   /* the CI photos of the unlock screen */
#endif
        if (g_screen == SCR_RESULTS && g_unl_bits) draw_unlock(&id, drc);
        if ((g_screen == SCR_RACE || g_screen == SCR_PAUSE) && g_mode != MODE_TT) { int it = r->k[r->player].item;
            if (it >= 0 && it < ITEM_COUNT && !r->jericho && g_mode != MODE_BATTLE) draw_item_window(0, 44, 118, 104, 1280, 720); }
        gfx_view_end(drc);
    }
    gfx_frame_end();
    for (int i = 0; i < ip_n; i++) { Kart *q = &r->k[i]; q->x = ipcur[i][0]; q->y = ipcur[i][1]; q->z = ipcur[i][2]; q->yaw = ipcur[i][3]; }   /* back to the true positions for the next step */
}
int game_debug_screen(void) { return g_screen + 20 * g_mode + 100 * g_menu_sel + 1000 * (int)g_race.state; }
const char *game_debug_track(void) { return g_track.name; }

const char *game_where(void) {   /* for the frame-rate lines in the log, so a slow spot can be found */
    static char buf[96]; int in_race = g_screen == SCR_RACE || g_screen == SCR_PAUSE;
    snprintf(buf, sizeof buf, "%s%s%s, %d player%s", in_race ? "racing " : "menus", in_race && g_def ? g_def->name : "", in_race && g_k3d[0].ok ? " (3D Jesus)" : "", g_multi ? g_multi : 1, g_multi > 1 ? "s" : "");
    return buf; }
