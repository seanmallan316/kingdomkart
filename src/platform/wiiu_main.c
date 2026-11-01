// Kingdom Kart: Road to Glory - Milestone 1 "technical foundation" build for the Wii U.
//
// What this proves on real hardware, in one small program:
//   1. The Wii U's native 3D graphics (GX2) work from our code: a lit, textured, depth-tested scene on the TV and GamePad.
//   2. The CafeGLSL runtime shader compiler loads from sd:/wiiu/libs/glslcompiler.rpl and compiles our shaders.
//   3. GamePad and Pro Controller input arrive (the left stick drives the cube around, A/B/ZL/ZR change it).
//   4. Files packed inside the .wuhb can be read (content/hello.txt).
//   5. How long a frame takes (the bar at the bottom of the screen: green = at or under 60 FPS budget).
//   6. A log is written to sd:/wiiu/kingdomkart_log.txt so problems can be reported without any tools.
//
// Screen colors tell you what happened if something goes wrong:
//   sky blue background + spinning cube  = everything works
//   solid ORANGE                         = glslcompiler.rpl was not found in sd:/wiiu/libs/
//   solid RED                            = the shader compiler was found but our shaders failed to compile (see the log)
#include <coreinit/time.h>
#include <coreinit/systeminfo.h>
#include <coreinit/memdefaultheap.h>
#include <gx2/clear.h>
#include <gx2/draw.h>
#include <gx2/enum.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <gx2/state.h>
#include <gx2/surface.h>
#include <gx2/texture.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>
#include <gx2r/resource.h>
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <vpad/input.h>
#include <whb/gfx.h>
#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_udp.h>
#include <whb/proc.h>
#include <whb/sdcard.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cafeglsl.h"
#include "../render/kkmath.h"

#define KK_VERSION "M1-0.1"

// ------------------------------------------------------------------ logging (UDP + SD card file)
static FILE *g_logfile = NULL;
static void LOG(const char *fmt, ...) {
    char buf[512]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    WHBLogPrintf("%s", buf);
    if (g_logfile) { fprintf(g_logfile, "%s\n", buf); fflush(g_logfile); }
}

// ------------------------------------------------------------------ shaders (GLSL source compiled on the console by CafeGLSL)
static const char *VS_SRC =
    "#version 450\n"
    "layout(location = 0) in vec3 in_position;\n"
    "layout(location = 1) in vec3 in_normal;\n"
    "layout(location = 2) in vec2 in_texcoord;\n"
    "layout(location = 3) in vec4 in_color;\n"
    "layout(binding = 0) uniform Frame { mat4 u_viewproj; vec4 u_lightdir; };\n"
    "layout(binding = 1) uniform Object { mat4 u_model; vec4 u_tint; };\n"
    "layout(location = 0) out vec2 v_texcoord;\n"
    "layout(location = 1) out vec4 v_color;\n"
    "layout(location = 2) out float v_light;\n"
    "void main() {\n"
    "  vec4 wp = u_model * vec4(in_position, 1.0);\n"
    "  vec3 n = normalize((u_model * vec4(in_normal, 0.0)).xyz);\n"
    "  v_light = 0.35 + 0.65 * max(dot(n, normalize(u_lightdir.xyz)), 0.0);\n"
    "  v_texcoord = in_texcoord;\n"
    "  v_color = in_color * u_tint;\n"
    "  gl_Position = u_viewproj * wp;\n"
    "}\n";
static const char *PS_SRC =
    "#version 450\n"
    "layout(location = 0) in vec2 v_texcoord;\n"
    "layout(location = 1) in vec4 v_color;\n"
    "layout(location = 2) in float v_light;\n"
    "layout(binding = 0) uniform sampler2D u_tex;\n"
    "layout(location = 0) out vec4 out_color;\n"
    "void main() {\n"
    "  vec4 t = texture(u_tex, v_texcoord);\n"
    "  out_color = vec4(t.rgb * v_color.rgb * v_light, t.a * v_color.a);\n"
    "}\n";

