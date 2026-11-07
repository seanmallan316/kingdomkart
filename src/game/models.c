// models.c - the scenery and road obstacles as real 3D models, built from simple shapes (no picture needed).
// Every model is built standing on the ground at (0, 0, 0), facing +z, about h meters tall.
#include "models.h"
#include <math.h>
#include <string.h>

static Batch *B;
static unsigned g_seed;
static float rnd(void) { g_seed = g_seed * 1664525u + 1013904223u; return (float)((g_seed >> 8) & 0xFFFF) / 65535.0f; }
static Vec3 nrm(Vec3 a, Vec3 b, Vec3 c) { return v3_norm(v3_cross(v3_sub(b, a), v3_sub(c, a))); }
static void tri(Vec3 a, Vec3 b, Vec3 c, float r, float g, float bl) {
    if (B->n + 3 > B->cap) return; Vec3 n = nrm(a, b, c);
    Vertex v[3] = { {a.x, a.y, a.z, n.x, n.y, n.z, 0, 0, r, g, bl, 1}, {b.x, b.y, b.z, n.x, n.y, n.z, 0, 0, r, g, bl, 1}, {c.x, c.y, c.z, n.x, n.y, n.z, 0, 0, r, g, bl, 1} };
    for (int k = 0; k < 3; k++) B->v[B->n++] = v[k]; }
static void quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, float r, float g, float bl) { tri(a, b, c, r, g, bl); tri(a, c, d, r, g, bl); }
static void box(Vec3 c, Vec3 h, float r, float g, float bl) { if (B->n + 36 <= B->cap) batch_box(B, c, h, r, g, bl); }
/* a round section from y0 to y1, radius r0 to r1, centred on (x, z), with a cap on top */
static void cyl(float x, float z, float r0, float r1, float y0, float y1, int n, float r, float g, float bl) {
    for (int i = 0; i < n; i++) { float a0 = i * 6.2831853f / n, a1 = (i + 1) * 6.2831853f / n;
        Vec3 p0 = v3(x + sinf(a0) * r0, y0, z + cosf(a0) * r0), p1 = v3(x + sinf(a1) * r0, y0, z + cosf(a1) * r0), q1 = v3(x + sinf(a1) * r1, y1, z + cosf(a1) * r1), q0 = v3(x + sinf(a0) * r1, y1, z + cosf(a0) * r1);
        float sh = 0.85f + 0.15f * sinf(a0 + 0.8f); quad(p0, p1, q1, q0, r * sh, g * sh, bl * sh);
        tri(v3(x, y1, z), q0, q1, r * 1.05f, g * 1.05f, bl * 1.05f); } }
/* a lumpy ball (foliage, wool, cloud, stone): rx, ry, rz radii, bumps of the given size */
static void ball(Vec3 c, float rx, float ry, float rz, float bump, float r, float g, float bl) {
    const int R = 6, S = 9; Vec3 p[R + 1][S + 1]; unsigned sd = g_seed;
    for (int i = 0; i <= R; i++) for (int j = 0; j <= S; j++) { float th = 3.14159265f * i / R, ph = 6.2831853f * (j % S) / S;
        unsigned hh = (unsigned)(i * 131 + (j % S) * 71) * 2654435761u + sd; float l = (i == 0 || i == R) ? 1.0f : 1.0f - bump + 2.0f * bump * ((hh >> 12) & 255) / 255.0f;
        p[i][j] = v3(c.x + sinf(th) * cosf(ph) * rx * l, c.y + cosf(th) * ry * l, c.z + sinf(th) * sinf(ph) * rz * l); }
    for (int i = 0; i < R; i++) for (int j = 0; j < S; j++) { float t = 1.0f - (float)i / R * 0.45f;   /* lighter on top */
        quad(p[i][j], p[i][j + 1], p[i + 1][j + 1], p[i + 1][j], r * t, g * t, bl * t); }
    rnd(); }
static void cone(float x, float z, float y0, float rad, float h, int n, float r, float g, float bl) {
    for (int i = 0; i < n; i++) { float a0 = i * 6.2831853f / n, a1 = (i + 1) * 6.2831853f / n; float sh = 0.8f + 0.2f * sinf(a0 + 0.8f);
        tri(v3(x + sinf(a0) * rad, y0, z + cosf(a0) * rad), v3(x + sinf(a1) * rad, y0, z + cosf(a1) * rad), v3(x, y0 + h, z), r * sh, g * sh, bl * sh); } }
