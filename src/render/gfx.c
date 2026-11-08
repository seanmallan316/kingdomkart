// gfx.c - GX2 renderer for the Wii U: one lit-textured shader compiled at startup by CafeGLSL.
#include "gfx.h"
#include <coreinit/cache.h>
#include <coreinit/time.h>
#include <gx2/clear.h>
#include <gx2/utils.h>
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
#include <whb/gfx.h>
#include <whb/log.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../platform/cafeglsl.h"

#define MESH_RING 4   /* a dynamic mesh can be refilled this many times per frame (one per split-screen view) */
struct Mesh { GX2RBuffer buf[MESH_RING]; int cur, nbuf; uint32_t count, cap; };
struct Texture { GX2Texture tex; GX2Sampler smp; };

static const char *VS_SRC =
    "#version 450\n"
    "layout(location = 0) in vec3 in_position;\n"
    "layout(location = 1) in vec3 in_normal;\n"
    "layout(location = 2) in vec2 in_texcoord;\n"
    "layout(location = 3) in vec4 in_color;\n"
    /* Frame: u_lightdir.w = ambient strength, u_fog = start, end, enabled, 0; u_sky / u_ground = the ambient light from above / below
       (hemisphere lighting), u_sun = the sun's color, u_eye = camera position. Object: u_params = specular strength, unlit flag, uv scroll */
    "layout(binding = 0) uniform Frame { mat4 u_viewproj; vec4 u_lightdir; vec4 u_fog; vec4 u_sky; vec4 u_ground; vec4 u_sun; vec4 u_eye; mat4 u_shadowvp; vec4 u_shadowp; };\n"
    "layout(binding = 1) uniform Object { mat4 u_model; vec4 u_tint; vec4 u_params; };\n"
    "layout(location = 0) out vec2 v_texcoord;\n"
    "layout(location = 1) out vec4 v_color;\n"
    "layout(location = 2) out vec4 v_n;\n"      /* the surface's direction (world), w = specular strength */
    "layout(location = 3) out float v_fog;\n"
    "layout(location = 4) out vec4 v_v;\n"      /* toward the camera (world, not normalized), w = unlit */
    "layout(location = 5) out vec4 v_shadow;\n"
    "void main() {\n"
    "  vec4 wp = u_model * vec4(in_position, 1.0);\n"
    /* wind: a normal stored longer than 1 marks a corner that sways (leaves, fronds, flags), by that much extra in meters */
    "  float sw = max(length(in_normal) - 1.05, 0.0);\n"
    "  if (sw > 0.0) { float tt = u_eye.w;\n"
    "    wp.x += sw * (sin(tt * 1.6 + wp.x * 0.17 + wp.z * 0.11) * 0.7 + sin(tt * 3.7 + wp.z * 0.5 + wp.y) * 0.3);\n"
    "    wp.z += sw * (cos(tt * 1.25 + wp.z * 0.15 + wp.x * 0.09) * 0.55 + sin(tt * 4.1 + wp.x * 0.45) * 0.25);\n"
    "    wp.y += sw * sin(tt * 2.3 + wp.x * 0.3) * 0.15; }\n"
    "  vec3 n = normalize((u_model * vec4(in_normal, 0.0)).xyz);\n"
    "  vec3 L = normalize(u_lightdir.xyz);\n"
    "  v_n = vec4(n, u_params.x);\n"
    "  v_v = vec4(u_eye.xyz - wp.xyz, u_params.y);\n"
    /* where this spot falls in the sun's view, for the shadow (pushed out a little along its normal so surfaces don't shadow themselves) */
    "  vec4 sp = u_shadowvp * vec4(wp.xyz + n * u_shadowp.w, 1.0);\n"
    "  v_shadow = vec4(sp.xy * 0.5 + 0.5, sp.z, max(dot(n, L), 0.0) + 2.0 * u_shadowp.x * (1.0 - u_params.y));\n"   /* w: facing the sun, plus 2 when shadows are on */
    "  v_texcoord = in_texcoord + u_params.zw;\n"
    "  v_color = in_color * u_tint;\n"
    "  vec4 cp = u_viewproj * wp;\n"
    "  v_fog = clamp((cp.w - u_fog.x) / max(u_fog.y - u_fog.x, 0.01), 0.0, 1.0) * u_fog.z * (1.0 - u_params.y);\n"
    "  gl_Position = cp;\n"
    "}\n";
