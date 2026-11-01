// battle_test.c - David vs Goliath with the autopilot driving David: the fight must end, both sides must be able to win.
#include "../src/sim/battle.h"
#include "../src/world/theme.h"
#include <stdio.h>
#include <string.h>
static int bad = 0;
#define CHECK(c, ...) do { if (!(c)) { bad = 1; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
int main(void) {
    int arena = -1; for (int d = 0; d < NUM_TRACK_DEFS; d++) if (TRACK_DEFS[d].bonus == 2) arena = d;
    CHECK(arena >= 0, "no arena road");
    static Track t; track_from_def(&t, &TRACK_DEFS[arena]);
    int wins = 0, losses = 0;
    for (int diff = 0; diff < 3; diff++) for (int rep = 0; rep < 3; rep++) {
        static Battle b; battle_init(&b, &t, diff); b.race.rng ^= (uint32_t)(rep * 0x9E3779B9u);
        KartInput in; memset(&in, 0, sizeof in); float time = 0; int thrown = 0, ghits = 0, dhits = 0, spears = 0;
        while (!b.over && time < 130) { battle_step(&b, &in, 1, SIM_DT); time += SIM_DT;
            for (int e = 0; e < b.nev; e++) { thrown += b.ev[e] == BEV_STONE_THROWN; ghits += b.ev[e] == BEV_GOLIATH_HIT; dhits += b.ev[e] == BEV_DAVID_HIT; spears += b.ev[e] == BEV_SPEAR; } }
        printf("difficulty %d run %d: %s after %.1fs, stones thrown %d, Goliath hit %d, David hit %d, spears %d\n", diff, rep, b.won ? "DAVID WINS" : "goliath wins", b.race.race_t, thrown, ghits, dhits, spears);
        CHECK(b.over, "the battle did not end");
        CHECK(thrown > 0, "David never threw a stone");
        wins += b.won; losses += !b.won;
        float gx = b.gx, gz = b.gz; CHECK(gx == gx && gz == gz, "NaN");
    }
    printf("David won %d of 9, Goliath won %d\n", wins, losses);
    CHECK(wins >= 1, "David never wins"); CHECK(losses >= 1, "Goliath never wins (too easy)");
    printf(bad ? "BATTLE TEST FAILED\n" : "BATTLE TEST OK\n"); return bad;
}
