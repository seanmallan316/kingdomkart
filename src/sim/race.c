#include "race.h"
#include <string.h>
#include <stdio.h>

/* stats: top speed, acceleration, handling, weight (1.0 = average). Balanced so no one wins every race. */
const Character CHARACTERS[NCHARS] = {
    {"Jesus",  "JESUS",  {1.01f, 1.00f, 1.01f, 1.00f}, 0.98f, 0.98f, 0.92f},   /* white and gold */
    {"Moses",  "MOSES",  {1.04f, 0.92f, 0.94f, 1.15f}, 0.25f, 0.50f, 0.95f},   /* parted-waters blue */
    {"David",  "DAVID",  {0.97f, 1.05f, 1.06f, 0.88f}, 0.75f, 0.52f, 0.25f},   /* wood and gold */
    {"Esther", "ESTHER", {0.99f, 0.98f, 1.08f, 0.92f}, 0.60f, 0.25f, 0.75f},   /* royal purple */
    {"Peter",  "PETER",  {1.01f, 0.97f, 0.96f, 1.10f}, 0.20f, 0.42f, 0.70f},   /* fishing blue */
    {"Mary",   "MARY",   {0.96f, 1.06f, 1.05f, 0.90f}, 0.55f, 0.80f, 0.98f},   /* light blue */
    {"Daniel", "DANIEL", {1.03f, 0.94f, 1.00f, 1.05f}, 0.92f, 0.70f, 0.20f},   /* gold and blue */
    {"Samson", "SAMSON", {1.00f, 0.98f, 0.90f, 1.40f}, 0.75f, 0.20f, 0.15f, 1}, /* bonus: the strongest, heaviest of all */
    {"Noah",   "NOAH",   {0.95f, 0.96f, 1.00f, 1.25f}, 0.30f, 0.40f, 0.75f, 1}, /* bonus: steady and heavy, ark blue */
    {"Joseph", "JOSEPH", {1.00f, 1.06f, 1.02f, 0.92f}, 0.90f, 0.55f, 0.20f, 1}, /* bonus: coat of many colors, quick off the line */
    {"Jonah",  "JONAH",  {0.97f, 1.00f, 1.08f, 0.98f}, 0.25f, 0.55f, 0.90f, 1}, /* bonus: sea blue, the best handling */
    {"John",   "JOHN",   {1.08f, 1.00f, 0.94f, 1.00f}, 0.60f, 0.42f, 0.22f, 1}, /* bonus: John the Baptist, the fastest of all */
};
static unsigned g_unlocked = 0x7F;      /* the computer drives only unlocked characters */
void race_set_unlocked(unsigned mask) { g_unlocked = mask | 0x7F; }
const KartStyle KART_STYLES[NSTYLES] = {
    {"STANDARD", {1.00f, 1.00f, 1.00f, 1.00f}},
    {"CHARIOT",  {1.05f, 0.97f, 0.94f, 1.00f}},     /* faster, wider turns */
    {"BOAT",     {0.98f, 1.00f, 1.08f, 0.95f}},     /* nimble */
    {"ARK",      {0.97f, 1.06f, 0.96f, 1.25f}},     /* heavy and quick off the line */
    {"SLING",    {1.00f, 1.04f, 1.06f, 0.95f}},     /* David's: light, nimble, a little quicker */
};
static int g_player_style = 0; static unsigned g_styles = 1;
void race_set_styles(int player_style, unsigned unlocked_styles) { g_player_style = player_style; g_styles = unlocked_styles | 1; }

