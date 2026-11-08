// batch.h - builds vertex lists in memory (quads, sprites, text) that are uploaded to a dynamic mesh once per frame.
#ifndef KK_BATCH_H
#define KK_BATCH_H
#include "gfx.h"
#include <string.h>

typedef struct { Vertex *v; uint32_t n, cap; } Batch;

static inline void batch_init(Batch *b, Vertex *storage, uint32_t cap) { b->v = storage; b->n = 0; b->cap = cap; }
static inline void batch_clear(Batch *b) { b->n = 0; }

/* a quad with center c, half-axes ax and ay, normal n, texture rectangle (u0,v0)-(u1,v1), color */
static inline void batch_quad(Batch *b, Vec3 c, Vec3 ax, Vec3 ay, Vec3 n, float u0, float v0, float u1, float v1, float r, float g, float bl, float a) {
    if (b->n + 6 > b->cap) return;
    Vec3 p[4] = { v3(c.x - ax.x - ay.x, c.y - ax.y - ay.y, c.z - ax.z - ay.z), v3(c.x + ax.x - ay.x, c.y + ax.y - ay.y, c.z + ax.z - ay.z),
                  v3(c.x + ax.x + ay.x, c.y + ax.y + ay.y, c.z + ax.z + ay.z), v3(c.x - ax.x + ay.x, c.y - ax.y + ay.y, c.z - ax.z + ay.z) };
    float uv[4][2] = {{u0, v1}, {u1, v1}, {u1, v0}, {u0, v0}};
    static const int idx[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; i++) { int k = idx[i]; Vertex q = {p[k].x, p[k].y, p[k].z, n.x, n.y, n.z, uv[k][0], uv[k][1], r, g, bl, a}; b->v[b->n++] = q; }
}
/* a quad given its four corners in order (counter-clockwise seen from the front) */
static inline void batch_quad4(Batch *b, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 n, float u0, float v0, float u1, float v1, float r, float g, float bl, float a) {
    if (b->n + 6 > b->cap) return;
    Vec3 p[4] = {p0, p1, p2, p3};
    float uv[4][2] = {{u0, v1}, {u1, v1}, {u1, v0}, {u0, v0}};
    static const int idx[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; i++) { int k = idx[i]; Vertex q = {p[k].x, p[k].y, p[k].z, n.x, n.y, n.z, uv[k][0], uv[k][1], r, g, bl, a}; b->v[b->n++] = q; }
}
/* an axis-aligned box (for the placeholder kart model) */
static inline void batch_box(Batch *b, Vec3 c, Vec3 h, float r, float g, float bl) {
    batch_quad(b, v3(c.x, c.y, c.z + h.z), v3(h.x, 0, 0), v3(0, h.y, 0), v3(0, 0, 1), 0, 0, 1, 1, r, g, bl, 1);
    batch_quad(b, v3(c.x, c.y, c.z - h.z), v3(-h.x, 0, 0), v3(0, h.y, 0), v3(0, 0, -1), 0, 0, 1, 1, r, g, bl, 1);
    batch_quad(b, v3(c.x + h.x, c.y, c.z), v3(0, 0, -h.z), v3(0, h.y, 0), v3(1, 0, 0), 0, 0, 1, 1, r, g, bl, 1);
    batch_quad(b, v3(c.x - h.x, c.y, c.z), v3(0, 0, h.z), v3(0, h.y, 0), v3(-1, 0, 0), 0, 0, 1, 1, r, g, bl, 1);
    batch_quad(b, v3(c.x, c.y + h.y, c.z), v3(h.x, 0, 0), v3(0, 0, -h.z), v3(0, 1, 0), 0, 0, 1, 1, r, g, bl, 1);
    batch_quad(b, v3(c.x, c.y - h.y, c.z), v3(h.x, 0, 0), v3(0, 0, h.z), v3(0, -1, 0), 0, 0, 1, 1, r, g, bl, 1);
}

