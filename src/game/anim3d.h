// anim3d.h - a character with real animations (Goliath): every clip baked into frames by tools/model3d (from a rigged,
// animated model), played back smoothly by blending between frames. content/models/<name>.kka + <name>_anim.png.
#ifndef KK_ANIM3D_H
#define KK_ANIM3D_H
#include "../render/gfx.h"
#include "../render/kkmath.h"
#define A3_MAXCLIPS 12
typedef struct { char name[16]; int nf, loop; float dur; uint8_t *data; } A3Clip;   /* data: nf frames x nv corners x 10 bytes (position 3 x int16 / 4000 m, normal 3 x int8 / 127) */
typedef struct { int ok, nv, nt, nc; float *uv; uint32_t *idx; A3Clip clip[A3_MAXCLIPS]; Texture *tex; Mesh *mesh; Vertex *vb; } Anim3D;
int anim3d_load(Anim3D *a, const char *name);
int anim3d_clip(const Anim3D *a, const char *name);   /* -1 if missing */
/* poses it 't' seconds into clip c (looping clips wrap, the others hold their last frame) and draws it at 'base' */
void anim3d_draw(Anim3D *a, int c, float t, const Mat4 *base, float r, float g, float b, float alpha);
#endif
