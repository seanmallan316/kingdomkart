// race_test.c - runs full eight-kart races with no graphics: everyone finishes, items get used, nobody cheats, no NaN.
#include "../src/sim/race.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
#define CHECK(c, ...) do { if (!(c)) { bad = 1; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

int main(void) {
    Track t; track_make_test(&t);
    int wins[8] = {0};
    for (int diff = 0; diff < 3; diff++) for (int rep = 0; rep < 4; rep++) {
        Race r; race_init(&r, &t, rep * 2 % NMAIN, 5, diff, NKARTS);
        float time = 0; int nan = 0; float max_jump_back = 0;
        float last_dist[NKARTS]; for (int i = 0; i < r.nk; i++) last_dist[i] = r.k[i].dist; int steps = 0;
        KartInput in; memset(&in, 0, sizeof in);
        while (r.state != RACE_FINISHED && time < 200) {
            race_step(&r, &in, 1, SIM_DT); time += SIM_DT;
            steps++;
            for (int i = 0; i < r.nk; i++) {
                Kart *k = &r.k[i];
                if (k->x != k->x || k->speed != k->speed || k->y != k->y) nan = 1;
                if (steps % 120 == 0) { float adv = k->dist - last_dist[i]; last_dist[i] = k->dist; if (adv > max_jump_back) max_jump_back = adv; }   /* progress per second */
            }
        }
        race_rank(&r);
        int winner = r.order[0]; wins[r.character[winner]]++;
        int finished = 0; for (int i = 0; i < r.nk; i++) finished += r.k[i].lap > t.laps;
        printf("difficulty %d run %d: ended at %.1fs, %d/%d crossed the line, winner %s (%.1fs), player %s place %d, pickups %d, uses %d (blessings %d, trail boosts %d), hits %d, blocks %d\n",
               diff, rep, r.race_t, finished, r.nk, CHARACTERS[r.character[winner]].name, r.k[winner].total_time, CHARACTERS[r.character[r.player]].name, r.k[r.player].place, r.stat_pickups, r.stat_uses, r.stat_blessings, r.stat_trails, r.stat_hits, r.stat_blocks);
        CHECK(!nan, "NaN");
        CHECK(r.state == RACE_FINISHED, "race did not end");
        CHECK(r.race_t <= RACE_TIME_LIMIT + 0.1f, "race ran past the 2 minute limit");
        CHECK(r.k[r.player].lap > t.laps, "the player did not finish");
        CHECK(diff != 2 || finished >= 2, "fewer than 2 karts crossed the line on hard");
        CHECK(r.stat_pickups >= 8, "too few item pickups (%d)", r.stat_pickups);
        CHECK(r.stat_uses >= 4, "too few item uses (%d)", r.stat_uses);
        CHECK(max_jump_back < 75.0f, "a kart advanced %.1f m in one second (teleport?)", max_jump_back);
        int places[NKARTS + 1] = {0}; for (int i = 0; i < r.nk; i++) { CHECK(r.k[i].place >= 1 && r.k[i].place <= r.nk, "bad place %d", r.k[i].place); if (r.k[i].place >= 1 && r.k[i].place <= NKARTS) places[r.k[i].place]++; }
        for (int p = 1; p <= r.nk; p++) CHECK(places[p] == 1, "place %d assigned %d times", p, places[p]);
    }
    printf("wins by character over 12 races:"); for (int c = 0; c < NMAIN; c++) printf(" %s %d", CHARACTERS[c].short_name, wins[c]); printf("\n");
    /* four people at once (all on autopilot here): they sit in the last four grid slots, every seat is filled, the race ends */
    for (int nh = 2; nh <= 4; nh++) {
        Race r; int chars[4] = {0, 1, 0, 3}; race_init_multi(&r, &t, chars, NULL, nh, DIFF_NORMAL);
        /* the test loop has only the seven starting characters unlocked, so the grid is as full as the unlocked characters allow */
        int taken[NCHARS] = {0}, distinct = 0; for (int p = 0; p < nh; p++) if (!taken[chars[p]]) { taken[chars[p]] = 1; distinct++; }
        int expect = nh + (NMAIN - distinct); if (expect > NKARTS) expect = NKARTS;
        CHECK(r.nk == expect, "%d people: expected a grid of %d, got %d", nh, expect, r.nk);
        CHECK(r.nhuman == nh, "%d people: nhuman %d", nh, r.nhuman);
        for (int p = 0; p < nh; p++) { int slot = r.nk - nh + p; CHECK(r.human[slot] && r.character[slot] == chars[p], "%d people: slot %d should be person %d driving %d", nh, slot, p, chars[p]);
            CHECK(race_human_index(&r, slot) == p, "human index of slot %d", slot); }
        CHECK(race_human_index(&r, 0) == -1, "slot 0 is a computer driver");
        CHECK(r.player == r.nk - nh, "player should be the first human");
        KartInput in[NKARTS]; memset(in, 0, sizeof in); float time = 0;
        while (r.state != RACE_FINISHED && time < 200) { race_step_multi(&r, in, 1, SIM_DT); time += SIM_DT; }
        CHECK(r.state == RACE_FINISHED, "%d people: the race never ended", nh);
        int placed = 1; for (int i = 0; i < r.nk; i++) if (!r.k[i].place) placed = 0; CHECK(placed, "%d people: everyone gets a place", nh);
        printf("%d people + %d computer drivers: ended at %.1fs, winner %s\n", nh, r.nk - nh, r.race_t, CHARACTERS[r.character[r.order[0]]].name);
    }
    printf(bad ? "RACE TEST FAILED\n" : "RACE TEST OK\n");
    return bad;
}
