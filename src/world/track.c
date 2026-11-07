#include "track.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float catmull(float p0, float p1, float p2, float p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
}

void track_build(Track *t, const char *name, int laps, const TrackPoint *pts, int npts, int per) {
    t->name = name; t->laps = laps; t->n = 0; t->shortcut_from = t->shortcut_to = -1; t->shortcut_side = 0;
    memset(t->field, 0, sizeof t->field); t->ngem = 0; t->water_y = -1e9f;
    int total = npts * per; if (total > TRACK_MAX_SAMPLES) { per = TRACK_MAX_SAMPLES / npts; total = npts * per; }
    for (int i = 0; i < npts; i++) {
        const TrackPoint *p0 = &pts[(i + npts - 1) % npts], *p1 = &pts[i], *p2 = &pts[(i + 1) % npts], *p3 = &pts[(i + 2) % npts];
        for (int k = 0; k < per; k++) {
            float u = (float)k / (float)per;
            TrackSample *s = &t->s[t->n++];
            s->x = catmull(p0->x, p1->x, p2->x, p3->x, u);
            s->y = catmull(p0->y, p1->y, p2->y, p3->y, u);
            s->z = catmull(p0->z, p1->z, p2->z, p3->z, u);
            s->width = catmull(p0->width, p1->width, p2->width, p3->width, u);
            s->bank = catmull(p0->bank, p1->bank, p2->bank, p3->bank, u);
            if (p1->jump) { s->y = (k == 0) ? p1->y : p2->y; }                   /* a jump: the lip, then the road falls away to the landing height */
            if (p0->jump && k < per / 2) s->y = p1->y;                           /* keep the landing flat so the kart lands on it */
        }
    }
    // tangents, normals and distances
    t->length = 0;
    for (int i = 0; i < t->n; i++) {
        TrackSample *s = &t->s[i], *nx = &t->s[(i + 1) % t->n], *pv = &t->s[(i + t->n - 1) % t->n];
        float dx = nx->x - pv->x, dz = nx->z - pv->z, l = sqrtf(dx * dx + dz * dz); if (l < 1e-5f) l = 1e-5f;
        s->tx = dx / l; s->tz = dz / l; s->nx = -s->tz; s->nz = s->tx;
        s->dist = t->length;
        float ex = nx->x - s->x, ez = nx->z - s->z; t->length += sqrtf(ex * ex + ez * ez);
    }
    for (int c = 0; c < TRACK_CHECKPOINTS; c++) t->checkpoint[c] = (t->n * c) / TRACK_CHECKPOINTS;
}

float track_edge(const Track *t, int idx) { return t->s[idx].width * 0.5f + SHOULDER_W; }

float track_bound(const Track *t, int idx, int side) { return track_edge(t, idx) + t->field[idx][side > 0]; }

/* the height of the ground at a distance from the center line: the banked road, the shoulder sloping down, then the
   open land, level at the height the shoulder ends (or the water's surface on a water road) */
static float surface_y(const Track *t, float y, float half, float bank, float lateral) {
    float a = fabsf(lateral), edge = half + SHOULDER_W;
    float bl = lateral; if (bl > edge) bl = edge; if (bl < -edge) bl = -edge;   /* the bank does not carry on tilting the land */
    float h = y + bl * tanf(bank);
    if (a > half) { float u = (a - half) / SHOULDER_W; if (u > 1) u = 1; h -= WALL_DROP * u * u; }   /* shoulder slopes down */
    if (t->water && t->water_y > -1e8f && a > edge + 0.5f) h = t->water_y;                           /* wading */
    return h;
}
float track_surface_height(const Track *t, int idx, float lateral) {
    const TrackSample *s = &t->s[idx];
    return surface_y(t, s->y, s->width * 0.5f, s->bank, lateral);
}