static const char *PS_SRC =
    "#version 450\n"
    "layout(location = 0) in vec2 v_texcoord;\n"
    "layout(location = 1) in vec4 v_color;\n"
    "layout(location = 2) in vec4 v_n;\n"
    "layout(location = 3) in float v_fog;\n"
    "layout(location = 4) in vec4 v_v;\n"
    "layout(location = 5) in vec4 v_shadow;\n"
    /* u_light: the sun's direction and the ambient share; u_sky / u_ground: ambient light from above / below;
       u_grade: x = film look (soft highlights, contrast, color), y = rim light, z = soft wrap of the sun round shapes, w = color richness */
    "layout(binding = 0) uniform FogColor { vec4 u_fogcolor; vec4 u_speccolor; vec4 u_shadowpx; vec4 u_light; vec4 u_skyc; vec4 u_groundc; vec4 u_grade; };\n"
    "layout(binding = 0) uniform sampler2D u_tex;\n"
    "layout(binding = 1) uniform sampler2D u_shadowmap;\n"
    "layout(location = 0) out vec4 out_color;\n"
    "void main() {\n"
    "  vec4 t = texture(u_tex, v_texcoord) * v_color;\n"
    "  if (t.a < 0.03) discard;\n"
    "  vec3 c = t.rgb;\n"
    "  if (v_v.w < 0.5 && u_light.w < 0.999) {\n"   /* lit things (the flat screen layer has full ambient: drawn as is) */
    "    vec3 N = normalize(v_n.xyz); float vl = length(v_v.xyz); vec3 V = vl > 0.0001 ? v_v.xyz / vl : N; vec3 L = normalize(u_light.xyz); float amb = u_light.w;\n"
    "    float nl = dot(N, L);\n"
    "    float lit = 1.0;\n"
    "    if (v_shadow.w > 1.5) {\n"   /* in the sun's view: 4 soft samples of what is nearest the sun there */
    "      vec2 uv = vec2(v_shadow.x, 1.0 - v_shadow.y); float d = v_shadow.z - u_shadowpx.y; float o = u_shadowpx.x;\n"
    "      if (uv.x > 0.0 && uv.x < 1.0 && uv.y > 0.0 && uv.y < 1.0) {\n"
    "        lit = 0.25 * (step(d, texture(u_shadowmap, uv + vec2(-o, -o)).r) + step(d, texture(u_shadowmap, uv + vec2(o, -o)).r)\n"
    "                    + step(d, texture(u_shadowmap, uv + vec2(-o, o)).r) + step(d, texture(u_shadowmap, uv + vec2(o, o)).r));\n"
    "        lit = mix(1.0, lit, clamp((v_shadow.w - 2.0) * 4.0, 0.0, 1.0) * u_shadowpx.z); } }\n"
    /* the sun wraps a little round rounded shapes (soft, animated-film light), then a gentle S so the lit side reads clearly */
    "    float w = u_grade.z; float diff = clamp((nl + w) / (1.0 + w), 0.0, 1.0); diff = diff * diff * (3.0 - 2.0 * diff);\n"
    "    vec3 hemi = mix(u_groundc.rgb, u_skyc.rgb, N.y * 0.5 + 0.5);\n"
    "    vec3 light = hemi * amb + u_speccolor.rgb * diff * (1.0 - amb) * lit * 1.15;\n"
    /* the rim: the sky's light catching the edges of things, stronger on the sunny side - it lifts every shape off its background */
    "    float fr = 1.0 - max(dot(N, V), 0.0); float rim = fr * fr * fr * u_grade.y;\n"
    "    light += mix(u_skyc.rgb, u_speccolor.rgb, 0.5 + 0.5 * clamp(nl, 0.0, 1.0)) * rim * (0.45 + 0.55 * lit);\n"
    "    vec3 H = normalize(L + V + vec3(0.0, 0.0001, 0.0)); float sp = pow(max(dot(N, H), 0.0), 48.0) * v_n.w + pow(max(dot(N, H), 0.0), 8.0) * 0.04 * u_grade.x;\n"
    "    c = t.rgb * light + u_speccolor.rgb * sp * lit;\n"
    "  }\n"
    "  c = mix(c, u_fogcolor.rgb, v_fog);\n"
    "  if (u_grade.x > 0.0) {\n"   /* the film look: bright parts roll off softly instead of clipping, a little contrast, richer color */
    "    vec3 hi = max(c - 0.78, 0.0); c = min(c, 0.78) + 0.22 * (1.0 - exp(-hi / 0.22));\n"
    "    vec3 s = c * c * (3.0 - 2.0 * c); c = mix(c, s, 0.18 * u_grade.x);\n"
    "    float lum = dot(c, vec3(0.299, 0.587, 0.114)); c = mix(vec3(lum), c, mix(1.0, u_grade.w, u_grade.x));\n"
    "    c = clamp(c, 0.0, 1.0); }\n"
    "  out_color = vec4(c, t.a);\n"
    "}\n";

/* the shadow pass: the scene seen from the sun, writing only how far each spot is from it (see-through leaf pixels skipped) */
static const char *VS_SHADOW_SRC =
    "#version 450\n"
    "layout(location = 0) in vec3 in_position;\n"
    "layout(location = 1) in vec3 in_normal;\n"
    "layout(location = 2) in vec2 in_texcoord;\n"
    "layout(location = 3) in vec4 in_color;\n"
    "layout(binding = 0) uniform Frame { mat4 u_viewproj; vec4 u_lightdir; vec4 u_fog; vec4 u_sky; vec4 u_ground; vec4 u_sun; vec4 u_eye; mat4 u_shadowvp; vec4 u_shadowp; };\n"
    "layout(binding = 1) uniform Object { mat4 u_model; vec4 u_tint; vec4 u_params; };\n"
    "layout(location = 0) out vec2 v_texcoord;\n"
    "layout(location = 1) out float v_depth;\n"
    "layout(location = 2) out float v_alpha;\n"
    "void main() {\n"
    "  vec4 wp = u_model * vec4(in_position, 1.0);\n"
    "  float sw = max(length(in_normal) - 1.05, 0.0);\n"
    "  if (sw > 0.0) { float tt = u_eye.w;\n"
    "    wp.x += sw * (sin(tt * 1.6 + wp.x * 0.17 + wp.z * 0.11) * 0.7 + sin(tt * 3.7 + wp.z * 0.5 + wp.y) * 0.3);\n"
    "    wp.z += sw * (cos(tt * 1.25 + wp.z * 0.15 + wp.x * 0.09) * 0.55 + sin(tt * 4.1 + wp.x * 0.45) * 0.25);\n"
    "    wp.y += sw * sin(tt * 2.3 + wp.x * 0.3) * 0.15; }\n"
    "  vec4 cp = u_viewproj * wp;\n"
    "  v_depth = cp.z; v_texcoord = in_texcoord + u_params.zw; v_alpha = in_color.a * u_tint.a;\n"
    "  gl_Position = cp;\n"
    "}\n";
static const char *PS_SHADOW_SRC =
    "#version 450\n"
    "layout(location = 0) in vec2 v_texcoord;\n"
    "layout(location = 1) in float v_depth;\n"
    "layout(location = 2) in float v_alpha;\n"
    "layout(binding = 0) uniform sampler2D u_tex;\n"
    "layout(location = 0) out vec4 out_color;\n"
    "void main() {\n"
    "  if (texture(u_tex, v_texcoord).a * v_alpha < 0.5) discard;\n"
    "  out_color = vec4(v_depth, 0.0, 0.0, 1.0);\n"
    "}\n";
typedef struct { float viewproj[16]; float lightdir[4]; float fog[4]; float sky[4]; float ground[4]; float sun[4]; float eye[4]; float shadowvp[16]; float shadowp[4]; } FrameBlock;
typedef struct { float model[16]; float tint[4]; float params[4]; } ObjectBlock;
static float g_grade[4] = {1.0f, 0.38f, 0.32f, 1.14f};   /* the film look, rim, wrap, color richness */
static float g_sun[3] = {1, 1, 1}, g_sky[3] = {1, 1, 1}, g_ground[3] = {1, 1, 1}, g_eye[3] = {0, 0, 0}, g_wind_t = 0;
void gfx_set_time(float t) { g_wind_t = t; }
typedef struct { float color[4]; float spec[4]; float shadowpx[4]; float light[4]; float sky[4]; float ground[4]; float grade[4]; } FogBlock;

