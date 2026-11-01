// battle.c - David vs Goliath. The arena is a wide loop road; David drives it like any kart, Goliath walks across it.
#include "battle.h"
#include <math.h>
#include <string.h>

#define DAVID 2                         /* David's slot in CHARACTERS */
static void bev(Battle *b, int e) { if (b->nev < 16) b->ev[b->nev++] = e; }
const BattleQuote BATTLE_QUOTES[BATTLE_NQUOTES] = {
    {"GOLIATH", "AM I A DOG, THAT THOU COMEST TO ME WITH STAVES?", "1 SAMUEL 17:43"},
    {"DAVID", "THOU COMEST TO ME WITH A SWORD, AND WITH A SPEAR, AND WITH A SHIELD: BUT I COME TO THEE IN THE NAME OF THE LORD OF HOSTS", "1 SAMUEL 17:45"},
    {"GOLIATH", "COME TO ME, AND I WILL GIVE THY FLESH UNTO THE FOWLS OF THE AIR", "1 SAMUEL 17:44"},
    {"DAVID", "THIS DAY WILL THE LORD DELIVER THEE INTO MINE HAND", "1 SAMUEL 17:46"},
    {"DAVID", "THE BATTLE IS THE LORD'S", "1 SAMUEL 17:47"},
    {"", "SO DAVID PREVAILED OVER THE PHILISTINE WITH A SLING AND WITH A STONE", "1 SAMUEL 17:50"},
};
static void say(Battle *b, int q) { if (b->quotes_said & (1 << q)) return; b->quotes_said |= 1 << q; b->quote = q; b->quote_t = 5.0f; bev(b, BEV_QUOTE); }
static float brand(Battle *b) { uint32_t *x = &b->race.rng; *x ^= *x << 13; *x ^= *x >> 17; *x ^= *x << 5; return (*x & 0xFFFF) / 65536.0f; }
static float wrap_pi(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }

void battle_init(Battle *b, const Track *arena, int difficulty) {
    memset(b, 0, sizeof *b);
    race_init(&b->race, arena, DAVID, 0, difficulty, 1);
    b->race.cd = 3.5f;
    /* Goliath starts at the far side of the arena */
    const TrackSample *s = &arena->s[arena->n / 2];
    b->gx = s->x; b->gz = s->z; b->gyaw = atan2f(-s->tx, -s->tz); b->gstate = GOL_STALK; b->gtimer = 2.0f;
    /* stones lie along the road, spaced around the arena */
    for (int i = 0; i < BATTLE_STONES; i++) { int idx = (arena->n * (i + 1)) / (BATTLE_STONES + 1); float lat = (i & 1) ? arena->s[idx].width * 0.3f : -arena->s[idx].width * 0.3f;
        b->stone_x[i] = arena->s[idx].x + arena->s[idx].nx * lat; b->stone_z[i] = arena->s[idx].z + arena->s[idx].nz * lat; }
    b->stones_held = 1; b->quote = -1;
    /* the stones lie in the brook that runs across the valley */
    for (int i = 0; i < BATTLE_STONES; i++) { static const float bx[BATTLE_STONES] = {-70, -45, -20, 20, 45, 70}; b->stone_x[i] = bx[i]; b->stone_z[i] = BROOK_Z + ((i & 1) ? 1.5f : -1.5f); }
}