// ------------------------------------------------------------------ uniform blocks
// The GPU reads uniform data little-endian while the CPU is big-endian, so every 32-bit word is swapped on upload.
typedef struct { float viewproj[16]; float lightdir[4]; } FrameBlock;
typedef struct { float model[16]; float tint[4]; } ObjectBlock;
#define UB_ALIGN 256
#define UB_SLOTS 64                    /* per frame, double-buffered: up to 64 objects per frame */
typedef struct { uint8_t *mem; uint32_t stride; int next; } UniformPool;
static UniformPool g_pool[2]; static int g_pool_cur = 0;
static void pool_init(UniformPool *p, uint32_t blocksize) {
    p->stride = (blocksize + UB_ALIGN - 1) & ~(UB_ALIGN - 1);
    p->mem = memalign(UB_ALIGN, p->stride * UB_SLOTS); memset(p->mem, 0, p->stride * UB_SLOTS); p->next = 0;
}
static void *pool_upload(UniformPool *p, const void *src, uint32_t bytes) {
    if (p->next >= UB_SLOTS) p->next = 0;
    uint8_t *dst = p->mem + p->next++ * p->stride;
    const uint32_t *s = src; uint32_t *d = (uint32_t *)dst;
    for (uint32_t i = 0; i < bytes / 4; i++) { uint32_t v = s[i]; d[i] = (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24); }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK, dst, p->stride);
    return dst;
}
static uint32_t block_location(const GX2VertexShader *vs, const char *name, uint32_t fallback) {
    for (uint32_t i = 0; i < vs->uniformBlockCount; i++)
        if (vs->uniformBlocks[i].name && strcmp(vs->uniformBlocks[i].name, name) == 0) return vs->uniformBlocks[i].offset;
    return fallback;
}

// ------------------------------------------------------------------ meshes
typedef struct { float px, py, pz, nx, ny, nz, u, v, r, g, b, a; } Vertex;   /* 48 bytes */
typedef struct { GX2RBuffer buf; uint32_t count; } Mesh;
static int mesh_create(Mesh *m, const Vertex *v, uint32_t count) {
    memset(m, 0, sizeof *m);
    m->buf.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_READ | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ;
    m->buf.elemSize = sizeof(Vertex); m->buf.elemCount = count; m->count = count;
    if (!GX2RCreateBuffer(&m->buf)) return 0;
    void *p = GX2RLockBufferEx(&m->buf, GX2R_RESOURCE_BIND_NONE);
    if (!p) return 0;
    memcpy(p, v, sizeof(Vertex) * count);
    GX2RUnlockBufferEx(&m->buf, GX2R_RESOURCE_BIND_NONE);
    return 1;
}
static void quad_face(Vertex *o, Vec3 c, Vec3 ax, Vec3 ay, Vec3 n, float r, float g, float b, float uvscale) {
    Vec3 p[4] = { v3(c.x - ax.x - ay.x, c.y - ax.y - ay.y, c.z - ax.z - ay.z), v3(c.x + ax.x - ay.x, c.y + ax.y - ay.y, c.z + ax.z - ay.z),
                  v3(c.x + ax.x + ay.x, c.y + ax.y + ay.y, c.z + ax.z + ay.z), v3(c.x - ax.x + ay.x, c.y - ax.y + ay.y, c.z - ax.z + ay.z) };
    float uv[4][2] = {{0, uvscale}, {uvscale, uvscale}, {uvscale, 0}, {0, 0}};
    int idx[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; i++) { int k = idx[i]; Vertex q = {p[k].x, p[k].y, p[k].z, n.x, n.y, n.z, uv[k][0], uv[k][1], r, g, b, 1}; o[i] = q; }
}
static void build_cube(Vertex *v) {   /* 36 vertices, counter-clockwise faces seen from outside */
    quad_face(v + 0,  v3(0, 0, 1),  v3(1, 0, 0),  v3(0, 1, 0),  v3(0, 0, 1),  1.0f, 0.85f, 0.3f, 1);     /* front  gold   */
    quad_face(v + 6,  v3(0, 0, -1), v3(-1, 0, 0), v3(0, 1, 0),  v3(0, 0, -1), 1.0f, 0.85f, 0.3f, 1);     /* back   gold   */
    quad_face(v + 12, v3(1, 0, 0),  v3(0, 0, -1), v3(0, 1, 0),  v3(1, 0, 0),  0.3f, 0.55f, 1.0f, 1);     /* right  blue   */
    quad_face(v + 18, v3(-1, 0, 0), v3(0, 0, 1),  v3(0, 1, 0),  v3(-1, 0, 0), 0.3f, 0.55f, 1.0f, 1);     /* left   blue   */
    quad_face(v + 24, v3(0, 1, 0),  v3(1, 0, 0),  v3(0, 0, -1), v3(0, 1, 0),  1.0f, 1.0f, 1.0f, 1);      /* top    white  */
    quad_face(v + 30, v3(0, -1, 0), v3(1, 0, 0),  v3(0, 0, 1),  v3(0, -1, 0), 0.6f, 0.6f, 0.6f, 1);      /* bottom grey   */
}