#define UB_ALIGN 256
#define UB_SLOTS 8192   /* per frame, both screens (the moving drivers add about 7 draws a kart); running out would overwrite matrices the GPU has not used yet */
typedef struct { uint8_t *mem; int next; } UniformPool;
static UniformPool g_pool[2]; static int g_pool_cur = 0;
static WHBGfxShaderGroup g_shader; static Texture *g_white; static Texture *g_bound;
static uint32_t g_loc_frame = 0, g_loc_object = 1, g_loc_fog = 0;
static void *g_framebuf = NULL, *g_fogbuf = NULL;
static int g_flags = -1, g_initialized = 0;
/* the sun's shadow map */
#define SHADOW_RES 1024
static WHBGfxShaderGroup g_shs; static uint32_t g_shloc_frame = 0, g_shloc_object = 1; static int g_shadow_ok = 0, g_in_shadow = 0, g_shadow_use = 0, g_shadow_have = 0;
static GX2ColorBuffer g_shcb; static GX2DepthBuffer g_shdb; static GX2Texture g_shtex; static GX2Sampler g_shsmp; static GX2ContextState *g_shctx; static float g_shadow_vp[16];
static void bind_shadow_sampler(void);
static uint32_t g_loc_tex = 0, g_loc_shmap = 1, g_shloc_tex = 0;
static uint32_t sampler_location(const GX2PixelShader *ps, const char *name, uint32_t fallback) { for (uint32_t i = 0; i < ps->samplerVarCount; i++) if (!strcmp(ps->samplerVars[i].name, name)) return ps->samplerVars[i].location; return fallback; }
static FILE *g_logfile = NULL;

void gfx_log(const char *fmt, ...) {
    char buf[512]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    WHBLogPrintf("%s", buf);
    if (g_logfile) { fprintf(g_logfile, "%s\n", buf); fflush(g_logfile); }
}
void gfx_set_logfile(FILE *f) { g_logfile = f; }

static void *pool_upload(const void *src, uint32_t bytes) {
    UniformPool *p = &g_pool[g_pool_cur];
    if (p->next >= UB_SLOTS) { static int warned = 0; if (!warned) { warned = 1; gfx_log("uniform pool ran out (%d draws in one frame)", UB_SLOTS); } p->next = 0; }
    uint8_t *dst = p->mem + p->next++ * UB_ALIGN;
    const uint32_t *s = src; uint32_t *d = (uint32_t *)dst;
    for (uint32_t i = 0; i < bytes / 4; i++) { uint32_t v = s[i]; d[i] = (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24); }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK, dst, UB_ALIGN);
    return dst;
}
static uint32_t vblock_location(const GX2VertexShader *vs, const char *name, uint32_t fallback) {
    for (uint32_t i = 0; i < vs->uniformBlockCount; i++) if (vs->uniformBlocks[i].name && strcmp(vs->uniformBlocks[i].name, name) == 0) return vs->uniformBlocks[i].offset;
    return fallback;
}
static uint32_t pblock_location(const GX2PixelShader *ps, const char *name, uint32_t fallback) {
    for (uint32_t i = 0; i < ps->uniformBlockCount; i++) if (ps->uniformBlocks[i].name && strcmp(ps->uniformBlocks[i].name, name) == 0) return ps->uniformBlocks[i].offset;
    return fallback;
}

