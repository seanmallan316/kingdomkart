// storymode.h - BIBLE JOURNEYS (Sean: "that's what I meant by story mode"): Sean's world map (assets/story/world_map_src.png) as a real 3D map. It lies folded on a
// wooden table, opens out panel by panel like a paper road map, then the land rises up out of the paper (the heights come
// from his painting: tools/story_height.py). Your character's kart drives from stop to stop along the roads; boats, camels,
// doves, clouds and fire move about; every stop has a glowing pin, a padlock until it is open and a gold flag once cleared.
// The 16 stops go in Bible order, Creation to Glory; finish in the top 3 (win at Jericho and against Goliath) to clear one
// and open the next. Included by view.c.
#ifndef KK_STORYMODE_H
#define KK_STORYMODE_H
#include "storymap_h.h"

#define SM_W 134.1f      /* the map in world units (his picture is 1341 x 1173: 10 pixels a unit) */
#define SM_D 117.3f
#define SM_HS 3.4f       /* how high the highest land stands */
#define SM_NODES 16
typedef struct { const char *name; float px, py; int track; const char *verse; } SmNode;
static const SmNode SM_NODE[SM_NODES] = {
    {"THE GARDEN OF EDEN", 672, 229, 5, "GENESIS 2:8 - THE LORD GOD PLANTED A GARDEN EASTWARD IN EDEN"},
    {"NILE RIVER RUSH", 215, 960, 6, "EXODUS 2:3 - A LITTLE ARK OF BULRUSHES BY THE RIVER'S BRINK"},
    {"PYRAMID PASS", 292, 826, 7, "EXODUS 1:11 - THEY BUILT TREASURE CITIES FOR PHARAOH"},
    {"PLAGUES OF EGYPT", 206, 692, 8, "EXODUS 9:1 - LET MY PEOPLE GO, THAT THEY MAY SERVE ME"},
    {"RED SEA ESCAPE", 466, 1063, 9, "EXODUS 14:22 - ON DRY GROUND IN THE MIDST OF THE SEA"},
    {"THE WALLS OF JERICHO", 268, 497, 14, "JOSHUA 6:20 - THE PEOPLE SHOUTED AND THE WALL FELL DOWN FLAT"},
    {"DAVID VS GOLIATH", 352, 312, 15, "1 SAMUEL 17:45 - I COME TO THEE IN THE NAME OF THE LORD"},
    {"ISHTAR GATE DASH", 895, 773, 10, "DANIEL 1:1 - NEBUCHADNEZZAR KING OF BABYLON CAME UNTO JERUSALEM"},
    {"HANGING GARDENS CIRCUIT", 1140, 778, 11, "DANIEL 4:30 - IS NOT THIS GREAT BABYLON, THAT I HAVE BUILT?"},
    {"THE FIERY FURNACE", 808, 918, 12, "DANIEL 3:25 - FOUR MEN LOOSE, WALKING IN THE MIDST OF THE FIRE"},
    {"DANIEL'S DEN", 1090, 975, 13, "DANIEL 6:22 - MY GOD HATH SENT HIS ANGEL, AND HATH SHUT THE LIONS' MOUTHS"},
    {"BETHLEHEM ROAD", 490, 520, 0, "LUKE 2:11 - UNTO YOU IS BORN THIS DAY A SAVIOUR"},
    {"SEA OF GALILEE", 830, 404, 1, "MATTHEW 14:29 - PETER WALKED ON THE WATER TO GO TO JESUS"},
    {"JERUSALEM ROAD", 781, 590, 2, "MATTHEW 21:9 - HOSANNA TO THE SON OF DAVID"},
    {"SUNRISE AT THE EMPTY TOMB", 1110, 481, 3, "MATTHEW 28:6 - HE IS NOT HERE: FOR HE IS RISEN"},
    {"ROAD TO HEAVEN", 1105, 262, 4, "REVELATION 21:21 - THE STREET OF THE CITY WAS PURE GOLD"},
};
/* the painted roads between the stops (picture pixels), for the kart to drive along */
typedef struct { int a, b, n; float p[6][2]; } SmEdge;
static const SmEdge SM_EDGE[] = {
    {6, 5, 5, {{352, 312}, {250, 330}, {195, 390}, {230, 470}, {268, 497}}},
    {5, 3, 4, {{268, 497}, {240, 560}, {212, 620}, {206, 692}}},
    {3, 2, 4, {{206, 692}, {185, 745}, {250, 790}, {292, 826}}},
    {2, 1, 4, {{292, 826}, {205, 862}, {190, 920}, {215, 960}}},
    {2, 4, 4, {{292, 826}, {380, 850}, {420, 950}, {466, 1063}}},
    {1, 4, 3, {{215, 960}, {330, 1005}, {466, 1063}}},
    {5, 11, 4, {{268, 497}, {330, 500}, {410, 470}, {490, 520}}},
    {11, 13, 5, {{490, 520}, {560, 470}, {640, 480}, {700, 540}, {781, 590}}},
    {0, 13, 5, {{672, 229}, {610, 300}, {600, 400}, {660, 500}, {781, 590}}},
    {13, 12, 4, {{781, 590}, {800, 500}, {840, 440}, {830, 404}}},
    {12, 14, 4, {{830, 404}, {920, 390}, {1000, 430}, {1110, 481}}},
    {14, 15, 4, {{1110, 481}, {1050, 380}, {1060, 300}, {1105, 262}}},
    {13, 7, 4, {{781, 590}, {760, 660}, {820, 720}, {895, 773}}},
    {7, 8, 4, {{895, 773}, {1000, 730}, {1080, 700}, {1140, 778}}},
    {7, 9, 3, {{895, 773}, {830, 820}, {808, 918}}},
    {9, 10, 4, {{808, 918}, {950, 880}, {1050, 900}, {1090, 975}}},
    {8, 10, 3, {{1140, 778}, {1185, 860}, {1090, 975}}},
    {14, 8, 4, {{1110, 481}, {1100, 560}, {1060, 620}, {1140, 778}}},
};
#define SM_NEDGE (int)(sizeof SM_EDGE / sizeof SM_EDGE[0])
/* where each character lived and traveled, as stops on Sean's map, with the verse for each */
typedef struct { int n; struct { int node; const char *line; } s[8]; } SmTrip;
enum { N_EDEN, N_NILE, N_PYR, N_PLAG, N_RSEA, N_JERICHO, N_ELAH, N_ISH, N_GAR, N_FUR, N_DEN, N_BETH, N_GAL, N_JERU, N_TOMB, N_HEAVEN };
static const SmTrip SM_TRIP[] = {
    /* JESUS */ {7, {{N_BETH, "BORN IN BETHLEHEM - LUKE 2:7"}, {N_NILE, "CARRIED INTO EGYPT - MATTHEW 2:14"}, {N_GAL, "FISHERS OF MEN BY THE SEA OF GALILEE - MATTHEW 4:19"}, {N_JERICHO, "CALLED ZACCHAEUS DOWN - LUKE 19:5"},
        {N_JERU, "RODE INTO JERUSALEM - MATTHEW 21:9"}, {N_TOMB, "HE IS RISEN - MATTHEW 28:6"}, {N_HEAVEN, "TAKEN UP INTO HEAVEN - ACTS 1:9"}}},
    /* MOSES */ {4, {{N_NILE, "IN A BASKET ON THE NILE - EXODUS 2:3"}, {N_PYR, "RAISED IN PHARAOH'S HOUSE - EXODUS 2:10"}, {N_PLAG, "LET MY PEOPLE GO - EXODUS 5:1"}, {N_RSEA, "THROUGH THE RED SEA - EXODUS 14:22"}}},
    /* DAVID */ {3, {{N_BETH, "A SHEPHERD BOY OF BETHLEHEM - 1 SAMUEL 16:11"}, {N_ELAH, "DEFEATED GOLIATH - 1 SAMUEL 17:50"}, {N_JERU, "KING IN THE CITY OF DAVID - 2 SAMUEL 5:7"}}},
    /* ESTHER */ {3, {{N_JERU, "HER PEOPLE CARRIED AWAY FROM JERUSALEM - ESTHER 2:6"}, {N_ISH, "IN EXILE IN THE EAST - ESTHER 2:5"}, {N_GAR, "MADE QUEEN BY THE KING - ESTHER 2:17"}}},
    /* PETER */ {3, {{N_GAL, "FISHERS OF MEN - MATTHEW 4:19"}, {N_TOMB, "RAN TO THE EMPTY TOMB - JOHN 20:4"}, {N_JERU, "PREACHED AT PENTECOST - ACTS 2:14"}}},
    /* MARY */ {3, {{N_BETH, "GAVE BIRTH TO JESUS - LUKE 2:7"}, {N_NILE, "FLED INTO EGYPT - MATTHEW 2:14"}, {N_JERU, "BY THE CROSS - JOHN 19:25"}}},
    /* DANIEL */ {4, {{N_JERU, "TAKEN CAPTIVE - DANIEL 1:1"}, {N_ISH, "SERVED THE KINGS OF BABYLON - DANIEL 2:48"}, {N_FUR, "HIS FRIENDS IN THE FURNACE - DANIEL 3:25"}, {N_DEN, "SAFE IN THE LIONS' DEN - DANIEL 6:22"}}},
    /* SAMSON */ {2, {{N_ELAH, "BORN IN ZORAH, BY THE VALLEY OF ELAH - JUDGES 13:2"}, {N_ELAH, "BROUGHT DOWN THE HOUSE OF DAGON - JUDGES 16:30"}}},
    /* NOAH */ {2, {{N_EDEN, "NOAH FOUND GRACE - GENESIS 6:8"}, {N_HEAVEN, "GOD SET HIS BOW IN THE CLOUD - GENESIS 9:13"}}},
    /* JOSEPH */ {3, {{N_BETH, "THE COAT OF MANY COLOURS - GENESIS 37:3"}, {N_NILE, "SOLD INTO EGYPT - GENESIS 37:28"}, {N_PYR, "RULER OVER ALL EGYPT - GENESIS 41:41"}}},
    /* JONAH */ {2, {{N_GAL, "FROM GATH-HEPHER IN GALILEE - 2 KINGS 14:25"}, {N_GAR, "PREACHED IN NINEVEH, FAR TO THE EAST - JONAH 3:4"}}},
    /* JOHN */ {2, {{N_JERU, "BORN IN THE HILL COUNTRY OF JUDAH - LUKE 1:39"}, {N_JERICHO, "BAPTIZED JESUS IN THE JORDAN - MATTHEW 3:16"}}},
    /* MICHAEL */ {2, {{N_HEAVEN, "WAR IN HEAVEN - REVELATION 12:7"}, {N_ISH, "CAME TO HELP DANIEL - DANIEL 10:13"}}},
    /* GABRIEL */ {4, {{N_HEAVEN, "I STAND IN THE PRESENCE OF GOD - LUKE 1:19"}, {N_ISH, "EXPLAINED DANIEL'S VISION - DANIEL 8:16"}, {N_JERU, "SPOKE TO ZACHARIAS - LUKE 1:13"}, {N_GAL, "GOOD NEWS FOR MARY - LUKE 1:26"}}},
    /* GOLIATH */ {2, {{N_ELAH, "THE CHAMPION OF GATH - 1 SAMUEL 17:4"}, {N_ELAH, "FELL TO DAVID'S STONE - 1 SAMUEL 17:49"}}},
    /* GIDEON */ {2, {{N_GAL, "THE ANGEL CAME TO OPHRAH - JUDGES 6:11"}, {N_GAL, "300 MEN BEAT MIDIAN - JUDGES 7:7"}}},
    /* ZACCHAEUS */ {2, {{N_JERICHO, "CHIEF OF THE TAX COLLECTORS - LUKE 19:2"}, {N_JERICHO, "SALVATION CAME TO HIS HOUSE - LUKE 19:9"}}},
    /* JACOB */ {4, {{N_JERU, "THE LADDER TO HEAVEN AT BETHEL - GENESIS 28:12"}, {N_GAR, "SERVED LABAN IN HARAN - GENESIS 29:20"}, {N_JERICHO, "WRESTLED TILL DAYBREAK - GENESIS 32:24"}, {N_NILE, "WENT DOWN INTO EGYPT - GENESIS 46:6"}}},
    /* ISAAC */ {2, {{N_BETH, "BORN TO ABRAHAM AND SARAH - GENESIS 21:2"}, {N_JERU, "GOD PROVIDED A RAM ON MORIAH - GENESIS 22:13"}}},
    /* ABRAHAM */ {5, {{N_ISH, "CALLED OUT OF UR - GENESIS 11:31"}, {N_GAR, "LEFT HARAN - GENESIS 12:4"}, {N_BETH, "THE PROMISED LAND - GENESIS 12:7"}, {N_NILE, "WENT DOWN INTO EGYPT - GENESIS 12:10"},
        {N_JERU, "GOD WILL PROVIDE ON MORIAH - GENESIS 22:14"}}},
    /* PHARAOH */ {3, {{N_PYR, "KING OF EGYPT - EXODUS 1:8"}, {N_PLAG, "HARDENED HIS HEART - EXODUS 7:13"}, {N_RSEA, "CHASED ISRAEL TO THE SEA - EXODUS 14:23"}}},
    /* NEBUCHADNEZZAR */ {4, {{N_ISH, "KING OF BABYLON - DANIEL 1:1"}, {N_JERU, "BESIEGED JERUSALEM - 2 KINGS 25:1"}, {N_FUR, "THE GOLDEN IMAGE - DANIEL 3:1"}, {N_GAR, "IS NOT THIS GREAT BABYLON - DANIEL 4:30"}}},
};
typedef char sm_one_trip_each[(sizeof SM_TRIP / sizeof SM_TRIP[0]) == NCHARS ? 1 : -1];

