// music.c - background music player. See music.h.
//
// How it works: the game thread calls music_update() once a frame. It decodes a little of the current MP3 with minimp3
// (one frame of 1152 samples at a time) into a ring buffer that stays about 1.5 seconds ahead. The sound thread's
// music_mix() takes samples out of the ring and adds them to the sound effects. Two "decks" exist so that one track
// can fade out while the next fades in. Each deck has two streams so a track can crossfade from its last second back
// to its first (a seamless loop). The whole MP3 file is read into memory when the deck starts (a few MB).
#include "music.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#define MINIMP3_FLOAT_OUTPUT
#include "../third_party/minimp3.h"

#ifndef KK_CONTENT_DIR
#define KK_CONTENT_DIR "/vol/content/"
#endif

#define RING_BITS 16                           /* 65536 samples per channel: two seconds at 32 kHz */
#define RING (1 << RING_BITS)
#define RING_MASK (RING - 1)
#define AHEAD 48000                            /* keep this many samples decoded ahead (1.5 s at 32 kHz) */
#define BLOCK 256                              /* samples mixed per step; gains change per block */
#define XF_SEC 1.0f                            /* crossfade from the end of a track back to its start */
#define FADE_SEC 0.6f                          /* fade between two different tracks */
#define MASTER 0.55f                           /* music level under the sound effects */

static int SR = 32000;
static int16_t ringL[RING], ringR[RING];
static volatile uint32_t ring_w = 0, ring_r = 0;
static volatile float g_duck = 1.0f, g_vol = 1.0f;
static int g_underruns = 0;
static void (*logf_)(const char *fmt, ...) = NULL;
#define LOG(...) do { if (logf_) logf_(__VA_ARGS__); } while (0)

typedef struct {
    const uint8_t *data; int size, pos;
    mp3dec_t dec;
    float pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int pcm_n, pcm_i, rate, ch, bps;           /* frames decoded, read index, sample rate, channels, bytes per second */
    float al, ar, bl, br, frac; int primed, ended;   /* linear resampler state */
} Stream;

typedef struct {
    char name[32]; uint8_t *data; int size;
    Stream s[2]; int cur, xf; float xf_u;
    float gain, target; int active;
} Deck;
static Deck D[2];

typedef struct { char name[32]; int avail; } Known;
static Known K[24]; static int nk = 0;

void music_set_logger(void (*log)(const char *fmt, ...)) { logf_ = log; }
void music_init(int sample_rate) { SR = sample_rate; memset(D, 0, sizeof D); ring_w = ring_r = 0; nk = 0; g_underruns = 0; }
void music_set_duck(float gain) { g_duck = gain < 0 ? 0 : (gain > 1 ? 1 : gain); }
void music_set_volume(float vol) { g_vol = vol < 0 ? 0 : (vol > 1 ? 1 : vol); }
int music_buffered(void) { return (int)(ring_w - ring_r); }
const char *music_now(void) {
    for (int i = 0; i < 2; i++) if (D[i].active && D[i].target > 0) return D[i].name;
    return NULL;
}

static uint8_t *read_file(const char *path, int *len) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 24 * 1024 * 1024) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f); fclose(f);
    if (got != (size_t)n) { free(buf); return NULL; }
    *len = (int)n; return buf;
}

int music_available(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < nk; i++) if (!strcmp(K[i].name, name)) return K[i].avail;
    char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "music/%s.mp3", name);
    FILE *f = fopen(path, "rb"); int ok = f != NULL; if (f) fclose(f);
    if (nk < (int)(sizeof K / sizeof K[0])) { snprintf(K[nk].name, sizeof K[nk].name, "%s", name); K[nk].avail = ok; nk++; }
    if (!ok) LOG("music: no file for %s", name);
    return ok;
}

