// assets.c - loads pictures packed in the game file (content/img/*.png) into textures. Missing files return NULL
// so the caller can fall back to a built-in placeholder.
#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include "../third_party/stb_image.h"

#ifndef KK_CONTENT_DIR
#define KK_CONTENT_DIR "/vol/content/"
#endif

static unsigned char *read_file(const char *path, int *len) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 16 * 1024 * 1024) { fclose(f); return NULL; }
    unsigned char *buf = malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f); fclose(f);
    if (got != (size_t)n) { free(buf); return NULL; }
    *len = (int)n; return buf;
}

Texture *asset_texture(const char *name, int repeat, int *out_w, int *out_h) {
    char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "img/%s.png", name);
    int len = 0; unsigned char *raw = read_file(path, &len);
    if (!raw) return NULL;
    int w = 0, h = 0, comp = 0;
    unsigned char *px = stbi_load_from_memory(raw, len, &w, &h, &comp, 4);
    free(raw);
    if (!px) { gfx_log("picture %s could not be decoded: %s", name, stbi_failure_reason()); return NULL; }
    if (w > 2048 || h > 2048) { gfx_log("picture %s is too large (%dx%d)", name, w, h); stbi_image_free(px); return NULL; }
    /* stb gives bytes R,G,B,A in memory; the texture wants 0xRRGGBBAA words in big-endian memory, which is the same byte order */
    Texture *t = gfx_texture_rgba((const uint32_t *)px, w, h, repeat, 0);
    stbi_image_free(px);
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    gfx_log("picture %s loaded (%dx%d)", name, w, h);
    return t;
}
