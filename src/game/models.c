// models.c - the scenery and road obstacles as real 3D models: smooth rounded shapes, real surfaces (plaster, stone,
// wood, bark, leaves, palm fronds, cloth, rock, wool, thatch - all drawn here, no picture files needed) and corners
// marked to sway in the wind. Every model is built standing on the ground at (0, 0, 0) facing +z, then turned and placed.
#include "models.h"
#include <math.h>
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ the surfaces ------------------------------------------------------------------ */
static float hsh(int x, int y, int s) { uint32_t n = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)s * 1442695041u; n = (n ^ (n >> 13)) * 1274126177u; n ^= n >> 16; return (float)(n & 0xFFFF) / 65535.0f; }
static float pnoise(float x, float y, int per, int s) {   /* smooth noise that repeats every 'per' cells, so the surfaces tile without seams */
    int xi = (int)floorf(x), yi = (int)floorf(y); float fx = x - xi, fy = y - yi; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    int x0 = ((xi % per) + per) % per, y0 = ((yi % per) + per) % per, x1 = (x0 + 1) % per, y1 = (y0 + 1) % per;
    float a = hsh(x0, y0, s), b = hsh(x1, y0, s), c = hsh(x0, y1, s), d = hsh(x1, y1, s);
    return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy; }
static float fbm(float u, float v, int base, int oct, int s) { float t = 0, a = 0.5f, w = 0; for (int o = 0; o < oct; o++) { int per = base << o; t += pnoise(u * per, v * per, per, s + o * 17) * a; w += a; a *= 0.5f; } return t / w; }
static uint32_t px(float l, float r, float g, float b, float a) {
    int R = (int)(clampf(l * r, 0, 1) * 255), G = (int)(clampf(l * g, 0, 1) * 255), B = (int)(clampf(l * b, 0, 1) * 255), A = (int)(clampf(a, 0, 1) * 255);
    return ((uint32_t)R << 24) | ((uint32_t)G << 16) | ((uint32_t)B << 8) | (uint32_t)A; }
static uint32_t g_seedt = 1; static float trnd(void) { g_seedt = g_seedt * 1664525u + 1013904223u; return (float)((g_seedt >> 8) & 0xFFFF) / 65535.0f; }
/* stamps a leaf (a pointed oval with a lighter middle) into the picture, wrapping round the edges if 'wrap' */
static void stamp_leaf(uint32_t *o, int S, float cx, float cy, float len, float wid, float ang, float bright, int wrap, float hue) {
    float ca = cosf(ang), sa = sinf(ang); int R = (int)(len + 2);
    for (int dy = -R; dy <= R; dy++) for (int dx = -R; dx <= R; dx++) {
        float lx = dx * ca + dy * sa, ly = -dx * sa + dy * ca, t = lx / len;   /* along the leaf -1..1 */
        if (t < -1 || t > 1) continue; float half = wid * sqrtf(fmaxf(0, 1 - t * t)) * (t > 0 ? 1 - 0.35f * t : 1); if (fabsf(ly) > half) continue;
        int x = (int)(cx + dx), y = (int)(cy + dy); if (wrap) { x = ((x % S) + S) % S; y = ((y % S) + S) % S; } else if (x < 0 || y < 0 || x >= S || y >= S) continue;
        float l = bright * (0.82f + 0.25f * (1 - fabsf(ly) / (half + 0.01f))) * (0.85f + 0.15f * (t + 1) * 0.5f); if (fabsf(ly) < 0.6f) l *= 1.12f;   /* the middle vein */
        o[y * S + x] = px(l, 0.92f + hue, 1.0f, 0.82f - hue, 1); } }
int model_mat_cutout(int mat) { return mat == MAT_LEAFCARD || mat == MAT_FROND; }
void model_texture(int mat, uint32_t *o, int S) {
    g_seedt = (uint32_t)mat * 7919u + 3;
    for (int y = 0; y < S; y++) for (int x = 0; x < S; x++) { float u = (float)x / S, v = (float)y / S, l = 1; uint32_t c = px(1, 1, 1, 1, 1);
        switch (mat) {
            case MAT_PLASTER: { l = 0.98f + 0.12f * (fbm(u, v, 3, 4, 1) - 0.5f) + 0.06f * (fbm(u, v, 32, 2, 2) - 0.5f); float blot = fbm(u, v, 5, 3, 3); if (blot > 0.64f) l -= (blot - 0.64f) * 0.4f;
                float cr = fabsf(fbm(u, v, 6, 3, 4) - 0.5f); if (cr < 0.008f) l -= 0.05f; c = px(l * 1.04f, 1, 0.98f, 0.94f, 1); break; }
            case MAT_STONE: { int row = y / (S / 4), cnt = (row & 1) ? 2 : 4, bw = S / cnt, off = (row * 37) % bw, xx = (x + off) % S, bi = xx / bw;
                int mx = xx % bw, my = y % (S / 4); float edge = fminf(fminf(mx, bw - 1 - mx), fminf(my, S / 4 - 1 - my));
                if (edge < 3) l = 0.55f + 0.1f * fbm(u, v, 16, 2, 5);   /* mortar */
                else { l = 0.84f + 0.16f * hsh(bi, row, 9) + 0.14f * (fbm(u, v, 8, 4, 6) - 0.5f); if (edge < 8) l *= 0.82f + 0.18f * (edge - 3) / 5.0f; float pit = fbm(u, v, 32, 2, 7); if (pit > 0.7f) l -= 0.1f; }
                c = px(l, 1, 0.97f, 0.9f, 1); break; }
            case MAT_WOOD: { int plank = y / (S / 4), py = y % (S / 4); float g = fbm(u + hsh(plank, 0, 11), v * 0.25f, 2, 3, 12);
                l = 0.62f + 0.12f * hsh(plank, 1, 13) + 0.16f * sinf(v * S * 0.55f + g * 18.0f + plank * 3) * 0.5f + 0.1f * (fbm(u, v, 64, 1, 14) - 0.5f);
                if (py < 2 || py > S / 4 - 3) l *= 0.45f; float knot = pnoise(u * 6, v * 12, 6, 15); if (knot > 0.85f) l *= 0.75f; c = px(l, 1, 0.95f, 0.88f, 1); break; }
            case MAT_BARK: { float r = fbm(u, v * 0.25f, 8, 3, 21), fur = fabsf(sinf((u * 10 + r * 2.5f) * 3.14159f)); l = 0.35f + 0.55f * powf(fur, 0.6f) + 0.15f * (fbm(u, v, 16, 3, 22) - 0.5f);
                c = px(l, 1, 0.94f, 0.86f, 1); break; }
            case MAT_CLOTH: { int wx = x & 3, wy = y & 3; float weave = ((wx < 2) ^ (wy < 2)) ? 0.06f : -0.04f; l = 0.86f + weave + 0.08f * (fbm(u, v, 4, 3, 31) - 0.5f) + 0.05f * sinf(u * 6.2831853f * 3 + fbm(u, v, 2, 2, 32) * 4);
                c = px(l, 1, 1, 1, 1); break; }
            case MAT_ROCK: { float n = fbm(u, v, 4, 5, 41); l = 0.5f + 0.42f * n + 0.06f * sinf(v * 40 + n * 9); float cr = fabsf(pnoise(u * 6, v * 6, 6, 42) - 0.5f); if (cr < 0.025f) l *= 0.6f;
                if (fbm(u, v, 16, 2, 43) > 0.68f) l *= 1.12f; c = px(l, 1, 0.97f, 0.92f, 1); break; }
            case MAT_WOOL: { float n = fbm(u, v, 16, 3, 51); l = 0.78f + 0.2f * sinf(n * 40.0f) * 0.5f + 0.12f * (fbm(u, v, 4, 2, 52) - 0.5f); c = px(l, 1, 0.99f, 0.95f, 1); break; }
            case MAT_THATCH: { int band = y / (S / 4), by = y % (S / 4); float strand = hsh(x / 3, band, 61), n = pnoise((float)x / 3, v * 8, S / 3, 62);
                l = 0.55f + 0.3f * strand + 0.15f * n; l *= 0.6f + 0.4f * (float)by / (S / 4); c = px(l, 1, 0.92f, 0.7f, 1); break; }
            case MAT_LEAF: c = px(0.3f + 0.15f * fbm(u, v, 8, 2, 71), 0.9f, 1, 0.8f, 1); break;
            case MAT_LEAFCARD: case MAT_FROND: c = 0; break;   /* see-through until the leaves are drawn in */
            default: break; }
        o[y * S + x] = c; }
    if (mat == MAT_LEAF) for (int k = 0; k < 900; k++) stamp_leaf(o, S, trnd() * S, trnd() * S, 7 + trnd() * 6, 3 + trnd() * 2, trnd() * 6.28f, 0.55f + trnd() * 0.5f, 1, (trnd() - 0.5f) * 0.25f);
    if (mat == MAT_LEAFCARD) for (int k = 0; k < 260; k++) { float a = trnd() * 6.28f, d = sqrtf(trnd()) * S * 0.38f;
        stamp_leaf(o, S, S / 2 + cosf(a) * d, S / 2 + sinf(a) * d, 9 + trnd() * 6, 4 + trnd() * 2, a + (trnd() - 0.5f) * 1.5f, 0.6f + trnd() * 0.5f, 0, (trnd() - 0.5f) * 0.25f); }
    if (mat == MAT_FROND) {   /* u across the frond, v along it: a stem down the middle and leaflets slanting out toward the tip */
        for (int y = 0; y < S; y++) for (int x = S / 2 - 2; x <= S / 2 + 1; x++) o[y * S + x] = px(0.75f, 0.95f, 1, 0.7f, 1);
        for (int k = 0; k < 46; k++) { float vy = (k + 0.5f) / 46 * S; for (int sd = -1; sd <= 1; sd += 2) { float len = S * 0.24f * (0.85f + 0.15f * trnd()), ang = sd > 0 ? 0.45f : 3.14159f - 0.45f;
            stamp_leaf(o, S, S / 2 + sd * len * 0.92f, vy + len * 0.36f, len, 3.2f, ang, 0.7f + trnd() * 0.35f, 0, (trnd() - 0.5f) * 0.2f); } } }
}

/* ------------------------------------------------------------------ building ------------------------------------------------------------------ */
static ModelOut *O; static int M = MAT_PLAIN; static float US = 2.0f;   /* the surface being laid, and meters per repeat of its picture */
static unsigned g_seed; static float rnd(void) { g_seed = g_seed * 1664525u + 1013904223u; return (float)((g_seed >> 8) & 0xFFFF) / 65535.0f; }
static int SWM = 0; static float SW_A = 0, SW_Y0 = 0, SW_H = 1; static Vec3 SW_C; static float SW_R = 1;   /* wind: by height above y0, or by distance from a point */
static Vec3 vadd(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static Vec3 vmul(Vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static float vlen(Vec3 a) { return sqrtf(v3_dot(a, a)); }
static void mat(int m) { M = m; US = m == MAT_STONE ? 2.2f : m == MAT_PLASTER ? 3.0f : m == MAT_WOOD ? 1.6f : m == MAT_BARK ? 1.2f : m == MAT_ROCK ? 3.0f : m == MAT_THATCH ? 1.5f : m == MAT_CLOTH ? 1.5f : m == MAT_WOOL ? 0.8f : 1.5f; }
static void nosway(void) { SW_A = 0; }
static void vtx(Vec3 p, Vec3 n, float u, float v, float r, float g, float b) {
    Batch *bb = O->b[M]; float w = 0, L = vlen(n);
    if (SW_A > 0) { float t = SWM ? vlen(v3_sub(p, SW_C)) / SW_R : (p.y - SW_Y0) / SW_H; if (t > 0) w = SW_A * fminf(t * t, 1.6f); }
    n = L < 1e-6f ? v3(0, 1, 0) : vmul(n, (w > 0.002f ? 1.05f + w : 1.0f) / L);
    Vertex vx = {p.x, p.y, p.z, n.x, n.y, n.z, u, v, r, g, b, 1}; bb->v[bb->n++] = vx; }
static int room(int k) { Batch *bb = O->b[M]; return bb->n + (uint32_t)k <= bb->cap; }
static Vec3 nrm(Vec3 a, Vec3 b, Vec3 c) { return v3_norm(v3_cross(v3_sub(b, a), v3_sub(c, a))); }
static void puv(Vec3 p, Vec3 n, float *u, float *v) {   /* a flat surface's picture laid straight on it, US meters per repeat */
    if (fabsf(n.y) > 0.7f) { *u = p.x / US; *v = p.z / US; return; }
    Vec3 t = v3_norm(v3_cross(v3(0, 1, 0), n)); *u = v3_dot(p, t) / US; *v = -p.y / US; }
static void ftri(Vec3 a, Vec3 b, Vec3 c, float r, float g, float bl) {
    if (!room(3)) return; Vec3 n = nrm(a, b, c); float u, v;
    puv(a, n, &u, &v); vtx(a, n, u, v, r, g, bl); puv(b, n, &u, &v); vtx(b, n, u, v, r, g, bl); puv(c, n, &u, &v); vtx(c, n, u, v, r, g, bl); }
static void fquad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, float r, float g, float bl) { ftri(a, b, c, r, g, bl); ftri(a, c, d, r, g, bl); }
/* a triangle with its own normals and picture positions (smooth surfaces) */
static void stri(Vec3 a, Vec3 na, float ua, float va, Vec3 b, Vec3 nb, float ub, float vb, Vec3 c, Vec3 nc, float uc, float vc, float r, float g, float bl, float sa, float sb, float sc) {
    if (!room(3)) return; vtx(a, na, ua, va, r * sa, g * sa, bl * sa); vtx(b, nb, ub, vb, r * sb, g * sb, bl * sb); vtx(c, nc, uc, vc, r * sc, g * sc, bl * sc); }
static void cbox(Vec3 c, Vec3 h, float r, float g, float bl) {   /* a plain box */
    Vec3 p[8]; for (int i = 0; i < 8; i++) p[i] = v3(c.x + (i & 1 ? h.x : -h.x), c.y + (i & 2 ? h.y : -h.y), c.z + (i & 4 ? h.z : -h.z));
    fquad(p[4], p[5], p[7], p[6], r, g, bl); fquad(p[1], p[0], p[2], p[3], r, g, bl); fquad(p[5], p[1], p[3], p[7], r, g, bl);
    fquad(p[0], p[4], p[6], p[2], r, g, bl); fquad(p[6], p[7], p[3], p[2], r, g, bl); fquad(p[0], p[1], p[5], p[4], r, g, bl); }
/* a box with its upright corners and top edges rounded off (bevel b): plaster walls, stones, beams */
static void rbox(Vec3 c, float hx, float hy, float hz, float b, float r, float g, float bl) {
    b = fminf(b, fminf(fminf(hx, hz) * 0.9f, hy)); float y0 = c.y - hy, y1 = c.y + hy - b, y2 = c.y + hy;
    Vec3 ring[3][8]; float ex[3] = {hx, hx, hx - b * 0.6f}, ez[3] = {hz, hz, hz - b * 0.6f}, ys[3] = {y0, y1, y2};
    for (int k = 0; k < 3; k++) { float X = ex[k], Z = ez[k], e = b * (k == 2 ? 1.0f : 0.7f);
        Vec3 q[8] = {v3(X - e, 0, Z), v3(X, 0, Z - e), v3(X, 0, -Z + e), v3(X - e, 0, -Z), v3(-X + e, 0, -Z), v3(-X, 0, -Z + e), v3(-X, 0, Z - e), v3(-X + e, 0, Z)};
        for (int i = 0; i < 8; i++) ring[k][i] = v3(c.x + q[i].x, ys[k], c.z + q[i].z); }
    for (int i = 0; i < 8; i++) { int j = (i + 1) % 8; float sh = (i & 1) ? 0.97f : 1.0f;
        fquad(ring[0][j], ring[0][i], ring[1][i], ring[1][j], r * sh, g * sh, bl * sh);
        fquad(ring[1][j], ring[1][i], ring[2][i], ring[2][j], r * 1.03f, g * 1.03f, bl * 1.03f); }
    for (int i = 1; i < 7; i++) ftri(ring[2][0], ring[2][i + 1], ring[2][i], r * 1.04f, g * 1.04f, bl * 1.04f); }
/* a smooth bent tube through np points with radius rad[i] (trunks, branches, legs, poles, robes); fl = flutes (columns) */
static int FLUTES = 0;
static void tube(const Vec3 *p, const float *rad, int np, int sides, float r, float g, float bl, int cap) {
    Vec3 nref = v3(1, 0, 0); float vlen_acc = 0; Vec3 prevring[32], prevn[32]; float prevv = 0;
    if (sides > 32) sides = 32;
    for (int i = 0; i < np; i++) {
        Vec3 t = v3_norm(v3_sub(p[i < np - 1 ? i + 1 : i], p[i > 0 ? i - 1 : i]));
        Vec3 sd = v3_cross(t, nref); if (vlen(sd) < 0.2f) { nref = v3(0, 0, 1); sd = v3_cross(t, nref); } sd = v3_norm(sd); Vec3 up = v3_norm(v3_cross(sd, t)); nref = up;   /* carries the frame along the bend */
        if (i > 0) vlen_acc += vlen(v3_sub(p[i], p[i - 1]));
        float circ = 6.2831853f * rad[i], ur = fmaxf(1.0f, roundf(circ / US)), vv = vlen_acc / US;
        Vec3 ring[33], rn[33];
        for (int k = 0; k <= sides; k++) { float a = k * 6.2831853f / sides, rr = rad[i]; if (FLUTES) rr *= 1.0f - 0.06f * (0.5f + 0.5f * cosf(a * FLUTES));
            Vec3 d = vadd(vmul(sd, cosf(a)), vmul(up, sinf(a))); ring[k] = vadd(p[i], vmul(d, rr)); rn[k] = d; }
        if (i > 0) for (int k = 0; k < sides; k++) { float u0 = (float)k / sides * ur, u1 = (float)(k + 1) / sides * ur;
            float s0 = 0.8f + 0.2f * (rn[k].y * 0.5f + 0.5f), s1 = 0.8f + 0.2f * (rn[k + 1].y * 0.5f + 0.5f), q0 = 0.8f + 0.2f * (prevn[k].y * 0.5f + 0.5f), q1 = 0.8f + 0.2f * (prevn[k + 1].y * 0.5f + 0.5f);
            stri(prevring[k], prevn[k], u0, prevv, prevring[k + 1], prevn[k + 1], u1, prevv, ring[k + 1], rn[k + 1], u1, vv, r, g, bl, q0, q1, s1);
            stri(prevring[k], prevn[k], u0, prevv, ring[k + 1], rn[k + 1], u1, vv, ring[k], rn[k], u0, vv, r, g, bl, q0, s1, s0); }
        for (int k = 0; k <= sides; k++) { prevring[k] = ring[k]; prevn[k] = rn[k]; } prevv = vv;
        if (cap && i == np - 1 && rad[i] > 0.01f) for (int k = 0; k < sides; k++) ftri(p[i], ring[k], ring[k + 1], r * 1.05f, g * 1.05f, bl * 1.05f); } }
static void tube2(Vec3 a, Vec3 b, float ra, float rb, int sides, float r, float g, float bl) { Vec3 p[2] = {a, b}; float rr[2] = {ra, rb}; tube(p, rr, 2, sides, r, g, bl, 1); }
static float n3(Vec3 d, unsigned s) {   /* smooth noise on a direction, for lumps that join up all the way round */
    float x = d.x * 2.2f + 9, y = d.y * 2.2f + 9, z = d.z * 2.2f + 9; int xi = (int)floorf(x), yi = (int)floorf(y), zi = (int)floorf(z); float fx = x - xi, fy = y - yi, fz = z - zi;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz); float c[2][2][2];
    for (int a = 0; a < 2; a++) for (int b = 0; b < 2; b++) for (int e = 0; e < 2; e++) c[a][b][e] = hsh(xi + a + (zi + e) * 57, yi + b, (int)s);
    float x00 = c[0][0][0] + (c[1][0][0] - c[0][0][0]) * fx, x10 = c[0][1][0] + (c[1][1][0] - c[0][1][0]) * fx, x01 = c[0][0][1] + (c[1][0][1] - c[0][0][1]) * fx, x11 = c[0][1][1] + (c[1][1][1] - c[0][1][1]) * fx;
    float y0 = x00 + (x10 - x00) * fy, y1 = x01 + (x11 - x01) * fy; return y0 + (y1 - y0) * fz; }