int gfx_init(void) {
    WHBGfxInit();
    int gr = glsl_init();
    if (gr != 0) { gfx_log("glslcompiler.rpl not loaded (code %d). Copy it to sd:/wiiu/libs/glslcompiler.rpl", gr); return GFX_NO_COMPILER; }
    char info[2048]; info[0] = 0;
    memset(&g_shader, 0, sizeof g_shader);
    g_shader.vertexShader = glsl_CompileVertexShader(VS_SRC, info, sizeof info, GLSL_COMPILER_FLAG_NONE);
    if (!g_shader.vertexShader) { gfx_log("vertex shader failed: %s", info); return GFX_SHADER_FAILED; }
    info[0] = 0;
    g_shader.pixelShader = glsl_CompilePixelShader(PS_SRC, info, sizeof info, GLSL_COMPILER_FLAG_NONE);
    if (!g_shader.pixelShader) { gfx_log("pixel shader failed: %s", info); return GFX_SHADER_FAILED; }
    int ok = 1;
    ok &= WHBGfxInitShaderAttribute(&g_shader, "in_position", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
    ok &= WHBGfxInitShaderAttribute(&g_shader, "in_normal", 0, 12, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
    ok &= WHBGfxInitShaderAttribute(&g_shader, "in_texcoord", 0, 24, GX2_ATTRIB_FORMAT_FLOAT_32_32);
    ok &= WHBGfxInitShaderAttribute(&g_shader, "in_color", 0, 32, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    if (!ok) { gfx_log("vertex attribute missing (attribVarCount %u)", g_shader.vertexShader->attribVarCount); return GFX_SHADER_FAILED; }
    if (!WHBGfxInitFetchShader(&g_shader)) { gfx_log("fetch shader failed"); return GFX_SHADER_FAILED; }
    if (g_shader.pixelShader->samplerVarCount == 0) { gfx_log("pixel shader has no sampler"); return GFX_SHADER_FAILED; }
    g_loc_frame = vblock_location(g_shader.vertexShader, "Frame", 0);
    g_loc_object = vblock_location(g_shader.vertexShader, "Object", 1);
    g_loc_fog = pblock_location(g_shader.pixelShader, "FogColor", 0);
    g_loc_tex = sampler_location(g_shader.pixelShader, "u_tex", g_shader.pixelShader->samplerVars[0].location); g_loc_shmap = sampler_location(g_shader.pixelShader, "u_shadowmap", 1);
    gfx_log("shaders ok: blocks Frame@%u Object@%u FogColor@%u, samplers %u", g_loc_frame, g_loc_object, g_loc_fog, g_shader.pixelShader->samplerVarCount);
    for (int i = 0; i < 2; i++) { g_pool[i].mem = memalign(UB_ALIGN, UB_ALIGN * UB_SLOTS); memset(g_pool[i].mem, 0, UB_ALIGN * UB_SLOTS); g_pool[i].next = 0; }
    uint32_t white[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
    g_white = gfx_texture_rgba(white, 2, 2, 1, 1);
    g_initialized = 1;
    /* the shadow map: optional - if any of this fails the game simply has no sun shadows */
    memset(&g_shs, 0, sizeof g_shs); info[0] = 0;
    g_shs.vertexShader = glsl_CompileVertexShader(VS_SHADOW_SRC, info, sizeof info, GLSL_COMPILER_FLAG_NONE);
    if (g_shs.vertexShader) g_shs.pixelShader = glsl_CompilePixelShader(PS_SHADOW_SRC, info, sizeof info, GLSL_COMPILER_FLAG_NONE);
    if (g_shs.vertexShader && g_shs.pixelShader) {
        int k = 1; k &= WHBGfxInitShaderAttribute(&g_shs, "in_position", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
        WHBGfxInitShaderAttribute(&g_shs, "in_normal", 0, 12, GX2_ATTRIB_FORMAT_FLOAT_32_32_32); WHBGfxInitShaderAttribute(&g_shs, "in_texcoord", 0, 24, GX2_ATTRIB_FORMAT_FLOAT_32_32);
        WHBGfxInitShaderAttribute(&g_shs, "in_color", 0, 32, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
        if (k && WHBGfxInitFetchShader(&g_shs) && g_shs.pixelShader->samplerVarCount >= 1 && g_shader.pixelShader->samplerVarCount >= 2) {
            g_shloc_tex = sampler_location(g_shs.pixelShader, "u_tex", g_shs.pixelShader->samplerVars[0].location); g_shloc_frame = vblock_location(g_shs.vertexShader, "Frame", 0); g_shloc_object = vblock_location(g_shs.vertexShader, "Object", 1);
            memset(&g_shcb, 0, sizeof g_shcb); g_shcb.surface.use = GX2_SURFACE_USE_TEXTURE | GX2_SURFACE_USE_COLOR_BUFFER; g_shcb.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
            g_shcb.surface.width = g_shcb.surface.height = SHADOW_RES; g_shcb.surface.depth = 1; g_shcb.surface.mipLevels = 1; g_shcb.surface.format = GX2_SURFACE_FORMAT_FLOAT_R32;
            g_shcb.surface.aa = GX2_AA_MODE1X; g_shcb.surface.tileMode = GX2_TILE_MODE_DEFAULT; g_shcb.viewNumSlices = 1;
            GX2CalcSurfaceSizeAndAlignment(&g_shcb.surface); GX2InitColorBufferRegs(&g_shcb);
            memset(&g_shdb, 0, sizeof g_shdb); g_shdb.surface.use = GX2_SURFACE_USE_DEPTH_BUFFER; g_shdb.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
            g_shdb.surface.width = g_shdb.surface.height = SHADOW_RES; g_shdb.surface.depth = 1; g_shdb.surface.mipLevels = 1; g_shdb.surface.format = GX2_SURFACE_FORMAT_FLOAT_D24_S8;
            g_shdb.surface.aa = GX2_AA_MODE1X; g_shdb.surface.tileMode = GX2_TILE_MODE_DEFAULT; g_shdb.viewNumSlices = 1; g_shdb.depthClear = 1.0f;
            GX2CalcSurfaceSizeAndAlignment(&g_shdb.surface); GX2InitDepthBufferRegs(&g_shdb);
            g_shcb.surface.image = memalign(g_shcb.surface.alignment, g_shcb.surface.imageSize); g_shdb.surface.image = memalign(g_shdb.surface.alignment, g_shdb.surface.imageSize);
            g_shctx = memalign(GX2_CONTEXT_STATE_ALIGNMENT, sizeof(GX2ContextState));
            if (g_shcb.surface.image && g_shdb.surface.image && g_shctx) {
                GX2Invalidate(GX2_INVALIDATE_MODE_CPU, g_shcb.surface.image, g_shcb.surface.imageSize); GX2Invalidate(GX2_INVALIDATE_MODE_CPU, g_shdb.surface.image, g_shdb.surface.imageSize);
                memset(&g_shtex, 0, sizeof g_shtex); g_shtex.surface = g_shcb.surface; g_shtex.surface.use = GX2_SURFACE_USE_TEXTURE; g_shtex.viewNumMips = 1; g_shtex.viewNumSlices = 1;
                g_shtex.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1); GX2InitTextureRegs(&g_shtex);
                GX2InitSampler(&g_shsmp, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);
                GX2SetupContextStateEx(g_shctx, TRUE); GX2SetContextState(g_shctx);
                GX2SetColorBuffer(&g_shcb, GX2_RENDER_TARGET_0); GX2SetDepthBuffer(&g_shdb);
                GX2SetViewport(0, 0, SHADOW_RES, SHADOW_RES, 0.0f, 1.0f); GX2SetScissor(0, 0, SHADOW_RES, SHADOW_RES);
                g_shadow_ok = 1; } } }
    gfx_log("sun shadows %s%s", g_shadow_ok ? "on" : "off: ", g_shadow_ok ? "" : info);
    return GFX_OK;
}

void gfx_shutdown(void) {
    if (g_initialized) {
        GX2DrawDone();
        if (g_shader.vertexShader) glsl_FreeVertexShader(g_shader.vertexShader);
        if (g_shader.pixelShader) glsl_FreePixelShader(g_shader.pixelShader);
    }
    glsl_shutdown();
    WHBGfxShutdown();
}

// ------------------------------------------------------------------ meshes
static int buffer_create(GX2RBuffer *b, uint32_t cap) {
    memset(b, 0, sizeof *b);
    b->flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_READ | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ;
    b->elemSize = sizeof(Vertex); b->elemCount = cap;
    return GX2RCreateBuffer(b) ? 1 : 0;
}
static int buffer_fill(GX2RBuffer *b, const Vertex *v, uint32_t n) {
    void *p = GX2RLockBufferEx(b, GX2R_RESOURCE_BIND_NONE);
    if (!p) return 0;
    if (n) memcpy(p, v, sizeof(Vertex) * n);
    GX2RUnlockBufferEx(b, GX2R_RESOURCE_BIND_NONE);
    return 1;
}
Mesh *gfx_mesh_static(const Vertex *v, uint32_t n) {
    Mesh *m = calloc(1, sizeof *m); if (!m) return NULL;
    m->nbuf = 1; m->cap = n ? n : 1; m->count = n;
    if (!buffer_create(&m->buf[0], m->cap) || !buffer_fill(&m->buf[0], v, n)) { gfx_log("static mesh of %u verts failed", n); free(m); return NULL; }
    return m;
}
Mesh *gfx_mesh_dynamic(uint32_t cap) {
    Mesh *m = calloc(1, sizeof *m); if (!m) return NULL;
    m->nbuf = MESH_RING; m->cap = cap; m->count = 0;
    for (int i = 0; i < MESH_RING; i++) if (!buffer_create(&m->buf[i], cap)) { gfx_log("dynamic mesh of %u verts failed", cap); free(m); return NULL; }
    return m;
}
void gfx_mesh_update(Mesh *m, const Vertex *v, uint32_t n) {
    if (!m) return;
    if (n > m->cap) n = m->cap;
    if (m->nbuf > 1) m->cur = (m->cur + 1) % m->nbuf;
    buffer_fill(&m->buf[m->cur], v, n);
    m->count = n;
}
void gfx_mesh_free(Mesh *m) { if (!m) return; for (int i = 0; i < m->nbuf; i++) GX2RDestroyBufferEx(&m->buf[i], GX2R_RESOURCE_BIND_NONE); free(m); }

// ------------------------------------------------------------------ textures
/* copies one level's linear pixels into the tiled texture */
static void *upload_level(GX2Texture *tx, int level, const uint32_t *pixels, int w, int h) {   /* returns the staging buffer: free it after GX2DrawDone */
    GX2Surface lin = tx->surface; lin.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED; lin.width = w; lin.height = h; lin.mipLevels = 1;
    GX2CalcSurfaceSizeAndAlignment(&lin);
    lin.image = memalign(lin.alignment, lin.imageSize);
    if (!lin.image) return NULL;
    memset(lin.image, 0, lin.imageSize);
    for (int y = 0; y < h; y++) memcpy((uint8_t *)lin.image + (size_t)y * lin.pitch * 4, pixels + (size_t)y * w, (size_t)w * 4);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, lin.image, lin.imageSize);
    GX2CopySurface(&lin, 0, 0, &tx->surface, level, 0);
    return lin.image;
}
/* half-size picture, averaging 2x2 pixels; colors are weighted by alpha so transparent pixels do not darken the edges */
static void downsample(const uint32_t *src, int w, int h, uint32_t *dst, int dw, int dh) {
    for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
        unsigned r = 0, g = 0, b = 0, a = 0, wsum = 0;
        for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) {
            int sx = x * 2 + dx, sy = y * 2 + dy; if (sx >= w) sx = w - 1; if (sy >= h) sy = h - 1;
            uint32_t p = src[sy * w + sx]; unsigned pa = p & 255, wgt = pa + 1;   /* 0xRRGGBBAA in memory on a big-endian machine = the word's low byte is A */
            r += ((p >> 24) & 255) * wgt; g += ((p >> 16) & 255) * wgt; b += ((p >> 8) & 255) * wgt; a += pa; wsum += wgt; }
        dst[y * dw + x] = ((r / wsum) << 24) | ((g / wsum) << 16) | ((b / wsum) << 8) | (a / 4);
    }
}
Texture *gfx_texture_rgba(const uint32_t *pixels, int w, int h, int repeat, int nearest) { return gfx_texture_rgba_ex(pixels, w, h, repeat, nearest, 1); }
Texture *gfx_texture_rgba_ex(const uint32_t *pixels, int w, int h, int repeat, int nearest, int mipmaps) {
    Texture *t = calloc(1, sizeof *t); if (!t) return NULL;
    GX2Texture *tx = &t->tex;
    /* up to four levels (full, half, quarter, eighth) is enough to stop the shimmering; more only costs load time */
    int levels = 1; if (mipmaps && !nearest && (w > 128 || h > 128)) { int m = w > h ? w : h; while ((m >> levels) >= 1 && levels < 4) levels++; }
    tx->surface.use = GX2_SURFACE_USE_TEXTURE; tx->surface.width = w; tx->surface.height = h; tx->surface.depth = 1;
    tx->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D; tx->surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    tx->surface.tileMode = GX2_TILE_MODE_DEFAULT; tx->surface.aa = GX2_AA_MODE1X; tx->surface.mipLevels = (uint32_t)levels;
    tx->viewNumSlices = 1; tx->viewNumMips = (uint32_t)levels; tx->compMap = 0x00010203;
    GX2CalcSurfaceSizeAndAlignment(&tx->surface);
    GX2InitTextureRegs(tx);
    tx->surface.image = memalign(tx->surface.alignment, tx->surface.imageSize);
    if (!tx->surface.image) { free(t); return NULL; }
    if (levels > 1 && tx->surface.mipmapSize) { tx->surface.mipmaps = memalign(tx->surface.alignment, tx->surface.mipmapSize); if (!tx->surface.mipmaps) { free(tx->surface.image); free(t); return NULL; } }
    /* The GPU writes the picture into this memory, but the CPU cache may still hold dirty lines for it left over from
       whatever used the memory before (the heap reuses freed blocks). If such a line is written back after the GPU copy,
       it overwrites part of the picture with garbage - the scrambled portraits seen on the real Wii U (the emulator has
       no CPU cache, so it never showed). Write those lines back now, before the GPU touches the memory. */
    DCFlushRange(tx->surface.image, tx->surface.imageSize);
    if (tx->surface.mipmaps) DCFlushRange(tx->surface.mipmaps, tx->surface.mipmapSize);
    void *staging[13] = {0}; int nst = 0;
    staging[nst++] = upload_level(tx, 0, pixels, w, h);
    if (!staging[0]) { free(tx->surface.mipmaps); free(tx->surface.image); free(t); return NULL; }
    if (levels > 1) {   /* the smaller copies for far-away drawing */
        uint32_t *cur = malloc((size_t)w * h * 4), *nxt = malloc((size_t)w * h * 4); int cw = w, ch = h;
        if (cur && nxt) { memcpy(cur, pixels, (size_t)w * h * 4);
            for (int lv = 1; lv < levels; lv++) { int dw = cw > 1 ? cw / 2 : 1, dh = ch > 1 ? ch / 2 : 1; downsample(cur, cw, ch, nxt, dw, dh); staging[nst++] = upload_level(tx, lv, nxt, dw, dh); uint32_t *tmp = cur; cur = nxt; nxt = tmp; cw = dw; ch = dh; } }
        free(cur); free(nxt);
    }
    GX2DrawDone();
    for (int i = 0; i < nst; i++) free(staging[i]);
    /* the GPU wrote it: only the GPU's own texture cache needs refreshing (a CPU flush here could write stale lines over it) */
    if (tx->surface.mipmaps) GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, tx->surface.mipmaps, tx->surface.mipmapSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, tx->surface.image, tx->surface.imageSize);
    GX2InitSampler(&t->smp, repeat ? GX2_TEX_CLAMP_MODE_WRAP : GX2_TEX_CLAMP_MODE_CLAMP, nearest ? GX2_TEX_XY_FILTER_MODE_POINT : GX2_TEX_XY_FILTER_MODE_LINEAR);
    if (levels > 1) {
        GX2InitSamplerXYFilter(&t->smp, GX2_TEX_XY_FILTER_MODE_LINEAR, GX2_TEX_XY_FILTER_MODE_LINEAR, GX2_TEX_ANISO_RATIO_4_TO_1);
        GX2InitSamplerZMFilter(&t->smp, GX2_TEX_Z_FILTER_MODE_NONE, GX2_TEX_MIP_FILTER_MODE_LINEAR);
        GX2InitSamplerLOD(&t->smp, 0.0f, (float)(levels - 1), 0.0f);
    }
    return t;
}
/* a picture the CPU rewrites every frame (the opener video): stored untiled so new pixels can be written straight in,
   with no GPU copy and no wait for the GPU. Keep two and alternate, so the GPU never reads one while it is being written. */
Texture *gfx_texture_stream(int w, int h) {
    Texture *t = calloc(1, sizeof *t); if (!t) return NULL;
    GX2Texture *tx = &t->tex;
    tx->surface.use = GX2_SURFACE_USE_TEXTURE; tx->surface.width = w; tx->surface.height = h; tx->surface.depth = 1;
    tx->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D; tx->surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    tx->surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED; tx->surface.aa = GX2_AA_MODE1X; tx->surface.mipLevels = 1;
    tx->viewNumSlices = 1; tx->viewNumMips = 1; tx->compMap = 0x00010203;
    GX2CalcSurfaceSizeAndAlignment(&tx->surface);
    GX2InitTextureRegs(tx);
    tx->surface.image = memalign(tx->surface.alignment, tx->surface.imageSize);
    if (!tx->surface.image) { free(t); return NULL; }
    memset(tx->surface.image, 0, tx->surface.imageSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, tx->surface.image, tx->surface.imageSize);   /* writes the black back to memory, refreshes the GPU's copy */
    GX2InitSampler(&t->smp, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
    return t;
}
void *gfx_texture_stream_pixels(Texture *t, int *pitch_px) { if (!t) return NULL; if (pitch_px) *pitch_px = (int)t->tex.surface.pitch; return t->tex.surface.image; }
void gfx_texture_stream_flush_cpu(Texture *t) { if (t) DCFlushRange(t->tex.surface.image, t->tex.surface.imageSize); }   /* any thread: push the new pixels out of the CPU cache */
void gfx_texture_stream_gpu(Texture *t) { if (t) GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, t->tex.surface.image, t->tex.surface.imageSize); }   /* drawing thread: the GPU forgets its old copy */
void gfx_texture_stream_put(Texture *t, const uint32_t *pixels, int w, int h) {
    if (!t || !pixels) return;
    GX2Surface *sf = &t->tex.surface; if ((int)sf->width < w) w = (int)sf->width; if ((int)sf->height < h) h = (int)sf->height;
    for (int y = 0; y < h; y++) memcpy((uint8_t *)sf->image + (size_t)y * sf->pitch * 4, pixels + (size_t)y * w, (size_t)w * 4);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, sf->image, sf->imageSize);
}
void gfx_texture_free(Texture *t) { if (!t) return; free(t->tex.surface.mipmaps); free(t->tex.surface.image); free(t); }

