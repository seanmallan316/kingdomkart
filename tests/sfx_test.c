// sfx_test.c - renders every sound effect plus an engine run-up into sfx_preview.wav and checks levels.
#include "../src/audio/sfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int bad = 0;
#define CHECK(c, ...) do { if (!(c)) { bad = 1; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
static void put32(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc((v >> 24) & 255, f); }
static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
int main(void) {
    const int SR = 32000; sfx_init(SR); sfx_load_samples();
    static const char *names[SFX_COUNT] = {"menu move", "menu ok", "menu back", "count tick", "go", "pickup", "use item", "throw", "hit", "block", "bump", "lap", "final lap", "finish", "win",
        "mini turbo", "super turbo", "boost pad", "bounce", "land", "splash", "shofar", "chariot of fire", "locusts", "dove", "unlock", "welcome home", "wrong way", "coin", "walls crumble", "the great shout", "countdown (recorded)", "engine start (recorded)"};
    int total = SR * (SFX_COUNT * 2 + 8); short *L = calloc(total, 2), *R = calloc(total, 2); int pos = 0;
    for (int id = 0; id < SFX_COUNT; id++) {
        sfx_play(id, 1.0f, 0); int n = SR * 2; sfx_render(L + pos, R + pos, n);
        float peak = 0; double e = 0; for (int i = 0; i < n; i++) { float a = fabsf(L[pos + i] / 32768.0f); if (a > peak) peak = a; e += a * a; }
        printf("%-16s peak %.2f rms %.3f\n", names[id], peak, sqrt(e / n));
        if ((id < SFX_COUNTDOWN || sfx_has_sample(id)) && !(id == SFX_COUNT_TICK && sfx_has_sample(SFX_COUNTDOWN))) { CHECK(peak > 0.05f, "%s is silent", names[id]); } CHECK(peak < 0.95f, "%s clips", names[id]);
        pos += n;
    }
    /* engine run-up: idle, accelerate, drift, boost, off-road */
    for (int s = 0; s < 8 * 50; s++) { float t = s / 50.0f;
        sfx_engine(t < 1 ? 0 : (t < 4 ? (t - 1) / 3 : 1.0f), t > 4.5f && t < 6, t > 6 && t < 7, t > 7, 0, 0, 1, t < 7.5f, t >= 7.5f);
        int n = SR / 50; sfx_render(L + pos, R + pos, n); pos += n; }
    float peak = 0; for (int i = pos - SR * 8; i < pos; i++) { float a = fabsf(L[i] / 32768.0f); if (a > peak) peak = a; }
    printf("engine run-up peak %.2f\n", peak); CHECK(peak > 0.05f && peak < 0.95f, "engine level %.2f", peak);
    FILE *f = fopen("sfx_preview.wav", "wb"); if (f) {
        unsigned data = pos * 4; fwrite("RIFF", 1, 4, f); put32(f, 36 + data); fwrite("WAVEfmt ", 1, 8, f); put32(f, 16); put16(f, 1); put16(f, 2); put32(f, SR); put32(f, SR * 4); put16(f, 4); put16(f, 16);
        fwrite("data", 1, 4, f); put32(f, data); for (int i = 0; i < pos; i++) { put16(f, (unsigned short)L[i]); put16(f, (unsigned short)R[i]); } fclose(f); printf("wrote sfx_preview.wav (%.0f s)\n", pos / (float)SR); }
    free(L); free(R);
    printf(bad ? "SFX TEST FAILED\n" : "SFX TEST OK\n"); return bad;
}