const char *item_name(int item) {
    switch (item) { case ITEM_MANNA: return "MANNA"; case ITEM_LOAVES: return "LOAVES AND FISHES"; case ITEM_SAMARITAN: return "GOOD SAMARITAN"; case ITEM_SHIELD: return "SHIELD OF FAITH";
                    case ITEM_PARTING: return "PARTING OF THE SEA"; case ITEM_JUBILEE: return "JUBILEE TRUMPET"; case ITEM_FIRE: return "CHARIOT OF FIRE"; case ITEM_WATER: return "LIVING WATER"; default: return ""; }
}
const char *item_verse(int item) {
    switch (item) { case ITEM_MANNA: return "EXODUS 16"; case ITEM_LOAVES: return "MATTHEW 14"; case ITEM_SAMARITAN: return "LUKE 10"; case ITEM_SHIELD: return "EPHESIANS 6";
                    case ITEM_PARTING: return "EXODUS 14"; case ITEM_JUBILEE: return "LEVITICUS 25"; case ITEM_FIRE: return "2 KINGS 2"; case ITEM_WATER: return "JOHN 4"; default: return ""; }
}
static float rr(Race *r) { r->rng ^= r->rng << 13; r->rng ^= r->rng >> 17; r->rng ^= r->rng << 5; return (r->rng & 0xFFFFFF) / 16777216.0f; }
static void push_event(Race *r, int type, int kart) { if (r->nev < 32) { r->ev[r->nev].type = type; r->ev[r->nev].kart = kart; r->nev++; } }
static float wrap_pi(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }

void race_init(Race *r, const Track *t, int player_character, int player_slot, int difficulty, int nk) {
    memset(r, 0, sizeof *r);
    if (nk < 1) nk = 1;
    if (nk > NKARTS) nk = NKARTS;
    if (player_slot >= nk) player_slot = 0;
    { int avail = 1; for (int c = 0; c < NCHARS; c++) if (c != player_character && (g_unlocked >> c) & 1) avail++; if (nk > avail) nk = avail; if (player_slot >= nk) player_slot = nk - 1; }   /* no more karts than characters to drive them */
    r->track = t; r->nk = nk; r->player = player_slot; r->difficulty = difficulty; r->rng = 0xC0FFEE11u + (uint32_t)player_character * 7919u;
    /* hand out characters: the player's pick, then the rest in order */
    int pool[NCHARS], np = 0;
    for (int c = 0; c < NCHARS; c++) if (c != player_character && (g_unlocked >> c) & 1) pool[np++] = c;
    for (int i = np - 1; i > 0; i--) { int j = (int)(rr(r) * (float)(i + 1)); if (j > i) j = i; int tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp; }   /* random grid */
    int next = 0;
    for (int i = 0; i < NKARTS; i++) r->character[i] = (i == player_slot) ? player_character : (next < np ? pool[next++] : player_character);
    for (int i = 0; i < NKARTS; i++) r->order[i] = i;
    float base = difficulty == DIFF_EASY ? 0.55f : (difficulty == DIFF_NORMAL ? 0.78f : 0.95f);
    for (int i = 0; i < r->nk; i++) {
        Kart *k = &r->k[i];
        memset(k, 0, sizeof *k);
        k->st = CHARACTERS[r->character[i]].st;
        if (i == player_slot) r->style[i] = g_player_style;
        else { int opts[NSTYLES], no = 0; for (int sty = 0; sty < NSTYLES; sty++) if ((g_styles >> sty) & 1) opts[no++] = sty; r->style[i] = opts[(int)(rr(r) * no) % no]; }
        const KartStats *m = &KART_STYLES[r->style[i]].mul; k->st.top_speed *= m->top_speed; k->st.accel *= m->accel; k->st.handling *= m->handling; k->st.weight *= m->weight;
        kart_reset(k, t, r->nk == 1 ? 0 : i);
        r->ai_skill[i] = base + (rr(r) - 0.5f) * 0.16f;
        r->ai_wobble[i] = rr(r) * 6.28f;
    }
    /* item boxes: rows of six across the road at three places */
    static const float at[3] = {0.11f, 0.42f, 0.74f};
    r->nbox = 0;
    for (int row = 0; row < 3 && nk > 1; row++) {
        int idx = (t->grid_idx + (int)(at[row] * (float)(t->n - t->grid_idx))) % t->n;   /* on a one-way road, spread over the raced part */
        for (int c = 0; c < 6; c++) {
            ItemBox *b = &r->box[r->nbox++];
            float lat = (c - 2.5f) * (t->s[idx].width * 0.16f);
            b->x = t->s[idx].x + t->s[idx].nx * lat; b->z = t->s[idx].z + t->s[idx].nz * lat; b->y = track_surface_height(t, idx, lat) + 1.0f;
            b->active = 1; b->respawn = 0; b->idx = idx;
        }
    }
    r->state = RACE_COUNTDOWN; r->cd = 3.5f; r->gas_since = -1;
    race_rank(r);
}