// ------------------------------------------------------------------ frames and state
void gfx_frame_begin(void) {
    g_pool_cur ^= 1; g_pool[g_pool_cur].next = 0;
    WHBGfxBeginRender();
}
void gfx_view_begin(int drc, float r, float g, float b) {
    if (drc) WHBGfxBeginRenderDRC(); else WHBGfxBeginRenderTV();
    WHBGfxClearColor(r, g, b, 1.0f);
    gfx_viewport(drc, 0, 0, 1, 1);
    if (!g_initialized) return;
    GX2SetFetchShader(&g_shader.fetchShader);
    GX2SetVertexShader(g_shader.vertexShader);
    GX2SetPixelShader(g_shader.pixelShader);
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
    g_flags = -1; g_bound = NULL;
    gfx_bind(g_white); bind_shadow_sampler();
}
void gfx_clear_depth(int drc) {   /* a fresh depth buffer partway through a screen (for a 3D scene drawn over another), keeping the picture */
    GX2DepthBuffer *db = drc ? WHBGfxGetDRCDepthBuffer() : WHBGfxGetTVDepthBuffer();
    GX2ClearDepthStencilEx(db, db->depthClear, db->stencilClear, GX2_CLEAR_FLAGS_DEPTH | GX2_CLEAR_FLAGS_STENCIL);
    GX2SetContextState(drc ? WHBGfxGetDRCContextState() : WHBGfxGetTVContextState());
    gfx_viewport(drc, 0, 0, 1, 1);
    if (!g_initialized) return;
    GX2SetFetchShader(&g_shader.fetchShader); GX2SetVertexShader(g_shader.vertexShader); GX2SetPixelShader(g_shader.pixelShader); GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
    g_flags = -1; Texture *b = g_bound; g_bound = NULL; gfx_bind(b ? b : g_white); bind_shadow_sampler(); }
