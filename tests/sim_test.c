// sim_test.c - drives the kart around the track on a PC with no graphics, to catch physics and lap-counting bugs.
// build: gcc -O2 -Wall -Wextra -I../src tests/sim_test.c src/world/track.c src/sim/kart.c -lm -o sim_test
#include "../src/sim/kart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
#define CHECK(c, ...) do { if (!(c)) { bad = 1; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

int main(int argc, char **argv) {
    Track t; track_make_test(&t);
    printf("track '%s': %d samples, %.0f m, %d laps, checkpoints at", t.name, t.n, t.length, t.laps);
    for (int c = 0; c < TRACK_CHECKPOINTS; c++) printf(" %d", t.checkpoint[c]);
    printf("\n");
    CHECK(t.length > 400 && t.length < 1200, "track length odd: %.0f", t.length);
    // the road must not overlap itself: any two samples far apart in index must be far apart in space
    for (int i = 0; i < t.n; i++) for (int j = i + 1; j < t.n; j++) {
        int gap = j - i; if (t.n - gap < gap) gap = t.n - gap; if (gap < 70) continue;
        float dx = t.s[i].x - t.s[j].x, dz = t.s[i].z - t.s[j].z, d = sqrtf(dx * dx + dz * dz);
        float need = (t.s[i].width + t.s[j].width) * 0.5f + 2 * SHOULDER_W;
        if (d < need) { CHECK(0, "road crosses itself near sample %d and %d (%.1f m apart, need %.1f)", i, j, d, need); i = t.n; break; }
    }
    FILE *trace = argc > 1 ? fopen(argv[1], "w") : NULL;
    for (int skill_i = 0; skill_i < 2; skill_i++) {
        float skill = skill_i ? 1.0f : 0.4f;
        Kart k; memset(&k, 0, sizeof k); kart_reset(&k, &t, 0);
        float tmax = 0, offroad = 0, wall_t = 0, air_total = 0; int jumps = 0, boosts = 0, nan = 0, wrong = 0; int was_air = 0;
        float time = 0;
        while (!k.finished && time < 240.0f) {
            KartInput in; kart_autopilot(&k, &t, &in, skill);
            kart_step(&k, &t, &in, SIM_DT); time += SIM_DT;
            if (k.speed > tmax) tmax = k.speed;
            if (!k.hit.on_road) offroad += SIM_DT;
            if (k.on_wall) wall_t += SIM_DT;
            if (k.air) { air_total += SIM_DT; if (!was_air) { jumps++; if (getenv("DEBUG")) printf("    jump at sample %d lateral %.1f speed %.1f lap %d\n", k.hit.idx, k.hit.lateral, k.speed, k.lap); } }
            was_air = k.air;
            if (k.mini_turbo_tier) boosts++;
            if (k.wrongway_t > WRONGWAY_DELAY) wrong++;
            if (k.x != k.x || k.y != k.y || k.speed != k.speed) { nan = 1; break; }
            if (trace && skill_i == 1 && ((int)(time * 120) % 6) == 0) fprintf(trace, "%.2f %.2f %.2f %.2f %.1f %d %d %d\n", time, k.x, k.y, k.z, k.speed, k.drifting, k.air, k.lap);
        }
        printf("skill %.1f: %s in %.1fs, laps:", skill, k.finished ? "finished" : "DID NOT FINISH", time);
        for (int l = 0; l < t.laps; l++) printf(" %.1f", k.lap_times[l]);
        printf("  top %.1f m/s, off-road %.1fs, on wall %.1fs, jumps %d, air %.1fs, mini-turbos %d, wrong-way frames %d%s\n", tmax, offroad, wall_t, jumps, air_total, boosts, wrong, nan ? "  NAN!" : "");
        CHECK(!nan, "NaN in physics");
        CHECK(k.finished, "autopilot did not finish");
        CHECK(k.lap_times[0] > 15 && k.lap_times[0] < 90, "lap time out of range");
        CHECK(jumps >= 2, "no jumps happened (expected the ramp each lap)");
        CHECK(skill_i == 0 || boosts >= 2, "no mini-turbos fired at full skill");
        CHECK(wrong == 0, "wrong-way warning fired while driving forward");
        CHECK(wall_t < 6.0f, "too much time against the wall");
    }
    // reversing must count backwards and trigger the wrong-way warning, then recover
    { Kart k; memset(&k, 0, sizeof k); kart_reset(&k, &t, 0); KartInput in = {0, 1, 0, 0, 0};
      for (int i = 0; i < 120 * 6; i++) kart_step(&k, &t, &in, SIM_DT);
      int lap_before = k.lap;
      k.yaw += 3.14159f; for (int i = 0; i < 120 * 6; i++) kart_step(&k, &t, &in, SIM_DT);
      CHECK(k.wrongway_t > WRONGWAY_DELAY, "wrong-way did not trigger when driving backwards (%.2f)", k.wrongway_t);
      CHECK(k.lap <= lap_before, "lap count went up while reversing");
      k.yaw += 3.14159f; for (int i = 0; i < 120 * 3; i++) kart_step(&k, &t, &in, SIM_DT);
      CHECK(k.wrongway_t < 0.5f, "wrong-way did not clear after turning around");
    }
    if (trace) fclose(trace);
    printf(bad ? "SIM TEST FAILED\n" : "SIM TEST OK\n");
    return bad;
}
