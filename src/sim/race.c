#include "race.h"
#include <string.h>
#include <stdio.h>

/* stats: top speed, acceleration, handling, weight (1.0 = average). Balanced so no one wins every race. */
const Character CHARACTERS[8] = {
    {"Jesus",  "JESUS",  {1.00f, 1.00f, 1.00f, 1.00f}, 0.98f, 0.98f, 0.92f},   /* white and gold */
    {"Moses",  "MOSES",  {1.04f, 0.92f, 0.94f, 1.15f}, 0.25f, 0.50f, 0.95f},   /* parted-waters blue */
    {"David",  "DAVID",  {0.97f, 1.05f, 1.06f, 0.88f}, 0.75f, 0.52f, 0.25f},   /* wood and gold */
    {"Esther", "ESTHER", {0.99f, 0.98f, 1.08f, 0.92f}, 0.60f, 0.25f, 0.75f},   /* royal purple */
    {"Peter",  "PETER",  {1.01f, 0.97f, 0.96f, 1.10f}, 0.20f, 0.42f, 0.70f},   /* fishing blue */
    {"Paul",   "PAUL",   {0.98f, 1.08f, 0.98f, 0.98f}, 0.80f, 0.22f, 0.18f},   /* red and bronze */
    {"Mary",   "MARY",   {0.96f, 1.06f, 1.05f, 0.90f}, 0.55f, 0.80f, 0.98f},   /* light blue */
    {"Daniel", "DANIEL", {1.06f, 0.94f, 0.97f, 1.05f}, 0.92f, 0.70f, 0.20f},   /* gold and blue */
};

const char *item_name(int item) {
    switch (item) { case ITEM_MANNA: return "MANNA"; case ITEM_STAFF: return "STAFF"; case ITEM_STONE: return "STONE"; case ITEM_OIL: return "OIL JAR"; default: return ""; }
}
static float rr(Race *r) { r->rng ^= r->rng << 13; r->rng ^= r->rng >> 17; r->rng ^= r->rng << 5; return (r->rng & 0xFFFFFF) / 16777216.0f; }
static void push_event(Race *r, int type, int kart) { if (r->nev < 32) { r->ev[r->nev].type = type; r->ev[r->nev].kart = kart; r->nev++; } }
static float wrap_pi(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }

void race_init(Race *r, const Track *t, int player_character, int player_slot, int difficulty) {
    memset(r, 0, sizeof *r);
    r->track = t; r->nk = NKARTS; r->player = player_slot; r->difficulty = difficulty; r->rng = 0xC0FFEE11u + (uint32_t)player_character * 7919u;
    /* hand out characters: the player's pick, then the rest in order */
    int pool[8], np = 0;
    for (int c = 0; c < 8; c++) if (c != player_character) pool[np++] = c;
    for (int i = np - 1; i > 0; i--) { int j = (int)(rr(r) * (float)(i + 1)); if (j > i) j = i; int tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp; }   /* random grid */
    int next = 0;
    for (int i = 0; i < NKARTS; i++) r->character[i] = (i == player_slot) ? player_character : pool[next++];
    float base = difficulty == DIFF_EASY ? 0.55f : (difficulty == DIFF_NORMAL ? 0.78f : 0.95f);
    for (int i = 0; i < NKARTS; i++) {
        Kart *k = &r->k[i];
        memset(k, 0, sizeof *k);
        k->st = CHARACTERS[r->character[i]].st;
        kart_reset(k, t, i);
        r->ai_skill[i] = base + (rr(r) - 0.5f) * 0.16f;
        r->ai_wobble[i] = rr(r) * 6.28f;
    }
    /* item boxes: rows of four across the road at three places */
    static const float at[3] = {0.11f, 0.42f, 0.74f};
    r->nbox = 0;
    for (int row = 0; row < 3; row++) {
        int idx = (int)(at[row] * (float)t->n) % t->n;
        for (int c = 0; c < 4; c++) {
            ItemBox *b = &r->box[r->nbox++];
            float lat = (c - 1.5f) * (t->s[idx].width * 0.22f);
            b->x = t->s[idx].x + t->s[idx].nx * lat; b->z = t->s[idx].z + t->s[idx].nz * lat; b->y = track_surface_height(t, idx, lat) + 1.0f;
            b->active = 1; b->respawn = 0; b->idx = idx;
        }
    }
    r->state = RACE_COUNTDOWN; r->cd = 3.5f;
    race_rank(r);
}