static void bind_shadow_sampler(void) {   /* the main pixel shader's second picture: the shadow map, or plain white when there is none */
    if (g_shader.pixelShader->samplerVarCount < 2) return; uint32_t loc = g_loc_shmap;
    if (g_shadow_ok) { GX2SetPixelTexture(&g_shtex, loc); GX2SetPixelSampler(&g_shsmp, loc); } else { GX2SetPixelTexture(&g_white->tex, loc); GX2SetPixelSampler(&g_white->smp, loc); } }
int gfx_shadow_begin(const Mat4 *lightvp) {   /* call between gfx_frame_begin and the first gfx_view_begin; then draw what casts shadows */
    if (!g_shadow_ok) return 0;
    GX2SetContextState(g_shctx);
    GX2ClearColor(&g_shcb, 1e6f, 0, 0, 1); GX2ClearDepthStencilEx(&g_shdb, 1.0f, 0, GX2_CLEAR_FLAGS_DEPTH | GX2_CLEAR_FLAGS_STENCIL);
    GX2SetContextState(g_shctx);
    GX2SetViewport(0, 0, SHADOW_RES, SHADOW_RES, 0.0f, 1.0f); GX2SetScissor(0, 0, SHADOW_RES, SHADOW_RES);
    GX2SetFetchShader(&g_shs.fetchShader); GX2SetVertexShader(g_shs.vertexShader); GX2SetPixelShader(g_shs.pixelShader); GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
    g_in_shadow = 1; g_flags = -1; g_bound = NULL; gfx_bind(g_white);
    memcpy(g_shadow_vp, lightvp->m, sizeof g_shadow_vp);
    gfx_camera(lightvp, v3(0, 1, 0), 1, 0, 0, 0, 1e6f, 2e6f);
    GX2SetDepthOnlyControl(TRUE, TRUE, GX2_COMPARE_FUNC_LEQUAL); GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE); GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x00, FALSE, TRUE); g_flags = ST_DEPTH | ST_DEPTH_WRITE;
    return 1; }
