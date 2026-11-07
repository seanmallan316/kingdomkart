#include <math.h>
#include <string.h>
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
    {"#crystal", 230, -12, 3, 2}, {"#crystal", 238, 12, 3, 2}, {"#crystal", 244, -12, 2.5f, 2},
    {"#inn", 296, -13, 9, 4}, {"#lantern", 302, 11, 4, 3}, {"#house1", 308, 12, 7, 0}, {"#house2", 312, -12, 9, 0}, {"#lantern", 316, -11, 4, 3}, {"#house1", 318, 12, 7, 0},
};

static const RoadPad BETH_PADS[] = {
    {26, -2.5f, 2.2f, PAD_BOOST}, {30, 2.5f, 2.2f, PAD_BOOST},       /* main street: two arrows */
    {78, 0.0f, 3.0f, PAD_BOUNCE},                                    /* a hop in the village square */
    {150, -3.0f, 2.0f, PAD_BOOST},                                    /* help up the hill */
    {250, 0.0f, 2.5f, PAD_BOOST},                                     /* down the switchback */
};
static const CoinRow BETH_COINS[] = { {44, 5, 3, 2.5f}, {100, 5, 3, -3.0f}, {160, 4, 4, 3.0f}, {212, 5, 3, -2.5f}, {262, 5, 3, 2.0f} };
static const Tunnel BETH_TUNNELS[] = { {226, 246, 5.5f} };
static const Mover BETH_MOVERS[] = {
    {"#sheep", 136, 0.0f, 7.0f, 0.55f, 0.0f, 1.6f, 2.0f},            /* sheep wandering across the hill road */
    {"#sheep", 198, 0.0f, 6.0f, 0.45f, 2.0f, 1.6f, 2.0f},
};

// ------------------------------------------------------------------ Sea of Galilee: a bright day on the shore, out along the causeway over the water
// Start on the beach road, swing out onto the stone causeway across the water, jump the gap at its end onto the
// sandbar, loop back through the fishing village and the palm grove.
static const TrackPoint GAL_PTS[] = {
    {    0, 0.0f,    0, 13, 0.00f},   /* start: beach road */
    {    5, 0.0f,   55, 13, 0.00f},
    {   25, 0.0f,  100, 13, 0.10f},
    {   70, 0.0f,  125, 12, 0.00f},   /* onto the causeway */
    {  125, 0.0f,  130, 11, 0.00f},
    {  175, 1.5f,  120, 11, 0.00f},   /* the causeway rises to the jump */
    {  205, 3.0f,  100, 11, 0.00f, 1},/* jump the broken end onto the sandbar */
    {  215, 0.0f,   60, 13, 0.00f},
    {  200, 0.0f,   15, 13, 0.20f},   /* banked sweep on the sand */
    {  165, 0.0f,  -15, 13, 0.00f},
    {  120, 0.0f,  -25, 12, 0.00f},   /* fishing village street */
    {   85, 0.0f,  -50, 12, 0.00f},
    {   55, 0.0f,  -75, 12, 0.00f},   /* palm grove */
    {   20, 0.0f,  -70, 13, 0.00f},
    {    0, 0.0f,  -40, 13, 0.00f},
};
static const Scenery GAL_SCENERY[] = {
    {"#palm", 8, -12, 11, 0}, {"#hut", 16, 12, 6, 0}, {"#net", 24, -12, 4, 0}, {"#palm", 34, 12, 12, 0}, {"#rock", 44, -12, 3, 0},
    {"#boat", 70, 14, 4, 0}, {"#waterfall", 86, -17, 9, 0}, {"#boat", 92, -16, 4, 0}, {"#rock", 104, 12, 3, 0}, {"#boat", 118, 15, 4, 0}, {"#boat", 140, -15, 4, 0},
    {"#rock", 160, 12, 3, 0}, {"#palm", 180, -13, 11, 0}, {"#rock", 190, 13, 3, 0},
    {"#hut", 206, -12, 6, 0}, {"#net", 214, 12, 4, 0}, {"#hut", 222, 12, 6, 0}, {"#boat", 232, -13, 4, 0}, {"#hut", 240, -12, 6, 0}, {"#net", 248, 12, 4, 0},
    {"#crystal", 258, -12, 3, 2}, {"#crystal", 264, 13, 3, 2}, {"#crystal", 270, -13, 2.5f, 2}, {"#palm", 282, 12, 12, 0}, {"#rock", 290, -12, 3, 0}, {"#palm", 296, 12, 11, 0},
};
static const RoadPad GAL_PADS[] = {
    {12, 0.0f, 2.5f, PAD_BOOST}, {18, 0.0f, 2.5f, PAD_BOOST},         /* beach straight */
    {118, 0.0f, 2.5f, PAD_BOOST},                                      /* run-up to the causeway jump */
    {175, -3.0f, 2.0f, PAD_BOUNCE},                                     /* hop on the sand */
    {272, 2.5f, 2.0f, PAD_BOOST},
};
static const CoinRow GAL_COINS[] = { {30, 5, 3, -3.0f}, {84, 6, 3, 2.5f}, {150, 5, 3, -2.5f}, {200, 5, 3, 3.0f}, {280, 5, 3, -2.0f} };
static const Tunnel GAL_TUNNELS[] = { {254, 272, 5.5f} };
static const Mover GAL_MOVERS[] = {
    {"#net", 92, 0.0f, 5.5f, 0.5f, 0.0f, 1.4f, 2.4f},                 /* fishermen hauling nets across the causeway */
    {"#net", 222, 0.0f, 5.0f, 0.6f, 1.5f, 1.4f, 2.4f},
};