/* a smooth rounded body (canopies, wool, clouds, rocks, heads): radii rx ry rz, lumps of size 'bump', darker underneath */
static void blob(Vec3 c, float rx, float ry, float rz, float bump, int R, int S, float r, float g, float bl) {
    if (R > 12) R = 12; if (S > 18) S = 18; Vec3 p[13][19]; unsigned sd = (unsigned)(rnd() * 60000);
    for (int i = 0; i <= R; i++) for (int j = 0; j <= S; j++) { float th = 3.14159265f * i / R, ph = 6.2831853f * j / S; Vec3 d = v3(sinf(th) * cosf(ph), cosf(th), sinf(th) * sinf(ph));
        float l = 1.0f + bump * (n3(d, sd) * 2 - 1) * 1.4f; p[i][j] = v3(c.x + d.x * rx * l, c.y + d.y * ry * l, c.z + d.z * rz * l); }
    Vec3 nn[13][19];
    for (int i = 0; i <= R; i++) for (int j = 0; j <= S; j++) {
        if (i == 0 || i == R) { nn[i][j] = v3(0, i == 0 ? 1.0f : -1.0f, 0); continue; }
        Vec3 a = v3_sub(p[i][(j + 1) % S], p[i][(j + S - 1) % S]), b = v3_sub(p[i + 1][j], p[i - 1][j]); Vec3 n = v3_norm(v3_cross(a, b));
        if (v3_dot(n, v3_sub(p[i][j], c)) < 0) n = vmul(n, -1); nn[i][j] = n; }
    float ku = fmaxf(1, roundf(6.2831853f * fmaxf(rx, rz) / US)), kv = fmaxf(1, roundf(3.14159f * ry / US));
    for (int i = 0; i < R; i++) for (int j = 0; j < S; j++) {
        float u0 = (float)j / S * ku, u1 = (float)(j + 1) / S * ku, v0 = (float)i / R * kv, v1 = (float)(i + 1) / R * kv;
        float s00 = 0.62f + 0.38f * (nn[i][j].y * 0.5f + 0.5f), s01 = 0.62f + 0.38f * (nn[i][j + 1].y * 0.5f + 0.5f), s10 = 0.62f + 0.38f * (nn[i + 1][j].y * 0.5f + 0.5f), s11 = 0.62f + 0.38f * (nn[i + 1][j + 1].y * 0.5f + 0.5f);
        stri(p[i][j], nn[i][j], u0, v0, p[i + 1][j], nn[i + 1][j], u0, v1, p[i + 1][j + 1], nn[i + 1][j + 1], u1, v1, r, g, bl, s00, s10, s11);
        stri(p[i][j], nn[i][j], u0, v0, p[i + 1][j + 1], nn[i + 1][j + 1], u1, v1, p[i][j + 1], nn[i][j + 1], u1, v0, r, g, bl, s00, s11, s01); } }
/* a leaf card: a square of see-through leaves at p facing out along d (from the canopy middle cc), lit as if part of the round canopy */
static void card(Vec3 p, Vec3 d, float s, Vec3 cc, float r, float g, float bl) {
    if (!room(6)) return; Vec3 ax = v3_cross(d, fabsf(d.y) > 0.9f ? v3(1, 0, 0) : v3(0, 1, 0)); ax = vmul(v3_norm(ax), s); Vec3 ay = vmul(v3_norm(v3_cross(d, ax)), s);
    float tw = rnd() * 6.28f, ct = cosf(tw), st = sinf(tw); Vec3 ax2 = vadd(vmul(ax, ct), vmul(ay, st)), ay2 = vadd(vmul(ay, ct), vmul(ax, -st));
    Vec3 q[4] = {v3_sub(v3_sub(p, ax2), ay2), vadd(v3_sub(p, ay2), ax2), vadd(vadd(p, ax2), ay2), vadd(v3_sub(p, ax2), ay2)}; float uv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    Vec3 nq[4]; float sh[4]; for (int k = 0; k < 4; k++) { nq[k] = v3_norm(vadd(v3_norm(v3_sub(q[k], cc)), v3(0, 0.4f, 0))); sh[k] = 0.7f + 0.3f * (nq[k].y * 0.5f + 0.5f); }
    stri(q[0], nq[0], uv[0][0], uv[0][1], q[1], nq[1], uv[1][0], uv[1][1], q[2], nq[2], uv[2][0], uv[2][1], r, g, bl, sh[0], sh[1], sh[2]);
    stri(q[0], nq[0], uv[0][0], uv[0][1], q[2], nq[2], uv[2][0], uv[2][1], q[3], nq[3], uv[3][0], uv[3][1], r, g, bl, sh[0], sh[2], sh[3]); }
/* a frond or long leaf: a strip from base along dir, rising then drooping, folded a little along its stem, picture u across and v along */
static void frond(Vec3 base, Vec3 dir, float len, float wid, float rise, float droop, int segs, float r, float g, float bl) {
    Vec3 side = v3_norm(v3_cross(dir, v3(0, 1, 0))); Vec3 pl = base, pc = base, pr = base; float pv = 0;
    for (int k = 1; k <= segs; k++) { float t = (float)k / segs, w = wid * sinf(3.14159265f * fminf(t * 0.95f + 0.1f, 1.0f));
        Vec3 c = v3(base.x + dir.x * len * t, base.y + dir.y * len * t + rise * t - droop * t * t, base.z + dir.z * len * t);
        Vec3 l = vadd(vmul(side, -w), v3(c.x, c.y - w * 0.3f, c.z)), rr = vadd(vmul(side, w), v3(c.x, c.y - w * 0.3f, c.z));
        Vec3 n0 = v3_norm(v3_cross(v3_sub(c, pc), side)); if (n0.y < 0) n0 = vmul(n0, -1); Vec3 nL = v3_norm(vadd(n0, vmul(side, -0.35f))), nR = v3_norm(vadd(n0, vmul(side, 0.35f)));
        float sh = 0.75f + 0.25f * t;
        stri(pl, nL, 0, pv, pc, n0, 0.5f, pv, c, n0, 0.5f, t, r, g, bl, sh, sh, sh); stri(pl, nL, 0, pv, c, n0, 0.5f, t, l, nL, 0, t, r, g, bl, sh, sh, sh);
        stri(pc, n0, 0.5f, pv, pr, nR, 1, pv, rr, nR, 1, t, r, g, bl, sh, sh, sh); stri(pc, n0, 0.5f, pv, rr, nR, 1, t, c, n0, 0.5f, t, r, g, bl, sh, sh, sh);
        pl = l; pc = c; pr = rr; pv = t; } }

/* ------------------------------------------------------------------ the models ------------------------------------------------------------------ */
static void canopy(Vec3 c, float sz, float flat, float lr, float lg, float lb, int ncard) {   /* a mass of leaves: a leafy round body with loose leaves standing out of it */
    mat(MAT_LEAF); blob(c, sz, sz * flat, sz, 0.16f, 7, 11, lr, lg, lb);
    mat(MAT_LEAFCARD); for (int k = 0; k < ncard; k++) { float a = rnd() * 6.2831853f, e = (rnd() * 1.3f - 0.35f); Vec3 d = v3_norm(v3(cosf(a) * cosf(e), sinf(e), sinf(a) * cosf(e)));
        Vec3 p = v3(c.x + d.x * sz * 0.88f, c.y + d.y * sz * flat * 0.88f, c.z + d.z * sz * 0.88f); float v = 0.85f + rnd() * 0.3f; card(p, d, sz * (0.5f + rnd() * 0.25f), c, lr * v * 1.1f, lg * v * 1.1f, lb * v * 1.1f); } }
static void m_tree(float h, int kind) {   /* 0 broad tree, 1 olive (silver-green, gnarled), 2 fruit tree, 3 tree of life (golden fruit) */
    float tr = 0.5f, tg = 0.4f, tb = 0.3f, lr = 0.36f, lg = 0.62f, lb = 0.26f;
    if (kind == 1) { tr = 0.58f; tg = 0.54f; tb = 0.46f; lr = 0.52f; lg = 0.64f; lb = 0.44f; }
    if (kind == 3) { lr = 0.42f; lg = 0.8f; lb = 0.34f; }
    float th = h * (kind == 1 ? 0.34f : 0.42f), rad = h * (kind == 1 ? 0.06f : 0.045f), lx = (rnd() - 0.5f) * h * 0.1f, lz = (rnd() - 0.5f) * h * 0.1f;
    SWM = 0; SW_Y0 = th * 0.7f; SW_H = h - SW_Y0; SW_A = h * 0.03f;
    mat(MAT_BARK);
    { Vec3 p[6]; float rr[6]; for (int k = 0; k < 6; k++) { float t = k / 5.0f, tw = kind == 1 ? sinf(t * 7 + rnd()) * h * 0.03f : 0;
            p[k] = v3(lx * t * t + tw, -0.3f + (th + 0.3f) * t, lz * t * t + (kind == 1 ? cosf(t * 6) * h * 0.02f : 0)); rr[k] = rad * (k == 0 ? 1.7f : k == 1 ? 1.15f : 1.0f - 0.25f * t); }
        tube(p, rr, 6, 10, tr, tg, tb, 1); }
    Vec3 top = v3(lx, th, lz); int nc = kind == 1 ? 5 : 6; Vec3 cc[8]; float cs[8];
    cc[0] = v3(lx, th + h * 0.24f, lz); cs[0] = h * 0.27f;
    for (int k = 1; k < nc; k++) { float a = k * 6.2831853f / (nc - 1) + rnd() * 0.6f, d = h * (0.17f + rnd() * 0.06f);
        cc[k] = v3(lx + cosf(a) * d, th + h * (0.06f + rnd() * 0.14f), lz + sinf(a) * d); cs[k] = h * (0.18f + rnd() * 0.05f);
        mat(MAT_BARK); Vec3 bp[3] = {v3(top.x, top.y - h * 0.05f, top.z), v3(top.x + (cc[k].x - top.x) * 0.5f, top.y + (cc[k].y - top.y) * 0.3f + h * 0.03f, top.z + (cc[k].z - top.z) * 0.5f), vadd(top, vmul(v3_sub(cc[k], top), 0.85f))};
        float br[3] = {rad * 0.6f, rad * 0.4f, rad * 0.2f}; tube(bp, br, 3, 6, tr, tg, tb, 1); }
    for (int k = 0; k < nc; k++) { float v = 0.88f + rnd() * 0.22f; canopy(cc[k], cs[k], kind == 1 ? 0.62f : 0.8f, lr * v, lg * v, lb * v, 7); }
    if (kind == 2 || kind == 3) { mat(MAT_PLAIN); for (int k = 0; k < 12; k++) { int q = (int)(rnd() * nc) % nc; float a = rnd() * 6.28f, e = rnd() * 0.9f - 0.2f;
            Vec3 d = v3(cosf(a) * cosf(e), sinf(e), sinf(a) * cosf(e)); Vec3 p = v3(cc[q].x + d.x * cs[q] * 0.98f, cc[q].y + d.y * cs[q] * 0.78f, cc[q].z + d.z * cs[q] * 0.98f);
            float fr = h * 0.03f; if (kind == 3) blob(p, fr * 1.2f, fr * 1.3f, fr * 1.2f, 0.02f, 4, 6, 1.0f, 0.86f, 0.3f); else blob(p, fr, fr, fr, 0.02f, 4, 6, 0.95f, 0.3f + rnd() * 0.3f, 0.12f); } }
    nosway(); }
static void m_palm(float h) {
    float lean = 0.1f + rnd() * 0.14f, dir = rnd() * 6.28f, r0 = h * 0.045f; Vec3 p[16]; float rr[16];
    for (int k = 0; k < 16; k++) { float t = k / 15.0f; p[k] = v3(cosf(dir) * lean * h * t * t, h * 0.92f * t - 0.2f * (k == 0), sinf(dir) * lean * h * t * t); rr[k] = r0 * (1.0f - 0.3f * t) * ((k & 1) ? 1.0f : 1.1f) * (k == 0 ? 1.4f : 1.0f); }
    mat(MAT_BARK); US = 0.6f; tube(p, rr, 16, 9, 0.66f, 0.55f, 0.42f, 1);   /* the ringed trunk */
    Vec3 top = p[15]; SWM = 1; SW_C = top; SW_R = h * 0.42f; SW_A = h * 0.045f;
    mat(MAT_LEAF); blob(v3(top.x, top.y + h * 0.02f, top.z), r0 * 1.6f, r0 * 1.4f, r0 * 1.6f, 0.1f, 5, 8, 0.4f, 0.48f, 0.26f);
    mat(MAT_FROND); for (int k = 0; k < 15; k++) { int inner = k < 5; float a = k * 6.2831853f / (inner ? 5 : 10) + (inner ? 0.4f : 0) + rnd() * 0.3f, len = h * (inner ? 0.36f : 0.48f) * (0.9f + rnd() * 0.2f);
        Vec3 d = v3_norm(v3(cosf(a), inner ? 0.9f : 0.25f, sinf(a))); float gv = 0.85f + rnd() * 0.25f;
        frond(top, d, len, h * 0.11f, inner ? h * 0.06f : h * 0.08f, inner ? h * 0.12f : h * 0.3f, 8, 0.42f * gv, 0.72f * gv, 0.3f * gv); }
    nosway(); mat(MAT_PLAIN); for (int k = 0; k < 5; k++) { float a = k * 1.3f; blob(v3(top.x + cosf(a) * r0 * 1.3f, top.y - h * 0.035f, top.z + sinf(a) * r0 * 1.3f), h * 0.026f, h * 0.03f, h * 0.026f, 0.03f, 6, 9, 0.42f, 0.4f, 0.16f); } }
static void m_cypress(float h) { mat(MAT_BARK); tube2(v3(0, -0.2f, 0), v3(0, h * 0.2f, 0), h * 0.04f, h * 0.03f, 7, 0.45f, 0.36f, 0.26f);
    SWM = 0; SW_Y0 = h * 0.2f; SW_H = h * 0.8f; SW_A = h * 0.025f;
    mat(MAT_LEAF); blob(v3(0, h * 0.55f, 0), h * 0.14f, h * 0.46f, h * 0.14f, 0.08f, 9, 10, 0.22f, 0.45f, 0.24f);
    mat(MAT_LEAFCARD); for (int k = 0; k < 16; k++) { float a = rnd() * 6.28f, y = h * (0.2f + rnd() * 0.7f), rr = h * 0.14f * sqrtf(fmaxf(0.05f, 1 - powf((y - h * 0.55f) / (h * 0.46f), 2)));
        Vec3 d = v3(cosf(a), 0.2f, sinf(a)); card(v3(cosf(a) * rr, y, sinf(a) * rr), v3_norm(d), h * 0.09f, v3(0, y, 0), 0.25f, 0.5f, 0.27f); }
    nosway(); }