void race_rank(Race *r) {
    float key[NKARTS];
    for (int i = 0; i < r->nk; i++) { key[i] = r->k[i].finished ? 1e9f - (float)r->k[i].place : r->k[i].dist; r->order[i] = i; }
    for (int i = 1; i < r->nk; i++) { int v = r->order[i], j = i - 1; while (j >= 0 && key[r->order[j]] < key[v]) { r->order[j + 1] = r->order[j]; j--; } r->order[j + 1] = v; }
}

static int give_item(Race *r, int position) {           /* back of the pack gets more help */
    float x = rr(r); float behind = (float)position / (float)(r->nk - 1);
    if (x < 0.30f + 0.35f * behind) return ITEM_MANNA;
    if (x < 0.55f + 0.25f * behind) return ITEM_STONE;
    if (x < 0.80f) return ITEM_STAFF;
    return ITEM_OIL;
}

static void spawn_proj(Race *r, int owner, int type) {
    for (int i = 0; i < MAX_PROJ; i++) if (!r->proj[i].alive) {
        Proj *p = &r->proj[i]; Kart *k = &r->k[owner]; memset(p, 0, sizeof *p);
        p->alive = 1; p->type = type; p->owner = owner; p->idx = k->hit.idx;
        float hx = sinf(k->yaw), hz = cosf(k->yaw);
        if (type == ITEM_STONE) { p->x = k->x + hx * 1.8f; p->z = k->z + hz * 1.8f; p->y = k->y + 0.6f; float sp = fmaxf(k->speed, 0) + 32.0f; p->vx = hx * sp; p->vz = hz * sp; p->life = 2.6f; }
        else { p->x = k->x - hx * 2.2f; p->z = k->z - hz * 2.2f; p->y = k->y; p->life = 30.0f; }
        return;
    }
}

static void use_item(Race *r, int i) {
    Kart *k = &r->k[i];
    switch (k->item) {
        case ITEM_MANNA: k->boost_t = 1.7f; k->boost_mul = 1.45f; push_event(r, EV_USE_MANNA, i); break;
        case ITEM_STAFF: k->shield = 8.0f; push_event(r, EV_USE_STAFF, i); break;
        case ITEM_STONE: spawn_proj(r, i, ITEM_STONE); push_event(r, EV_USE_STONE, i); r->stat_stones++; break;
        case ITEM_OIL: spawn_proj(r, i, ITEM_OIL); push_event(r, EV_USE_OIL, i); r->stat_oils++; break;
        default: return;
    }
    r->stat_uses++;
    k->item = ITEM_NONE; k->item_hold = 0;
}

