// view.c - draws the race: track, eight karts, items, effects, HUD, results. The rules live in sim/race.c.
#include "game.h"
#include "../sim/race.h"
#include "../render/gfx.h"
#include "../render/batch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SKID 500
#define MAX_DUST 200
#define MAX_TREES 120

static int g_status;
static Track g_track;
static Race g_race;
static int g_player_char = 0, g_difficulty = DIFF_NORMAL;
static float g_idle;                   /* seconds since the last real controller input */
static int g_demo;                     /* autopilot driving the player's kart */
static float g_time, g_msg_t; static char g_msg[40];
static float g_frame_ms = 16.7f;
static float g_results_t;

/* textures */
static Texture *tx_road, *tx_dirt, *tx_grass, *tx_wall, *tx_tree, *tx_blob, *tx_font, *tx_flag, *tx_sky;
/* meshes */
static Mesh *m_road, *m_shoulder, *m_ground, *m_wall, *m_startline, *m_kart_body, *m_kart_rest, *m_cube, *m_shield, *m_items, *m_trees, *m_skid, *m_dust, *m_hud, *m_shadow, *m_sky;
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
/* trees */
typedef struct { Vec3 p; float h; } Tree;
static Tree g_tree[MAX_TREES]; static int g_tree_n;
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
static void px_wall(int x, int y, uint32_t *o) {       /* stone blocks */
    int row = y / 16, off = (row & 1) * 16; int bx = (x + off) % 32, by = y % 16; int mortar = bx < 2 || by < 2;
    float n = smooth_noise(x / 6.0f, y / 6.0f); int v = 170 + (int)(n * 50) - mortar * 60;
    *o = rgba(v, v - 10, v - 30, 255);
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
static void px_sky(int x, int y, uint32_t *o) {        /* vertical gradient with a few soft clouds */
    float t = y / 255.0f; float n = sky_noise((float)x, (float)y);
    int r = (int)(80 + 120 * t), g = (int)(150 + 80 * t), b = (int)(230 + 20 * t);
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
    /* ground: a big grass plane under everything */
    batch_clear(&g_b);
    batch_quad(&g_b, v3(80, -2.4f, 70), v3(400, 0, 0), v3(0, 0, -400), v3(0, 1, 0), 0, 0, 80, 80, 1, 1, 1, 1);
    m_ground = gfx_mesh_static(g_b.v, g_b.n);
    /* sky: a big cylinder-ish ring far away (drawn without depth) */
    batch_clear(&g_b);
    for (int k = 0; k < 24; k++) {
        float a0 = k * 6.2831853f / 24, a1 = (k + 1) * 6.2831853f / 24, R = 900.0f;
        Vec3 p0 = v3(cosf(a0) * R, -60, sinf(a0) * R), p1 = v3(cosf(a1) * R, -60, sinf(a1) * R), p2 = v3(cosf(a1) * R, 420, sinf(a1) * R), p3 = v3(cosf(a0) * R, 420, sinf(a0) * R);
        batch_quad4(&g_b, p1, p0, p3, p2, v3(0, 1, 0), k / 24.0f * 3, 1, (k + 1) / 24.0f * 3, 0, 1, 1, 1, 1);
    }
    m_sky = gfx_mesh_static(g_b.v, g_b.n);
    /* trees along the outside of the road */
    g_tree_n = 0;
    for (int i = 3; i < t->n && g_tree_n < MAX_TREES; i += 7) {
        int side = ((i / 7) & 1) ? 1 : -1; float e = track_edge(t, i) + 2.5f + frand01() * 7.0f;
        Vec3 p = road_point(t, i, side * e); p.y = fminf(-2.4f, track_surface_height(t, i, side * track_edge(t, i)) - 1.0f) + 0.3f;
        g_tree[g_tree_n].p = p; g_tree[g_tree_n].h = 5.0f + frand01() * 4.0f; g_tree_n++;
    }
    m_trees = gfx_mesh_dynamic(MAX_TREES * 6);
    m_skid = gfx_mesh_dynamic(MAX_SKID * 6);
    m_dust = gfx_mesh_dynamic(MAX_DUST * 6);
    m_hud = gfx_mesh_dynamic(4096);
    /* placeholder kart: boxes until the character art arrives. The body is white and tinted with the character's color. */
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.42f, 0), v3(0.75f, 0.17f, 1.05f), 1, 1, 1);                    /* body (tinted) */
    batch_box(&g_b, v3(0, 0.48f, 0.95f), v3(0.6f, 0.12f, 0.2f), 1, 1, 1);                  /* front bumper */
    batch_box(&g_b, v3(0, 1.0f, -0.25f), v3(0.22f, 0.3f, 0.2f), 0.9f, 0.9f, 0.9f);          /* driver robe (tinted) */
    m_kart_body = gfx_mesh_static(g_b.v, g_b.n);
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.62f, -0.15f), v3(0.55f, 0.08f, 0.55f), 0.2f, 0.2f, 0.25f);     /* seat */
    batch_box(&g_b, v3(0, 1.48f, -0.25f), v3(0.19f, 0.19f, 0.19f), 0.98f, 0.82f, 0.65f);   /* head */
    for (int wx = -1; wx <= 1; wx += 2) for (int wz = -1; wz <= 1; wz += 2)
        batch_box(&g_b, v3(wx * 0.85f, 0.32f, wz * 0.78f), v3(0.14f, 0.32f, 0.32f), 0.12f, 0.12f, 0.12f);
    m_kart_rest = gfx_mesh_static(g_b.v, g_b.n);
    /* item box: a cube */
    batch_clear(&g_b); batch_box(&g_b, v3(0, 0, 0), v3(0.65f, 0.65f, 0.65f), 1, 1, 1); m_cube = gfx_mesh_static(g_b.v, g_b.n);
    /* shield: a bigger cube drawn transparent around the kart */
    batch_clear(&g_b); batch_box(&g_b, v3(0, 0.8f, 0), v3(1.4f, 1.2f, 1.6f), 1, 1, 1); m_shield = gfx_mesh_static(g_b.v, g_b.n);
    m_items = gfx_mesh_dynamic(MAX_PROJ * 6 + 6);
    m_shadow = gfx_mesh_dynamic(NKARTS * 6);
}