/* a leaf blade from base along dir (unit, mostly sideways), drooping, tapering: palm fronds, ferns */
static void blade(Vec3 base, Vec3 dir, float len, float wid, float droop, float r, float g, float bl) {
    Vec3 side = v3_norm(v3_cross(dir, v3(0, 1, 0))); Vec3 prev = base; float pw = wid * 0.3f;
    for (int k = 1; k <= 5; k++) { float t = k / 5.0f; Vec3 p = v3(base.x + dir.x * len * t, base.y + dir.y * len * t - droop * t * t, base.z + dir.z * len * t); float w2 = wid * sinf(3.14159265f * fminf(t + 0.15f, 1.0f)) + 0.05f;
        quad(v3(prev.x - side.x * pw, prev.y, prev.z - side.z * pw), v3(prev.x + side.x * pw, prev.y, prev.z + side.z * pw), v3(p.x + side.x * w2, p.y, p.z + side.z * w2), v3(p.x - side.x * w2, p.y, p.z - side.z * w2), r * (0.8f + 0.2f * t), g * (0.8f + 0.2f * t), bl);
        prev = p; pw = w2; } }

static void m_tree(float h, int kind) {   /* 0 broad tree, 1 olive (silver-green, gnarled), 2 fruit tree, 3 tree of life (golden fruit) */
    float tr = 0.42f, tg = 0.3f, tb = 0.18f, lr = 0.25f, lg = 0.55f, lb = 0.2f;
    if (kind == 1) { tr = 0.45f; tg = 0.4f; tb = 0.3f; lr = 0.45f; lg = 0.58f; lb = 0.38f; }
    if (kind == 3) { lr = 0.3f; lg = 0.7f; lb = 0.3f; }
    float th = h * (kind == 1 ? 0.38f : 0.45f), trad = h * 0.05f, lean = (rnd() - 0.5f) * h * 0.08f;
    cyl(0, 0, trad * 1.3f, trad * 0.8f, 0, th * 0.55f, 7, tr, tg, tb); cyl(lean * 0.5f, 0, trad * 0.8f, trad * 0.6f, th * 0.55f, th, 7, tr, tg, tb);
    if (kind == 1) for (int k = 0; k < 2; k++) { float a = rnd() * 6.28f; cyl(sinf(a) * h * 0.08f, cosf(a) * h * 0.08f, trad * 0.5f, trad * 0.3f, th * 0.6f, th * 1.05f, 5, tr, tg, tb); }   /* gnarled limbs */
    int nb = kind == 1 ? 6 : 5;
    for (int k = 0; k < nb; k++) { float a = k * 6.2831853f / nb + rnd() * 0.6f, d = h * (k == 0 ? 0.0f : 0.17f + rnd() * 0.07f);
        float sz = h * (k == 0 ? 0.26f : 0.19f + rnd() * 0.05f), y = th + h * (k == 0 ? 0.2f : 0.08f + rnd() * 0.1f);
        float v = 0.85f + rnd() * 0.25f; ball(v3(lean + sinf(a) * d, y, cosf(a) * d), sz, sz * (kind == 1 ? 0.65f : 0.8f), sz, 0.18f, lr * v, lg * v, lb * v); }
    if (kind == 2 || kind == 3) for (int k = 0; k < 9; k++) { float a = rnd() * 6.28f, d = h * (0.15f + rnd() * 0.2f);
        box(v3(lean + sinf(a) * d, th + h * (0.05f + rnd() * 0.3f), cosf(a) * d), v3(h * 0.025f, h * 0.025f, h * 0.025f), kind == 3 ? 1.0f : 0.9f, kind == 3 ? 0.82f : 0.25f, kind == 3 ? 0.2f : 0.15f); }
}
static void m_palm(float h) {
    float lean = 0.12f + rnd() * 0.12f, dir = rnd() * 6.28f; Vec3 p = v3(0, 0, 0); float r0 = h * 0.045f;
    for (int k = 0; k < 6; k++) { float t = (k + 1) / 6.0f; Vec3 q = v3(sinf(dir) * lean * h * t * t, h * t, cosf(dir) * lean * h * t * t); float r1 = r0 * (1.0f - 0.25f * t);
        cyl(p.x, p.z, r0, r1, p.y, q.y, 7, 0.55f, 0.42f, 0.28f); box(v3(q.x, q.y, q.z), v3(r1 * 1.15f, h * 0.012f, r1 * 1.15f), 0.45f, 0.34f, 0.22f); p = q; r0 = r1; }   /* ringed trunk */
    for (int k = 0; k < 8; k++) { float a = k * 6.2831853f / 8 + rnd() * 0.3f; blade(p, v3_norm(v3(sinf(a), 0.35f, cosf(a))), h * 0.42f, h * 0.06f, h * 0.22f, 0.25f, 0.58f + rnd() * 0.1f, 0.2f); }
    for (int k = 0; k < 3; k++) { float a = k * 2.1f; ball(v3(p.x + sinf(a) * h * 0.04f, p.y - h * 0.03f, p.z + cosf(a) * h * 0.04f), h * 0.03f, h * 0.035f, h * 0.03f, 0.05f, 0.45f, 0.32f, 0.15f); }
}
static void m_cypress(float h) { cyl(0, 0, h * 0.04f, h * 0.03f, 0, h * 0.15f, 6, 0.4f, 0.3f, 0.2f);
    ball(v3(0, h * 0.55f, 0), h * 0.13f, h * 0.45f, h * 0.13f, 0.1f, 0.15f, 0.38f, 0.18f); }