static void throw_stone(Battle *b) {
    Kart *k = &b->race.k[0];
    for (int i = 0; i < BATTLE_SPEARS; i++) if (!b->stone[i].alive) { Missile *m = &b->stone[i];
        float hx = sinf(k->yaw), hz = cosf(k->yaw);
        /* aim a little toward Goliath when he is roughly ahead */
        float dx = b->gx - k->x, dz = b->gz - k->z, d = sqrtf(dx * dx + dz * dz) + 1e-3f;
        float ax = hx, az = hz; float dot = (dx * hx + dz * hz) / d;
        if (dot > 0.3f) { float lead = d / 38.0f; float tx = b->gx + sinf(b->gyaw) * b->gspeed * lead, tz = b->gz + cosf(b->gyaw) * b->gspeed * lead; ax = tx - k->x; az = tz - k->z; float l = sqrtf(ax * ax + az * az) + 1e-3f; ax /= l; az /= l; }   /* the sling finds its mark when he is roughly ahead */
        m->alive = 1; m->x = k->x + ax * 1.5f; m->z = k->z + az * 1.5f; m->y = k->y + 1.0f; m->vx = ax * 38.0f + hx * fmaxf(k->speed, 0) * 0.3f; m->vz = az * 38.0f + hz * fmaxf(k->speed, 0) * 0.3f; m->life = 2.0f;
        b->stones_held--; bev(b, BEV_STONE_THROWN); return; }
}

