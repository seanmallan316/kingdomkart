// race.c - Milestone 2: one kart on the test track. Track meshes, placeholder kart, chase camera, effects, HUD.
#include "game.h"
#include "../render/gfx.h"
#include "../render/batch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SKID 400
#define MAX_DUST 160
#define MAX_TREES 120

static int g_status;
static Track g_track;
static Kart g_kart;
static float g_cd;                     /* countdown seconds before the race starts */
static float g_idle;                   /* seconds since the last real controller input */
static int g_demo;                     /* autopilot driving */
static float g_time, g_msg_t; static char g_msg[32];
static float g_frame_ms = 16.7f;

/* textures */
static Texture *tx_road, *tx_dirt, *tx_grass, *tx_wall, *tx_tree, *tx_blob, *tx_font, *tx_flag, *tx_sky;
/* meshes */
static Mesh *m_road, *m_shoulder, *m_ground, *m_wall, *m_startline, *m_kart, *m_trees, *m_skid, *m_dust, *m_hud, *m_shadow, *m_sky;
static Vertex g_vtmp[8192];
static Batch g_b;

/* camera state */
static float cam_yaw, cam_x, cam_y, cam_z, cam_fov = 62.0f;
/* skid marks: ring of quads */
typedef struct { Vec3 l0, r0, l1, r1; float a; int used; } Skid;
static Skid g_skid[MAX_SKID]; static int g_skid_n; static Vec3 g_last_l, g_last_r; static int g_last_skid;
/* dust */
typedef struct { Vec3 p, v; float life, size; } Dust;
static Dust g_dust[MAX_DUST];
/* trees */
typedef struct { Vec3 p; float h; } Tree;
static Tree g_tree[MAX_TREES]; static int g_tree_n;

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
static void px_sky(int x, int y, uint32_t *o) {        /* vertical gradient with a few soft clouds */
    float t = y / 255.0f; float n = smooth_noise(x / 40.0f, y / 20.0f) * 0.6f + smooth_noise(x / 15.0f, y / 8.0f) * 0.4f;
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
    batch_quad(&g_b, v3(80, -1.3f, 70), v3(400, 0, 0), v3(0, 0, -400), v3(0, 1, 0), 0, 0, 80, 80, 1, 1, 1, 1);
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
        Vec3 p = road_point(t, i, side * e); p.y = -1.3f + (track_surface_height(t, i, side * track_edge(t, i)) + 1.3f) * 0.5f;
        g_tree[g_tree_n].p = p; g_tree[g_tree_n].h = 5.0f + frand01() * 4.0f; g_tree_n++;
    }
    m_trees = gfx_mesh_dynamic(MAX_TREES * 6);
    m_skid = gfx_mesh_dynamic(MAX_SKID * 6);
    m_dust = gfx_mesh_dynamic(MAX_DUST * 6);
    m_hud = gfx_mesh_dynamic(4096);
    m_shadow = gfx_mesh_dynamic(6);
    /* placeholder kart: boxes (body, seat, four wheels, driver) until the character art arrives */
    batch_clear(&g_b);
    batch_box(&g_b, v3(0, 0.42f, 0), v3(0.75f, 0.17f, 1.05f), 0.95f, 0.78f, 0.25f);        /* body, gold */
    batch_box(&g_b, v3(0, 0.62f, -0.15f), v3(0.55f, 0.08f, 0.55f), 0.25f, 0.45f, 0.95f);   /* seat area, blue */
    batch_box(&g_b, v3(0, 0.48f, 0.95f), v3(0.6f, 0.12f, 0.2f), 0.95f, 0.95f, 0.95f);      /* front bumper */
    batch_box(&g_b, v3(0, 1.0f, -0.25f), v3(0.22f, 0.3f, 0.2f), 0.85f, 0.25f, 0.2f);       /* driver torso, red robe */
    batch_box(&g_b, v3(0, 1.48f, -0.25f), v3(0.19f, 0.19f, 0.19f), 0.98f, 0.82f, 0.65f);   /* head */
    for (int wx = -1; wx <= 1; wx += 2) for (int wz = -1; wz <= 1; wz += 2)
        batch_box(&g_b, v3(wx * 0.85f, 0.32f, wz * 0.78f), v3(0.14f, 0.32f, 0.32f), 0.12f, 0.12f, 0.12f);
    m_kart = gfx_mesh_static(g_b.v, g_b.n);
}

