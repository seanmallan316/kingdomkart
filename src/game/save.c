// save.c - progress file. Format: one "key value" per line; unknown keys are ignored so old files keep working.
#include "save.h"
#include "../render/gfx.h"
#include <stdio.h>
#include <string.h>

Save g_save;
static char g_path[256] = "kingdomkart_save.txt";
static const float PAR_TIME[SAVE_MAX_ROADS] = {218.0f, 215.0f, 225.0f, 229.0f, 210.0f, 234.0f, 0, 0};   /* a little above what the Hard computer drivers manage */   /* time trial targets per road, seconds (0 = no target) */

void save_set_path(const char *path) { snprintf(g_path, sizeof g_path, "%s", path); }
const char *save_dir_path(char *buf, int n, const char *file) {
    const char *slash = strrchr(g_path, '/'); int len = slash ? (int)(slash - g_path + 1) : 0;
    snprintf(buf, n, "%.*s%s", len, g_path, file); return buf;
}

void save_load(void) {
    memset(&g_save, 0, sizeof g_save); g_save.music_vol = 8; g_save.sfx_vol = 10; g_save.rumble = 1; g_save.unlocks = UNLOCK_GOLIATH;   /* David vs Goliath is open from the start */
    FILE *f = fopen(g_path, "r");
    if (!f) { gfx_log("no save file at %s (first run)", g_path); return; }
    char key[32], line[128]; long val; int layout = 1;
    while (fgets(line, sizeof line, f)) {   /* one "key value" per line; the comment line at the top (and anything else) is skipped */
        if (sscanf(line, "%31s %ld", key, &val) != 2) continue;
        if (!strcmp(key, "layout")) { layout = (int)val; continue; }
        if (!strncmp(key, "best", 4)) { int i = key[4] - '0'; if (i >= 0 && i < SAVE_MAX_ROADS) g_save.best_ms[i] = (int)val; }
        else if (!strcmp(key, "races")) g_save.races = (int)val;
        else if (!strcmp(key, "wins")) g_save.wins = (int)val;
        else if (!strncmp(key, "gpwins", 6)) { int i = key[6] - '0'; if (i >= 0 && i < 3) g_save.gp_wins[i] = (int)val; }
        else if (!strcmp(key, "winchars")) g_save.win_chars = (unsigned)val;
        else if (!strcmp(key, "unlocks")) g_save.unlocks = (unsigned)val | UNLOCK_GOLIATH;
        else if (!strcmp(key, "battles")) g_save.battles = (int)val;
        else if (!strcmp(key, "battlewins")) g_save.battle_wins = (int)val;
        else if (!strcmp(key, "battlehardwins")) g_save.battle_hard_wins = (int)val;
        else if (!strcmp(key, "hardwins")) g_save.hard_wins = (int)val;
        else if (!strcmp(key, "pharaohwins")) g_save.pharaoh_wins = (int)val;
        else if (!strcmp(key, "coins")) g_save.coins = (int)val;
        else if (!strcmp(key, "musicvol")) g_save.music_vol = (int)val;
        else if (!strcmp(key, "sfxvol")) g_save.sfx_vol = (int)val;
        else if (!strcmp(key, "rumble")) g_save.rumble = (int)val;
        else if (!strcmp(key, "jericho")) g_save.jericho_ms = (int)val;
        else if (!strcmp(key, "jerichowins")) g_save.jericho_wins = (int)val;
        else if (!strcmp(key, "glorywins")) g_save.glory_wins = (int)val;
    }
    fclose(f);
    if (layout < 3) { memset(g_save.best_ms, 0, sizeof g_save.best_ms); g_save.dirty = 1; gfx_log("roads were lengthened: old best times cleared"); }   /* times from the shorter roads no longer compare */
    if (layout < 4) { g_save.best_ms[6] = 0; g_save.dirty = 1; }   /* the Walls of Jericho got shorter laps */
    if (layout < 5) { memset(g_save.best_ms, 0, sizeof g_save.best_ms); g_save.dirty = 1; gfx_log("roads were lengthened again: old best times cleared"); }
    gfx_log("save loaded: %d races, %d wins, unlocks %u", g_save.races, g_save.wins, g_save.unlocks);
}

