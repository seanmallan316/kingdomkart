// sfx.c - sound effects synthesizer. Every effect is a short recipe of a few "voices": a wave with a pitch sweep and an
// envelope. The engine, tire and wind sounds are continuous voices steered by the player's speed.
#include "sfx.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAXV 24
#define TWO_PI 6.28318530718f
enum { W_SINE, W_SAW, W_SQUARE, W_TRI, W_NOISE };

typedef struct {
    int on, wave;
    float phase, f0, f1, t, dur;      /* pitch sweeps from f0 to f1 over dur seconds */
    float att, dec;                   /* attack time, decay time (seconds) */
    float vol, pan;
    float vib_rate, vib_depth;        /* vibrato */
    float lp, lp_state;               /* one-pole low-pass (0 = off), state */
    float am_rate;                    /* amplitude flutter */
    int   delay_smp;                  /* samples to wait before starting (for arpeggios) */
    uint32_t rng;
} Voice;

static Voice V[MAXV];
static float SR = 32000.0f, ISR = 1.0f / 32000.0f;
/* request ring, game thread -> audio thread */
typedef struct { int id; float vol, pan; } Req;
static Req rq[64]; static volatile int rq_w = 0, rq_r = 0;
/* continuous sounds */
static volatile float e_speed = 0; static volatile int e_drift = 0, e_boost = 0, e_off = 0, e_air = 0, e_water = 0, e_active = 0;
static float eng_phase = 0, eng_phase2 = 0, eng_f = 40, drift_lp = 0, road_lp = 0, wind_lp = 0, eng_vol = 0, drift_vol = 0, road_vol = 0, boost_vol = 0, boost_phase = 0;
static uint32_t nrng = 0x1234567u;
static inline float frand(uint32_t *s) { *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5; return ((*s & 0xFFFFFF) / 8388608.0f) - 1.0f; }

/* ---- recorded samples (content/sfx/NAME.wav, 32 kHz 16-bit): the kart engine, stings, and replacements for synthesized effects ---- */
#ifndef KK_CONTENT_DIR
#define KK_CONTENT_DIR "/vol/content/"
#endif
typedef struct { int16_t *d; int n, ch; } Sample;
enum { SMP_IDLE, SMP_ACCEL, SMP_TOP, SMP_DECEL, SMP_START, SMP_SHIFT, SMP_POP, SMP_BRAKE, SMP_HARDBRAKE, SMP_COUNTDOWN, SMP_UNLOCK, SMP_COUNT };
static const char *SMP_NAMES[SMP_COUNT] = {"kart_engine_idle", "kart_engine_acceleration", "kart_engine_top_speed", "kart_engine_deceleration", "kart_engine_start", "kart_gear_shift", "kart_exhaust_pop", "kart_braking", "kart_hard_braking", "sting_countdown", "sting_unlock"};
static const char *SFX_NAMES[SFX_COUNT] = {"menu_move", "menu_ok", "menu_back", "count_tick", "go", "pickup", "use_item", "throw", "hit", "block", "bump", "lap", "final_lap", "finish", "win",
    "mini_turbo", "super_turbo", "boost_pad", "bounce", "land", "splash", "shofar", "fire", "locusts", "dove", "unlock", "welcome_home", "wrong_way", "coin", "crumble", "shout", "countdown", "engine_start"};