void track_make_field(Track *t, int water) {
    /* walk outward from each stretch of road a meter at a time; stop where the nearest bit of road would be some other
       part of the course (half way to it, less a margin), so open land never joins two parts of the road together */
    const int near_idx = 9;
    for (int i = 0; i < t->n; i++) for (int sd = 0; sd < 2; sd++) {
        const TrackSample *s = &t->s[i]; float side = sd ? 1.0f : -1.0f, edge = track_edge(t, i), got = 0;
        for (float L = edge + 2.0f; L <= edge + FIELD_MAX + 3.0f; L += 2.0f) {
            float px = s->x + s->nx * side * L, pz = s->z + s->nz * side * L; int best = i; float bd = 1e30f;
            for (int j = 0; j < t->n; j++) { float dx = px - t->s[j].x, dz = pz - t->s[j].z, d = dx * dx + dz * dz - t->s[j].width * t->s[j].width * 0.25f; if (d < bd) { bd = d; best = j; } }
            int di = abs(best - i); if (di > t->n / 2) di = t->n - di;
            if (di > near_idx) break;
            got = L - edge;
        }
        got -= 3.0f; if (got < 0) got = 0; if (got > FIELD_MAX) got = FIELD_MAX;
        t->field[i][sd] = got;
    }
    /* no open land in tunnels, and none where it would be only a sliver */
    for (int k = 0; k < t->ntunnel; k++) { int len = (t->tunnel[k].to - t->tunnel[k].from + t->n) % t->n; for (int q = -2; q <= len + 2; q++) { int i = ((t->tunnel[k].from + q) % t->n + t->n) % t->n; t->field[i][0] = t->field[i][1] = 0; } }
    for (int i = 0; i < t->n; i++) for (int sd = 0; sd < 2; sd++) if (t->field[i][sd] < 4.0f) t->field[i][sd] = 0;
    /* smooth: the wall never swings in or out by more than a meter per stretch, so it is a gentle slant, never a step across */
    float step = 1.0f;
    for (int pass = 0; pass < 3; pass++) for (int sd = 0; sd < 2; sd++) {
        for (int k = 0; k < 2 * t->n; k++) { int i = k % t->n, p = (i + t->n - 1) % t->n; if (t->field[i][sd] > t->field[p][sd] + step) t->field[i][sd] = t->field[p][sd] + step; }
        for (int k = 2 * t->n - 1; k >= 0; k--) { int i = k % t->n, q = (i + 1) % t->n; if (t->field[i][sd] > t->field[q][sd] + step) t->field[i][sd] = t->field[q][sd] + step; }
    }
    /* water roads: the open land is shallow water just under the road's level */
    if (water) { float lo = 1e9f; for (int i = 0; i < t->n; i++) { float y = t->s[i].y - WALL_DROP; if (y < lo) lo = y; } t->water_y = lo - 0.3f; }
    /* out in the open land: a few boost arrows and bounce ramps, each in a wide stretch, away from each other */
    int added = 0;
    for (int i = 20; i < t->n && added < 8 && t->npad < MAX_PADS; i += 3) {
        int sd = ((i / 11) & 1); float f = t->field[i][sd];
        if (f < 11.0f || t->field[(i + 6) % t->n][sd] < 11.0f) continue;
        float frac = (added & 2) ? 0.7f : 0.4f;
        int ok = 1; for (int p = 0; p < t->npad; p++) { int di = abs(t->pad[p].idx - i); if (di > t->n / 2) di = t->n - di; if (di < (fabsf(t->pad[p].lateral) > track_edge(t, t->pad[p].idx) ? t->n / 6 : 3)) ok = 0; }
        if (!ok) continue;
        RoadPad *pd = &t->pad[t->npad++]; pd->idx = i; pd->lateral = (sd ? 1.0f : -1.0f) * (track_edge(t, i) + f * frac); pd->halfw = 2.0f; pd->kind = (added & 1) ? PAD_BOUNCE : PAD_BOOST; added++;
    }
    /* hidden gems: in the widest stretches of open land, out of sight of the racing line */
    t->ngem = 0;
    for (int i = 0; i < t->n && t->ngem < MAX_GEMS; i++) {
        int sd = (i / 7) & 1; float f = t->field[i][sd];
        if (f < 9.0f || i < 12) continue;
        int ok = 1; for (int g = 0; g < t->ngem; g++) { int di = abs(t->gem[g].idx - i); if (di > t->n / 2) di = t->n - di; if (di < t->n / 9) ok = 0; }
        for (int p = 0; p < t->npad; p++) { int di = abs(t->pad[p].idx - i); if (di > t->n / 2) di = t->n - di; if (di < 4 && fabsf(t->pad[p].lateral) > track_edge(t, i)) ok = 0; }
        if (!ok) continue;
        t->gem[t->ngem].idx = i; t->gem[t->ngem].lateral = (sd ? 1.0f : -1.0f) * (track_edge(t, i) + f * 0.7f); t->gem[t->ngem].color = t->ngem % 3; t->ngem++;
    }
}

