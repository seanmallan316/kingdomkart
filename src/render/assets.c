// assets.c - loads pictures packed in the game file (content/img/*.png) into textures. Missing files return NULL
// so the caller can fall back to a built-in placeholder.
#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS       /* the Wii U executable format has no thread-local storage */
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

int asset_exists(const char *relpath) { char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "%s", relpath); FILE *f = fopen(path, "rb"); if (!f) return 0; fclose(f); return 1; }
Texture *asset_texture_file(const char *relpath, int mipmaps, int *out_w, int *out_h) {
    char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "%s", relpath);
    int len = 0; unsigned char *raw = read_file(path, &len);
    if (!raw) return NULL;
    int w = 0, h = 0, comp = 0;
    unsigned char *px = stbi_load_from_memory(raw, len, &w, &h, &comp, 4);
    free(raw);
    if (!px) { gfx_log("picture %s could not be decoded: %s", relpath, stbi_failure_reason()); return NULL; }
    Texture *t = gfx_texture_rgba_ex((const uint32_t *)px, w, h, 0, 0, mipmaps);
    stbi_image_free(px);
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    return t;
}
/* a cut-out of the blue lettering in a rectangle of a picture (u, v from 0 to 1): the blue letters and their white rim
   stay, the sky behind them becomes clear, so the words can be drawn again over effects without a box round them */
Texture *asset_texture_letters(const char *name, float u0, float v0, float u1, float v1, int *out_w, int *out_h) {
    char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "img/%s.png", name);
    int len = 0; unsigned char *raw = read_file(path, &len);
    if (!raw) return NULL;
    int w = 0, h = 0, comp = 0;
    unsigned char *px = stbi_load_from_memory(raw, len, &w, &h, &comp, 4);
    free(raw);
    if (!px) return NULL;
    int x0 = (int)(u0 * w), y0 = (int)(v0 * h), cw = (int)((u1 - u0) * w), ch = (int)((v1 - v0) * h);
    if (cw <= 0 || ch <= 0 || x0 + cw > w || y0 + ch > h) { stbi_image_free(px); return NULL; }
    float *m = calloc((size_t)cw * ch, sizeof(float)); unsigned char *out = malloc((size_t)cw * ch * 4);
    if (!m || !out) { free(m); free(out); stbi_image_free(px); return NULL; }
    for (int y = 0; y < ch; y++) for (int x = 0; x < cw; x++) { const unsigned char *p = px + ((size_t)(y0 + y) * w + x0 + x) * 4;
        float blue = ((float)p[2] - (float)p[0] - 40.0f) / 70.0f; m[y * cw + x] = blue < 0 ? 0 : (blue > 1 ? 1 : blue); }
    int rad = (int)(cw / 120.0f) + 2;   /* grow the letters a little so their white rim comes too */
    for (int y = 0; y < ch; y++) for (int x = 0; x < cw; x++) { float a = 0;
        for (int dy = -rad; dy <= rad; dy++) for (int dx = -rad; dx <= rad; dx++) { int xx = x + dx, yy = y + dy; if (xx < 0 || yy < 0 || xx >= cw || yy >= ch || dx * dx + dy * dy > rad * rad) continue;
            float v = m[yy * cw + xx] * (1.0f - 0.35f * sqrtf((float)(dx * dx + dy * dy)) / rad); if (v > a) a = v; }
        const unsigned char *p = px + ((size_t)(y0 + y) * w + x0 + x) * 4; unsigned char *o = out + ((size_t)y * cw + x) * 4;
        o[0] = p[0]; o[1] = p[1]; o[2] = p[2]; o[3] = (unsigned char)(a * 255.0f); }
    Texture *t = gfx_texture_rgba((const uint32_t *)out, cw, ch, 0, 0);
    free(m); free(out); stbi_image_free(px);
    if (out_w) *out_w = cw;
    if (out_h) *out_h = ch;
    return t;
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

// ------------------------------------------------------------------ video: frames decoded ahead on the other two CPU cores
/* Three helper threads (one each on the Wii U cores 0 and 2, and one on core 1 that only runs while the game is idle)
   read each compressed frame ahead of time and turn it into pixels; the main loop puts the newest finished frame
   on screen. A frame that would show up late is skipped, so the picture keeps time with the music. */