static void m_flowers(float h, int lily) {
    for (int k = 0; k < (lily ? 4 : 9); k++) { float x = (rnd() - 0.5f) * h * 2.5f, z = (rnd() - 0.5f) * h * 2.5f;
        if (lily) { cyl(x, z, h * 0.5f, h * 0.5f, 0.0f, 0.05f, 9, 0.2f, 0.55f, 0.25f); box(v3(x, 0.15f, z), v3(h * 0.12f, h * 0.12f, h * 0.12f), 1, 0.95f, 0.98f); continue; }
        float sh = h * (0.6f + rnd() * 0.5f); box(v3(x, sh / 2, z), v3(0.03f, sh / 2, 0.03f), 0.2f, 0.5f, 0.2f);
        static const float col[4][3] = {{1, 0.3f, 0.4f}, {1, 0.85f, 0.2f}, {0.6f, 0.4f, 1}, {1, 1, 1}}; int c = (int)(rnd() * 3.99f);
        ball(v3(x, sh, z), h * 0.16f, h * 0.1f, h * 0.16f, 0.2f, col[c][0], col[c][1], col[c][2]); } }
static void m_lantern(float h) { cyl(0, 0, 0.08f, 0.06f, 0, h * 0.82f, 6, 0.25f, 0.2f, 0.15f);
    box(v3(0, h * 0.86f, 0), v3(h * 0.08f, h * 0.1f, h * 0.08f), 1.0f, 0.8f, 0.35f); cone(0, 0, h * 0.96f, h * 0.11f, h * 0.08f, 4, 0.3f, 0.22f, 0.15f); }
static void m_crystal(float h) { for (int k = 0; k < 5; k++) { float a = k * 1.3f, d = k ? h * 0.18f : 0, ch = h * (k ? 0.45f + rnd() * 0.3f : 1.0f), cw = ch * 0.18f, tilt = k ? 0.3f : 0;
        Vec3 base = v3(sinf(a) * d, 0, cosf(a) * d), top = v3(base.x + sinf(a) * tilt * ch, ch, base.z + cosf(a) * tilt * ch);
        for (int q = 0; q < 6; q++) { float a0 = q * 1.0472f, a1 = (q + 1) * 1.0472f; Vec3 p0 = v3(base.x + sinf(a0) * cw, ch * 0.15f, base.z + cosf(a0) * cw), p1 = v3(base.x + sinf(a1) * cw, ch * 0.15f, base.z + cosf(a1) * cw);
            Vec3 m0 = v3(top.x + sinf(a0) * cw * 0.8f - sinf(a) * tilt * ch * 0.2f, ch * 0.75f, top.z + cosf(a0) * cw * 0.8f), m1 = v3(top.x + sinf(a1) * cw * 0.8f - sinf(a) * tilt * ch * 0.2f, ch * 0.75f, top.z + cosf(a1) * cw * 0.8f);
            float s2 = 0.75f + 0.25f * sinf(a0 * 2 + k); quad(p0, p1, m1, m0, 0.65f * s2, 0.45f * s2, 1.0f * s2); tri(m0, m1, top, 0.85f, 0.7f, 1.0f); } } }