static float g_sm_dt = 1.0f / 60;   /* the last frame's length (the camera eases by it) */
static int g_story, g_sm_from_menu;                   /* 1: the races are being played from STORY MODE */
static int g_sm_sel, g_sm_at;         /* the stop picked; the stop the kart is at */
static float g_sm_t;                  /* seconds since the map came out (the unfolding plays from 0) */
static int g_sm_over;                 /* + : the whole map */
static int g_sm_view, g_sm_jchar;     /* 0 race the stops, 1 a character's journey, 2 my travels; whose journey */
static float g_sm_jt;                 /* seconds into the journey being drawn */
static float g_sm_jl[1200][2]; static int g_sm_njl; static float g_sm_jstop[25]; static int g_sm_jnode[25], g_sm_jn;   /* the journey line, where each stop is along it */
static Vec3 g_sm_eye, g_sm_look; static int g_sm_cam_ok;
static float g_sm_route[96][2]; static int g_sm_nroute; static float g_sm_rlen, g_sm_rpos; static int g_sm_driving;
static int g_sm_won, g_sm_raced;      /* the last story race: cleared it? */
static float g_sm_cel;                /* > 0: a stop has just been cleared (the celebration) */
static int g_sm_cel_node, g_sm_tapped;
static float g_sm_scr[SM_NODES][3];   /* each stop on the screen (x, y, visible) */
static Texture *tx_story; static int g_sm_tex_tried;
enum { SMX_UNFOLD, SMX_RISE, SMX_PIN, SMX_SELECT, SMX_DRIVE, SMX_UNLOCK, SMX_STOP, SMX_DRAW, SMX_COUNT };   /* the map's own sounds: content/sfx/map_*.wav (tools/make_map_sfx.py) */
static int g_smx[SMX_COUNT] = {-1, -1, -1, -1, -1, -1, -1, -1}; static int g_smx_loaded;
static void smx(int i, float vol) { if (i >= 0 && i < SMX_COUNT && g_smx[i] >= 0) sfx_voice_play(g_smx[i], vol, 0); }
#define SM_UNFOLD_END 4.8f

static int sm_open(int i) { return i <= g_save.story && i < SM_NODES; }
static int sm_cleared(int i) { return i < g_save.story; }
static float sm_fold_c(void) { return 1.0f - sc_smooth((g_sm_t - 0.9f) / 1.5f); }    /* the three panels across: 1 folded .. 0 open */
static float sm_fold_r(void) { return 1.0f - sc_smooth((g_sm_t - 2.5f) / 1.2f); }    /* the two panels down */
static float sm_relief(void) { return sc_smooth((g_sm_t - 3.7f) / 1.0f); }           /* the land rising out of the paper */
static float sm_hgt(float px, float py) {   /* the land's height (0..1) under a picture point */
    float fx = clampf(px / 1341.0f, 0, 1) * SMH_NX, fz = clampf(py / 1173.0f, 0, 1) * SMH_NZ; int x0 = (int)fx, z0 = (int)fz; if (x0 >= SMH_NX) x0 = SMH_NX - 1; if (z0 >= SMH_NZ) z0 = SMH_NZ - 1;
    float u = fx - x0, v = fz - z0; return ((SMH[z0][x0] * (1 - u) + SMH[z0][x0 + 1] * u) * (1 - v) + (SMH[z0 + 1][x0] * (1 - u) + SMH[z0 + 1][x0 + 1] * u) * v) / 255.0f; }
static Vec3 sm_pos(float px, float py) { return v3(px * 0.1f - SM_W * 0.5f, sm_hgt(px, py) * SM_HS * sm_relief(), py * 0.1f - SM_D * 0.5f); }   /* on the open map */

/* ---- the paper: folded, unfolding, open ---- */
static Vec3 sm_fold(float x, float z, float h, float fc, float fr) {   /* a point of the flat map (x, z, height off the paper) where the folds put it */
    const float L = SM_W / 3, LIFT = 0.25f;
    /* the two rows: the far half folds up and over onto the near half (the fold runs across the middle) */
    float y = h; if (z > 0) { float ps = 3.14159265f * fr, t = z; z = t * cosf(ps) - h * sinf(ps); y = t * sinf(ps) + h * cosf(ps) + LIFT * fr; }
    /* the three columns: a zig-zag (accordion) fold */
    int c = (int)floorf((x + SM_W * 0.5f) / L); if (c < 0) c = 0; if (c > 2) c = 2;
    float s = x + SM_W * 0.5f - c * L, sx = -SM_W * 0.5f, sy = 0; float ph[3] = {0, 3.14159265f * fc, 0};
    for (int k = 0; k < c; k++) { sx += L * cosf(ph[k]); sy += L * sinf(ph[k]); }
    float X = sx + s * cosf(ph[c]) - y * sinf(ph[c]), Y = sy + s * sinf(ph[c]) + y * cosf(ph[c]) + c * LIFT * fc;
    return v3(X + SM_W / 3 * fc, Y + 0.05f, z + SM_D / 4 * fr); }