static Sample smp[SMP_COUNT], smp_fx[SFX_COUNT];
static void (*logf_)(const char *fmt, ...) = NULL;
#define LOG(...) do { if (logf_) logf_(__VA_ARGS__); } while (0)
typedef struct { int on; const Sample *s; float pos, rate, vol, pan; int loop, delay_smp; } SVoice;
#define MAXSV 10
static SVoice SV[MAXSV];           /* one-shot sample voices */
static SVoice EL[4];               /* engine layers: idle, acceleration, top speed, deceleration (looping) */
static float el_vol[4], el_want[4], el_rate[4];
static int use_samples = 0;
static volatile float g_sfx_vol = 1.0f;
void sfx_set_volume(float vol) { g_sfx_vol = vol < 0 ? 0 : (vol > 1 ? 1 : vol); }

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static int load_wav(const char *name, Sample *out) {
    char path[256]; snprintf(path, sizeof path, KK_CONTENT_DIR "sfx/%s.wav", name);
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 44 || n > 8 * 1024 * 1024) { fclose(f); return 0; }
    uint8_t *buf = malloc((size_t)n); if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); return 0; }
    fclose(f);
    if (memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) { free(buf); return 0; }
    int ch = 0, rate = 0, bits = 0; long pos = 12; const uint8_t *data = NULL; uint32_t dlen = 0;
    while (pos + 8 <= n) {
        uint32_t len = rd32(buf + pos + 4);
        if (!memcmp(buf + pos, "fmt ", 4) && len >= 16) { ch = buf[pos + 10] | (buf[pos + 11] << 8); rate = (int)rd32(buf + pos + 12); bits = buf[pos + 22] | (buf[pos + 23] << 8); }
        else if (!memcmp(buf + pos, "data", 4)) { data = buf + pos + 8; dlen = len; if (pos + 8 + (long)dlen > n) dlen = (uint32_t)(n - pos - 8); break; }
        pos += 8 + len + (len & 1);
    }
    if (!data || bits != 16 || (ch != 1 && ch != 2) || rate != (int)SR) { LOG("sample %s: unsupported format (%d ch, %d Hz, %d bit)", name, ch, rate, bits); free(buf); return 0; }
    int frames = (int)(dlen / (2 * ch));
    out->d = malloc((size_t)frames * ch * sizeof(int16_t)); if (!out->d) { free(buf); return 0; }
    for (int i = 0; i < frames * ch; i++) out->d[i] = (int16_t)(data[i * 2] | (data[i * 2 + 1] << 8));
    out->n = frames; out->ch = ch; free(buf);
    return 1;
}
void sfx_set_logger(void (*log)(const char *fmt, ...)) { logf_ = log; }
void sfx_load_samples(void) {
    int got = 0;
    for (int i = 0; i < SMP_COUNT; i++) if (!smp[i].d && load_wav(SMP_NAMES[i], &smp[i])) got++;
    for (int i = 0; i < SFX_COUNT; i++) if (!smp_fx[i].d) { char nm[48]; snprintf(nm, sizeof nm, "sfx_%s", SFX_NAMES[i]); if (load_wav(nm, &smp_fx[i])) got++; }
    use_samples = smp[SMP_IDLE].d && smp[SMP_TOP].d && smp[SMP_ACCEL].d;
    for (int i = 0; i < 4; i++) { EL[i].s = &smp[i]; EL[i].loop = 1; EL[i].on = smp[i].d != NULL; EL[i].rate = 1; }
    LOG("sound: %d recorded samples loaded, engine %s", got, use_samples ? "recorded" : "synthesized");
}
int sfx_has_sample(int id) {
    if (id < 0 || id >= SFX_COUNT) return 0;
    if (smp_fx[id].d) return 1;
    return (id == SFX_COUNTDOWN && smp[SMP_COUNTDOWN].d) || (id == SFX_ENGINE_START && smp[SMP_START].d) || (id == SFX_UNLOCK && smp[SMP_UNLOCK].d);
}
static void svoice_start(const Sample *s, float rate, float vol, float pan, float delay) {
    if (!s || !s->d) return;
    SVoice *v = NULL; float oldest = -1;
    for (int i = 0; i < MAXSV; i++) { if (!SV[i].on) { v = &SV[i]; break; } if (SV[i].pos > oldest) { oldest = SV[i].pos; v = &SV[i]; } }
    memset(v, 0, sizeof *v); v->on = 1; v->s = s; v->rate = rate; v->vol = vol; v->pan = pan; v->delay_smp = (int)(delay * SR);
}
static inline void sample_read(const Sample *s, float pos, float *l, float *r) {
    int i = (int)pos; float f = pos - i; int j = i + 1 < s->n ? i + 1 : (s->ch ? 0 : i);
    if (s->ch == 2) { *l = (s->d[i * 2] + (s->d[j * 2] - s->d[i * 2]) * f) * (1.0f / 32768.0f); *r = (s->d[i * 2 + 1] + (s->d[j * 2 + 1] - s->d[i * 2 + 1]) * f) * (1.0f / 32768.0f); }
    else { *l = *r = (s->d[i] + (s->d[j] - s->d[i]) * f) * (1.0f / 32768.0f); }
}