// ------------------------------------------------------------------ init
static void start_race(void) {
    race_init(&g_race, &g_track, g_player_char, 5, g_difficulty);
    g_skid_n = 0; memset(g_skid, 0, sizeof g_skid); memset(g_dust, 0, sizeof g_dust); memset(g_last_skid, 0, sizeof g_last_skid);
    Kart *k = &g_race.k[g_race.player];
    cam_yaw = k->yaw; cam_x = k->x - sinf(cam_yaw) * 8; cam_z = k->z - cosf(cam_yaw) * 8; cam_y = k->y + 3.5f;
    g_msg_t = 0; g_results_t = 0;
}
int game_init(int status) {
    g_status = status;
    track_make_test(&g_track);
    /* mini-map scale: fit the track into a 200x200 box */
    { float minx = 1e9f, maxx = -1e9f, minz = 1e9f, maxz = -1e9f;
      for (int i = 0; i < g_track.n; i++) { TrackSample *s = &g_track.s[i]; if (s->x < minx) minx = s->x; if (s->x > maxx) maxx = s->x; if (s->z < minz) minz = s->z; if (s->z > maxz) maxz = s->z; }
      float span = fmaxf(maxx - minx, maxz - minz) + 20.0f; map_scale = 190.0f / span; map_x0 = (minx + maxx) * 0.5f; map_z0 = (minz + maxz) * 0.5f; }
    start_race();
    g_time = 0; g_idle = 99; g_demo = 1;
    if (status != GFX_OK) return 0;
    tx_road = make_tex(256, 256, 1, 0, px_road);
    tx_dirt = make_tex(128, 128, 1, 0, px_dirt);
    tx_grass = make_tex(128, 128, 1, 0, px_grass);
    tx_wall = make_tex(64, 64, 1, 0, px_wall);
    tx_tree = make_tex(64, 64, 0, 0, px_tree);
    tx_blob = make_tex(64, 64, 0, 0, px_blob);
    tx_flag = make_tex(32, 32, 1, 1, px_flag);
    tx_sky = make_tex(256, 256, 1, 0, px_sky);
    { static uint32_t fpx[FONT_TEX_W * FONT_TEX_H]; font_build_texture(fpx); tx_font = gfx_texture_rgba(fpx, FONT_TEX_W, FONT_TEX_H, 0, 1); }
    build_track_meshes();
    gfx_log("track '%s': %d samples, %.0f m", g_track.name, g_track.n, g_track.length);
    return 1;
}
void game_shutdown(void) {}