/* ---- one MP3 stream: decode, then resample to the mixer rate with linear interpolation ---- */
static void stream_start(Stream *s, const uint8_t *data, int size) {
    memset(s, 0, sizeof *s); s->data = data; s->size = size; mp3dec_init(&s->dec);
}
static int stream_decode(Stream *s) {
    while (s->pos < s->size) {
        mp3dec_frame_info_t info; memset(&info, 0, sizeof info);
        int n = mp3dec_decode_frame(&s->dec, s->data + s->pos, s->size - s->pos, s->pcm, &info);
        if (info.frame_bytes <= 0) break;
        s->pos += info.frame_bytes;
        if (n > 0) { s->pcm_n = n; s->pcm_i = 0; s->rate = info.hz; s->ch = info.channels; if (info.bitrate_kbps > 0) s->bps = info.bitrate_kbps * 125; return 1; }
    }
    s->ended = 1; return 0;
}
static int stream_frame(Stream *s, float *l, float *r) {
    while (s->pcm_i >= s->pcm_n) if (!stream_decode(s)) return 0;
    if (s->ch >= 2) { *l = s->pcm[s->pcm_i * 2]; *r = s->pcm[s->pcm_i * 2 + 1]; } else { *l = *r = s->pcm[s->pcm_i]; }
    s->pcm_i++; return 1;
}
static int stream_out(Stream *s, float *l, float *r) {
    if (!s->primed) {
        if (!stream_frame(s, &s->al, &s->ar)) return 0;
        if (!stream_frame(s, &s->bl, &s->br)) { s->bl = s->al; s->br = s->ar; }
        s->primed = 1; s->frac = 0;
    }
    while (s->frac >= 1.0f) {
        s->al = s->bl; s->ar = s->br; s->frac -= 1.0f;
        if (!stream_frame(s, &s->bl, &s->br)) return 0;
    }
    *l = s->al + (s->bl - s->al) * s->frac; *r = s->ar + (s->br - s->ar) * s->frac;
    s->frac += s->rate > 0 ? (float)s->rate / (float)SR : 1.0f;
    return 1;
}

/* ---- a deck: the track plus its loop crossfade ---- */
static int deck_start(Deck *d, const char *name) {
    char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "music/%s.mp3", name);
    int size = 0; uint8_t *data = read_file(path, &size);
    if (!data) return 0;
    if (d->data) free(d->data);
    memset(d, 0, sizeof *d);
    snprintf(d->name, sizeof d->name, "%s", name); d->data = data; d->size = size;
    stream_start(&d->s[0], data, size); d->cur = 0; d->active = 1; d->gain = 0; d->target = 1;
    /* decode the first frame now so the log can say what the file is */
    stream_decode(&d->s[0]); d->s[0].pcm_i = 0;
    LOG("music: %s (%d Hz, %d channels, %d kbps, %d KB)", name, d->s[0].rate, d->s[0].ch, d->s[0].bps / 125, size / 1024);
    return 1;
}
static void deck_stop(Deck *d) { if (d->data) free(d->data); memset(d, 0, sizeof *d); }

/* one block of output from a deck, before its gain */
static void deck_block(Deck *d, float *l, float *r, int n) {
    for (int i = 0; i < n; i++) {
        Stream *c = &d->s[d->cur];
        float cl = 0, cr = 0; int ok = stream_out(c, &cl, &cr);
        if (!d->xf && ok && c->bps > 0 && (c->size - c->pos) < (int)(c->bps * XF_SEC) && d->size > c->bps * 3) {
            stream_start(&d->s[d->cur ^ 1], d->data, d->size); d->xf = 1; d->xf_u = 0;
        }
        if (d->xf) {
            Stream *nx = &d->s[d->cur ^ 1]; float nl = 0, nr = 0; stream_out(nx, &nl, &nr);
            float u = d->xf_u; float g1 = ok ? cosf(u * 1.5707963f) : 0, g2 = ok ? sinf(u * 1.5707963f) : 1;
            l[i] = cl * g1 + nl * g2; r[i] = cr * g1 + nr * g2;
            d->xf_u += 1.0f / (XF_SEC * SR);
            if (d->xf_u >= 1.0f || !ok) { d->cur ^= 1; d->xf = 0; }
        } else if (ok) { l[i] = cl; r[i] = cr; }
        else { stream_start(c, d->data, d->size); if (!stream_out(c, &l[i], &r[i])) { l[i] = r[i] = 0; } }   /* short file or no bitrate: plain restart */
    }
}