void sfx_init(int sample_rate) { SR = (float)sample_rate; ISR = 1.0f / SR; memset(V, 0, sizeof V); memset(SV, 0, sizeof SV); rq_w = rq_r = 0; }
/* the engine model: which recorded layer plays how loud and how fast, plus the one-shots on gear changes, braking, boosts */
static int p_active = 0, p_boost = 0, p_brake = 0, p_drift = 0; static float p_speed = 0; static int shift_cd = 0, brake_cd = 0;
void sfx_engine(float speed, int drifting, int boosting, int offroad, int in_air, int water, int active, int gas, int brake) {
    speed = speed < 0 ? -speed : speed;
    e_speed = speed; e_drift = drifting; e_boost = boosting; e_off = offroad; e_air = in_air; e_water = water; e_active = active;
    if (use_samples) {
        float s = speed > 1 ? 1 : speed;
        float up = s < 0.05f ? 0 : (s > 0.35f ? 1 : (s - 0.05f) / 0.3f), top = s < 0.45f ? 0 : (s > 0.9f ? 1 : (s - 0.45f) / 0.45f);
        int coasting = !gas && s > 0.12f && !boosting;
        el_want[0] = active ? (1.0f - (s * 2.5f > 1 ? 1 : s * 2.5f)) * 0.55f + 0.08f : 0;
        el_want[1] = active && !coasting ? up * (1.0f - top) * 0.6f : 0;
        el_want[2] = active ? top * (boosting ? 0.75f : 0.6f) : 0;
        el_want[3] = active && coasting ? 0.5f * (s < 0.3f ? (s - 0.12f) / 0.18f : 1) : 0;
        el_rate[0] = 1.0f + 0.15f * s; el_rate[1] = 0.8f + 0.6f * s; el_rate[2] = 0.85f + 0.35f * s + (boosting ? 0.1f : 0); el_rate[3] = 0.8f + 0.5f * s;
        if (shift_cd > 0) shift_cd--;
        if (brake_cd > 0) brake_cd--;
        if (active) {
            if (gas && !coasting && shift_cd == 0 && ((p_speed < 0.3f && s >= 0.3f) || (p_speed < 0.6f && s >= 0.6f))) { sfx_play(SFX_COUNT + 1, 0.45f, 0); shift_cd = 45; }
            if (p_boost && !boosting) sfx_play(SFX_COUNT + 2, 0.5f, 0);
            if (brake && !p_brake && s > 0.3f && brake_cd == 0) { sfx_play(SFX_COUNT + 3, 0.5f, 0); brake_cd = 60; }
            if (drifting && !p_drift && s > 0.5f && brake_cd == 0) { sfx_play(SFX_COUNT + 4, 0.45f, 0); brake_cd = 60; }
        }
    }
    p_active = active; p_boost = boosting; p_brake = brake; p_drift = drifting; p_speed = speed;
}
#define MAX_NAMED 128
static Sample smp_named[MAX_NAMED]; static char named_name[MAX_NAMED][48]; static int n_named = 0;
int sfx_voice_load(const char *name) {
    for (int i = 0; i < n_named; i++) if (!strcmp(named_name[i], name)) return smp_named[i].d ? i : -1;
    if (n_named >= MAX_NAMED) return -1;
    int i = n_named++; snprintf(named_name[i], sizeof named_name[i], "%s", name);
    if (!load_wav(name, &smp_named[i])) { smp_named[i].d = NULL; return -1; }
    return i;
}
void sfx_voice_play(int handle, float volume, float pan) { if (handle < 0 || handle >= n_named || !smp_named[handle].d) return; sfx_play(1000 + handle, volume, pan); }
void sfx_play(int id, float volume, float pan) {
    if (id < 0 || (id >= SFX_COUNT + 8 && id < 1000) || id >= 1000 + MAX_NAMED) return;
    int w = rq_w; if (w - rq_r >= 60) return;
    rq[w & 63].id = id; rq[w & 63].vol = volume; rq[w & 63].pan = pan;
    __sync_synchronize(); rq_w = w + 1;
}
int sfx_voices_active(void) { int n = 0; for (int i = 0; i < MAXV; i++) n += V[i].on; return n; }