void save_store(void) {
    if (!g_save.dirty) return;
    FILE *f = fopen(g_path, "w");
    if (!f) { gfx_log("could not write %s", g_path); return; }
    fprintf(f, "# Kingdom Kart: Road to Glory - progress. Delete this file to start over.\n");
    fprintf(f, "layout 5\n");
    for (int i = 0; i < SAVE_MAX_ROADS; i++) if (g_save.best_ms[i]) fprintf(f, "best%d %d\n", i, g_save.best_ms[i]);
    fprintf(f, "races %d\nwins %d\n", g_save.races, g_save.wins);
    for (int i = 0; i < 3; i++) fprintf(f, "gpwins%d %d\n", i, g_save.gp_wins[i]);
    fprintf(f, "winchars %u\nunlocks %u\nbattles %d\nbattlewins %d\nbattlehardwins %d\nhardwins %d\ncoins %d\npharaohwins %d\n", g_save.win_chars, g_save.unlocks, g_save.battles, g_save.battle_wins, g_save.battle_hard_wins, g_save.hard_wins, g_save.coins, g_save.pharaoh_wins);
    fprintf(f, "musicvol %d\nsfxvol %d\nrumble %d\n", g_save.music_vol, g_save.sfx_vol, g_save.rumble);
    if (g_save.jericho_ms) fprintf(f, "jericho %d\n", g_save.jericho_ms);
    fprintf(f, "jerichowins %d\nglorywins %d\n", g_save.jericho_wins, g_save.glory_wins);
    fclose(f);
    g_save.dirty = 0;
}

unsigned save_check_unlocks(void) {
    unsigned want = g_save.unlocks;
    if (g_save.wins >= 5) want |= UNLOCK_SAMSON;
    if (g_save.gp_wins[0] + g_save.gp_wins[1] + g_save.gp_wins[2] >= 1) want |= UNLOCK_NOAH;
    if ((g_save.win_chars & 0x7F) == 0x7F) want |= UNLOCK_JOSEPH;
    if (PAR_TIME[1] > 0 && g_save.best_ms[1] > 0 && g_save.best_ms[1] <= (int)(PAR_TIME[1] * 1000)) want |= UNLOCK_JONAH;
    if (g_save.gp_wins[2] >= 1) want |= UNLOCK_JOHN;
    if (g_save.gp_wins[1] + g_save.gp_wins[2] >= 1) want |= UNLOCK_HEAVEN;
    if (g_save.wins >= 10) want |= UNLOCK_KART_CHARIOT;
    int all_par = 1; for (int i = 0; i < SAVE_MAX_ROADS; i++) if (PAR_TIME[i] > 0 && (g_save.best_ms[i] == 0 || g_save.best_ms[i] > (int)(PAR_TIME[i] * 1000))) all_par = 0;
    if (all_par) want |= UNLOCK_KART_BOAT;
    if (g_save.races >= 25) want |= UNLOCK_KART_ARK;
    if (g_save.win_chars & (1u << 2)) want |= UNLOCK_GOLIATH;
    if (g_save.battle_wins >= 1) want |= UNLOCK_KART_SLING;
    if (g_save.battle_hard_wins >= 1) want |= UNLOCK_GOLIATH_RACER;
    if (g_save.hard_wins >= 3) want |= UNLOCK_GIDEON;
    if (g_save.coins >= 300) want |= UNLOCK_ZACCHAEUS;
    if (g_save.wins >= 14) want |= UNLOCK_JACOB;
    if (g_save.gp_wins[0] + g_save.gp_wins[1] + g_save.gp_wins[2] >= 3) want |= UNLOCK_ISAAC;
    if (g_save.races >= 100) want |= UNLOCK_ABRAHAM;
    if (g_save.pharaoh_wins >= 1) want |= UNLOCK_PHARAOH;
    if (g_save.jericho_wins >= 1) want |= UNLOCK_MICHAEL;
    if (g_save.glory_wins >= 1) want |= UNLOCK_GABRIEL;
    unsigned fresh = want & ~g_save.unlocks;
    if (fresh) { g_save.unlocks = want; g_save.dirty = 1; }
    return fresh;
}