/* a wheel: an eight-sided cylinder along the x axis, with a lighter hub on each side */
static inline void batch_wheel(Batch *b, Vec3 c, float hw, float rad, float r, float g, float bl) {
    for (int i = 0; i < 8; i++) {
        float a0 = i * 6.2831853f / 8, a1 = (i + 1) * 6.2831853f / 8;
        Vec3 p0 = v3(c.x - hw, c.y + cosf(a0) * rad, c.z + sinf(a0) * rad), p1 = v3(c.x + hw, c.y + cosf(a0) * rad, c.z + sinf(a0) * rad);
        Vec3 p2 = v3(c.x + hw, c.y + cosf(a1) * rad, c.z + sinf(a1) * rad), p3 = v3(c.x - hw, c.y + cosf(a1) * rad, c.z + sinf(a1) * rad);
        Vec3 n = v3(0, cosf((a0 + a1) * 0.5f), sinf((a0 + a1) * 0.5f));
        batch_quad4(b, p0, p3, p2, p1, n, 0, 0, 1, 1, r, g, bl, 1);
        /* side caps as fans around the center, hub a little lighter */
        Vec3 cl = v3(c.x - hw, c.y, c.z), cr = v3(c.x + hw, c.y, c.z);
        if (b->n + 12 > b->cap) return;
        Vertex q0 = {cl.x, cl.y, cl.z, -1, 0, 0, 0, 0, r + 0.3f, g + 0.3f, bl + 0.3f, 1}, q1 = {p3.x, p3.y, p3.z, -1, 0, 0, 0, 0, r, g, bl, 1}, q2 = {p0.x, p0.y, p0.z, -1, 0, 0, 0, 0, r, g, bl, 1};
        b->v[b->n++] = q0; b->v[b->n++] = q1; b->v[b->n++] = q2;
        Vertex q3 = {cr.x, cr.y, cr.z, 1, 0, 0, 0, 0, r + 0.3f, g + 0.3f, bl + 0.3f, 1}, q4 = {p1.x, p1.y, p1.z, 1, 0, 0, 0, 0, r, g, bl, 1}, q5 = {p2.x, p2.y, p2.z, 1, 0, 0, 0, 0, r, g, bl, 1};
        b->v[b->n++] = q3; b->v[b->n++] = q4; b->v[b->n++] = q5;
    }
}

/* ---- 2D helpers: virtual screen of 1280 x 720, origin top-left, mapped to clip space */
static inline Vec3 scr(float x, float y) { return v3(x / 640.0f - 1.0f, 1.0f - y / 360.0f, 0); }
static inline void batch_rect_uv(Batch *b, float x, float y, float w, float h, float u0, float v0, float u1, float v1, float r, float g, float bl, float a) {
    Vec3 c = scr(x + w * 0.5f, y + h * 0.5f);
    batch_quad(b, c, v3(w / 1280.0f, 0, 0), v3(0, h / 720.0f, 0), v3(0, 0, 1), u0, v0, u1, v1, r, g, bl, a);
}
/* a solid rectangle; uses the white block kept in the font texture so text and boxes share one texture */
#define WHITE_U ((120.0f + 3.0f) / 128.0f)
#define WHITE_V ((24.0f + 3.0f) / 32.0f)
static inline void batch_rect(Batch *b, float x, float y, float w, float h, float r, float g, float bl, float a) {
    batch_rect_uv(b, x, y, w, h, WHITE_U, WHITE_V, WHITE_U + 0.001f, WHITE_V + 0.001f, r, g, bl, a);
}

