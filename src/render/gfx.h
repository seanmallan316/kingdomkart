// gfx.h - the small rendering interface the game uses. Only gfx.c knows about GX2.
#ifndef KK_GFX_H
#define KK_GFX_H
#include <stdint.h>
#include "kkmath.h"

typedef struct { float px, py, pz, nx, ny, nz, u, v, r, g, b, a; } Vertex;   /* 48 bytes */
typedef struct Mesh Mesh;
typedef struct Texture Texture;

enum { GFX_OK = 0, GFX_NO_COMPILER = 1, GFX_SHADER_FAILED = 2 };
enum { ST_DEPTH = 1, ST_DEPTH_WRITE = 2, ST_BLEND = 4, ST_CULL = 8, ST_ADD = 16, ST_LESS = 32 };   /* ST_LESS: a spot already drawn at this depth is not drawn again (shadows don't double up) */
#define ST_SOLID (ST_DEPTH | ST_DEPTH_WRITE | ST_CULL)
#define ST_CUTOUT (ST_DEPTH | ST_DEPTH_WRITE | ST_BLEND)
#define ST_TRANSPARENT (ST_DEPTH | ST_BLEND)
#define ST_HUD (ST_BLEND)

int  gfx_init(void);                         /* returns GFX_OK or an error code; the game shows a colored screen for errors */
void gfx_shutdown(void);
void gfx_log(const char *fmt, ...);

Mesh *gfx_mesh_static(const Vertex *v, uint32_t n);
Mesh *gfx_mesh_dynamic(uint32_t capacity);
void gfx_mesh_update(Mesh *m, const Vertex *v, uint32_t n);   /* at most once per frame per mesh */
void gfx_mesh_free(Mesh *m);
Texture *gfx_texture_rgba(const uint32_t *pixels, int w, int h, int repeat, int nearest);   /* pixels are 0xRRGGBBAA; mipmapped */
Texture *gfx_texture_rgba_ex(const uint32_t *pixels, int w, int h, int repeat, int nearest, int mipmaps);
void gfx_texture_free(Texture *t);
Texture *gfx_texture_stream(int w, int h);                                      /* for pictures rewritten every frame (video) */
void gfx_texture_stream_put(Texture *t, const uint32_t *pixels, int w, int h);
void *gfx_texture_stream_pixels(Texture *t, int *pitch_px);   /* the picture's memory, to write straight into (any thread) */
void gfx_texture_stream_flush_cpu(Texture *t);                 /* after writing it (any thread) */
void gfx_texture_stream_gpu(Texture *t);                       /* before drawing with it (drawing thread) */

void gfx_frame_begin(void);
void gfx_view_begin(int drc, float r, float g, float b);      /* drc = 0 for the TV, 1 for the GamePad */
void gfx_view_end(int drc);
void gfx_frame_end(void);
float gfx_aspect(int drc);
void gfx_viewport(int drc, float fx, float fy, float fw, float fh);   /* draw into part of the screen: fractions, origin top-left (split screen); gfx_view_begin resets it */

void gfx_camera(const Mat4 *viewproj, Vec3 lightdir, float ambient, float fog_r, float fog_g, float fog_b, float fog_start, float fog_end);
void gfx_clear_depth(int drc);
/* sun shadows: between gfx_frame_begin and the first gfx_view_begin, gfx_shadow_begin(sun's view) then draw what casts shadows,
   then gfx_shadow_end. gfx_shadow_use(1) before a scene's gfx_camera lets that scene receive them; gfx_shadow_none() for a frame with none */
int gfx_shadow_begin(const Mat4 *lightvp);
void gfx_shadow_end(void);
void gfx_shadow_use(int on);
void gfx_shadow_none(void);
Texture *gfx_shadow_texture(void);                                 /* fresh depth partway through a screen, for a 3D scene drawn over another */
void gfx_set_time(float t);                                    /* the clock the wind runs on (trees, fronds, flags sway) */
void gfx_camera_2d(void);                                     /* clip-space drawing for the HUD, no lighting or fog */
void gfx_camera_2d_virtual(float vw, float vh);               /* the same, with the 2D helpers' 1280 x 720 layout mapped onto a view of vw x vh pixels */
void gfx_bind(Texture *t);
void gfx_state(int flags);
void gfx_light(Vec3 sun, Vec3 sky, Vec3 ground, Vec3 eye);   /* sun color, ambient from above and from below, camera position; call before gfx_camera */
void gfx_draw(const Mesh *m, const Mat4 *model, float r, float g, float b, float a);
void gfx_draw_ex(const Mesh *m, const Mat4 *model, float r, float g, float b, float a, float specular, int unlit, float scroll_u, float scroll_v);
#endif