// ------------------------------------------------------------------ Jerusalem Road: golden afternoon, up through the gate into the city streets
static const TrackPoint JERU_PTS[] = {
    {    0, 0.0f,    0, 13, 0.00f},   /* start on the pilgrim road below the walls */
    {    0, 1.0f,   55, 13, 0.00f},
    {  -15, 3.0f,  100, 13, 0.00f},   /* climbing to the gate */
    {  -45, 5.0f,  125, 12, 0.00f},
    {  -85, 6.0f,  120, 11, 0.00f},   /* through the gate: narrow */
    { -115, 6.0f,   95, 11, 0.00f},   /* market street */
    { -130, 6.0f,   60, 11, 0.15f},
    { -125, 8.0f,   25, 11, 0.00f},   /* up the temple steps */
    { -110, 9.0f,   -5, 12, 0.00f, 1},/* jump off the top of the stairs */
    {  -90, 5.0f,  -35, 12, 0.00f},
    {  -70, 3.0f,  -65, 12, 0.10f},
    {  -40, 1.0f,  -80, 13, 0.00f},   /* back outside the walls, olive slope */
    {  -10, 0.0f,  -70, 13, 0.00f},
    {    3, 0.0f,  -35, 13, 0.00f},
};
static const Scenery JERU_SCENERY[] = {
    {"#cypress", 8, -12, 8, 0}, {"#pillar", 14, 12, 5, 0}, {"#cypress", 22, 12, 8, 0}, {"#pillar", 30, -12, 5, 0}, {"#cypress", 38, -12, 8, 0},
    {"#tower", 52, 12, 14, 0}, {"#wallseg", 60, -13, 9, 0}, {"#tower", 62, -13, 14, 0}, {"#lantern", 72, 12, 4, 3}, {"#lantern", 78, -12, 4, 3}, {"#wallseg", 86, 12, 9, 0},
    {"#stall", 96, -12, 5, 0}, {"#house2", 102, 12, 9, 0}, {"#stall", 108, 12, 5, 0}, {"#house1", 114, -12, 7, 0}, {"#stall", 120, -12, 5, 0}, {"#house2", 126, 12, 9, 0},
    {"#pillar", 134, -12, 5, 0}, {"#waterfall", 138, 16, 8, 0}, {"#pillar", 146, -12, 5, 0}, {"#pillar", 152, 12, 5, 0},
    {"#tower", 164, 13, 14, 0}, {"#cypress", 176, -12, 8, 0}, {"#wallseg", 184, 12, 9, 0},
    {"#olive", 196, -12, 6, 0}, {"#olive", 204, 12, 7, 0}, {"#olive", 212, -13, 6, 0}, {"#olive", 222, 12, 7, 0}, {"#crystal", 240, -12, 3, 2}, {"#crystal", 246, 12, 3, 2},
    {"#cypress", 244, 12, 8, 0}, {"#olive", 252, -12, 6, 0}, {"#cypress", 262, 12, 8, 0}, {"#pillar", 270, -12, 5, 0},
};
static const RoadPad JERU_PADS[] = {
    {16, -2.5f, 2.0f, PAD_BOOST}, {20, 2.5f, 2.0f, PAD_BOOST},         /* the pilgrim road, palm branches waving */
    {90, 0.0f, 2.5f, PAD_BOOST},                                       /* through the gate */
    {150, 0.0f, 2.5f, PAD_BOOST},                                      /* run-up to the stairs jump */
    {230, 0.0f, 3.0f, PAD_BOUNCE},                                     /* a hop on the olive slope */
};
static const CoinRow JERU_COINS[] = { {26, 5, 3, 2.5f}, {100, 5, 3, -2.5f}, {134, 6, 3, 2.5f}, {196, 5, 3, -3.0f}, {250, 5, 3, 2.5f} };
static const Tunnel JERU_TUNNELS[] = { {66, 84, 6.0f}, {236, 250, 5.0f} };
static const Mover JERU_MOVERS[] = {
    {"#cart", 106, 0.0f, 5.0f, 0.7f, 0.0f, 1.8f, 2.6f},               /* market carts crossing the street */
    {"#cart", 124, 0.0f, 5.0f, 0.6f, 2.5f, 1.8f, 2.6f},
};

// ------------------------------------------------------------------ Sunrise at the Empty Tomb: dawn in the garden, the stone rolled away
static const TrackPoint TOMB_PTS[] = {
    {    0, 0.0f,    0, 13, 0.00f},   /* start on the garden path */
    {    0, 0.5f,   50, 13, 0.00f},
    {   15, 2.0f,   95, 13, 0.00f},
    {   50, 4.0f,  120, 12, 0.00f},   /* climbing past the olive trees */
    {   95, 5.0f,  115, 12, 0.10f},
    {  125, 5.5f,   85, 12, 0.00f, 1},/* the garden wall: a jump down */
    {  130, 1.0f,   45, 13, 0.00f},
    {  115, 0.0f,    5, 14, 0.00f},   /* the tomb clearing, wide */
    {   85, 0.0f,  -25, 13, 0.00f},
    {   50, 1.0f,  -45, 12, 0.00f},
    {   15, 2.5f,  -60, 12, 0.15f},   /* banked bend below the three hills */
    {  -20, 1.5f,  -50, 12, 0.00f},
    {  -25, 0.0f,  -15, 13, 0.00f},
};
static const Scenery TOMB_SCENERY[] = {
    {"#olive", 6, -12, 6, 0}, {"#lily", 12, 12, 2, 0}, {"#olive", 20, 12, 7, 0}, {"#lily", 28, -12, 2, 0}, {"#olive", 36, -12, 6, 0}, {"#lily", 44, 12, 2, 0},
    {"#crystal", 44, 12, 3, 2}, {"#crystal", 50, -12, 3, 2}, {"#olive", 70, -12, 6, 0}, {"#olive", 80, 12, 7, 0}, {"#rock", 90, -12, 3, 0},
    {"#waterfall", 98, 16, 10, 0}, {"#rock", 100, -12, 3, 0}, {"#olive", 106, -12, 6, 0},
    {"#lily", 124, -13, 2, 0}, {"#lily", 130, 13, 2, 0}, {"#tomb", 142, 14, 8, 6}, {"angel", 146, 11, 5, 4}, {"#lily", 150, -12, 2, 0}, {"#rock", 156, -12, 3, 0},
    {"#olive", 166, 12, 7, 0}, {"#lily", 172, -12, 2, 0}, {"#olive", 180, -12, 6, 0}, {"#crosses", 196, -16, 10, 0}, {"#olive", 206, 12, 7, 0},
    {"#rock", 216, -12, 3, 0}, {"#olive", 226, 12, 6, 0}, {"#lily", 236, -12, 2, 0}, {"#olive", 246, 12, 7, 0}, {"#lily", 254, -12, 2, 0},
};
static const RoadPad TOMB_PADS[] = {
    {14, 0.0f, 2.5f, PAD_BOOST},
    {92, 0.0f, 2.5f, PAD_BOOST},                                       /* run-up to the wall jump */
    {140, -3.0f, 2.0f, PAD_BOUNCE}, {140, 3.0f, 2.0f, PAD_BOUNCE},     /* two hops by the tomb */
    {200, 0.0f, 2.5f, PAD_BOOST},
};
static const CoinRow TOMB_COINS[] = { {26, 5, 3, 2.5f}, {70, 5, 3, -2.5f}, {120, 5, 3, 3.0f}, {176, 5, 3, -2.5f}, {236, 5, 3, 2.5f} };
static const Tunnel TOMB_TUNNELS[] = { {40, 56, 5.5f} };
static const Mover TOMB_MOVERS[] = {
    {"#stone", 160, 0.0f, 5.5f, 0.9f, 0.0f, 1.5f, 2.2f},              /* rolling stones */
    {"#stone", 214, 0.0f, 5.0f, 0.8f, 2.0f, 1.5f, 2.2f},
};