#ifdef __WIIU__
#include <coreinit/thread.h>
#include <coreinit/mutex.h>
#include <coreinit/time.h>
#include <malloc.h>
typedef OSMutex VidLock;
static void vlock_init(VidLock *m) { OSInitMutex(m); }
static void vlock(VidLock *m) { OSLockMutex(m); }
static void vunlock(VidLock *m) { OSUnlockMutex(m); }
static void vsleep_ms(int ms) { OSSleepTicks(OSMillisecondsToTicks(ms)); }
#else
#include <pthread.h>
#include <unistd.h>
#include <time.h>
typedef pthread_mutex_t VidLock;
static void vlock_init(VidLock *m) { pthread_mutex_init(m, NULL); }
static void vlock(VidLock *m) { pthread_mutex_lock(m); }
static void vunlock(VidLock *m) { pthread_mutex_unlock(m); }
static void vsleep_ms(int ms) { usleep((useconds_t)ms * 1000); }
#endif

enum { VS_EMPTY, VS_WORKING, VS_READY, VS_SHOWN, VS_RETIRED };
#define VID_SLOTS 12
#define VID_WORKERS 2
/* Each slot owns a picture in GPU-readable memory. A helper thread decodes a frame straight into a free slot's picture;
   the drawing thread only switches which slot it draws (no copying on the main thread). A slot that has just been on
   screen waits two more frames before it is reused, as the GPU may still be drawing from it. */
typedef struct { volatile int state; int frame, retired_at; Texture *tex; } VidSlot;
static struct {
    int open, nframes, w, h, shown, cur, threads, upd;
    volatile int want, next_load;
    char prefix[64];
    VidSlot slot[VID_SLOTS];
    VidLock lock;
    volatile int quit;
    volatile int decoded, dec_ms_total; int put;
#ifdef __WIIU__
    OSThread *thr[VID_WORKERS]; void *stack[VID_WORKERS];
#else
    pthread_t thr[VID_WORKERS];
#endif
} V;

static int vid_ms(void) {
#ifdef __WIIU__
    return (int)OSTicksToMilliseconds(OSGetTime());
#else
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}
static int vid_read_decode_into(int frame, Texture *tx) {   /* 1 if the frame's pixels are now in the picture */
    char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "%s%03d.jpg", V.prefix, frame);
    int len = 0; unsigned char *raw = read_file(path, &len);
    if (!raw) return 0;
    int w = 0, h = 0, comp = 0, t0 = vid_ms();
    unsigned char *px = stbi_load_from_memory(raw, len, &w, &h, &comp, 4);
#ifdef VID_TEST_DELAY
    vsleep_ms(VID_TEST_DELAY);   /* host test: pretend to be a slower CPU */
#endif
    free(raw);
    if (!px) return 0;
    int ok = 0, pitch = 0; uint8_t *dst = gfx_texture_stream_pixels(tx, &pitch);
    if (dst && w == V.w && h == V.h) { for (int y = 0; y < h; y++) memcpy(dst + (size_t)y * pitch * 4, px + (size_t)y * w * 4, (size_t)w * 4); gfx_texture_stream_flush_cpu(tx); ok = 1; }
    stbi_image_free(px);
    V.dec_ms_total += vid_ms() - t0; V.decoded++;
    return ok;
}
static int vid_work(void) {   /* a helper thread: claim the next frame and a free slot, read and decode; 0 when there was nothing to do */
    int i = -1, frame = -1;
    vlock(&V.lock);
    if (V.next_load < V.want) V.next_load = V.want;
    if (V.next_load < V.nframes && V.next_load < V.want + VID_SLOTS - 4)
        for (int k = 0; k < VID_SLOTS; k++) if (V.slot[k].state == VS_EMPTY) { i = k; frame = V.next_load++; V.slot[k].state = VS_WORKING; V.slot[k].frame = frame; break; }
    vunlock(&V.lock);
    if (i < 0) return 0;
    int ok = frame >= V.want ? vid_read_decode_into(frame, V.slot[i].tex) : 0;   /* already late: skip it */
    vlock(&V.lock);
    V.slot[i].state = ok && frame > V.shown ? VS_READY : VS_EMPTY;
    vunlock(&V.lock);
    return 1;
}
#ifdef __WIIU__
static int vid_worker(int argc, void *argv) { (void)argc; (void)argv; while (!V.quit) if (!vid_work()) vsleep_ms(2); return 0; }
#else
static void *vid_worker(void *arg) { (void)arg; while (!V.quit) if (!vid_work()) vsleep_ms(2); return NULL; }
#endif

