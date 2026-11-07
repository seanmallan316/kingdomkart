// anim3d.c - loading and playing a baked-animation character (see anim3d.h).
#include "anim3d.h"
#include "../render/assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t rd32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }   /* the file is big-endian, read byte by byte so it works on any machine */
static float rdf(const uint8_t *p) { uint32_t u = rd32(p); float f; memcpy(&f, &u, 4); return f; }
static int16_t rd16(const uint8_t *p) { return (int16_t)(((uint16_t)p[0] << 8) | p[1]); }
int anim3d_load(Anim3D *a, const char *name) {
    memset(a, 0, sizeof *a); char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "models/%s.kka", name);
    FILE *f = fopen(path, "rb"); if (!f) { gfx_log("animated %s: no file", name); return 0; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)sz); if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return 0; } fclose(f);
    if (sz < 16 || memcmp(buf, "KKA2", 4)) { free(buf); gfx_log("animated %s: not a KKA2 file", name); return 0; }
    a->nv = (int)rd32(buf + 4); a->nt = (int)rd32(buf + 8); a->nc = (int)rd32(buf + 12); if (a->nc > A3_MAXCLIPS) a->nc = A3_MAXCLIPS;
    size_t off = 16; a->uv = malloc(sizeof(float) * 2 * a->nv); a->idx = malloc(sizeof(uint32_t) * 3 * a->nt);
    for (int i = 0; i < a->nv * 2; i++, off += 4) a->uv[i] = rdf(buf + off);
    for (int i = 0; i < a->nt * 3; i++, off += 4) a->idx[i] = rd32(buf + off);
    for (int c = 0; c < a->nc; c++) { A3Clip *cl = &a->clip[c]; memcpy(cl->name, buf + off, 15); off += 16;
        cl->nf = (int)rd32(buf + off); cl->dur = rdf(buf + off + 4); cl->loop = (int)rd32(buf + off + 8); off += 12;
        size_t n = (size_t)cl->nf * a->nv * 10; if (off + n > (size_t)sz) { a->nc = c; break; }
        cl->data = malloc(n); memcpy(cl->data, buf + off, n); off += n; }
    free(buf);
    snprintf(path, sizeof path, "models/%s_anim.png", name); int w, h; a->tex = asset_texture_file(path, 1, &w, &h);
    a->vb = malloc(sizeof(Vertex) * 3 * a->nt); a->mesh = gfx_mesh_dynamic((uint32_t)(3 * a->nt));
    a->ok = a->nc > 0 && a->tex && a->vb && a->mesh;
    gfx_log("animated %s: %s (%d corners, %d triangles, %d moves)", name, a->ok ? "loaded" : "failed", a->nv, a->nt, a->nc);
    return a->ok; }
int anim3d_clip(const Anim3D *a, const char *name) { for (int c = 0; c < a->nc; c++) if (!strcmp(a->clip[c].name, name)) return c; return -1; }
void anim3d_draw(Anim3D *a, int c, float t, const Mat4 *base, float r, float g, float b, float alpha) {
    if (!a->ok || c < 0 || c >= a->nc) return; const A3Clip *cl = &a->clip[c];
    float fpos = cl->dur > 0 ? t / cl->dur * (cl->loop ? cl->nf : cl->nf - 1) : 0; int i0, i1; float w;
    if (cl->loop) { fpos = fmodf(fpos, (float)cl->nf); if (fpos < 0) fpos += cl->nf; i0 = (int)fpos; i1 = (i0 + 1) % cl->nf; w = fpos - i0; }
    else { if (fpos < 0) fpos = 0; if (fpos > cl->nf - 1) fpos = (float)(cl->nf - 1); i0 = (int)fpos; i1 = i0 + 1 < cl->nf ? i0 + 1 : i0; w = fpos - i0; }
    const uint8_t *f0 = cl->data + (size_t)i0 * a->nv * 10, *f1 = cl->data + (size_t)i1 * a->nv * 10;
    for (int k = 0; k < a->nt * 3; k++) { uint32_t v = a->idx[k]; const uint8_t *p0 = f0 + v * 10, *p1 = f1 + v * 10; Vertex *o = &a->vb[k];
        o->px = (rd16(p0) + (rd16(p1) - rd16(p0)) * w) * (1.0f / 4000); o->py = (rd16(p0 + 2) + (rd16(p1 + 2) - rd16(p0 + 2)) * w) * (1.0f / 4000); o->pz = (rd16(p0 + 4) + (rd16(p1 + 4) - rd16(p0 + 4)) * w) * (1.0f / 4000);
        o->nx = ((int8_t)p0[6] + ((int8_t)p1[6] - (int8_t)p0[6]) * w) * (1.0f / 127); o->ny = ((int8_t)p0[7] + ((int8_t)p1[7] - (int8_t)p0[7]) * w) * (1.0f / 127); o->nz = ((int8_t)p0[8] + ((int8_t)p1[8] - (int8_t)p0[8]) * w) * (1.0f / 127);
        o->u = a->uv[v * 2]; o->v = a->uv[v * 2 + 1]; o->r = o->g = o->b = o->a = 1; }
    gfx_mesh_update(a->mesh, a->vb, (uint32_t)(3 * a->nt)); gfx_bind(a->tex); gfx_draw_ex(a->mesh, base, r, g, b, alpha, 0.25f, 0, 0, 0); }