// ------------------------------------------------------------------ Road to Heaven (bonus): one run from the earth up through the clouds to the gates
// Sample 0 is the finish line in Heaven. The road then comes back down the east side (nobody races that part),
// the grid stands on the earth at the bottom, and the race climbs the west side.
static const TrackPoint HEAVEN_PTS[] = {
    {    0, 88.0f,  300, 16, 0.00f},   /* FINISH: the plaza before the gates */
    {   60, 88.0f,  335, 16, 0.00f},
    {  125, 82.0f,  300, 14, 0.00f},   /* the way back down (unused by the race) */
    {  155, 62.0f,  215, 13, 0.00f},
    {  155, 40.0f,  125, 13, 0.00f},
    {  120, 18.0f,   55, 13, 0.00f},
    {   60,  4.0f,   15, 13, 0.00f},
    {    0,  0.0f,    0, 13, 0.00f},   /* EARTH: the grid stands here, heading west */
    {  -70,  2.0f,    5, 13, 0.00f},
    { -130,  8.0f,   35, 13, 0.10f},   /* the climb begins */
    { -160, 18.0f,   95, 13, 0.00f},
    { -130, 28.0f,  150, 12, 0.15f},   /* switchback */
    {  -70, 36.0f,  175, 12, 0.00f},
    {  -20, 46.0f,  215, 12, 0.15f},   /* switchback, into the clouds */
    {  -70, 58.0f,  255, 12, 0.00f},
    { -130, 68.0f,  275, 13, 0.10f},
    { -150, 80.0f,  320, 13, 0.00f},
    { -100, 90.0f,  360, 14, 0.00f},
    {  -45, 94.0f,  345, 15, 0.00f, 1},/* the last rise: a leap over the threshold into Heaven */
};
static const Scenery HEAVEN_SCENERY[] = {
    /* heaven: angels line the plaza, trumpets at the gate */
    {"angel", 4, -14, 7, 3}, {"angel", 4, 14, 7, 3}, {"shofar", 10, -13, 4, 2}, {"shofar", 10, 13, 4, 2}, {"angel", 16, -14, 7, 3}, {"angel", 16, 14, 7, 3},
    {"#gpillar", 24, -14, 9, 0}, {"#gpillar", 24, 14, 9, 0}, {"angel", 30, -14, 7, 3}, {"angel", 30, 14, 7, 3},
    /* the way down: clouds */
    {"#cloud", 60, -16, 8, 0}, {"#cloud", 80, 16, 9, 0}, {"#cloud", 100, -16, 8, 0}, {"#cloud", 120, 16, 9, 0},
    /* earth */
    {"#olive", 126, -12, 6, 0}, {"#cypress", 132, 12, 8, 0}, {"#house1", 138, 12, 7, 0}, {"#olive", 148, -12, 6, 0}, {"#cypress", 154, 12, 8, 0},
    {"#olive", 162, -12, 6, 0}, {"#house2", 168, 12, 9, 0}, {"#olive", 176, -12, 6, 0}, {"#cypress", 184, 12, 8, 0}, {"#olive", 194, -12, 6, 0},
    /* the climb: clouds gather */
    {"#waterfall", 200, 16, 12, 0}, {"#cloud", 206, -16, 7, 0}, {"#cloud", 216, 16, 8, 0}, {"#cloud", 228, -17, 9, 0}, {"#cloud", 240, 16, 8, 0}, {"#cloud", 252, -16, 9, 0}, {"#cloud", 264, 17, 10, 0},
    {"#cloud", 276, -16, 8, 0}, {"#cloud", 288, 16, 9, 0}, {"#crystal", 306, -13, 3, 2}, {"#crystal", 312, 13, 3, 2},
    {"angel", 322, -14, 7, 3}, {"#cloud", 330, 16, 9, 0}, {"angel", 338, 14, 7, 3}, {"#gpillar", 346, -14, 9, 0}, {"#gpillar", 346, 14, 9, 0},
    {"angel", 356, -14, 7, 3}, {"angel", 356, 14, 7, 3}, {"shofar", 366, -14, 4, 2}, {"shofar", 366, 14, 4, 2}, {"angel", 374, -14, 7, 3}, {"angel", 374, 14, 7, 3},
};
static const RoadPad HEAVEN_PADS[] = {
    {152, 0.0f, 2.5f, PAD_BOOST},                                       /* leaving the earth */
    {196, -2.5f, 2.0f, PAD_BOOST}, {202, 2.5f, 2.0f, PAD_BOOST},        /* up the climb */
    {236, 0.0f, 2.5f, PAD_BOOST},
    {272, 0.0f, 3.0f, PAD_BOUNCE},                                      /* a hop among the clouds */
    {296, 0.0f, 2.5f, PAD_BOOST}, {340, 0.0f, 2.5f, PAD_BOOST},
};
static const CoinRow HEAVEN_COINS[] = { {158, 5, 3, 2.5f}, {210, 6, 3, -2.5f}, {250, 5, 3, 2.5f}, {300, 6, 3, -2.5f}, {350, 6, 3, 2.5f} };
static const Tunnel HEAVEN_TUNNELS[] = { {302, 318, 6.0f} };