static Mesh *m_sm_top[2], *m_sm_back[2], *m_sm_top_s, *m_sm_back_s; static float g_sm_built_key = -1;
static Vertex *g_sm_vt, *g_sm_vb;
static uint32_t sm_build(float fc, float fr, float rel) {   /* fills g_sm_vt (the printed side) and g_sm_vb (the back): returns the vertex count */
    static Vec3 P[SMH_NZ + 1][SMH_NX + 1], B[SMH_NZ + 1][SMH_NX + 1], N[SMH_NZ + 1][SMH_NX + 1];
    if (!g_sm_vt) { g_sm_vt = malloc(sizeof(Vertex) * SMH_NX * SMH_NZ * 6); g_sm_vb = malloc(sizeof(Vertex) * SMH_NX * SMH_NZ * 6); }
    for (int j = 0; j <= SMH_NZ; j++) for (int i = 0; i <= SMH_NX; i++) { float x = SM_W * i / SMH_NX - SM_W * 0.5f, z = SM_D * j / SMH_NZ - SM_D * 0.5f;
        P[j][i] = sm_fold(x, z, SMH[j][i] / 255.0f * SM_HS * rel, fc, fr); B[j][i] = sm_fold(x, z, -0.12f, fc, fr); }
    for (int j = 0; j <= SMH_NZ; j++) for (int i = 0; i <= SMH_NX; i++) { int il = i > 0 ? i - 1 : i, ir = i < SMH_NX ? i + 1 : i, ju = j > 0 ? j - 1 : j, jd = j < SMH_NZ ? j + 1 : j;
        Vec3 n = v3_cross(v3_sub(P[jd][i], P[ju][i]), v3_sub(P[j][ir], P[j][il])); float l = sqrtf(v3_dot(n, n)); N[j][i] = l > 1e-6f ? v3(n.x / l, n.y / l, n.z / l) : v3(0, 1, 0); }
    uint32_t n = 0;
    for (int j = 0; j < SMH_NZ; j++) for (int i = 0; i < SMH_NX; i++) { int cx[6] = {i, i, i + 1, i, i + 1, i + 1}, cz[6] = {j, j + 1, j + 1, j, j + 1, j};
        for (int k = 0; k < 6; k++) { int x = cx[k], z = cz[k]; Vec3 p = P[z][x], q = N[z][x], b = B[z][x]; float u = (float)x / SMH_NX, v = (float)z / SMH_NZ;
            Vertex vt = {p.x, p.y, p.z, q.x, q.y, q.z, u, v, 1, 1, 1, 1}; g_sm_vt[n] = vt;
            int kk = 5 - k; int bx = cx[kk], bz = cz[kk]; Vec3 bb = B[bz][bx], bn = N[bz][bx];   /* the back: the same squares the other way round, facing down */
            Vertex vb = {bb.x, bb.y, bb.z, -bn.x, -bn.y, -bn.z, 0.5f, 0.5f, 0.86f, 0.74f, 0.52f, 1}; g_sm_vb[n] = vb; (void)b;
            n++; } }
    return n; }
static int g_sm_tick_fold;
static void sm_update_paper(int drc, int *use_static) {
    float fc = sm_fold_c(), fr = sm_fold_r(), rel = sm_relief();
    if (fc <= 0 && fr <= 0 && rel >= 1) { if (!m_sm_top_s) { uint32_t n = sm_build(0, 0, 1); m_sm_top_s = gfx_mesh_static(g_sm_vt, n); m_sm_back_s = gfx_mesh_static(g_sm_vb, n); } *use_static = 1; return; }
    *use_static = 0; uint32_t cap = SMH_NX * SMH_NZ * 6;
    if (!m_sm_top[drc]) { m_sm_top[drc] = gfx_mesh_dynamic(cap); m_sm_back[drc] = gfx_mesh_dynamic(cap); }
    static uint32_t n; float key = fc * 1000 + fr * 10 + rel * 0.1f + 7;
    if (key != g_sm_built_key) { n = sm_build(fc, fr, rel); g_sm_built_key = key; }
    gfx_mesh_update(m_sm_top[drc], g_sm_vt, n); gfx_mesh_update(m_sm_back[drc], g_sm_vb, n); }

/* ---- driving between stops ---- */
static float sm_edge_len(int e) { float L = 0; for (int i = 0; i + 1 < SM_EDGE[e].n; i++) { float dx = SM_EDGE[e].p[i + 1][0] - SM_EDGE[e].p[i][0], dy = SM_EDGE[e].p[i + 1][1] - SM_EDGE[e].p[i][1]; L += sqrtf(dx * dx + dy * dy); } return L; }
static int sm_path(int from, int to, float (*out)[2], int cap) {   /* the painted roads from one stop to another: the points, from first to last */
    int n = 0; if (from == to) { out[0][0] = SM_NODE[from].px; out[0][1] = SM_NODE[from].py; return 1; }
    float dist[SM_NODES]; int prev[SM_NODES], pe[SM_NODES], done[SM_NODES];
    for (int i = 0; i < SM_NODES; i++) { dist[i] = 1e9f; prev[i] = -1; pe[i] = -1; done[i] = 0; } dist[from] = 0;
    for (int it = 0; it < SM_NODES; it++) { int u = -1; for (int i = 0; i < SM_NODES; i++) if (!done[i] && (u < 0 || dist[i] < dist[u])) u = i; if (u < 0 || dist[u] >= 1e9f) break; done[u] = 1;
        for (int e = 0; e < SM_NEDGE; e++) { int v = SM_EDGE[e].a == u ? SM_EDGE[e].b : SM_EDGE[e].b == u ? SM_EDGE[e].a : -1; if (v < 0) continue; float d = dist[u] + sm_edge_len(e); if (d < dist[v]) { dist[v] = d; prev[v] = u; pe[v] = e; } } }
    if (prev[to] < 0) return 0;
    int chain[SM_NODES], nc = 0; for (int v = to; v != from && nc < SM_NODES; v = prev[v]) chain[nc++] = v;
    out[n][0] = SM_NODE[from].px; out[n][1] = SM_NODE[from].py; n++;
    int cur = from; for (int k = nc - 1; k >= 0; k--) { int v = chain[k]; const SmEdge *E = &SM_EDGE[pe[v]]; int fwd = E->a == cur;
        for (int i = 1; i < E->n && n < cap; i++) { int ii = fwd ? i : E->n - 1 - i; out[n][0] = E->p[ii][0]; out[n][1] = E->p[ii][1]; n++; } cur = v; }
    return n; }
static void sm_route_to(int to) {   /* the shortest way along the painted roads from where the kart is */
    int from = g_sm_at; if (from == to) return; float dist[SM_NODES]; int prev[SM_NODES], pe[SM_NODES], done[SM_NODES];
    for (int i = 0; i < SM_NODES; i++) { dist[i] = 1e9f; prev[i] = -1; pe[i] = -1; done[i] = 0; } dist[from] = 0;
    for (int it = 0; it < SM_NODES; it++) { int u = -1; for (int i = 0; i < SM_NODES; i++) if (!done[i] && (u < 0 || dist[i] < dist[u])) u = i; if (u < 0 || dist[u] >= 1e9f) break; done[u] = 1;
        for (int e = 0; e < SM_NEDGE; e++) { int v = SM_EDGE[e].a == u ? SM_EDGE[e].b : SM_EDGE[e].b == u ? SM_EDGE[e].a : -1; if (v < 0) continue; float d = dist[u] + sm_edge_len(e); if (d < dist[v]) { dist[v] = d; prev[v] = u; pe[v] = e; } } }
    if (prev[to] < 0) return;
    int chain[SM_NODES], nc = 0; for (int v = to; v != from && nc < SM_NODES; v = prev[v]) chain[nc++] = v;
    g_sm_nroute = 0; g_sm_route[g_sm_nroute][0] = SM_NODE[from].px; g_sm_route[g_sm_nroute][1] = SM_NODE[from].py; g_sm_nroute++;
    int cur = from; for (int k = nc - 1; k >= 0; k--) { int v = chain[k], e = pe[v]; const SmEdge *E = &SM_EDGE[e]; int fwd = E->a == cur;
        for (int i = 1; i < E->n && g_sm_nroute < 96; i++) { int ii = fwd ? i : E->n - 1 - i; g_sm_route[g_sm_nroute][0] = E->p[ii][0]; g_sm_route[g_sm_nroute][1] = E->p[ii][1]; g_sm_nroute++; } cur = v; }
    g_sm_rlen = 0; for (int i = 0; i + 1 < g_sm_nroute; i++) { float dx = g_sm_route[i + 1][0] - g_sm_route[i][0], dy = g_sm_route[i + 1][1] - g_sm_route[i][1]; g_sm_rlen += sqrtf(dx * dx + dy * dy); }
    g_sm_rpos = 0; g_sm_driving = 1; g_sm_at = to; }
