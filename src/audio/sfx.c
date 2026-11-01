// sfx.c - sound effects synthesizer. Every effect is a short recipe of a few "voices": a wave with a pitch sweep and an
// envelope. The engine, tire and wind sounds are continuous voices steered by the player's speed.
#include "sfx.h"
#include <math.h>
#include <string.h>

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

void sfx_init(int sample_rate) { SR = (float)sample_rate; ISR = 1.0f / SR; memset(V, 0, sizeof V); rq_w = rq_r = 0; }
void sfx_engine(float speed, int drifting, int boosting, int offroad, int in_air, int water, int active) {
    e_speed = speed < 0 ? -speed : speed; e_drift = drifting; e_boost = boosting; e_off = offroad; e_air = in_air; e_water = water; e_active = active;
}
void sfx_play(int id, float volume, float pan) {
    if (id < 0 || id >= SFX_COUNT) return;
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
    case SFX_WRONG_WAY: N(W_SQUARE, 400, 400, 0.15f, 0.002f, 0.15f, 0.25f, 0); N(W_SQUARE, 300, 300, 0.2f, 0.002f, 0.2f, 0.25f, 0.18f); break;
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
        float cont = eng + drift + road + boost;
        l += cont * 1.0f; r += cont * 0.9f;
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
        l = tanhf(l * 1.2f) * 0.85f; r = tanhf(r * 1.2f) * 0.85f;
        left[i] = (int16_t)(l * 32000.0f); right[i] = (int16_t)(r * 32000.0f);
    }
}