float save_par_time(int road) { return road >= 0 && road < SAVE_MAX_ROADS ? PAR_TIME[road] : 0; }

const char *unlock_name(int bit) {
    switch (bit) {
        case UNLOCK_SAMSON: return "SAMSON"; case UNLOCK_NOAH: return "NOAH"; case UNLOCK_JOSEPH: return "JOSEPH"; case UNLOCK_JONAH: return "JONAH"; case UNLOCK_JOHN: return "JOHN THE BAPTIST"; case UNLOCK_HEAVEN: return "ROAD TO HEAVEN";
        case UNLOCK_KART_CHARIOT: return "CHARIOT KART"; case UNLOCK_KART_BOAT: return "GALILEE BOAT KART"; case UNLOCK_KART_ARK: return "ARK KART";
        case UNLOCK_GOLIATH: return "DAVID VS GOLIATH"; case UNLOCK_KART_SLING: return "SLING KART";
        case UNLOCK_MICHAEL: return "MICHAEL THE ARCHANGEL"; case UNLOCK_GABRIEL: return "GABRIEL THE ARCHANGEL"; case UNLOCK_GOLIATH_RACER: return "GOLIATH"; case UNLOCK_GIDEON: return "GIDEON"; case UNLOCK_ZACCHAEUS: return "ZACCHAEUS"; case UNLOCK_JACOB: return "JACOB"; case UNLOCK_ISAAC: return "ISAAC"; case UNLOCK_ABRAHAM: return "ABRAHAM"; case UNLOCK_PHARAOH: return "PHARAOH";
        default: return "";
    }
}
const char *unlock_hint(int bit) {
    switch (bit) {
        case UNLOCK_SAMSON: return "WIN 5 RACES"; case UNLOCK_NOAH: return "WIN A GRAND PRIX"; case UNLOCK_JOSEPH: return "WIN WITH ALL 7 CHARACTERS";
        case UNLOCK_JONAH: return "BEAT THE TARGET ON THE SEA OF GALILEE"; case UNLOCK_JOHN: return "WIN THE GRAND PRIX ON HARD"; case UNLOCK_HEAVEN: return "WIN THE GRAND PRIX ON NORMAL";
        case UNLOCK_KART_CHARIOT: return "WIN 10 RACES"; case UNLOCK_KART_BOAT: return "BEAT THE TARGET TIME ON EVERY ROAD"; case UNLOCK_KART_ARK: return "FINISH 25 RACES";
        case UNLOCK_GOLIATH: return "WIN A RACE AS DAVID"; case UNLOCK_KART_SLING: return "BEAT GOLIATH";
        case UNLOCK_GOLIATH_RACER: return "BEAT GOLIATH ON HARD"; case UNLOCK_GIDEON: return "WIN 3 RACES ON HARD"; case UNLOCK_ZACCHAEUS: return "COLLECT 300 COINS"; case UNLOCK_JACOB: return "WIN 14 RACES"; case UNLOCK_ISAAC: return "WIN 3 GRAND PRIX CUPS"; case UNLOCK_ABRAHAM: return "FINISH 100 RACES"; case UNLOCK_PHARAOH: return "WIN THE PHARAOH'S CUP"; case UNLOCK_MICHAEL: return "WIN THE WALLS OF JERICHO"; case UNLOCK_GABRIEL: return "WIN THE GENESIS TO GLORY CUP";
        default: return "";
    }
}