// ------------------------------------------------------------------ The Garden of Eden: a bright morning in the garden God planted (Genesis 2)
// Start in the flower meadow, splash through the ford of the river Pishon, climb the hill and circle the Tree of Life,
// leap the falls of the Gihon, run down through the orchard and under the bower of boughs back to the meadow.
static const TrackPoint EDEN_PTS[] = {
    {    0, 0.0f,    0, 14, 0.00f},   /* start: the meadow, heading north */
    {    5, 0.0f,   55, 14, 0.00f},
    {   25, -1.2f, 100, 15, 0.00f},   /* down to the ford of the Pishon */
    {   60, -1.6f, 125, 15, 0.00f},   /* splashing through the shallows */
    {  100, 1.0f,  135, 13, 0.10f},
    {  135, 5.0f,  115, 12, 0.00f},   /* climbing the hill */
    {  155, 9.0f,   80, 12, 0.18f},   /* round the Tree of Life, banked */
    {  150, 11.0f,  40, 12, 0.18f},
    {  125, 10.0f,  10, 12, 0.00f, 1},/* the leap over the falls of the Gihon */
    {  100, 3.0f,  -25, 13, 0.00f},
    {   65, 1.0f,  -55, 13, 0.00f},   /* the orchard */
    {   25, 0.5f,  -75, 12, 0.12f},   /* under the bower of boughs */
    {  -15, 0.0f,  -65, 13, 0.00f},
    {  -20, 0.0f,  -30, 14, 0.00f},
};
static const Scenery EDEN_SCENERY[] = {
    /* the meadow (samples 0-39): flowers, fruit trees, a lamb and a lion lying down together */
    {"#flowers", 4, -13, 2.5f, 0}, {"#fruittree", 8, 13, 8, 0}, {"#lamb", 12, -12, 2.2f, 0}, {"#lion", 15, -14, 3.2f, 0}, {"#fruittree", 20, -14, 8, 0}, {"#flowers", 24, 13, 2.5f, 0},
    {"#palm", 30, 14, 12, 0}, {"#fruittree", 36, -13, 8, 0},
    /* the ford of the Pishon (40-79): water on both sides, rocks, ferns */
    {"#waterfall", 44, -19, 9, 0}, {"#rock", 48, 13, 3, 0}, {"#waterfall", 56, 18, 10, 0}, {"#fern", 62, -12, 3, 0}, {"#rock", 68, -13, 3, 0}, {"#fern", 74, 13, 3, 0}, {"#flowers", 78, -12, 2.5f, 0},
    /* the hill (80-119): fruit trees, deer, and the Tree of Life standing in the middle of the garden */
    {"#fruittree", 84, 13, 8, 0}, {"#deer", 90, -12, 3.2f, 0}, {"#fruittree", 96, -13, 9, 0}, {"#palm", 104, 14, 12, 0}, {"#fruittree", 112, 13, 8, 0},
    /* round the Tree of Life (120-159) */
    {"#treeoflife", 130, -36, 26, 10}, {"#flowers", 124, 13, 2.5f, 0}, {"#fruittree", 134, 14, 9, 0}, {"#lamb", 142, 12, 2.2f, 0}, {"#flowers", 150, 13, 2.5f, 0}, {"#fruittree", 156, 13, 8, 0},
    /* the falls of the Gihon (160-179) */
    {"#waterfall", 164, -17, 14, 0}, {"#waterfall", 168, 17, 14, 0}, {"#rock", 174, 12, 3, 0}, {"#fern", 178, -12, 3, 0},
    /* the orchard (180-219) and the bower (220-239) */
    {"#fruittree", 184, 13, 8, 0}, {"#fruittree", 190, -13, 9, 0}, {"#deer", 196, 12, 3.2f, 0}, {"#fruittree", 202, -12, 8, 0}, {"#fruittree", 208, 13, 9, 0}, {"#lion", 214, -14, 3.2f, 0},
    {"#crystal", 224, -12, 3, 2}, {"#crystal", 230, 12, 3, 2}, {"#crystal", 236, -12, 2.5f, 2},
    /* home to the meadow (240-279) */
    {"#fruittree", 244, 13, 8, 0}, {"#flowers", 250, -12, 2.5f, 0}, {"#palm", 256, 13, 12, 0}, {"#lamb", 262, -12, 2.2f, 0}, {"#fruittree", 268, -13, 8, 0}, {"#flowers", 274, 13, 2.5f, 0},
};
static const RoadPad EDEN_PADS[] = {
    {10, -2.5f, 2.2f, PAD_BOOST}, {14, 2.5f, 2.2f, PAD_BOOST},         /* across the meadow */
    {86, 0.0f, 2.5f, PAD_BOOST},                                       /* up the hill */
    {150, 0.0f, 2.5f, PAD_BOOST},                                      /* run-up to the leap over the falls */
    {200, 0.0f, 3.0f, PAD_BOUNCE},                                     /* a hop in the orchard */
};
static const CoinRow EDEN_COINS[] = { {26, 5, 3, 2.5f}, {60, 5, 3, -2.5f}, {100, 6, 3, 2.5f}, {186, 5, 3, -3.0f}, {250, 5, 3, 2.5f} };
static const Tunnel EDEN_TUNNELS[] = { {220, 240, 5.5f} };
static const Mover EDEN_MOVERS[] = {
    {"#deer", 92, 0.0f, 6.0f, 0.6f, 0.0f, 1.5f, 3.0f},                 /* deer bounding across the hill road */
    {"#lamb", 198, 0.0f, 5.5f, 0.45f, 2.0f, 1.4f, 2.0f},               /* lambs wandering the orchard */
};