void battle_step(Battle *b, const KartInput *in_real, int autopilot, float dt) {
    Race *r = &b->race; Kart *k = &r->k[0]; const Track *t = r->track;
    b->nev = 0;
    if (b->over) { r->after_t += dt; return; }
    KartInput in = *in_real;
    if (autopilot) {    /* the demo David: fetch a stone, then drive at Goliath and sling it */
        kart_autopilot(k, t, &in, 0.8f);
        float tx, tz; int have = 0;
        if (b->stones_held < 2) { float best = 1e9f; for (int i = 0; i < BATTLE_STONES; i++) { if (b->stone_respawn[i] > 0) continue; float ddx = b->stone_x[i] - k->x, ddz = b->stone_z[i] - k->z, d2 = ddx * ddx + ddz * ddz; if (d2 < best) { best = d2; tx = b->stone_x[i]; tz = b->stone_z[i]; have = 1; } } }
        else { tx = b->gx; tz = b->gz; have = 1; }
        if (have) { float err = wrap_pi(atan2f(tx - k->x, tz - k->z) - k->yaw); in.steer = err > 0 ? fminf(err * 2.5f, 1.0f) : fmaxf(err * 2.5f, -1.0f); in.gas = fabsf(err) < 1.0f || k->speed < 8.0f; in.brake = fabsf(err) > 1.8f && k->speed > 12.0f; in.drift = 0;
            float gd = sqrtf((b->gx - k->x) * (b->gx - k->x) + (b->gz - k->z) * (b->gz - k->z));
            if (b->stones_held > 0 && gd < 20.0f && b->gstate == GOL_CHARGE) { in.steer = err > 0 ? -1.0f : 1.0f; in.gas = 1; in.brake = 0; }   /* swerve out of a charge */
            else if (b->stones_held > 0 && gd < 14.0f) { float away = wrap_pi(atan2f(k->x - b->gx, k->z - b->gz) - k->yaw); in.steer = away > 0 ? 1.0f : -1.0f; in.gas = 1; in.brake = 0; }   /* keep slinging distance */
            in.item = b->stones_held > 0 && fabsf(wrap_pi(atan2f(b->gx - k->x, b->gz - k->z) - k->yaw)) < 1.1f && gd < 40.0f && gd > 8.0f;
            /* dodge a spear coming in */
            for (int i = 0; i < BATTLE_SPEARS; i++) { const Missile *m = &b->spear[i]; if (!m->alive) continue; float sx = k->x - m->x, sz = k->z - m->z, sd = sqrtf(sx * sx + sz * sz) + 1e-3f;
                float closing = (sx * m->vx + sz * m->vz) / sd; if (closing > 10.0f && sd < 18.0f) { float side = m->vx * sz - m->vz * sx; in.steer = side > 0 ? -1.0f : 1.0f; in.gas = 1; } } }
    }
    int want_throw = in.item; in.item = 0;
    race_step(r, &in, 0, dt);
    if (r->state != RACE_RUNNING) return;
    b->t += dt; if (b->invuln > 0) b->invuln -= dt;
    if (b->quote_t > 0) { b->quote_t -= dt; if (b->quote_t <= 0) b->quote = -1; }
    if (b->t > 0.5f) say(b, 0);
    if (b->t > 6.0f) say(b, 1);
    if (b->hits_on_goliath >= 2) say(b, 3);
    if (b->hits_on_goliath >= 4) say(b, 4);
    float speed_scale = r->difficulty == DIFF_EASY ? 0.8f : (r->difficulty == DIFF_HARD ? 1.25f : 1.0f);

    /* stones on the ground */
    for (int i = 0; i < BATTLE_STONES; i++) {
        if (b->stone_respawn[i] > 0) { b->stone_respawn[i] -= dt; continue; }
        float dx = k->x - b->stone_x[i], dz = k->z - b->stone_z[i];
        if (dx * dx + dz * dz < 3.5f * 3.5f && b->stones_held < 3) { b->stones_held++; b->stone_respawn[i] = 6.0f; bev(b, BEV_STONE_PICKED); }
    }
    if (want_throw && !b->was_throw && b->stones_held > 0 && k->stun <= 0) throw_stone(b);
    b->was_throw = want_throw;

    /* Goliath */
    float dx = k->x - b->gx, dz = k->z - b->gz, dist = sqrtf(dx * dx + dz * dz) + 1e-3f;
    float to_david = atan2f(dx, dz);
    b->gtimer -= dt; b->roar_t -= dt;
    switch (b->gstate) {
        case GOL_STALK:
            b->gyaw += wrap_pi(to_david - b->gyaw) * fminf(1.0f, 2.0f * dt);
            b->gspeed = 7.0f * speed_scale;
            if (b->gtimer <= 0) { b->gstate = (dist < 45.0f && brand(b) < 0.5f) || dist < 12.0f ? GOL_WINDUP : GOL_THROW; b->gtimer = b->gstate == GOL_WINDUP ? 0.8f : 0.5f; if (b->gstate == GOL_WINDUP) bev(b, BEV_ROAR); }
            break;
        case GOL_WINDUP:          /* stops, roars, then charges where David is now */
            b->gspeed = 0; b->gyaw += wrap_pi(to_david - b->gyaw) * fminf(1.0f, 4.0f * dt);
            if (b->gtimer <= 0) { b->gstate = GOL_CHARGE; b->gtimer = 1.6f; b->charge_tx = k->x + sinf(k->yaw) * k->speed * 0.6f; b->charge_tz = k->z + cosf(k->yaw) * k->speed * 0.6f; b->gyaw = atan2f(b->charge_tx - b->gx, b->charge_tz - b->gz); bev(b, BEV_CHARGE); say(b, 2); }
            break;
        case GOL_CHARGE:
            b->gspeed = 24.0f * speed_scale;
            if (b->gtimer <= 0) { b->gstate = GOL_STALK; b->gtimer = 2.5f; }
            break;
        case GOL_THROW:
            b->gspeed = 0; b->gyaw += wrap_pi(to_david - b->gyaw) * fminf(1.0f, 6.0f * dt);
            if (b->gtimer <= 0) {
                for (int i = 0; i < BATTLE_SPEARS; i++) if (!b->spear[i].alive) { Missile *m = &b->spear[i];
                    /* lead the target a little */
                    float lead = dist / 26.0f * (r->difficulty == DIFF_EASY ? 0.3f : (r->difficulty == DIFF_HARD ? 0.9f : 0.6f)); float tx = k->x + sinf(k->yaw) * k->speed * lead, tz = k->z + cosf(k->yaw) * k->speed * lead;
                    float ax = tx - b->gx, az = tz - b->gz, l = sqrtf(ax * ax + az * az) + 1e-3f;
                    m->alive = 1; m->x = b->gx + ax / l * 2.0f; m->z = b->gz + az / l * 2.0f; m->y = 3.0f; m->vx = ax / l * 28.0f * speed_scale; m->vz = az / l * 28.0f * speed_scale; m->life = 2.5f; bev(b, BEV_SPEAR); break; }
                b->gstate = GOL_STALK; b->gtimer = (2.0f + brand(b) * 1.5f) / speed_scale;
            }
            break;
        case GOL_STUNNED:
            b->gspeed = 0; b->gstun -= dt;
            if (b->gstun <= 0) { b->gstate = GOL_STALK; b->gtimer = 1.0f; }
            break;
        case GOL_DOWN: b->gspeed = 0; break;
    }
    /* his footsteps: the ground shakes with every stride, faster when he charges */
    if (b->gspeed > 0) { b->step_t -= dt * (b->gstate == GOL_CHARGE ? 3.0f : 1.0f); if (b->step_t <= 0) { b->step_t = 0.9f; bev(b, BEV_STEP); } }
    /* Goliath walks, kept inside the arena walls */
    b->gx += sinf(b->gyaw) * b->gspeed * dt; b->gz += cosf(b->gyaw) * b->gspeed * dt;
    { TrackHit h; track_locate(t, b->gx, b->gz, -1, &h); float e = track_edge(t, h.idx) - 1.5f;
      if (fabsf(h.lateral) > e) { const TrackSample *s = &t->s[h.idx]; float over = fabsf(h.lateral) - e, sign = h.lateral > 0 ? 1 : -1; b->gx -= s->nx * sign * over; b->gz -= s->nz * sign * over; if (b->gstate == GOL_CHARGE) { b->gstate = GOL_STALK; b->gtimer = 2.0f; } } }
    /* Goliath runs into David */
    dx = k->x - b->gx; dz = k->z - b->gz; dist = sqrtf(dx * dx + dz * dz) + 1e-3f;
    if (dist < 3.2f && b->invuln <= 0 && b->gstate != GOL_STUNNED && b->gstate != GOL_DOWN) {
        k->x += dx / dist * 3.0f; k->z += dz / dist * 3.0f; k->speed *= 0.3f;
        if (kart_hit(k, 1.2f)) { b->hits_on_david++; b->invuln = 2.5f; bev(b, BEV_DAVID_HIT); }
    }
    /* stones in the air */
    for (int i = 0; i < BATTLE_SPEARS; i++) { Missile *m = &b->stone[i]; if (!m->alive) continue;
        m->x += m->vx * dt; m->z += m->vz * dt; m->life -= dt; if (m->life <= 0) m->alive = 0;
        float sx = m->x - b->gx, sz = m->z - b->gz;
        if (sx * sx + sz * sz < 2.6f * 2.6f && b->gstate != GOL_DOWN) { m->alive = 0; b->hits_on_goliath++; b->gstate = GOL_STUNNED; b->gstun = r->difficulty == DIFF_EASY ? 1.8f : (r->difficulty == DIFF_HARD ? 0.8f : 1.3f); bev(b, BEV_GOLIATH_HIT);
            if (b->hits_on_goliath >= BATTLE_WIN_HITS) { b->gstate = GOL_DOWN; b->over = 1; b->won = 1; r->state = RACE_FINISHED; k->finished = 1; k->place = 1; bev(b, BEV_WIN); b->quotes_said &= ~(1 << 5); say(b, 5); b->quote_t = 30.0f; } }
    }
    /* spears */
    for (int i = 0; i < BATTLE_SPEARS; i++) { Missile *m = &b->spear[i]; if (!m->alive) continue;
        m->x += m->vx * dt; m->z += m->vz * dt; m->life -= dt; m->y -= 0.8f * dt; if (m->life <= 0) m->alive = 0;
        float sx = m->x - k->x, sz = m->z - k->z;
        if (sx * sx + sz * sz < 1.6f * 1.6f && b->invuln <= 0 && !k->air) { m->alive = 0; if (kart_hit(k, 1.2f)) { b->hits_on_david++; b->invuln = 2.5f; bev(b, BEV_DAVID_HIT); } }
    }
    if (b->hits_on_david >= BATTLE_LIVES || r->race_t >= RACE_TIME_LIMIT - 0.5f) { b->over = 1; b->won = 0; r->state = RACE_FINISHED; bev(b, BEV_LOSE); }
}