// ------------------------------------------------------------------ texture (procedural: blue and gold checker with a white border)
static GX2Texture g_tex; static GX2Sampler g_sampler;
static int texture_create(void) {
    const int W = 256, H = 256;
    uint32_t *pixels = malloc((size_t)W * H * 4);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int cx = x / 32, cy = y / 32, edge = (x % 32 < 2 || y % 32 < 2);
        uint8_t r, g, b;
        if (edge) { r = g = b = 240; }
        else if ((cx + cy) & 1) { r = 245; g = 196; b = 66; }      /* gold */
        else { r = 52; g = 120; b = 230; }                          /* blue */
        pixels[y * W + x] = ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | 0xFF;   /* R8G8B8A8, big-endian memory */
    }
    memset(&g_tex, 0, sizeof g_tex);
    g_tex.surface.use = GX2_SURFACE_USE_TEXTURE; g_tex.surface.width = W; g_tex.surface.height = H; g_tex.surface.depth = 1;
    g_tex.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D; g_tex.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    g_tex.surface.tileMode = GX2_TILE_MODE_DEFAULT; g_tex.surface.aa = GX2_AA_MODE1X; g_tex.surface.mipLevels = 1;
    g_tex.viewNumSlices = 1; g_tex.viewNumMips = 1; g_tex.compMap = 0x00010203;
    GX2CalcSurfaceSizeAndAlignment(&g_tex.surface);
    GX2InitTextureRegs(&g_tex);
    g_tex.surface.image = memalign(g_tex.surface.alignment, g_tex.surface.imageSize);
    if (!g_tex.surface.image) { free(pixels); return 0; }
    // upload through a linear surface, then let the GPU convert to its tiled layout
    GX2Surface lin = g_tex.surface; lin.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    GX2CalcSurfaceSizeAndAlignment(&lin);
    lin.image = memalign(lin.alignment, lin.imageSize);
    if (!lin.image) { free(pixels); return 0; }
    for (int y = 0; y < H; y++) memcpy((uint8_t *)lin.image + (size_t)y * lin.pitch * 4, pixels + (size_t)y * W, (size_t)W * 4);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, lin.image, lin.imageSize);
    GX2CopySurface(&lin, 0, 0, &g_tex.surface, 0, 0);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, g_tex.surface.image, g_tex.surface.imageSize);
    GX2DrawDone();
    free(lin.image); free(pixels);
    GX2InitSampler(&g_sampler, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_XY_FILTER_MODE_LINEAR);
    return 1;
}

// ------------------------------------------------------------------ input (GamePad first, Pro Controller as a fallback)
typedef struct { float sx, sy; int a, b, zl, zr, plus; int source; } Pad;
static void read_pad(Pad *p) {
    memset(p, 0, sizeof *p);
    VPADStatus vs; VPADReadError ve = VPAD_READ_SUCCESS;
    if (VPADRead(VPAD_CHAN_0, &vs, 1, &ve) > 0 && ve == VPAD_READ_SUCCESS) {
        p->sx = vs.leftStick.x; p->sy = vs.leftStick.y;
        p->a = !!(vs.hold & VPAD_BUTTON_A); p->b = !!(vs.hold & VPAD_BUTTON_B);
        p->zl = !!(vs.hold & VPAD_BUTTON_ZL); p->zr = !!(vs.hold & VPAD_BUTTON_ZR); p->plus = !!(vs.trigger & VPAD_BUTTON_PLUS);
        p->source = 1;
    }
    KPADStatus ks; KPADError ke = KPAD_ERROR_OK;
    if (KPADReadEx(WPAD_CHAN_0, &ks, 1, &ke) > 0 && ke == KPAD_ERROR_OK && ks.extensionType == WPAD_EXT_PRO_CONTROLLER) {
        if (fabsf(ks.pro.leftStick.x) > 0.1f || fabsf(ks.pro.leftStick.y) > 0.1f) { p->sx = ks.pro.leftStick.x; p->sy = ks.pro.leftStick.y; }
        p->a |= !!(ks.pro.hold & WPAD_PRO_BUTTON_A); p->b |= !!(ks.pro.hold & WPAD_PRO_BUTTON_B);
        p->zl |= !!(ks.pro.hold & WPAD_PRO_TRIGGER_ZL); p->zr |= !!(ks.pro.hold & WPAD_PRO_TRIGGER_ZR); p->plus |= !!(ks.pro.trigger & WPAD_PRO_BUTTON_PLUS);
        p->source |= 2;
    }
}