static void sm_route_point(float d, float *px, float *py, float *dx, float *dy) {   /* a smooth point along the route, d pixels in */
    for (int i = 0; i + 1 < g_sm_nroute; i++) { float ax = g_sm_route[i][0], ay = g_sm_route[i][1], bx = g_sm_route[i + 1][0], by = g_sm_route[i + 1][1], l = sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
        if (d <= l || i + 2 == g_sm_nroute) { float u = l > 0 ? clampf(d / l, 0, 1) : 1; *px = ax + (bx - ax) * u; *py = ay + (by - ay) * u; *dx = bx - ax; *dy = by - ay; return; } d -= l; }
    *px = SM_NODE[g_sm_at].px; *py = SM_NODE[g_sm_at].py; *dx = 0; *dy = 1; }
static void sm_select(int i) { if (i < 0 || i >= SM_NODES || i == g_sm_sel) return; g_sm_sel = i; smx(SMX_SELECT, 0.9f); if (sm_open(i) && i != g_sm_at) { sm_route_to(i); smx(SMX_DRIVE, 0.8f); } }

/* ---- a character's journey, or your own travels: a line that draws itself along the roads ---- */
static float g_sm_jcum[1200];
static int sm_trip_n(void) { return g_sm_view == 1 ? SM_TRIP[g_sm_jchar].n : (g_save.ntrail < 24 ? g_save.ntrail : 24); }
static int sm_trip_node(int i) { if (g_sm_view == 1) return SM_TRIP[g_sm_jchar].s[i].node; int r = g_save.trail[i]; for (int k = 0; k < SM_NODES; k++) if (SM_NODE[k].track == r) return k; return N_JERU; }
static void sm_journey_build(void) {
    g_sm_njl = 0; g_sm_jn = sm_trip_n(); if (g_sm_jn > 25) g_sm_jn = 25; float cum = 0; static float tmp[200][2];
    for (int i = 0; i < g_sm_jn; i++) { int nd = sm_trip_node(i); g_sm_jnode[i] = nd;
        if (i == 0 || g_sm_njl == 0) { if (g_sm_njl < 1200) { g_sm_jl[g_sm_njl][0] = SM_NODE[nd].px; g_sm_jl[g_sm_njl][1] = SM_NODE[nd].py; g_sm_jcum[g_sm_njl] = 0; g_sm_njl++; } g_sm_jstop[i] = 0; continue; }
        int n = sm_path(g_sm_jnode[i - 1], nd, tmp, 200);
        for (int k = 1; k < n && g_sm_njl < 1200; k++) { float dx = tmp[k][0] - g_sm_jl[g_sm_njl - 1][0], dy = tmp[k][1] - g_sm_jl[g_sm_njl - 1][1]; cum += sqrtf(dx * dx + dy * dy);
            g_sm_jl[g_sm_njl][0] = tmp[k][0]; g_sm_jl[g_sm_njl][1] = tmp[k][1]; g_sm_jcum[g_sm_njl] = cum; g_sm_njl++; }
        g_sm_jstop[i] = cum; }
    g_sm_jt = 0; }
#define SM_JSPEED 230.0f
#define SM_JPAUSE 2.6f
static float sm_jprog(int *stop, int *moving) {   /* how far along the line the drawing has got; the last stop reached */
    float t = g_sm_jt - 1.0f; *stop = 0; *moving = 0; if (g_sm_jn == 0) return 0; if (t < 0) return 0;
    for (int i = 0; i + 1 < g_sm_jn; i++) { t -= SM_JPAUSE; if (t < 0) { *stop = i; return g_sm_jstop[i]; }
        float seg = g_sm_jstop[i + 1] - g_sm_jstop[i], tt = fmaxf(seg / SM_JSPEED, 0.6f); if (t < tt) { *stop = i; *moving = 1; return g_sm_jstop[i] + seg * (t / tt); } t -= tt; }
    *stop = g_sm_jn - 1; return g_sm_jstop[g_sm_jn - 1]; }
static int sm_jdone(void) { int s0, mv; float d = sm_jprog(&s0, &mv); (void)d; return s0 == g_sm_jn - 1 && g_sm_jt > 1.0f + 0.1f; }
static void sm_jpoint(float d, float *px, float *py, float *dx, float *dy) {
    *dx = 0; *dy = 1; if (g_sm_njl == 0) { *px = *py = 0; return; }
    for (int i = 0; i + 1 < g_sm_njl; i++) if (d <= g_sm_jcum[i + 1] || i + 2 == g_sm_njl) { float l = g_sm_jcum[i + 1] - g_sm_jcum[i], u = l > 0 ? clampf((d - g_sm_jcum[i]) / l, 0, 1) : 1;
        *px = g_sm_jl[i][0] + (g_sm_jl[i + 1][0] - g_sm_jl[i][0]) * u; *py = g_sm_jl[i][1] + (g_sm_jl[i + 1][1] - g_sm_jl[i][1]) * u; *dx = g_sm_jl[i + 1][0] - g_sm_jl[i][0]; *dy = g_sm_jl[i + 1][1] - g_sm_jl[i][1]; return; }
    *px = g_sm_jl[0][0]; *py = g_sm_jl[0][1]; }
static void sm_set_view(int v) { g_sm_view = v; if (v == 1 && !char_unlocked(g_sm_jchar)) g_sm_jchar = g_player_char; if (v) sm_journey_build(); }
/* a race is over: it goes on MY TRAVELS */
static void sm_trail_add(int road, float meters) {
    if (road < 0 || road >= SAVE_MAX_ROADS) return; g_save.meters += (int)fmaxf(meters, 0);
    for (int i = 0; i < g_save.ntrail; i++) if (g_save.trail[i] == road) { g_save.dirty = 1; return; }   /* each road once, in the order first raced */
    if (g_save.ntrail < 24) g_save.trail[g_save.ntrail++] = road; g_save.dirty = 1; }

/* ---- the screen ---- */
static void story_enter(int unfold) {
    g_story = 1; g_screen = SCR_STORY; g_screen_t = unfold ? 0 : 1; if (unfold) { g_sm_t = 0; g_sm_cam_ok = 0; } else g_sm_t = 99;
    if (!g_smx_loaded) { static const char *nm[SMX_COUNT] = {"map_unfold", "map_rise", "map_pin", "map_select", "map_drive", "map_unlock", "map_stop", "map_draw"}; g_smx_loaded = 1; for (int i = 0; i < SMX_COUNT; i++) g_smx[i] = sfx_voice_load(nm[i]); }
    sc_load_props(); if (!g_sm_tex_tried) { g_sm_tex_tried = 1; tx_story = asset_texture("story_map", 0, NULL, NULL); }
    int cur = g_save.story < SM_NODES ? g_save.story : SM_NODES - 1;
    if (unfold) { g_sm_sel = g_sm_at = cur; g_sm_driving = 0; } }
static void story_start_race(void);
static void sm_nav(int mx, int my) {   /* the stick: the nearest stop that way */
    float bx = SM_NODE[g_sm_sel].px, by = SM_NODE[g_sm_sel].py; int best = -1; float bs = 1e9f;
    for (int i = 0; i < SM_NODES; i++) { if (i == g_sm_sel) continue; float dx = SM_NODE[i].px - bx, dy = SM_NODE[i].py - by, d = sqrtf(dx * dx + dy * dy); if (d < 1) continue;
        float along = (dx * mx + dy * my) / d; if (along < 0.45f) continue; float score = d * (2.0f - along); if (score < bs) { bs = score; best = i; } }
    if (best >= 0) sm_select(best); }