void race_rank(Race *r) {
    float key[NKARTS];
    for (int i = 0; i < r->nk; i++) { key[i] = r->k[i].finished ? 1e6f - (float)r->k[i].place : r->k[i].dist;   /* (1e6 keeps the places apart in float precision) */ r->order[i] = i; }
    for (int i = 1; i < r->nk; i++) { int v = r->order[i], j = i - 1; while (j >= 0 && key[r->order[j]] < key[v]) { r->order[j + 1] = r->order[j]; j--; } r->order[j + 1] = v; }
}

static int give_item(Race *r, int position) {           /* back of the pack gets the big helps */
    float x = rr(r); float behind = r->nk > 1 ? (float)position / (float)(r->nk - 1) : 0.0f;   /* 0 = leading, 1 = last */
    /*                                       none manna loaves samar shield parting jubilee fire  water */
    static const float front[ITEM_COUNT] = {0,  3.0f, 3.0f,  1.0f, 2.5f,  2.0f,   1.0f,   0.0f, 2.0f};
    static const float back[ITEM_COUNT]  = {0,  2.0f, 1.0f,  3.0f, 1.0f,  2.0f,   2.0f,   3.5f, 1.0f};
    float w[ITEM_COUNT], total = 0;
    for (int i = 1; i < ITEM_COUNT; i++) { w[i] = front[i] * (1 - behind) + back[i] * behind; total += w[i]; }
    float pick = x * total;
    for (int i = 1; i < ITEM_COUNT; i++) { pick -= w[i]; if (pick <= 0) return i; }
    return ITEM_MANNA;
}
static int rank_pos(Race *r, int i) { for (int p = 0; p < r->nk; p++) if (r->order[p] == i) return p; return 0; }
static void bless(Race *r, int who, int by) { if (who == by || by < 0 || by >= r->nk) return; r->blessed_by[who] = by; r->blessed_t[who] = 2.5f; r->given[by]++; r->stat_blessings++; push_event(r, EV_BLESSED, who); }
static void give_boost(Kart *k, float t, float mul) { if (k->stun > 0) return; k->boost_t = fmaxf(k->boost_t, t); k->boost_mul = fmaxf(k->boost_mul, mul); }

/* a piece of trail dropped on the road where the kart is (manna, or the fire of the chariot) */
static void drop_trail(Race *r, int owner, int type, float back, float life) {
    for (int i = 0; i < MAX_PROJ; i++) if (!r->proj[i].alive) {
        Proj *p = &r->proj[i]; Kart *k = &r->k[owner]; memset(p, 0, sizeof *p);
        float hx = sinf(k->yaw), hz = cosf(k->yaw);
        p->alive = 1; p->type = type; p->owner = owner; p->idx = k->hit.idx; p->x = k->x - hx * back; p->z = k->z - hz * back; p->y = k->y; p->life = life;
        return;
    }
}

