// tracks.c - the roads: control points, lighting, textures and scenery for each one.
#include "theme.h"

// ------------------------------------------------------------------ Test Loop (development track)
static const TrackPoint TEST_PTS[] = {
    {   0, 0.0f,    0, 14, 0.00f},
    {   0, 0.0f,   60, 14, 0.00f},
    {   8, 1.5f,  120, 14, 0.00f},
    {  30, 4.0f,  160, 13, 0.00f},
    {  60, 2.0f,  185, 13, 0.00f},
    { 100, 0.0f,  195, 14, 0.00f},
    { 140, 0.0f,  180, 14, 0.25f},
    { 165, 0.0f,  140, 14, 0.35f},
    { 160, 0.0f,  100, 14, 0.20f},
    { 135, 0.0f,   75, 13, 0.00f},
    { 121, 0.0f,   48, 12, 0.00f},
    { 118, 4.0f,   22, 12, 0.00f, 1},
    { 115, 0.0f,   -8, 14, 0.00f},
    { 112, 0.0f,  -40, 13, 0.00f},
    {  95, 0.0f,  -62, 12, 0.00f},
    {  70, 0.0f,  -72, 12, 0.00f},
    {  40, 2.5f,  -70, 13, 0.00f},
    {  15, 0.5f,  -58, 14, 0.00f},
    {   2, 0.0f,  -35, 14, 0.00f},
};
static const Scenery TEST_SCENERY[] = {
    {"#tree", 10, -12, 7, 0}, {"#tree", 24, 13, 8, 0}, {"#tree", 40, -13, 6, 0}, {"#tree", 55, 14, 9, 0}, {"#tree", 72, -14, 7, 0},
    {"#tree", 90, 13, 8, 0}, {"#tree", 110, -13, 7, 0}, {"#tree", 130, 14, 8, 0}, {"#tree", 150, -13, 6, 0}, {"#tree", 170, 13, 8, 0},
    {"#tree", 190, -14, 7, 0}, {"#tree", 250, 13, 8, 0}, {"#tree", 270, -13, 7, 0}, {"#tree", 300, 13, 9, 0}, {"#tree", 330, -13, 7, 0}, {"#tree", 360, 13, 8, 0},
};

