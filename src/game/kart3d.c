// kart3d.c - loading and drawing a racer's 3D kart.
#include "kart3d.h"
#include "../render/assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int g_little = -1;
static uint32_t be32(uint32_t v) {   /* the file is in the Wii U's byte order (big-endian): turn it round on a little-endian computer (the tests) */
    if (g_little < 0) { uint16_t one = 1; g_little = *(uint8_t *)&one == 1; }
    return g_little ? ((v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24)) : v; }
static void be_words(void *p, size_t n) { uint32_t *w = p; for (size_t i = 0; i < n; i++) w[i] = be32(w[i]); }
Texture *g_kart3d_tex_override;
int kart3d_load(Kart3D *k, const char *name) {
    memset(k, 0, sizeof *k); char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "models/%s.kkm", name);
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    char magic[4]; uint32_t np = 0; int good = fread(magic, 1, 4, f) == 4 && !memcmp(magic, "KKM1", 4) && fread(&np, 4, 1, f) == 1 && (np = be32(np)) >= 1 && np <= 16;
    k->lo = v3(1e9f, 1e9f, 1e9f); k->hi = v3(-1e9f, -1e9f, -1e9f);
    for (uint32_t p = 0; good && p < np; p++) {
        uint32_t kind, n; float hdr[4];
        if (fread(&kind, 4, 1, f) != 1 || fread(hdr, 4, 4, f) != 4 || fread(&n, 4, 1, f) != 1) { good = 0; break; }
        kind = be32(kind); n = be32(n); be_words(hdr, 4); if (n > 400000 || kind >= K3_PARTS) { good = 0; break; }
        Vertex *v = malloc(sizeof(Vertex) * (n ? n : 1)); if (!v) { good = 0; break; }
        if (fread(v, sizeof(Vertex), n, f) != n) { free(v); good = 0; break; }
        be_words(v, (size_t)n * 12);
        for (uint32_t i = 0; i < n; i++) { float x = v[i].px + hdr[0], y = v[i].py + hdr[1], z = v[i].pz + hdr[2];
            if (x < k->lo.x) k->lo.x = x; if (y < k->lo.y) k->lo.y = y; if (z < k->lo.z) k->lo.z = z; if (x > k->hi.x) k->hi.x = x; if (y > k->hi.y) k->hi.y = y; if (z > k->hi.z) k->hi.z = z; }
        k->m[kind] = n ? gfx_mesh_static(v, n) : NULL; k->piv[kind] = v3(hdr[0], hdr[1], hdr[2]); k->rad[kind] = hdr[3]; free(v); }
    fclose(f);
    if (good && k->m[K3_BODY]) { if (g_kart3d_tex_override) k->tex = g_kart3d_tex_override; else { snprintf(path, sizeof path, "models/%s.png", name); int w, h; k->tex = asset_texture_file(path, 1, &w, &h); } }
    k->ok = good && k->m[K3_BODY] && k->tex;
    k->rig = k->ok && k->m[K3_HEAD];   /* the torso, the arms and the steering wheel are optional: a driver may only turn the head */
    gfx_log("3D kart %s: %s%s", name, k->ok ? "loaded" : (good ? "picture missing" : "model file unreadable"), k->rig ? " (driver moves)" : "");
    if (!k->ok) { for (int i = 0; i < K3_PARTS; i++) { gfx_mesh_free(k->m[i]); k->m[i] = NULL; } }
    return k->ok; }
static void draw_body(const Kart3D *k, const Mat4 *base, float wheel_ang, float steer, float r, float g, float b, float a) {
    if (!k->ok) return; gfx_bind(k->tex);
    gfx_draw_ex(k->m[K3_BODY], base, r, g, b, a, 0.3f, 0, 0, 0);   /* a polished shine on the gold and paint */
    for (int w = K3_FL; w <= K3_BR; w++) { if (!k->m[w]) continue;
        Mat4 m = m4_mul(*base, m4_translate(k->piv[w].x, k->piv[w].y, k->piv[w].z));
        if (w == K3_FL || w == K3_FR) m = m4_mul(m, m4_rotate_y(steer));
        m = m4_mul(m, m4_rotate_x(wheel_ang)); gfx_draw_ex(k->m[w], &m, r, g, b, a, 0.05f, 0, 0, 0); } }