/* AI decides whether to use its item now */
static int ai_wants_item(Race *r, int i) {
    Kart *k = &r->k[i]; const Track *t = r->track;
    if (!k->item || k->stun > 0) return 0;
    switch (k->item) {
        case ITEM_MANNA: { int look = 12; const TrackSample *a = &t->s[(k->hit.idx + look) % t->n], *b = &t->s[(k->hit.idx + look * 2) % t->n];
            float curve = fabsf(wrap_pi(atan2f(b->tx, b->tz) - atan2f(a->tx, a->tz))); return curve < 0.2f && !k->air; }
        case ITEM_STAFF: return k->item_hold > 2.5f;
        case ITEM_STONE: for (int j = 0; j < r->nk; j++) { if (j == i) continue; Kart *o = &r->k[j];
                float dx = o->x - k->x, dz = o->z - k->z, d = sqrtf(dx * dx + dz * dz);
                if (d < 45.0f && fabsf(wrap_pi(atan2f(dx, dz) - k->yaw)) < 0.22f) return 1; }
            return k->item_hold > 10.0f;
        case ITEM_OIL: for (int j = 0; j < r->nk; j++) { if (j == i) continue; Kart *o = &r->k[j];
                float dx = o->x - k->x, dz = o->z - k->z, d = sqrtf(dx * dx + dz * dz);
                if (d < 18.0f && fabsf(wrap_pi(atan2f(-dx, -dz) - k->yaw)) < 0.5f) return 1; }
            return k->item_hold > 12.0f;
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
        if (r->cd <= 0) { r->state = RACE_RUNNING; push_event(r, EV_GO, -1); }
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
            else in.steer += 0.06f * sinf(r->ai_wobble[i]) * (1.0f - skill);      /* human-like imperfection */
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
    /* projectiles and hazards */
    for (int p = 0; p < MAX_PROJ; p++) {
        Proj *pr = &r->proj[p]; if (!pr->alive) continue;
        pr->age += dt; pr->life -= dt;
        if (pr->type == ITEM_STONE) {
            /* a little homing toward the nearest kart ahead of the thrower */
            int target = -1; float best = 1e9f;
            for (int i = 0; i < r->nk; i++) { if (i == pr->owner) continue; Kart *o = &r->k[i]; float dx = o->x - pr->x, dz = o->z - pr->z, d = dx * dx + dz * dz;
                float ahead = dx * pr->vx + dz * pr->vz; if (ahead > 0 && d < best && d < 40.0f * 40.0f) { best = d; target = i; } }
            if (target >= 0) { Kart *o = &r->k[target]; float dx = o->x - pr->x, dz = o->z - pr->z, d = sqrtf(dx * dx + dz * dz) + 1e-3f; float sp = sqrtf(pr->vx * pr->vx + pr->vz * pr->vz);
                float wx = pr->vx / sp * (1 - 2.5f * dt) + dx / d * 2.5f * dt, wz = pr->vz / sp * (1 - 2.5f * dt) + dz / d * 2.5f * dt; float wl = sqrtf(wx * wx + wz * wz) + 1e-3f; pr->vx = wx / wl * sp; pr->vz = wz / wl * sp; }
            pr->x += pr->vx * dt; pr->z += pr->vz * dt;
            TrackHit h; track_locate(t, pr->x, pr->z, pr->idx, &h); pr->idx = h.idx; pr->y = h.height + 0.6f;
            if (fabsf(h.lateral) > track_edge(t, h.idx)) pr->alive = 0;
        }
        if (pr->life <= 0) { pr->alive = 0; continue; }
        for (int i = 0; i < r->nk; i++) {
            Kart *k = &r->k[i];
            if (i == pr->owner && pr->age < (pr->type == ITEM_OIL ? 1.5f : 0.4f)) continue;
            float dx = k->x - pr->x, dz = k->z - pr->z, rad = pr->type == ITEM_OIL ? 1.9f : 1.6f;
            if (dx * dx + dz * dz < rad * rad && fabsf(k->y - pr->y) < 2.5f) {
                int had_shield = k->shield > 0;
                if (kart_hit(k, pr->type == ITEM_OIL ? 1.0f : 1.3f)) { r->stat_hits++; push_event(r, EV_HIT, i); }
                else if (had_shield) { r->stat_blocks++; push_event(r, EV_BLOCK, i); }
                pr->alive = 0; break;
            }
        }
    }
    /* kart-to-kart bumping: push apart, heavier kart wins */
    for (int i = 0; i < r->nk; i++) for (int j = i + 1; j < r->nk; j++) {
        Kart *a = &r->k[i], *b = &r->k[j];
        float dx = b->x - a->x, dz = b->z - a->z, d = sqrtf(dx * dx + dz * dz);
        if (d < 2.2f && d > 0.01f && fabsf(a->y - b->y) < 2.0f) {
            float push = (2.2f - d), wa = a->st.weight, wb = b->st.weight, fa = wb / (wa + wb), fb = wa / (wa + wb);
            a->x -= dx / d * push * fa; a->z -= dz / d * push * fa; b->x += dx / d * push * fb; b->z += dz / d * push * fb;
            float rel = fabsf(a->speed - b->speed);
            if (rel > 4.0f) { a->speed *= 0.97f; b->speed *= 0.97f; push_event(r, EV_BUMP, i); }
        }
    }
    /* the race ends 3 s after the player finishes, when everyone finishes, or at the time limit */
    Kart *pk = &r->k[r->player];
    if (pk->finished) r->after_t += dt;
    if (r->after_t > 3.0f || r->nfin >= r->nk || r->race_t >= RACE_TIME_LIMIT) {
        /* anyone who has not finished gets a place by progress */
        race_rank(r);
        for (int p = 0; p < r->nk; p++) { Kart *k = &r->k[r->order[p]]; if (!k->finished) { k->finished = 1; k->place = ++r->nfin; } }
        r->state = RACE_FINISHED; r->after_t = 0;
    }
}