static void m_rockpile(float h, float sx, float r, float g, float bl) { ball(v3(0, h * 0.4f, 0), h * 0.6f * sx, h * 0.45f, h * 0.5f, 0.2f, r, g, bl); ball(v3(h * 0.5f, h * 0.2f, h * 0.2f), h * 0.3f, h * 0.25f, h * 0.3f, 0.25f, r * 0.9f, g * 0.9f, bl * 0.9f); }
static void m_sheep(float h, int lamb) { float s = lamb ? 0.75f : 1.0f; h = 1.1f * s;
    for (int k = 0; k < 4; k++) box(v3((k & 1 ? 0.25f : -0.25f) * s, 0.25f * s, (k & 2 ? 0.35f : -0.35f) * s), v3(0.06f * s, 0.25f * s, 0.06f * s), 0.15f, 0.12f, 0.1f);
    ball(v3(0, 0.7f * s, 0), 0.42f * s, 0.36f * s, 0.6f * s, 0.25f, 0.95f, 0.93f, 0.88f);
    ball(v3(0, 0.85f * s, 0.62f * s), 0.17f * s, 0.2f * s, 0.24f * s, 0.05f, 0.18f, 0.15f, 0.12f); (void)h; }
static void m_deer(float h) { (void)h; for (int k = 0; k < 4; k++) box(v3(k & 1 ? 0.2f : -0.2f, 0.45f, k & 2 ? 0.45f : -0.45f), v3(0.05f, 0.45f, 0.05f), 0.45f, 0.3f, 0.18f);
    ball(v3(0, 1.05f, 0), 0.3f, 0.3f, 0.7f, 0.05f, 0.65f, 0.45f, 0.28f); cyl(0, 0.6f, 0.12f, 0.1f, 1.1f, 1.6f, 6, 0.65f, 0.45f, 0.28f);
    ball(v3(0, 1.7f, 0.75f), 0.13f, 0.14f, 0.24f, 0.05f, 0.6f, 0.42f, 0.26f); for (int s = -1; s <= 1; s += 2) box(v3(s * 0.12f, 2.0f, 0.65f), v3(0.02f, 0.2f, 0.02f), 0.8f, 0.75f, 0.6f); }
static void m_lion(float h) { (void)h; for (int k = 0; k < 4; k++) box(v3(k & 1 ? 0.28f : -0.28f, 0.35f, k & 2 ? 0.55f : -0.55f), v3(0.09f, 0.35f, 0.09f), 0.85f, 0.65f, 0.3f);
    ball(v3(0, 0.9f, 0), 0.38f, 0.36f, 0.85f, 0.05f, 0.9f, 0.7f, 0.35f); ball(v3(0, 1.15f, 0.85f), 0.45f, 0.45f, 0.35f, 0.3f, 0.6f, 0.38f, 0.15f); ball(v3(0, 1.1f, 1.1f), 0.22f, 0.24f, 0.25f, 0.05f, 0.9f, 0.72f, 0.38f);
    cyl(0, -0.8f, 0.05f, 0.04f, 0.9f, 0.6f, 5, 0.85f, 0.65f, 0.3f); }
static void m_boat(float h) { float L = h * 1.3f, W = h * 0.35f;   /* a fishing boat with a mast and a furled sail */
    Vec3 bowl = v3(0, h * 0.35f, L * 0.55f), sl = v3(-W, h * 0.35f, 0), sr = v3(W, h * 0.35f, 0), sb = v3(0, h * 0.35f, -L * 0.45f), kb = v3(0, 0, L * 0.3f), ks = v3(0, 0, -L * 0.3f);
    tri(bowl, sl, kb, 0.5f, 0.32f, 0.18f); tri(bowl, kb, sr, 0.45f, 0.3f, 0.16f); quad(sl, sb, ks, kb, 0.55f, 0.36f, 0.2f); quad(sr, kb, ks, sb, 0.5f, 0.33f, 0.18f); tri(sb, sl, sr, 0.4f, 0.27f, 0.15f);
    tri(bowl, sr, sl, 0.35f, 0.25f, 0.15f); tri(sl, sr, sb, 0.35f, 0.25f, 0.15f);   /* the deck */
    cyl(0, 0, 0.08f, 0.06f, h * 0.35f, h * 1.5f, 6, 0.4f, 0.3f, 0.2f); box(v3(0, h * 1.2f, 0), v3(W * 1.1f, 0.12f, 0.12f), 0.92f, 0.88f, 0.75f); }