void gfx_shadow_end(void) {
    if (!g_in_shadow) return;
    g_in_shadow = 0; g_shadow_have = 1; g_bound = NULL; g_flags = -1;
    GX2Invalidate(GX2_INVALIDATE_MODE_COLOR_BUFFER | GX2_INVALIDATE_MODE_TEXTURE, g_shcb.surface.image, g_shcb.surface.imageSize); }
/* the picture drawn so far, copied into a texture (for the boost blur: stretched copies of it laid over the edges) */
static Texture g_grab[2]; static int g_grab_ok[2];
Texture *gfx_grab(int drc) { return gfx_grab_ex(drc, drc); }
Texture *gfx_grab_ex(int src, int drc) {   /* src: which screen's picture (0 TV, 1 GamePad); drc: the screen being drawn now */
    if (!g_initialized || g_in_shadow) return NULL;
    src = src ? 1 : 0; drc = drc ? 1 : 0;
    GX2ColorBuffer *cb = src ? WHBGfxGetDRCColourBuffer() : WHBGfxGetTVColourBuffer();
    Texture *t = &g_grab[src]; GX2Texture *tx = &t->tex;
    if (!g_grab_ok[src]) {
        g_grab_ok[src] = -1;
        tx->surface.use = GX2_SURFACE_USE_TEXTURE; tx->surface.width = cb->surface.width; tx->surface.height = cb->surface.height; tx->surface.depth = 1;
        tx->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D; tx->surface.format = cb->surface.format;
        tx->surface.tileMode = GX2_TILE_MODE_DEFAULT; tx->surface.aa = GX2_AA_MODE1X; tx->surface.mipLevels = 1;
        tx->viewNumSlices = 1; tx->viewNumMips = 1; tx->compMap = 0x00010205;   /* red, green, blue, and alpha always 1 */
        GX2CalcSurfaceSizeAndAlignment(&tx->surface); GX2InitTextureRegs(tx);
        tx->surface.image = memalign(tx->surface.alignment, tx->surface.imageSize);
        if (!tx->surface.image) { gfx_log("screen copy: no memory"); return NULL; }
        DCFlushRange(tx->surface.image, tx->surface.imageSize);
        GX2InitSampler(&t->smp, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
        g_grab_ok[src] = 1; gfx_log("screen copy %d ready (%ux%u)", src, tx->surface.width, tx->surface.height); }
    if (g_grab_ok[src] < 0) return NULL;
    GX2CopySurface(&cb->surface, 0, 0, &tx->surface, 0, 0);
    GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, tx->surface.image, tx->surface.imageSize);
    /* the copy may leave the GPU's state changed: put ours back (as gfx_clear_depth does); the caller sets its viewport */
    GX2SetContextState(drc ? WHBGfxGetDRCContextState() : WHBGfxGetTVContextState());
    GX2SetFetchShader(&g_shader.fetchShader); GX2SetVertexShader(g_shader.vertexShader); GX2SetPixelShader(g_shader.pixelShader); GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
    g_flags = -1; g_bound = NULL; gfx_bind(g_white); bind_shadow_sampler();
    return t; }