static void m_flowers(float h, int lily) {
    SWM = 0; SW_Y0 = 0; SW_H = h; SW_A = 0.05f;
    if (lily) { mat(MAT_PLAIN); nosway(); for (int k = 0; k < 4; k++) { float x = (rnd() - 0.5f) * h * 3, z = (rnd() - 0.5f) * h * 3, pr = h * (0.4f + rnd() * 0.2f);
            for (int q = 0; q < 14; q++) { if (q == 0) continue; float a0 = q * 6.2831853f / 15, a1 = (q + 1) * 6.2831853f / 15;
                ftri(v3(x, 0.06f, z), v3(x + cosf(a1) * pr, 0.04f, z + sinf(a1) * pr), v3(x + cosf(a0) * pr, 0.04f, z + sinf(a0) * pr), 0.3f, 0.6f + 0.05f * (q & 1), 0.28f); }
            if (k < 2) { for (int q = 0; q < 8; q++) { float a = q * 0.785f + rnd() * 0.2f, pl = h * 0.22f; Vec3 b = v3(x, 0.1f, z), tip = v3(x + cosf(a) * pl * 0.7f, 0.1f + pl * 0.8f, z + sinf(a) * pl * 0.7f), sd = v3(-sinf(a) * pl * 0.25f, 0, cosf(a) * pl * 0.25f);
                    ftri(b, vadd(vmul(vadd(b, tip), 0.5f), sd), tip, 1, 0.92f, 0.95f); ftri(b, tip, v3_sub(vmul(vadd(b, tip), 0.5f), sd), 0.98f, 0.85f, 0.9f); }
                blob(v3(x, 0.15f, z), h * 0.05f, h * 0.04f, h * 0.05f, 0.05f, 3, 6, 1, 0.85f, 0.25f); } }
        return; }
    static const float col[5][3] = {{1, 0.32f, 0.42f}, {1, 0.85f, 0.22f}, {0.66f, 0.45f, 1}, {1, 1, 1}, {1, 0.55f, 0.2f}};
    mat(MAT_PLAIN);
    for (int k = 0; k < 16; k++) { float x = (rnd() - 0.5f) * h * 2.6f, z = (rnd() - 0.5f) * h * 2.6f, a = rnd() * 6.28f, bl = h * (0.5f + rnd() * 0.4f); Vec3 sd = v3(cosf(a) * 0.03f, 0, sinf(a) * 0.03f);
        Vec3 tip = v3(x + sinf(a) * bl * 0.3f, bl, z + cosf(a) * bl * 0.3f); ftri(v3_sub(v3(x, 0, z), sd), vadd(v3(x, 0, z), sd), tip, 0.3f, 0.62f, 0.25f); ftri(vadd(v3(x, 0, z), sd), v3_sub(v3(x, 0, z), sd), tip, 0.26f, 0.55f, 0.22f); }   /* grass */
    for (int k = 0; k < 8; k++) { float x = (rnd() - 0.5f) * h * 2.2f, z = (rnd() - 0.5f) * h * 2.2f, sh = h * (0.7f + rnd() * 0.5f); int c = (int)(rnd() * 4.99f);
        tube2(v3(x, 0, z), v3(x, sh, z), 0.025f, 0.018f, 4, 0.25f, 0.55f, 0.22f);
        for (int q = 0; q < 5; q++) { float a0 = q * 1.2566f, pr = h * 0.17f; Vec3 cc = v3(x, sh, z), tip = v3(x + cosf(a0) * pr, sh + pr * 0.25f, z + sinf(a0) * pr), sd = v3(-sinf(a0) * pr * 0.35f, 0, cosf(a0) * pr * 0.35f);
            ftri(cc, vadd(vmul(vadd(cc, tip), 0.5f), sd), tip, col[c][0], col[c][1], col[c][2]); ftri(cc, tip, v3_sub(vmul(vadd(cc, tip), 0.5f), sd), col[c][0] * 0.92f, col[c][1] * 0.92f, col[c][2] * 0.92f);
            ftri(cc, tip, vadd(vmul(vadd(cc, tip), 0.5f), sd), col[c][0], col[c][1], col[c][2]); ftri(cc, v3_sub(vmul(vadd(cc, tip), 0.5f), sd), tip, col[c][0] * 0.92f, col[c][1] * 0.92f, col[c][2] * 0.92f); }
        blob(v3(x, sh + 0.02f, z), h * 0.05f, h * 0.04f, h * 0.05f, 0.05f, 3, 6, c == 1 ? 0.6f : 1, c == 1 ? 0.35f : 0.85f, 0.2f); }
    nosway(); }
static void m_fern(float h) { SWM = 1; SW_C = v3(0, 0, 0); SW_R = h; SW_A = 0.12f; mat(MAT_FROND);
    for (int k = 0; k < 9; k++) { float a = k * 0.7f + rnd() * 0.3f; frond(v3(0, 0, 0), v3_norm(v3(cosf(a), 1.1f, sinf(a))), h * 0.75f, h * 0.14f, h * 0.15f, h * 0.55f, 6, 0.32f, 0.62f, 0.26f); } nosway(); }
static void m_lantern(float h) { mat(MAT_WOOD); tube2(v3(0, 0, 0), v3(0, h * 0.8f, 0), 0.09f, 0.07f, 8, 0.35f, 0.28f, 0.2f); rbox(v3(0, 0.15f, 0), 0.2f, 0.15f, 0.2f, 0.05f, 0.4f, 0.32f, 0.22f);
    mat(MAT_PLAIN); float s = h * 0.09f, y = h * 0.86f;
    for (int k = 0; k < 4; k++) { float x = (k & 1) ? s : -s, z = (k & 2) ? s : -s; tube2(v3(x, y - s * 1.2f, z), v3(x, y + s * 1.2f, z), 0.02f, 0.02f, 4, 0.15f, 0.12f, 0.1f); }
    rbox(v3(0, y, 0), s * 0.92f, s * 1.15f, s * 0.92f, s * 0.3f, 1.0f, 0.86f, 0.45f);   /* the glowing glass */
    for (int k = 0; k < 4; k++) { float a0 = k * 1.5708f + 0.785f, a1 = a0 + 1.5708f, rr = s * 1.6f; ftri(v3(cosf(a0) * rr, y + s * 1.2f, sinf(a0) * rr), v3(0, y + s * 2.2f, 0), v3(cosf(a1) * rr, y + s * 1.2f, sinf(a1) * rr), 0.25f, 0.2f, 0.15f); }
    blob(v3(0, y + s * 2.3f, 0), s * 0.25f, s * 0.25f, s * 0.25f, 0, 3, 6, 0.85f, 0.7f, 0.3f); }
static void m_crystal(float h) { mat(MAT_PLAIN); for (int k = 0; k < 6; k++) { float a = k * 1.1f, d = k ? h * 0.2f : 0, ch = h * (k ? 0.4f + rnd() * 0.3f : 1.0f), cw = ch * 0.17f, tilt = k ? 0.32f : 0;
        Vec3 base = v3(cosf(a) * d, 0, sinf(a) * d), top = v3(base.x + cosf(a) * tilt * ch, ch, base.z + sinf(a) * tilt * ch);
        for (int q = 0; q < 6; q++) { float a0 = q * 1.0472f, a1 = (q + 1) * 1.0472f; Vec3 p0 = v3(base.x + cosf(a0) * cw, 0, base.z + sinf(a0) * cw), p1 = v3(base.x + cosf(a1) * cw, 0, base.z + sinf(a1) * cw);
            Vec3 m0 = v3(top.x - cosf(a) * tilt * ch * 0.2f + cosf(a0) * cw * 0.85f, ch * 0.78f, top.z - sinf(a) * tilt * ch * 0.2f + sinf(a0) * cw * 0.85f), m1 = v3(top.x - cosf(a) * tilt * ch * 0.2f + cosf(a1) * cw * 0.85f, ch * 0.78f, top.z - sinf(a) * tilt * ch * 0.2f + sinf(a1) * cw * 0.85f);
            float s2 = 0.78f + 0.22f * sinf(a0 * 2 + k); fquad(p1, p0, m0, m1, 0.7f * s2, 0.55f * s2, 1.0f * s2); ftri(m1, m0, top, 0.9f, 0.8f, 1.0f); } } }
static void legs4(float x, float z, float y, float r, float lr, float lg, float lb) { for (int k = 0; k < 4; k++) { float xx = (k & 1) ? x : -x, zz = (k & 2) ? z : -z;
        Vec3 p[3] = {v3(xx, y, zz), v3(xx, y * 0.45f, zz + 0.02f), v3(xx, 0.0f, zz)}; float rr[3] = {r * 1.3f, r * 0.9f, r * 0.8f}; tube(p, rr, 3, 6, lr, lg, lb, 1); } }
static void m_sheep(int lamb) { float s = lamb ? 0.72f : 1.0f; mat(MAT_PLAIN); legs4(0.22f * s, 0.32f * s, 0.5f * s, 0.05f * s, 0.18f, 0.15f, 0.13f);
    mat(MAT_WOOL); blob(v3(0, 0.72f * s, 0), 0.42f * s, 0.36f * s, 0.6f * s, 0.12f, 8, 12, 0.98f, 0.96f, 0.9f);
    blob(v3(0, 0.95f * s, 0.5f * s), 0.2f * s, 0.18f * s, 0.18f * s, 0.15f, 5, 8, 0.98f, 0.96f, 0.9f);   /* the woolly crown */
    mat(MAT_PLAIN); blob(v3(0, 0.86f * s, 0.7f * s), 0.13f * s, 0.15f * s, 0.2f * s, 0.02f, 6, 9, 0.2f, 0.17f, 0.15f);
    for (int sd = -1; sd <= 1; sd += 2) { blob(v3(sd * 0.16f * s, 0.93f * s, 0.62f * s), 0.09f * s, 0.035f * s, 0.05f * s, 0, 3, 6, 0.22f, 0.18f, 0.16f); blob(v3(sd * 0.075f * s, 0.92f * s, 0.84f * s), 0.022f * s, 0.022f * s, 0.022f * s, 0, 3, 5, 0.02f, 0.02f, 0.02f); } }
static void m_deer(void) { mat(MAT_PLAIN); legs4(0.19f, 0.42f, 0.95f, 0.045f, 0.55f, 0.38f, 0.24f);
    blob(v3(0, 1.08f, 0), 0.27f, 0.3f, 0.62f, 0.03f, 8, 12, 0.68f, 0.48f, 0.3f); Vec3 nk[3] = {v3(0, 1.2f, 0.45f), v3(0, 1.5f, 0.62f), v3(0, 1.72f, 0.7f)}; float nr[3] = {0.14f, 0.1f, 0.08f}; tube(nk, nr, 3, 8, 0.68f, 0.48f, 0.3f, 0);
    blob(v3(0, 1.75f, 0.82f), 0.11f, 0.12f, 0.22f, 0.02f, 6, 9, 0.66f, 0.46f, 0.29f); blob(v3(0, 1.0f, -0.6f), 0.08f, 0.1f, 0.06f, 0.05f, 4, 6, 0.95f, 0.92f, 0.88f);
    for (int sd = -1; sd <= 1; sd += 2) { blob(v3(sd * 0.13f, 1.86f, 0.75f), 0.09f, 0.04f, 0.04f, 0, 3, 6, 0.6f, 0.42f, 0.27f);
        Vec3 an[3] = {v3(sd * 0.06f, 1.86f, 0.78f), v3(sd * 0.18f, 2.1f, 0.72f), v3(sd * 0.25f, 2.35f, 0.62f)}; float ar[3] = {0.025f, 0.02f, 0.012f}; tube(an, ar, 3, 5, 0.85f, 0.78f, 0.62f, 1);
        tube2(v3(sd * 0.18f, 2.1f, 0.72f), v3(sd * 0.12f, 2.3f, 0.86f), 0.015f, 0.01f, 4, 0.85f, 0.78f, 0.62f); } }
static void m_lion(void) { mat(MAT_PLAIN); legs4(0.26f, 0.52f, 0.75f, 0.08f, 0.85f, 0.65f, 0.34f);
    blob(v3(0, 0.9f, 0), 0.36f, 0.36f, 0.82f, 0.03f, 8, 12, 0.88f, 0.68f, 0.36f);
    mat(MAT_WOOL); US = 0.4f; blob(v3(0, 1.15f, 0.78f), 0.46f, 0.48f, 0.36f, 0.22f, 8, 12, 0.6f, 0.36f, 0.14f);
    mat(MAT_PLAIN); blob(v3(0, 1.12f, 1.02f), 0.22f, 0.24f, 0.26f, 0.02f, 6, 9, 0.9f, 0.72f, 0.4f); blob(v3(0, 1.05f, 1.22f), 0.1f, 0.08f, 0.08f, 0, 4, 6, 0.3f, 0.2f, 0.15f);
    Vec3 tl[3] = {v3(0, 1.0f, -0.78f), v3(0, 0.7f, -1.0f), v3(0, 0.6f, -1.25f)}; float trr[3] = {0.05f, 0.04f, 0.03f}; tube(tl, trr, 3, 5, 0.85f, 0.65f, 0.32f, 0); blob(v3(0, 0.6f, -1.3f), 0.07f, 0.07f, 0.07f, 0.2f, 3, 6, 0.5f, 0.3f, 0.12f); }
static void hull(float L, float W, float D, float y0, float r, float g, float bl) {   /* a wooden hull: curved sides from bow to stern, planked */
    enum { NS = 10, NC = 6 }; Vec3 p[NS + 1][NC + 1]; mat(MAT_WOOD); US = 1.2f;
    for (int i = 0; i <= NS; i++) { float t = (float)i / NS * 2 - 1, wz = W * sqrtf(fmaxf(0.02f, 1 - powf(fabsf(t), 2.4f))), dz = D * (1 - 0.35f * t * t), sheer = 0.25f * D * t * t;
        for (int j = 0; j <= NC; j++) { float a = (float)j / NC * 3.14159265f; p[i][j] = v3(-cosf(a) * wz, y0 + D + sheer - sinf(a) * dz, t * L); } }
    for (int i = 0; i < NS; i++) for (int j = 0; j < NC; j++) fquad(p[i][j], p[i + 1][j], p[i + 1][j + 1], p[i][j + 1], r, g, bl);
    for (int i = 0; i < NS; i++) { Vec3 a = p[i][0], b = p[i + 1][0], c = p[i + 1][NC], d = p[i][NC]; tube2(vadd(a, v3(0, 0.03f, 0)), vadd(b, v3(0, 0.03f, 0)), 0.06f, 0.06f, 5, r * 0.8f, g * 0.8f, bl * 0.8f); tube2(vadd(d, v3(0, 0.03f, 0)), vadd(c, v3(0, 0.03f, 0)), 0.06f, 0.06f, 5, r * 0.8f, g * 0.8f, bl * 0.8f);
        fquad(v3(a.x * 0.95f, a.y - D * 0.25f, a.z), v3(d.x * 0.95f, d.y - D * 0.25f, d.z), v3(c.x * 0.95f, c.y - D * 0.25f, c.z), v3(b.x * 0.95f, b.y - D * 0.25f, b.z), r * 0.75f, g * 0.75f, bl * 0.75f); } }   /* the deck */
static void m_boat(float h) { float L = h * 0.75f, W = h * 0.32f; hull(L, W, h * 0.3f, 0, 0.62f, 0.45f, 0.3f);
    mat(MAT_WOOD); tube2(v3(0, h * 0.2f, 0), v3(0, h * 1.5f, 0), 0.09f, 0.06f, 7, 0.5f, 0.38f, 0.26f); tube2(v3(-W * 1.3f, h * 1.3f, 0), v3(W * 1.3f, h * 1.3f, 0), 0.05f, 0.05f, 6, 0.5f, 0.38f, 0.26f);
    mat(MAT_CLOTH); { Vec3 p[5]; float rr[5]; for (int k = 0; k < 5; k++) { float t = k / 4.0f; p[k] = v3(-W * 1.25f + W * 2.5f * t, h * 1.22f, 0.02f); rr[k] = 0.16f * sinf(3.14159f * (0.12f + 0.76f * t)) + 0.04f; } tube(p, rr, 5, 8, 0.93f, 0.9f, 0.8f, 1); }
    mat(MAT_PLAIN); for (int sd = -1; sd <= 1; sd += 2) tube2(v3(sd * W * 0.95f, h * 0.55f, -L * 0.1f), v3(sd * W * 1.9f, h * 0.05f, -L * 0.35f), 0.035f, 0.035f, 4, 0.5f, 0.38f, 0.26f); }
static void m_net(float h) { mat(MAT_WOOD); for (int sd = -1; sd <= 1; sd += 2) tube2(v3(sd * h * 0.65f, 0, 0), v3(sd * h * 0.65f, h, 0), 0.08f, 0.065f, 7, 0.45f, 0.35f, 0.24f);
    tube2(v3(-h * 0.65f, h * 0.95f, 0), v3(h * 0.65f, h * 0.95f, 0), 0.03f, 0.03f, 5, 0.78f, 0.72f, 0.55f);
    mat(MAT_PLAIN); for (int k = 0; k <= 6; k++) { Vec3 p[7]; float rr[7]; for (int q = 0; q <= 6; q++) { float t = q / 6.0f; p[q] = v3((t - 0.5f) * h * 1.3f, h * (0.25f + 0.11f * k) - sinf(t * 3.14159f) * h * 0.08f, 0.02f * sinf(t * 9)); rr[q] = 0.012f; } tube(p, rr, 7, 3, 0.82f, 0.76f, 0.58f, 0); }
    for (int k = -6; k <= 6; k++) tube2(v3(k * h * 0.1f, h * 0.93f, 0), v3(k * h * 0.1f, h * 0.22f - (36 - k * k) * h * 0.002f, 0), 0.012f, 0.012f, 3, 0.82f, 0.76f, 0.58f);
    for (int k = -5; k <= 5; k += 2) blob(v3(k * h * 0.11f, h * 0.95f, 0), 0.07f, 0.06f, 0.07f, 0, 3, 6, 0.85f, 0.6f, 0.3f); }
