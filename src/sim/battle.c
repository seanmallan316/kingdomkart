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

int battle_out(const Battle *b, int d) { return b->dhits[d] >= BATTLE_LIVES; }
int battle_need(const Battle *b) { return b->race.difficulty == DIFF_EASY ? 4 : (b->race.difficulty == DIFF_HARD ? 6 : BATTLE_WIN_HITS); }
float battle_head_y(const Battle *b) { return b->gstate == GOL_DAZED ? 4.7f : 5.1f; }
#define STONE_G 22.0f
static void battle_common(Battle *b, const Track *arena);
void battle_init(Battle *b, const Track *arena, int difficulty) {
    memset(b, 0, sizeof *b);
    race_init(&b->race, arena, DAVID, 0, difficulty, 1); b->nd = 1;
    battle_common(b, arena);
}
void battle_init_multi(Battle *b, const Track *arena, int difficulty, int ndavids, const int *chars) {
    memset(b, 0, sizeof *b);
    if (ndavids < 1) ndavids = 1;
    if (ndavids > MAX_HUMANS) ndavids = MAX_HUMANS;
    race_init_tt_multi(&b->race, arena, chars, ndavids, difficulty); b->nd = ndavids;   /* only the people's karts, no boxes */
    battle_common(b, arena);
}
static void battle_common(Battle *b, const Track *arena) {
    b->race.cd = 3.5f + RACE_READY_T;
    /* Goliath starts at the far side of the arena */
    const TrackSample *s = &arena->s[arena->n / 2];
    b->gx = s->x; b->gz = s->z; b->gyaw = atan2f(-s->tx, -s->tz); b->sb_yaw = b->gyaw; b->gstate = GOL_STALK; b->gtimer = 2.0f;
    /* stones lie along the road, spaced around the arena */
    for (int i = 0; i < BATTLE_STONES; i++) { int idx = (arena->n * (i + 1)) / (BATTLE_STONES + 1); float lat = (i & 1) ? arena->s[idx].width * 0.3f : -arena->s[idx].width * 0.3f;
        b->stone_x[i] = arena->s[idx].x + arena->s[idx].nx * lat; b->stone_z[i] = arena->s[idx].z + arena->s[idx].nz * lat; }
    for (int d = 0; d < MAX_HUMANS; d++) b->dstones[d] = 1;
    b->stones_held = 1; b->quote = -1;
    /* the stones lie in the brook that runs across the valley */
    for (int i = 0; i < BATTLE_STONES; i++) { static const float bx[BATTLE_STONES] = {-64, -32, 0, 32, 64}; b->stone_x[i] = bx[i]; b->stone_z[i] = BROOK_Z + ((i & 1) ? 1.5f : -1.5f); }
}

static void throw_stone(Battle *b, int who) {
    Kart *k = &b->race.k[who];
    for (int i = 0; i < BATTLE_SPEARS; i++) if (!b->stone[i].alive) { Missile *m = &b->stone[i];
        float hx = sinf(k->yaw), hz = cosf(k->yaw);
        /* aim a little toward Goliath when he is roughly ahead */
        float dx = b->gx - k->x, dz = b->gz - k->z, d = sqrtf(dx * dx + dz * dz) + 1e-3f;
        float ax = hx, az = hz; float dot = (dx * hx + dz * hz) / d;
        if (dot > 0.3f) { float lead = d / 38.0f; float tx = b->gx + sinf(b->gyaw) * b->gspeed * lead, tz = b->gz + cosf(b->gyaw) * b->gspeed * lead; ax = tx - k->x; az = tz - k->z; float l = sqrtf(ax * ax + az * az) + 1e-3f; ax /= l; az /= l; }   /* the sling finds its mark when he is roughly ahead */
        m->alive = 1; m->x = k->x + ax * 1.5f; m->z = k->z + az * 1.5f; m->y = k->y + 1.6f; m->vx = ax * 38.0f + hx * fmaxf(k->speed, 0) * 0.3f; m->vz = az * 38.0f + hz * fmaxf(k->speed, 0) * 0.3f; m->life = 2.5f;
        { float hs = sqrtf(m->vx * m->vx + m->vz * m->vz) + 1e-3f, tt = fminf(d, 60.0f) / hs; if (tt < 0.15f) tt = 0.15f;   /* the sling sends it up in an arc that comes down at his head (1 Samuel 17:49) */
          m->vy = (b->gy + battle_head_y(b) - m->y) / tt + 0.5f * STONE_G * tt; if (m->vy > 22.0f) m->vy = 22.0f; if (m->vy < 2.0f) m->vy = 2.0f; }
        b->dstones[who]--; bev(b, BEV_STONE_THROWN); return; }
}