// ------------------------------------------------------------------ effects
static void add_skid(int ki, Vec3 l, Vec3 r) {
    if (g_last_skid[ki]) {
        float jx = l.x - g_last_l[ki].x, jz = l.z - g_last_l[ki].z;
        if (jx * jx + jz * jz > 1.2f * 1.2f) { g_last_l[ki] = l; g_last_r[ki] = r; return; }   /* the kart was shoved: start a fresh mark */
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

// ------------------------------------------------------------------ per-frame update
void game_frame(const Pad *pad, float dt) {
    if (dt > 0.1f) dt = 0.1f;
    g_frame_ms = dt * 1000.0f;
    g_time += dt;
    Race *r = &g_race; Kart *k = &r->k[r->player];
    int real_input = pad->source && (fabsf(pad->sx) > 0.15f || pad->a || pad->b || pad->zr || pad->zl);
    if (real_input) { g_idle = 0; g_demo = 0; } else { g_idle += dt; if (g_idle > 6.0f) g_demo = 1; }
    if (g_msg_t > 0) g_msg_t -= dt;
    if (r->state == RACE_FINISHED) { g_results_t += dt; if (pad->plus || pad->a) { if (g_results_t > 1.0f) { g_difficulty = (g_difficulty + (pad->plus ? 1 : 0)) % 3; start_race(); } } }

    /* fixed-step simulation */
    static float acc = 0; acc += dt;
    KartInput in; memset(&in, 0, sizeof in);
    in.steer = pad->sx; in.gas = pad->a; in.brake = pad->b; in.drift = pad->zr; in.item = pad->zl || pad->x || pad->y;
    int steps = 0;
    while (acc >= SIM_DT && steps < 8) {
        race_step(r, &in, g_demo, SIM_DT); acc -= SIM_DT; steps++;
        /* react to what happened this step */
        for (int e = 0; e < r->nev; e++) {
            RaceEvent *ev = &r->ev[e]; int mine = ev->kart == r->player;
            switch (ev->type) {
                case EV_GO: snprintf(g_msg, sizeof g_msg, "GO!"); g_msg_t = 1.0f; break;
                case EV_FINAL_LAP: if (mine) { snprintf(g_msg, sizeof g_msg, "FINAL LAP!"); g_msg_t = 2.0f; } break;
                case EV_FINISH: if (mine) { snprintf(g_msg, sizeof g_msg, "FINISH!"); g_msg_t = 3.0f; } break;
                case EV_HIT: if (mine) { snprintf(g_msg, sizeof g_msg, "OUCH!"); g_msg_t = 0.8f; } add_dust(v3(r->k[ev->kart].x, r->k[ev->kart].y, r->k[ev->kart].z), 2.0f, 3.0f); break;
                case EV_BLOCK: if (mine) { snprintf(g_msg, sizeof g_msg, "BLOCKED!"); g_msg_t = 0.8f; } break;
                case EV_PICKUP: if (mine) { snprintf(g_msg, sizeof g_msg, "%s", item_name(r->k[ev->kart].item)); g_msg_t = 1.0f; } break;
                default: break;
            }
        }
        for (int i = 0; i < r->nk; i++) {
            Kart *q = &r->k[i];
            float hx = sinf(q->yaw + q->slide), hz = cosf(q->yaw + q->slide), rx = cosf(q->yaw + q->slide), rz = -sinf(q->yaw + q->slide);
            if (q->skid && !q->air) add_skid(i, v3(q->x - rx * 0.85f - hx * 0.8f, q->y + 0.04f, q->z - rz * 0.85f - hz * 0.8f), v3(q->x + rx * 0.85f - hx * 0.8f, q->y + 0.04f, q->z + rz * 0.85f - hz * 0.8f));
            else g_last_skid[i] = 0;
            if (i == r->player && q->mini_turbo_tier) { snprintf(g_msg, sizeof g_msg, q->mini_turbo_tier == 2 ? "SUPER TURBO!" : "MINI TURBO!"); g_msg_t = 1.2f; }
        }
    }
    if (steps == 8) acc = 0;

    /* effects */
    for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i];
        float near = (q->x - k->x) * (q->x - k->x) + (q->z - k->z) * (q->z - k->z) < 60.0f * 60.0f;
        if (near && !q->air && (!q->hit.on_road || q->drifting) && q->speed > 5.0f && frand01() < 0.6f) add_dust(v3(q->x, q->y, q->z), 1.5f, 1.5f);
        if (near && q->boost_t > 0 && frand01() < 0.9f) add_dust(v3(q->x - sinf(q->yaw) * 1.2f, q->y, q->z - cosf(q->yaw) * 1.2f), 0.6f, 1.5f); }
    for (int i = 0; i < MAX_DUST; i++) if (g_dust[i].life > 0) { Dust *d = &g_dust[i]; d->life -= dt; d->p.x += d->v.x * dt; d->p.y += d->v.y * dt; d->p.z += d->v.z * dt; d->v.y -= 2.0f * dt; d->size += 1.2f * dt; }
    for (int i = 0; i < MAX_SKID; i++) if (g_skid[i].used) { g_skid[i].a -= dt * 0.02f; if (g_skid[i].a <= 0) g_skid[i].used = 0; }

    /* chase camera: sits behind the kart, lags on turns, pulls back with speed */
    float want_yaw = k->yaw + k->slide * 0.3f;
    float dy = want_yaw - cam_yaw; while (dy > 3.14159f) dy -= 6.28318f; while (dy < -3.14159f) dy += 6.28318f;
    cam_yaw += dy * fminf(1.0f, 5.0f * dt);
    float back = 7.0f + k->speed * 0.06f, up = 3.0f + k->speed * 0.01f;
    float tx = k->x - sinf(cam_yaw) * back, tz = k->z - cosf(cam_yaw) * back, ty = k->y + up;
    cam_x += (tx - cam_x) * fminf(1.0f, 12.0f * dt); cam_z += (tz - cam_z) * fminf(1.0f, 12.0f * dt); cam_y += (ty - cam_y) * fminf(1.0f, 6.0f * dt);
    float want_fov = 62.0f + (k->boost_t > 0 ? 10.0f : 0.0f) + k->speed * 0.15f;
    cam_fov += (want_fov - cam_fov) * fminf(1.0f, 4.0f * dt);
    Vec3 eye = v3(cam_x, cam_y, cam_z), at = v3(k->x + sinf(k->yaw) * 3.0f, k->y + 1.2f, k->z + cosf(k->yaw) * 3.0f);

    /* dynamic geometry shared by both screens: trees, skid marks, dust, shadows, items */
    float cy = cam_yaw; Vec3 ax = v3(cosf(cy), 0, -sinf(cy));
    {
        batch_clear(&g_b);
        for (int i = 0; i < g_tree_n; i++) { Tree *tr = &g_tree[i]; float w = tr->h * 0.45f;
            batch_quad(&g_b, v3(tr->p.x, tr->p.y + tr->h * 0.5f, tr->p.z), v3(ax.x * w, 0, ax.z * w), v3(0, tr->h * 0.5f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 1); }
        gfx_mesh_update(m_trees, g_b.v, g_b.n);
        batch_clear(&g_b);
        for (int i = 0; i < MAX_SKID; i++) if (g_skid[i].used) { Skid *s = &g_skid[i];
            batch_quad4(&g_b, s->l0, s->l1, s->r1, s->r0, v3(0, 1, 0), 0, 0, 1, 1, 0.05f, 0.05f, 0.05f, s->a); }
        gfx_mesh_update(m_skid, g_b.v, g_b.n);
        batch_clear(&g_b);
        for (int i = 0; i < MAX_DUST; i++) if (g_dust[i].life > 0) { Dust *d = &g_dust[i]; float s = d->size * 0.5f;
            batch_quad(&g_b, d->p, v3(ax.x * s, 0, ax.z * s), v3(0, s, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.85f, 0.8f, 0.7f, d->life * 0.7f); }
        gfx_mesh_update(m_dust, g_b.v, g_b.n);
        batch_clear(&g_b);
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; TrackHit h2; track_locate(&g_track, q->x, q->z, q->hit.idx, &h2);
            float hx = sinf(q->yaw), hz = cosf(q->yaw);
            batch_quad(&g_b, v3(q->x, h2.height + 0.05f, q->z), v3(cosf(q->yaw) * 1.1f, 0, -sinf(q->yaw) * 1.1f), v3(hx * 1.3f, 0, hz * 1.3f), v3(0, 1, 0), 0, 0, 1, 1, 0, 0, 0, 0.45f * (1.0f - clampf((q->y - h2.height) / 6.0f, 0, 0.8f))); }
        gfx_mesh_update(m_shadow, g_b.v, g_b.n);
        /* oil puddles and flying stones as sprites */
        batch_clear(&g_b);
        for (int p = 0; p < MAX_PROJ; p++) { Proj *pr = &r->proj[p]; if (!pr->alive) continue;
            if (pr->type == ITEM_OIL) batch_quad(&g_b, v3(pr->x, pr->y + 0.06f, pr->z), v3(1.6f, 0, 0), v3(0, 0, -1.6f), v3(0, 1, 0), 0, 0, 1, 1, 0.08f, 0.05f, 0.02f, 0.9f);
            else batch_quad(&g_b, v3(pr->x, pr->y + 0.3f, pr->z), v3(ax.x * 0.45f, 0, ax.z * 0.45f), v3(0, 0.45f, 0), v3(0, 1, 0), 0, 0, 1, 1, 0.5f, 0.5f, 0.5f, 1); }
        gfx_mesh_update(m_items, g_b.v, g_b.n);
    }

    /* ---- draw both screens */
    gfx_frame_begin();
    for (int drc = 0; drc < 2; drc++) {
        if (g_status == GFX_NO_COMPILER) { gfx_view_begin(drc, 1.0f, 0.5f, 0.0f); gfx_view_end(drc); continue; }
        if (g_status == GFX_SHADER_FAILED) { gfx_view_begin(drc, 0.8f, 0.05f, 0.05f); gfx_view_end(drc); continue; }
        gfx_view_begin(drc, 0.55f, 0.75f, 0.98f);
        Mat4 view = m4_look_at(eye, at, v3(0, 1, 0));
        Mat4 proj = m4_perspective(cam_fov * 3.14159265f / 180.0f, gfx_aspect(drc), 0.3f, 1200.0f);
        Mat4 vp = m4_mul(proj, view);
        Mat4 id = m4_identity();
        gfx_camera(&vp, v3(0.35f, 1.0f, 0.5f), 1.0f, 0.72f, 0.82f, 0.95f, 1e6f, 2e6f);
        gfx_state(ST_BLEND); gfx_bind(tx_sky); { Mat4 sm = m4_translate(cam_x, -30.0f, cam_z); gfx_draw(m_sky, &sm, 1, 1, 1, 1); }
        gfx_camera(&vp, v3(0.35f, 1.0f, 0.5f), 0.45f, 0.72f, 0.82f, 0.95f, 120.0f, 520.0f);
        gfx_state(ST_SOLID);
        gfx_bind(tx_grass); gfx_draw(m_ground, &id, 1, 1, 1, 1);
        gfx_bind(tx_dirt); gfx_draw(m_shoulder, &id, 1, 1, 1, 1);
        gfx_bind(tx_road); gfx_draw(m_road, &id, 1, 1, 1, 1);
        gfx_bind(tx_flag); gfx_draw(m_startline, &id, 1, 1, 1, 1);
        gfx_bind(tx_wall); gfx_draw(m_wall, &id, 1, 1, 1, 1);
        gfx_state(ST_CUTOUT); gfx_bind(tx_tree); gfx_draw(m_trees, &id, 1, 1, 1, 1);
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
            float pitch = -atan2f(q->hit.slope, 1.0f) * (q->air ? 0.3f : 1.0f) + (q->air ? -q->vy * 0.03f : 0.0f);
            float roll = q->hit.on_road ? -g_track.s[q->hit.idx].bank : 0.0f;
            float sq = 1.0f - q->squash * 0.25f;
            Mat4 m = m4_mul(m4_translate(q->x, q->y + q->hop * 0.4f, q->z), m4_mul(m4_rotate_y(q->yaw + q->slide + q->spin_vis), m4_mul(m4_rotate_x(pitch), m4_mul(m4_rotate_z(roll + q->steer_vis * -0.04f), m4_scale(1.0f + (1 - sq) * 0.5f, sq, 1.0f)))));
            gfx_draw(m_kart_body, &m, c->r, c->g, c->b, 1);
            gfx_draw(m_kart_rest, &m, 1, 1, 1, 1); }
        /* shields */
        gfx_state(ST_TRANSPARENT); gfx_bind(NULL);
        for (int i = 0; i < r->nk; i++) { Kart *q = &r->k[i]; if (q->shield <= 0) continue;
            Mat4 m = m4_mul(m4_translate(q->x, q->y, q->z), m4_rotate_y(g_time * 2.0f));
            gfx_draw(m_shield, &m, 0.4f, 0.8f, 1.0f, 0.25f + 0.1f * sinf(g_time * 8)); }
        gfx_bind(tx_blob); gfx_draw(m_items, &id, 1, 1, 1, 1);
        gfx_state(ST_TRANSPARENT | ST_ADD); gfx_draw(m_dust, &id, 1, 1, 1, 1);

        /* ---- HUD */
        gfx_camera_2d(); gfx_state(ST_HUD);
        batch_clear(&g_b);
        char buf[64];
        int lap = k->lap < 1 ? 1 : (k->lap > g_track.laps ? g_track.laps : k->lap);
        snprintf(buf, sizeof buf, "LAP %d/%d", lap, g_track.laps); batch_text(&g_b, buf, 40, 30, 36, 1, 1, 1, 1);
        fmt_time(buf, sizeof buf, k->lap >= 1 ? k->lap_time : 0.0f); batch_text(&g_b, buf, 40, 72, 28, 1, 1, 0.6f, 1);
        if (k->best_lap > 0) { char tb[32]; fmt_time(tb, sizeof tb, k->best_lap); snprintf(buf, sizeof buf, "BEST %s", tb); batch_text(&g_b, buf, 40, 104, 20, 0.8f, 0.9f, 1, 1); }
        /* position */
        int mypos = 0; for (int p = 0; p < r->nk; p++) if (r->order[p] == r->player) mypos = p + 1;
        if (k->finished && k->place) mypos = k->place;
        batch_text(&g_b, place_name(mypos), 1240 - text_width(place_name(mypos), 64), 24, 64, 1, 0.9f, 0.3f, 1);
        snprintf(buf, sizeof buf, "%3.0f KM/H", fabsf(k->speed) * 3.6f); batch_text(&g_b, buf, 1240 - text_width(buf, 24), 96, 24, 1, 1, 1, 1);
        /* item slot */
        batch_rect(&g_b, 40, 140, 150, 60, 0.05f, 0.1f, 0.25f, 0.7f);
        batch_rect(&g_b, 43, 143, 144, 54, 0.15f, 0.3f, 0.6f, 0.7f);
        if (k->item) batch_text(&g_b, item_name(k->item), 115 - text_width(item_name(k->item), 22) / 2, 159, 22, 1, 0.9f, 0.4f, 1);
        else batch_text(&g_b, "ZL: ITEM", 115 - text_width("ZL: ITEM", 16) / 2, 162, 16, 0.7f, 0.8f, 1, 0.8f);
        /* drift charge bar */
        if (k->drifting) { float c = clampf(k->drift_charge / 1.6f, 0, 1); batch_rect(&g_b, 1040, 130, 200, 14, 0.1f, 0.1f, 0.1f, 0.6f);
            batch_rect(&g_b, 1040, 130, 200 * c, 14, c >= 1 ? 1.0f : 0.3f, c >= 0.5f ? 0.9f : 0.4f, 0.2f, 1); }
        /* mini-map: bottom right */
        { float mx = 1070, mz = 560;
          batch_rect(&g_b, mx - 105, mz - 105, 210, 210, 0.0f, 0.0f, 0.0f, 0.35f);
          for (int i = 0; i < g_track.n; i += 2) { TrackSample *s = &g_track.s[i];
              batch_rect(&g_b, mx + (s->x - map_x0) * map_scale - 2, mz - (s->z - map_z0) * map_scale - 2, 4, 4, 0.9f, 0.9f, 0.9f, 0.8f); }
          for (int p = r->nk - 1; p >= 0; p--) { int i = r->order[p]; Kart *q = &r->k[i]; const Character *c = &CHARACTERS[r->character[i]]; float sz = i == r->player ? 12 : 8;
              batch_rect(&g_b, mx + (q->x - map_x0) * map_scale - sz / 2, mz - (q->z - map_z0) * map_scale - sz / 2, sz, sz, c->r, c->g, c->b, 1);
              if (i == r->player) batch_rect(&g_b, mx + (q->x - map_x0) * map_scale - 3, mz - (q->z - map_z0) * map_scale - 3, 6, 6, 1, 1, 1, 1); } }
        /* messages */
        if (r->state == RACE_COUNTDOWN) { int n = (int)ceilf(r->cd - 0.5f); if (n >= 1 && n <= 3) { snprintf(buf, sizeof buf, "%d", n); batch_text(&g_b, buf, 640 - text_width(buf, 110) / 2, 250, 110, 1, 0.9f, 0.3f, 1); } }
        if (g_msg_t > 0) batch_text(&g_b, g_msg, 640 - text_width(g_msg, 60) / 2, 260, 60, 1, 0.95f, 0.4f, 1);
        if (k->wrongway_t > WRONGWAY_DELAY && ((int)(g_time * 4) & 1)) batch_text(&g_b, "WRONG WAY", 640 - text_width("WRONG WAY", 56) / 2, 330, 56, 1, 0.3f, 0.2f, 1);
        /* results */
        if (r->state == RACE_FINISHED) {
            batch_rect(&g_b, 290, 90, 700, 540, 0.05f, 0.08f, 0.2f, 0.85f);
            batch_rect(&g_b, 296, 96, 688, 528, 0.12f, 0.2f, 0.45f, 0.9f);
            batch_text(&g_b, "RESULTS", 640 - text_width("RESULTS", 44) / 2, 110, 44, 1, 0.85f, 0.3f, 1);
            int rows[NKARTS]; for (int p = 0; p < r->nk; p++) rows[p] = r->order[p];
            for (int a2 = 1; a2 < r->nk; a2++) { int v = rows[a2], j = a2 - 1; while (j >= 0 && r->k[rows[j]].place > r->k[v].place) { rows[j + 1] = rows[j]; j--; } rows[j + 1] = v; }
            for (int p = 0; p < r->nk; p++) { int i = rows[p]; Kart *q = &r->k[i]; const Character *c = &CHARACTERS[r->character[i]];
                float y = 170 + p * 50; int me = i == r->player;
                if (me) batch_rect(&g_b, 310, y - 6, 660, 44, 1, 0.85f, 0.3f, 0.25f);
                batch_text(&g_b, place_name(q->place ? q->place : p + 1), 330, y, 30, 1, 1, 1, 1);
                batch_rect(&g_b, 440, y + 2, 26, 26, c->r, c->g, c->b, 1);
                batch_text(&g_b, c->short_name, 480, y, 30, me ? 1.0f : 0.9f, me ? 0.95f : 0.9f, me ? 0.5f : 0.9f, 1);
                if (q->lap > g_track.laps) fmt_time(buf, sizeof buf, q->total_time); else snprintf(buf, sizeof buf, "--:--.--");
                batch_text(&g_b, buf, 800, y, 30, 1, 1, 1, 1); }
            batch_text(&g_b, "A: RACE AGAIN   +: NEXT DIFFICULTY", 640 - text_width("A: RACE AGAIN   +: NEXT DIFFICULTY", 20) / 2, 585, 20, 0.9f, 0.95f, 1, 1);
        }
        { const char *dn[3] = {"EASY", "NORMAL", "HARD"}; batch_text(&g_b, dn[g_difficulty], 40, 690, 16, 0.8f, 0.8f, 0.8f, 0.7f); }
        if (g_demo && r->state != RACE_FINISHED) batch_text(&g_b, "DEMO - MOVE THE STICK TO DRIVE", 640 - text_width("DEMO - MOVE THE STICK TO DRIVE", 20) / 2, 680, 20, 1, 1, 1, 0.8f);
        { float frac = clampf(g_frame_ms / 33.3f, 0.02f, 1.0f); int good = g_frame_ms <= 16.8f;
          batch_rect(&g_b, 20, 708, 300 * frac, 6, good ? 0.2f : 1.0f, good ? 1.0f : 0.2f, 0.2f, 0.8f);
          batch_rect(&g_b, 20 + 300 * (16.7f / 33.3f), 704, 2, 14, 1, 1, 1, 0.8f); }
        batch_rect(&g_b, 1220, 690, 20, 20, pad->source & 2 ? 1.0f : (pad->source & 1 ? 0.2f : 0.5f), pad->source & 2 ? 0.8f : (pad->source & 1 ? 0.5f : 0.5f), pad->source ? 0.2f : 0.5f, 0.9f);
        gfx_mesh_update(m_hud, g_b.v, g_b.n);
        gfx_bind(tx_font); gfx_draw(m_hud, &id, 1, 1, 1, 1);
        gfx_view_end(drc);
    }
    gfx_frame_end();
}