static void story_update(float dt, int pa, int pb, int mx, int my, int px_, int py_, int pplus) {
    g_sm_t += dt; g_sm_dt = dt;
    if (g_sm_t < SM_UNFOLD_END) {   /* the map coming out: the paper sounds; A skips it */
        static float last; if (g_sm_t < last) last = 0;
        if ((last < 0.9f && g_sm_t >= 0.9f) || (last < 2.5f && g_sm_t >= 2.5f)) smx(SMX_UNFOLD, 1.0f);   /* the paper opening out */
        if (last < 3.7f && g_sm_t >= 3.7f) smx(SMX_RISE, 0.9f);                                          /* the land rising */
        for (int i = 0; i < SM_NODES; i += 3) { float at = 3.7f + 0.4f + i * 0.03f; if (last < at && g_sm_t >= at) smx(SMX_PIN, 0.5f); }   /* the pins popping up */
        last = g_sm_t;
        if (pa || pb) g_sm_t = SM_UNFOLD_END; return; }
    if (g_sm_driving) { g_sm_rpos += dt * 260.0f; if (g_sm_rpos >= g_sm_rlen) { g_sm_driving = 0; g_sm_rpos = g_sm_rlen; } }
    if (g_sm_cel > 0) { float was = g_sm_cel; g_sm_cel -= dt;
        if (was > 1.6f && g_sm_cel <= 1.6f && g_save.story < SM_NODES) { sm_select(g_save.story); smx(SMX_UNLOCK, 1.0f); }   /* the next stop opens and the kart drives there */
        if (pa && g_sm_cel < 3.0f) g_sm_cel = 0; return; }
    if (py_) { sm_set_view((g_sm_view + 1) % 3); if (g_sm_view == 1) g_sm_jchar = g_player_char, sm_journey_build(); sfx_play(SFX_MENU_OK, 1, 0); return; }
    if (g_sm_view) {   /* a journey drawing itself: left / right another character, A to skip to the end or play it again */
        { int s0a, mva; sm_jprog(&s0a, &mva); g_sm_jt += dt; int s0b, mvb; sm_jprog(&s0b, &mvb); static float scr;   /* a chime at each stop reached, a quill scratching while the line draws */
          if (g_sm_jt > 1.0f && (s0b != s0a || (g_sm_jt - dt <= 1.0f))) smx(SMX_STOP, 0.8f);
          if (mvb) { scr -= dt; if (scr <= 0) { smx(SMX_DRAW, 0.5f); scr = 0.75f; } } else scr = 0; }
        if (mx && g_sm_view == 1) { for (int k = 0; k < NCHARS; k++) { g_sm_jchar = (g_sm_jchar + mx + NCHARS) % NCHARS; if (char_unlocked(g_sm_jchar)) break; } sm_journey_build(); sfx_play(SFX_MENU_MOVE, 1, 0); }
        if (pa) { if (sm_jdone()) g_sm_jt = 0; else g_sm_jt = 1e4f; sfx_play(SFX_MENU_OK, 0.7f, 0); }
        if (pb) { g_story = 0; g_screen = SCR_MAIN; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); }
        return; }
    if (mx || my) sm_nav(mx, my);
    if (pplus) { g_sm_over = !g_sm_over; sfx_play(SFX_MENU_MOVE, 1, 0); }
    if (px_) { g_screen = SCR_CHAR; g_screen_t = 0; sfx_play(SFX_MENU_OK, 1, 0); return; }
    if (pa && !g_sm_tapped) { if (sm_open(g_sm_sel)) { sfx_play(SFX_MENU_OK, 1, 0); story_start_race(); } else sfx_play(SFX_WRONG_WAY, 1, 0); }
    g_sm_tapped = 0;
    if (pb) { g_story = 0; g_screen = SCR_MAIN; g_screen_t = 0; sfx_play(SFX_MENU_BACK, 1, 0); } }
static void story_start_race(void) {
    int tr = SM_NODE[g_sm_sel].track; g_multi = 0; g_sm_raced = 0; g_sm_won = 0;
    if (tr == ARENA_TRACK) { g_mode = MODE_BATTLE; begin_battle(); }
    else if (tr == JERICHO_TRACK) { g_mode = MODE_JERICHO; begin_player_race(); }
    else { g_mode = MODE_SINGLE; g_track_idx = tr; begin_player_race(); }
    g_screen_t = 0; }
static void story_race_over(const Race *r) {   /* called once when a story race ends: did it clear the stop? */
    if (!g_story) return; g_sm_raced = 1; const Kart *k = &r->k[r->player];
    g_sm_won = g_mode == MODE_BATTLE ? g_battle.won : g_mode == MODE_JERICHO ? k->place == 1 : k->place <= 3; }
static void story_back_from_race(void) {
    int cleared_new = g_sm_raced && g_sm_won && g_sm_sel == g_save.story && g_save.story < SM_NODES;
    if (cleared_new) { g_save.story++; g_save.dirty = 1; g_sm_cel = 4.5f; g_sm_cel_node = g_sm_sel; sfx_play(SFX_WIN, 1, 0); }
    set_track(0); g_mode = MODE_SINGLE; g_multi = 0; start_race(1);
    story_enter(0); g_sm_raced = 0; }

static void sm_draw_model(const Kart3D *k, Vec3 at, float yaw, float want, float r, float g, float b, float a) {   /* a model scaled so its biggest side is 'want' units */
    if (!k || !k->ok) return; float ext = fmaxf(fmaxf(k->hi.x - k->lo.x, k->hi.y - k->lo.y), k->hi.z - k->lo.z); if (ext < 1e-4f) return; float s = want / ext;
    Mat4 m = m4_mul(m4_translate(at.x, at.y - k->lo.y * s, at.z), m4_mul(m4_rotate_y(yaw), m4_scale(s, s, s))); kart3d_draw(k, &m, 0, 0, r, g, b, a); }