static void m_net(float h) { for (int s = -1; s <= 1; s += 2) cyl(s * h * 0.6f, 0, 0.07f, 0.06f, 0, h, 6, 0.4f, 0.3f, 0.2f);
    for (int k = 0; k <= 5; k++) box(v3(0, h * (0.2f + 0.14f * k), 0), v3(h * 0.6f, 0.02f, 0.02f), 0.75f, 0.7f, 0.55f);
    for (int k = -5; k <= 5; k++) box(v3(k * h * 0.11f, h * 0.55f, 0), v3(0.02f, h * 0.35f, 0.02f), 0.75f, 0.7f, 0.55f); }
static void m_person(float h, float r, float g, float bl, int helmet, int priest) {   /* robe, head, arms, a headcloth or a helmet */
    float s = h / 1.8f; cone(0, 0, 0, 0.38f * s, 1.45f * s, 9, r, g, bl);
    if (priest) box(v3(0, 0.95f * s, 0.12f * s), v3(0.2f * s, 0.2f * s, 0.05f * s), 0.3f, 0.4f, 0.9f);   /* the breastplate */
    for (int sd = -1; sd <= 1; sd += 2) box(v3(sd * 0.27f * s, 1.05f * s, 0.05f * s), v3(0.08f * s, 0.3f * s, 0.08f * s), r * 0.9f, g * 0.9f, bl * 0.9f);
    ball(v3(0, 1.55f * s, 0), 0.17f * s, 0.2f * s, 0.17f * s, 0.02f, 0.82f, 0.62f, 0.45f);
    if (helmet) { cone(0, 0, 1.6f * s, 0.2f * s, 0.25f * s, 8, 0.75f, 0.6f, 0.3f); box(v3(0, 1.62f * s, 0), v3(0.21f * s, 0.04f * s, 0.21f * s), 0.75f, 0.6f, 0.3f); }
    else ball(v3(0, 1.66f * s, -0.03f * s), 0.19f * s, 0.12f * s, 0.2f * s, 0.05f, priest ? 0.95f : r * 1.1f, priest ? 0.95f : g * 1.1f, priest ? 0.95f : bl * 1.1f); }
static void m_banner(float h, float r, float g, float bl) { cyl(0, 0, 0.08f, 0.06f, 0, h, 6, 0.4f, 0.3f, 0.2f);
    for (int k = 0; k < 4; k++) { float x0 = 0.1f + k * h * 0.12f, x1 = x0 + h * 0.12f, w0 = sinf(k * 0.9f) * 0.12f, w1 = sinf((k + 1) * 0.9f) * 0.12f;
        quad(v3(x0, h * 0.95f, w0), v3(x1, h * 0.95f, w1), v3(x1, h * 0.6f, w1), v3(x0, h * 0.6f, w0), k & 1 ? r * 0.85f : r, k & 1 ? g * 0.85f : g, k & 1 ? bl * 0.85f : bl); } }
static void m_cloud(float h) { for (int k = 0; k < 6; k++) { float a = k * 1.1f; ball(v3(sinf(a) * h * 0.5f * (k > 0), h * 0.5f + (rnd() - 0.5f) * h * 0.2f, cosf(a) * h * 0.25f * (k > 0)), h * 0.35f, h * 0.25f, h * 0.3f, 0.12f, 1, 1, 1); } }
static void m_cart(float h) { (void)h; box(v3(0, 0.8f, 0), v3(0.8f, 0.35f, 1.1f), 0.55f, 0.38f, 0.2f); for (int s = -1; s <= 1; s += 2) { cyl(s * 0.9f, 0, 0.5f, 0.5f, 0, 0, 1, 0, 0, 0);
        for (int q = 0; q < 10; q++) { float a0 = q * 0.628f, a1 = a0 + 0.628f; quad(v3(s * 0.9f, 0.55f + sinf(a0) * 0.5f, cosf(a0) * 0.5f), v3(s * 0.9f, 0.55f + sinf(a1) * 0.5f, cosf(a1) * 0.5f), v3(s * 1.0f, 0.55f + sinf(a1) * 0.5f, cosf(a1) * 0.5f), v3(s * 1.0f, 0.55f + sinf(a0) * 0.5f, cosf(a0) * 0.5f), 0.35f, 0.25f, 0.15f); }
        tri(v3(s * 0.88f, 0.55f, 0), v3(s * 0.88f, 1.05f, 0), v3(s * 0.88f, 0.55f, 0.5f), 0.4f, 0.3f, 0.18f); }
    box(v3(0, 0.75f, 1.6f), v3(0.05f, 0.05f, 0.6f), 0.45f, 0.32f, 0.18f); box(v3(0, 1.2f, 0), v3(0.7f, 0.1f, 0.9f), 0.85f, 0.75f, 0.45f); }