// ------------------------------------------------------------------ The Walls of Jericho: seven laps round the walled city (Joshua 6)
// The road circles the city under its walls: Israel's camp and the priests on the outside, the walls and towers on the inside,
// the Ark carried ahead on the north road. On the seventh lap the walls come down and the rubble lands on the road.
static const TrackPoint JERICHO_PTS[] = {
    {    0, 0.0f,    0, 13, 0.00f},   /* start: the east road, heading north */
    {    0, 0.0f,   24, 13, 0.00f},
    {   -6, 0.7f,   46, 13, 0.12f},   /* the north-east corner, banked */
    {  -24, 1.4f,   59, 13, 0.00f},
    {  -49, 1.8f,   64, 14, 0.00f},   /* the north road: the Ark goes ahead here */
    {  -74, 1.4f,   59, 13, 0.00f},
    {  -91, 0.7f,   46, 13, 0.12f},
    {  -98, 0.0f,   24, 13, 0.00f},
    {  -99, 1.0f,    0, 13, 0.00f, 1},/* the west road: a leap over the dry ditch */
    {  -92, 0.0f,  -21, 13, 0.10f},
    {  -74, 0.0f,  -34, 13, 0.00f},
    {  -49, 0.0f,  -38, 14, 0.00f},   /* the south road, past Rahab's house */
    {  -24, 0.0f,  -34, 13, 0.00f},
    {   -6, 0.0f,  -21, 13, 0.12f},
};
static const Scenery JERICHO_SCENERY[] = {
    /* the walls and towers, all the way round on the inside (the order of these is the order they fall) */
    {"#jtower", 2, -18, 14, 0}, {"#jwall", 10, -18, 9, 0}, {"#jwall", 20, -18, 9, 0}, {"#jwall", 30, -18, 9, 0}, {"#jtower", 42, -19, 14, 0},
    {"#jwall", 54, -18, 9, 0}, {"#jwall", 66, -18, 9, 0}, {"#jwall", 78, -18, 9, 0}, {"#jwall", 90, -18, 9, 0}, {"#jwall", 102, -18, 9, 0}, {"#jtower", 114, -19, 14, 0},
    {"#jwall", 126, -18, 9, 0}, {"#jwall", 138, -18, 9, 0}, {"#jwall", 150, -18, 9, 0}, {"#jtower", 162, -19, 14, 0}, {"#jwall", 174, -18, 9, 0},
    {"#jwall", 186, -18, 9, 0}, {"#jwall", 198, -18, 9, 0}, {"#jwall", 210, -18, 9, 0}, {"#rahab", 222, -17, 9, 0}, {"#jwall", 234, -18, 9, 0}, {"#jtower", 246, -19, 14, 0},
    {"#jwall", 258, -18, 9, 0}, {"#jwall", 270, -18, 9, 0},
    /* Israel's camp and the priests on the outside */
    {"#tent", 6, 16, 6, 0}, {"#priest", 14, 13, 4, 0}, {"#banner_israel", 22, 15, 8, 0}, {"#palm", 34, 15, 11, 0}, {"#priest", 46, 13, 4, 0}, {"#tent", 58, 16, 6, 0},
    {"#priest", 70, 13, 4, 0}, {"#priest", 76, 13, 4, 0}, {"#banner_israel", 86, 15, 8, 0}, {"#priest", 94, 13, 4, 0}, {"#tent", 106, 16, 6, 0}, {"#palm", 118, 15, 11, 0},
    {"#rock", 130, 14, 3, 0}, {"#tent", 142, 16, 6, 0}, {"#priest", 154, 13, 4, 0}, {"#rock", 166, 14, 3, 0}, {"#banner_israel", 178, 15, 8, 0}, {"#palm", 190, 15, 11, 0},
    {"#tent", 202, 16, 6, 0}, {"#priest", 214, 13, 4, 0}, {"#tent", 226, 16, 6, 0}, {"#palm", 238, 15, 11, 0}, {"#priest", 250, 13, 4, 0}, {"#banner_israel", 262, 15, 8, 0}, {"#tent", 272, 16, 6, 0},
};
static const RoadPad JERICHO_PADS[] = {
    {12, -2.5f, 2.2f, PAD_BOOST}, {16, 2.5f, 2.2f, PAD_BOOST},         /* the east road */
    {150, 0.0f, 2.5f, PAD_BOOST},                                      /* run-up to the leap over the ditch */
    {224, 0.0f, 2.5f, PAD_BOOST},
};
static const CoinRow JERICHO_COINS[] = { {26, 5, 3, 2.5f}, {96, 5, 3, -2.5f}, {180, 5, 3, 2.5f}, {240, 5, 3, -2.5f} };
static const Mover JERICHO_MOVERS[] = {
    {"#ark", 80, 0.0f, 0.0f, 0.0f, 0.0f, 1.5f, 3.6f},                   /* the Ark carried ahead on the north road: steer round it */
    {"#rubble", 36, -3.0f, 0.0f, 0.0f, 0.0f, 1.1f, 2.2f}, {"#rubble", 120, 3.0f, 0.0f, 0.0f, 0.0f, 1.1f, 2.2f},   /* stones of the wall on the road once it falls */
    {"#rubble", 196, -2.5f, 0.0f, 0.0f, 0.0f, 1.1f, 2.2f}, {"#rubble", 256, 3.5f, 0.0f, 0.0f, 0.0f, 1.1f, 2.2f},
};
// ------------------------------------------------------------------ Valley of Elah: the battle arena (a wide, flat ring between the two camps)
static const TrackPoint ELAH_PTS[] = {      /* the open valley floor: a loop so wide (100 m) that it fills the whole circle, 104 m across to the camps */
    {    0.0, 0.0f,    0.0, 100, 0.00f },
    {   35.4, 0.0f,   14.6, 100, 0.00f },
    {   50.0, 0.0f,   50.0, 100, 0.00f },
    {   35.4, 0.0f,   85.4, 100, 0.00f },
    {    0.0, 0.0f,  100.0, 100, 0.00f },
    {  -35.4, 0.0f,   85.4, 100, 0.00f },
    {  -50.0, 0.0f,   50.0, 100, 0.00f },
    {  -35.4, 0.0f,   14.6, 100, 0.00f },
};
static const Scenery ELAH_SCENERY[] = {
    /* Israel's camp along one side of the valley, the Philistines along the other, all watching */
    {"#israelite", 2, 37, 5, 0}, {"#israelite", 8, 37, 5, 0}, {"#tent", 14, 42, 6, 0}, {"#camp", 17, 44, 5, 0}, {"#israelite", 20, 37, 5, 0}, {"#israelite", 26, 37, 5, 0}, {"#banner_israel", 32, 39, 8, 0},
    {"#israelite", 38, 37, 5, 0}, {"#tent", 44, 42, 6, 0}, {"#israelite", 50, 37, 5, 0}, {"#israelite", 56, 37, 5, 0}, {"#banner_israel", 62, 39, 8, 0}, {"#israelite", 68, 37, 5, 0}, {"#tent", 74, 42, 6, 0},
    {"#philistine", 82, 37, 5, 0}, {"#philistine", 88, 37, 5, 0}, {"#tent", 94, 42, 6, 0}, {"#camp", 97, 44, 5, 0}, {"#philistine", 100, 37, 5, 0}, {"#banner_philistine", 106, 39, 8, 0}, {"#philistine", 112, 37, 5, 0},
    {"#philistine", 118, 37, 5, 0}, {"#tent", 124, 42, 6, 0}, {"#philistine", 130, 37, 5, 0}, {"#banner_philistine", 136, 39, 8, 0}, {"#philistine", 142, 37, 5, 0}, {"#philistine", 148, 37, 5, 0}, {"#tent", 154, 42, 6, 0},
};