static void use_item(Race *r, int i) {
    Kart *k = &r->k[i]; const Track *t = r->track; int mine = rank_pos(r, i);
    switch (k->item) {
        case ITEM_MANNA:        /* bread from heaven: a burst for you, and three pieces on the road behind for whoever comes next */
            give_boost(k, 1.7f, 1.45f); for (int n = 0; n < 3; n++) drop_trail(r, i, TRAIL_MANNA, 3.0f + n * 4.5f, 14.0f); push_event(r, EV_USE_MANNA, i); break;
        case ITEM_LOAVES:       /* feeding the five thousand: everyone gets coins, the giver twice as many */
            for (int j = 0; j < r->nk; j++) { Kart *o = &r->k[j]; int add = j == i ? 10 : 5; o->coins += add; if (o->coins > 10) o->coins = 10; if (j != i) bless(r, j, i); }
            push_event(r, EV_USE_LOAVES, i); break;
        case ITEM_SAMARITAN: {  /* the one who fell among thieves is carried: whoever is last (or you, if that is you) */
            int last = r->order[r->nk - 1]; Kart *o = &r->k[last];
            o->carried_t = 4.0f; o->stun = 0; o->slow_t = 0; o->shield = fmaxf(o->shield, 4.0f);
            if (last != i) { bless(r, last, i); give_boost(k, 1.0f, 1.25f); }
            push_event(r, EV_CARRIED, last); push_event(r, EV_USE_SAMARITAN, i); break; }
        case ITEM_SHIELD: {     /* the shield of faith, for you and for the one behind you */
            k->shield = fmaxf(k->shield, 8.0f);
            if (mine + 1 < r->nk) { int behind = r->order[mine + 1]; r->k[behind].shield = fmaxf(r->k[behind].shield, 8.0f); bless(r, behind, i); }
            push_event(r, EV_USE_SHIELD, i); break; }
        case ITEM_PARTING: {    /* the sea parts through the next corner for everyone, for ten seconds */
            r->parting_t = 10.0f; r->parting_idx = (k->hit.idx + 6) % t->n; r->parting_len = 70;
            give_boost(k, 1.2f, 1.3f);
            for (int j = 0; j < r->nk; j++) if (j != i) bless(r, j, i);
            push_event(r, EV_USE_PARTING, i); break; }
        case ITEM_JUBILEE: {    /* the year of release: every trouble cancelled, the far-behind lifted to the pack */
            float lead = r->k[r->order[0]].dist;
            for (int j = 0; j < r->nk; j++) { Kart *o = &r->k[j]; int helped = 0;
                if (o->stun > 0 || o->slow_t > 0) { o->stun = 0; o->slow_t = 0; o->spin_vis = 0; helped = 1; }
                if (lead - o->dist > 300.0f) { o->carried_t = fmaxf(o->carried_t, 3.0f); helped = 1; }
                if (helped && j != i) bless(r, j, i); }
            give_boost(k, 1.5f, 1.3f);
            push_event(r, EV_USE_JUBILEE, i); break; }
        case ITEM_FIRE:         /* the chariot of fire: blazing speed, and a fire trail that carries whoever follows */
            give_boost(k, 4.0f, 1.5f); k->shield = fmaxf(k->shield, 4.0f); r->fire_drop[i] = 0.0f; push_event(r, EV_USE_FIRE, i); break;
        case ITEM_WATER: {      /* living water: made whole, and the two nearest racers with you */
            k->stun = 0; k->slow_t = 0; k->spin_vis = 0; give_boost(k, 1.0f, 1.25f);
            int near[2] = {-1, -1}; float nd[2] = {1e9f, 1e9f};
            for (int j = 0; j < r->nk; j++) { if (j == i) continue; Kart *o = &r->k[j]; float dx = o->x - k->x, dz = o->z - k->z, d = dx * dx + dz * dz;
                if (d < nd[0]) { nd[1] = nd[0]; near[1] = near[0]; nd[0] = d; near[0] = j; } else if (d < nd[1]) { nd[1] = d; near[1] = j; } }
            for (int n = 0; n < 2; n++) if (near[n] >= 0) { Kart *o = &r->k[near[n]]; o->stun = 0; o->slow_t = 0; o->spin_vis = 0; give_boost(o, 1.0f, 1.15f); bless(r, near[n], i); }
            push_event(r, EV_USE_WATER, i); break; }
        default: return;
    }
    r->stat_uses++; r->stat_item_used[k->item]++;
    k->item = ITEM_NONE; k->item_hold = 0;
}

/* AI decides whether to use its item now */
static int ai_wants_item(Race *r, int i) {
    Kart *k = &r->k[i]; const Track *t = r->track;
    if (!k->item || k->stun > 0) return 0;
    int look = 12; const TrackSample *a = &t->s[(k->hit.idx + look) % t->n], *b = &t->s[(k->hit.idx + look * 2) % t->n];
    float curve = fabsf(wrap_pi(atan2f(b->tx, b->tz) - atan2f(a->tx, a->tz)));
    switch (k->item) {
        case ITEM_MANNA: return curve < 0.2f && !k->air;
        case ITEM_LOAVES: return k->item_hold > 1.0f;
        case ITEM_SAMARITAN: return k->item_hold > (rank_pos(r, i) == r->nk - 1 ? 0.3f : 1.5f);
        case ITEM_SHIELD: return k->item_hold > 2.5f;
        case ITEM_PARTING: return curve > 0.35f && r->parting_t <= 0;
        case ITEM_JUBILEE: return k->item_hold > 3.0f || k->slow_t > 0;
        case ITEM_FIRE: return k->item_hold > 1.0f && !k->air && curve < 0.3f;
        case ITEM_WATER: return k->slow_t > 0 || k->item_hold > 6.0f;
    }
    return 0;
}

