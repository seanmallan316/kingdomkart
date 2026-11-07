// kart3d.c - loading and drawing a racer's 3D kart.
#include "kart3d.h"
#include "../render/assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int kart3d_load(Kart3D *k, const char *name) {
    memset(k, 0, sizeof *k); char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "models/%s.kkm", name);
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    char magic[4]; uint32_t np = 0; int good = fread(magic, 1, 4, f) == 4 && !memcmp(magic, "KKM1", 4) && fread(&np, 4, 1, f) == 1 && np >= 1 && np <= 16;
    k->lo = v3(1e9f, 1e9f, 1e9f); k->hi = v3(-1e9f, -1e9f, -1e9f);
    for (uint32_t p = 0; good && p < np; p++) {
        uint32_t kind, n; float hdr[4];
        if (fread(&kind, 4, 1, f) != 1 || fread(hdr, 4, 4, f) != 4 || fread(&n, 4, 1, f) != 1 || n > 400000 || kind >= K3_PARTS) { good = 0; break; }
        Vertex *v = malloc(sizeof(Vertex) * (n ? n : 1)); if (!v) { good = 0; break; }
        if (fread(v, sizeof(Vertex), n, f) != n) { free(v); good = 0; break; }
        for (uint32_t i = 0; i < n; i++) { float x = v[i].px + hdr[0], y = v[i].py + hdr[1], z = v[i].pz + hdr[2];
            if (x < k->lo.x) k->lo.x = x; if (y < k->lo.y) k->lo.y = y; if (z < k->lo.z) k->lo.z = z; if (x > k->hi.x) k->hi.x = x; if (y > k->hi.y) k->hi.y = y; if (z > k->hi.z) k->hi.z = z; }
        k->m[kind] = n ? gfx_mesh_static(v, n) : NULL; k->piv[kind] = v3(hdr[0], hdr[1], hdr[2]); k->rad[kind] = hdr[3]; free(v); }
    fclose(f);
    if (good && k->m[K3_BODY]) { snprintf(path, sizeof path, "models/%s.png", name); int w, h; k->tex = asset_texture_file(path, 1, &w, &h); }
    k->ok = good && k->m[K3_BODY] && k->tex;
    if (!k->ok) { for (int i = 0; i < K3_PARTS; i++) { gfx_mesh_free(k->m[i]); k->m[i] = NULL; } }
    return k->ok; }
void kart3d_draw(const Kart3D *k, const Mat4 *base, float wheel_ang, float steer, float r, float g, float b, float a) {
    if (!k->ok) return; gfx_bind(k->tex);
    gfx_draw_ex(k->m[K3_BODY], base, r, g, b, a, 0.35f, 0, 0, 0);
    for (int w = K3_FL; w <= K3_BR; w++) { if (!k->m[w]) continue;
        Mat4 m = m4_mul(*base, m4_translate(k->piv[w].x, k->piv[w].y, k->piv[w].z));
        if (w == K3_FL || w == K3_FR) m = m4_mul(m, m4_rotate_y(steer));
        m = m4_mul(m, m4_rotate_x(wheel_ang)); gfx_draw_ex(k->m[w], &m, r, g, b, a, 0.25f, 0, 0, 0); } }