// ------------------------------------------------------------------ Bethlehem Road: a village at night under the Star
// Start on the main street heading north, curve into the village square, climb the hill past the stable,
// switch back down through the olive groves, and cut through the alley by the inn back to the start.
static const TrackPoint BETH_PTS[] = {
    {    0, 0.0f,    0, 12, 0.00f},   /* start on the main street */
    {    0, 0.0f,   50, 12, 0.00f},
    {  -10, 0.0f,   95, 13, 0.00f},
    {  -35, 0.5f,  120, 16, 0.00f},   /* village square, wide */
    {  -70, 1.5f,  125, 14, 0.00f},
    { -100, 4.0f,  110, 13, 0.00f},   /* climbing */
    { -120, 7.0f,   80, 12, 0.00f},
    { -125, 9.0f,   45, 12, 0.00f},   /* hilltop: the stable and the Star */
    { -110, 8.0f,   15, 12, 0.00f},
    {  -85, 5.0f,    0, 11, 0.00f},   /* coming down */
    {  -70, 3.0f,  -25, 11, 0.12f},   /* switchback, banked a little */
    {  -95, 2.0f,  -50, 12, 0.00f},
    {  -75, 0.5f,  -75, 12, 0.00f},
    {  -40, 0.0f,  -80, 13, 0.00f},   /* tight corner: the alley shortcut is on the inside */
    {  -15, 0.0f,  -60, 12, 0.00f},
    {   -5, 0.0f,  -30, 12, 0.00f},
};
static const Scenery BETH_SCENERY[] = {
    /* main street: houses and lanterns both sides */
    {"#house1", 6, -12, 7, 0}, {"#lantern", 12, 11, 4, 3}, {"#house2", 18, 12, 9, 0}, {"#house1", 26, -12, 7, 0}, {"#lantern", 32, -11, 4, 3},
    {"#house2", 38, 12, 9, 0}, {"#house1", 46, 12, 7, 0}, {"#lantern", 52, 12, 4, 3}, {"#house1", 58, -12, 7, 0},
    /* the square: palms, the well, a lantern ring */
    {"#palm", 66, -14, 11, 0}, {"#palm", 74, 15, 12, 0}, {"#well", 80, -15, 4, 2}, {"#lantern", 86, 14, 4, 3}, {"#palm", 92, -15, 11, 0}, {"#house2", 100, 15, 9, 0},
    /* up the hill: olive trees and sheep */
    {"#olive", 112, -12, 6, 0}, {"#olive", 122, 12, 7, 0}, {"#olive", 134, -13, 6, 0}, {"#sheep", 142, 12, 2.2f, 0}, {"#olive", 150, 13, 7, 0},
    {"#olive", 160, -12, 6, 0}, {"#lantern", 168, 11, 4, 3},
    /* hilltop: the stable with the manger glow, sheep and a palm */
    {"#stable", 180, 13, 7, 5}, {"#sheep", 186, 11, 2.2f, 0}, {"#palm", 192, -13, 11, 0}, {"#olive", 204, 12, 6, 0},
    /* down the switchbacks */
    {"#olive", 216, -12, 6, 0}, {"#lantern", 226, 10, 4, 3}, {"#olive", 236, 12, 7, 0}, {"#house1", 248, -12, 7, 0}, {"#olive", 258, 13, 6, 0},
    {"#olive", 270, -12, 7, 0}, {"#lantern", 280, 11, 4, 3}, {"#house2", 292, 13, 9, 0},
    /* the inn and the alley back to the start */
    {"#inn", 296, -13, 9, 4}, {"#lantern", 302, 11, 4, 3}, {"#house1", 308, 12, 7, 0}, {"#house2", 312, -12, 9, 0}, {"#lantern", 316, -11, 4, 3}, {"#house1", 318, 12, 7, 0},
};

const TrackDef TRACK_DEFS[] = {
    { "Bethlehem Road", 3, BETH_PTS, (int)(sizeof BETH_PTS / sizeof BETH_PTS[0]),
      0.03f, 0.04f, 0.14f,                      /* sky */
      0.05f, 0.06f, 0.18f, 70.0f, 320.0f,       /* fog */
      -0.35f, 1.0f, 0.25f, 0.38f,               /* moonlight direction, ambient */
      0.55f, 0.62f, 0.88f,                      /* cool moonlight tint */
      1.0f, 0.72f, 0.35f,                       /* warm lantern glow */
      1, "beth_road", "beth_dirt", "beth_grass", "beth_wall", "beth_sky",
      BETH_SCENERY, (int)(sizeof BETH_SCENERY / sizeof BETH_SCENERY[0]),
      -230.0f, 120.0f, 60.0f, 46.0f,            /* the Star, high over the hill */
      300, 335, -1 },                           /* alley shortcut: inside of the tight corner */
    { "Test Loop", 3, TEST_PTS, (int)(sizeof TEST_PTS / sizeof TEST_PTS[0]),
      0.55f, 0.75f, 0.98f,
      0.72f, 0.82f, 0.95f, 120.0f, 520.0f,
      0.35f, 1.0f, 0.5f, 0.45f,
      1.0f, 1.0f, 1.0f,
      1.0f, 0.9f, 0.6f,
      0, "test_road", "test_dirt", "test_grass", "test_wall", "test_sky",
      TEST_SCENERY, (int)(sizeof TEST_SCENERY / sizeof TEST_SCENERY[0]),
      0, 0, 0, 0,
      -1, -1, 0 },
};
const int NUM_TRACK_DEFS = (int)(sizeof TRACK_DEFS / sizeof TRACK_DEFS[0]);

void track_from_def(Track *t, const TrackDef *d) {
    track_build(t, d->name, d->laps, d->points, d->npoints, 20);
    t->shortcut_from = d->shortcut_from; t->shortcut_to = d->shortcut_to; t->shortcut_side = d->shortcut_side;
}
void track_make_test(Track *t) { track_from_def(t, &TRACK_DEFS[1]); }