/* ---- bitmap font: 5x7 glyphs in an atlas of 8x8 cells, 16 per row */
#define FONT_CHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?:-'/+()"
static const unsigned char FONT_GLYPHS[][7] = {
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},{0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},{0x11,0x19,0x15,0x13,0x11,0x11,0x11},{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},{0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},{0x11,0x11,0x11,0x15,0x15,0x15,0x0A},{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},{0x00,0x00,0x00,0x00,0x0C,0x04,0x08},{0x04,0x04,0x04,0x04,0x04,0x00,0x04},
    {0x0E,0x11,0x01,0x02,0x04,0x00,0x04},{0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00},{0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
    {0x04,0x04,0x08,0x00,0x00,0x00,0x00},{0x01,0x02,0x02,0x04,0x08,0x08,0x10},{0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02},{0x08,0x04,0x02,0x02,0x02,0x04,0x08},
};
#define FONT_TEX_W 128
#define FONT_TEX_H 32
static inline void font_build_texture(uint32_t *px) {        /* px has FONT_TEX_W*FONT_TEX_H entries */
    memset(px, 0, FONT_TEX_W * FONT_TEX_H * 4);
    int n = (int)(sizeof FONT_GLYPHS / sizeof FONT_GLYPHS[0]);
    for (int i = 0; i < n; i++) {
        int cx = (i % 16) * 8, cy = (i / 16) * 8;
        for (int row = 0; row < 7; row++) for (int col = 0; col < 5; col++)
            if (FONT_GLYPHS[i][row] & (1 << (4 - col))) px[(cy + row) * FONT_TEX_W + cx + col] = 0xFFFFFFFFu;
    }
    for (int y = 24; y < 32; y++) for (int x = 120; x < 128; x++) px[y * FONT_TEX_W + x] = 0xFFFFFFFFu;   /* solid white block for rectangles */
}
#include "font_hd.h"
static int g_font_hd = 0;      /* 1 when content/img/font_hd.png is loaded and bound as the text texture */
/* draws text in the virtual 1280x720 space; size = pixel height of a capital letter; returns width used */
/* gold: -1 = decide by the colour (big text in gold uses the gold letters), 0 = plain letters tinted, 1 = gold letters tinted */
static inline float batch_text_g(Batch *b, const char *s, float x, float y, float size, float r, float g, float bl, float a, int gold);
static inline float batch_text(Batch *b, const char *s, float x, float y, float size, float r, float g, float bl, float a) { return batch_text_g(b, s, x, y, size, r, g, bl, a, -1); }
static inline float batch_text_g(Batch *b, const char *s, float x, float y, float size, float r, float g, float bl, float a, int gold) {
    float sc = size / 7.0f, x0 = x;
    if (g_font_hd) {
        float hs = size / 50.0f;     /* the HD glyphs have 50-pixel capitals in an 80-pixel cell, cap tops 10 pixels down */
        if (gold < 0) { gold = size >= 34 && r > 0.9f && g > 0.7f && g < 0.95f && bl < 0.5f; if (gold) { r = g = bl = 1; } }   /* big text asked for in gold uses the gold letters (Sean's title style) */
        for (; *s; s++) {
            char c = *s; if (c >= 'a' && c <= 'z') c = (char)(c - 32);
            if (c == ' ') { x += 24 * hs; continue; }
            const char *p = strchr(FONT_CHARS, c); if (!p) continue;
            int i = (int)(p - FONT_CHARS); float u0 = (float)(i % FONT_HD_COLS) * FONT_HD_CW / FONT_HD_W, v0 = ((float)(i / FONT_HD_COLS) * FONT_HD_CH + (gold ? FONT_HD_GOLD_Y : 0)) / FONT_HD_H;
            batch_rect_uv(b, x, y - 10 * hs, FONT_HD_CW * hs, FONT_HD_CH * hs, u0, v0, u0 + (float)FONT_HD_CW / FONT_HD_W, v0 + (float)FONT_HD_CH / FONT_HD_H, r, g, bl, a);
            x += FONT_HD_ADV[i] * hs;
        }
        return x - x0;
    }
    for (; *s; s++) {
        char c = *s; if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (c == ' ') { x += 6 * sc; continue; }
        const char *p = strchr(FONT_CHARS, c); if (!p) continue;
        int i = (int)(p - FONT_CHARS); float u0 = (float)(i % 16) * 8.0f / FONT_TEX_W, v0 = (float)(i / 16) * 8.0f / FONT_TEX_H;
        batch_rect_uv(b, x, y, 5 * sc, 7 * sc, u0, v0, u0 + 5.0f / FONT_TEX_W, v0 + 7.0f / FONT_TEX_H, r, g, bl, a);
        x += 6 * sc;
    }
    return x - x0;
}
static inline float text_width(const char *s, float size) {
    if (g_font_hd) { float hs = size / 50.0f, w = 0; for (; *s; s++) { char c = *s; if (c >= 'a' && c <= 'z') c = (char)(c - 32); if (c == ' ') { w += 24 * hs; continue; } const char *p = strchr(FONT_CHARS, c); if (p) w += FONT_HD_ADV[p - FONT_CHARS] * hs; } return w - 6 * hs; }
    return (float)strlen(s) * 6.0f * (size / 7.0f) - size / 7.0f;
}
#endif
