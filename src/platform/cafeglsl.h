// cafeglsl.h - C loader for the CafeGLSL runtime shader compiler (glslcompiler.rpl).
// The compiler is a separate module on the SD card (sd:/wiiu/libs/glslcompiler.rpl).
// Based on the API in https://github.com/Exzap/CafeGLSL (MIT).
#ifndef KK_CAFEGLSL_H
#define KK_CAFEGLSL_H
#include <coreinit/dynload.h>
#include <gx2/shaders.h>

typedef enum { GLSL_COMPILER_FLAG_NONE = 0, GLSL_COMPILER_FLAG_GENERATE_DISASSEMBLY = 1 } GLSL_COMPILER_FLAG;

static OSDynLoad_Module glsl_module = NULL;
static void (*glsl_InitCompiler)(void) = NULL;
static void (*glsl_DestroyCompiler)(void) = NULL;
static GX2VertexShader *(*glsl_CompileVertexShader)(const char *src, char *infoLog, int infoLogMax, GLSL_COMPILER_FLAG flags) = NULL;
static GX2PixelShader *(*glsl_CompilePixelShader)(const char *src, char *infoLog, int infoLogMax, GLSL_COMPILER_FLAG flags) = NULL;
static void (*glsl_FreeVertexShader)(GX2VertexShader *s) = NULL;
static void (*glsl_FreePixelShader)(GX2PixelShader *s) = NULL;

// returns 0 on success, 1 if the module was not found, 2 if an export is missing
static int glsl_init(void) {
    if (glsl_module) return 0;
    OSDynLoad_Error r = OSDynLoad_Acquire("glslcompiler", &glsl_module);
    if (r != OS_DYNLOAD_OK) r = OSDynLoad_Acquire("~/wiiu/libs/glslcompiler.rpl", &glsl_module);
    if (r != OS_DYNLOAD_OK) { glsl_module = NULL; return 1; }
    int bad = 0;
    bad |= OSDynLoad_FindExport(glsl_module, OS_DYNLOAD_EXPORT_FUNC, "InitGLSLCompiler", (void **)&glsl_InitCompiler) != OS_DYNLOAD_OK;
    bad |= OSDynLoad_FindExport(glsl_module, OS_DYNLOAD_EXPORT_FUNC, "DestroyGLSLCompiler", (void **)&glsl_DestroyCompiler) != OS_DYNLOAD_OK;
    bad |= OSDynLoad_FindExport(glsl_module, OS_DYNLOAD_EXPORT_FUNC, "CompileVertexShader", (void **)&glsl_CompileVertexShader) != OS_DYNLOAD_OK;
    bad |= OSDynLoad_FindExport(glsl_module, OS_DYNLOAD_EXPORT_FUNC, "CompilePixelShader", (void **)&glsl_CompilePixelShader) != OS_DYNLOAD_OK;
    bad |= OSDynLoad_FindExport(glsl_module, OS_DYNLOAD_EXPORT_FUNC, "FreeVertexShader", (void **)&glsl_FreeVertexShader) != OS_DYNLOAD_OK;
    bad |= OSDynLoad_FindExport(glsl_module, OS_DYNLOAD_EXPORT_FUNC, "FreePixelShader", (void **)&glsl_FreePixelShader) != OS_DYNLOAD_OK;
    if (bad) { OSDynLoad_Release(glsl_module); glsl_module = NULL; return 2; }
    glsl_InitCompiler();
    return 0;
}
static void glsl_shutdown(void) {
    if (!glsl_module) return;
    glsl_DestroyCompiler();
    OSDynLoad_Release(glsl_module);
    glsl_module = NULL;
}
#endif
