// music_test.c - plays the packed soundtrack through the music player on a PC: decodes, loops a short track across its
// crossfade, switches tracks with a fade, pauses (ducks), and writes music_preview.wav. Build with -DKK_CONTENT_DIR='"content/"'.
#include "../src/audio/music.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int bad = 0;
#define CHECK(c, ...) do { if (!(c)) { bad = 1; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
static void put32(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc((v >> 24) & 255, f); }
static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
#include <stdarg.h>
static void logger(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); printf("\n"); }
#include <stdarg.h>

static short *L, *R; static int pos, cap;
/* one simulated second: 60 game frames of music_update, 333 audio callbacks of music_mix */
static float run(float seconds, float *peak_out) {
    int n = (int)(seconds * 32000); float peak = 0; double e = 0; int frames = 0;
    for (int i = 0; i < n; i += 96) {
        if (i / 533 != (i - 96) / 533 || i == 0) { music_update(16384); frames++; }
        int m = n - i < 96 ? n - i : 96;
        memset(L + pos, 0, m * 2); memset(R + pos, 0, m * 2);
        music_mix(L + pos, R + pos, m);
        for (int j = 0; j < m; j++) { float a = fabsf(L[pos + j] / 32768.0f); if (a > peak) peak = a; e += a * a; }
        pos += m;
    }
    if (peak_out) *peak_out = peak;
    return (float)sqrt(e / n);
}

int main(void) {
    cap = 32000 * 120; L = calloc(cap, 2); R = calloc(cap, 2);
    music_set_logger(logger); music_init(32000);
    CHECK(music_available("music_menu"), "content/music/music_menu.mp3 should exist");
    CHECK(!music_available("music_nothing"), "a missing track must report missing");
    float peak;
    /* 1. the menu track (25 s) played for 60 s: it must loop (sound all the way through) */
    music_want("music_menu");
    CHECK(music_buffered() == 0, "nothing decoded before update");
    music_update(16384);
    CHECK(music_buffered() >= 16384, "update should decode ahead, got %d", music_buffered());
    float rms_a = run(5, &peak); printf("menu 0-5 s: rms %.3f peak %.2f\n", rms_a, peak);
    CHECK(rms_a > 0.02f, "menu track is silent (rms %.3f)", rms_a); CHECK(peak < 1.0f, "clipping");
    float rms_b = run(25, &peak); printf("menu 5-30 s (across the loop point): rms %.3f peak %.2f\n", rms_b, peak);
    CHECK(rms_b > 0.02f, "silent across the loop");
    float rms_c = run(30, &peak); printf("menu 30-60 s (second loop): rms %.3f peak %.2f\n", rms_c, peak);
    CHECK(rms_c > 0.02f, "silent on the second loop");
    /* check there is no silent gap anywhere in the first 60 s (a window of 100 ms with nothing in it) */
    int gaps = 0; for (int i = 32000; i + 3200 < pos; i += 1600) { float mx = 0; for (int j = 0; j < 3200; j++) { float a = fabsf(L[i + j] / 32768.0f); if (a > mx) mx = a; } if (mx < 0.002f) gaps++; }
    printf("silent 100 ms windows in the first minute: %d\n", gaps); CHECK(gaps == 0, "the loop has a gap");
    /* 2. switch to the Galilee track: the old fades out, the new fades in, never silent for long */
    music_want("music_gal"); run(0.1f, NULL); CHECK(music_now() && !strcmp(music_now(), "music_gal"), "now playing should be music_gal");
    float rms_d = run(3, &peak); printf("switch to gal: rms %.3f peak %.2f\n", rms_d, peak); CHECK(rms_d > 0.02f, "silent after switching");
    /* 3. a missing track asked for: fades to silence, no crash */
    music_want("music_nothing"); run(2, NULL); float rms_e = run(1, &peak); printf("missing track: rms %.3f\n", rms_e); CHECK(rms_e < 0.001f, "should be silent for a missing track");
    /* 4. ducking while paused */
    music_want("music_jeru"); run(2, NULL); float loud = run(2, NULL); music_set_duck(0.3f); float quiet = run(2, NULL); music_set_duck(1);
    printf("duck: %.3f -> %.3f\n", loud, quiet); CHECK(quiet < loud * 0.5f, "ducking should lower the music");
    /* 5. every track decodes */
    static const char *all[] = {"music_beth", "music_gal", "music_jeru", "music_tomb", "music_heaven", "music_elah", "music_results", "music_finallap", "music_select", "music_lose", "music_trophy", "music_credits", "music_eden", "music_jericho"};
    for (int i = 0; i < 14; i++) { if (!music_available(all[i])) { printf("%s: not provided yet\n", all[i]); continue; } music_want(all[i]); run(1.5f, NULL); float rr = run(1, &peak); printf("%-15s rms %.3f peak %.2f\n", all[i], rr, peak); CHECK(rr > 0.01f, "%s is silent", all[i]); }
    music_shutdown();
    FILE *f = fopen("music_preview.wav", "wb"); if (f) {
        unsigned data = pos * 4; fwrite("RIFF", 1, 4, f); put32(f, 36 + data); fwrite("WAVEfmt ", 1, 8, f); put32(f, 16); put16(f, 1); put16(f, 2); put32(f, 32000); put32(f, 32000 * 4); put16(f, 4); put16(f, 16);
        fwrite("data", 1, 4, f); put32(f, data); for (int i = 0; i < pos; i++) { put16(f, (unsigned short)L[i]); put16(f, (unsigned short)R[i]); } fclose(f); printf("wrote music_preview.wav (%.0f s)\n", pos / 32000.0f); }
    printf(bad ? "MUSIC TEST FAILED\n" : "MUSIC TEST OK\n"); return bad;
}
