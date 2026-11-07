#include "kart.h"
#include <string.h>

#define BASE_TOP 36.0f          /* m/s on the road for an average kart (about 130 km/h) */
#define BASE_ACCEL 13.0f
#define BRAKE 28.0f
#define COAST 3.0f
#define OFFROAD_MUL 0.5f
#define GRAVITY 22.0f           /* a little stronger than earth; arcade jumps feel better */

static float wrap_pi(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
#define clampf1 clampf

void kart_reset(Kart *k, const Track *t, int slot) {
    KartStats st = k->st;
    memset(k, 0, sizeof *k);
    k->st = st;
    if (k->st.top_speed <= 0) { k->st.top_speed = 1; k->st.accel = 1; k->st.handling = 1; k->st.weight = 1; }
    /* grid: two columns, behind the start line */
    const TrackSample *s0 = &t->s[t->grid_idx];
    float back = 6.0f + (float)(slot / 2) * 5.0f, side = (slot % 2) ? 2.8f : -2.8f;
    int idx = (t->grid_idx + t->n - (int)(back / (t->length / (float)t->n))) % t->n; if (idx < 0) idx += t->n;
    const TrackSample *s = &t->s[idx];
    k->x = s->x + s->nx * side; k->z = s->z + s->nz * side;
    k->yaw = atan2f(s0->tx, s0->tz);
    track_locate(t, k->x, k->z, -1, &k->hit);
    k->y = k->hit.height;
    k->next_cp = 0; k->lap = 0; k->best_lap = 0;
    k->dist = -back;
    if (t->grid_idx > 0) {      /* a one-way road: the lap is already under way, the next gate is the first one past the grid */
        k->lap = 1; k->next_cp = 0;
        for (int c = 1; c < TRACK_CHECKPOINTS; c++) if (t->checkpoint[c] > t->grid_idx + 2) { k->next_cp = c; break; }
    }
    k->boost_mul = 1.0f;
}

static void advance_progress(Kart *k, const Track *t, int prev_idx) {
    int d = k->hit.idx - prev_idx;
    if (d > t->n / 2) d -= t->n; else if (d < -t->n / 2) d += t->n;
    float step_len = t->length / (float)t->n;
    k->dist += (float)d * step_len;
    /* checkpoints must be passed in order; gate 0 (start line) completes a lap */
    int gate = t->checkpoint[k->next_cp];
    int passed = 0;
    if (d > 0) { for (int q = 1; q <= d; q++) if (((prev_idx + q) % t->n) == gate) passed = 1; }
    if (passed) {
        if (k->next_cp == 0) {
            if (k->lap >= 1 && k->lap <= MAX_LAPS) { k->lap_times[k->lap - 1] = k->lap_time; if (k->best_lap <= 0 || k->lap_time < k->best_lap) k->best_lap = k->lap_time; }
            k->lap++; k->lap_time = 0;
            if (k->lap > t->laps) k->finished = 1;
        }
        k->next_cp = (k->next_cp + 1) % TRACK_CHECKPOINTS;
    }
    const TrackSample *cs = &t->s[k->hit.idx];
    float along = sinf(k->yaw) * cs->tx + cosf(k->yaw) * cs->tz;             /* +1 = facing the right way */
    if (k->speed > 2.0f && along < -0.2f) k->wrongway_t = clampf1(k->wrongway_t + SIM_DT, 0, 10);
    else k->wrongway_t = clampf1(k->wrongway_t - SIM_DT * 2.0f, 0, 10);
}

int kart_hit(Kart *k, float stun_seconds) {
    if (k->shield > 0) { k->shield = 0; k->blocked_flag = 1; return 0; }
    if (k->stun > 0) return 0;
    k->stun = stun_seconds; k->hit_flag = 1; k->drifting = 0; k->drift_charge = 0; k->boost_t = 0; k->boost_mul = 1.0f;
    k->coins -= 3; if (k->coins < 0) k->coins = 0;
    return 1;
}

void kart_step(Kart *k, const Track *t, const KartInput *in_real, float dt) {
    const KartStats *st = &k->st;
    k->skid = 0; k->mini_turbo_tier = 0; k->blocked_flag = 0; k->hit_flag = 0; k->pad_flag = 0;
    if (k->slow_t > 0) k->slow_t -= dt;
    if (k->pad_cd > 0) k->pad_cd -= dt;
    if (k->obstacle_cd > 0) k->obstacle_cd -= dt;
    if (k->item) k->item_hold += dt;
    if (k->shield > 0) k->shield -= dt;
    KartInput stunned_in; const KartInput *in = in_real;
    if (k->stun > 0) {                                    /* spinning out: no control, slowing down */
        k->stun -= dt; k->spin_vis += 9.0f * dt;
        memset(&stunned_in, 0, sizeof stunned_in); stunned_in.brake = 0; in = &stunned_in;
        if (k->speed > 0) k->speed = fmaxf(0, k->speed - 20.0f * dt);
    } else if (k->spin_vis != 0) { k->spin_vis = 0; }
    if (!k->finished) { k->total_time += dt; if (k->lap >= 1) k->lap_time += dt; }
    float top = BASE_TOP * st->top_speed * k->boost_mul * (1.0f + 0.01f * (float)k->coins);
    int on_road = k->hit.on_road;
    if (!on_road && !k->air && k->grace_t <= 0) top *= k->hit.on_field ? (t->water ? 0.42f : 0.5f) : OFFROAD_MUL;   /* (Sean) off the road you really slow down: dirt, grass, sand; wading is slowest */
    if (k->carried_t > 0) { k->carried_t -= dt; k->boost_t = fmaxf(k->boost_t, 0.2f); k->boost_mul = fmaxf(k->boost_mul, 1.7f); k->shield = fmaxf(k->shield, 0.2f); top = BASE_TOP * st->top_speed * 1.7f; }
    if (k->grace_t > 0) { k->grace_t -= dt; top *= 1.12f; }
    if (k->slow_t > 0) top *= 0.62f;
    float accel = BASE_ACCEL * st->accel * (k->boost_t > 0 ? 1.8f : 1.0f);
    float steer = clampf1(in->steer, -1, 1);

    /* ---- drifting: hold the drift button while steering, at speed */
    if (!k->air) {
        if (in->drift && !k->drifting && fabsf(steer) > 0.3f && k->speed > top * 0.35f) {
            k->drifting = 1; k->drift_dir = steer < 0 ? -1 : 1; k->drift_charge = 0; k->hop = 1.0f; k->vy = 3.2f; k->air = 1; k->air_t = 0;
        }
        if (k->drifting && (!in->drift || k->speed < 6.0f)) {
            if (k->drift_charge >= 1.6f) { k->boost_t = 1.5f; k->boost_mul = 1.42f; k->mini_turbo_tier = 2; }
            else if (k->drift_charge >= 0.8f) { k->boost_t = 0.9f; k->boost_mul = 1.25f; k->mini_turbo_tier = 1; }
            k->drifting = 0; k->drift_charge = 0;
        }
    } else if (k->drifting && !in->drift) { k->drifting = 0; k->drift_charge = 0; }
    /* ---- tricks: a tap of the drift button while flying off a jump (not the little drift hop) */
    k->trick_flag = 0;
    if (k->air && in->drift && !k->drift_prev && !k->trick && k->air_t > 0.15f && k->hop <= 0 && k->stun <= 0) { k->trick = 1; k->trick_vis = 0; k->trick_flag = 1; }
    if (k->trick) { k->trick_vis += 7.0f * dt; if (k->trick_vis > 6.2831853f) k->trick_vis = 6.2831853f; }
    k->drift_prev = in->drift;
    if (k->drifting) k->drift_charge += dt * (0.6f + 0.4f * fabsf(steer));

    /* ---- turning */
    float grip = on_road ? 1.0f : 0.8f;
    float spd_f = clampf1(fabsf(k->speed) / 12.0f, 0, 1);                      /* no turning when stopped */
    float turn = 1.9f * st->handling * grip * (1.15f - 0.4f * clampf1(fabsf(k->speed) / BASE_TOP, 0, 1));
    float yaw_rate;
    if (k->drifting) {
        float into = clampf1(steer * (float)k->drift_dir, -1.0f, 1.0f);          /* steer into the drift = tighter, away = wider */
        yaw_rate = (float)k->drift_dir * turn * (0.45f + 0.65f * into);
        k->slide += ((float)k->drift_dir * 0.55f - k->slide) * 6.0f * dt;
        k->skid = 1;
    } else {
        yaw_rate = steer * turn * (k->air ? 0.35f : 1.0f);
        k->slide += (0 - k->slide) * 8.0f * dt;
        if (fabsf(steer) > 0.97f && fabsf(k->speed) > top * 0.85f && !k->air) k->skid = 1;
    }
    if (k->speed < 0) yaw_rate = -yaw_rate;
    k->yaw += yaw_rate * spd_f * dt;
    k->steer_vis += (steer - k->steer_vis) * 10.0f * dt;

    /* ---- speed */
    if (k->boost_t > 0) { k->boost_t -= dt; if (k->boost_t <= 0) k->boost_mul = 1.0f; }
    if (in->gas && !in->brake) k->speed += accel * dt;
    else if (in->brake) { if (k->speed > 0) k->speed -= BRAKE * dt; else k->speed -= accel * 0.5f * dt; }
    else { if (k->speed > 0) k->speed = fmaxf(0, k->speed - COAST * dt); else k->speed = fminf(0, k->speed + COAST * dt); }
    if (!k->air) k->speed -= GRAVITY * k->hit.slope * 0.35f * dt;                 /* hills slow you down and speed you up */
    if (k->speed > top) k->speed = fmaxf(top, k->speed - (k->boost_t > 0 ? (on_road ? 4.0f : 14.0f) : (on_road ? 18.0f : 30.0f)) * dt);   /* off the road the ground drags you down fast, even on a boost */
    if (k->speed < -8.0f * st->top_speed) k->speed = -8.0f * st->top_speed;
    if (k->drifting) k->speed = fmaxf(k->speed - 1.5f * dt, 0);

    /* ---- move */
    float hx = sinf(k->yaw), hz = cosf(k->yaw);
    float sx = sinf(k->yaw - k->slide * 0.7f), sz = cosf(k->yaw - k->slide * 0.7f);    /* drifting: the nose points into the turn, the kart slides toward the outside */
    float ox = k->x, oz = k->z;
    k->x += sx * k->speed * dt; k->z += sz * k->speed * dt;
    (void)hx; (void)hz;

    /* ---- track position, walls */
    int prev_idx = k->hit.idx; float prev_slope = k->hit.slope;
    track_locate(t, k->x, k->z, prev_idx, &k->hit);
    float edge = k->hit.bound;
    k->on_wall_prev = k->on_wall; k->on_wall = 0;
    if (fabsf(k->hit.lateral) > edge) {
        const TrackSample *s = &t->s[k->hit.idx];
        float over = fabsf(k->hit.lateral) - edge, sign = k->hit.lateral > 0 ? 1.0f : -1.0f;
        k->x -= s->nx * sign * (over + 0.05f); k->z -= s->nz * sign * (over + 0.05f);
        /* scrape along the wall: the kart is turned to run along it (never into it, never round), and loses speed by how
           straight it hit; a glancing touch costs little, a head-on hit a lot */
        float wall_dot = (sx * s->nx + sz * s->nz) * sign;           /* how straight into the wall we were going (0 = along it, 1 = head on) */
        if (wall_dot > 0.02f) {
            float road = atan2f(s->tx, s->tz), rel = wrap_pi(k->yaw - road);
            float along = fabsf(rel) < 1.5708f ? road : road + 3.14159265f;     /* the way along the wall closest to where we were heading */
            float off = wrap_pi(k->yaw - along), keep = 1.0f - clampf1(wall_dot * 2.2f, 0.0f, 0.85f);
            k->yaw = along + off * keep;                                        /* swing the nose round toward running along the wall */
            if (!k->on_wall_prev) k->speed *= 1.0f - 0.55f * wall_dot * wall_dot - 0.05f;
            else k->speed *= 1.0f - 0.8f * dt;
        }
        k->on_wall = 1;
        track_locate(t, k->x, k->z, k->hit.idx, &k->hit);
        k->drifting = 0;
    }
    advance_progress(k, t, prev_idx);

    /* ---- pads painted on the road */
    if (!k->air && k->pad_cd <= 0 && k->stun <= 0) {
        for (int p = 0; p < t->npad; p++) {
            const RoadPad *pd = &t->pad[p]; int d = k->hit.idx - pd->idx; if (d > t->n / 2) d -= t->n; if (d < -t->n / 2) d += t->n;
            if (d < -1 || d > 1 || fabsf(k->hit.lateral - pd->lateral) > pd->halfw) continue;
            k->pad_cd = 0.6f; k->pad_flag = pd->kind;
            if (pd->kind == PAD_BOOST) { k->boost_t = fmaxf(k->boost_t, 1.3f); k->boost_mul = fmaxf(k->boost_mul, 1.4f); k->speed = fmaxf(k->speed, BASE_TOP * st->top_speed * 1.1f); }
            else if (pd->kind == PAD_BOUNCE) { k->air = 1; k->air_t = 0; k->vy = 12.0f; k->drifting = 0; }
            break;
        }
    }

    /* ---- vertical: follow the ground, or fly */
    float ground = k->hit.height;
    if (!k->air) {
        int lip = prev_slope > 0.06f && k->hit.slope < prev_slope - 0.12f && k->speed > 12.0f;   /* driving up a ramp that suddenly drops away */
        if (lip || (ground < k->y - 0.35f && k->speed > 8.0f)) {
            k->air = 1; k->air_t = 0;
            k->vy = clampf1(k->speed * fmaxf(prev_slope, 0.08f) * 1.1f, 2.5f, 18.0f);
        } else { k->y = ground; k->vy = 0; }
    }
    if (k->air) {
        k->air_t += dt; k->vy -= GRAVITY * dt; k->y += k->vy * dt;
        if (k->y <= ground) { k->y = ground; k->air = 0; k->squash = clampf1(-k->vy / 12.0f, 0, 1); k->vy = 0; if (k->hop > 0) k->hop = 0;
            if (k->trick) { k->trick = 0; k->trick_vis = 0; if (k->stun <= 0) { k->boost_t = fmaxf(k->boost_t, 0.7f); k->boost_mul = fmaxf(k->boost_mul, 1.22f); k->trick_flag = 2; } } }
    }
    k->hop = fmaxf(0, k->hop - 3.0f * dt); k->squash = fmaxf(0, k->squash - 4.0f * dt);
    (void)ox; (void)oz;
}

void kart_autopilot(const Kart *k, const Track *t, KartInput *in, float skill) {
    memset(in, 0, sizeof *in);
    float dsc = t->def_scale > 0 ? t->def_scale : 1.0f; int look = (int)((5 + k->speed * 0.35f) * dsc);   /* in samples: more of them on the finer-sampled roads */
    const TrackSample *h = &t->s[(k->hit.idx + 2) % t->n], *a = &t->s[(k->hit.idx + look) % t->n], *b = &t->s[(k->hit.idx + look * 2) % t->n], *c = &t->s[(k->hit.idx + look * 3) % t->n];
    float ang_h = atan2f(h->tx, h->tz), ang_a = atan2f(a->tx, a->tz), ang_b = atan2f(b->tx, b->tz), ang_c = atan2f(c->tx, c->tz);
    float curve_now = wrap_pi(ang_a - ang_h);                      /* the bend we are in right now */
    float curve = wrap_pi(ang_b - ang_a), curve_far = wrap_pi(ang_c - ang_b);
    /* racing line: aim a little to the inside of the upcoming curve, never closer than 2.5 m to the edge */
    float half = a->width * 0.5f, room = fmaxf(half - 2.5f, 0.5f);
    float want_lat = clampf1(-curve * 5.0f, -room, room) * (0.5f + 0.5f * skill);
    float aimx = a->x + a->nx * want_lat, aimz = a->z + a->nz * want_lat;
    float want = atan2f(aimx - k->x, aimz - k->z);
    float err = wrap_pi(want - k->yaw);
    in->steer = clampf1(err * 2.5f, -1, 1);
    /* how hard to slow: the turn over the whole stretch ahead, so a quick left-right S-bend (which the racing line runs
       nearly straight through) does not look like two hairpins */
    float net = wrap_pi(ang_b - ang_h), net_far = wrap_pi(ang_c - ang_a);
    float sharp = fmaxf(fminf(fabsf(curve), fabsf(net) * 0.6f + 0.15f), fminf(fabsf(curve_far), fabsf(net_far) * 0.6f + 0.15f) * 0.7f);
    float limit = BASE_TOP * k->st.top_speed * (1.2f - 0.95f * clampf1(sharp * 1.3f, 0, 1)) * (0.60f + 0.40f * skill);   /* lower skill = never quite flat out */
    in->gas = k->speed < limit;
    in->brake = k->speed > limit * 1.3f && !k->air;
    /* drift through real corners (press hard into the turn so the drift starts), let go when it straightens so the mini-turbo fires */
    int want_drift = (fabsf(curve) > 0.5f && fabsf(curve_now) > 0.2f && k->speed > 18.0f) || (k->drifting && (fabsf(curve_now) > 0.12f || k->drift_charge < 0.85f));
    if (want_drift && skill > 0.5f) {
        in->drift = 1;
        float dir = curve_now > 0 ? 1.0f : -1.0f;
        if (!k->drifting) in->steer = dir * fmaxf(fabsf(in->steer), 0.7f);
    }
    if (k->wrongway_t > 0.6f || k->on_wall) {                      /* lost: turn toward the road's direction and get going */
        const TrackSample *cs = &t->s[k->hit.idx];
        float e2 = wrap_pi(atan2f(cs->tx, cs->tz) - k->yaw);
        in->steer = clampf1(e2 * 3.0f, -1, 1); in->gas = 1; in->brake = 0; in->drift = 0;
    }
}