// ------------------------------------------------------------------ init
int game_init(int status) {
    g_status = status;
    track_make_test(&g_track);
    memset(&g_kart, 0, sizeof g_kart);
    g_kart.st.top_speed = 1.0f; g_kart.st.accel = 1.0f; g_kart.st.handling = 1.0f; g_kart.st.weight = 1.0f;
    kart_reset(&g_kart, &g_track, 0);
    g_cd = 3.5f; g_time = 0; g_idle = 99; g_demo = 1;
    cam_yaw = g_kart.yaw; cam_x = g_kart.x - sinf(cam_yaw) * 8; cam_z = g_kart.z - cosf(cam_yaw) * 8; cam_y = g_kart.y + 3.5f;
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
static void add_skid(Vec3 l, Vec3 r) {
    if (g_last_skid) {
        Skid *s = &g_skid[g_skid_n % MAX_SKID]; g_skid_n++;
        s->l0 = g_last_l; s->r0 = g_last_r; s->l1 = l; s->r1 = r; s->a = 0.55f; s->used = 1;
    }
    g_last_l = l; g_last_r = r; g_last_skid = 1;
}
static void add_dust(Vec3 p, float spread, float r, float g, float b) {
    for (int i = 0; i < MAX_DUST; i++) if (g_dust[i].life <= 0) {
        Dust *d = &g_dust[i];
        d->p = v3(p.x + (frand01() - 0.5f) * spread, p.y + 0.2f, p.z + (frand01() - 0.5f) * spread);
        d->v = v3((frand01() - 0.5f) * 3, 1.5f + frand01() * 2, (frand01() - 0.5f) * 3);
        d->life = 0.5f + frand01() * 0.5f; d->size = 0.5f + frand01() * 0.6f;
        (void)r; (void)g; (void)b;
        return;
    }
}

// ------------------------------------------------------------------ simulation step (120 Hz)
static void sim_step(const KartInput *in) {
    Kart *k = &g_kart;
    if (g_cd > 0) { KartInput none; memset(&none, 0, sizeof none); kart_step(k, &g_track, &none, SIM_DT); k->total_time = 0; return; }
    kart_step(k, &g_track, in, SIM_DT);
    /* tire positions for skid marks */
    float hx = sinf(k->yaw + k->slide), hz = cosf(k->yaw + k->slide), rx = cosf(k->yaw + k->slide), rz = -sinf(k->yaw + k->slide);
    if (k->skid && !k->air) add_skid(v3(k->x - rx * 0.85f - hx * 0.8f, k->y + 0.04f, k->z - rz * 0.85f - hz * 0.8f), v3(k->x + rx * 0.85f - hx * 0.8f, k->y + 0.04f, k->z + rz * 0.85f - hz * 0.8f));
    else g_last_skid = 0;
    if (k->mini_turbo_tier) { snprintf(g_msg, sizeof g_msg, k->mini_turbo_tier == 2 ? "SUPER TURBO!" : "MINI TURBO!"); g_msg_t = 1.2f; }
    if (k->lap >= 1 && k->lap == g_track.laps && k->lap_time < 0.02f && !k->finished) { snprintf(g_msg, sizeof g_msg, "FINAL LAP!"); g_msg_t = 2.0f; }
    if (k->finished && g_msg_t <= 0 && k->lap_time < 0.05f) { snprintf(g_msg, sizeof g_msg, "FINISH!"); g_msg_t = 4.0f; }
}

// ------------------------------------------------------------------ per-frame update
void game_frame(const Pad *pad, float dt) {
    if (dt > 0.1f) dt = 0.1f;
    g_frame_ms = dt * 1000.0f;
    g_time += dt;
    int real_input = pad->source && (fabsf(pad->sx) > 0.15f || pad->a || pad->b || pad->zr || pad->zl);
    if (real_input) { g_idle = 0; g_demo = 0; } else { g_idle += dt; if (g_idle > 6.0f) g_demo = 1; }
    if (g_cd > 0) { g_cd -= dt; if (g_cd <= 0) { snprintf(g_msg, sizeof g_msg, "GO!"); g_msg_t = 1.0f; } }
    if (g_msg_t > 0) g_msg_t -= dt;
    if (pad->plus && g_kart.finished) { kart_reset(&g_kart, &g_track, 0); g_cd = 3.5f; g_skid_n = 0; memset(g_skid, 0, sizeof g_skid); }

    /* fixed-step simulation */
    static float acc = 0; acc += dt;
    KartInput in;
    if (g_demo) kart_autopilot(&g_kart, &g_track, &in, 0.9f);
    else { memset(&in, 0, sizeof in); in.steer = pad->sx; in.gas = pad->a; in.brake = pad->b; in.drift = pad->zr; in.item = pad->zl; }
    int steps = 0;
    while (acc >= SIM_DT && steps < 8) { sim_step(&in); acc -= SIM_DT; steps++; }
    if (steps == 8) acc = 0;

    /* effects */
    Kart *k = &g_kart;
    if (!k->air && (!k->hit.on_road || k->drifting) && k->speed > 5.0f && frand01() < 0.8f) add_dust(v3(k->x, k->y, k->z), 1.5f, 1, 1, 1);
    if (k->boost_t > 0 && frand01() < 0.9f) add_dust(v3(k->x - sinf(k->yaw) * 1.2f, k->y, k->z - cosf(k->yaw) * 1.2f), 0.6f, 1, 1, 1);
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

    /* dynamic geometry shared by both screens: trees, skid marks, dust, shadow */
    {
        batch_clear(&g_b);
        float cy = cam_yaw; Vec3 ax = v3(cosf(cy), 0, -sinf(cy));
        for (int i = 0; i < g_tree_n; i++) { Tree *tr = &g_tree[i]; float w = tr->h * 0.45f;
            batch_quad(&g_b, v3(tr->p.x, tr->p.y + tr->h * 0.5f, tr->p.z), v3(ax.x * w, 0, ax.z * w), v3(0, tr->h * 0.5f, 0), v3(0, 1, 0), 0, 0, 1, 1, 1, 1, 1, 1); }
        gfx_mesh_update(m_trees, g_b.v, g_b.n);
        /* skid marks */
        batch_clear(&g_b);
        for (int i = 0; i < MAX_SKID; i++) if (g_skid[i].used) { Skid *s = &g_skid[i];
            batch_quad4(&g_b, s->l0, s->l1, s->r1, s->r0, v3(0, 1, 0), 0, 0, 1, 1, 0.05f, 0.05f, 0.05f, s->a); }
        gfx_mesh_update(m_skid, g_b.v, g_b.n);
        /* dust */
        batch_clear(&g_b);
        for (int i = 0; i < MAX_DUST; i++) if (g_dust[i].life > 0) { Dust *d = &g_dust[i]; float s = d->size * 0.5f;
            float col = k->hit.on_road ? 0.9f : 0.75f;
            batch_quad(&g_b, d->p, v3(ax.x * s, 0, ax.z * s), v3(0, s, 0), v3(0, 1, 0), 0, 0, 1, 1, col, col * 0.95f, col * 0.8f, d->life * 0.7f); }
        gfx_mesh_update(m_dust, g_b.v, g_b.n);
        /* shadow under the kart */
        batch_clear(&g_b);
        { TrackHit h2; track_locate(&g_track, k->x, k->z, k->hit.idx, &h2);
          float hx = sinf(k->yaw), hz = cosf(k->yaw);
          batch_quad(&g_b, v3(k->x, h2.height + 0.05f, k->z), v3(cosf(k->yaw) * 1.1f, 0, -sinf(k->yaw) * 1.1f), v3(hx * 1.3f, 0, hz * 1.3f), v3(0, 1, 0), 0, 0, 1, 1, 0, 0, 0, 0.45f * (1.0f - clampf((k->y - h2.height) / 6.0f, 0, 0.8f))); }
        gfx_mesh_update(m_shadow, g_b.v, g_b.n);
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
        /* sky ring follows the camera so it never gets closer; drawn without fog */
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
        /* the kart */
        gfx_state(ST_SOLID); gfx_bind(NULL);
        { float pitch = -atan2f(k->hit.slope, 1.0f) * (k->air ? 0.3f : 1.0f) + (k->air ? -k->vy * 0.03f : 0.0f);
          float roll = k->hit.on_road ? -g_track.s[k->hit.idx].bank : 0.0f;
          float sq = 1.0f - k->squash * 0.25f;
          Mat4 m = m4_mul(m4_translate(k->x, k->y + k->hop * 0.4f, k->z), m4_mul(m4_rotate_y(k->yaw + k->slide), m4_mul(m4_rotate_x(pitch), m4_mul(m4_rotate_z(roll + k->steer_vis * -0.04f), m4_scale(1.0f + (1 - sq) * 0.5f, sq, 1.0f)))));
          gfx_draw(m_kart, &m, 1, 1, 1, 1); }
        gfx_state(ST_TRANSPARENT | ST_ADD); gfx_bind(tx_blob); gfx_draw(m_dust, &id, 1, 1, 1, 1);

        /* ---- HUD */
        gfx_camera_2d(); gfx_state(ST_HUD);
        batch_clear(&g_b);
        char buf[64];
        /* lap and time */
        int lap = k->lap < 1 ? 1 : (k->lap > g_track.laps ? g_track.laps : k->lap);
        snprintf(buf, sizeof buf, "LAP %d/%d", lap, g_track.laps); batch_text(&g_b, buf, 40, 30, 36, 1, 1, 1, 1);
        float tt = k->lap >= 1 ? k->lap_time : 0.0f; int mm = (int)(tt / 60), ss = (int)tt % 60, hh = (int)(tt * 100) % 100;
        snprintf(buf, sizeof buf, "%d:%02d.%02d", mm, ss, hh); batch_text(&g_b, buf, 40, 72, 28, 1, 1, 0.6f, 1);
        if (k->best_lap > 0) { tt = k->best_lap; mm = (int)(tt / 60); ss = (int)tt % 60; hh = (int)(tt * 100) % 100; snprintf(buf, sizeof buf, "BEST %d:%02d.%02d", mm, ss, hh); batch_text(&g_b, buf, 40, 104, 20, 0.8f, 0.9f, 1, 1); }
        /* speed */
        snprintf(buf, sizeof buf, "%3.0f KM/H", fabsf(k->speed) * 3.6f); batch_text(&g_b, buf, 1240 - text_width(buf, 32), 30, 32, 1, 1, 1, 1);
        /* drift charge bar */
        if (k->drifting) { float c = clampf(k->drift_charge / 1.6f, 0, 1); batch_rect(&g_b, 1040, 72, 200, 14, 0.1f, 0.1f, 0.1f, 0.6f);
            batch_rect(&g_b, 1040, 72, 200 * c, 14, c >= 1 ? 1.0f : 0.3f, c >= 0.5f ? 0.9f : 0.4f, 0.2f, 1); }
        /* messages */
        if (g_cd > 0) { int n = (int)ceilf(g_cd - 0.5f); if (n >= 1 && n <= 3) { snprintf(buf, sizeof buf, "%d", n); batch_text(&g_b, buf, 640 - text_width(buf, 110) / 2, 250, 110, 1, 0.9f, 0.3f, 1); } }
        if (g_msg_t > 0) batch_text(&g_b, g_msg, 640 - text_width(g_msg, 60) / 2, 260, 60, 1, 0.95f, 0.4f, 1);
        if (k->wrongway_t > WRONGWAY_DELAY && ((int)(g_time * 4) & 1)) batch_text(&g_b, "WRONG WAY", 640 - text_width("WRONG WAY", 56) / 2, 320, 56, 1, 0.3f, 0.2f, 1);
        if (k->finished) { snprintf(buf, sizeof buf, "TIME %d:%02d.%02d", (int)(k->total_time / 60), (int)k->total_time % 60, (int)(k->total_time * 100) % 100);
            batch_text(&g_b, buf, 640 - text_width(buf, 40) / 2, 400, 40, 1, 1, 1, 1); batch_text(&g_b, "PRESS + TO RACE AGAIN", 640 - text_width("PRESS + TO RACE AGAIN", 24) / 2, 460, 24, 1, 1, 1, 1); }
        if (g_demo) batch_text(&g_b, "DEMO - MOVE THE STICK TO DRIVE", 640 - text_width("DEMO - MOVE THE STICK TO DRIVE", 20) / 2, 680, 20, 1, 1, 1, 0.8f);
        /* frame-time bar (bottom-left): green at or under the 60 fps budget */
        { float frac = clampf(g_frame_ms / 33.3f, 0.02f, 1.0f); int good = g_frame_ms <= 16.8f;
          batch_rect(&g_b, 20, 700, 300 * frac, 8, good ? 0.2f : 1.0f, good ? 1.0f : 0.2f, 0.2f, 0.8f);
          batch_rect(&g_b, 20 + 300 * (16.7f / 33.3f), 696, 2, 16, 1, 1, 1, 0.8f); }
        /* controller indicator */
        batch_rect(&g_b, 1220, 690, 20, 20, pad->source & 2 ? 1.0f : (pad->source & 1 ? 0.2f : 0.5f), pad->source & 2 ? 0.8f : (pad->source & 1 ? 0.5f : 0.5f), pad->source ? 0.2f : 0.5f, 0.9f);
        gfx_mesh_update(m_hud, g_b.v, g_b.n);
        gfx_bind(tx_font); gfx_draw(m_hud, &id, 1, 1, 1, 1);
        gfx_view_end(drc);
    }
    gfx_frame_end();
}