static void m_ark(float h) { float L = h * 1.8f, W = h * 0.55f;   /* Noah's ark: a long hull with a house on deck and a roof */
    quad(v3(-W, h * 0.4f, -L), v3(W, h * 0.4f, -L), v3(W * 0.7f, 0, -L * 0.85f), v3(-W * 0.7f, 0, -L * 0.85f), 0.45f, 0.3f, 0.15f);
    quad(v3(-W, h * 0.4f, L), v3(-W * 0.7f, 0, L * 0.85f), v3(W * 0.7f, 0, L * 0.85f), v3(W, h * 0.4f, L), 0.45f, 0.3f, 0.15f);
    for (int s = -1; s <= 1; s += 2) quad(v3(s * W, h * 0.4f, -L), v3(s * W, h * 0.4f, L), v3(s * W * 0.7f, 0, L * 0.85f), v3(s * W * 0.7f, 0, -L * 0.85f), 0.5f, 0.33f, 0.17f);
    box(v3(0, h * 0.62f, 0), v3(W * 0.6f, h * 0.22f, L * 0.6f), 0.55f, 0.38f, 0.2f);
    for (int s = -1; s <= 1; s += 2) quad(v3(s * W * 0.7f, h * 0.84f, -L * 0.65f), v3(s * W * 0.7f, h * 0.84f, L * 0.65f), v3(0, h * 1.1f, L * 0.65f), v3(0, h * 1.1f, -L * 0.65f), 0.4f, 0.28f, 0.14f); }
static void m_tomb(float h) { ball(v3(0, h * 0.4f, 0), h * 0.9f, h * 0.6f, h * 0.7f, 0.12f, 0.72f, 0.68f, 0.6f);   /* the garden tomb: a rock hill, a dark doorway, the great stone rolled away */
    box(v3(0, h * 0.28f, h * 0.68f), v3(h * 0.18f, h * 0.28f, 0.05f), 0.05f, 0.04f, 0.04f);
    cyl(h * 0.5f, h * 0.72f, h * 0.32f, h * 0.32f, 0, 0, 1, 0, 0, 0);
    for (int q = 0; q < 12; q++) { float a0 = q * 0.5236f, a1 = a0 + 0.5236f; Vec3 c = v3(h * 0.52f, h * 0.33f, h * 0.72f);
        tri(c, v3(c.x, c.y + sinf(a0) * h * 0.32f, c.z + cosf(a0) * h * 0.32f), v3(c.x, c.y + sinf(a1) * h * 0.32f, c.z + cosf(a1) * h * 0.32f), 0.62f, 0.6f, 0.55f);
        quad(v3(c.x - 0.25f, c.y + sinf(a0) * h * 0.32f, c.z + cosf(a0) * h * 0.32f), v3(c.x - 0.25f, c.y + sinf(a1) * h * 0.32f, c.z + cosf(a1) * h * 0.32f), v3(c.x + 0.25f, c.y + sinf(a1) * h * 0.32f, c.z + cosf(a1) * h * 0.32f), v3(c.x + 0.25f, c.y + sinf(a0) * h * 0.32f, c.z + cosf(a0) * h * 0.32f), 0.55f, 0.52f, 0.48f); } }
static void m_crosses(float h) { for (int k = -1; k <= 1; k++) { float s = k == 0 ? 1.0f : 0.8f, x = k * h * 0.8f;
    box(v3(x, h * 0.5f * s, 0), v3(0.15f, h * 0.5f * s, 0.15f), 0.45f, 0.32f, 0.2f); box(v3(x, h * 0.78f * s, 0), v3(h * 0.22f * s, 0.12f, 0.13f), 0.45f, 0.32f, 0.2f); }
    m_rockpile(h * 0.25f, 3.0f, 0.5f, 0.45f, 0.38f); }