static void m_person(float h, float r, float g, float bl, int helmet, int priest) {   /* a robed figure: shaped robe, sleeves, hands, face, beard, headcloth or helmet */
    float s = h / 1.8f; mat(MAT_CLOTH);
    { float ys[7] = {0, 0.35f, 0.75f, 1.0f, 1.2f, 1.36f, 1.44f}, rs[7] = {0.4f, 0.36f, 0.3f, 0.26f, 0.27f, 0.22f, 0.09f}; Vec3 p[7]; float rr[7];
        for (int k = 0; k < 7; k++) { p[k] = v3(0, ys[k] * s, (k > 3 ? 0.02f : 0) * s); rr[k] = rs[k] * s; } tube(p, rr, 7, 12, r, g, bl, 1); }
    tube2(v3(0, 0.95f * s, 0), v3(0, 1.05f * s, 0), 0.28f * s, 0.275f * s, 12, priest ? 0.3f : r * 0.6f, priest ? 0.4f : g * 0.6f, priest ? 0.9f : bl * 0.6f);   /* the sash */
    for (int sd = -1; sd <= 1; sd += 2) { Vec3 a[3] = {v3(sd * 0.25f * s, 1.33f * s, 0), v3(sd * 0.32f * s, 1.12f * s, 0.06f * s), v3(sd * 0.28f * s, 0.95f * s, 0.2f * s)}; float ar[3] = {0.08f * s, 0.085f * s, 0.1f * s}; tube(a, ar, 3, 7, r * 0.92f, g * 0.92f, bl * 0.92f, 1); }
    mat(MAT_PLAIN); for (int sd = -1; sd <= 1; sd += 2) blob(v3(sd * 0.27f * s, 0.92f * s, 0.25f * s), 0.05f * s, 0.06f * s, 0.05f * s, 0, 3, 6, 0.85f, 0.64f, 0.48f);
    blob(v3(0, 1.58f * s, 0.01f * s), 0.12f * s, 0.15f * s, 0.13f * s, 0.01f, 6, 9, 0.86f, 0.66f, 0.5f); blob(v3(0, 1.58f * s, 0.13f * s), 0.025f * s, 0.035f * s, 0.03f * s, 0, 3, 5, 0.8f, 0.6f, 0.45f);
    if (!helmet) blob(v3(0, 1.46f * s, 0.07f * s), 0.11f * s, 0.12f * s, 0.08f * s, 0.15f, 5, 8, priest ? 0.85f : 0.3f, priest ? 0.83f : 0.22f, priest ? 0.8f : 0.15f);   /* the beard */
    if (priest) { cbox(v3(0, 1.2f * s, 0.24f * s), v3(0.13f * s, 0.14f * s, 0.02f * s), 0.85f, 0.7f, 0.3f);
        for (int k = 0; k < 12; k++) { static const float gc[4][3] = {{0.9f, 0.15f, 0.2f}, {0.2f, 0.7f, 0.3f}, {0.2f, 0.4f, 0.95f}, {0.95f, 0.85f, 0.3f}};
            blob(v3((k % 3 - 1) * 0.07f * s, (1.29f - (k / 3) * 0.06f) * s, 0.265f * s), 0.025f * s, 0.022f * s, 0.012f * s, 0, 3, 5, gc[k & 3][0], gc[k & 3][1], gc[k & 3][2]); }
        mat(MAT_CLOTH); blob(v3(0, 1.72f * s, -0.01f * s), 0.15f * s, 0.11f * s, 0.15f * s, 0.12f, 5, 9, 0.97f, 0.97f, 0.95f); }
    else if (helmet) { blob(v3(0, 1.66f * s, -0.01f * s), 0.15f * s, 0.13f * s, 0.16f * s, 0, 5, 9, 0.8f, 0.62f, 0.3f); tube2(v3(0, 1.58f * s, -0.01f * s), v3(0, 1.6f * s, -0.01f * s), 0.17f * s, 0.17f * s, 10, 0.75f, 0.58f, 0.28f);
        for (int k = 0; k < 7; k++) { float a = -1.2f + k * 0.4f; tube2(v3(0, 1.78f * s, 0), v3(0, (1.78f + cosf(a) * 0.22f) * s, sinf(a) * 0.22f * s), 0.03f * s, 0.015f * s, 4, 0.85f, 0.15f, 0.12f); }
        mat(MAT_PLAIN); cbox(v3(0, 1.2f * s, 0.2f * s), v3(0.22f * s, 0.16f * s, 0.06f * s), 0.7f, 0.55f, 0.3f); }
    else { mat(MAT_CLOTH); blob(v3(0, 1.66f * s, -0.02f * s), 0.16f * s, 0.12f * s, 0.16f * s, 0.06f, 5, 9, r * 1.15f, g * 1.15f, bl * 1.15f);
        Vec3 dp[3] = {v3(0, 1.62f * s, -0.08f * s), v3(0, 1.45f * s, -0.15f * s), v3(0, 1.25f * s, -0.18f * s)}; float dr[3] = {0.14f * s, 0.13f * s, 0.1f * s}; tube(dp, dr, 3, 8, r * 1.1f, g * 1.1f, bl * 1.1f, 1);
        tube2(v3(0, 1.69f * s, -0.01f * s), v3(0, 1.72f * s, -0.01f * s), 0.165f * s, 0.165f * s, 10, 0.25f, 0.2f, 0.15f); } }
static void m_banner(float h, float r, float g, float bl) { mat(MAT_WOOD); tube2(v3(0, 0, 0), v3(0, h, 0), 0.08f, 0.06f, 7, 0.45f, 0.34f, 0.22f);
    mat(MAT_PLAIN); blob(v3(0, h + 0.1f, 0), 0.11f, 0.11f, 0.11f, 0, 4, 7, 0.95f, 0.8f, 0.35f);
    SWM = 1; SW_C = v3(0, h * 0.8f, 0); SW_R = h * 0.5f; SW_A = 0.35f; mat(MAT_CLOTH);
    enum { NX = 7, NY = 4 }; Vec3 p[NX + 1][NY + 1]; float W = h * 0.5f, Hh = h * 0.38f;
    for (int i = 0; i <= NX; i++) for (int j = 0; j <= NY; j++) { float t = (float)i / NX; p[i][j] = v3(0.08f + t * W, h * 0.95f - (float)j / NY * Hh * (1 - 0.15f * t), sinf(t * 5.0f + j * 0.3f) * 0.12f * t); }
    for (int i = 0; i < NX; i++) for (int j = 0; j < NY; j++) { float sh = (j == 1 || j == 2) ? 1.0f : 0.82f; fquad(p[i][j], p[i + 1][j], p[i + 1][j + 1], p[i][j + 1], r * sh, g * sh, bl * sh); fquad(p[i][j + 1], p[i + 1][j + 1], p[i + 1][j], p[i][j], r * sh * 0.9f, g * sh * 0.9f, bl * sh * 0.9f); }
    nosway(); }
static void m_cloud(float h) { mat(MAT_PLAIN); for (int k = 0; k < 9; k++) { float a = k * 0.8f, d = k ? h * (0.3f + rnd() * 0.25f) : 0, sz = h * (k ? 0.22f + rnd() * 0.12f : 0.36f);
        blob(v3(cosf(a) * d * 1.6f, h * 0.5f + (rnd() - 0.3f) * h * 0.15f, sinf(a) * d * 0.7f), sz * 1.2f, sz * 0.8f, sz, 0.06f, 6, 10, 1, 1, 1); } }
static void wheel(float x, float y, float z, float R, float r, float g, float bl) {   /* a cart wheel standing in the y-z plane: rim, spokes and hub */
    Vec3 p[17]; float rr[17]; for (int k = 0; k <= 16; k++) { float a = k * 6.2831853f / 16; p[k] = v3(x, y + sinf(a) * R, z + cosf(a) * R); rr[k] = R * 0.09f; } tube(p, rr, 17, 6, r, g, bl, 0);
    for (int k = 0; k < 8; k++) { float a = k * 0.785f; tube2(v3(x, y, z), v3(x, y + sinf(a) * R, z + cosf(a) * R), R * 0.04f, R * 0.035f, 4, r, g, bl); }
    tube2(v3(x - R * 0.12f, y, z), v3(x + R * 0.12f, y, z), R * 0.14f, R * 0.14f, 8, r * 0.8f, g * 0.8f, bl * 0.8f); }
static void m_cart(void) { mat(MAT_WOOD); rbox(v3(0, 0.85f, 0), 0.8f, 0.08f, 1.1f, 0.03f, 0.62f, 0.45f, 0.28f);
    for (int sd = -1; sd <= 1; sd += 2) { rbox(v3(sd * 0.76f, 1.1f, 0), 0.05f, 0.2f, 1.1f, 0.03f, 0.58f, 0.42f, 0.26f); rbox(v3(0, 1.1f, sd * 1.06f), 0.8f, 0.2f, 0.05f, 0.03f, 0.58f, 0.42f, 0.26f);
        wheel(sd * 0.92f, 0.55f, 0, 0.55f, 0.5f, 0.36f, 0.22f); tube2(v3(sd * 0.35f, 0.85f, 1.1f), v3(sd * 0.25f, 0.7f, 2.3f), 0.05f, 0.04f, 6, 0.55f, 0.4f, 0.24f); }
    mat(MAT_CLOTH); for (int k = 0; k < 4; k++) blob(v3((k & 1 ? 0.35f : -0.35f), 1.2f, (k & 2 ? 0.45f : -0.45f)), 0.32f, 0.28f, 0.4f, 0.1f, 5, 8, 0.82f, 0.72f, 0.5f); }
static void m_ark(float h) { float L = h * 1.8f, W = h * 0.6f; hull(L, W, h * 0.45f, 0, 0.55f, 0.4f, 0.24f);
    mat(MAT_WOOD); rbox(v3(0, h * 0.68f, 0), W * 0.62f, h * 0.24f, L * 0.62f, 0.08f, 0.6f, 0.44f, 0.27f);
    for (int sd = -1; sd <= 1; sd += 2) { fquad(v3(sd * W * 0.75f, h * 0.9f, -L * 0.68f), v3(sd * W * 0.75f, h * 0.9f, L * 0.68f), v3(0, h * 1.18f, L * 0.68f), v3(0, h * 1.18f, -L * 0.68f), 0.5f, 0.36f, 0.22f);
        fquad(v3(0, h * 1.17f, -L * 0.68f), v3(0, h * 1.17f, L * 0.68f), v3(sd * W * 0.75f, h * 0.89f, L * 0.68f), v3(sd * W * 0.75f, h * 0.89f, -L * 0.68f), 0.4f, 0.3f, 0.2f);
        mat(MAT_PLAIN); for (int k = -2; k <= 2; k++) fquad(v3(sd * W * 0.63f, h * 0.8f, k * L * 0.22f - 0.3f), v3(sd * W * 0.63f, h * 0.8f, k * L * 0.22f + 0.3f), v3(sd * W * 0.63f, h * 0.62f, k * L * 0.22f + 0.3f), v3(sd * W * 0.63f, h * 0.62f, k * L * 0.22f - 0.3f), 0.08f, 0.06f, 0.05f);
        mat(MAT_WOOD); } }
static void m_tomb(float h) { mat(MAT_ROCK); blob(v3(0, h * 0.35f, -h * 0.1f), h * 0.95f, h * 0.65f, h * 0.75f, 0.12f, 9, 14, 0.78f, 0.74f, 0.66f);
    mat(MAT_STONE); for (int sd = -1; sd <= 1; sd += 2) rbox(v3(sd * h * 0.25f, h * 0.3f, h * 0.62f), h * 0.07f, h * 0.3f, h * 0.08f, 0.05f, 0.8f, 0.76f, 0.68f);
    rbox(v3(0, h * 0.63f, h * 0.62f), h * 0.33f, h * 0.05f, h * 0.09f, 0.05f, 0.8f, 0.76f, 0.68f);
    mat(MAT_PLAIN); fquad(v3(-h * 0.18f, 0, h * 0.68f), v3(h * 0.18f, 0, h * 0.68f), v3(h * 0.18f, h * 0.58f, h * 0.68f), v3(-h * 0.18f, h * 0.58f, h * 0.68f), 0.04f, 0.035f, 0.03f);
    mat(MAT_ROCK); { float R = h * 0.34f; Vec3 c = v3(h * 0.65f, R, h * 0.72f); Vec3 p[2] = {v3(c.x - 0.25f, c.y, c.z), v3(c.x + 0.25f, c.y, c.z)}; float rr[2] = {R, R}; tube(p, rr, 2, 18, 0.66f, 0.63f, 0.57f, 1);
        for (int k = 0; k < 18; k++) { float a0 = k * 0.349f, a1 = a0 + 0.349f; ftri(p[0], v3(p[0].x, c.y + sinf(a1) * R, c.z + cosf(a1) * R), v3(p[0].x, c.y + sinf(a0) * R, c.z + cosf(a0) * R), 0.66f, 0.63f, 0.57f); } } }
static void m_crosses(float h) { mat(MAT_ROCK); blob(v3(0, -h * 0.05f, 0), h * 1.4f, h * 0.22f, h * 0.7f, 0.15f, 6, 12, 0.6f, 0.55f, 0.46f);
    mat(MAT_WOOD); US = 1.0f; for (int k = -1; k <= 1; k++) { float s = k == 0 ? 1.0f : 0.82f, x = k * h * 0.85f;
        rbox(v3(x, h * 0.5f * s, 0), 0.15f, h * 0.5f * s, 0.14f, 0.04f, 0.45f, 0.33f, 0.21f); rbox(v3(x, h * 0.78f * s, 0.02f), h * 0.23f * s, 0.12f, 0.12f, 0.04f, 0.43f, 0.32f, 0.2f); } }
static void m_stable(float h) { mat(MAT_BARK); for (int k = 0; k < 4; k++) tube2(v3(k & 1 ? h * 0.6f : -h * 0.6f, 0, k & 2 ? h * 0.45f : -h * 0.45f), v3(k & 1 ? h * 0.6f : -h * 0.6f, k & 2 ? h * 0.85f : h * 0.7f, k & 2 ? h * 0.45f : -h * 0.45f), 0.1f, 0.09f, 7, 0.5f, 0.38f, 0.26f);
    tube2(v3(-h * 0.7f, h * 0.84f, h * 0.45f), v3(h * 0.7f, h * 0.84f, h * 0.45f), 0.08f, 0.08f, 6, 0.5f, 0.38f, 0.26f); tube2(v3(-h * 0.7f, h * 0.69f, -h * 0.45f), v3(h * 0.7f, h * 0.69f, -h * 0.45f), 0.08f, 0.08f, 6, 0.5f, 0.38f, 0.26f);
    mat(MAT_THATCH); { Vec3 a = v3(-h * 0.8f, h * 0.7f, -h * 0.62f), b = v3(h * 0.8f, h * 0.7f, -h * 0.62f), c = v3(h * 0.8f, h * 0.92f, h * 0.62f), d = v3(-h * 0.8f, h * 0.92f, h * 0.62f), up = v3(0, 0.15f, 0);
        fquad(vadd(a, up), vadd(b, up), vadd(c, up), vadd(d, up), 0.85f, 0.75f, 0.45f); fquad(b, a, d, c, 0.6f, 0.5f, 0.3f); fquad(a, b, vadd(b, up), vadd(a, up), 0.7f, 0.6f, 0.35f); fquad(c, d, vadd(d, up), vadd(c, up), 0.7f, 0.6f, 0.35f); }
    mat(MAT_WOOD); for (int k = 0; k < 5; k++) rbox(v3(0, h * 0.1f + k * h * 0.13f, -h * 0.5f), h * 0.6f, h * 0.06f, 0.05f, 0.02f, 0.55f, 0.42f, 0.28f);   /* the back wall */
    rbox(v3(0, h * 0.14f, 0), h * 0.26f, h * 0.14f, h * 0.15f, 0.04f, 0.58f, 0.43f, 0.26f); mat(MAT_THATCH); blob(v3(0, h * 0.28f, 0), h * 0.24f, h * 0.06f, h * 0.13f, 0.2f, 4, 8, 0.95f, 0.85f, 0.5f); }
static void m_camp(float h) { mat(MAT_ROCK); for (int k = 0; k < 8; k++) { float a = k * 0.785f; blob(v3(cosf(a) * h * 0.28f, 0.08f, sinf(a) * h * 0.28f), 0.16f, 0.12f, 0.14f, 0.15f, 4, 6, 0.5f, 0.48f, 0.44f); }
    mat(MAT_BARK); for (int k = 0; k < 3; k++) { float a = k * 1.05f; tube2(v3(cosf(a) * h * 0.25f, 0.05f, sinf(a) * h * 0.25f), v3(-cosf(a) * h * 0.25f, 0.12f, -sinf(a) * h * 0.25f), 0.07f, 0.07f, 6, 0.4f, 0.3f, 0.2f); }
    SWM = 1; SW_C = v3(0, 0, 0); SW_R = h * 0.4f; SW_A = 0.08f; mat(MAT_PLAIN);
    for (int k = 0; k < 7; k++) { float a = k * 0.9f, d = k ? 0.12f : 0, fh = h * (k ? 0.22f : 0.38f); Vec3 b = v3(cosf(a) * d, 0.1f, sinf(a) * d);
        for (int q = 0; q < 3; q++) { float aa = q * 2.094f + a; Vec3 e0 = v3(b.x + cosf(aa) * 0.1f, b.y, b.z + sinf(aa) * 0.1f), e1 = v3(b.x + cosf(aa + 2.094f) * 0.1f, b.y, b.z + sinf(aa + 2.094f) * 0.1f);
            ftri(e0, e1, v3(b.x, b.y + fh, b.z), 1.0f, 0.5f + 0.3f * (k == 0), 0.12f); ftri(e1, e0, v3(b.x, b.y + fh, b.z), 1.0f, 0.75f, 0.2f); } }
    nosway(); }
static void m_rocks(float h, float w, float r, float g, float bl) { mat(MAT_ROCK); blob(v3(0, h * 0.42f, 0), w * 0.5f, h * 0.55f, w * 0.42f, 0.2f, 7, 11, r, g, bl);
    blob(v3(w * 0.45f, h * 0.2f, w * 0.2f), w * 0.25f, h * 0.28f, w * 0.22f, 0.22f, 5, 8, r * 0.92f, g * 0.92f, bl * 0.92f); }
static void m_rubble(float h) { mat(MAT_STONE); for (int k = 0; k < 6; k++) { float a = k * 1.1f, d = k ? h * 0.45f : 0, s = h * (0.25f + rnd() * 0.15f);
        rbox(v3(cosf(a) * d, s * 0.6f, sinf(a) * d), s * 1.1f, s * 0.6f, s * 0.7f, s * 0.2f, 0.78f, 0.7f, 0.56f); }
    mat(MAT_ROCK); for (int k = 0; k < 5; k++) { float a = k * 1.4f + 0.5f, d = h * 0.6f; blob(v3(cosf(a) * d, h * 0.1f, sinf(a) * d), h * 0.2f, h * 0.15f, h * 0.18f, 0.25f, 4, 7, 0.72f, 0.66f, 0.54f); } }