void track_locate(const Track *t, float x, float z, int hint, TrackHit *out) {
    int best = -1; float bd = 1e30f;
    int lo = 0, hi = t->n - 1;
    if (hint >= 0) { lo = hint - 24; hi = hint + 24; }
    for (int k = lo; k <= hi; k++) {
        int i = ((k % t->n) + t->n) % t->n;
        float dx = x - t->s[i].x, dz = z - t->s[i].z, d = dx * dx + dz * dz;
        if (d < bd) { bd = d; best = i; }
    }
    if (hint >= 0 && bd > 60.0f * 60.0f) { track_locate(t, x, z, -1, out); return; }   /* lost: full search */
    /* project onto the segment toward the neighbor that is ahead of or behind us, so height and width are smooth */
    const TrackSample *s = &t->s[best];
    float dx = x - s->x, dz = z - s->z;
    float ahead = dx * s->tx + dz * s->tz;
    int j = ahead >= 0 ? (best + 1) % t->n : (best + t->n - 1) % t->n;
    const TrackSample *q = &t->s[j];
    float ex = q->x - s->x, ez = q->z - s->z, el = ex * ex + ez * ez;
    float u = el > 1e-6f ? clampf((dx * ex + dz * ez) / el, 0.0f, 1.0f) : 0.0f;
    float cx = s->x + ex * u, cz = s->z + ez * u, cy = s->y + (q->y - s->y) * u;
    float width = s->width + (q->width - s->width) * u, bank = s->bank + (q->bank - s->bank) * u;
    float tx = s->tx + (q->tx - s->tx) * u, tz = s->tz + (q->tz - s->tz) * u, tl = sqrtf(tx * tx + tz * tz); if (tl < 1e-5f) tl = 1e-5f; tx /= tl; tz /= tl;
    float nx = -tz, nz = tx;
    out->idx = best;
    out->lateral = (x - cx) * nx + (z - cz) * nz;
    out->height = surface_y(t, cy, width * 0.5f, bank, out->lateral);
    {   int sd = out->lateral > 0; float f = t->field[best][sd] + (t->field[j][sd] - t->field[best][sd]) * u;
        out->bound = width * 0.5f + SHOULDER_W + f; }
    const TrackSample *nx1 = &t->s[(best + 1) % t->n], *pv = &t->s[(best + t->n - 1) % t->n];
    float ddist = 2.0f * t->length / (float)t->n;
    out->slope = (nx1->y - pv->y) / ddist;
    float a = fabsf(out->lateral);
    out->on_road = a <= width * 0.5f;
    out->on_shoulder = !out->on_road && a <= width * 0.5f + SHOULDER_W;
    out->on_field = a > width * 0.5f + SHOULDER_W;
    /* a shortcut: the shoulder on one side of a corner is packed dirt, as fast as the road */
    if (out->on_shoulder && t->shortcut_from >= 0 && best >= t->shortcut_from && best <= t->shortcut_to && (out->lateral < 0 ? -1 : 1) == t->shortcut_side) out->on_road = 1;
}


void track_mover_pos(const Track *t, const Mover *m, float time, float *x, float *y, float *z) {
    int idx = m->idx % t->n; const TrackSample *s = &t->s[idx];
    float lat = m->lateral + m->amp * sinf(time * m->speed + m->phase);
    float lim = track_edge(t, idx) - 0.5f; if (lat > lim) lat = lim; if (lat < -lim) lat = -lim;
    *x = s->x + s->nx * lat; *z = s->z + s->nz * lat; *y = track_surface_height(t, idx, lat);
}
