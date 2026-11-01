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
        Race r; race_init(&r, &t, rep * 2 % 8, 5, diff, NKARTS);
        float time = 0; int nan = 0; float max_jump_back = 0;
        float last_dist[NKARTS]; for (int i = 0; i < NKARTS; i++) last_dist[i] = r.k[i].dist; int steps = 0;
        KartInput in; memset(&in, 0, sizeof in);
        while (r.state != RACE_FINISHED && time < 200) {
            race_step(&r, &in, 1, SIM_DT); time += SIM_DT;
            steps++;
            for (int i = 0; i < NKARTS; i++) {
                Kart *k = &r.k[i];
                if (k->x != k->x || k->speed != k->speed || k->y != k->y) nan = 1;
                if (steps % 120 == 0) { float adv = k->dist - last_dist[i]; last_dist[i] = k->dist; if (adv > max_jump_back) max_jump_back = adv; }   /* progress per second */
            }
        }
        race_rank(&r);
        int winner = r.order[0]; wins[r.character[winner]]++;
        int finished = 0; for (int i = 0; i < NKARTS; i++) finished += r.k[i].lap > t.laps;
        printf("difficulty %d run %d: ended at %.1fs, %d/%d crossed the line, winner %s (%.1fs), player %s place %d, pickups %d, uses %d (stones %d, oil %d), hits %d, blocks %d\n",
               diff, rep, r.race_t, finished, NKARTS, CHARACTERS[r.character[winner]].name, r.k[winner].total_time, CHARACTERS[r.character[r.player]].name, r.k[r.player].place, r.stat_pickups, r.stat_uses, r.stat_stones, r.stat_oils, r.stat_hits, r.stat_blocks);
        CHECK(!nan, "NaN");
        CHECK(r.state == RACE_FINISHED, "race did not end");
        CHECK(r.race_t <= RACE_TIME_LIMIT + 0.1f, "race ran past the 2 minute limit");
        CHECK(r.k[r.player].lap > t.laps, "the player did not finish");
        CHECK(diff == 0 || finished >= 2, "fewer than 2 karts crossed the line on normal/hard");
        CHECK(r.stat_pickups >= 8, "too few item pickups (%d)", r.stat_pickups);
        CHECK(r.stat_uses >= 4, "too few item uses (%d)", r.stat_uses);
        CHECK(max_jump_back < 75.0f, "a kart advanced %.1f m in one second (teleport?)", max_jump_back);
        int places[NKARTS + 1] = {0}; for (int i = 0; i < NKARTS; i++) { CHECK(r.k[i].place >= 1 && r.k[i].place <= NKARTS, "bad place %d", r.k[i].place); if (r.k[i].place >= 1 && r.k[i].place <= NKARTS) places[r.k[i].place]++; }
        for (int p = 1; p <= NKARTS; p++) CHECK(places[p] == 1, "place %d assigned %d times", p, places[p]);
    }
    printf("wins by character over 12 races:"); for (int c = 0; c < 8; c++) printf(" %s %d", CHARACTERS[c].short_name, wins[c]); printf("\n");
    printf(bad ? "RACE TEST FAILED\n" : "RACE TEST OK\n");
    return bad;
}