void battle_step(Battle *b, const KartInput *in_real, int autopilot, float dt) {
    Race *r = &b->race; Kart *k = &r->k[0]; const Track *t = r->track;
    b->nev = 0;
    if (b->over) { r->after_t += dt; return; }
    KartInput in = *in_real; b->stones_held = b->dstones[0];
    if (autopilot && b->nd == 1) {    /* the demo David: fetch a stone, then drive at Goliath and sling it */
        kart_autopilot(k, t, &in, 0.8f);
        float tx, tz; int have = 0;
        if (b->stones_held < 2) { float best = 1e9f; for (int i = 0; i < BATTLE_STONES; i++) { if (b->stone_respawn[i] > 0) continue; float ddx = b->stone_x[i] - k->x, ddz = b->stone_z[i] - k->z, d2 = ddx * ddx + ddz * ddz; if (d2 < best) { best = d2; tx = b->stone_x[i]; tz = b->stone_z[i]; have = 1; } } }
        else { tx = b->gx; tz = b->gz; have = 1; }
        if (have) { float err = wrap_pi(atan2f(tx - k->x, tz - k->z) - k->yaw); in.steer = err > 0 ? fminf(err * 2.5f, 1.0f) : fmaxf(err * 2.5f, -1.0f); in.gas = fabsf(err) < 1.0f || k->speed < 8.0f; in.brake = fabsf(err) > 1.8f && k->speed > 12.0f; in.drift = 0;
            float gd = sqrtf((b->gx - k->x) * (b->gx - k->x) + (b->gz - k->z) * (b->gz - k->z));
            if (b->stones_held > 0 && gd < 20.0f && b->gstate == GOL_CHARGE) { in.steer = err > 0 ? -1.0f : 1.0f; in.gas = 1; in.brake = 0; }   /* swerve out of a charge */
            else if (b->stones_held > 0 && gd < 14.0f) { float away = wrap_pi(atan2f(k->x - b->gx, k->z - b->gz) - k->yaw); in.steer = away > 0 ? 1.0f : -1.0f; in.gas = 1; in.brake = 0; }   /* keep slinging distance */
            float head_on = -((b->gx - k->x) * sinf(b->sb_yaw) + (b->gz - k->z) * cosf(b->sb_yaw)) / (gd + 1e-3f);   /* 1 = David is straight behind the shield-bearer */
            int open_side = head_on < 0.5f || b->gstate != GOL_STALK;
            if (b->stones_held > 0 && !open_side && gd > 16.0f && gd < 34.0f && b->gstate == GOL_STALK) {   /* the shield-bearer is in the way: edge round to Goliath's side (he turns slower than a kart) */
                float side = (k->x - b->gx) * cosf(b->sb_yaw) - (k->z - b->gz) * sinf(b->sb_yaw) >= 0 ? 1.0f : -1.0f;
                float want = atan2f(b->gx - k->x, b->gz - k->z) + side * 0.9f; float e2 = wrap_pi(want - k->yaw); in.steer = e2 > 0 ? fminf(e2 * 2.5f, 1.0f) : fmaxf(e2 * 2.5f, -1.0f); in.gas = 1; in.brake = 0; }
            in.item = b->stones_held > 0 && fabsf(wrap_pi(atan2f(b->gx - k->x, b->gz - k->z) - k->yaw)) < 1.1f && gd < 40.0f && gd > 8.0f && (open_side || gd > 30.0f);
            /* dodge a spear coming in */
            for (int i = 0; i < BATTLE_SPEARS; i++) { const Missile *m = &b->spear[i]; if (!m->alive) continue; float sx = k->x - m->x, sz = k->z - m->z, sd = sqrtf(sx * sx + sz * sz) + 1e-3f;
                float closing = (sx * m->vx + sz * m->vz) / sd; if (closing > 10.0f && sd < 18.0f) { float side = m->vx * sz - m->vz * sx; in.steer = side > 0 ? -1.0f : 1.0f; in.gas = 1; } } }
    }
    KartInput all[NKARTS]; memset(all, 0, sizeof all); int want[MAX_HUMANS];
    for (int d = 0; d < b->nd; d++) { all[d] = d == 0 ? in : in_real[d]; want[d] = all[d].item; all[d].item = 0;
        if (battle_out(b, d)) { all[d].gas = 0; all[d].brake = 1; want[d] = 0; } }   /* a David who is out sits it out */
    if (b->nd == 1) race_step(r, &all[0], 0, dt); else race_step_multi(r, all, 0, dt);
    if (r->state != RACE_RUNNING) return;
    b->t += dt; for (int d = 0; d < b->nd; d++) if (b->dinv[d] > 0) b->dinv[d] -= dt;
    if (b->quote_t > 0) { b->quote_t -= dt; if (b->quote_t <= 0) b->quote = -1; }
    if (b->t > 0.5f) say(b, 0);
    if (b->t > 6.0f) say(b, 1);
    if (b->hits_on_goliath >= 2) say(b, 3);
    if (b->hits_on_goliath >= 4) say(b, 4);
    float speed_scale = r->difficulty == DIFF_EASY ? 0.9f : (r->difficulty == DIFF_HARD ? 1.4f : 1.15f);   /* (Sean: a little harder) */

    /* stones on the ground */
    for (int i = 0; i < BATTLE_STONES; i++) {
        if (b->stone_respawn[i] > 0) { b->stone_respawn[i] -= dt; continue; }
        for (int d = 0; d < b->nd; d++) { const Kart *kd = &r->k[d]; if (battle_out(b, d)) continue;
            float dx = kd->x - b->stone_x[i], dz = kd->z - b->stone_z[i];
            if (dx * dx + dz * dz < 4.0f * 4.0f && b->dstones[d] < 5) { b->dstones[d]++; b->stone_respawn[i] = 6.0f; bev(b, BEV_STONE_PICKED); break; } }
    }
    for (int d = 0; d < b->nd; d++) {
        if (want[d] && !b->dthrow[d] && b->dstones[d] > 0 && r->k[d].stun <= 0) throw_stone(b, d);
        else if (want[d] && !b->dthrow[d] && b->dstones[d] == 0) bev(b, BEV_EMPTY);
        b->dthrow[d] = want[d]; }

    /* Goliath goes after the nearest David still in the fight */
    { float bd = 1e30f; for (int d = 0; d < b->nd; d++) { if (battle_out(b, d)) continue; float ex = r->k[d].x - b->gx, ez = r->k[d].z - b->gz; if (ex * ex + ez * ez < bd) { bd = ex * ex + ez * ez; b->target = d; } }
      k = &r->k[b->target]; }
    /* Goliath */
    float dx = k->x - b->gx, dz = k->z - b->gz, dist = sqrtf(dx * dx + dz * dz) + 1e-3f;
    float to_david = atan2f(dx, dz);
    b->gtimer -= dt; b->roar_t -= dt;
    switch (b->gstate) {
        case GOL_STALK:
            b->gyaw += wrap_pi(to_david - b->gyaw) * fminf(1.0f, 2.0f * dt);
            b->gspeed = 7.0f * speed_scale;
            if (b->gtimer <= 0) { b->gstate = (dist < 45.0f && brand(b) < 0.5f) || dist < 12.0f ? GOL_WINDUP : GOL_THROW; b->gtimer = b->gstate == GOL_WINDUP ? 0.8f : 0.9f; if (b->gstate == GOL_WINDUP) bev(b, BEV_ROAR); }
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
                int nsp = r->difficulty == DIFF_EASY ? 1 : (r->difficulty == DIFF_HARD ? 3 : 2);   /* a fan of spears on the harder settings */
                for (int sp = 0; sp < nsp; sp++)
                for (int i = 0; i < BATTLE_SPEARS; i++) if (!b->spear[i].alive) { Missile *m = &b->spear[i];
                    /* lead the target a little */
                    float lead = dist / 26.0f * (r->difficulty == DIFF_EASY ? 0.3f : (r->difficulty == DIFF_HARD ? 0.9f : 0.6f)); float tx = k->x + sinf(k->yaw) * k->speed * lead, tz = k->z + cosf(k->yaw) * k->speed * lead;
                    float ax = tx - b->gx, az = tz - b->gz, l = sqrtf(ax * ax + az * az) + 1e-3f;
                    float spread = (sp - (nsp - 1) * 0.5f) * 0.16f, ca = cosf(spread), sa = sinf(spread); float ux = (ax * ca + az * sa) / l, uz = (-ax * sa + az * ca) / l, spd = 28.0f * speed_scale;
                    m->alive = 1; m->x = b->gx + ux * 2.0f; m->z = b->gz + uz * 2.0f; m->y = 5.6f; m->vx = ux * spd; m->vz = uz * spd; m->life = 3.0f;
                    { float tt = l / spd; if (tt < 0.2f) tt = 0.2f; m->vy = (1.0f - m->y) / tt + 0.5f * 9.0f * tt; }   /* thrown from over his shoulder, arcing down at David */
                    bev(b, BEV_SPEAR); break; }
                b->gstate = GOL_STALK; b->gtimer = (1.6f + brand(b) * 1.4f) / speed_scale;
            }
            break;
        case GOL_STUNNED:
            b->gspeed = 0; b->gstun -= dt;
            if (b->gstun <= 0) { b->gstate = GOL_STALK; b->gtimer = 1.0f; }
            break;
        case GOL_DAZED:   /* reeling with his head in his hands: one stone to the forehead now and he falls */
            b->gspeed = 0; b->gstun -= dt;
            if (b->gstun <= 0) { b->gstate = GOL_WINDUP; b->gtimer = 0.8f; b->hits_on_goliath = battle_need(b) - 2; if (b->hits_on_goliath < 0) b->hits_on_goliath = 0; bev(b, BEV_RECOVER); bev(b, BEV_ROAR); }
            break;
        case GOL_DOWN: b->gspeed = 0; break;
    }
    /* his footsteps: the ground shakes with every stride, faster when he charges */
    b->sb_yaw += wrap_pi(b->gyaw - b->sb_yaw) * fminf(1.0f, (r->difficulty == DIFF_HARD ? 1.2f : 0.8f) * dt);   /* the shield-bearer keeps up with Goliath only slowly */
    if (b->gspeed > 0) { b->step_t -= dt * (b->gstate == GOL_CHARGE ? 3.0f : 1.0f); if (b->step_t <= 0) { b->step_t = 0.9f; bev(b, BEV_STEP); } }
    /* Goliath walks, kept inside the arena walls */
    b->gx += sinf(b->gyaw) * b->gspeed * dt; b->gz += cosf(b->gyaw) * b->gspeed * dt;
    { TrackHit h; track_locate(t, b->gx, b->gz, -1, &h); b->gy = h.height; float e = track_edge(t, h.idx) - 1.5f;
      if (fabsf(h.lateral) > e) { const TrackSample *s = &t->s[h.idx]; float over = fabsf(h.lateral) - e, sign = h.lateral > 0 ? 1 : -1; b->gx -= s->nx * sign * over; b->gz -= s->nz * sign * over; if (b->gstate == GOL_CHARGE) { b->gstate = GOL_STALK; b->gtimer = 2.0f; } } }
    /* Goliath runs into David */
    for (int d = 0; d < b->nd; d++) { Kart *kd = &r->k[d]; if (battle_out(b, d)) continue;
        dx = kd->x - b->gx; dz = kd->z - b->gz; dist = sqrtf(dx * dx + dz * dz) + 1e-3f;
        if (dist < 3.2f && b->dinv[d] <= 0 && b->gstate != GOL_STUNNED && b->gstate != GOL_DOWN && b->gstate != GOL_DAZED) {
            kd->x += dx / dist * 3.0f; kd->z += dz / dist * 3.0f; kd->speed *= 0.3f;
            if (kart_hit(kd, 1.2f)) { b->dhits[d]++; b->dinv[d] = 2.5f; bev(b, BEV_DAVID_HIT); } } }
    /* stones in the air: they fly in an arc; only a stone to his head counts - his armour turns the rest (1 Samuel 17:5-6, 49) */
    for (int i = 0; i < BATTLE_SPEARS; i++) { Missile *m = &b->stone[i]; if (!m->alive) continue;
        m->x += m->vx * dt; m->z += m->vz * dt; m->y += m->vy * dt; m->vy -= STONE_G * dt; m->life -= dt;
        TrackHit gh; track_locate(t, m->x, m->z, -1, &gh); if (m->life <= 0 || m->y < gh.height - 0.2f) { m->alive = 0; continue; }
        float sx = m->x - b->gx, sz = m->z - b->gz, hd2 = sx * sx + sz * sz;
        if (hd2 < 3.4f * 3.4f && b->gstate == GOL_STALK && m->y < b->gy + 4.5f) {   /* the shield-bearer walks before him: while Goliath stalks, a stone from straight ahead is caught; when he stops to throw, or charges, he is open */
            float vl = sqrtf(m->vx * m->vx + m->vz * m->vz) + 1e-3f; float head_on = -(m->vx / vl * sinf(b->sb_yaw) + m->vz / vl * cosf(b->sb_yaw));
            float cone = r->difficulty == DIFF_EASY ? 0.88f : (r->difficulty == DIFF_HARD ? 0.55f : 0.72f);
            if (head_on > cone) { m->alive = 0; b->blocks++; bev(b, BEV_SHIELD); continue; } }
        if (b->gstate == GOL_DOWN) continue;
        float hy = b->gy + battle_head_y(b), dy = m->y - hy, hr = r->difficulty == DIFF_EASY ? 1.7f : (r->difficulty == DIFF_HARD ? 1.15f : 1.4f);
        if (hd2 + dy * dy < hr * hr) {   /* the head */
            m->alive = 0;
            if (b->gstate == GOL_DAZED) {   /* "the stone sunk into his forehead; and he fell upon his face to the earth" */
                b->gstate = GOL_DOWN; b->over = 1; b->won = 1; r->state = RACE_FINISHED; for (int d = 0; d < b->nd; d++) { r->k[d].finished = 1; r->k[d].place = 1; }
                bev(b, BEV_FOREHEAD); bev(b, BEV_WIN); b->quotes_said &= ~(1 << 5); say(b, 5); b->quote_t = 30.0f; continue; }
            b->hits_on_goliath++; bev(b, BEV_GOLIATH_HIT);
            if (b->hits_on_goliath >= battle_need(b)) { b->gstate = GOL_DAZED; b->gstun = r->difficulty == DIFF_EASY ? 6.0f : (r->difficulty == DIFF_HARD ? 3.8f : 4.8f); bev(b, BEV_DAZED); }
            else { b->gstate = GOL_STUNNED; b->gstun = r->difficulty == DIFF_EASY ? 1.3f : (r->difficulty == DIFF_HARD ? 0.55f : 0.85f); }
            continue; }
        if (hd2 < 2.4f * 2.4f && m->y < hy - 0.6f && m->y > b->gy) { m->alive = 0; bev(b, BEV_ARMOUR); }   /* off his bronze armour: no harm */
    }
    /* spears */
    for (int i = 0; i < BATTLE_SPEARS; i++) { Missile *m = &b->spear[i]; if (!m->alive) continue;
        m->x += m->vx * dt; m->z += m->vz * dt; m->y += m->vy * dt; m->vy -= 9.0f * dt; m->life -= dt; if (m->life <= 0 || m->y < -1.0f) m->alive = 0;
        for (int d = 0; d < b->nd; d++) { Kart *kd = &r->k[d]; if (battle_out(b, d)) continue;
            float sx = m->x - kd->x, sz = m->z - kd->z;
            if (sx * sx + sz * sz < 1.7f * 1.7f && fabsf(m->y - kd->y - 0.8f) < 1.6f && b->dinv[d] <= 0) { m->alive = 0; if (kart_hit(kd, 1.2f)) { b->dhits[d]++; b->dinv[d] = 2.5f; bev(b, BEV_DAVID_HIT); } break; } }
    }
    if (b->gstate != b->gprev) { b->gprev = b->gstate; b->gstate_t = 0; } else b->gstate_t += dt;
    int all_out = 1; for (int d = 0; d < b->nd; d++) if (!battle_out(b, d)) all_out = 0;
    b->hits_on_david = b->dhits[0]; b->stones_held = b->dstones[0]; b->invuln = b->dinv[0];
    if (all_out || r->race_t >= BATTLE_TIME_LIMIT - 0.5f) { b->over = 1; b->won = 0; r->state = RACE_FINISHED; bev(b, BEV_LOSE); }
}