/* ---- the moving driver: every piece is solid; joints turn it about the hips, neck, shoulders and elbows ---- */
static Vec3 v3_add(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static Vec3 v3_mulf(Vec3 a, float f) { return v3(a.x * f, a.y * f, a.z * f); }
static float v3_len(Vec3 a) { return sqrtf(v3_dot(a, a)); }
static Vec3 m4_pt(const Mat4 *m, Vec3 p) { return v3(m->m[0] * p.x + m->m[4] * p.y + m->m[8] * p.z + m->m[12], m->m[1] * p.x + m->m[5] * p.y + m->m[9] * p.z + m->m[13], m->m[2] * p.x + m->m[6] * p.y + m->m[10] * p.z + m->m[14]); }
static Vec3 m4_dir(const Mat4 *m, Vec3 p) { return v3(m->m[0] * p.x + m->m[4] * p.y + m->m[8] * p.z, m->m[1] * p.x + m->m[5] * p.y + m->m[9] * p.z, m->m[2] * p.x + m->m[6] * p.y + m->m[10] * p.z); }
static Mat4 m4_axis(Vec3 a, float ang) {   /* turn by ang about the (unit) direction a */
    float c = cosf(ang), s = sinf(ang), t = 1 - c; Mat4 r = m4_identity();
    r.m[0] = t * a.x * a.x + c;       r.m[4] = t * a.x * a.y - s * a.z; r.m[8] = t * a.x * a.z + s * a.y;
    r.m[1] = t * a.x * a.y + s * a.z; r.m[5] = t * a.y * a.y + c;       r.m[9] = t * a.y * a.z - s * a.x;
    r.m[2] = t * a.x * a.z - s * a.y; r.m[6] = t * a.y * a.z + s * a.x; r.m[10] = t * a.z * a.z + c; return r; }
static Mat4 m4_aim(Vec3 from, Vec3 to) {   /* the smallest turn that points direction 'from' along 'to' */
    from = v3_norm(from); to = v3_norm(to); Vec3 ax = v3_cross(from, to); float sn = v3_len(ax), cs = v3_dot(from, to);
    if (sn < 1e-6f) return m4_identity();
    return m4_axis(v3_mulf(ax, 1.0f / sn), atan2f(sn, cs)); }
static Mat4 about(Vec3 p, Mat4 r) { return m4_mul(m4_translate(p.x, p.y, p.z), m4_mul(r, m4_translate(-p.x, -p.y, -p.z))); }
static Mat4 placed(Vec3 at, Mat4 rot, Vec3 piv) { return m4_mul(m4_translate(at.x, at.y, at.z), m4_mul(rot, m4_translate(-piv.x, -piv.y, -piv.z))); }
static float smooth01(float x) { x = clampf(x, 0, 1); return x * x * (3 - 2 * x); }
/* the place each piece of the driver goes, in the kart's own space; out[] indexed by K3_* */
static void pose_driver(const Kart3D *k, float steer, const K3Pose *p, Mat4 out[K3_PARTS]) {
    static const K3Pose rest; if (!p) p = &rest;
    Vec3 pt = k->piv[K3_TORSO];
    Mat4 rt = m4_identity(); out[K3_TORSO] = m4_identity();   /* no torso piece: the body stays with the kart */
    if (k->m[K3_TORSO]) { rt = m4_mul(m4_rotate_y(p->twist), m4_mul(m4_rotate_z(p->lean), m4_rotate_x(p->bow))); out[K3_TORSO] = placed(v3(pt.x, pt.y + p->bob, pt.z), rt, pt); }
    float lim = k->rad[K3_HEAD] > 0.05f ? k->rad[K3_HEAD] : 1.6f, hs = lim < 1.0f ? lim : 1.0f;   /* some heads may only turn a little (long hair down the back) */
    out[K3_HEAD] = m4_mul(out[K3_TORSO], about(k->piv[K3_HEAD], m4_mul(m4_rotate_y(clampf(p->look_yaw, -lim, lim)), m4_mul(m4_rotate_x(-p->look_pitch * hs), m4_rotate_z(p->tilt * hs)))));
    Vec3 col = v3_norm(k->piv[K3_SWAXIS]); if (v3_len(k->piv[K3_SWAXIS]) < 0.5f) col = v3(0, 0, 1);
    out[K3_SWHEEL] = about(k->piv[K3_SWHEEL], m4_axis(col, -steer * 2.4f));
    for (int sd = 0; sd < 2; sd++) {
        int ua = sd ? K3_UARM_R : K3_UARM_L, fa = sd ? K3_FARM_R : K3_FARM_L, gp = sd ? K3_GRIP_R : K3_GRIP_L; float sg = sd ? -1.0f : 1.0f;
        if (!k->m[fa] || !k->m[K3_SWHEEL]) { out[ua] = out[fa] = out[K3_TORSO]; continue; }
        Vec3 ps = k->piv[ua], pe = k->piv[fa], gr = k->piv[gp];
        if (!k->m[ua]) {   /* no upper-arm piece: the arm turns only where its piece joins the body (elbow, or the shoulder) */
            float L2 = v3_len(v3_sub(gr, pe)); Vec3 E = m4_pt(&out[K3_TORSO], pe), H = m4_pt(&out[K3_SWHEEL], gr);
            float w = smooth01(p->arm_w[sd]), t = p->t;
            if (w > 0 && p->arm[sd] != K3A_WHEEL) { Vec3 o;
                switch (p->arm[sd]) {
                case K3A_WAVE:  o = v3(sg * (0.35f + 0.28f * sinf(t * 10.0f)), 0.92f, 0.12f); break;
                case K3A_UP:    o = v3(sg * 0.15f, 1.0f, 0.0f); break;
                case K3A_PUMP:  { float u = sinf(t * 9.0f); u = u > 0 ? u : 0; o = v3(sg * 0.1f, 0.45f + 0.5f * u, 0.55f - 0.3f * u); } break;
                case K3A_GUARD: o = v3(sg * -0.5f, 0.65f, 0.5f); break;
                default:        o = v3(sg * 0.1f, 0.2f, 1.0f); break; }
                Vec3 Hp = m4_pt(&out[K3_TORSO], v3_add(pe, v3_mulf(o, L2)));
                H = v3_add(v3_mulf(H, 1 - w), v3_mulf(Hp, w)); }
            out[ua] = out[K3_TORSO];
            out[fa] = placed(E, m4_mul(m4_aim(m4_dir(&rt, v3_sub(gr, pe)), v3_sub(H, E)), rt), pe); continue; }
        float L1 = v3_len(v3_sub(pe, ps)), L2 = v3_len(v3_sub(gr, pe)), Lt = L1 + L2;
        Vec3 S = m4_pt(&out[K3_TORSO], ps), H = m4_pt(&out[K3_SWHEEL], gr);
        float w = smooth01(p->arm_w[sd]), t = p->t;
        if (w > 0 && p->arm[sd] != K3A_WHEEL) {   /* where the hand goes instead, measured from the shoulder in the body's own directions */
            Vec3 o;
            switch (p->arm[sd]) {
            case K3A_WAVE:  o = v3(sg * (0.42f + 0.16f * sinf(t * 10.0f)), 0.78f, 0.12f); break;
            case K3A_UP:    o = v3(sg * 0.22f, 0.93f + 0.04f * sinf(t * 9.0f), 0.08f); break;
            case K3A_PUMP:  { float u = sinf(t * 9.0f); u = u > 0 ? u : 0; o = v3(sg * 0.18f, 0.35f + 0.5f * u, 0.38f - 0.15f * u); } break;
            case K3A_GUARD: o = v3(sg * -0.08f, 0.62f, 0.55f); break;
            default:        o = v3(sg * 0.15f, 0.30f, 0.90f); break;   /* pointing ahead */
            }
            Vec3 Hp = m4_pt(&out[K3_TORSO], v3_add(ps, v3_mulf(o, Lt)));
            H = v3_add(v3_mulf(H, 1 - w), v3_mulf(Hp, w)); }
        /* two-bone reach: the elbow bends the way it bends sitting at the wheel */
        Vec3 dv = v3_sub(H, S); float d = v3_len(dv); Vec3 dir = d > 1e-5f ? v3_mulf(dv, 1.0f / d) : v3(0, 0, 1);
        float dmin = fabsf(L1 - L2) + 0.01f, dmax = Lt * 0.995f; d = clampf(d, dmin, dmax);
        Vec3 e0 = v3_sub(pe, ps), d0 = v3_norm(v3_sub(gr, ps)), pole0 = v3_sub(e0, v3_mulf(d0, v3_dot(e0, d0)));
        Vec3 pole = v3_add(m4_dir(&rt, pole0), v3_mulf(v3(sg, -0.2f, -0.6f), 0.25f * L1 * w));
        pole = v3_sub(pole, v3_mulf(dir, v3_dot(pole, dir))); if (v3_len(pole) < 1e-5f) pole = v3(sg, -1, 0); pole = v3_norm(pole);
        float a = (L1 * L1 - L2 * L2 + d * d) / (2 * d), h = L1 * L1 - a * a; h = h > 0 ? sqrtf(h) : 0;
        Vec3 E = v3_add(S, v3_add(v3_mulf(dir, a), v3_mulf(pole, h))), Hh = v3_add(S, v3_mulf(dir, d));
        Mat4 rua = m4_mul(m4_aim(m4_dir(&rt, e0), v3_sub(E, S)), rt);
        out[ua] = placed(S, rua, ps);
        out[fa] = placed(E, m4_mul(m4_aim(m4_dir(&rua, v3_sub(gr, pe)), v3_sub(Hh, E)), rua), pe); } }
static void draw_driver(const Kart3D *k, const Mat4 *base, float steer, const K3Pose *pose, float r, float g, float b, float a, float spec, int unlit) {
    Mat4 pm[K3_PARTS]; pose_driver(k, steer, pose, pm);
    for (int i = K3_TORSO; i <= K3_SWHEEL; i++) { if (!k->m[i]) continue;
        Mat4 m = m4_mul(*base, m4_mul(pm[i], m4_translate(k->piv[i].x, k->piv[i].y, k->piv[i].z)));   /* the piece is stored round its own joint */
        gfx_draw_ex(k->m[i], &m, r, g, b, a, unlit ? 0 : (i == K3_SWHEEL ? 0.3f : spec), unlit, 0, 0); } }
void kart3d_draw(const Kart3D *k, const Mat4 *base, float wheel_ang, float steer, float r, float g, float b, float a) { kart3d_draw_pose(k, base, wheel_ang, steer, NULL, r, g, b, a); }
void kart3d_draw_pose(const Kart3D *k, const Mat4 *base, float wheel_ang, float steer, const K3Pose *pose, float r, float g, float b, float a) {
    draw_body(k, base, wheel_ang, steer, r, g, b, a);
    if (k->ok && k->rig) draw_driver(k, base, steer, pose, r, g, b, a, 0.12f, 0); }
void kart3d_draw_flat(const Kart3D *k, const Mat4 *base, float wheel_ang, float steer, const K3Pose *pose, float r, float g, float b, float a) {
    if (!k->ok) return;
    gfx_draw_ex(k->m[K3_BODY], base, r, g, b, a, 0, 1, 0, 0);
    for (int w = K3_FL; w <= K3_BR; w++) { if (!k->m[w]) continue;
        Mat4 m = m4_mul(*base, m4_translate(k->piv[w].x, k->piv[w].y, k->piv[w].z));
        if (w == K3_FL || w == K3_FR) m = m4_mul(m, m4_rotate_y(steer));
        m = m4_mul(m, m4_rotate_x(wheel_ang)); gfx_draw_ex(k->m[w], &m, r, g, b, a, 0, 1, 0, 0); }
    if (k->rig) draw_driver(k, base, steer, pose, r, g, b, a, 0, 1); }
