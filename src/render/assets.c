// assets.c - loads pictures packed in the game file (content/img/*.png) into textures. Missing files return NULL
// so the caller can fall back to a built-in placeholder.
#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
typedef pthread_mutex_t VidLock;
static void vlock_init(VidLock *m) { pthread_mutex_init(m, NULL); }
static void vlock(VidLock *m) { pthread_mutex_lock(m); }
static void vunlock(VidLock *m) { pthread_mutex_unlock(m); }
static void vsleep_ms(int ms) { usleep((useconds_t)ms * 1000); }
#endif

enum { VS_EMPTY, VS_WORKING, VS_READY };
#define VID_SLOTS 10
#define VID_WORKERS 3
typedef struct { volatile int state; int frame; unsigned char *px; } VidSlot;
static struct {
    int open, nframes, w, h, shown, cur_tex, threads;
    volatile int want, next_load;
    char prefix[64];
    VidSlot slot[VID_SLOTS];
    Texture *tex[2];
    VidLock lock;
    volatile int quit;
#ifdef __WIIU__
    OSThread *thr[VID_WORKERS]; void *stack[VID_WORKERS];
#else
    pthread_t thr[VID_WORKERS];
#endif
} V;

static unsigned char *vid_read_decode(int frame) {   /* the pixels of one frame, or NULL */
    char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "%s%03d.jpg", V.prefix, frame);
    int len = 0; unsigned char *raw = read_file(path, &len);
    if (!raw) return NULL;
    int w = 0, h = 0, comp = 0;
    unsigned char *px = stbi_load_from_memory(raw, len, &w, &h, &comp, 4);
#ifdef VID_TEST_DELAY
    vsleep_ms(VID_TEST_DELAY);   /* host test: pretend to be a slower CPU */
#endif
    free(raw);
    if (px && (w != V.w || h != V.h)) { stbi_image_free(px); px = NULL; }
    return px;
}
static int vid_work(void) {   /* a helper thread: claim the next frame, read and decode it; 0 when there was nothing to do */
    int i = -1, frame = -1;
    vlock(&V.lock);
    if (V.next_load < V.want) V.next_load = V.want;
    if (V.next_load < V.nframes && V.next_load < V.want + VID_SLOTS)
        for (int k = 0; k < VID_SLOTS; k++) if (V.slot[k].state == VS_EMPTY) { i = k; frame = V.next_load++; V.slot[k].state = VS_WORKING; V.slot[k].frame = frame; break; }
    vunlock(&V.lock);
    if (i < 0) return 0;
    unsigned char *px = frame >= V.want ? vid_read_decode(frame) : NULL;   /* already late: skip it */
    vlock(&V.lock);
    if (px && frame >= V.shown) { V.slot[i].px = px; V.slot[i].state = VS_READY; }
    else { if (px) stbi_image_free(px); V.slot[i].state = VS_EMPTY; }
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
    snprintf(V.prefix, sizeof V.prefix, "%s", prefix); V.nframes = nframes; V.w = w; V.h = h; V.shown = -1;
    V.tex[0] = gfx_texture_stream(w, h); V.tex[1] = gfx_texture_stream(w, h);
    if (!V.tex[0] || !V.tex[1]) { gfx_texture_free(V.tex[0]); gfx_texture_free(V.tex[1]); V.tex[0] = V.tex[1] = NULL; gfx_log("video: no memory for the picture"); return 0; }
    vlock_init(&V.lock);
    for (int i = 0; i < VID_WORKERS; i++) {
#ifdef __WIIU__
        const uint32_t ss = 256 * 1024;
        V.thr[i] = memalign(16, sizeof(OSThread)); V.stack[i] = memalign(16, ss);
        if (!V.thr[i] || !V.stack[i]) break;
        memset(V.thr[i], 0, sizeof(OSThread));
        if (!OSCreateThread(V.thr[i], vid_worker, 0, NULL, (uint8_t *)V.stack[i] + ss, ss, 20, i == 0 ? OS_THREAD_ATTRIB_AFFINITY_CPU0 : i == 1 ? OS_THREAD_ATTRIB_AFFINITY_CPU2 : OS_THREAD_ATTRIB_AFFINITY_CPU1)) break;
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
static void vid_put(const unsigned char *px, int frame) { V.cur_tex ^= 1; gfx_texture_stream_put(V.tex[V.cur_tex], (const uint32_t *)px, V.w, V.h); V.shown = frame; }
int vid_update(int want) {   /* returns how many frames are decoded and waiting (for the start-up buffering) */
    if (!V.open) return 0;
    if (want >= V.nframes) want = V.nframes - 1;
    if (want < 0) want = 0;
    if (V.threads == 0) {   /* no helper threads: read and decode right here, one frame at a time (the old way) */
        if (V.shown != want) { unsigned char *px = vid_read_decode(want); if (px) { vid_put(px, want); stbi_image_free(px); } else V.shown = want; }
        return VID_SLOTS;
    }
    int show = -1, ready = 0;
    vlock(&V.lock);
    V.want = want;
    for (int i = 0; i < VID_SLOTS; i++) { VidSlot *s = &V.slot[i];
        if (s->state == VS_READY && s->frame <= want && (show < 0 || s->frame > V.slot[show].frame)) show = i; }
    vunlock(&V.lock);
    if (show >= 0 && V.slot[show].frame != V.shown) vid_put(V.slot[show].px, V.slot[show].frame);   /* only this thread empties a ready slot, so it is safe to read */
    vlock(&V.lock);
    for (int i = 0; i < VID_SLOTS; i++) { VidSlot *s = &V.slot[i];   /* frames already passed are finished with */
        if (s->state == VS_READY && s->frame <= V.shown) { stbi_image_free(s->px); s->px = NULL; s->state = VS_EMPTY; }
        if (s->state == VS_READY) ready++; }
    vunlock(&V.lock);
    return ready;
}
Texture *vid_texture(void) { return V.open && V.shown >= 0 ? V.tex[V.cur_tex] : NULL; }
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
    for (int i = 0; i < VID_SLOTS; i++) if (V.slot[i].px) stbi_image_free(V.slot[i].px);
    gfx_texture_free(V.tex[0]); gfx_texture_free(V.tex[1]);
    memset(&V, 0, sizeof V);
}
int vid_debug_shown(void) { return V.shown; }
