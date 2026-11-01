#include "track.h"
#include <stdio.h>

static float catmull(float p0, float p1, float p2, float p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
}

void track_build(Track *t, const char *name, int laps, const TrackPoint *pts, int npts, int per) {
    t->name = name; t->laps = laps; t->n = 0; t->shortcut_from = t->shortcut_to = -1; t->shortcut_side = 0;
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

float track_surface_height(const Track *t, int idx, float lateral) {
    const TrackSample *s = &t->s[idx];
    float half = s->width * 0.5f, a = fabsf(lateral);
    float y = s->y + lateral * tanf(s->bank);
    if (a > half) { float u = (a - half) / SHOULDER_W; if (u > 1) u = 1; y -= WALL_DROP * u * u; }   /* shoulder slopes down */
    return y;
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
    {   float half = width * 0.5f, a = fabsf(out->lateral);
        float y = cy + out->lateral * tanf(bank);
        if (a > half) { float w = (a - half) / SHOULDER_W; if (w > 1) w = 1; y -= WALL_DROP * w * w; }
        out->height = y; }
    const TrackSample *nx1 = &t->s[(best + 1) % t->n], *pv = &t->s[(best + t->n - 1) % t->n];
    float ddist = 2.0f * t->length / (float)t->n;
    out->slope = (nx1->y - pv->y) / ddist;
    float a = fabsf(out->lateral);
    out->on_road = a <= width * 0.5f;
    out->on_shoulder = !out->on_road && a <= width * 0.5f + SHOULDER_W;
    /* a shortcut: the shoulder on one side of a corner is packed dirt, as fast as the road */
    if (out->on_shoulder && t->shortcut_from >= 0 && best >= t->shortcut_from && best <= t->shortcut_to && (out->lateral < 0 ? -1 : 1) == t->shortcut_side) out->on_road = 1;
}