void gfx_shadow_use(int on) { g_shadow_use = on; }
Texture *gfx_shadow_texture(void) { static Texture t; if (!g_shadow_ok) return NULL; t.tex = g_shtex; t.smp = g_shsmp; return &t; }   /* for looking at the shadow map itself (tests) */
void gfx_shadow_none(void) { g_shadow_have = 0; }
void gfx_view_end(int drc) { if (drc) WHBGfxFinishRenderDRC(); else WHBGfxFinishRenderTV(); }
void gfx_frame_end(void) { WHBGfxFinishRender(); }
float gfx_aspect(int drc) { return drc ? 854.0f / 480.0f : 1280.0f / 720.0f; }
/* draws into one rectangle of the current screen (split screen); fractions of the screen, origin top-left */
void gfx_viewport(int drc, float fx, float fy, float fw, float fh) {
    GX2ColorBuffer *cb = drc ? WHBGfxGetDRCColourBuffer() : WHBGfxGetTVColourBuffer();
    float W = (float)cb->surface.width, H = (float)cb->surface.height;
    float x = floorf(fx * W), y = floorf(fy * H), w = floorf(fw * W), h = floorf(fh * H);
    GX2SetViewport(x, y, w, h, 0.0f, 1.0f);
    GX2SetScissor((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h);
}

void gfx_camera(const Mat4 *vp, Vec3 l, float ambient, float fr, float fg, float fb, float fs, float fe) {
    FrameBlock fb2; memcpy(fb2.viewproj, vp->m, sizeof fb2.viewproj);
    fb2.lightdir[0] = l.x; fb2.lightdir[1] = l.y; fb2.lightdir[2] = l.z; fb2.lightdir[3] = ambient;
    fb2.fog[0] = fs; fb2.fog[1] = fe; fb2.fog[2] = 1.0f; fb2.fog[3] = 0;
    for (int i = 0; i < 3; i++) { fb2.sky[i] = g_sky[i]; fb2.ground[i] = g_ground[i]; fb2.sun[i] = g_sun[i]; fb2.eye[i] = g_eye[i]; }
    fb2.sky[3] = fb2.ground[3] = fb2.sun[3] = 0; fb2.eye[3] = g_wind_t;
    int sh = g_shadow_ok && g_shadow_have && g_shadow_use && !g_in_shadow;
    memcpy(fb2.shadowvp, g_shadow_vp, sizeof fb2.shadowvp); fb2.shadowp[0] = sh ? 1.0f : 0.0f; fb2.shadowp[1] = fb2.shadowp[2] = 0; fb2.shadowp[3] = 0.08f;
    g_framebuf = pool_upload(&fb2, sizeof fb2);
    FogBlock fg2 = {{fr, fg, fb, 1}, {g_sun[0], g_sun[1], g_sun[2], 1}, {0.7f / SHADOW_RES, 0.0015f, 0.78f, 0}, {l.x, l.y, l.z, ambient},
                    {g_sky[0], g_sky[1], g_sky[2], 0}, {g_ground[0], g_ground[1], g_ground[2], 0}, {g_grade[0], g_grade[1], g_grade[2], g_grade[3]}};
    g_fogbuf = pool_upload(&fg2, sizeof fg2);
}
void gfx_grade(float film, float rim, float wrap, float rich) { g_grade[0] = film; g_grade[1] = rim; g_grade[2] = wrap; g_grade[3] = rich; }
void gfx_light(Vec3 sun, Vec3 sky, Vec3 ground, Vec3 eye) {
    g_sun[0] = sun.x; g_sun[1] = sun.y; g_sun[2] = sun.z; g_sky[0] = sky.x; g_sky[1] = sky.y; g_sky[2] = sky.z;
    g_ground[0] = ground.x; g_ground[1] = ground.y; g_ground[2] = ground.z; g_eye[0] = eye.x; g_eye[1] = eye.y; g_eye[2] = eye.z;
}
void gfx_camera_2d(void) { gfx_camera_2d_virtual(1280.0f, 720.0f); }
/* the 3D menus: the 1280 x 720 layout as a plane facing a camera 'eye' pixels away, so a menu card can lift toward you,
   tilt and turn in real perspective; anything left on the plane (z = 0) lands exactly where the 2D layout put it */
void gfx_camera_ui3d(float eye) {
    gfx_camera_2d_virtual(1280.0f, 720.0f);
    FrameBlock fb2; Mat4 m; memset(&m, 0, sizeof m);
    m.m[0] = eye / 640.0f; m.m[12] = -eye; m.m[5] = -eye / 360.0f; m.m[13] = eye; m.m[11] = -1.0f; m.m[15] = eye;
    memcpy(fb2.viewproj, m.m, sizeof fb2.viewproj);
    fb2.lightdir[0] = 0; fb2.lightdir[1] = 0; fb2.lightdir[2] = 1; fb2.lightdir[3] = 1.0f;
    fb2.fog[0] = 0; fb2.fog[1] = 1; fb2.fog[2] = 0; fb2.fog[3] = 0;
    for (int i = 0; i < 4; i++) { fb2.sky[i] = fb2.ground[i] = fb2.sun[i] = 1; fb2.eye[i] = 0; }
    memset(fb2.shadowvp, 0, sizeof fb2.shadowvp); memset(fb2.shadowp, 0, sizeof fb2.shadowp);
    g_framebuf = pool_upload(&fb2, sizeof fb2);
}
/* the 2D helpers lay out on a 1280 x 720 screen; in a split-screen view the virtual size is the view's own size in pixels, so text keeps its shape */
void gfx_camera_2d_virtual(float vw, float vh) {
    FrameBlock fb2; Mat4 id = m4_identity();
    float a = 1280.0f / vw, b = 720.0f / vh;      /* x' = a x + (a - 1), y' = b y + (1 - b) in clip space */
    id.m[0] = a; id.m[5] = b; id.m[12] = a - 1.0f; id.m[13] = 1.0f - b;
    memcpy(fb2.viewproj, id.m, sizeof fb2.viewproj);
    fb2.lightdir[0] = 0; fb2.lightdir[1] = 0; fb2.lightdir[2] = 1; fb2.lightdir[3] = 1.0f;
    fb2.fog[0] = 0; fb2.fog[1] = 1; fb2.fog[2] = 0; fb2.fog[3] = 0;
    for (int i = 0; i < 4; i++) { fb2.sky[i] = fb2.ground[i] = fb2.sun[i] = 1; fb2.eye[i] = 0; }
    memset(fb2.shadowvp, 0, sizeof fb2.shadowvp); memset(fb2.shadowp, 0, sizeof fb2.shadowp);
    g_framebuf = pool_upload(&fb2, sizeof fb2);
    FogBlock fg2 = {{0, 0, 0, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 1, 1}, {1, 1, 1, 0}, {1, 1, 1, 0}, {0, 0, 0, 1}};
    g_fogbuf = pool_upload(&fg2, sizeof fg2);
}
void gfx_bind(Texture *t) {
    if (!t) t = g_white;
    if (t == g_bound) return;
    g_bound = t;
    uint32_t loc = g_in_shadow ? g_shloc_tex : g_loc_tex;
    GX2SetPixelTexture(&t->tex, loc);
    GX2SetPixelSampler(&t->smp, loc);
}
void gfx_state(int flags) {
    if (g_in_shadow) return;   /* the shadow pass keeps its own: depth on, nothing blended */
    if (flags == g_flags) return;
    g_flags = flags;
    GX2SetDepthOnlyControl((flags & ST_DEPTH) ? TRUE : FALSE, (flags & ST_DEPTH_WRITE) ? TRUE : FALSE, (flags & ST_LESS) ? GX2_COMPARE_FUNC_LESS : GX2_COMPARE_FUNC_LEQUAL);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, (flags & ST_CULL) ? TRUE : FALSE);
    GX2SetTargetChannelMasks((flags & ST_AMASK) ? GX2_CHANNEL_MASK_A : GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA,
                             GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA);
    if (flags & ST_MULDA) {
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
        GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_ZERO, GX2_BLEND_MODE_DST_ALPHA, GX2_BLEND_COMBINE_MODE_ADD, TRUE, GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO, GX2_BLEND_COMBINE_MODE_ADD);
    } else if (flags & ST_BLEND) {
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
        if (flags & ST_ADD)
            GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_ONE, GX2_BLEND_COMBINE_MODE_ADD, TRUE, GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ONE, GX2_BLEND_COMBINE_MODE_ADD);
        else
            GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD, TRUE, GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD);
    } else {
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x00, FALSE, TRUE);
    }
}
void gfx_draw(const Mesh *m, const Mat4 *model, float r, float g, float b, float a) { gfx_draw_ex(m, model, r, g, b, a, 0, 0, 0, 0); }
void gfx_draw_ex(const Mesh *m, const Mat4 *model, float r, float g, float b, float a, float specular, int unlit, float scroll_u, float scroll_v) {
    if (!m || m->count == 0 || !g_framebuf) return;
    ObjectBlock ob; memcpy(ob.model, model->m, sizeof ob.model); ob.tint[0] = r; ob.tint[1] = g; ob.tint[2] = b; ob.tint[3] = a;
    ob.params[0] = specular; ob.params[1] = unlit ? 1.0f : 0.0f; ob.params[2] = scroll_u; ob.params[3] = scroll_v;
    void *obuf = pool_upload(&ob, sizeof ob);
    if (g_in_shadow) { GX2SetVertexUniformBlock(g_shloc_frame, sizeof(FrameBlock), g_framebuf); GX2SetVertexUniformBlock(g_shloc_object, sizeof(ObjectBlock), obuf); }
    else {
    GX2SetVertexUniformBlock(g_loc_frame, sizeof(FrameBlock), g_framebuf);
    GX2SetVertexUniformBlock(g_loc_object, sizeof(ObjectBlock), obuf);
    GX2SetPixelUniformBlock(g_loc_fog, sizeof(FogBlock), g_fogbuf); }
    GX2RSetAttributeBuffer((GX2RBuffer *)&m->buf[m->cur], 0, sizeof(Vertex), 0);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, m->count, 0, 1);
}