/* buildings */
static void door(float x, float y0, float z, float w, float hh, int face) {   /* a deep doorway: stone frame, a dark inside, a wooden door standing half open */
    float f = (float)face; mat(MAT_PLAIN); fquad(v3(x - w, y0, z + 0.02f * f), v3(x + w, y0, z + 0.02f * f), v3(x + w, y0 + hh, z + 0.02f * f), v3(x - w, y0 + hh, z + 0.02f * f), 0.06f, 0.05f, 0.04f);
    if (face < 0) { Vec3 t = v3(x - w, y0, z - 0.02f); (void)t; }
    mat(MAT_WOOD); US = 1.0f; for (int sd = -1; sd <= 1; sd += 2) rbox(v3(x + sd * (w + 0.08f), y0 + hh / 2, z + 0.04f * f), 0.09f, hh / 2, 0.07f, 0.03f, 0.5f, 0.37f, 0.24f);
    rbox(v3(x, y0 + hh + 0.09f, z + 0.05f * f), w + 0.22f, 0.09f, 0.09f, 0.03f, 0.48f, 0.35f, 0.22f);
    Vec3 hinge = v3(x - w, y0, z + 0.04f * f), dd = v3(cosf(0.9f) * w * 1.9f, 0, sinf(0.9f) * w * 1.9f * f);
    fquad(hinge, vadd(hinge, dd), vadd(vadd(hinge, dd), v3(0, hh, 0)), vadd(hinge, v3(0, hh, 0)), 0.55f, 0.4f, 0.25f); fquad(vadd(hinge, v3(0, hh, 0)), vadd(vadd(hinge, dd), v3(0, hh, 0)), vadd(hinge, dd), hinge, 0.5f, 0.36f, 0.22f); }
static void window(float x, float y, float z, float w, float hh, int face) { float f = (float)face;
    mat(MAT_PLAIN); fquad(v3(x - w, y - hh, z + 0.02f * f), v3(x + w, y - hh, z + 0.02f * f), v3(x + w, y + hh, z + 0.02f * f), v3(x - w, y + hh, z + 0.02f * f), 0.07f, 0.06f, 0.06f);
    mat(MAT_WOOD); US = 0.8f; rbox(v3(x, y - hh - 0.05f, z + 0.06f * f), w + 0.12f, 0.05f, 0.1f, 0.02f, 0.5f, 0.37f, 0.24f); rbox(v3(x, y + hh + 0.05f, z + 0.04f * f), w + 0.1f, 0.05f, 0.07f, 0.02f, 0.5f, 0.37f, 0.24f);
    for (int sd = -1; sd <= 1; sd += 2) { Vec3 hg = v3(x + sd * w, y - hh, z + 0.05f * f), d = v3(sd * cosf(0.5f) * w, 0, sinf(0.5f) * w * f); fquad(hg, vadd(hg, d), vadd(vadd(hg, d), v3(0, hh * 2, 0)), vadd(hg, v3(0, hh * 2, 0)), 0.36f, 0.48f, 0.55f); fquad(vadd(hg, v3(0, hh * 2, 0)), vadd(vadd(hg, d), v3(0, hh * 2, 0)), vadd(hg, d), hg, 0.3f, 0.4f, 0.46f); } }
static void jar(float x, float z, float s) { mat(MAT_PLAIN); Vec3 p[6] = {v3(x, 0, z), v3(x, s * 0.25f, z), v3(x, s * 0.6f, z), v3(x, s * 0.85f, z), v3(x, s * 0.95f, z), v3(x, s, z)}; float rr[6] = {s * 0.18f, s * 0.32f, s * 0.36f, s * 0.2f, s * 0.13f, s * 0.16f};
    tube(p, rr, 6, 10, 0.78f, 0.45f, 0.28f, 1); }
static void m_house(float h, float w, unsigned hsh2, float r, float g, float bl, int kind) {   /* kind 0 house, 1 inn, 2 market stall */
    float d = w * 0.8f, wh = h * 0.78f;
    mat(MAT_STONE); rbox(v3(0, 0.2f, 0), w / 2 + 0.12f, 0.25f, d / 2 + 0.12f, 0.06f, r * 0.8f, g * 0.8f, bl * 0.8f);
    mat(MAT_PLASTER); rbox(v3(0, wh / 2 + 0.1f, 0), w / 2, wh / 2, d / 2, 0.3f, r, g, bl);
    for (int sd = -1; sd <= 1; sd += 2) { rbox(v3(0, wh + 0.32f, sd * (d / 2 - 0.12f)), w / 2, 0.24f, 0.14f, 0.1f, r * 0.97f, g * 0.97f, bl * 0.97f); rbox(v3(sd * (w / 2 - 0.12f), wh + 0.32f, 0), 0.14f, 0.24f, d / 2, 0.1f, r * 0.97f, g * 0.97f, bl * 0.97f); }
    mat(MAT_WOOD); for (int k = 0; k < 6; k++) { float x = -w / 2 + w * (k + 0.5f) / 6; for (int sd = -1; sd <= 1; sd += 2) tube2(v3(x, wh - 0.1f, sd * (d / 2 - 0.3f)), v3(x, wh - 0.1f, sd * (d / 2 + 0.45f)), 0.09f, 0.085f, 6, 0.45f, 0.33f, 0.22f); }   /* the roof beams poking through */
    for (int sd = -1; sd <= 1; sd += 2) { door(sd < 0 ? w * 0.18f : -w * 0.1f, 0.4f, sd * d / 2, fminf(0.55f, w * 0.12f), fminf(wh * 0.55f, 2.2f), sd);
        window(-w * 0.3f * sd, wh * 0.65f, sd * d / 2, fminf(0.4f, w * 0.08f), 0.35f, sd); if (w > 6) window(w * 0.33f * sd, wh * 0.65f, sd * d / 2, fminf(0.4f, w * 0.08f), 0.35f, sd); }
    if (hsh2 & 2) { mat(MAT_STONE); for (int k = 0; k < 7; k++) { float t = (k + 1) / 7.0f; rbox(v3(w / 2 + 0.45f, wh * t * 0.5f + 0.05f, -d / 2 + 0.4f + t * d * 0.65f), 0.4f, wh * t * 0.5f, 0.2f, 0.05f, r * 0.85f, g * 0.85f, bl * 0.85f); } }   /* steps up to the roof */
    if (hsh2 & 1) { mat(MAT_PLASTER); rbox(v3(w * 0.22f, wh + 1.05f, -d * 0.1f), w * 0.2f, 0.95f, d * 0.24f, 0.2f, r * 1.02f, g * 1.02f, bl * 1.02f); door(w * 0.22f, wh + 0.1f, -d * 0.1f + d * 0.24f, 0.4f, 1.5f, 1); }
    if (kind) { mat(MAT_CLOTH); float aw = w * 0.42f, y = kind == 2 ? wh * 0.55f : wh * 0.6f; int stripes = 6;   /* a striped awning on poles */
        for (int k = 0; k < stripes; k++) { float x0 = -aw + 2 * aw * k / stripes, x1 = x0 + 2 * aw / stripes; float c1 = (k & 1) ? 1.0f : 0.8f, c2 = (k & 1) ? 0.95f : 0.25f, c3 = (k & 1) ? 0.9f : 0.2f;
            fquad(v3(x0, y + 0.5f, d / 2 + 0.05f), v3(x1, y + 0.5f, d / 2 + 0.05f), v3(x1, y, d / 2 + 1.6f), v3(x0, y, d / 2 + 1.6f), c1, c2, c3); fquad(v3(x0, y, d / 2 + 1.6f), v3(x1, y, d / 2 + 1.6f), v3(x1, y + 0.5f, d / 2 + 0.05f), v3(x0, y + 0.5f, d / 2 + 0.05f), c1 * 0.8f, c2 * 0.8f, c3 * 0.8f); }
        mat(MAT_WOOD); for (int sd = -1; sd <= 1; sd += 2) tube2(v3(sd * aw, 0, d / 2 + 1.55f), v3(sd * aw, y + 0.05f, d / 2 + 1.55f), 0.06f, 0.05f, 6, 0.48f, 0.35f, 0.22f);
        if (kind == 2) { mat(MAT_WOOD); rbox(v3(0, 0.45f, d / 2 + 0.9f), aw * 0.8f, 0.45f, 0.4f, 0.04f, 0.55f, 0.4f, 0.25f); mat(MAT_THATCH);
            for (int k = 0; k < 4; k++) blob(v3(-aw * 0.6f + k * aw * 0.4f, 0.98f, d / 2 + 0.9f), 0.22f, 0.12f, 0.22f, 0.1f, 4, 8, k & 1 ? 0.95f : 0.85f, k & 1 ? 0.55f : 0.75f, k & 1 ? 0.2f : 0.3f); } }
    jar(-w * 0.38f, d / 2 + 0.4f, 0.8f); if (hsh2 & 4) jar(-w * 0.28f, d / 2 + 0.55f, 0.6f); }
static void m_hut(float h, float w, float r, float g, float bl) {   /* a round mud hut with a thick thatched roof */
    float wh = h * 0.55f, R = w * 0.5f; mat(MAT_PLASTER); { Vec3 p[4] = {v3(0, -0.1f, 0), v3(0, wh * 0.4f, 0), v3(0, wh * 0.85f, 0), v3(0, wh, 0)}; float rr[4] = {R * 1.04f, R * 1.02f, R * 0.98f, R * 0.96f}; tube(p, rr, 4, 16, 0.72f * r, 0.6f * g, 0.45f * bl, 0); }
    door(0, 0, R * 0.99f, R * 0.25f, wh * 0.7f, 1);
    mat(MAT_THATCH); { Vec3 p[5] = {v3(0, wh - 0.35f, 0), v3(0, wh, 0), v3(0, wh + (h - wh) * 0.5f, 0), v3(0, h * 0.97f, 0), v3(0, h, 0)}; float rr[5] = {R * 1.32f, R * 1.25f, R * 0.7f, R * 0.18f, 0.02f}; tube(p, rr, 5, 16, 0.88f, 0.76f, 0.48f, 0); }
    mat(MAT_PLAIN); for (int k = 0; k < 16; k++) { float a0 = k * 0.3927f, a1 = a0 + 0.3927f; ftri(v3(0, wh - 0.3f, 0), v3(cosf(a1) * R * 1.32f, wh - 0.35f, sinf(a1) * R * 1.32f), v3(cosf(a0) * R * 1.32f, wh - 0.35f, sinf(a0) * R * 1.32f), 0.35f, 0.3f, 0.2f); } }
static void m_tower(float h, float w, float r, float g, float bl) { float tw = fminf(w, h * 0.45f) * 0.5f; mat(MAT_STONE);
    rbox(v3(0, 0.6f, 0), tw + 0.3f, 0.6f, tw + 0.3f, 0.15f, r * 0.9f, g * 0.9f, bl * 0.9f); rbox(v3(0, h / 2, 0), tw, h / 2, tw, 0.18f, r, g, bl);
    rbox(v3(0, h + 0.15f, 0), tw + 0.25f, 0.18f, tw + 0.25f, 0.06f, r * 1.02f, g * 1.02f, bl * 1.02f);
    for (int q = 0; q < 4; q++) for (int k = -1; k <= 1; k++) { float o = tw + 0.05f, a = k * tw * 0.66f; Vec3 c = q == 0 ? v3(a, h + 0.75f, o) : q == 1 ? v3(a, h + 0.75f, -o) : q == 2 ? v3(o, h + 0.75f, a) : v3(-o, h + 0.75f, a);
        rbox(c, q < 2 ? 0.3f : 0.18f, 0.42f, q < 2 ? 0.18f : 0.3f, 0.05f, r * 0.97f, g * 0.97f, bl * 0.97f); }
    mat(MAT_PLAIN); for (int q = 0; q < 2; q++) { float y = h * (0.45f + 0.25f * q); for (int sd = -1; sd <= 1; sd += 2) fquad(v3(-0.1f, y - 0.5f, sd * (tw + 0.02f)), v3(0.1f, y - 0.5f, sd * (tw + 0.02f)), v3(0.1f, y + 0.5f, sd * (tw + 0.02f)), v3(-0.1f, y + 0.5f, sd * (tw + 0.02f)), 0.05f, 0.04f, 0.04f); }
    door(0, 1.2f, tw, 0.55f, 2.2f, 1); }
static void m_pillar(float h, float r, float g, float bl, int gold) { float rad = fmaxf(0.35f, h * 0.08f); mat(gold ? MAT_PLAIN : MAT_STONE);
    rbox(v3(0, 0.22f, 0), rad * 1.55f, 0.22f, rad * 1.55f, 0.06f, r * 0.92f, g * 0.92f, bl * 0.92f);
    { Vec3 p[3] = {v3(0, 0.44f, 0), v3(0, 0.55f, 0), v3(0, 0.66f, 0)}; float rr[3] = {rad * 1.3f, rad * 1.35f, rad * 1.1f}; tube(p, rr, 3, 18, r, g, bl, 0); }
    mat(gold ? MAT_PLAIN : MAT_PLASTER); FLUTES = 16; { Vec3 p[3] = {v3(0, 0.6f, 0), v3(0, h * 0.5f, 0), v3(0, h - 0.6f, 0)}; float rr[3] = {rad, rad * 0.97f, rad * 0.88f}; tube(p, rr, 3, 24, r, g, bl, 0); } FLUTES = 0;
    mat(gold ? MAT_PLAIN : MAT_STONE); { Vec3 p[3] = {v3(0, h - 0.62f, 0), v3(0, h - 0.45f, 0), v3(0, h - 0.32f, 0)}; float rr[3] = {rad * 0.9f, rad * 1.2f, rad * 1.4f}; tube(p, rr, 3, 18, r, g, bl, 0); }
    rbox(v3(0, h - 0.16f, 0), rad * 1.5f, 0.16f, rad * 1.5f, 0.05f, r * 1.02f, g * 1.02f, bl * 1.02f); }
static void m_tent(float h, float w) {   /* a black goat-hair tent: cloth sagging between poles, guy ropes to pegs */
    float L = w * 0.5f + 0.6f, W = w * 0.5f; enum { NZ = 6, NX = 6 }; Vec3 p[NZ + 1][NX + 1]; mat(MAT_CLOTH);
    for (int i = 0; i <= NZ; i++) for (int j = 0; j <= NX; j++) { float tz = (float)i / NZ * 2 - 1, tx = (float)j / NX * 2 - 1, ridge = h * (1 - 0.1f * tz * tz) - fabsf(sinf(tz * 3.14159f * 1.5f)) * 0.15f;
        float y = ridge * (1 - fabsf(tx)) * (1 - 0.25f * fabsf(tx) * (1 - fabsf(tx))) + 0.3f * fabsf(tx); p[i][j] = v3(tx * W * (1 + 0.1f * (1 - tz * tz)), y, tz * L); }
    for (int i = 0; i < NZ; i++) for (int j = 0; j < NX; j++) { int band = (i + (j >= NX / 2)) % 3 == 0; float c = band ? 0.55f : 0.3f, c2 = band ? 0.45f : 0.24f;
        fquad(p[i][j], p[i + 1][j], p[i + 1][j + 1], p[i][j + 1], c, c2 * 0.95f, c2 * 0.85f); fquad(p[i][j + 1], p[i + 1][j + 1], p[i + 1][j], p[i][j], c * 0.6f, c2 * 0.55f, c2 * 0.5f); }
    for (int j = 0; j < NX; j++) fquad(p[0][j + 1], p[0][j], v3(p[0][j].x, 0, p[0][j].z), v3(p[0][j + 1].x, 0, p[0][j + 1].z), 0.3f, 0.24f, 0.2f);   /* the closed back */
    mat(MAT_WOOD); for (int k = -1; k <= 1; k++) tube2(v3(0, 0, k * L * 0.85f), v3(0, h * (1 - 0.1f * k * k * 0.7f), k * L * 0.85f), 0.07f, 0.06f, 6, 0.45f, 0.34f, 0.22f);
    mat(MAT_PLAIN); for (int i = 0; i <= NZ; i += 2) for (int sd = 0; sd <= NX; sd += NX) { Vec3 a = p[i][sd], b = v3(a.x * 1.6f, 0, a.z * 1.05f); tube2(a, b, 0.015f, 0.015f, 3, 0.75f, 0.68f, 0.5f); cbox(b, v3(0.04f, 0.12f, 0.04f), 0.5f, 0.38f, 0.25f); } }
static void m_well(float h, float w, float r, float g, float bl) { float R = w * 0.45f; mat(MAT_STONE);
    { Vec3 p[2] = {v3(0, 0, 0), v3(0, h * 0.5f, 0)}; float rr[2] = {R, R * 0.98f}; tube(p, rr, 2, 18, r, g, bl, 0); float ri[2] = {R * 0.78f, R * 0.78f}; tube(p, ri, 2, 18, r * 0.6f, g * 0.6f, bl * 0.6f, 0); }
    for (int k = 0; k < 18; k++) { float a0 = k * 0.349f, a1 = a0 + 0.349f; fquad(v3(cosf(a0) * R, h * 0.5f, sinf(a0) * R), v3(cosf(a0) * R * 0.78f, h * 0.5f, sinf(a0) * R * 0.78f), v3(cosf(a1) * R * 0.78f, h * 0.5f, sinf(a1) * R * 0.78f), v3(cosf(a1) * R, h * 0.5f, sinf(a1) * R), r * 1.05f, g * 1.05f, bl * 1.05f); }
    mat(MAT_PLAIN); for (int k = 0; k < 18; k++) { float a0 = k * 0.349f, a1 = a0 + 0.349f; ftri(v3(0, h * 0.25f, 0), v3(cosf(a0) * R * 0.78f, h * 0.25f, sinf(a0) * R * 0.78f), v3(cosf(a1) * R * 0.78f, h * 0.25f, sinf(a1) * R * 0.78f), 0.12f, 0.22f, 0.35f); }
    mat(MAT_WOOD); for (int sd = -1; sd <= 1; sd += 2) tube2(v3(sd * R * 0.9f, h * 0.5f, 0), v3(sd * R * 0.9f, h * 1.25f, 0), 0.08f, 0.07f, 6, 0.45f, 0.34f, 0.22f);
    tube2(v3(-R, h * 1.2f, 0), v3(R, h * 1.2f, 0), 0.07f, 0.07f, 6, 0.45f, 0.34f, 0.22f); mat(MAT_PLAIN); tube2(v3(0, h * 1.2f, 0), v3(0, h * 0.8f, 0), 0.015f, 0.015f, 3, 0.7f, 0.62f, 0.45f);
    jar(0, 0, 0.0f); { Vec3 p[3] = {v3(0, h * 0.62f, 0), v3(0, h * 0.72f, 0), v3(0, h * 0.8f, 0)}; float rr[3] = {0.14f, 0.17f, 0.18f}; tube(p, rr, 3, 8, 0.5f, 0.38f, 0.25f, 0); } }