static void draw_story_map(int drc) {
    Mat4 id = m4_identity(); float t = g_time, rel = sm_relief(), open = g_sm_t >= SM_UNFOLD_END ? 1 : 0;
    if (!g_sm_tex_tried) story_enter(0);
    /* the camera: close on the folded map, back as it opens, then over the stop picked (or the whole map: +) */
    Vec3 want_at, want_eye; float dist;
    if (g_sm_t < SM_UNFOLD_END) { float u = sc_smooth((g_sm_t - 0.6f) / 3.4f); dist = 70 + 70 * u; want_at = v3(0, 0, 6 * u); }
    else if (g_sm_view && g_sm_njl > 0) { float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f; for (int i = 0; i < g_sm_njl; i++) { x0 = fminf(x0, g_sm_jl[i][0]); x1 = fmaxf(x1, g_sm_jl[i][0]); z0 = fminf(z0, g_sm_jl[i][1]); z1 = fmaxf(z1, g_sm_jl[i][1]); }
        want_at = sm_pos((x0 + x1) * 0.5f, (z0 + z1) * 0.5f + 40); want_at.y = 0; dist = clampf(fmaxf((x1 - x0) * 0.1f, (z1 - z0) * 0.1f * 1.5f) * 1.05f + 45, 60, 145); }
    else if (g_sm_over || g_sm_view) { dist = 140; want_at = v3(0, 0, 6); }
    else { float kx, ky, dx, dy; if (g_sm_driving) sm_route_point(g_sm_rpos, &kx, &ky, &dx, &dy); else { kx = SM_NODE[g_sm_sel].px; ky = SM_NODE[g_sm_sel].py; }
        if (!g_sm_driving && !sm_open(g_sm_sel)) { kx = SM_NODE[g_sm_sel].px; ky = SM_NODE[g_sm_sel].py; }
        want_at = sm_pos(kx, ky); want_at.x = clampf(want_at.x, -SM_W * 0.3f, SM_W * 0.3f); want_at.z = clampf(want_at.z, -SM_D * 0.3f, SM_D * 0.32f); dist = 62; }
    want_eye = v3(want_at.x + sinf(t * 0.13f) * dist * 0.05f, want_at.y + dist * 0.8f, want_at.z + dist * 0.6f);
    if (!g_sm_cam_ok) { g_sm_eye = want_eye; g_sm_look = want_at; g_sm_cam_ok = 1; }
    { float f = g_sm_t < SM_UNFOLD_END ? 1.0f : fminf(1.0f, g_sm_dt * 2.5f); g_sm_eye = sc_lerp(g_sm_eye, want_eye, f); g_sm_look = sc_lerp(g_sm_look, want_at, f); }
    float aspect = drc ? 854.0f / 480.0f : 1280.0f / 720.0f;
    Mat4 vp = m4_mul(m4_perspective(0.75f, aspect, 1.0f, 2000.0f), m4_look_at(g_sm_eye, g_sm_look, v3(0, 1, 0)));
    { Mat4 lift = m4_identity(); lift.m[13] = 0.1f; vp = m4_mul(lift, vp); }
    gfx_clear_depth(drc);
    gfx_light(v3(1.05f, 0.98f, 0.88f), v3(0.62f, 0.62f, 0.66f), v3(0.36f, 0.3f, 0.24f), g_sm_eye);
    gfx_camera(&vp, v3_norm(v3(-0.35f, 0.85f, 0.4f)), 0.6f, 0.14f, 0.09f, 0.06f, 3000.0f, 6000.0f);
    gfx_state(ST_DEPTH | ST_DEPTH_WRITE); gfx_bind(NULL);
    {   /* the table: dark wooden boards, lit in the middle */
        static Mesh *m_tab; if (!m_tab) { static Vertex tv[6 * 40]; Batch tb; batch_init(&tb, tv, 6 * 40);
            for (int k = 0; k < 20; k++) { float z0 = -150 + k * 15, sh = 0.8f + 0.2f * noise2(k, 3); batch_quad4(&tb, v3(-260, -1.2f, z0), v3(-260, -1.2f, z0 + 14.6f), v3(260, -1.2f, z0 + 14.6f), v3(260, -1.2f, z0), v3(0, 1, 0), 0.5f, 0.5f, 0.5f, 0.5f, 0.36f * sh, 0.22f * sh, 0.12f * sh, 1);
                batch_quad4(&tb, v3(-260, -1.25f, z0 + 14.6f), v3(-260, -1.25f, z0 + 15), v3(260, -1.25f, z0 + 15), v3(260, -1.25f, z0 + 14.6f), v3(0, 1, 0), 0.5f, 0.5f, 0.5f, 0.5f, 0.12f, 0.07f, 0.04f, 1); }
            m_tab = gfx_mesh_static(tb.v, tb.n); }
        gfx_draw_ex(m_tab, &id, 1, 1, 1, 1, 0.15f, 0, 0, 0); }
    {   /* the map itself */
        int st = 0; sm_update_paper(drc, &st);
        gfx_bind(NULL); gfx_draw_ex(st ? m_sm_back_s : m_sm_back[drc], &id, 1, 1, 1, 1, 0, 0, 0, 0);
        gfx_bind(tx_story); gfx_draw_ex(st ? m_sm_top_s : m_sm_top[drc], &id, 1, 1, 1, 1, 0.05f, 0, 0, 0); gfx_bind(NULL); }
    static Mesh *m_dyn[2], *m_glow[2]; static Vertex dv[16000], gv[6000]; if (!m_dyn[drc]) { m_dyn[drc] = gfx_mesh_dynamic(16000); m_glow[drc] = gfx_mesh_dynamic(6000); }
    Batch db; batch_init(&db, dv, 16000); Batch gb; batch_init(&gb, gv, 6000);
    Vec3 cam_r = v3_norm(v3_cross(v3(0, 1, 0), v3_sub(g_sm_eye, g_sm_look))), cam_u = v3(0, 1, 0);
#define SM_GLOW(P, S, R, G, B, A) batch_quad(&gb, (P), v3(cam_r.x * (S), cam_r.y * (S), cam_r.z * (S)), v3(0, (S), 0), v3(0, 0, 1), 0, 0, 1, 1, R, G, B, A)
    if (rel > 0.01f) {
        float pop = rel;   /* everything springs up with the land */
        /* the moving pictures, all 3D models */
        gfx_state(ST_DEPTH | ST_DEPTH_WRITE);
        { Vec3 p = sm_pos(800 + 30 * cosf(t * 0.25f), 330 + 14 * sinf(t * 0.25f)); p.y = 0.25f + 0.25f * sinf(t * 1.7f); sm_draw_model(&g_prop[PR_BOAT], p, -t * 0.25f + 1.57f, 7.5f * pop, 1, 1, 1, 1); }   /* a fishing boat sailing round Galilee */
        { float u = 0.5f + 0.5f * sinf(t * 0.18f); Vec3 p = sm_pos(110 + 60 * u, 600 + 140 * u); sm_draw_model(road_prop("@camel"), p, cosf(t * 0.18f) > 0 ? 0.6f : 3.74f, 5.5f * pop, 1, 1, 1, 1); }   /* a camel plodding across Egypt */
        { Vec3 p = sm_pos(160 + 40 * sinf(t * 0.2f), 905); p.y = 0.1f; sm_draw_model(road_prop("@crocodile"), p, cosf(t * 0.2f) > 0 ? 1.57f : -1.57f, 6.0f * pop, 1, 1, 1, 1); }   /* a crocodile in the Nile */
        { float a = t * 0.3f; Vec3 p = sm_pos(1180 + 40 * cosf(a), 1035 + 18 * sinf(a)); sm_draw_model(road_prop("@lion_roar"), p, -a, 5.5f * pop, 1, 1, 1, 1); }   /* a lion pacing outside the den */
        { Vec3 p = sm_pos(420, 610); sm_draw_model(&g_prop[PR_SHEEP], p, 0.8f + 0.3f * sinf(t * 0.5f), 2.2f * pop, 1, 1, 1, 1); p = sm_pos(445, 625); sm_draw_model(&g_prop[PR_SHEEP], p, 2.5f, 2.0f * pop, 1, 1, 1, 1); }   /* sheep near Bethlehem */
        for (int k = 0; k < 3; k++) { float a = t * 0.5f + k * 2.1f; Vec3 c = k == 2 ? sm_pos(490, 450) : sm_pos(672, 160); Vec3 p = v3(c.x + 9 * cosf(a), c.y + 9 + 1.5f * sinf(t * 2 + k), c.z + 6 * sinf(a));   /* doves over Eden and Bethlehem */
            sm_draw_model(&g_prop[PR_DOVE], p, -a, 3.2f * pop, 1, 1, 1, 1); }
        for (int k = 0; k < 4; k++) { float x = 960 + fmodf(t * 6 + k * 90, 360), y = 160 + k * 85; Vec3 p = sm_pos(x, y); p.y = 5 + k % 2; float fade = sinf(clampf((x - 960) / 360, 0, 1) * 3.14159f);   /* clouds drifting round Heaven */
            sm_draw_model(&g_prop[PR_CLOUD], p, 0.3f * k, 9.0f * pop * (0.4f + 0.6f * fade), 1, 1, 1, 1); }
        /* the glows: Bethlehem's star, Heaven's gate, the empty tomb, the furnace fire, the Red Sea spray, sparkles on the sea */
        { Vec3 p = sm_pos(490, 385); SM_GLOW(v3(p.x, p.y + 2.5f, p.z), 4 + 0.8f * sinf(t * 3), 1, 0.95f, 0.7f, 0.9f * pop); }
        { Vec3 p = sm_pos(1105, 115); SM_GLOW(v3(p.x, p.y + 2, p.z), 9 + 1.5f * sinf(t * 1.5f), 1, 0.9f, 0.5f, 0.7f * pop); }
        { Vec3 p = sm_pos(1110, 395); SM_GLOW(v3(p.x, p.y + 1.5f, p.z), 4.5f + sinf(t * 2), 1, 0.85f, 0.4f, 0.8f * pop); }
        for (int k = 0; k < 14; k++) { float h1 = noise2(k, 41), ph = fmodf(t * 0.9f + h1, 1.0f); Vec3 p = sm_pos(770 + 76 * h1, 830); p.y += 1 + ph * 7;
            SM_GLOW(p, 1.6f * (1 - ph) + 0.4f, 1, 0.55f + 0.3f * (1 - ph), 0.15f, (1 - ph) * pop); }
        for (int k = 0; k < 12; k++) { float h1 = noise2(k, 77), ph = fmodf(t * 0.6f + h1, 1.0f); Vec3 p = sm_pos(400 + 130 * h1, 945 + 20 * noise2(k, 5)); p.y += 1 + 5 * sinf(ph * 3.14159f);
            SM_GLOW(p, 1.3f, 0.8f, 0.92f, 1, sinf(ph * 3.14159f) * pop); }
        for (int k = 0; k < 40; k++) { float h1 = noise2(k, 9), h2 = noise2(k, 13), ph = fmodf(t * 0.35f + h1 * 3, 1.0f); float px = h1 * 1341, py = h2 * 1173; if (sm_hgt(px, py) > 0.08f) continue;
            Vec3 p = sm_pos(px, py); SM_GLOW(v3(p.x, 0.3f, p.z), 1.1f, 0.8f, 0.95f, 1, sinf(ph * 3.14159f) * 0.8f * pop); }
        /* the stops: a pin for each, a padlock until it opens, a gold flag once it is cleared, a beacon over the next one */
        for (int i = 0; i < SM_NODES; i++) { Vec3 c = sm_pos(SM_NODE[i].px, SM_NODE[i].py); float sp = sc_smooth((rel - i * 0.03f) * 2.5f), bob = 0.35f * sinf(t * 2.6f + i), sel = i == g_sm_sel;
            if (sp <= 0) continue; int op = sm_open(i) || g_sm_view, cl = sm_cleared(i) && !g_sm_view, nxt = i == g_save.story && !g_sm_view; if (g_sm_view) sel = 0;
            float pr = cl ? 1.0f : op ? 0.4f : 0.55f, pg = cl ? 0.82f : op ? 0.9f : 0.55f, pb = cl ? 0.25f : op ? 1.0f : 0.6f;
            sc_tube(&db, v3(c.x, c.y, c.z), v3(c.x, c.y + 0.35f, c.z), 2.6f * sp, 2.6f * sp, 18, 0.95f, 0.78f, 0.35f, 1, 1);   /* a gold base */
            sc_tube(&db, v3(c.x, c.y + 0.35f, c.z), v3(c.x, c.y + 3.6f * sp, c.z), 0.18f, 0.18f, 6, 0.9f, 0.9f, 0.88f, 1, 1);
            sc_ball(&db, v3(c.x, c.y + 4.4f * sp + bob, c.z), (sel ? 1.7f : 1.3f) * sp, 12, pr, pg, pb, 1); SM_GLOW(v3(c.x, c.y + 4.4f * sp + bob, c.z), 3.2f * sp, pr, pg, pb, 0.45f);
            if (!op) {   /* a padlock */
                Vec3 l = v3(c.x + 2.2f, c.y + 1.4f, c.z + 1.2f); sc_tube(&db, v3(l.x, l.y - 0.8f, l.z), v3(l.x, l.y + 0.6f, l.z), 0.9f, 0.9f, 4, 0.55f, 0.5f, 0.45f, 1, 1);
                for (int s2 = 0; s2 < 6; s2++) { float a0 = 3.14159f * s2 / 6, a1 = 3.14159f * (s2 + 1) / 6; sc_tube(&db, v3(l.x + 0.55f * cosf(a0), l.y + 0.6f + 0.7f * sinf(a0), l.z), v3(l.x + 0.55f * cosf(a1), l.y + 0.6f + 0.7f * sinf(a1), l.z), 0.14f, 0.14f, 5, 0.75f, 0.75f, 0.78f, 1, 1); } }
            if (cl) {   /* a gold flag waving */
                Vec3 f = v3(c.x - 1.6f, c.y, c.z - 0.8f); sc_tube(&db, f, v3(f.x, f.y + 6.5f * sp, f.z), 0.12f, 0.12f, 5, 0.5f, 0.32f, 0.16f, 1, 1); sc_ball(&db, v3(f.x, f.y + 6.6f * sp, f.z), 0.25f, 6, 1, 0.85f, 0.3f, 1);
                for (int s2 = 0; s2 < 6; s2++) { float u0 = s2 / 6.0f, u1 = (s2 + 1) / 6.0f, w0 = 0.35f * sinf(t * 5 - u0 * 4) * u0, w1 = 0.35f * sinf(t * 5 - u1 * 4) * u1, y0 = f.y + 6.3f * sp;
                    batch_quad4(&db, v3(f.x + u0 * 3, y0 - 1.8f, f.z + w0), v3(f.x + u1 * 3, y0 - 1.8f, f.z + w1), v3(f.x + u1 * 3, y0, f.z + w1), v3(f.x + u0 * 3, y0, f.z + w0), v3(0, 0, 1), 0.5f, 0.5f, 0.5f, 0.5f, 1, 0.8f - 0.1f * (s2 & 1), 0.22f, 1);
                    batch_quad4(&db, v3(f.x + u0 * 3, y0, f.z + w0 - 0.02f), v3(f.x + u1 * 3, y0, f.z + w1 - 0.02f), v3(f.x + u1 * 3, y0 - 1.8f, f.z + w1 - 0.02f), v3(f.x + u0 * 3, y0 - 1.8f, f.z + w0 - 0.02f), v3(0, 0, -1), 0.5f, 0.5f, 0.5f, 0.5f, 1, 0.8f, 0.22f, 1); } }
            if (nxt && op) { float pu = 0.5f + 0.5f * sinf(t * 4); SM_GLOW(v3(c.x, c.y + 4.4f + bob, c.z), 4 + 2 * pu, 0.6f, 0.95f, 1, 0.9f);   /* the next stop: a pulsing beacon */
                for (int k = 0; k < 8; k++) { float ph = fmodf(t * 0.7f + k / 8.0f, 1.0f); SM_GLOW(v3(c.x + 1.5f * cosf(k * 0.8f + t), c.y + ph * 14, c.z + 1.5f * sinf(k * 0.8f + t)), 0.7f, 0.7f, 0.95f, 1, 1 - ph); } }
            if (sel) for (int k = 0; k < 24; k++) { float a = k * 0.2618f + t * 1.5f; SM_GLOW(v3(c.x + 3.2f * cosf(a), c.y + 0.6f, c.z + 3.2f * sinf(a)), 0.55f, 1, 0.9f, 0.5f, 0.9f); }
            if (g_sm_cel > 0 && i == g_sm_cel_node) for (int k = 0; k < 30; k++) { float h1 = noise2(k, 3), ph = clampf((4.5f - g_sm_cel) / 1.6f, 0, 1), a = h1 * 6.283f;   /* the burst of light over a stop just cleared */
                SM_GLOW(v3(c.x + cosf(a) * ph * 9, c.y + 5 + sinf(k * 1.7f) * ph * 6, c.z + sinf(a) * ph * 9), 1.2f, 1, 0.85f + 0.15f * h1, 0.4f, 1 - ph); } }
        if (g_sm_view && g_sm_njl > 1) {   /* the journey: a glowing line drawn along the roads up to where it has got, pins at the stops */
            int s0, mv; float d = sm_jprog(&s0, &mv); float lr = g_sm_view == 2 ? 1.0f : 1.0f, lg = g_sm_view == 2 ? 0.3f : 0.82f, lb = g_sm_view == 2 ? 0.22f : 0.25f;
            Vec3 prev = sm_pos(g_sm_jl[0][0], g_sm_jl[0][1]); prev.y += 0.9f;
            for (float u = 6; u <= d + 0.01f; u += 6) { float px, py2, dx, dy; sm_jpoint(fminf(u, d), &px, &py2, &dx, &dy); Vec3 q = sm_pos(px, py2); q.y += 0.9f; sc_tube(&db, prev, q, 0.42f, 0.42f, 6, lr, lg, lb, 1, 1); prev = q;
                if ((int)(u / 6) % 4 == 0) SM_GLOW(q, 1.2f, lr, lg + 0.1f, lb + 0.2f, 0.35f); }
            for (int i = 0; i < g_sm_jn; i++) { if (g_sm_jstop[i] > d + 0.01f || (i > s0)) break; Vec3 c = sm_pos(SM_NODE[g_sm_jnode[i]].px, SM_NODE[g_sm_jnode[i]].py); int first = i == 0, last = i == g_sm_jn - 1 && sm_jdone();
                float r = first ? 0.3f : last ? 1.0f : 0.95f, g = first ? 0.95f : last ? 0.8f : 0.95f, b = first ? 0.4f : last ? 0.2f : 0.95f, bob = 0.4f * sinf(t * 3 + i);
                sc_tube(&db, v3(c.x + 1.2f, c.y, c.z + 1.2f), v3(c.x + 1.2f, c.y + 7, c.z + 1.2f), 0.2f, 0.2f, 5, 0.9f, 0.9f, 0.9f, 1, 1); sc_ball(&db, v3(c.x + 1.2f, c.y + 7.8f + bob, c.z + 1.2f), first || last ? 1.6f : 1.1f, 10, r, g, b, 1);
                SM_GLOW(v3(c.x + 1.2f, c.y + 7.8f + bob, c.z + 1.2f), 3.5f, r, g, b, 0.6f); } }
        if (db.n) { gfx_mesh_update(m_dyn[drc], db.v, db.n); gfx_state(ST_DEPTH | ST_DEPTH_WRITE); gfx_bind(NULL); gfx_draw_ex(m_dyn[drc], &id, 1, 1, 1, 1, 0.6f, 0, 0, 0); }
        /* your character's kart, driving along the painted roads */
        { int ch = g_sm_view == 1 ? g_sm_jchar : g_player_char, jmv = 0; if (ch >= 0 && ch < NCHARS && g_k3d[ch].ok && (!g_sm_view || g_sm_njl > 0)) { float kx, ky, dx = 0, dy = 1;
            if (g_sm_view) { int s0; float d = sm_jprog(&s0, &jmv); sm_jpoint(d, &kx, &ky, &dx, &dy); if (!jmv) { kx += 22; ky += 18; dx = -1; dy = 1; } }
            else if (g_sm_driving) sm_route_point(g_sm_rpos, &kx, &ky, &dx, &dy); else { kx = SM_NODE[g_sm_at].px + 22; ky = SM_NODE[g_sm_at].py + 18; dx = -1; dy = 1; }
            int driving = g_sm_view ? jmv : g_sm_driving;
            Vec3 p = sm_pos(kx, ky); float yaw = atan2f(dx, dy); Vec3 ah = sm_pos(kx + dx * 0.05f, ky + dy * 0.05f);
            K3Pose P; memset(&P, 0, sizeof P); P.t = t; if (!driving) { P.arm[1] = K3A_WAVE; P.arm_w[1] = 1; P.look_yaw = 0.4f * sinf(t * 0.7f); } else P.bob = 0.03f * fabsf(sinf(t * 9));
            if (g_sm_cel > 0) { P.arm[0] = P.arm[1] = K3A_UP; P.arm_w[0] = P.arm_w[1] = 1; }
            float s = 3.4f * pop; (void)ah; Mat4 m = m4_mul(m4_translate(p.x, p.y + 0.1f + (driving ? 0.15f * fabsf(sinf(t * 11)) : 0), p.z), m4_mul(m4_rotate_y(yaw), m4_scale(s, s, s)));
            gfx_state(ST_DEPTH | ST_DEPTH_WRITE); kart3d_draw_pose(&g_k3d[ch], &m, driving ? t * 12 : 0, 0, &P, 1, 1, 1, 1);
            if (driving) for (int k = 0; k < 6; k++) { float ph = fmodf(t * 3 + k / 6.0f, 1.0f); SM_GLOW(v3(p.x - sinf(yaw) * (1.5f + ph * 3), p.y + 0.5f + ph, p.z - cosf(yaw) * (1.5f + ph * 3)), 0.7f + ph, 0.9f, 0.8f, 0.6f, 0.5f * (1 - ph)); } } }
        if (gb.n) { gfx_mesh_update(m_glow[drc], gb.v, gb.n); gfx_state(ST_TRANSPARENT | ST_ADD); gfx_bind(tx_blob); gfx_draw_ex(m_glow[drc], &id, 1, 1, 1, 1, 0, 1, 0, 0); gfx_bind(NULL); }
    }
#undef SM_GLOW
    (void)open; (void)cam_u;
    for (int i = 0; i < SM_NODES; i++) { Vec3 c = sm_pos(SM_NODE[i].px, SM_NODE[i].py); const float *M = vp.m; float cx = M[0] * c.x + M[4] * c.y + M[8] * c.z + M[12], cy = M[1] * c.x + M[5] * c.y + M[9] * c.z + M[13], cw = M[3] * c.x + M[7] * c.y + M[11] * c.z + M[15];
        g_sm_scr[i][2] = 0; if (cw < 0.5f) continue; g_sm_scr[i][0] = (cx / cw * 0.5f + 0.5f) * 1280; g_sm_scr[i][1] = (1 - (cy / cw * 0.5f + 0.5f)) * 720; g_sm_scr[i][2] = 1; }
    gfx_camera_2d(); gfx_state(ST_HUD); gfx_bind(NULL); }