static void m_stable(float h) { for (int k = 0; k < 4; k++) cyl(k & 1 ? h * 0.6f : -h * 0.6f, k & 2 ? h * 0.4f : -h * 0.4f, 0.1f, 0.09f, 0, h * 0.7f, 6, 0.4f, 0.3f, 0.18f);
    quad(v3(-h * 0.75f, h * 0.7f, -h * 0.5f), v3(h * 0.75f, h * 0.7f, -h * 0.5f), v3(h * 0.75f, h * 0.9f, h * 0.5f), v3(-h * 0.75f, h * 0.9f, h * 0.5f), 0.7f, 0.6f, 0.3f);
    box(v3(0, h * 0.12f, 0), v3(h * 0.25f, h * 0.12f, h * 0.15f), 0.55f, 0.4f, 0.22f); box(v3(0, h * 0.25f, 0), v3(h * 0.2f, 0.04f, h * 0.1f), 0.95f, 0.85f, 0.5f); }   /* the manger, with straw */
static void m_camp(float h) { for (int k = 0; k < 6; k++) { float a = k * 1.047f; box(v3(sinf(a) * h * 0.25f, 0.12f, cosf(a) * h * 0.25f), v3(0.15f, 0.12f, 0.15f), 0.45f, 0.43f, 0.4f); }
    for (int k = 0; k < 3; k++) { float a = k * 2.1f; quad(v3(sinf(a) * 0.3f, 0.1f, cosf(a) * 0.3f), v3(sinf(a + 0.6f) * 0.3f, 0.1f, cosf(a + 0.6f) * 0.3f), v3(0, h * 0.35f, 0), v3(0, h * 0.35f, 0), 1.0f, 0.55f, 0.15f); } }

int model_known(const char *img) {
    static const char *names[] = {"#tree", "#olive", "#fruittree", "#treeoflife", "#palm", "#cypress", "#flowers", "#lily", "#fern", "#lantern", "#crystal", "#sheep", "#lamb",
        "#deer", "#lion", "#boat", "#net", "#priest", "#israelite", "#philistine", "#banner_israel", "#banner_philistine", "#cloud", "#cart", "#ark", "#tomb", "#crosses",
        "#stable", "#camp", "#stone", "#rubble"};
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) if (!strcmp(img, names[i])) return 1;
    return 0; }
int model_build(Batch *b, const char *img, float h, unsigned seed) {
    B = b; g_seed = seed * 2654435761u + 12345u; int n0 = (int)b->n;
    if (!strcmp(img, "#tree")) m_tree(h, 0); else if (!strcmp(img, "#olive")) m_tree(h, 1); else if (!strcmp(img, "#fruittree")) m_tree(h, 2); else if (!strcmp(img, "#treeoflife")) m_tree(h, 3);
    else if (!strcmp(img, "#palm")) m_palm(h); else if (!strcmp(img, "#cypress")) m_cypress(h);
    else if (!strcmp(img, "#flowers")) m_flowers(h * 0.4f, 0); else if (!strcmp(img, "#lily")) m_flowers(h * 0.5f, 1);
    else if (!strcmp(img, "#fern")) { for (int k = 0; k < 7; k++) { float a = k * 0.9f; blade(v3(0, 0, 0), v3_norm(v3(sinf(a), 0.9f, cosf(a))), h * 0.8f, h * 0.12f, h * 0.4f, 0.2f, 0.55f, 0.2f); } }
    else if (!strcmp(img, "#lantern")) m_lantern(h); else if (!strcmp(img, "#crystal")) m_crystal(h);
    else if (!strcmp(img, "#sheep")) m_sheep(h, 0); else if (!strcmp(img, "#lamb")) m_sheep(h, 1); else if (!strcmp(img, "#deer")) m_deer(h); else if (!strcmp(img, "#lion")) m_lion(h);
    else if (!strcmp(img, "#boat")) m_boat(h); else if (!strcmp(img, "#net")) m_net(h);
    else if (!strcmp(img, "#priest")) m_person(h, 0.95f, 0.95f, 0.92f, 0, 1); else if (!strcmp(img, "#israelite")) m_person(h, 0.6f, 0.45f, 0.3f, 0, 0); else if (!strcmp(img, "#philistine")) m_person(h, 0.7f, 0.2f, 0.18f, 1, 0);
    else if (!strcmp(img, "#banner_israel")) m_banner(h, 0.25f, 0.45f, 0.9f); else if (!strcmp(img, "#banner_philistine")) m_banner(h, 0.85f, 0.2f, 0.15f);
    else if (!strcmp(img, "#cloud")) m_cloud(h); else if (!strcmp(img, "#cart")) m_cart(h); else if (!strcmp(img, "#ark")) m_ark(h);
    else if (!strcmp(img, "#tomb")) m_tomb(h); else if (!strcmp(img, "#crosses")) m_crosses(h); else if (!strcmp(img, "#stable")) m_stable(h); else if (!strcmp(img, "#camp")) m_camp(h);
    else if (!strcmp(img, "#stone")) ball(v3(0, h * 0.5f, 0), h * 0.5f, h * 0.5f, h * 0.5f, 0.15f, 0.62f, 0.58f, 0.52f);   /* a rolling stone: round */
    else if (!strcmp(img, "#rubble")) m_rockpile(h * 0.6f, 1.5f, 0.75f, 0.68f, 0.55f);
    return (int)b->n - n0; }