static void m_wallseg(float h, float w, float r, float g, float bl) { mat(MAT_STONE); rbox(v3(0, h / 2, 0), w / 2, h / 2, 0.6f, 0.12f, r, g, bl);
    for (int k = -2; k <= 2; k++) rbox(v3(k * w / 5, h + 0.32f, 0), w / 14, 0.32f, 0.62f, 0.06f, r * 0.97f, g * 0.97f, bl * 0.97f); }


/* ------------------------------------------------------------------ buildings, second pass (Sean: the houses looked like boxes) ------------------------------------------------------------------
   Village houses of Judea: thick limestone or whitewashed walls on a stone footing, flat roofs of packed clay behind a low
   parapet, roof beams poking through, arched doorways in a stone surround with painted doors, small arched windows with
   wooden grilles and open shutters, an outside stair to the roof, sometimes an upper room, a vine arbor, cloth awnings,
   clay jars and potted plants. Each feature is built flat against a wall (x along it, y up, z out of it) and then turned
   into place, and the whole house is darkened toward the ground where light reaches less. */
static uint32_t FB[MAT_N];
static void feat_begin(void) { for (int m = 0; m < MAT_N; m++) FB[m] = O->b[m]->n; }
static void feat_end(Vec3 at, float yaw) { float c = cosf(yaw), s = sinf(yaw);
    for (int m = 0; m < MAT_N; m++) for (uint32_t i = FB[m]; i < O->b[m]->n; i++) { Vertex *v = &O->b[m]->v[i]; float x = v->px, z = v->pz, nx = v->nx, nz = v->nz;
        v->px = at.x + x * c + z * s; v->pz = at.z - x * s + z * c; v->py += at.y; v->nx = nx * c + nz * s; v->nz = -nx * s + nz * c; } }
static void arch_fill(float hw, float hh, float z, float r, float g, float b) {   /* an arch-topped shape facing +z, bottom at y = 0 */
    float sy = fmaxf(hh - hw, 0.05f);
    fquad(v3(-hw, 0, z), v3(hw, 0, z), v3(hw, sy, z), v3(-hw, sy, z), r, g, b);
    for (int k = 0; k < 8; k++) { float a0 = 3.14159265f * k / 8, a1 = 3.14159265f * (k + 1) / 8;
        ftri(v3(0, sy, z), v3(cosf(a0) * hw, sy + sinf(a0) * hw, z), v3(cosf(a1) * hw, sy + sinf(a1) * hw, z), r, g, b); } }
static void arch_frame(float hw, float hh, float th, float r, float g, float b) {   /* a stone surround: jambs, a rounded arch moulding and a keystone */
    float sy = fmaxf(hh - hw, 0.05f); Vec3 p[9]; float rr[9];
    for (int k = 0; k <= 8; k++) { float a = 3.14159265f * k / 8; p[k] = v3(cosf(a) * (hw + th * 0.8f), sy + sinf(a) * (hw + th * 0.8f), th * 0.4f); rr[k] = th; }
    tube(p, rr, 9, 4, r, g, b, 0);
    for (int sd = -1; sd <= 1; sd += 2) cbox(v3(sd * (hw + th * 0.8f), sy * 0.5f, th * 0.3f), v3(th, sy * 0.5f, th * 0.75f), r, g, b);
    cbox(v3(0, hh + th * 0.9f, th * 0.6f), v3(th * 1.1f, th * 1.4f, th), r * 1.05f, g * 1.05f, b * 1.05f); }
static void f_door(float hw, float hh, const float *dc, const float *sc) {
    mat(MAT_PLAIN); arch_fill(hw + 0.03f, hh + 0.03f, 0.012f, 0.05f, 0.04f, 0.035f);
    mat(MAT_WOOD); US = 1.0f; arch_fill(hw - 0.03f, hh - 0.03f, 0.04f, dc[0], dc[1], dc[2]);
    cbox(v3(0, (hh - hw) * 0.5f, 0.05f), v3(0.035f, (hh - hw) * 0.5f, 0.02f), dc[0] * 0.7f, dc[1] * 0.7f, dc[2] * 0.7f);   /* the two leaves meet */
    mat(MAT_PLAIN); for (int sd = -1; sd <= 1; sd += 2) { for (int j = 0; j < 3; j++) cbox(v3(sd * hw * 0.5f, 0.35f + j * (hh - hw) * 0.38f, 0.06f), v3(0.035f, 0.035f, 0.025f), 0.16f, 0.14f, 0.12f);
        cbox(v3(sd * 0.14f, (hh - hw) * 0.55f, 0.07f), v3(0.05f, 0.05f, 0.03f), 0.8f, 0.62f, 0.3f); }   /* iron studs and brass rings */
    mat(MAT_STONE); arch_frame(hw, hh, 0.13f, sc[0], sc[1], sc[2]);
    cbox(v3(0, 0.09f, 0.32f), v3(hw + 0.4f, 0.09f, 0.32f), sc[0] * 0.92f, sc[1] * 0.92f, sc[2] * 0.92f); }   /* the doorstep */
static void f_window(float hw, float hh, const float *shc, const float *sc, int flowers) {
    mat(MAT_PLAIN); arch_fill(hw, hh, 0.012f, 0.045f, 0.04f, 0.035f);
    mat(MAT_WOOD); for (int k = -1; k <= 1; k++) cbox(v3(k * hw * 0.5f, hh * 0.45f, 0.035f), v3(0.022f, hh * 0.45f, 0.018f), 0.4f, 0.29f, 0.19f);   /* the wooden grille */
    for (int j = 1; j <= 2; j++) cbox(v3(0, (hh - hw) * j / 2.5f, 0.04f), v3(hw, 0.022f, 0.018f), 0.4f, 0.29f, 0.19f);
    for (int sd = -1; sd <= 1; sd += 2) cbox(v3(sd * (hw * 2.0f + 0.08f), (hh - hw * 0.4f) * 0.5f, 0.06f), v3(hw * 0.95f, (hh - hw * 0.4f) * 0.5f, 0.035f), shc[0], shc[1], shc[2]);   /* shutters folded open */
    mat(MAT_STONE); cbox(v3(0, -0.05f, 0.1f), v3(hw + 0.2f, 0.07f, 0.15f), sc[0], sc[1], sc[2]);   /* the sill */
    arch_frame(hw, hh, 0.08f, sc[0], sc[1], sc[2]);
    if (flowers) { mat(MAT_WOOD); cbox(v3(0, -0.28f, 0.28f), v3(hw + 0.15f, 0.15f, 0.16f), 0.5f, 0.36f, 0.22f);
        mat(MAT_LEAF); for (int k = 0; k < 4; k++) { float x = -hw + (k + 0.5f) * (hw * 2) / 4; blob(v3(x, -0.08f, 0.3f), 0.2f, 0.16f, 0.16f, 0.2f, 3, 5, 0.45f, 0.7f, 0.3f);
            mat(MAT_PLAIN); blob(v3(x + 0.05f, 0.02f, 0.38f), 0.07f, 0.06f, 0.06f, 0.1f, 2, 4, k & 1 ? 0.95f : 0.9f, k & 1 ? 0.35f : 0.3f, k & 1 ? 0.45f : 0.25f); mat(MAT_LEAF); } } }
static void f_awning(float hw, float y, float out, const float *c1, const float *c2) {   /* a striped cloth awning sagging between its poles */
    enum { NS = 8, NO = 4 }; mat(MAT_CLOTH); Vec3 p[NS + 1][NO + 1];
    for (int i = 0; i <= NS; i++) for (int j = 0; j <= NO; j++) { float tx = (float)i / NS, to = (float)j / NO, sag = sinf(tx * 3.14159265f) * 0.18f * to + sinf(to * 3.14159265f) * 0.08f;
        p[i][j] = v3(-hw + tx * hw * 2, y - to * 0.55f - sag, 0.05f + to * out); }
    for (int i = 0; i < NS; i++) { const float *c = (i & 1) ? c2 : c1;
        for (int j = 0; j < NO; j++) { fquad(p[i][j], p[i + 1][j], p[i + 1][j + 1], p[i][j + 1], c[0], c[1], c[2]); { Vec3 o = v3(0, -0.03f, 0); fquad(vadd(p[i][j + 1], o), vadd(p[i + 1][j + 1], o), vadd(p[i + 1][j], o), vadd(p[i][j], o), c[0] * 0.75f, c[1] * 0.75f, c[2] * 0.75f); } }
        Vec3 a = p[i][NO], b = p[i + 1][NO]; fquad(a, b, v3(b.x, b.y - 0.22f, b.z), v3(a.x, a.y - 0.22f, a.z), c[0] * 0.9f, c[1] * 0.9f, c[2] * 0.9f);   /* the hanging valance */
        fquad(v3(a.x, a.y - 0.22f, a.z), v3(b.x, b.y - 0.22f, b.z), b, a, c[0] * 0.7f, c[1] * 0.7f, c[2] * 0.7f); }
    mat(MAT_WOOD); for (int sd = -1; sd <= 1; sd += 2) tube2(v3(sd * hw, 0, out + 0.05f), v3(sd * hw, y - 0.5f, out + 0.05f), 0.06f, 0.05f, 6, 0.48f, 0.35f, 0.22f);
    tube2(v3(-hw, y - 0.55f, out + 0.05f), v3(hw, y - 0.55f, out + 0.05f), 0.045f, 0.045f, 5, 0.48f, 0.35f, 0.22f); }
static void jar2(float x, float z, float s, float r, float g, float b) { mat(MAT_PLAIN); Vec3 p[7] = {v3(x, 0, z), v3(x, s * 0.2f, z), v3(x, s * 0.55f, z), v3(x, s * 0.8f, z), v3(x, s * 0.9f, z), v3(x, s * 0.97f, z), v3(x, s, z)};
    float rr[7] = {s * 0.16f, s * 0.3f, s * 0.36f, s * 0.24f, s * 0.13f, s * 0.15f, s * 0.13f}; tube(p, rr, 7, 10, r, g, b, 1); }
static void potplant(float x, float z, float s) { jar2(x, z, s * 0.5f, 0.75f, 0.42f, 0.26f); mat(MAT_LEAF); blob(v3(x, s * 0.75f, z), s * 0.38f, s * 0.35f, s * 0.38f, 0.22f, 5, 8, 0.4f, 0.68f, 0.3f); }
static void parapet(float x0, float x1, float z0, float z1, float y, float r, float g, float b, int merlons) {   /* a low wall round a flat roof */
    float t = 0.16f, hgt = 0.5f, cx = (x0 + x1) / 2, cz = (z0 + z1) / 2, hx = (x1 - x0) / 2, hz = (z1 - z0) / 2;
    mat(MAT_PLASTER); rbox(v3(cx, y + hgt / 2, z1 - t), hx, hgt / 2, t, 0.08f, r, g, b); rbox(v3(cx, y + hgt / 2, z0 + t), hx, hgt / 2, t, 0.08f, r, g, b);
    rbox(v3(x0 + t, y + hgt / 2, cz), t, hgt / 2, hz - 2 * t, 0.08f, r, g, b); rbox(v3(x1 - t, y + hgt / 2, cz), t, hgt / 2, hz - 2 * t, 0.08f, r, g, b);
    if (merlons) { int nx = (int)(hx / 0.7f); for (int k = 0; k <= nx; k++) { float x = x0 + 0.25f + (x1 - x0 - 0.5f) * k / nx; for (int sd = 0; sd < 2; sd++) rbox(v3(x, y + hgt + 0.18f, sd ? z1 - t : z0 + t), 0.2f, 0.18f, t, 0.07f, r, g, b); } }
    mat(MAT_PLASTER); rbox(v3(cx, y + 0.03f, cz), hx - 0.1f, 0.04f, hz - 0.1f, 0.02f, 0.78f, 0.64f, 0.48f); }   /* the packed-clay roof */
static void vigas(float x0, float x1, float zf, float y, int n) { mat(MAT_WOOD); for (int k = 0; k < n; k++) { float x = x0 + (x1 - x0) * (k + 0.5f) / n; tube2(v3(x, y, zf - 0.3f), v3(x, y - 0.02f, zf + 0.42f), 0.1f, 0.09f, 6, 0.46f, 0.33f, 0.21f); } }
static void quoins(float x0, float x1, float z0, float z1, float y0, float y1, const float *sc) {   /* dressed stones up every corner, long and short in turn, and a moulded band under the roof */
    mat(MAT_STONE); int n = (int)((y1 - y0) / 0.42f); if (n < 2) n = 2; float bh = (y1 - y0) / n;
    for (int c = 0; c < 4; c++) { float x = c & 1 ? x1 : x0, z = c & 2 ? z1 : z0, sx = c & 1 ? -1.0f : 1.0f, sz = c & 2 ? -1.0f : 1.0f;
        for (int k = 0; k < n; k++) { float y = y0 + bh * (k + 0.5f), sh = 0.94f + 0.08f * hsh(c, k, 5);   /* one stone a course, long on one face then the other */
            if (k & 1) cbox(v3(x + sx * 0.24f, y, z - sz * 0.13f), v3(0.3f, bh / 2 - 0.02f, 0.2f), sc[0] * sh, sc[1] * sh, sc[2] * sh);
            else cbox(v3(x - sx * 0.13f, y, z + sz * 0.24f), v3(0.2f, bh / 2 - 0.02f, 0.3f), sc[0] * sh, sc[1] * sh, sc[2] * sh); } }
    float cx = (x0 + x1) / 2, cz = (z0 + z1) / 2, hx = (x1 - x0) / 2, hz = (z1 - z0) / 2;
    cbox(v3(cx, y1 - 0.06f, z1 + 0.02f), v3(hx + 0.1f, 0.09f, 0.08f), sc[0], sc[1], sc[2]); cbox(v3(cx, y1 - 0.06f, z0 - 0.02f), v3(hx + 0.1f, 0.09f, 0.08f), sc[0], sc[1], sc[2]);
    cbox(v3(x1 + 0.02f, y1 - 0.06f, cz), v3(0.08f, 0.09f, hz + 0.1f), sc[0], sc[1], sc[2]); cbox(v3(x0 - 0.02f, y1 - 0.06f, cz), v3(0.08f, 0.09f, hz + 0.1f), sc[0], sc[1], sc[2]); }
static void ao_pass(const uint32_t *n0, float y0, float span) {   /* darker toward the ground, where the walls see less sky */
    for (int m = 0; m < MAT_N; m++) for (uint32_t i = n0[m]; i < O->b[m]->n; i++) { Vertex *v = &O->b[m]->v[i]; float t = clampf((v->py - y0) / span, 0, 1); t = t * t * (3 - 2 * t); float f = 0.66f + 0.34f * t; v->r *= f; v->g *= f; v->b *= f; } }