const TrackDef TRACK_DEFS[] = {
    { "Bethlehem Road", 3, BETH_PTS, (int)(sizeof BETH_PTS / sizeof BETH_PTS[0]),
      0.03f, 0.04f, 0.14f,                      /* sky */
      0.05f, 0.06f, 0.18f, 70.0f, 320.0f,       /* fog */
      -0.35f, 1.0f, 0.25f, 0.38f,               /* moonlight direction, ambient */
      0.72f, 0.74f, 0.92f,                      /* cool moonlight tint */
      1.0f, 0.72f, 0.35f,                       /* warm lantern glow */
      1, "beth_road", "beth_dirt", "beth_grass", "beth_wall", "beth_sky",
      BETH_SCENERY, (int)(sizeof BETH_SCENERY / sizeof BETH_SCENERY[0]),
      -230.0f, 120.0f, 60.0f, 46.0f,            /* the Star, high over the hill */
      300, 335, -1,                             /* alley shortcut: inside of the tight corner */
      "beth", BETH_PADS, (int)(sizeof BETH_PADS / sizeof BETH_PADS[0]), BETH_MOVERS, (int)(sizeof BETH_MOVERS / sizeof BETH_MOVERS[0]), 0,
      0, NULL, 0, 0, 0, 0, BETH_COINS, (int)(sizeof BETH_COINS / sizeof BETH_COINS[0]), BETH_TUNNELS, 1 },
    { "Sea of Galilee", 3, GAL_PTS, (int)(sizeof GAL_PTS / sizeof GAL_PTS[0]),
      0.50f, 0.74f, 0.98f,                      /* clear blue sky */
      0.78f, 0.88f, 0.98f, 140.0f, 520.0f,      /* light haze */
      0.30f, 1.0f, 0.45f, 0.50f,                /* high sun */
      1.0f, 0.98f, 0.92f,
      1.0f, 0.95f, 0.8f,
      0, "gal_road", "gal_sand", "gal_water", "gal_wall", "gal_sky",
      GAL_SCENERY, (int)(sizeof GAL_SCENERY / sizeof GAL_SCENERY[0]),
      0, 0, 0, 0,
      150, 185, 1,                              /* sandbar shortcut on the outside of the sweep */
      "gal", GAL_PADS, (int)(sizeof GAL_PADS / sizeof GAL_PADS[0]), GAL_MOVERS, (int)(sizeof GAL_MOVERS / sizeof GAL_MOVERS[0]), 1,
      0, NULL, 0, 0, 0, 0, GAL_COINS, (int)(sizeof GAL_COINS / sizeof GAL_COINS[0]), GAL_TUNNELS, 1 },
    { "Jerusalem Road", 3, JERU_PTS, (int)(sizeof JERU_PTS / sizeof JERU_PTS[0]),
      0.80f, 0.62f, 0.45f,                      /* golden afternoon */
      0.90f, 0.74f, 0.52f, 90.0f, 420.0f,
      -0.6f, 0.7f, -0.3f, 0.42f,                /* low warm sun */
      1.0f, 0.90f, 0.72f,
      1.0f, 0.85f, 0.5f,
      0, "jeru_road", "jeru_dirt", "jeru_grass", "jeru_wall", "jeru_sky",
      JERU_SCENERY, (int)(sizeof JERU_SCENERY / sizeof JERU_SCENERY[0]),
      0, 0, 0, 0,
      232, 262, -1,                             /* side alley on the olive slope */
      "jeru", JERU_PADS, (int)(sizeof JERU_PADS / sizeof JERU_PADS[0]), JERU_MOVERS, (int)(sizeof JERU_MOVERS / sizeof JERU_MOVERS[0]), 0,
      0, NULL, 0, 0, 0, 0, JERU_COINS, (int)(sizeof JERU_COINS / sizeof JERU_COINS[0]), JERU_TUNNELS, 2 },
    { "Sunrise at the Empty Tomb", 3, TOMB_PTS, (int)(sizeof TOMB_PTS / sizeof TOMB_PTS[0]),
      0.95f, 0.70f, 0.60f,                      /* pink dawn */
      0.98f, 0.80f, 0.70f, 80.0f, 380.0f,
      0.7f, 0.45f, 0.5f, 0.45f,                 /* sun just over the hills */
      1.0f, 0.88f, 0.80f,
      1.0f, 0.95f, 0.75f,
      0, "tomb_road", "tomb_dirt", "tomb_grass", "tomb_wall", "tomb_sky",
      TOMB_SCENERY, (int)(sizeof TOMB_SCENERY / sizeof TOMB_SCENERY[0]),
      0, 0, 0, 0,
      -1, -1, 0,
      "tomb", TOMB_PADS, (int)(sizeof TOMB_PADS / sizeof TOMB_PADS[0]), TOMB_MOVERS, (int)(sizeof TOMB_MOVERS / sizeof TOMB_MOVERS[0]), 0,
      0, NULL, 0, 0, 0, 0, TOMB_COINS, (int)(sizeof TOMB_COINS / sizeof TOMB_COINS[0]), TOMB_TUNNELS, 1 },
    { "Road to Heaven", 1, HEAVEN_PTS, (int)(sizeof HEAVEN_PTS / sizeof HEAVEN_PTS[0]),
      0.80f, 0.88f, 1.00f,                      /* bright pale sky */
      0.98f, 0.95f, 0.85f, 120.0f, 600.0f,      /* golden haze */
      0.2f, 1.0f, 0.3f, 0.60f,                  /* light from above */
      1.0f, 0.98f, 0.90f,
      1.0f, 0.92f, 0.6f,
      0, "heaven_road", "heaven_clouds", "heaven_earth", "heaven_cloudwall", "heaven_sky",
      HEAVEN_SCENERY, (int)(sizeof HEAVEN_SCENERY / sizeof HEAVEN_SCENERY[0]),
      0, 0, 0, 0,
      -1, -1, 0,
      "heaven", HEAVEN_PADS, (int)(sizeof HEAVEN_PADS / sizeof HEAVEN_PADS[0]), NULL, 0, 0,
      140,                                      /* the grid stands on the earth */
      "heaven_gates", 34, 70.0f, 40.0f,         /* the gates with God's open hands, standing across the plaza */
      1, HEAVEN_COINS, (int)(sizeof HEAVEN_COINS / sizeof HEAVEN_COINS[0]), HEAVEN_TUNNELS, 1 },
    { "The Garden of Eden", 3, EDEN_PTS, (int)(sizeof EDEN_PTS / sizeof EDEN_PTS[0]),
      0.52f, 0.78f, 0.98f,                      /* clear morning sky */
      0.82f, 0.94f, 0.86f, 110.0f, 480.0f,      /* soft green haze */
      0.35f, 1.0f, 0.4f, 0.55f,                 /* high morning sun */
      1.0f, 0.98f, 0.86f,
      1.0f, 0.95f, 0.6f,
      0, "eden_road", "eden_dirt", "eden_grass", "eden_wall", "eden_sky",
      EDEN_SCENERY, (int)(sizeof EDEN_SCENERY / sizeof EDEN_SCENERY[0]),
      0, 0, 0, 0,
      128, 150, 1,                              /* the flower bank on the outside of the Tree of Life bend */
      "eden", EDEN_PADS, (int)(sizeof EDEN_PADS / sizeof EDEN_PADS[0]), EDEN_MOVERS, (int)(sizeof EDEN_MOVERS / sizeof EDEN_MOVERS[0]), 0,
      0, NULL, 0, 0, 0, 0, EDEN_COINS, (int)(sizeof EDEN_COINS / sizeof EDEN_COINS[0]), EDEN_TUNNELS, 1 },
    { "The Walls of Jericho", 7, JERICHO_PTS, (int)(sizeof JERICHO_PTS / sizeof JERICHO_PTS[0]),
      0.58f, 0.76f, 0.96f,                      /* hard blue desert sky */
      0.92f, 0.86f, 0.70f, 100.0f, 460.0f,      /* dusty haze */
      0.2f, 1.0f, 0.3f, 0.52f,                  /* midday sun */
      1.0f, 0.96f, 0.86f,
      1.0f, 0.85f, 0.5f,
      0, "jericho_road", "jericho_dirt", "jericho_grass", "jericho_wall", "jericho_sky",
      JERICHO_SCENERY, (int)(sizeof JERICHO_SCENERY / sizeof JERICHO_SCENERY[0]),
      0, 0, 0, 0,
      214, 236, -1,                             /* Rahab's house: a strip along the wall on the inside of the south road */
      "jericho", JERICHO_PADS, (int)(sizeof JERICHO_PADS / sizeof JERICHO_PADS[0]), JERICHO_MOVERS, (int)(sizeof JERICHO_MOVERS / sizeof JERICHO_MOVERS[0]), 0,
      0, NULL, 0, 0, 0, 3, JERICHO_COINS, (int)(sizeof JERICHO_COINS / sizeof JERICHO_COINS[0]), NULL, 0 },   /* bonus = 3: the Walls of Jericho mode, not on the road-select screen */
    { "Valley of Elah", 99, ELAH_PTS, (int)(sizeof ELAH_PTS / sizeof ELAH_PTS[0]),
      0.75f, 0.70f, 0.55f,                      /* dusty afternoon */
      0.85f, 0.78f, 0.6f, 120.0f, 500.0f,
      0.4f, 0.9f, 0.3f, 0.5f,
      1.0f, 0.95f, 0.85f,
      1.0f, 0.8f, 0.5f,
      0, "elah_road", "elah_dirt", "elah_grass", "elah_wall", "elah_sky",
      ELAH_SCENERY, (int)(sizeof ELAH_SCENERY / sizeof ELAH_SCENERY[0]),
      0, 0, 0, 0,
      -1, -1, 0,
      "elah", NULL, 0, NULL, 0, 0,
      0, NULL, 0, 0, 0, 2 },                    /* bonus = 2: the battle arena, never a race road */
    { "Test Loop", 3, TEST_PTS, (int)(sizeof TEST_PTS / sizeof TEST_PTS[0]),
      0.55f, 0.75f, 0.98f,
      0.72f, 0.82f, 0.95f, 120.0f, 520.0f,
      0.35f, 1.0f, 0.5f, 0.45f,
      1.0f, 1.0f, 1.0f,
      1.0f, 0.9f, 0.6f,
      0, "test_road", "test_dirt", "test_grass", "test_wall", "test_sky",
      TEST_SCENERY, (int)(sizeof TEST_SCENERY / sizeof TEST_SCENERY[0]),
      0, 0, 0, 0,
      -1, -1, 0,
      "test", NULL, 0, NULL, 0, 0 },
};
const int NUM_TRACK_DEFS = (int)(sizeof TRACK_DEFS / sizeof TRACK_DEFS[0]);