static void hud_journey(float st, const UiLook *L) {   /* the journey views: whose journey, the stop reached and its verse */
    char buf[96]; int s0, mv; float d = sm_jprog(&s0, &mv); (void)d;
    if (g_sm_view == 1) { const char *nm = CHARACTERS[g_sm_jchar].name; int i = 0; for (; nm[i] && i < 60; i++) buf[i] = (char)(nm[i] >= 'a' && nm[i] <= 'z' ? nm[i] - 32 : nm[i]); buf[i] = 0; }
    else snprintf(buf, sizeof buf, "MY TRAVELS");
    text3d(buf, 640 - text_width(buf, 28) / 2, 13, 28, 1, 1, 1, st, 0);
    if (g_sm_view == 2) { snprintf(buf, sizeof buf, "%d ROADS - %d KM", g_save.ntrail, g_save.meters / 1000); text3d(buf, 1250 - text_width(buf, 20), 16, 20, 1, 0.85f, 0.4f, st, 1); }
    /* the numbers over the stops reached */
    for (int i = 0; i < g_sm_jn && i <= s0; i++) { int nd = g_sm_jnode[i]; if (!g_sm_scr[nd][2] || g_sm_jt < 1.0f) continue; int dup = 0; for (int k = i + 1; k <= s0 && k < g_sm_jn; k++) if (g_sm_jnode[k] == nd) dup = 1; if (dup) continue;
        snprintf(buf, sizeof buf, "%d", i + 1); float x = g_sm_scr[nd][0] + 8, y = g_sm_scr[nd][1] - 92; ui_plaque(x - 18, y - 6, 36 + (i >= 9 ? 12 : 0), 36, 12, 8, i == 0 ? &UI_SEL : &UI_GOLD, 1, 0, -1);
        text3d(buf, x - text_width(buf, 22) / 2 + (i >= 9 ? 6 : 0), y + 1, 22, i == 0 ? 1 : 0.35f, i == 0 ? 1 : 0.2f, i == 0 ? 1 : 0.05f, 1, 0); }
    float py = 720 - 132 * st; ui_plaque(14, py, 1252, 160, 22, 22, L, 0.95f, 0, -1);
    if (g_sm_jn == 0) text_c("RACE ON ANY ROAD AND IT APPEARS HERE", py + 40, 20, 1, 0.92f, 0.7f, 1);
    else { const char *line = g_sm_view == 1 ? SM_TRIP[g_sm_jchar].s[s0].line : SM_NODE[g_sm_jnode[s0]].name;
        snprintf(buf, sizeof buf, s0 == 0 ? "START" : (s0 == g_sm_jn - 1 && sm_jdone()) ? "FINISH" : "STOP %d", s0 + 1); text3d(buf, 40, py + 14, 22, 1, 0.85f, 0.4f, 1, 1);
        { float sz = 24; while (sz > 13 && text_width(line, sz) > 1180) sz -= 1; text3d(line, 40, py + 50, sz, 1, 1, 1, 1, 0); }
        for (int i = 0; i < g_sm_jn; i++) { float s2 = i == s0 ? 9 : 6; ui_plaque(1236 - (g_sm_jn - 1 - i) * 24 - s2, py + 26 - s2, s2 * 2, s2 * 2, s2 * 0.9f, 5, i <= s0 ? &UI_GOLD : &UI_DIM, 1, i == s0 ? 0.5f : 0, -1); } }
    text_c(g_sm_view == 1 ? "LEFT/RIGHT: CHARACTER   A: SKIP / REPLAY   Y: MY TRAVELS   B: BACK" : "A: SKIP / REPLAY   Y: RACE THE STOPS   B: BACK", 694, 13, 0.9f, 0.92f, 1, 0.85f); }