static void m_bld(float h, float w, unsigned sd, float tr, float tg, float tb, int kind) {   /* kind 0 one-storey house, 1 two-storey house, 2 the inn */
    static const float PAL[5][3] = {{0.98f, 0.96f, 0.91f}, {0.97f, 0.89f, 0.72f}, {0.95f, 0.83f, 0.62f}, {0.97f, 0.87f, 0.79f}, {0.93f, 0.9f, 0.82f}};
    static const float DOOR[5][3] = {{0.2f, 0.42f, 0.7f}, {0.13f, 0.5f, 0.52f}, {0.55f, 0.3f, 0.17f}, {0.32f, 0.5f, 0.28f}, {0.62f, 0.2f, 0.16f}};
    static const float AWN[4][2][3] = {{{0.85f, 0.2f, 0.16f}, {0.96f, 0.92f, 0.82f}}, {{0.18f, 0.38f, 0.75f}, {0.96f, 0.92f, 0.82f}}, {{0.2f, 0.55f, 0.35f}, {0.95f, 0.85f, 0.45f}}, {{0.6f, 0.2f, 0.45f}, {0.96f, 0.92f, 0.82f}}};
    unsigned hs = sd * 2654435761u; const float *pc = PAL[(hs >> 3) % 5], *dc = DOOR[(hs >> 7) % 5], (*aw)[3] = AWN[(hs >> 11) % 4];
    float wr = pc[0] * 0.8f + tr * 0.2f, wg = pc[1] * 0.8f + tg * 0.2f, wb = pc[2] * 0.8f + tb * 0.2f;   /* the walls, a little of the road's own stone */
    float sc[3] = {0.84f * 0.75f + tr * 0.25f, 0.76f * 0.75f + tg * 0.25f, 0.6f * 0.75f + tb * 0.25f}, shc[3] = {dc[0] * 0.9f + 0.05f, dc[1] * 0.9f + 0.05f, dc[2] * 0.9f + 0.05f};
    uint32_t n0[MAT_N]; for (int m = 0; m < MAT_N; m++) n0[m] = O->b[m]->n;
    float d = w * (kind == 2 ? 0.75f : 0.82f), wh = kind == 0 ? fminf(h * 0.62f, 4.2f) : fminf(h * 0.5f, 3.8f), fh = 0.45f;   /* storey height, stone footing */
    /* footing and the ground floor */
    mat(MAT_STONE); rbox(v3(0, fh / 2, 0), w / 2 + 0.15f, fh / 2, d / 2 + 0.15f, 0.08f, sc[0] * 0.95f, sc[1] * 0.95f, sc[2] * 0.95f);
    mat(MAT_PLASTER); rbox(v3(0, fh + wh / 2, 0), w / 2, wh / 2, d / 2, 0.32f, wr, wg, wb);
    float top = fh + wh;
    quoins(-w / 2, w / 2, -d / 2, d / 2, fh, top, sc);
    /* the second storey: the whole house (inn) or an upper room on the back half of the roof */
    float uw = kind == 2 ? w : w * 0.5f, ud = kind == 2 ? d : d * 0.5f, ux = kind == 2 ? 0 : -w * 0.2f, uz = kind == 2 ? 0 : -d * 0.22f, uh = wh * (kind == 2 ? 0.9f : 0.82f);
    if (kind >= 1) { mat(MAT_PLASTER); rbox(v3(ux, top + uh / 2, uz), uw / 2 - (kind == 2 ? 0.12f : 0), uh / 2, ud / 2 - (kind == 2 ? 0.12f : 0), 0.3f, wr * 1.01f, wg * 1.01f, wb * 1.01f);
        if (kind == 2) { mat(MAT_STONE); rbox(v3(0, top + 0.08f, 0), w / 2 + 0.12f, 0.1f, d / 2 + 0.12f, 0.04f, sc[0], sc[1], sc[2]); }   /* a string course between the floors */
        quoins(ux - uw / 2 + (kind == 2 ? 0.12f : 0), ux + uw / 2 - (kind == 2 ? 0.12f : 0), uz - ud / 2 + (kind == 2 ? 0.12f : 0), uz + ud / 2 - (kind == 2 ? 0.12f : 0), top + 0.2f, top + uh, sc);
        parapet(ux - uw / 2, ux + uw / 2, uz - ud / 2, uz + ud / 2, top + uh, wr, wg, wb, kind == 2);
        vigas(ux - uw / 2 + 0.3f, ux + uw / 2 - 0.3f, uz + ud / 2, top + uh - 0.25f, (int)(uw / 0.9f)); }
    if (kind != 2) { parapet(-w / 2, w / 2, -d / 2, d / 2, top, wr, wg, wb, 0); vigas(-w / 2 + 0.3f, w / 2 - 0.3f, d / 2, top - 0.25f, (int)(w / 0.85f)); }
    else vigas(-w / 2 + 0.3f, w / 2 - 0.3f, d / 2, top - 0.2f, (int)(w / 0.85f));
    /* the front: the inn has an arcade of arches, a house a doorway and windows */
    if (kind == 2) { int na = w > 9 ? 4 : 3; float span = w * 0.86f;
        for (int k = 0; k < na; k++) { float x = -span / 2 + span * (k + 0.5f) / na, aw2 = span / na * 0.36f;
            feat_begin(); if (k == 1) f_door(aw2, wh * 0.78f, dc, sc); else { mat(MAT_PLAIN); arch_fill(aw2, wh * 0.78f, 0.012f, 0.06f, 0.05f, 0.045f); mat(MAT_STONE); arch_frame(aw2, wh * 0.78f, 0.14f, sc[0], sc[1], sc[2]);
                mat(MAT_PLAIN); jar2(aw2 * 0.55f, 0.45f, 0.8f, 0.72f, 0.42f, 0.26f); }
            feat_end(v3(x, fh, d / 2), 0); }
        for (int k = 0; k < na; k++) { float x = -span / 2 + span * (k + 0.5f) / na; feat_begin(); f_window(0.32f, 0.95f, shc, sc, k & 1); feat_end(v3(x, top + uh * 0.35f, d / 2 - 0.1f), 0); }
        for (int fl = 0; fl < 2; fl++) for (int sd2 = -1; sd2 <= 1; sd2 += 2) { float y = fl ? top + uh * 0.35f : fh + wh * 0.42f;   /* windows on the back and both ends, both floors */
            feat_begin(); f_window(0.3f, 0.9f, shc, sc, fl); feat_end(v3(sd2 * w * 0.25f, y, -d / 2 + (fl ? 0.12f : 0)), 3.14159265f);
            feat_begin(); f_window(0.3f, 0.9f, shc, sc, 0); feat_end(v3(sd2 * (w / 2 - (fl ? 0.12f : 0)), y, 0), sd2 * 1.5707963f); }
        /* a wooden balcony along the upper floor */
        mat(MAT_WOOD); rbox(v3(0, top + 0.12f, d / 2 + 0.55f), w * 0.4f, 0.08f, 0.55f, 0.03f, 0.5f, 0.36f, 0.22f);
        for (int k = 0; k <= 12; k++) { float x = -w * 0.4f + w * 0.8f * k / 12; tube2(v3(x, top + 0.2f, d / 2 + 1.05f), v3(x, top + 1.0f, d / 2 + 1.05f), 0.035f, 0.035f, 4, 0.45f, 0.32f, 0.2f); }
        tube2(v3(-w * 0.4f, top + 1.02f, d / 2 + 1.05f), v3(w * 0.4f, top + 1.02f, d / 2 + 1.05f), 0.06f, 0.06f, 5, 0.5f, 0.36f, 0.22f);
        for (int sd = -1; sd <= 1; sd += 2) for (int j = 0; j < 2; j++) tube2(v3(sd * w * 0.38f, top - 0.4f, d / 2 + 0.05f), v3(sd * w * 0.38f, top + 0.05f, d / 2 + 1.0f), 0.06f, 0.05f, 5, 0.45f, 0.32f, 0.2f);
        /* a small white dome on the roof, and a striped awning over one end */
        mat(MAT_PLASTER); rbox(v3(w * 0.22f, top + uh + 0.5f, -d * 0.1f), 1.25f, 0.5f, 1.25f, 0.2f, wr, wg, wb); blob(v3(w * 0.22f, top + uh + 1.0f, -d * 0.1f), 1.2f, 1.15f, 1.2f, 0.0f, 8, 14, 0.98f, 0.97f, 0.94f);
        feat_begin(); f_awning(w * 0.18f, wh * 0.92f, 1.8f, aw[0], aw[1]); feat_end(v3(w * 0.3f, fh, d / 2), 0);
        mat(MAT_WOOD); rbox(v3(-w * 0.32f, 0.3f, d / 2 + 1.0f), 0.9f, 0.3f, 0.35f, 0.05f, 0.5f, 0.36f, 0.22f);   /* a bench outside */
    } else {
        float dx = ((hs >> 13) & 1) ? w * 0.18f : -w * 0.12f;
        feat_begin(); f_door(fminf(0.62f, w * 0.08f), fminf(wh * 0.72f, 2.5f), dc, sc); feat_end(v3(dx, fh, d / 2), 0);
        feat_begin(); f_window(0.3f, 0.85f, shc, sc, (hs >> 15) & 1); feat_end(v3(dx < 0 ? w * 0.26f : -w * 0.28f, fh + wh * 0.48f, d / 2), 0);
        if (w > 7.5f) { feat_begin(); f_window(0.3f, 0.85f, shc, sc, 0); feat_end(v3(dx < 0 ? -w * 0.36f : w * 0.4f, fh + wh * 0.48f, d / 2), 0); }
        for (int sd2 = -1; sd2 <= 1; sd2 += 2) { feat_begin(); f_window(0.28f, 0.8f, shc, sc, 0); feat_end(v3(sd2 * w / 2, fh + wh * 0.5f, d * 0.12f), sd2 * 1.5707963f); }   /* the side walls */
        feat_begin(); f_door(0.55f, 2.1f, dc, sc); feat_end(v3(-dx * 0.5f, fh, -d / 2), 3.14159265f);   /* the back door */
        for (int sd2 = -1; sd2 <= 1; sd2 += 2) { feat_begin(); f_window(0.28f, 0.8f, shc, sc, sd2 > 0); feat_end(v3(-dx * 0.5f + sd2 * w * 0.3f, fh + wh * 0.5f, -d / 2), 3.14159265f); }
        if (kind == 1) { feat_begin(); f_window(0.28f, 0.8f, shc, sc, 1); feat_end(v3(ux, top + uh * 0.3f, uz + ud / 2), 0);
            feat_begin(); f_door(0.45f, 1.9f, dc, sc); feat_end(v3(ux + uw / 2, top, uz + ud * 0.1f), 1.5707963f); }   /* the upper room's door onto the roof */
        /* the outside stair to the roof, up the side wall from the front */
        int right = (hs >> 17) & 1; float sx = right ? w / 2 + 0.55f : -w / 2 - 0.55f; int ns = 9;
        mat(MAT_STONE); for (int k = 0; k < ns; k++) { float t = (k + 1.0f) / ns, y = top * t; rbox(v3(sx, y / 2, d / 2 - 0.5f - (d - 1.2f) * k / ns), 0.5f, y / 2, (d - 1.2f) / ns * 0.55f, 0.04f, sc[0] * 0.95f, sc[1] * 0.95f, sc[2] * 0.95f); }
        mat(MAT_PLASTER); for (int k = 0; k < ns; k++) { float t = (k + 1.0f) / ns, y = top * t; rbox(v3(sx + (right ? 0.5f : -0.5f), y / 2 + 0.3f, d / 2 - 0.5f - (d - 1.2f) * k / ns), 0.1f, y / 2 + 0.3f, (d - 1.2f) / ns * 0.55f, 0.04f, wr, wg, wb); }   /* its low side wall */
        /* the roof terrace: a vine arbor on poles (or drying washing) */
        if ((hs >> 19) % 3 != 0) { float px0 = kind == 1 ? w * 0.05f : -w * 0.3f, px1 = w / 2 - 0.5f, pz0 = kind == 1 ? 0.0f : -d * 0.3f, pz1 = d / 2 - 0.5f;
            mat(MAT_WOOD); for (int k = 0; k < 4; k++) { float x = k & 1 ? px1 : px0, z = k & 2 ? pz1 : pz0; tube2(v3(x, top, z), v3(x, top + 2.1f, z), 0.07f, 0.06f, 6, 0.45f, 0.33f, 0.21f); }
            for (int k = 0; k < 5; k++) { float z = pz0 + (pz1 - pz0) * k / 4; tube2(v3(px0 - 0.2f, top + 2.12f, z), v3(px1 + 0.2f, top + 2.12f, z), 0.045f, 0.045f, 5, 0.45f, 0.33f, 0.21f); }
            mat(MAT_LEAF); for (int k = 0; k < 5; k++) { float x = px0 + (px1 - px0) * (k + 0.5f) / 5; blob(v3(x, top + 2.3f, (pz0 + pz1) / 2), (px1 - px0) / 5 * 0.85f, 0.32f, (pz1 - pz0) / 2 + 0.2f, 0.28f, 4, 8, 0.38f, 0.62f, 0.26f); }
            mat(MAT_PLAIN); for (int k = 0; k < 6; k++) { float x = px0 + (px1 - px0) * (k + 0.5f) / 6; blob(v3(x, top + 1.95f, pz0 + (pz1 - pz0) * ((k * 37) % 5) / 5.0f), 0.1f, 0.14f, 0.1f, 0.1f, 3, 6, 0.42f, 0.2f, 0.42f); } }   /* bunches of grapes */
        else { mat(MAT_WOOD); tube2(v3(-w * 0.35f, top + 1.6f, d * 0.2f), v3(w * 0.35f, top + 1.6f, d * 0.2f), 0.02f, 0.02f, 3, 0.7f, 0.65f, 0.55f);
            for (int sd2 = -1; sd2 <= 1; sd2 += 2) tube2(v3(sd2 * w * 0.35f, top, d * 0.2f), v3(sd2 * w * 0.35f, top + 1.7f, d * 0.2f), 0.05f, 0.05f, 5, 0.45f, 0.33f, 0.21f);
            mat(MAT_CLOTH); for (int k = 0; k < 4; k++) { float x = -w * 0.28f + k * w * 0.18f; const float *c = aw[k & 1]; fquad(v3(x - 0.35f, top + 0.75f, d * 0.2f), v3(x + 0.35f, top + 0.75f, d * 0.2f), v3(x + 0.35f, top + 1.6f, d * 0.2f), v3(x - 0.35f, top + 1.6f, d * 0.2f), c[0], c[1], c[2]);
                fquad(v3(x - 0.35f, top + 1.6f, d * 0.2f), v3(x + 0.35f, top + 1.6f, d * 0.2f), v3(x + 0.35f, top + 0.75f, d * 0.2f), v3(x - 0.35f, top + 0.75f, d * 0.2f), c[0] * 0.8f, c[1] * 0.8f, c[2] * 0.8f); } }
        if ((hs >> 21) & 1) { feat_begin(); f_awning(w * 0.2f, wh * 0.85f, 1.5f, aw[0], aw[1]); feat_end(v3(dx < 0 ? w * 0.26f : -w * 0.28f, fh, d / 2), 0); }
    }
    /* things by the door */
    jar2(-w * 0.42f, d / 2 + 0.45f, 0.85f, 0.76f, 0.44f, 0.27f); jar2(-w * 0.35f, d / 2 + 0.7f, 0.6f, 0.7f, 0.4f, 0.25f);
    potplant(w * 0.42f, d / 2 + 0.5f, 1.1f); if ((hs >> 23) & 1) potplant(w * 0.05f - (kind == 2 ? 0 : w * 0.3f), d / 2 + 0.55f, 0.9f);
    ao_pass(n0, 0.0f, 2.4f); }

static void m_stall2(float h, float w, unsigned sd) {   /* a market stall: a cloth canopy on poles over a counter of baskets, rugs and jars */
    static const float CL[4][3] = {{0.86f, 0.22f, 0.16f}, {0.2f, 0.4f, 0.78f}, {0.92f, 0.7f, 0.2f}, {0.25f, 0.58f, 0.38f}};
    unsigned hs = sd * 2654435761u; const float *c1 = CL[(hs >> 5) % 4], cw[3] = {0.97f, 0.93f, 0.84f}; uint32_t n0[MAT_N]; for (int m = 0; m < MAT_N; m++) n0[m] = O->b[m]->n;
    float W = fmaxf(w * 0.45f, 1.8f), D = 1.4f, H = fmaxf(h * 0.55f, 2.4f);
    mat(MAT_WOOD); for (int k = 0; k < 4; k++) { float x = k & 1 ? W : -W, z = k & 2 ? D : -D; tube2(v3(x, 0, z), v3(x, H + (k & 2 ? 0 : 0.5f), z), 0.08f, 0.07f, 6, 0.48f, 0.35f, 0.22f); }
    for (int k = 0; k < 2; k++) tube2(v3(-W, H + (k ? 0 : 0.5f), k ? D : -D), v3(W, H + (k ? 0 : 0.5f), k ? D : -D), 0.06f, 0.06f, 5, 0.48f, 0.35f, 0.22f);
    /* the canopy: sloping front to back, sagging in the middle, striped */
    enum { NX = 8, NZ = 4 }; Vec3 p[NX + 1][NZ + 1]; mat(MAT_CLOTH);
    for (int i = 0; i <= NX; i++) for (int j = 0; j <= NZ; j++) { float tx = (float)i / NX, tz = (float)j / NZ; p[i][j] = v3(-W - 0.15f + tx * (W + 0.15f) * 2, H + 0.5f - tz * 0.5f + 0.05f - sinf(tx * 3.14159f) * sinf(tz * 3.14159f) * 0.22f, -D - 0.15f + tz * (D + 0.15f) * 2); }
    for (int i = 0; i < NX; i++) { const float *c = (i & 1) ? cw : c1; for (int j = 0; j < NZ; j++) { fquad(p[i][j], p[i][j + 1], p[i + 1][j + 1], p[i + 1][j], c[0], c[1], c[2]); { Vec3 o = v3(0, -0.03f, 0); fquad(vadd(p[i + 1][j], o), vadd(p[i + 1][j + 1], o), vadd(p[i][j + 1], o), vadd(p[i][j], o), c[0] * 0.72f, c[1] * 0.72f, c[2] * 0.72f); } }
        Vec3 a = p[i][NZ], b = p[i + 1][NZ]; fquad(a, b, v3(b.x, b.y - 0.3f, b.z), v3(a.x, a.y - 0.3f, a.z), c[0] * 0.9f, c[1] * 0.9f, c[2] * 0.9f); fquad(v3(a.x, a.y - 0.3f, a.z), v3(b.x, b.y - 0.3f, b.z), b, a, c[0] * 0.7f, c[1] * 0.7f, c[2] * 0.7f); }
    /* the counter and its baskets of fruit, grain, bread and olives */
    mat(MAT_WOOD); rbox(v3(0, 0.45f, D - 0.35f), W - 0.1f, 0.45f, 0.4f, 0.05f, 0.55f, 0.4f, 0.25f);
    static const float FR[5][3] = {{1.0f, 0.55f, 0.1f}, {0.75f, 0.12f, 0.15f}, {0.45f, 0.5f, 0.2f}, {0.9f, 0.75f, 0.4f}, {0.55f, 0.25f, 0.45f}};
    int nb = (int)(W / 0.6f); for (int k = 0; k < nb; k++) { float x = -W + 0.45f + (2 * W - 0.9f) * k / (nb > 1 ? nb - 1 : 1); const float *f = FR[(k + hs) % 5];
        mat(MAT_THATCH); { Vec3 q[3] = {v3(x, 0.9f, D - 0.35f), v3(x, 1.02f, D - 0.35f), v3(x, 1.1f, D - 0.35f)}; float rr[3] = {0.2f, 0.26f, 0.28f}; tube(q, rr, 3, 10, 0.85f, 0.72f, 0.48f, 0); }
        mat(MAT_PLAIN); for (int j = 0; j < 5; j++) { float a = j * 1.26f, rr2 = j ? 0.13f : 0; blob(v3(x + cosf(a) * rr2, 1.13f + (j ? 0 : 0.05f), D - 0.35f + sinf(a) * rr2), 0.09f, 0.09f, 0.09f, 0.05f, 3, 6, f[0], f[1], f[2]); } }
    /* a rug hanging on the side, sacks and jars */
    mat(MAT_CLOTH); for (int k = 0; k < 6; k++) { float y0 = 0.4f + k * (H - 0.6f) / 6, y1 = y0 + (H - 0.6f) / 6; const float *c = (k & 1) ? cw : CL[(hs >> 9) % 4];
        fquad(v3(W + 0.1f, y0, -D + 0.2f), v3(W + 0.1f, y0, D - 0.2f), v3(W + 0.1f, y1, D - 0.2f), v3(W + 0.1f, y1, -D + 0.2f), c[0] * 0.95f, c[1] * 0.95f, c[2] * 0.95f);
        fquad(v3(W + 0.08f, y1, -D + 0.2f), v3(W + 0.08f, y1, D - 0.2f), v3(W + 0.08f, y0, D - 0.2f), v3(W + 0.08f, y0, -D + 0.2f), c[0] * 0.7f, c[1] * 0.7f, c[2] * 0.7f); }
    mat(MAT_CLOTH); for (int k = 0; k < 3; k++) blob(v3(-W + 0.4f + k * 0.55f, 0.38f, -D + 0.5f), 0.28f, 0.4f, 0.26f, 0.12f, 5, 8, 0.82f, 0.72f, 0.52f);
    jar2(-W - 0.5f, D - 0.2f, 0.9f, 0.74f, 0.43f, 0.27f); jar2(-W - 0.3f, D + 0.35f, 0.65f, 0.7f, 0.4f, 0.25f);
    ao_pass(n0, 0.0f, 1.6f); }