/* a kart tire, 1 m across... radius 1, axle along x from -0.5 to 0.5: black rubber with raised tread blocks round the
   outside, rounded shoulders, and a gold rim with five spokes and a hub on each side. Turned about x to roll. */
int model_tire(Batch *b) {
    B = b; int n0 = (int)b->n; const int N = 24; const float rr = 0.25f, gg = 0.24f, bb = 0.25f;   /* rubber: dark grey so its shape still reads */
    for (int i = 0; i < N; i++) {
        float a0 = i * 6.2831853f / N, a1 = (i + 1) * 6.2831853f / N, lug = (i & 1) ? 1.0f : 0.93f, sh = (i & 1) ? 1.0f : 0.7f;
        Vec3 o0 = v3(0, sinf(a0), cosf(a0)), o1 = v3(0, sinf(a1), cosf(a1));
        #define RP(o, rad, x) v3(x, (o).y * (rad), (o).z * (rad))
        /* the tread face across the width, and the shoulders curving down to the sidewalls */
        quad(RP(o0, lug, -0.4f), RP(o1, lug, -0.4f), RP(o1, lug, 0.4f), RP(o0, lug, 0.4f), rr * sh, gg * sh, bb * sh);
        for (int s = -1; s <= 1; s += 2) {
            quad(RP(o0, lug, s * 0.4f), RP(o1, lug, s * 0.4f), RP(o1, 0.9f, s * 0.5f), RP(o0, 0.9f, s * 0.5f), rr * 0.85f, gg * 0.85f, bb * 0.85f);
            quad(RP(o0, 0.9f, s * 0.5f), RP(o1, 0.9f, s * 0.5f), RP(o1, 0.64f, s * 0.5f), RP(o0, 0.64f, s * 0.5f), rr * 0.75f, gg * 0.75f, bb * 0.75f);   /* sidewall */
            /* the rim: a gold ring set in a little, then the dish */
            quad(RP(o0, 0.64f, s * 0.5f), RP(o1, 0.64f, s * 0.5f), RP(o1, 0.56f, s * 0.46f), RP(o0, 0.56f, s * 0.46f), 0.95f, 0.78f, 0.32f);
            quad(RP(o0, 0.56f, s * 0.46f), RP(o1, 0.56f, s * 0.46f), RP(o1, 0.18f, s * 0.4f), RP(o0, 0.18f, s * 0.4f), 0.45f, 0.32f, 0.16f);
            tri(v3(s * 0.44f, 0, 0), RP(o0, 0.18f, s * 0.44f), RP(o1, 0.18f, s * 0.44f), 0.98f, 0.85f, 0.4f);   /* hub cap */
        }
        if (lug == 1.0f) for (int s = -1; s <= 1; s += 2) quad(RP(o0, 0.93f, s * 0.4f), RP(o0, 1.0f, s * 0.4f), RP(o0, 1.0f, -s * 0.4f), RP(o0, 0.93f, -s * 0.4f), rr * 0.6f, gg * 0.6f, bb * 0.6f);   /* the block's leading edge */
        #undef RP
    }
    for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 5; k++) {   /* five gold spokes on each side: they show the wheel turning */
        float a = k * 6.2831853f / 5, ca = cosf(a), sa = sinf(a), w = 0.07f, x = s * 0.43f;
        Vec3 p0 = v3(x, sa * 0.16f - ca * w, ca * 0.16f + sa * w), p1 = v3(x, sa * 0.16f + ca * w, ca * 0.16f - sa * w), p2 = v3(x, sa * 0.58f + ca * w, ca * 0.58f - sa * w), p3 = v3(x, sa * 0.58f - ca * w, ca * 0.58f + sa * w);
        quad(p0, p1, p2, p3, 1.0f, 0.84f, 0.38f); }
    return (int)b->n - n0;
}
