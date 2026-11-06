// track_test.c - sanity checks for every built-in road: no self-crossing, sensible length, laps finish in time.
#include "../src/world/theme.h"
#include "../src/sim/race.h"
#include <stdio.h>
#include <string.h>
static int bad = 0;
#define CHECK(c, ...) do { if (!(c)) { bad = 1; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
int main(void) {
    for (int d = 0; d < NUM_TRACK_DEFS; d++) {
        Track t; track_from_def(&t, &TRACK_DEFS[d]);
        printf("%s: %d samples, %.0f m, %d laps, %d scenery items\n", t.name, t.n, t.length, t.laps, TRACK_DEFS[d].nscenery);
        if (TRACK_DEFS[d].bonus == 2) { printf("  (battle arena: no race checks)\n"); continue; }
        CHECK(t.length * t.laps > 1200 && t.length < 2000, "length %.0f x %d laps", t.length, t.laps);   /* a race is at least 1.2 km (Jericho: a short ring, seven times round) */
        for (int i = 0; i < t.n; i++) for (int j = i + 1; j < t.n; j++) {
            int gap = j - i; if (t.n - gap < gap) gap = t.n - gap; if (gap < 70) continue;
            float dx = t.s[i].x - t.s[j].x, dz = t.s[i].z - t.s[j].z, dd = sqrtf(dx * dx + dz * dz);
            float need = (t.s[i].width + t.s[j].width) * 0.5f + 2 * SHOULDER_W;
            if (dd < need) { CHECK(0, "road crosses itself near sample %d and %d (%.1f m apart, need %.1f)", i, j, dd, need); i = t.n; break; }
        }
        for (int s = 0; s < TRACK_DEFS[d].nscenery; s++) { const Scenery *sc = &TRACK_DEFS[d].scenery[s];
            CHECK(sc->idx >= 0 && sc->idx < t.n, "scenery %d off the track (%d)", s, sc->idx);
            CHECK(fabsf(sc->side) >= track_edge(&t, sc->idx % t.n) - 0.1f, "scenery %d (%s) stands inside the walls at sample %d (side %.1f, edge %.1f)", s, sc->img, sc->idx, sc->side, track_edge(&t, sc->idx % t.n)); }
        /* a full race must end in time on every road */
        Race r; race_init(&r, &t, 0, 5, DIFF_NORMAL, NKARTS); KartInput in; memset(&in, 0, sizeof in); float time = 0;
        while (r.state != RACE_FINISHED && time < 200) { race_step(&r, &in, 1, SIM_DT); time += SIM_DT; }
        int finished = 0; for (int i = 0; i < r.nk; i++) finished += r.k[i].lap > t.laps;
        printf("  race ended at %.1fs, %d/8 crossed the line, winner time %.1fs, pickups %d, hits %d (by obstacles %d), pads %d, items:", r.race_t, finished, r.k[r.order[0]].total_time, r.stat_pickups, r.stat_hits, r.stat_mover_hits, r.stat_pads);
        for (int it = 1; it < ITEM_COUNT; it++) printf(" %s %d", item_name(it), r.stat_item_used[it]);
        printf(", coins %d\n", r.stat_coins);
        for (int c = 0; c < t.ncoin; c++) CHECK(fabsf(t.coin[c].lateral) <= t.s[t.coin[c].idx].width * 0.5f - 0.5f, "coin %d is off the road", c);
        CHECK(t.ncoin == 0 || r.stat_coins >= 10, "too few coins collected on %s (%d)", t.name, r.stat_coins);
        for (int p = 0; p < t.npad; p++) CHECK(fabsf(t.pad[p].lateral) + t.pad[p].halfw <= t.s[t.pad[p].idx].width * 0.5f + 0.1f, "pad %d is off the road", p);
        for (int m = 0; m < t.nmover; m++) CHECK(fabsf(t.mover[m].lateral) + t.mover[m].amp <= track_edge(&t, t.mover[m].idx) + 0.1f, "obstacle %d (%s) swings outside the walls", m, t.mover[m].img);
        CHECK(t.npad == 0 || r.stat_pads > 0, "nobody touched a pad on %s", t.name);
        CHECK(r.state == RACE_FINISHED && r.race_t < RACE_TIME_LIMIT, "race on %s did not end in time", t.name);
        CHECK(finished >= 1, "nobody finished on %s", t.name);   /* the race ends 7 s after the (autopilot) player; the field behind may still be racing */
    }
    printf(bad ? "TRACK TEST FAILED\n" : "TRACK TEST OK\n");
    return bad;
}