/* How much each road's layout is stretched (x and z; heights, widths and scenery spacing from the road stay as they are).
   Chosen so the winner of a race takes about 2:15 (Sean: at least two minutes).
   Order = TRACK_DEFS. Battle arena and test loop stay as built. */
static const float TRACK_SCALE[] = {2.7f, 2.55f, 3.0f, 2.65f, 3.6f, 2.75f, 1.2f, 1.0f, 1.0f};   /* (Sean, again: longer) about 3:20 a race */   /* Jericho: seven laps, so each one is short (Sean: it went on forever) */

/* every racing road is this much wider than first drawn (Sean: room between the racers); arena and test loop stay */
#define ROAD_WIDEN 1.45f
void track_from_def(Track *t, const TrackDef *d) {
    int di = (int)(d - TRACK_DEFS); float sc = (di >= 0 && di < (int)(sizeof TRACK_SCALE / sizeof TRACK_SCALE[0])) ? TRACK_SCALE[di] : 1.0f;
    float wd = (sc != 1.0f || d->laps == 7) ? ROAD_WIDEN : 1.0f;   /* the stretched roads and Jericho */
    static TrackPoint pts[256]; int np = d->npoints < 256 ? d->npoints : 256;
    /* every stretch between two points grows by sc, except a jump's gap, which keeps its length so the jump can still be cleared;
       the small amount the loop then fails to close by is spread over the other stretches */
    float ex = 0, ez = 0; int nfree = 0;
    for (int i = 0; i < np; i++) { const TrackPoint *a = &d->points[i], *b = &d->points[(i + 1) % np]; nfree++; (void)a; (void)b; }
    pts[0] = d->points[0]; pts[0].x *= sc; pts[0].z *= sc;
    for (int i = 0; i + 1 < np; i++) { const TrackPoint *a = &d->points[i], *b = &d->points[i + 1]; float f = sc;   /* a jump is a drop, not a gap, so it can stretch like the rest (keeping it short bent the Eden road back on itself at the line) */
        pts[i + 1] = *b; pts[i + 1].x = pts[i].x + (b->x - a->x) * f; pts[i + 1].z = pts[i].z + (b->z - a->z) * f;
        if (!a->jump && nfree) { pts[i + 1].x += ex / nfree; pts[i + 1].z += ez / nfree; } }
    for (int i = 0; i < np; i++) pts[i].width *= wd;
    const int SPS = 30; float ks = SPS / 20.0f;   /* finer sampling for the longer roads; indices from the definition scale with it */
    track_build(t, d->name, d->laps, pts, np, SPS); t->def_scale = ks;
#define DI(i) ((int)((i) * ks + 0.5f))
    t->shortcut_from = DI(d->shortcut_from); t->shortcut_to = DI(d->shortcut_to); t->shortcut_side = d->shortcut_side;
    t->npad = 0; for (int i = 0; i < d->npads && i < MAX_PADS; i++) { t->pad[i] = d->pads[i]; t->pad[i].idx = DI(t->pad[i].idx) % t->n; t->pad[i].lateral *= wd; t->pad[i].halfw *= wd; t->npad++; }
    t->nmover = 0; for (int i = 0; i < d->nmovers && i < MAX_MOVERS; i++) { t->mover[i] = d->movers[i]; t->mover[i].idx = DI(t->mover[i].idx) % t->n; t->mover[i].lateral *= wd; t->mover[i].amp *= wd; t->nmover++; }
    t->water = d->water; t->grid_idx = DI(d->grid_idx) % t->n;
    t->ncoin = 0;
    for (int i = 0; i < d->ncoinrows; i++) for (int c = 0; c < d->coins[i].n && t->ncoin < MAX_COINS; c++) { t->coin[t->ncoin].idx = DI(d->coins[i].idx + c * d->coins[i].step) % t->n; t->coin[t->ncoin].lateral = d->coins[i].lateral * wd; t->ncoin++; }
    t->ntunnel = 0; for (int i = 0; i < d->ntunnels && i < MAX_TUNNELS; i++) { t->tunnel[i] = d->tunnels[i]; t->tunnel[i].from = DI(t->tunnel[i].from) % t->n; t->tunnel[i].to = DI(t->tunnel[i].to) % t->n; t->ntunnel++; }
    /* open land beside the road (Sean: a semi-open world), except on the road through the sky to Heaven, round the walls
       of Jericho, and in the battle arena */
    memset(t->field, 0, sizeof t->field); t->ngem = 0; t->water_y = -1e9f;
    if (!d->finale_img && d->bonus != 2 && d->bonus != 3) track_make_field(t, d->water);
    /* scenery that stands out in the open land (not past its wall) is solid: a kart has to steer round it */
    t->nobst = 0;
    for (int i = 0; i < d->nscenery && t->nobst < 96; i++) { const Scenery *sc = &d->scenery[i]; if (sc->img[0] != '#') continue;
        int idx = DI(sc->idx) % t->n; float lat = sc->side * wd, sgn = lat < 0 ? -1.0f : 1.0f;
        if (fabsf(lat) < track_edge(t, idx) + 0.5f || fabsf(lat) > track_bound(t, idx, (int)sgn) - 1.0f) continue;
        if (!strcmp(sc->img, "#cloud") || !strcmp(sc->img, "#flowers") || !strcmp(sc->img, "#lily") || !strcmp(sc->img, "#waterfall") || !strcmp(sc->img, "#sheep") || !strcmp(sc->img, "#lamb")) continue;
        const TrackSample *s2 = &t->s[idx]; float r = sc->height * 0.16f; if (r < 0.7f) r = 0.7f; if (r > 3.0f) r = 3.0f;
        t->obst[t->nobst].x = s2->x + s2->nx * lat; t->obst[t->nobst].z = s2->z + s2->nz * lat; t->obst[t->nobst].r = r; t->nobst++; }
}
void track_make_test(Track *t) { track_from_def(t, &TRACK_DEFS[NUM_TRACK_DEFS - 1]); }