int vid_open(const char *prefix, int nframes, int w, int h) {
    vid_close();
    memset(&V, 0, sizeof V);
    snprintf(V.prefix, sizeof V.prefix, "%s", prefix); V.nframes = nframes; V.w = w; V.h = h; V.shown = -1; V.cur = -1;
    for (int i = 0; i < VID_SLOTS; i++) { V.slot[i].tex = gfx_texture_stream(w, h);
        if (!V.slot[i].tex) { for (int j = 0; j < i; j++) gfx_texture_free(V.slot[j].tex); memset(&V, 0, sizeof V); gfx_log("video: no memory for the pictures"); return 0; } }
    vlock_init(&V.lock);
    for (int i = 0; i < VID_WORKERS; i++) {
#ifdef __WIIU__
        const uint32_t ss = 256 * 1024;
        V.thr[i] = memalign(16, sizeof(OSThread)); V.stack[i] = memalign(16, ss);
        if (!V.thr[i] || !V.stack[i]) break;
        memset(V.thr[i], 0, sizeof(OSThread));
        if (!OSCreateThread(V.thr[i], vid_worker, 0, NULL, (uint8_t *)V.stack[i] + ss, ss, 20, i == 0 ? OS_THREAD_ATTRIB_AFFINITY_CPU0 : OS_THREAD_ATTRIB_AFFINITY_CPU2)) break;
        OSSetThreadName(V.thr[i], "kk video");
        OSResumeThread(V.thr[i]);
#else
        if (pthread_create(&V.thr[i], NULL, vid_worker, NULL) != 0) break;
#endif
        V.threads++;
    }
    V.open = 1;
    gfx_log("video: %d frames %dx%d, %d decoding threads", nframes, w, h, V.threads);
    return 1;
}
int vid_update(int want) {   /* returns how many frames are decoded and waiting (for the start-up buffering) */
    if (!V.open) return 0;
    if (want >= V.nframes) want = V.nframes - 1;
    if (want < 0) want = 0;
    V.upd++;
    if (V.threads == 0) {   /* no helper threads: read and decode right here, one frame at a time, two slots in turn */
        if (V.shown != want) { int i = V.cur == 0 ? 1 : 0; if (vid_read_decode_into(want, V.slot[i].tex)) { gfx_texture_stream_gpu(V.slot[i].tex); V.cur = i; V.put++; } V.shown = want; }
        return VID_SLOTS;
    }
    int show = -1, ready = 0;
    vlock(&V.lock);
    V.want = want;
    for (int i = 0; i < VID_SLOTS; i++) { VidSlot *s = &V.slot[i];
        if (s->state == VS_READY && s->frame <= want && (show < 0 || s->frame > V.slot[show].frame)) show = i;
        if (s->state == VS_RETIRED && V.upd - s->retired_at >= 3) s->state = VS_EMPTY; }   /* the GPU is surely done with it now */
    if (show >= 0) {
        if (V.cur >= 0) { V.slot[V.cur].state = VS_RETIRED; V.slot[V.cur].retired_at = V.upd; }
        V.cur = show; V.slot[show].state = VS_SHOWN; V.shown = V.slot[show].frame; V.put++;
        for (int i = 0; i < VID_SLOTS; i++) if (V.slot[i].state == VS_READY && V.slot[i].frame < V.shown) V.slot[i].state = VS_EMPTY;   /* skipped: never drawn */
    }
    for (int i = 0; i < VID_SLOTS; i++) if (V.slot[i].state == VS_READY) ready++;
    vunlock(&V.lock);
    if (show >= 0) gfx_texture_stream_gpu(V.slot[show].tex);
    return ready;
}
Texture *vid_texture(void) { return V.open && V.cur >= 0 ? V.slot[V.cur].tex : NULL; }
void vid_close(void) {
    if (!V.open) return;
    V.quit = 1;
    for (int i = 0; i < V.threads; i++) {
#ifdef __WIIU__
        int res = 0; OSJoinThread(V.thr[i], &res);
#else
        pthread_join(V.thr[i], NULL);
#endif
    }
#ifdef __WIIU__
    for (int i = 0; i < VID_WORKERS; i++) { free(V.thr[i]); free(V.stack[i]); }
#endif
    gfx_log("video: %d of %d frames shown, %d decoded, %d ms per decode", V.put, V.nframes, V.decoded, V.decoded ? V.dec_ms_total / V.decoded : 0);
    for (int i = 0; i < VID_SLOTS; i++) gfx_texture_free(V.slot[i].tex);
    memset(&V, 0, sizeof V);
}
int vid_debug_shown(void) { return V.shown; }