static Voice *voice(int wave, float f0, float f1, float dur, float att, float dec, float vol, float pan, float delay) {
    Voice *v = NULL; float oldest = -1;
    for (int i = 0; i < MAXV; i++) { if (!V[i].on) { v = &V[i]; break; } if (V[i].t > oldest) { oldest = V[i].t; v = &V[i]; } }
    memset(v, 0, sizeof *v);
    v->on = 1; v->wave = wave; v->f0 = f0; v->f1 = f1; v->dur = dur; v->att = att; v->dec = dec; v->vol = vol; v->pan = pan;
    v->delay_smp = (int)(delay * SR); v->rng = 0x9E3779B9u ^ (uint32_t)(f0 * 100);
    return v;
}
#define N(wave, f0, f1, dur, att, dec, vol, delay) voice(wave, f0, f1, dur, att, dec, (vol) * g, p, delay)
static void fire(int id, float g, float p) {
    Voice *v;
    /* requests above SFX_COUNT are the engine's own one-shots: 1 gear shift, 2 exhaust pop, 3 braking, 4 hard braking */
    if (id >= 1000) { if (id - 1000 < n_named && smp_named[id - 1000].d) svoice_start(&smp_named[id - 1000], 1.0f, g, p, 0); return; }
    if (id > SFX_COUNT) { static const int map[5] = {0, SMP_SHIFT, SMP_POP, SMP_BRAKE, SMP_HARDBRAKE}; if (id - SFX_COUNT < 5) svoice_start(&smp[map[id - SFX_COUNT]], 1.0f, g, p, 0); return; }
    if (smp_fx[id].d) { svoice_start(&smp_fx[id], 1.0f, g, p, 0); return; }   /* a recorded replacement for this effect */
    if (id == SFX_COUNTDOWN) { svoice_start(&smp[SMP_COUNTDOWN], 1.0f, g * 0.8f, p, 0); return; }
    if (id == SFX_ENGINE_START) { svoice_start(&smp[SMP_START], 1.0f, g, p, 0); return; }
    if (id == SFX_UNLOCK && smp[SMP_UNLOCK].d) { svoice_start(&smp[SMP_UNLOCK], 1.0f, g * 0.8f, p, 0); return; }
    if (id == SFX_COUNT_TICK && smp[SMP_COUNTDOWN].d) return;   /* the recorded countdown has its own beats */
    switch (id) {
    case SFX_MENU_MOVE: N(W_SINE, 1400, 1400, 0.05f, 0.002f, 0.05f, 0.35f, 0); break;
    case SFX_MENU_OK:   N(W_SINE, 880, 880, 0.08f, 0.002f, 0.08f, 0.4f, 0); N(W_SINE, 1320, 1320, 0.2f, 0.002f, 0.2f, 0.4f, 0.08f); break;
    case SFX_MENU_BACK: N(W_SINE, 660, 440, 0.15f, 0.002f, 0.15f, 0.35f, 0); break;
    case SFX_COUNT_TICK: N(W_SQUARE, 880, 880, 0.12f, 0.002f, 0.12f, 0.35f, 0); break;
    case SFX_GO:        N(W_SQUARE, 1320, 1320, 0.5f, 0.002f, 0.5f, 0.4f, 0); N(W_SAW, 660, 660, 0.5f, 0.002f, 0.5f, 0.15f, 0); break;
    case SFX_PICKUP:    N(W_SINE, 900, 1800, 0.12f, 0.002f, 0.12f, 0.4f, 0); N(W_SINE, 1800, 2700, 0.15f, 0.002f, 0.15f, 0.3f, 0.1f); break;
    case SFX_USE_ITEM:  v = N(W_NOISE, 0, 0, 0.25f, 0.01f, 0.25f, 0.4f, 0); v->lp = 2500; N(W_SINE, 300, 900, 0.25f, 0.01f, 0.25f, 0.25f, 0); break;
    case SFX_THROW:     v = N(W_NOISE, 0, 0, 0.18f, 0.005f, 0.18f, 0.35f, 0); v->lp = 4000; N(W_SINE, 700, 200, 0.18f, 0.002f, 0.18f, 0.25f, 0); break;
    case SFX_HIT:       N(W_SINE, 180, 50, 0.4f, 0.002f, 0.4f, 0.8f, 0); v = N(W_NOISE, 0, 0, 0.3f, 0.002f, 0.3f, 0.5f, 0); v->lp = 1500;
                        N(W_SQUARE, 300, 80, 0.3f, 0.002f, 0.3f, 0.2f, 0); break;
    case SFX_BLOCK:     N(W_SINE, 2200, 2200, 0.25f, 0.002f, 0.25f, 0.4f, 0); N(W_SINE, 3300, 3300, 0.3f, 0.002f, 0.3f, 0.25f, 0.03f); break;
    case SFX_BUMP:      N(W_SINE, 120, 60, 0.15f, 0.002f, 0.15f, 0.5f, 0); v = N(W_NOISE, 0, 0, 0.08f, 0.002f, 0.08f, 0.2f, 0); v->lp = 800; break;
    case SFX_LAP:       N(W_SINE, 1047, 1047, 0.15f, 0.002f, 0.15f, 0.4f, 0); N(W_SINE, 1568, 1568, 0.3f, 0.002f, 0.3f, 0.4f, 0.15f); break;
    case SFX_FINAL_LAP: for (int i = 0; i < 3; i++) N(W_SQUARE, 1047 * (1 + i * 0.26f), 1047 * (1 + i * 0.26f), 0.12f, 0.002f, 0.12f, 0.3f, i * 0.13f);
                        N(W_SQUARE, 2093, 2093, 0.4f, 0.002f, 0.4f, 0.3f, 0.39f); break;
    case SFX_FINISH:    { static const float f[5] = {784, 988, 1175, 1568, 1568}; for (int i = 0; i < 5; i++) N(W_SQUARE, f[i], f[i], i == 4 ? 0.6f : 0.14f, 0.002f, i == 4 ? 0.6f : 0.14f, 0.3f, i * 0.14f); } break;
    case SFX_WIN:       { static const float f[8] = {523, 659, 784, 1047, 784, 1047, 1319, 1568}; for (int i = 0; i < 8; i++) { N(W_SQUARE, f[i], f[i], i == 7 ? 0.9f : 0.14f, 0.002f, i == 7 ? 0.9f : 0.14f, 0.3f, i * 0.14f); N(W_TRI, f[i] * 0.5f, f[i] * 0.5f, i == 7 ? 0.9f : 0.14f, 0.002f, i == 7 ? 0.9f : 0.14f, 0.25f, i * 0.14f); } } break;
    case SFX_MINI_TURBO: v = N(W_NOISE, 0, 0, 0.2f, 0.002f, 0.2f, 0.3f, 0); v->lp = 6000; N(W_SINE, 400, 1200, 0.25f, 0.002f, 0.25f, 0.3f, 0); break;
    case SFX_SUPER_TURBO: v = N(W_NOISE, 0, 0, 0.3f, 0.002f, 0.3f, 0.35f, 0); v->lp = 8000; N(W_SAW, 300, 1500, 0.35f, 0.002f, 0.35f, 0.3f, 0); N(W_SINE, 1500, 2400, 0.3f, 0.002f, 0.3f, 0.2f, 0.1f); break;
    case SFX_BOOST_PAD: N(W_SAW, 200, 1400, 0.35f, 0.002f, 0.35f, 0.3f, 0); v = N(W_NOISE, 0, 0, 0.3f, 0.002f, 0.3f, 0.25f, 0); v->lp = 5000; break;
    case SFX_BOUNCE:    v = N(W_SINE, 300, 900, 0.3f, 0.002f, 0.3f, 0.5f, 0); v->vib_rate = 12; v->vib_depth = 0.15f; break;
    case SFX_LAND:      N(W_SINE, 150, 60, 0.12f, 0.002f, 0.12f, 0.4f, 0); v = N(W_NOISE, 0, 0, 0.1f, 0.002f, 0.1f, 0.2f, 0); v->lp = 1200; break;
    case SFX_SPLASH:    v = N(W_NOISE, 0, 0, 0.5f, 0.01f, 0.5f, 0.45f, 0); v->lp = 3000; N(W_SINE, 500, 150, 0.3f, 0.002f, 0.3f, 0.2f, 0); break;
    case SFX_SHOFAR:    v = N(W_SAW, 310, 330, 1.2f, 0.08f, 1.2f, 0.45f, 0); v->vib_rate = 5.5f; v->vib_depth = 0.03f; v->lp = 2200;
                        v = N(W_SAW, 465, 495, 1.0f, 0.08f, 1.0f, 0.3f, 0.5f); v->vib_rate = 6; v->vib_depth = 0.03f; v->lp = 2500; break;
    case SFX_FIRE:      v = N(W_NOISE, 0, 0, 1.5f, 0.05f, 1.5f, 0.4f, 0); v->lp = 1800; v->am_rate = 9; N(W_SAW, 120, 480, 1.2f, 0.05f, 1.2f, 0.25f, 0); break;
    case SFX_LOCUSTS:   v = N(W_SAW, 180, 170, 1.6f, 0.1f, 1.6f, 0.25f, 0); v->am_rate = 23; v->lp = 3000; v = N(W_NOISE, 0, 0, 1.6f, 0.1f, 1.6f, 0.25f, 0); v->am_rate = 31; v->lp = 2500; break;
    case SFX_DOVE:      v = N(W_NOISE, 0, 0, 0.8f, 0.05f, 0.8f, 0.3f, 0); v->am_rate = 14; v->lp = 2000; N(W_SINE, 1800, 2600, 0.5f, 0.05f, 0.5f, 0.15f, 0); break;
    case SFX_UNLOCK:    { static const float f[4] = {1047, 1319, 1568, 2093}; for (int i = 0; i < 4; i++) N(W_SINE, f[i], f[i], i == 3 ? 1.0f : 0.25f, 0.002f, i == 3 ? 1.0f : 0.25f, 0.35f, i * 0.12f); } break;
    case SFX_WELCOME_HOME: { static const float f[4] = {523, 659, 784, 1047}; for (int i = 0; i < 4; i++) { N(W_TRI, f[i], f[i], 3.0f, 0.3f, 3.0f, 0.22f, 0); N(W_SINE, f[i] * 2, f[i] * 2, 3.0f, 0.5f, 3.0f, 0.1f, 0.3f); } } break;
    case SFX_COIN:      N(W_SQUARE, 1760, 1760, 0.05f, 0.002f, 0.05f, 0.25f, 0); N(W_SQUARE, 2349, 2349, 0.25f, 0.002f, 0.25f, 0.25f, 0.05f); break;
    case SFX_WRONG_WAY: N(W_SQUARE, 400, 400, 0.15f, 0.002f, 0.15f, 0.25f, 0); N(W_SQUARE, 300, 300, 0.2f, 0.002f, 0.2f, 0.25f, 0.18f); break;
    case SFX_CRUMBLE:   v = N(W_NOISE, 0, 0, 1.4f, 0.05f, 1.4f, 0.55f, 0); v->lp = 500; v->am_rate = 7;      /* stone tumbling: a long low rumble with thuds */
                        N(W_SINE, 70, 35, 1.2f, 0.01f, 1.2f, 0.6f, 0); N(W_SINE, 90, 40, 0.5f, 0.01f, 0.5f, 0.5f, 0.4f); N(W_SINE, 60, 30, 0.6f, 0.01f, 0.6f, 0.5f, 0.8f); break;
    case SFX_SHOUT:     { for (int i = 0; i < 5; i++) { v = N(W_SAW, 180 + i * 37, 200 + i * 37, 1.6f, 0.15f, 1.6f, 0.2f, i * 0.04f); v->vib_rate = 4 + i; v->vib_depth = 0.04f; v->lp = 1800; }   /* a crowd of voices and horns */
                        v = N(W_SAW, 310, 330, 1.6f, 0.08f, 1.6f, 0.35f, 0); v->vib_rate = 5.5f; v->vib_depth = 0.03f; v->lp = 2200; v = N(W_NOISE, 0, 0, 1.6f, 0.2f, 1.6f, 0.2f, 0); v->lp = 1200; } break;
    default: break;
    }
}