// ------------------------------------------------------------------ scene
static WHBGfxShaderGroup g_shader; static int g_shader_ok = 0;
static Mesh g_cube, g_ground, g_quad;
static uint32_t g_loc_frame = 0, g_loc_object = 1;

static void draw_mesh(const Mesh *m, const Mat4 *model, float tr, float tg, float tb, void *framebuf) {
    ObjectBlock ob; memcpy(ob.model, model->m, sizeof ob.model); ob.tint[0] = tr; ob.tint[1] = tg; ob.tint[2] = tb; ob.tint[3] = 1;
    void *obuf = pool_upload(&g_pool[g_pool_cur], &ob, sizeof ob);
    GX2SetVertexUniformBlock(g_loc_frame, sizeof(FrameBlock), framebuf);
    GX2SetVertexUniformBlock(g_loc_object, sizeof(ObjectBlock), obuf);
    GX2RSetAttributeBuffer((GX2RBuffer *)&m->buf, 0, sizeof(Vertex), 0);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, m->count, 0, 1);
}

typedef struct { float cx, cz, heading, spin, tilt; float frame_ms; int pad_source; } World;

static void render_view(const World *w, float aspect) {
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, TRUE);
    GX2SetDepthOnlyControl(TRUE, TRUE, GX2_COMPARE_FUNC_LESS);
    GX2SetFetchShader(&g_shader.fetchShader);
    GX2SetVertexShader(g_shader.vertexShader);
    GX2SetPixelShader(g_shader.pixelShader);
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
    GX2SetPixelTexture(&g_tex, g_shader.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&g_sampler, g_shader.pixelShader->samplerVars[0].location);

    // 3D: chase camera behind the cube, like the kart camera will be
    Vec3 target = v3(w->cx, 0.8f, w->cz);
    Vec3 eye = v3(w->cx - sinf(w->heading) * 7.0f, 4.0f, w->cz - cosf(w->heading) * 7.0f);
    Mat4 view = m4_look_at(eye, target, v3(0, 1, 0));
    Mat4 proj = m4_perspective(60.0f * 3.14159265f / 180.0f, aspect, 0.1f, 200.0f);
    FrameBlock fb; Mat4 vp = m4_mul(proj, view); memcpy(fb.viewproj, vp.m, sizeof fb.viewproj);
    fb.lightdir[0] = 0.4f; fb.lightdir[1] = 1.0f; fb.lightdir[2] = 0.6f; fb.lightdir[3] = 0;
    void *fbuf = pool_upload(&g_pool[g_pool_cur], &fb, sizeof fb);

    Mat4 ground = m4_identity();
    draw_mesh(&g_ground, &ground, 0.55f, 0.8f, 0.45f, fbuf);
    Mat4 model = m4_mul(m4_translate(w->cx, 1.0f, w->cz), m4_mul(m4_rotate_y(w->heading + w->spin), m4_rotate_x(w->tilt)));
    draw_mesh(&g_cube, &model, 1, 1, 1, fbuf);
    // a few fixed pillars so movement is visible
    for (int i = 0; i < 8; i++) {
        float a = (float)i * 0.785f; Mat4 pm = m4_mul(m4_translate(cosf(a) * 14.0f, 1.5f, sinf(a) * 14.0f), m4_scale(0.6f, 1.5f, 0.6f));
        draw_mesh(&g_cube, &pm, 0.9f, 0.9f, 0.9f, fbuf);
    }

    // 2D overlay: frame-time bar at the bottom (full width = 33 ms, green at or under 16.7 ms)
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    FrameBlock f2; Mat4 id = m4_identity(); memcpy(f2.viewproj, id.m, sizeof f2.viewproj);
    f2.lightdir[0] = 0; f2.lightdir[1] = 0; f2.lightdir[2] = 1; f2.lightdir[3] = 0;
    void *f2buf = pool_upload(&g_pool[g_pool_cur], &f2, sizeof f2);
    float frac = w->frame_ms / 33.3f; if (frac > 1) frac = 1; if (frac < 0.02f) frac = 0.02f;
    Mat4 bar = m4_mul(m4_translate(-1.0f + frac * 0.9f, -0.93f, 0), m4_scale(frac * 0.9f, 0.03f, 1));
    int good = w->frame_ms <= 16.8f;
    draw_mesh(&g_quad, &bar, good ? 0.2f : 1.0f, good ? 1.0f : 0.2f, 0.2f, f2buf);
    // budget mark at 16.7 ms
    Mat4 mark = m4_mul(m4_translate(-1.0f + 0.9f * (16.7f / 33.3f) * 2.0f, -0.93f, 0), m4_scale(0.004f, 0.05f, 1));
    draw_mesh(&g_quad, &mark, 1, 1, 1, f2buf);
    // controller indicator: small square top-left, blue = GamePad, gold = Pro Controller, grey = none
    Mat4 ind = m4_mul(m4_translate(-0.95f, 0.92f, 0), m4_scale(0.025f, 0.045f, 1));
    float ir = 0.5f, ig = 0.5f, ib = 0.5f;
    if (w->pad_source & 2) { ir = 1.0f; ig = 0.8f; ib = 0.2f; } else if (w->pad_source & 1) { ir = 0.2f; ig = 0.5f; ib = 1.0f; }
    draw_mesh(&g_quad, &ind, ir, ig, ib, f2buf);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    WHBLogCafeInit(); WHBLogUdpInit();
    WHBProcInit();
    if (WHBMountSdCard()) {
        char path[256]; snprintf(path, sizeof path, "%s/wiiu/kingdomkart_log.txt", WHBGetSdCardMountPath());
        g_logfile = fopen(path, "w");
    }
    LOG("Kingdom Kart %s starting", KK_VERSION);
    WHBGfxInit();
    KPADInit(); WPADEnableURCC(TRUE);

    // 1. is the content inside the .wuhb readable?
    { FILE *f = fopen("/vol/content/hello.txt", "r"); char line[128] = "";
      if (f) { if (fgets(line, sizeof line, f)) { char *nl = strchr(line, '\n'); if (nl) *nl = 0; } fclose(f); LOG("content ok: \"%s\"", line); }
      else LOG("content FAILED: /vol/content/hello.txt not readable"); }

    // 2. shader compiler + shaders
    int status = 0;    /* 0 ok, 1 compiler missing, 2 compile failed */
    int gr = glsl_init();
    if (gr != 0) { status = 1; LOG("glslcompiler.rpl not loaded (code %d). Copy it to sd:/wiiu/libs/glslcompiler.rpl", gr); }
    else {
        char info[2048]; info[0] = 0;
        memset(&g_shader, 0, sizeof g_shader);
        g_shader.vertexShader = glsl_CompileVertexShader(VS_SRC, info, sizeof info, GLSL_COMPILER_FLAG_NONE);
        if (!g_shader.vertexShader) { status = 2; LOG("vertex shader failed: %s", info); }
        else {
            info[0] = 0;
            g_shader.pixelShader = glsl_CompilePixelShader(PS_SRC, info, sizeof info, GLSL_COMPILER_FLAG_NONE);
            if (!g_shader.pixelShader) { status = 2; LOG("pixel shader failed: %s", info); }
        }
        if (status == 0) {
            int ok = 1;
            ok &= WHBGfxInitShaderAttribute(&g_shader, "in_position", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
            ok &= WHBGfxInitShaderAttribute(&g_shader, "in_normal", 0, 12, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
            ok &= WHBGfxInitShaderAttribute(&g_shader, "in_texcoord", 0, 24, GX2_ATTRIB_FORMAT_FLOAT_32_32);
            ok &= WHBGfxInitShaderAttribute(&g_shader, "in_color", 0, 32, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
            if (!ok) { status = 2; LOG("a vertex attribute name was not found in the compiled shader (attribVarCount %u)", g_shader.vertexShader->attribVarCount); }
            else if (!WHBGfxInitFetchShader(&g_shader)) { status = 2; LOG("fetch shader init failed"); }
            g_loc_frame = block_location(g_shader.vertexShader, "Frame", 0);
            g_loc_object = block_location(g_shader.vertexShader, "Object", 1);
            LOG("shaders ok: %u attribs, blocks Frame@%u Object@%u, pixel samplers %u", g_shader.numAttributes, g_loc_frame, g_loc_object, g_shader.pixelShader->samplerVarCount);
            if (g_shader.pixelShader->samplerVarCount == 0) { status = 2; LOG("pixel shader has no sampler"); }
        }
    }
    g_shader_ok = (status == 0);

    // 3. geometry, texture, uniform pools
    if (g_shader_ok) {
        Vertex cube[36]; build_cube(cube);
        Vertex ground[6]; quad_face(ground, v3(0, 0, 0), v3(40, 0, 0), v3(0, 0, -40), v3(0, 1, 0), 1, 1, 1, 20);
        Vertex quad[6]; quad_face(quad, v3(0, 0, 0), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), 1, 1, 1, 1);
        if (!mesh_create(&g_cube, cube, 36) || !mesh_create(&g_ground, ground, 6) || !mesh_create(&g_quad, quad, 6)) { LOG("vertex buffer creation failed"); g_shader_ok = 0; status = 2; }
        if (!texture_create()) { LOG("texture creation failed"); g_shader_ok = 0; status = 2; }
        pool_init(&g_pool[0], sizeof(ObjectBlock)); pool_init(&g_pool[1], sizeof(ObjectBlock));
        LOG("scene ready");
    }

    World w = {0, 0, 0, 0, 0, 16.7f, 0};
    OSTime last = OSGetTime(); uint64_t frames = 0; double acc_ms = 0, worst_ms = 0;
    while (WHBProcIsRunning()) {
        OSTime now = OSGetTime();
        float dt_ms = (float)OSTicksToMicroseconds(now - last) / 1000.0f; last = now;
        if (dt_ms > 100.0f) dt_ms = 100.0f;
        w.frame_ms = dt_ms; frames++; acc_ms += dt_ms; if (dt_ms > worst_ms) worst_ms = dt_ms;
        if (frames % 120 == 0) { LOG("frame %llu: avg %.2f ms, worst %.2f ms, controller %d", (unsigned long long)frames, acc_ms / 120.0, worst_ms, w.pad_source); acc_ms = 0; worst_ms = 0; }

        Pad pad; read_pad(&pad); w.pad_source = pad.source;
        float dt = dt_ms / 1000.0f;
        w.heading -= pad.sx * 2.2f * dt;
        float speed = (pad.a ? 9.0f : 0.0f) - (pad.b ? 4.0f : 0.0f) + pad.sy * 5.0f;
        w.cx += sinf(w.heading) * speed * dt; w.cz += cosf(w.heading) * speed * dt;
        w.cx = w.cx > 36 ? 36 : (w.cx < -36 ? -36 : w.cx); w.cz = w.cz > 36 ? 36 : (w.cz < -36 ? -36 : w.cz);
        w.spin += (pad.zr ? 6.0f : 1.2f) * dt;
        w.tilt = pad.zl ? 0.6f : 0.0f;

        g_pool_cur ^= 1; g_pool[g_pool_cur].next = 0;
        WHBGfxBeginRender();
        WHBGfxBeginRenderTV();
        if (status == 1) WHBGfxClearColor(1.0f, 0.5f, 0.0f, 1.0f);
        else if (status == 2) WHBGfxClearColor(0.8f, 0.05f, 0.05f, 1.0f);
        else { WHBGfxClearColor(0.45f, 0.72f, 0.98f, 1.0f); render_view(&w, 1280.0f / 720.0f); }
        WHBGfxFinishRenderTV();
        WHBGfxBeginRenderDRC();
        if (status == 1) WHBGfxClearColor(1.0f, 0.5f, 0.0f, 1.0f);
        else if (status == 2) WHBGfxClearColor(0.8f, 0.05f, 0.05f, 1.0f);
        else { WHBGfxClearColor(0.45f, 0.72f, 0.98f, 1.0f); render_view(&w, 854.0f / 480.0f); }
        WHBGfxFinishRenderDRC();
        WHBGfxFinishRender();
    }

    LOG("shutting down after %llu frames", (unsigned long long)frames);
    if (g_shader_ok) {
        GX2DrawDone();
        GX2RDestroyBufferEx(&g_cube.buf, GX2R_RESOURCE_BIND_NONE); GX2RDestroyBufferEx(&g_ground.buf, GX2R_RESOURCE_BIND_NONE); GX2RDestroyBufferEx(&g_quad.buf, GX2R_RESOURCE_BIND_NONE);
        free(g_tex.surface.image); free(g_pool[0].mem); free(g_pool[1].mem);
        if (g_shader.vertexShader) glsl_FreeVertexShader(g_shader.vertexShader);
        if (g_shader.pixelShader) glsl_FreePixelShader(g_shader.pixelShader);
    }
    glsl_shutdown();
    WHBGfxShutdown();
    KPADShutdown();
    if (g_logfile) fclose(g_logfile);
    WHBUnmountSdCard();
    WHBProcShutdown();
    WHBLogUdpDeinit(); WHBLogCafeDeinit();
    return 0;
}