void race_step(Race *r, const KartInput *player_in, int player_auto, float dt) {
    const Track *t = r->track;
    r->nev = 0;
    if (r->state == RACE_COUNTDOWN) {
        int before = (int)ceilf(r->cd - 0.5f);
        r->cd -= dt;
        int after = (int)ceilf(r->cd - 0.5f);
        if (after != before && after >= 1 && after <= 3) push_event(r, EV_COUNT_TICK, -1);
        /* rocket start: hold gas from the last half second before GO; hold it from too early and the wheels spin */
        int gas = player_in && player_in->gas && !player_auto;
        if (!gas) r->gas_since = -1; else if (r->gas_since < 0) r->gas_since = r->cd + dt;
        if (r->cd <= 0) { r->state = RACE_RUNNING; push_event(r, EV_GO, -1);
            Kart *pk = &r->k[r->player];
            if (gas && r->gas_since <= 0.65f) { pk->boost_t = 1.1f; pk->boost_mul = 1.32f; pk->rocket = 1; push_event(r, EV_ROCKET_START, r->player); }
            else if (gas && r->gas_since > 1.5f) { pk->slow_t = 1.0f; pk->rocket = -1; push_event(r, EV_WHEELSPIN, r->player); }
            /* some computer drivers get a rocket start too, more of them on Hard */
            for (int i = 0; i < r->nk; i++) if (i != r->player && rr(r) < 0.15f + 0.2f * r->difficulty) { r->k[i].boost_t = 1.0f; r->k[i].boost_mul = 1.25f; }
        }
        KartInput none; memset(&none, 0, sizeof none);
        for (int i = 0; i < r->nk; i++) { kart_step(&r->k[i], t, &none, dt); r->k[i].total_time = 0; }
        return;
    }
    if (r->state == RACE_FINISHED) { r->after_t += dt; return; }
    r->race_t += dt;
    race_rank(r);
    int rank_of[NKARTS]; for (int p = 0; p < r->nk; p++) rank_of[r->order[p]] = p;

    for (int i = 0; i < r->nk; i++) {
        Kart *k = &r->k[i];
        KartInput in;
        int ai = (i != r->player) || player_auto;
        if (ai) {
            float skill = (i == r->player) ? 0.9f : r->ai_skill[i];
            /* keep the race close without cheating: a little braver when far behind the player, a little more careful when far ahead */
            if (i != r->player) { float gap = k->dist - r->k[r->player].dist; if (gap < -60) skill += 0.08f; else if (gap > 60) skill -= 0.10f; }
            kart_autopilot(k, t, &in, clampf(skill, 0.3f, 1.0f));
            r->ai_wobble[i] += dt * 0.7f;
            if (k->finished) { in.steer *= 0.5f; in.gas = k->speed < 14.0f; }
            /* good drivers do a trick off big jumps */
            if (k->air && k->air_t > 0.2f && k->hop <= 0 && !k->trick && skill > 0.4f && rr(r) < 0.08f) in.drift = 1;
            else in.steer += 0.06f * sinf(r->ai_wobble[i]) * (1.0f - skill);      /* human-like imperfection */
            /* drift toward a coin just ahead (only a nudge, so the racing line still wins) */
            if (k->coins < 10 && !k->air) { for (int c = 0; c < t->ncoin; c++) { if (r->coin_respawn[c] > 0) continue;
                int ahead = t->coin[c].idx - k->hit.idx; if (ahead < 0) ahead += t->n; float dist = (float)ahead * (t->length / (float)t->n);
                if (dist < 4.0f || dist > 30.0f) continue;
                float gap = t->coin[c].lateral - k->hit.lateral;
                if (fabsf(gap) > 3.5f) continue;
                in.steer = clampf(in.steer - (gap > 0 ? 1.0f : -1.0f) * 0.25f * fminf(fabsf(gap), 1.0f), -1.0f, 1.0f); break; } }
            /* steer around the obstacles crossing the road ahead (good drivers see them earlier) */
            for (int m = 0; m < t->nmover; m++) {
                const Mover *mv = &t->mover[m]; int ahead = mv->idx - k->hit.idx; if (ahead < 0) ahead += t->n;
                float dist = (float)ahead * (t->length / (float)t->n); float see = 18.0f + 22.0f * skill;
                if (dist > see || k->air) continue;
                float eta = dist / fmaxf(k->speed, 6.0f);
                float mlat = mv->lateral + mv->amp * sinf((r->race_t + eta) * mv->speed + mv->phase);
                float gap = k->hit.lateral - mlat, need = mv->radius + 2.2f;
                if (fabsf(gap) < need) { float dir = gap >= 0 ? 1.0f : -1.0f; if (fabsf(mlat + dir * need) > track_edge(t, mv->idx) - 1.0f) dir = -dir;
                    in.steer = clampf(in.steer - dir * 0.8f * (1.0f - fabsf(gap) / need), -1.0f, 1.0f); }
            }
            in.item = ai_wants_item(r, i);
        } else in = *player_in;
        if (in.item && k->item) use_item(r, i);
        kart_step(k, t, &in, dt);
        if (k->finished && k->place == 0) { k->place = ++r->nfin; push_event(r, EV_FINISH, i); }
        /* lap events */
        if (k->lap >= 2 && k->lap_time < dt * 1.5f && !k->finished) push_event(r, k->lap == t->laps ? EV_FINAL_LAP : EV_LAP, i);
    }

    /* item boxes */
    for (int b = 0; b < r->nbox; b++) {
        ItemBox *bx = &r->box[b];
        if (!bx->active) { bx->respawn -= dt; if (bx->respawn <= 0) bx->active = 1; continue; }
        for (int i = 0; i < r->nk; i++) {
            Kart *k = &r->k[i]; if (k->item || k->finished) continue;
            float dx = k->x - bx->x, dz = k->z - bx->z;
            if (dx * dx + dz * dz < 2.0f * 2.0f && fabsf(k->y - bx->y) < 3.0f) {
                k->item = give_item(r, rank_of[i]); k->item_hold = 0; bx->active = 0; bx->respawn = 4.0f;
                r->stat_pickups++; push_event(r, EV_PICKUP, i); break;
            }
        }
    }
    /* coins */
    for (int c = 0; c < t->ncoin; c++) {
        if (r->coin_respawn[c] > 0) { r->coin_respawn[c] -= dt; continue; }
        const Coin *cn = &t->coin[c]; const TrackSample *s = &t->s[cn->idx];
        float cx = s->x + s->nx * cn->lateral, cz = s->z + s->nz * cn->lateral;
        for (int i = 0; i < r->nk; i++) { Kart *k = &r->k[i]; if (k->coins >= 10 || k->finished) continue;
            float dx = k->x - cx, dz = k->z - cz;
            if (dx * dx + dz * dz < 1.6f * 1.6f && k->y < s->y + 3.0f) { k->coins++; r->coin_respawn[c] = 10.0f; r->stat_coins++; push_event(r, EV_COIN, i); break; } }
    }
    /* the parted sea: karts inside the window drive the shoulders as if they were road */
    if (r->parting_t > 0) { r->parting_t -= dt;
        for (int i = 0; i < r->nk; i++) { Kart *k = &r->k[i]; int rel = (k->hit.idx - r->parting_idx + t->n) % t->n; if (rel < r->parting_len) k->grace_t = 0.3f; } }
    /* a blazing chariot drops fire behind it */
    for (int i = 0; i < r->nk; i++) { Kart *k = &r->k[i]; if (k->boost_t > 0 && k->boost_mul >= 1.5f && k->shield > 0) { r->fire_drop[i] -= dt; if (r->fire_drop[i] <= 0) { drop_trail(r, i, TRAIL_FIRE, 2.0f, 6.0f); r->fire_drop[i] = 0.35f; } } }
    for (int i = 0; i < r->nk; i++) if (r->blessed_t[i] > 0) r->blessed_t[i] -= dt;
    /* trails on the road: manna and fire give a boost to whoever drives through (not the one who left them) */
    for (int p = 0; p < MAX_PROJ; p++) {
        Proj *pr = &r->proj[p]; if (!pr->alive) continue;
        pr->age += dt; pr->life -= dt;
        if (pr->life <= 0) { pr->alive = 0; continue; }
        for (int i = 0; i < r->nk; i++) {
            Kart *k = &r->k[i]; if (i == pr->owner || k->finished) continue;
            float dx = k->x - pr->x, dz = k->z - pr->z;
            if (dx * dx + dz * dz < 1.8f * 1.8f && fabsf(k->y - pr->y) < 2.5f) {
                if (pr->type == TRAIL_MANNA) { float behind = r->nk > 1 ? (float)rank_pos(r, i) / (float)(r->nk - 1) : 0; give_boost(k, 1.0f + behind, 1.25f + 0.25f * behind); }
                else give_boost(k, 2.0f, 1.3f);
                bless(r, i, pr->owner); r->stat_trails++; push_event(r, EV_TRAIL_BOOST, i); pr->alive = 0; break;
            }
        }
    }
    /* things crossing the road: sheep, carts, boats, rolling stones */
    for (int m = 0; m < t->nmover; m++) {
        const Mover *mv = &t->mover[m]; float mx, my, mz; track_mover_pos(t, mv, r->race_t, &mx, &my, &mz);
        for (int i = 0; i < r->nk; i++) { Kart *k = &r->k[i]; if ((k->air && k->y > my + 1.5f) || k->obstacle_cd > 0 || k->stun > 0) continue;
            float dx = k->x - mx, dz = k->z - mz, d = sqrtf(dx * dx + dz * dz);
            if (d < mv->radius + 0.9f) {
                float push = (mv->radius + 0.9f - d) + 0.1f; k->x += dx / (d + 1e-3f) * push; k->z += dz / (d + 1e-3f) * push;
                int had = k->shield > 0; k->obstacle_cd = 2.0f;
                if (kart_hit(k, 0.9f)) { r->stat_hits++; r->stat_mover_hits++; push_event(r, EV_HIT, i); } else if (had) { r->stat_blocks++; push_event(r, EV_BLOCK, i); }
            } }
    }
    for (int i = 0; i < r->nk; i++) if (r->k[i].pad_flag) { r->stat_pads++; push_event(r, r->k[i].pad_flag == PAD_BOOST ? EV_PAD_BOOST : EV_PAD_BOUNCE, i); }
    for (int i = 0; i < r->nk; i++) if (r->k[i].trick_flag) push_event(r, r->k[i].trick_flag == 1 ? EV_TRICK : EV_TRICK_LAND, i);
    /* kart-to-kart bumping: push apart, heavier kart wins */
    for (int i = 0; i < r->nk; i++) for (int j = i + 1; j < r->nk; j++) {
        Kart *a = &r->k[i], *b = &r->k[j];
        float dx = b->x - a->x, dz = b->z - a->z, d = sqrtf(dx * dx + dz * dz);
        if (d < 2.2f && d > 0.01f && fabsf(a->y - b->y) < 2.0f) {
            float push = (2.2f - d), wa = a->st.weight, wb = b->st.weight, fa = wb / (wa + wb), fb = wa / (wa + wb);
            a->x -= dx / d * push * fa; a->z -= dz / d * push * fa; b->x += dx / d * push * fb; b->z += dz / d * push * fb;
            float rel = fabsf(a->speed - b->speed);
            if (rel > 4.0f) { a->speed *= 0.97f; b->speed *= 0.97f; push_event(r, EV_BUMP, i);
                /* turn the other cheek: the one who was bumped gets a little push forward */
                int bumper = a->speed > b->speed ? i : j, bumped = bumper == i ? j : i; Kart *v = &r->k[bumped];
                if (v->boost_t <= 0.3f) { give_boost(v, 0.6f, 1.18f); bless(r, bumped, bumper); push_event(r, EV_CHEEK, bumped); } }
        }
    }
    /* the race ends 5 s after the player finishes, when everyone finishes, or at the time limit */
    Kart *pk = &r->k[r->player];
    if (pk->finished) r->after_t += dt;
    if (r->after_t > 5.0f || r->nfin >= r->nk || r->race_t >= RACE_TIME_LIMIT) {
        /* anyone who has not finished gets a place by progress */
        race_rank(r);
        for (int p = 0; p < r->nk; p++) { Kart *k = &r->k[r->order[p]]; if (!k->finished) { k->finished = 1; k->place = ++r->nfin; } }
        r->state = RACE_FINISHED; r->after_t = 0;
    }
}