static inline float osc(Voice *v, float f) {
    v->phase += f * ISR; if (v->phase >= 1.0f) v->phase -= (int)v->phase;
    switch (v->wave) {
        case W_SINE: return sinf(v->phase * TWO_PI);
        case W_SAW: return 2.0f * v->phase - 1.0f;
        case W_SQUARE: return v->phase < 0.5f ? 0.6f : -0.6f;
        case W_TRI: return 4.0f * fabsf(v->phase - 0.5f) - 1.0f;
        default: return frand(&v->rng);
    }
}

void sfx_render(int16_t *left, int16_t *right, int n) {
    /* new requests */
    while (rq_r != rq_w) { Req *q = &rq[rq_r & 63]; fire(q->id, q->vol, q->pan); rq_r++; }
    /* smooth the continuous controls */
    float spd = e_speed, k = 0.0f;
    float want_eng = e_active ? 0.10f + 0.10f * spd : 0.0f;
    float want_drift = (e_active && e_drift && !e_air) ? 0.22f : 0.0f;
    float want_road = (e_active && !e_air) ? (e_off ? 0.18f : 0.05f) * spd : 0.0f;
    float want_boost = (e_active && e_boost) ? 0.2f : 0.0f;
    float want_f = 42.0f + spd * (e_boost ? 150.0f : 120.0f);
    for (int i = 0; i < n; i++) {
        float l = 0, r = 0;
        /* engine: a saw plus a square an octave down, through a low-pass, slightly loud on the left */
        k = 0.0005f; eng_vol += (want_eng - eng_vol) * k; drift_vol += (want_drift - drift_vol) * k; road_vol += (want_road - road_vol) * k; boost_vol += (want_boost - boost_vol) * k; eng_f += (want_f - eng_f) * 0.0004f;
        eng_phase += eng_f * ISR; if (eng_phase >= 1) eng_phase -= 1; eng_phase2 += eng_f * 0.5f * ISR; if (eng_phase2 >= 1) eng_phase2 -= 1;
        float eng = (2.0f * eng_phase - 1.0f) * 0.6f + (eng_phase2 < 0.5f ? 0.4f : -0.4f);
        road_lp += (eng - road_lp) * 0.08f; eng = road_lp * eng_vol;
        /* tires, road noise, wind, boost whoosh */
        float nz = frand(&nrng);
        drift_lp += (nz - drift_lp) * 0.25f; float drift = drift_lp * drift_vol;
        wind_lp += (nz - wind_lp) * (e_water ? 0.06f : 0.12f); float road = wind_lp * road_vol;
        boost_phase += (300.0f + 200.0f * sinf(boost_phase * 0.001f)) * ISR; float boost = (sinf(boost_phase * TWO_PI) * 0.5f + nz * 0.5f) * boost_vol;
        float cont = (use_samples ? 0 : eng) + drift + road + boost;
        l += cont * 1.0f; r += cont * 0.9f;
        if (use_samples) {
            for (int j = 0; j < 4; j++) { SVoice *e = &EL[j]; if (!e->on) continue;
                el_vol[j] += (el_want[j] - el_vol[j]) * 0.0004f; e->rate += (el_rate[j] - e->rate) * 0.0003f;
                if (el_vol[j] > 0.001f) { float sl, sr; sample_read(e->s, e->pos, &sl, &sr); l += sl * el_vol[j]; r += sr * el_vol[j] * 0.9f; }
                e->pos += e->rate; if (e->pos >= e->s->n - 1) e->pos -= (float)(e->s->n - 1); }
        }
        for (int j = 0; j < MAXSV; j++) { SVoice *sv = &SV[j]; if (!sv->on) continue;
            if (sv->delay_smp > 0) { sv->delay_smp--; continue; }
            float sl, sr; sample_read(sv->s, sv->pos, &sl, &sr);
            float pl = sv->pan < 0 ? 1.0f : 1.0f - sv->pan, pr = sv->pan > 0 ? 1.0f : 1.0f + sv->pan;
            l += sl * sv->vol * pl; r += sr * sv->vol * pr;
            sv->pos += sv->rate; if (sv->pos >= sv->s->n - 1) sv->on = 0; }
        /* one-shot voices */
        for (int j = 0; j < MAXV; j++) {
            Voice *v = &V[j]; if (!v->on) continue;
            if (v->delay_smp > 0) { v->delay_smp--; continue; }
            float u = v->dur > 0 ? v->t / v->dur : 1.0f; if (u > 1) u = 1;
            float f = v->f0 + (v->f1 - v->f0) * u;
            if (v->vib_rate > 0) f *= 1.0f + v->vib_depth * sinf(v->t * v->vib_rate * TWO_PI);
            float s = osc(v, f);
            if (v->lp > 0) { float a = v->lp * ISR * TWO_PI; if (a > 1) a = 1; v->lp_state += (s - v->lp_state) * a; s = v->lp_state; }
            float env = v->t < v->att ? v->t / v->att : (v->dur - v->t) / (v->dec > 0 ? v->dec : 0.001f);
            if (env > 1) env = 1;
            if (env < 0) env = 0;
            if (v->am_rate > 0) env *= 0.6f + 0.4f * sinf(v->t * v->am_rate * TWO_PI);
            s *= env * v->vol;
            float pl = v->pan < 0 ? 1.0f : 1.0f - v->pan, pr = v->pan > 0 ? 1.0f : 1.0f + v->pan;
            l += s * pl; r += s * pr;
            v->t += ISR; if (v->t >= v->dur) v->on = 0;
        }
        /* soft limiter */
        l = tanhf(l * 1.2f) * 0.85f * g_sfx_vol; r = tanhf(r * 1.2f) * 0.85f * g_sfx_vol;
        left[i] = (int16_t)(l * 32000.0f); right[i] = (int16_t)(r * 32000.0f);
    }
}