/* a change of track should be heard right away: throw away most of what was decoded ahead (the fading track skips a
   second, which is not noticeable under the fade) */
static void ring_catch_up(void) {
    uint32_t r = ring_r; int ahead = (int)(ring_w - r);
    if (ahead > 8000) { __sync_synchronize(); ring_w = r + 8000; }   /* keep a quarter second: a slow frame while a road loads must not run the music dry */
}
void music_want(const char *name) {
    if (!name) { for (int i = 0; i < 2; i++) if (D[i].target > 0) { D[i].target = 0; ring_catch_up(); } return; }
    for (int i = 0; i < 2; i++) if (D[i].active && !strcmp(D[i].name, name)) { if (D[i].target < 1) ring_catch_up(); D[i].target = 1; for (int j = 0; j < 2; j++) if (j != i) D[j].target = 0; return; }
    if (!music_available(name)) { for (int i = 0; i < 2; i++) if (D[i].target > 0) { D[i].target = 0; ring_catch_up(); } return; }
    ring_catch_up();
    /* fade the playing deck out, start the new one on the other deck */
    int use = -1;
    for (int i = 0; i < 2; i++) if (!D[i].active) { use = i; break; }
    if (use < 0) { use = D[0].gain <= D[1].gain ? 0 : 1; }
    for (int i = 0; i < 2; i++) if (i != use) D[i].target = 0;
    if (!deck_start(&D[use], name)) { LOG("music: could not read %s", name); for (int i = 0; i < nk; i++) if (!strcmp(K[i].name, name)) K[i].avail = 0; }
}

void music_update(int max_samples) {
    static float bl[BLOCK], br[BLOCK], dl[BLOCK], dr[BLOCK];
    int done = 0;
    float step = BLOCK / (FADE_SEC * SR);
    while ((int)(ring_w - ring_r) < AHEAD && done < max_samples) {
        memset(bl, 0, sizeof bl); memset(br, 0, sizeof br);
        for (int k = 0; k < 2; k++) {
            Deck *d = &D[k]; if (!d->active) continue;
            if (d->gain < d->target) { d->gain += step; if (d->gain > d->target) d->gain = d->target; }
            else if (d->gain > d->target) { d->gain -= step; if (d->gain < d->target) d->gain = d->target; }
            if (d->gain <= 0 && d->target <= 0) { deck_stop(d); continue; }
            deck_block(d, dl, dr, BLOCK);
            float g = d->gain * MASTER;
            for (int i = 0; i < BLOCK; i++) { bl[i] += dl[i] * g; br[i] += dr[i] * g; }
        }
        uint32_t w = ring_w;
        for (int i = 0; i < BLOCK; i++) {
            float l = bl[i] * 32767.0f, r = br[i] * 32767.0f;
            ringL[(w + i) & RING_MASK] = (int16_t)(l > 32767 ? 32767 : (l < -32768 ? -32768 : l));
            ringR[(w + i) & RING_MASK] = (int16_t)(r > 32767 ? 32767 : (r < -32768 ? -32768 : r));
        }
        __sync_synchronize(); ring_w = w + BLOCK;
        done += BLOCK;
    }
}

void music_mix(int16_t *left, int16_t *right, int n) {
    float duck = g_duck * g_vol; uint32_t r = ring_r, w = ring_w;
    int have = (int)(w - r); if (have < 0) have = 0;
    if (have < n) { if (have == 0 && music_now()) g_underruns++; }
    int m = have < n ? have : n;
    for (int i = 0; i < m; i++) {
        int l = left[i] + (int)(ringL[(r + i) & RING_MASK] * duck), rr = right[i] + (int)(ringR[(r + i) & RING_MASK] * duck);
        left[i] = (int16_t)(l > 32767 ? 32767 : (l < -32768 ? -32768 : l));
        right[i] = (int16_t)(rr > 32767 ? 32767 : (rr < -32768 ? -32768 : rr));
    }
    __sync_synchronize(); ring_r = r + m;
}

void music_shutdown(void) { for (int i = 0; i < 2; i++) deck_stop(&D[i]); if (g_underruns) LOG("music: %d buffer underruns", g_underruns); }