static void hud_story(void) {
    static const UiLook UI_MAPW = {{0.2f, 0.3f, 0.62f}, {0.06f, 0.09f, 0.28f}, {0.95f, 0.76f, 0.32f}, {0.4f, 0.28f, 0.1f}};
    if (g_sm_t < SM_UNFOLD_END - 0.6f) { text_c("A: SKIP", 690, 14, 0.9f, 0.9f, 1, 0.6f); return; }
    float st = clampf((g_sm_t - (SM_UNFOLD_END - 0.6f)) / 0.6f, 0, 1); char buf[96];
    ui_plaque(-30, -40 - (1 - st) * 80, 1340, 100, 16, 18, &UI_MAPW, 0.95f, 0, -1);
    text3d("BIBLE JOURNEYS", 26, 12, 22, 1, 0.85f, 0.4f, st, 1);
    if (g_sm_view) { hud_journey(st, &UI_MAPW); return; }
    { const char *nm = CHARACTERS[g_player_char].name; int i = 0; for (; nm[i] && i < 60; i++) buf[i] = (char)(nm[i] >= 'a' && nm[i] <= 'z' ? nm[i] - 32 : nm[i]); buf[i] = 0; }
    text3d(buf, 640 - text_width(buf, 26) / 2, 14, 26, 1, 1, 1, st, 0);
    snprintf(buf, sizeof buf, "%d / %d", g_save.story < SM_NODES ? g_save.story : SM_NODES, SM_NODES); text3d(buf, 1250 - text_width(buf, 24), 14, 24, 1, 0.85f, 0.4f, st, 1);
    /* touch a stop on the GamePad to pick it; touch it again to race */
    for (int i = 0; i < SM_NODES; i++) if (g_sm_scr[i][2]) tzone(g_sm_scr[i][0] - 45, g_sm_scr[i][1] - 70, 90, 90, TZ_SEL, i);
    /* the panel: the stop picked */
    float py = 720 - 132 * st; const SmNode *N = &SM_NODE[g_sm_sel]; int op = sm_open(g_sm_sel), cl = sm_cleared(g_sm_sel);
    ui_plaque(14, py, 1252, 160, 22, 22, &UI_MAPW, 0.95f, op && !cl ? 0.25f + 0.25f * sinf(g_time * 3) : 0, -1);
    snprintf(buf, sizeof buf, "STOP %d - %s", g_sm_sel + 1, N->name); { float sz = 24; while (sz > 14 && text_width(buf, sz) > 900) sz -= 1; text3d(buf, 40, py + 14, sz, 1, 0.85f, 0.4f, 1, 1); }
    { float sz = 17; while (sz > 11 && text_width(N->verse, sz) > 1190) sz -= 1; batch_text(&g_b, N->verse, 40, py + 52, sz, 0.95f, 0.95f, 1, 1); }
    const char *state = cl ? "CLEARED" : op ? (N->track == ARENA_TRACK || N->track == JERICHO_TRACK ? "WIN TO CLEAR IT" : "FINISH IN THE TOP 3 TO CLEAR IT") : "LOCKED - CLEAR THE STOPS BEFORE IT";
    batch_text(&g_b, state, 1240 - text_width(state, 16), py + 20, 16, cl ? 1 : op ? 0.6f : 0.75f, cl ? 0.85f : op ? 1 : 0.75f, cl ? 0.35f : op ? 0.6f : 0.8f, 1);
    if (op) { ui_plaque(1080, py + 46, 170, 48, 14, 10, &UI_GOLD, 1, 0.4f + 0.3f * sinf(g_time * 4), -1); text3d("A: RACE", 1165 - text_width("A: RACE", 22) / 2, py + 57, 22, 0.35f, 0.2f, 0.05f, 1, 0); tzone(1080, py + 46, 170, 48, TZ_A, 0); }
    text_c("STICK: MOVE   A: RACE   X: CHARACTER   Y: JOURNEYS   +: WHOLE MAP   B: BACK", 694, 13, 0.9f, 0.92f, 1, 0.85f);
    if (g_save.story >= SM_NODES) { float pu = 0.85f + 0.15f * sinf(g_time * 2.5f); text3d("STORY COMPLETE - GLORY!", 640 - text_width("STORY COMPLETE - GLORY!", 30) / 2, 92, 30, pu, pu * 0.85f, 0.35f, st, 1); }
    if (g_sm_cel > 0) { float a = clampf(g_sm_cel / 0.5f, 0, 1) * clampf((4.5f - g_sm_cel) / 0.3f, 0, 1); text3d("STOP CLEARED!", 640 - text_width("STOP CLEARED!", 50) / 2, 250, 50, 1, 0.9f, 0.35f, a, 1);
        if (g_save.story < SM_NODES && g_sm_cel < 2.4f) { snprintf(buf, sizeof buf, "NEXT: %s", SM_NODE[g_save.story].name); text3d(buf, 640 - text_width(buf, 26) / 2, 318, 26, 1, 1, 1, a, 0); } } }
#endif