/* a waterfall's rock: a cliff of big rounded boulders with a notch in the middle where the water pours over, mossy on
   top, and a ring of rocks round the pool at its foot (the falling water itself is drawn by the game, moving). Faces +z. */
static void m_falls(float h) { float W = h * 0.9f; mat(MAT_ROCK);
    blob(v3(-W * 0.42f, h * 0.48f, -W * 0.28f), W * 0.36f, h * 0.55f, W * 0.34f, 0.18f, 8, 12, 0.6f, 0.56f, 0.5f);
    blob(v3(W * 0.42f, h * 0.45f, -W * 0.26f), W * 0.36f, h * 0.52f, W * 0.34f, 0.18f, 8, 12, 0.62f, 0.58f, 0.52f);
    blob(v3(0, h * 0.42f, -W * 0.45f), W * 0.32f, h * 0.55f, W * 0.3f, 0.15f, 8, 12, 0.55f, 0.52f, 0.47f);   /* behind the water */
    blob(v3(-W * 0.62f, h * 0.2f, -W * 0.05f), W * 0.22f, h * 0.24f, W * 0.22f, 0.22f, 6, 9, 0.58f, 0.55f, 0.48f);
    blob(v3(W * 0.6f, h * 0.18f, -W * 0.02f), W * 0.22f, h * 0.22f, W * 0.22f, 0.22f, 6, 9, 0.6f, 0.57f, 0.5f);
    mat(MAT_LEAF); for (int sd = -1; sd <= 1; sd += 2) blob(v3(sd * W * 0.42f, h * 0.98f, -W * 0.3f), W * 0.3f, h * 0.1f, W * 0.28f, 0.25f, 5, 9, 0.38f, 0.6f, 0.28f);   /* moss and grass on top */
    mat(MAT_ROCK); for (int k = 0; k < 9; k++) { float a = 3.14159265f * (0.05f + 0.9f * k / 8.0f), R = W * 0.48f;
        blob(v3(cosf(a) * R, h * 0.04f, W * 0.3f + sinf(a) * R * 0.75f), W * 0.12f, h * 0.06f, W * 0.1f, 0.25f, 4, 7, 0.55f, 0.53f, 0.48f); } }
static const char *const NAMES[] = {"#tree", "#olive", "#fruittree", "#treeoflife", "#palm", "#cypress", "#flowers", "#lily", "#fern", "#lantern", "#crystal", "#sheep", "#lamb",
    "#deer", "#lion", "#boat", "#net", "#priest", "#israelite", "#philistine", "#banner_israel", "#banner_philistine", "#cloud", "#cart", "#ark", "#tomb", "#crosses",
    "#stable", "#camp", "#stone", "#rubble", "#house1", "#house2", "#inn", "#stall", "#hut", "#tower", "#pillar", "#gpillar", "#tent", "#rock", "#well", "#wallseg", "#waterfall"};
int model_known(const char *img) { for (unsigned i = 0; i < sizeof NAMES / sizeof NAMES[0]; i++) if (!strcmp(img, NAMES[i])) return 1; return 0; }
int model_build(ModelOut *o, const char *img, float h, float w, unsigned seed, Vec3 at, float yaw, float tr, float tg, float tb) {
    O = o; g_seed = seed * 2654435761u + 12345u; rnd(); nosway(); FLUTES = 0; mat(MAT_PLAIN);
    uint32_t n0[MAT_N]; for (int m = 0; m < MAT_N; m++) n0[m] = o->b[m]->n;
    for (int m = 0; m < MAT_N; m++) if (o->b[m]->n + 13000 > o->b[m]->cap) return -1;   /* no room: the caller empties the buckets first */
    if (w < h * 0.6f) w = h * 0.6f;
    if (!strcmp(img, "#tree")) m_tree(h, 0); else if (!strcmp(img, "#olive")) m_tree(h, 1); else if (!strcmp(img, "#fruittree")) m_tree(h, 2); else if (!strcmp(img, "#treeoflife")) m_tree(h, 3);
    else if (!strcmp(img, "#palm")) m_palm(h); else if (!strcmp(img, "#cypress")) m_cypress(h);
    else if (!strcmp(img, "#flowers")) m_flowers(h * 0.4f, 0); else if (!strcmp(img, "#lily")) m_flowers(h * 0.5f, 1); else if (!strcmp(img, "#fern")) m_fern(h);
    else if (!strcmp(img, "#lantern")) m_lantern(h); else if (!strcmp(img, "#crystal")) m_crystal(h);
    else if (!strcmp(img, "#sheep")) m_sheep(0); else if (!strcmp(img, "#lamb")) m_sheep(1); else if (!strcmp(img, "#deer")) m_deer(); else if (!strcmp(img, "#lion")) m_lion();
    else if (!strcmp(img, "#boat")) m_boat(h); else if (!strcmp(img, "#net")) m_net(h);
    else if (!strcmp(img, "#priest")) m_person(h, 0.95f, 0.95f, 0.92f, 0, 1); else if (!strcmp(img, "#israelite")) m_person(h, 0.66f, 0.5f, 0.34f, 0, 0); else if (!strcmp(img, "#philistine")) m_person(h, 0.72f, 0.22f, 0.2f, 1, 0);
    else if (!strcmp(img, "#banner_israel")) m_banner(h, 0.25f, 0.45f, 0.9f); else if (!strcmp(img, "#banner_philistine")) m_banner(h, 0.85f, 0.2f, 0.15f);
    else if (!strcmp(img, "#cloud")) m_cloud(h); else if (!strcmp(img, "#cart")) m_cart(); else if (!strcmp(img, "#ark")) m_ark(h);
    else if (!strcmp(img, "#tomb")) m_tomb(h); else if (!strcmp(img, "#crosses")) m_crosses(h); else if (!strcmp(img, "#stable")) m_stable(h); else if (!strcmp(img, "#camp")) m_camp(h);
    else if (!strcmp(img, "#stone")) { mat(MAT_ROCK); blob(v3(0, h * 0.5f, 0), h * 0.5f, h * 0.5f, h * 0.5f, 0.05f, 9, 14, 0.7f, 0.66f, 0.58f); }
    else if (!strcmp(img, "#rubble")) m_rubble(h * 0.6f);
    else if (!strcmp(img, "#house1")) m_bld(h, w, seed, tr, tg, tb, 0); else if (!strcmp(img, "#house2")) m_bld(h, w, seed, tr, tg, tb, 1); else if (!strcmp(img, "#inn")) m_bld(h, w, seed, tr, tg, tb, 2); else if (!strcmp(img, "#stall")) m_stall2(h, w, seed);
    else if (!strcmp(img, "#hut")) m_hut(h, w, 1, 1, 1); else if (!strcmp(img, "#tower")) m_tower(h, w, tr, tg, tb);
    else if (!strcmp(img, "#pillar")) m_pillar(h, tr, tg, tb, tr > 0.96f); else if (!strcmp(img, "#gpillar")) m_pillar(h, 1.0f, 0.85f, 0.45f, 1);
    else if (!strcmp(img, "#tent")) m_tent(h, w); else if (!strcmp(img, "#rock")) m_rocks(h, w, 0.68f, 0.64f, 0.58f); else if (!strcmp(img, "#well")) m_well(h, w, tr, tg, tb); else if (!strcmp(img, "#wallseg")) m_wallseg(h, w, tr, tg, tb); else if (!strcmp(img, "#waterfall")) m_falls(h);
    nosway();
    /* turn it to face 'yaw' and stand it at 'at' */
    float c = cosf(yaw), s_ = sinf(yaw); int added = 0;
    for (int m = 0; m < MAT_N; m++) for (uint32_t i = n0[m]; i < o->b[m]->n; i++) { Vertex *v = &o->b[m]->v[i]; float x = v->px, z = v->pz, nx = v->nx, nz = v->nz;
        v->px = at.x + x * c + z * s_; v->pz = at.z - x * s_ + z * c; v->py += at.y; v->nx = nx * c + nz * s_; v->nz = -nx * s_ + nz * c; added++; }
    return added; }

/* a kart tire, radius 1, axle along x from -0.5 to 0.5: grey rubber with raised tread blocks round the
   outside, rounded shoulders, and a gold rim with five spokes and a hub on each side. Turned about x to roll. */
static Batch *TB;
static void ttri(Vec3 a, Vec3 b, Vec3 c, float r, float g, float bl) { if (TB->n + 3 > TB->cap) return; Vec3 n = nrm(a, b, c);
    Vertex v[3] = { {a.x, a.y, a.z, n.x, n.y, n.z, 0, 0, r, g, bl, 1}, {b.x, b.y, b.z, n.x, n.y, n.z, 0, 0, r, g, bl, 1}, {c.x, c.y, c.z, n.x, n.y, n.z, 0, 0, r, g, bl, 1} };
    for (int k = 0; k < 3; k++) TB->v[TB->n++] = v[k]; }
static void tquad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, float r, float g, float bl) { ttri(a, b, c, r, g, bl); ttri(a, c, d, r, g, bl); }
int model_tire(Batch *b) {
    TB = b; int n0 = (int)b->n; const int N = 24; const float rr = 0.36f, gg = 0.34f, bb = 0.33f;
    for (int i = 0; i < N; i++) {
        float a0 = i * 6.2831853f / N, a1 = (i + 1) * 6.2831853f / N, lug = (i & 1) ? 1.0f : 0.9f, sh = (i & 1) ? 1.15f : 0.35f;   /* raised blocks light, the grooves between them dark: the rolling shows */
        Vec3 o0 = v3(0, sinf(a0), cosf(a0)), o1 = v3(0, sinf(a1), cosf(a1));
        #define RP(o, rad, x) v3(x, (o).y * (rad), (o).z * (rad))
        tquad(RP(o0, lug, -0.4f), RP(o1, lug, -0.4f), RP(o1, lug, -0.04f), RP(o0, lug, -0.04f), rr * sh, gg * sh, bb * sh);
        tquad(RP(o0, lug * 0.97f, -0.04f), RP(o1, lug * 0.97f, -0.04f), RP(o1, lug * 0.97f, 0.04f), RP(o0, lug * 0.97f, 0.04f), rr * 0.3f, gg * 0.3f, bb * 0.3f);
        tquad(RP(o0, lug, 0.04f), RP(o1, lug, 0.04f), RP(o1, lug, 0.4f), RP(o0, lug, 0.4f), rr * sh, gg * sh, bb * sh);
        for (int s = -1; s <= 1; s += 2) {
            tquad(RP(o0, lug, s * 0.4f), RP(o1, lug, s * 0.4f), RP(o1, 0.9f, s * 0.5f), RP(o0, 0.9f, s * 0.5f), rr * 0.85f, gg * 0.85f, bb * 0.85f);
            tquad(RP(o0, 0.9f, s * 0.5f), RP(o1, 0.9f, s * 0.5f), RP(o1, 0.64f, s * 0.5f), RP(o0, 0.64f, s * 0.5f), rr * 0.75f, gg * 0.75f, bb * 0.75f);
            tquad(RP(o0, 0.64f, s * 0.5f), RP(o1, 0.64f, s * 0.5f), RP(o1, 0.56f, s * 0.46f), RP(o0, 0.56f, s * 0.46f), 0.95f, 0.78f, 0.32f);
            tquad(RP(o0, 0.56f, s * 0.46f), RP(o1, 0.56f, s * 0.46f), RP(o1, 0.18f, s * 0.4f), RP(o0, 0.18f, s * 0.4f), 0.45f, 0.32f, 0.16f);
            ttri(v3(s * 0.44f, 0, 0), RP(o0, 0.18f, s * 0.44f), RP(o1, 0.18f, s * 0.44f), 0.98f, 0.85f, 0.4f); }
        if (lug == 1.0f) for (int s = -1; s <= 1; s += 2) tquad(RP(o0, 0.93f, s * 0.4f), RP(o0, 1.0f, s * 0.4f), RP(o0, 1.0f, -s * 0.4f), RP(o0, 0.93f, -s * 0.4f), rr * 0.6f, gg * 0.6f, bb * 0.6f);
        #undef RP
    }
    for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 5; k++) {
        float a = k * 6.2831853f / 5, ca = cosf(a), sa = sinf(a), w = 0.07f, x = s * 0.43f;
        Vec3 p0 = v3(x, sa * 0.16f - ca * w, ca * 0.16f + sa * w), p1 = v3(x, sa * 0.16f + ca * w, ca * 0.16f - sa * w), p2 = v3(x, sa * 0.58f + ca * w, ca * 0.58f - sa * w), p3 = v3(x, sa * 0.58f - ca * w, ca * 0.58f + sa * w);
        tquad(p0, p1, p2, p3, 1.0f, 0.84f, 0.38f); }
    return (int)b->n - n0;
}

/* a stretch of the walls of Jericho, all in one bucket (drawn with the stone picture): x along its length, up to H,
   from z = -T to 0; coursed stone with battlements, and on Rahab's stretch her house with its window and the scarlet cord */
int model_jwall(Batch *b, float L, float H, float T, int rahab, unsigned seed) {
    ModelOut o; for (int m = 0; m < MAT_N; m++) o.b[m] = b; O = &o; g_seed = seed * 2654435761u + 7u; nosway(); FLUTES = 0; int n0 = (int)b->n;
    mat(MAT_STONE); US = 2.2f;
    rbox(v3(0, H * 0.06f, -T / 2), L / 2 + 0.25f, H * 0.06f, T / 2 + 0.25f, 0.1f, 0.72f, 0.6f, 0.42f);
    for (int c = 0; c < 4; c++) { float y0 = H * 0.12f + (H * 0.88f) * c / 4, y1 = H * 0.12f + (H * 0.88f) * (c + 1) / 4, sh = 0.94f + 0.06f * rnd();
        rbox(v3(0, (y0 + y1) / 2, -T / 2 + 0.06f * (c & 1)), L / 2, (y1 - y0) / 2, T / 2 - 0.03f * c, 0.12f, 0.86f * sh, 0.74f * sh, 0.54f * sh); }
    int nm = (int)(L / 1.6f); for (int k = 0; k < nm; k++) rbox(v3(-L / 2 + 0.8f + k * 1.6f, H + 0.42f, -T / 2), 0.5f, 0.42f, T / 2, 0.08f, 0.82f, 0.7f, 0.5f);
    if (rahab) { mat(MAT_STONE); rbox(v3(0, H + 1.6f, -T / 2), L * 0.3f, 1.6f, T * 0.45f, 0.2f, 0.88f, 0.78f, 0.6f);
        cbox(v3(L * 0.1f, H + 1.8f, 0.02f), v3(0.4f, 0.5f, 0.03f), 0.1f, 0.08f, 0.08f);
        Vec3 p[6]; float rr[6]; for (int k = 0; k < 6; k++) { float t = k / 5.0f; p[k] = v3(L * 0.1f + sinf(t * 4) * 0.08f, H + 1.4f - t * (H * 0.4f + 0.4f), 0.08f); rr[k] = 0.05f; } tube(p, rr, 6, 5, 0.9f, 0.08f, 0.08f, 1); }   /* her window and the scarlet cord (Joshua 2:18) */
    return (int)b->n - n0; }

/* the champion's cup, gold, about 1 tall, standing on y = 0: a round foot, a stem with a knot, a deep bowl with a lip,
   two looping handles and a small cross on the front */
int model_cup(Batch *b) {
    ModelOut o; for (int m = 0; m < MAT_N; m++) o.b[m] = b; O = &o; nosway(); FLUTES = 0; mat(MAT_PLAIN); int n0 = (int)b->n;
    const float gr = 1.0f, gg = 0.8f, gb = 0.32f;
    { Vec3 p[12]; float rr[12]; const float ys[12] = {0, 0.03f, 0.08f, 0.12f, 0.3f, 0.34f, 0.38f, 0.48f, 0.62f, 0.78f, 0.92f, 0.95f}, rs[12] = {0.3f, 0.3f, 0.22f, 0.07f, 0.06f, 0.1f, 0.06f, 0.2f, 0.32f, 0.38f, 0.4f, 0.43f};
      for (int k = 0; k < 12; k++) { p[k] = v3(0, ys[k], 0); rr[k] = rs[k]; } tube(p, rr, 12, 20, gr, gg, gb, 0); }
    { Vec3 p[3] = {v3(0, 0.95f, 0), v3(0, 0.9f, 0), v3(0, 0.6f, 0)}; float rr[3] = {0.4f, 0.37f, 0.05f}; tube(p, rr, 3, 20, gr * 0.6f, gg * 0.6f, gb * 0.6f, 0); }   /* the inside of the bowl */
    for (int sd = -1; sd <= 1; sd += 2) { Vec3 p[9]; float rr[9]; for (int k = 0; k < 9; k++) { float a = -1.3f + k * 2.6f / 8; p[k] = v3(sd * (0.36f + cosf(a) * 0.2f), 0.72f + sinf(a) * 0.17f, 0); rr[k] = 0.035f; } tube(p, rr, 9, 8, gr, gg, gb, 1); }
    cbox(v3(0, 0.7f, 0.37f), v3(0.025f, 0.11f, 0.02f), 1, 0.95f, 0.75f); cbox(v3(0, 0.74f, 0.37f), v3(0.08f, 0.025f, 0.02f), 1, 0.95f, 0.75f);
    return (int)b->n - n0; }
