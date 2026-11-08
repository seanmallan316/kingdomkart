// biblemap.h - the Bible map (Sean: "an ancient map of where each individual character lived and traveled ... animated and
// 3d. Start points and finish with lines drawn for where you've been"). Included by view.c (it uses view.c's helpers).
// One raised parchment map of the Bible lands from the Nile to the Tigris, rolled on two wooden rods: hills and mountains
// stand up, the seas and rivers shine, every town of the stories has a little 3D landmark. Pick a character and their
// journey draws itself across the map stop by stop, their own kart riding the line; or look at MY TRAVELS: every road
// you have raced, joined up in the order you first raced them, from your start pin to your latest.
#ifndef KK_BIBLEMAP_H
#define KK_BIBLEMAP_H

/* ---- where things are (latitude, longitude) ---- */
#define BM_LAT0 27.5f
#define BM_LAT1 40.5f
#define BM_LON0 29.0f
#define BM_LON1 49.5f
#define BM_K 40.0f   /* map units per degree */
static Vec3 bm_pos(float lat, float lon) { return v3((lon - BM_LON0) * BM_K, 0, (BM_LAT0 + BM_LAT1) * 0.5f * BM_K - lat * BM_K); }

enum { MK_NONE, MK_PYRAMID, MK_ZIGG, MK_GATE, MK_TEMPLE, MK_TOWN, MK_ARK, MK_BOAT, MK_PALM, MK_LAMASSU, MK_STATUE, MK_TENT, MK_SPHINX, MK_OBELISK };
typedef struct { const char *name; float lat, lon; int mark, big; } BmPlace;
enum { P_UR, P_HARAN, P_BABYLON, P_DURA, P_NINEVEH, P_SHUSHAN, P_ARARAT, P_MESO, P_EDEN, P_MEMPHIS, P_GOSHEN, P_REDSEA, P_SINAI, P_MIDIAN, P_KADESH, P_NEBO,
       P_SHECHEM, P_BETHEL, P_JERUSALEM, P_BETHLEHEM, P_EINKAREM, P_HEBRON, P_BEERSHEBA, P_GERAR, P_GATH, P_ELAH, P_ZORAH, P_TIMNAH, P_ASHKELON, P_GAZA,
       P_JOPPA, P_CAESAREA, P_NAZARETH, P_GATHHEPHER, P_CAPERNAUM, P_BETHSAIDA, P_GALILEE, P_JERICHO, P_JORDAN, P_MACHAERUS, P_WILDERNESS, P_ENGEDI,
       P_PENIEL, P_DOTHAN, P_OPHRAH, P_HAROD, P_MOREH, P_SYCAMORE, P_GREATSEA, P_GIZA, P_NPLACE };
static const BmPlace BM_PLACES[P_NPLACE] = {
    {"UR", 30.96f, 46.10f, MK_ZIGG, 1}, {"HARAN", 36.87f, 39.03f, MK_TOWN, 1}, {"BABYLON", 32.54f, 44.42f, MK_GATE, 1}, {"DURA", 32.25f, 44.20f, MK_STATUE, 0},
    {"NINEVEH", 36.36f, 43.15f, MK_LAMASSU, 1}, {"SHUSHAN", 32.19f, 48.25f, MK_TEMPLE, 1}, {"ARARAT", 39.70f, 44.30f, MK_ARK, 1}, {"MESOPOTAMIA", 33.4f, 43.0f, MK_TOWN, 0},
    {"EDEN", 31.0f, 47.4f, MK_PALM, 1}, {"EGYPT", 29.85f, 31.25f, MK_PYRAMID, 1}, {"GOSHEN", 30.80f, 31.85f, MK_TOWN, 0}, {"RED SEA CROSSING", 29.95f, 32.55f, MK_NONE, 0},
    {"MOUNT SINAI", 28.54f, 33.97f, MK_NONE, 1}, {"MIDIAN", 28.30f, 35.10f, MK_TENT, 0}, {"KADESH", 30.68f, 34.49f, MK_TENT, 0}, {"MOUNT NEBO", 31.77f, 35.73f, MK_NONE, 0},
    {"SHECHEM", 32.21f, 35.28f, MK_TOWN, 0}, {"BETHEL", 31.93f, 35.22f, MK_TOWN, 0}, {"JERUSALEM", 31.78f, 35.23f, MK_TEMPLE, 1}, {"BETHLEHEM", 31.70f, 35.20f, MK_TOWN, 0},
    {"EIN KAREM", 31.77f, 35.16f, MK_TOWN, 0}, {"HEBRON", 31.53f, 35.09f, MK_TENT, 0}, {"BEERSHEBA", 31.25f, 34.79f, MK_TENT, 0}, {"GERAR", 31.38f, 34.60f, MK_TOWN, 0},
    {"GATH", 31.70f, 34.85f, MK_TOWN, 0}, {"VALLEY OF ELAH", 31.69f, 34.96f, MK_NONE, 0}, {"ZORAH", 31.77f, 34.99f, MK_TOWN, 0}, {"TIMNAH", 31.76f, 34.91f, MK_TOWN, 0},
    {"ASHKELON", 31.67f, 34.57f, MK_TOWN, 0}, {"GAZA", 31.50f, 34.47f, MK_TOWN, 0}, {"JOPPA", 32.05f, 34.75f, MK_BOAT, 0}, {"CAESAREA", 32.50f, 34.89f, MK_TOWN, 0},
    {"NAZARETH", 32.70f, 35.30f, MK_TOWN, 0}, {"GATH-HEPHER", 32.74f, 35.33f, MK_TOWN, 0}, {"CAPERNAUM", 32.88f, 35.57f, MK_TOWN, 0}, {"BETHSAIDA", 32.91f, 35.63f, MK_TOWN, 0},
    {"SEA OF GALILEE", 32.82f, 35.59f, MK_BOAT, 1}, {"JERICHO", 31.87f, 35.44f, MK_PALM, 0}, {"JORDAN RIVER", 31.84f, 35.55f, MK_NONE, 0}, {"MACHAERUS", 31.56f, 35.63f, MK_TOWN, 0},
    {"WILDERNESS OF JUDEA", 31.60f, 35.30f, MK_NONE, 0}, {"EN-GEDI", 31.46f, 35.39f, MK_PALM, 0}, {"PENIEL", 32.18f, 35.62f, MK_NONE, 0}, {"DOTHAN", 32.42f, 35.24f, MK_TOWN, 0},
    {"OPHRAH", 32.50f, 35.20f, MK_TOWN, 0}, {"WELL OF HAROD", 32.55f, 35.36f, MK_NONE, 0}, {"HILL OF MOREH", 32.62f, 35.35f, MK_TENT, 0}, {"SYCAMORE TREE", 31.86f, 35.47f, MK_PALM, 0},
    {"THE GREAT SEA", 32.3f, 33.6f, MK_NONE, 0}, {"GIZA", 29.98f, 31.13f, MK_SPHINX, 0},
};
/* each character's journey: the places in order, with a line for each (the Bible verse) */
typedef struct { int place; const char *line; } BmStop;
typedef struct { int n; BmStop s[8]; } BmTrip;
static const BmTrip BM_TRIPS[] = {
    /* JESUS */ {7, {{P_BETHLEHEM, "BORN IN BETHLEHEM - LUKE 2:7"}, {P_MEMPHIS, "CARRIED INTO EGYPT - MATTHEW 2:14"}, {P_NAZARETH, "GREW UP IN NAZARETH - LUKE 2:51"}, {P_JORDAN, "BAPTIZED IN THE JORDAN - MARK 1:9"},
        {P_CAPERNAUM, "TAUGHT BY THE SEA OF GALILEE - MATTHEW 4:13"}, {P_JERICHO, "CALLED ZACCHAEUS DOWN - LUKE 19:5"}, {P_JERUSALEM, "DIED AND ROSE AGAIN - LUKE 24:6"}}},
    /* MOSES */ {8, {{P_GOSHEN, "BORN IN EGYPT - EXODUS 2:2"}, {P_MIDIAN, "FLED TO MIDIAN - EXODUS 2:15"}, {P_SINAI, "THE BURNING BUSH - EXODUS 3:2"}, {P_MEMPHIS, "LET MY PEOPLE GO - EXODUS 5:1"},
        {P_REDSEA, "THROUGH THE RED SEA - EXODUS 14:22"}, {P_SINAI, "THE TEN COMMANDMENTS - EXODUS 20:1"}, {P_KADESH, "FORTY YEARS IN THE WILDERNESS - NUMBERS 14:33"}, {P_NEBO, "SAW THE PROMISED LAND - DEUTERONOMY 34:1"}}},
    /* DAVID */ {5, {{P_BETHLEHEM, "A SHEPHERD BOY - 1 SAMUEL 16:11"}, {P_ELAH, "DEFEATED GOLIATH - 1 SAMUEL 17:50"}, {P_ENGEDI, "SPARED KING SAUL - 1 SAMUEL 24:4"},
        {P_HEBRON, "CROWNED KING IN HEBRON - 2 SAMUEL 2:4"}, {P_JERUSALEM, "TOOK THE CITY OF DAVID - 2 SAMUEL 5:7"}}},
    /* ESTHER */ {3, {{P_JERUSALEM, "HER PEOPLE CARRIED AWAY - ESTHER 2:6"}, {P_BABYLON, "IN EXILE - ESTHER 2:6"}, {P_SHUSHAN, "MADE QUEEN IN SHUSHAN - ESTHER 2:17"}}},
    /* PETER */ {6, {{P_BETHSAIDA, "FROM BETHSAIDA - JOHN 1:44"}, {P_CAPERNAUM, "FISHERS OF MEN - MATTHEW 4:19"}, {P_GALILEE, "WALKED ON THE WATER - MATTHEW 14:29"},
        {P_JERUSALEM, "PREACHED AT PENTECOST - ACTS 2:14"}, {P_JOPPA, "RAISED TABITHA - ACTS 9:40"}, {P_CAESAREA, "THE GOSPEL TO CORNELIUS - ACTS 10:34"}}},
    /* MARY */ {6, {{P_NAZARETH, "GABRIEL CAME TO HER - LUKE 1:26"}, {P_EINKAREM, "VISITED ELISABETH - LUKE 1:40"}, {P_BETHLEHEM, "GAVE BIRTH TO JESUS - LUKE 2:7"},
        {P_MEMPHIS, "FLED INTO EGYPT - MATTHEW 2:14"}, {P_NAZARETH, "HOME TO NAZARETH - MATTHEW 2:23"}, {P_JERUSALEM, "BY THE CROSS - JOHN 19:25"}}},
    /* DANIEL */ {4, {{P_JERUSALEM, "TAKEN CAPTIVE - DANIEL 1:1"}, {P_BABYLON, "SERVED THE KINGS OF BABYLON - DANIEL 2:48"}, {P_DURA, "HIS FRIENDS IN THE FURNACE - DANIEL 3:1"}, {P_SHUSHAN, "A VISION BY THE ULAI - DANIEL 8:2"}}},
    /* SAMSON */ {5, {{P_ZORAH, "BORN IN ZORAH - JUDGES 13:2"}, {P_TIMNAH, "TORE A LION APART - JUDGES 14:6"}, {P_ASHKELON, "WENT DOWN TO ASHKELON - JUDGES 14:19"},
        {P_GAZA, "CARRIED OFF THE GATES OF GAZA - JUDGES 16:3"}, {P_GAZA, "BROUGHT DOWN THE TEMPLE - JUDGES 16:30"}}},
    /* NOAH */ {2, {{P_MESO, "BUILT THE ARK - GENESIS 6:14"}, {P_ARARAT, "THE ARK RESTED ON ARARAT - GENESIS 8:4"}}},
    /* JOSEPH */ {5, {{P_HEBRON, "THE COAT OF MANY COLOURS - GENESIS 37:3"}, {P_SHECHEM, "SENT TO FIND HIS BROTHERS - GENESIS 37:14"}, {P_DOTHAN, "SOLD AS A SLAVE - GENESIS 37:28"},
        {P_MEMPHIS, "RULER OVER EGYPT - GENESIS 41:41"}, {P_GOSHEN, "HIS FAMILY TO GOSHEN - GENESIS 47:6"}}},
    /* JONAH */ {4, {{P_GATHHEPHER, "FROM GATH-HEPHER - 2 KINGS 14:25"}, {P_JOPPA, "RAN AWAY TO JOPPA - JONAH 1:3"}, {P_GREATSEA, "SWALLOWED BY A GREAT FISH - JONAH 1:17"},
        {P_NINEVEH, "PREACHED IN NINEVEH - JONAH 3:4"}}},
    /* JOHN */ {4, {{P_EINKAREM, "BORN TO ELISABETH - LUKE 1:57"}, {P_WILDERNESS, "PREACHED IN THE WILDERNESS - MATTHEW 3:1"}, {P_JORDAN, "BAPTIZED JESUS - MATTHEW 3:16"},
        {P_MACHAERUS, "PUT IN PRISON BY HEROD - MATTHEW 14:3"}}},
    /* MICHAEL */ {2, {{P_SHUSHAN, "STOOD AGAINST THE PRINCE OF PERSIA - DANIEL 10:13"}, {P_NEBO, "CONTENDED FOR THE BODY OF MOSES - JUDE 1:9"}}},
    /* GABRIEL */ {3, {{P_SHUSHAN, "EXPLAINED DANIEL'S VISION - DANIEL 8:16"}, {P_JERUSALEM, "SPOKE TO ZACHARIAS - LUKE 1:19"}, {P_NAZARETH, "GOOD NEWS FOR MARY - LUKE 1:26"}}},
    /* GOLIATH */ {2, {{P_GATH, "THE CHAMPION OF GATH - 1 SAMUEL 17:4"}, {P_ELAH, "FELL TO DAVID'S STONE - 1 SAMUEL 17:49"}}},
    /* GIDEON */ {3, {{P_OPHRAH, "THE ANGEL CAME TO OPHRAH - JUDGES 6:11"}, {P_HAROD, "300 MEN AT THE WELL - JUDGES 7:1"}, {P_MOREH, "VICTORY OVER MIDIAN - JUDGES 7:21"}}},
    /* ZACCHAEUS */ {3, {{P_JERICHO, "CHIEF OF THE TAX COLLECTORS - LUKE 19:2"}, {P_SYCAMORE, "CLIMBED THE SYCAMORE TREE - LUKE 19:4"}, {P_JERICHO, "SALVATION CAME TO HIS HOUSE - LUKE 19:9"}}},
    /* JACOB */ {6, {{P_BEERSHEBA, "GREW UP AT BEERSHEBA - GENESIS 26:23"}, {P_BETHEL, "THE LADDER TO HEAVEN - GENESIS 28:12"}, {P_HARAN, "SERVED LABAN 20 YEARS - GENESIS 31:41"},
        {P_PENIEL, "WRESTLED TILL DAYBREAK - GENESIS 32:24"}, {P_SHECHEM, "CAME TO SHECHEM - GENESIS 33:18"}, {P_GOSHEN, "WENT DOWN INTO EGYPT - GENESIS 46:6"}}},
    /* ISAAC */ {5, {{P_BEERSHEBA, "BORN TO ABRAHAM AND SARAH - GENESIS 21:2"}, {P_JERUSALEM, "GOD PROVIDED A RAM ON MORIAH - GENESIS 22:13"}, {P_GERAR, "LIVED IN GERAR - GENESIS 26:6"},
        {P_BEERSHEBA, "THE LORD APPEARED TO HIM - GENESIS 26:24"}, {P_HEBRON, "BURIED AT MAMRE - GENESIS 35:27"}}},
    /* ABRAHAM */ {7, {{P_UR, "CALLED OUT OF UR - GENESIS 11:31"}, {P_HARAN, "LEFT HARAN - GENESIS 12:4"}, {P_SHECHEM, "THE PROMISE AT SHECHEM - GENESIS 12:7"}, {P_BETHEL, "AN ALTAR AT BETHEL - GENESIS 12:8"},
        {P_MEMPHIS, "WENT DOWN INTO EGYPT - GENESIS 12:10"}, {P_HEBRON, "THE OAKS OF MAMRE - GENESIS 13:18"}, {P_JERUSALEM, "GOD WILL PROVIDE ON MORIAH - GENESIS 22:14"}}},
    /* PHARAOH */ {3, {{P_MEMPHIS, "KING OF EGYPT - EXODUS 1:8"}, {P_GOSHEN, "HARDENED HIS HEART - EXODUS 7:13"}, {P_REDSEA, "CHASED ISRAEL TO THE SEA - EXODUS 14:23"}}},
    /* NEBUCHADNEZZAR */ {4, {{P_BABYLON, "KING OF BABYLON - DANIEL 1:1"}, {P_JERUSALEM, "BESIEGED JERUSALEM - 2 KINGS 25:1"}, {P_DURA, "THE GOLDEN IMAGE - DANIEL 3:1"},
        {P_BABYLON, "PRAISED THE KING OF HEAVEN - DANIEL 4:37"}}},
};
typedef char bm_one_trip_each[(sizeof BM_TRIPS / sizeof BM_TRIPS[0]) == NCHARS ? 1 : -1];
/* where each road is (TRACK_DEFS order): for MY TRAVELS */
static const int BM_ROAD_PLACE[] = {P_BETHLEHEM, P_GALILEE, P_JERUSALEM, P_JERUSALEM, P_JERUSALEM, P_EDEN, P_MEMPHIS, P_GIZA, P_GOSHEN, P_REDSEA, P_BABYLON, P_BABYLON, P_DURA, P_BABYLON, P_JERICHO, P_ELAH};

/* ---- the land ---- */
static float bm_seg_dist(float la, float lo, float a0, float o0, float a1, float o1) {   /* degrees, roughly */
    float dx = o1 - o0, dy = a1 - a0, L = dx * dx + dy * dy, u = L > 0 ? clampf(((lo - o0) * dx + (la - a0) * dy) / L, 0, 1) : 0, px = o0 + dx * u - lo, py = a0 + dy * u - la; return sqrtf(px * px + py * py); }
static float bm_interp(const float (*t)[2], int n, float x) { if (x <= t[0][0]) return t[0][1]; for (int i = 0; i + 1 < n; i++) if (x <= t[i + 1][0]) { float u = (x - t[i][0]) / (t[i + 1][0] - t[i][0]); return t[i][1] + (t[i + 1][1] - t[i][1]) * u; } return t[n - 1][1]; }
static const float BM_NILE[][2] = {{27.5f, 30.85f}, {29.0f, 31.2f}, {30.1f, 31.25f}, {30.6f, 31.1f}, {31.45f, 30.4f}};
static const float BM_NILE2[][2] = {{30.1f, 31.25f}, {30.8f, 31.6f}, {31.45f, 31.9f}};
static const float BM_EUPH[][2] = {{39.0f, 39.5f}, {38.3f, 38.6f}, {36.9f, 38.1f}, {35.95f, 39.0f}, {34.4f, 41.0f}, {33.4f, 43.2f}, {32.54f, 44.42f}, {31.3f, 45.6f}, {30.95f, 47.0f}, {30.0f, 48.4f}};
static const float BM_TIGR[][2] = {{38.0f, 40.2f}, {37.0f, 42.5f}, {36.36f, 43.15f}, {35.0f, 43.5f}, {33.3f, 44.4f}, {32.5f, 45.8f}, {31.0f, 47.3f}};
static const float BM_JORDAN[][2] = {{33.25f, 35.62f}, {32.92f, 35.6f}, {32.7f, 35.58f}, {32.2f, 35.56f}, {31.78f, 35.55f}};
static const float BM_SUEZ[][2] = {{30.0f, 32.55f}, {29.2f, 32.8f}, {28.4f, 33.3f}, {27.5f, 33.9f}};
static const float BM_AQABA[][2] = {{29.55f, 34.97f}, {28.8f, 34.75f}, {28.0f, 34.45f}, {27.5f, 34.35f}};
static float bm_line_dist(const float (*p)[2], int n, float la, float lo) { float d = 1e9f; for (int i = 0; i + 1 < n; i++) d = fminf(d, bm_seg_dist(la, lo, p[i][0], p[i][1], p[i + 1][0], p[i + 1][1])); return d; }
#define BMN(a) (int)(sizeof a / sizeof a[0])
static int bm_is_sea(float la, float lo) {
    static const float LEV[][2] = {{31.2f, 34.25f}, {32.0f, 34.72f}, {32.8f, 34.95f}, {33.5f, 35.15f}, {34.5f, 35.85f}, {35.5f, 35.9f}, {36.6f, 35.9f}};
    static const float DEL[][2] = {{29.0f, 31.0f}, {30.4f, 31.45f}, {31.9f, 31.55f}, {32.4f, 31.2f}, {34.2f, 31.2f}};
    static const float TUR[][2] = {{29.0f, 36.6f}, {32.0f, 36.6f}, {34.0f, 36.15f}, {35.9f, 36.7f}};
    if (lo < bm_interp(LEV, BMN(LEV), la) && la > bm_interp(DEL, BMN(DEL), lo) && la < bm_interp(TUR, BMN(TUR), lo)) return 1;   /* the Great Sea */
    if (bm_line_dist(BM_SUEZ, BMN(BM_SUEZ), la, lo) < 0.18f + (30.0f - la) * 0.05f) return 1;   /* the Red Sea's two arms */
    if (bm_line_dist(BM_AQABA, BMN(BM_AQABA), la, lo) < 0.14f) return 1;
    if (la < 28.0f && lo > 33.6f && lo < 34.5f && la < 27.6f + (lo - 33.6f) * 0.0f) return 1;
    if (la < 30.05f - (lo - 47.6f) * 0.25f && lo > 47.6f) return 1;   /* the Persian Gulf */
    { float dx = (lo - 35.59f) / 0.07f, dy = (la - 32.82f) / 0.11f; if (dx * dx + dy * dy < 1) return 2; }   /* the Sea of Galilee */
    { float dx = (lo - 35.48f) / 0.07f, dy = (la - 31.5f) / 0.36f; if (dx * dx + dy * dy < 1) return 2; }   /* the Salt Sea */
    return 0; }
static float bm_height(float la, float lo, float *green) {   /* map units above the sea */
    float h = 1.2f + 0.6f * smooth_noise(lo * 3.1f, la * 3.1f) + 0.3f * smooth_noise(lo * 9.0f, la * 9.0f);
    float mtn = 0;
    if (la > 37.0f) mtn += clampf((la - 37.0f) * 2.2f, 0, 5) * (0.6f + 0.6f * smooth_noise(lo * 2, la * 2));                 /* the mountains of the north */
    mtn += 10.0f * expf(-((la - 39.7f) * (la - 39.7f) + (lo - 44.3f) * (lo - 44.3f)) * 6.0f);                                  /* Ararat */
    { float d = bm_seg_dist(la, lo, 37.2f, 44.6f, 30.2f, 50.5f); mtn += clampf(1.0f - d / 1.5f, 0, 1) * 5.0f * (0.7f + 0.5f * smooth_noise(lo * 2.5f, la * 2.5f)); }   /* the Zagros */
    mtn += 5.5f * expf(-((la - 28.55f) * (la - 28.55f) + (lo - 33.95f) * (lo - 33.95f)) * 3.0f);                               /* Sinai */
    { float d = bm_seg_dist(la, lo, 33.3f, 35.7f, 34.6f, 36.4f); mtn += clampf(1.0f - d / 0.5f, 0, 1) * 4.0f; }                   /* Lebanon */
    { float d = bm_seg_dist(la, lo, 31.2f, 35.1f, 32.6f, 35.25f); mtn += clampf(1.0f - d / 0.3f, 0, 1) * 1.6f; }                  /* the hill country of Judah and Ephraim */
    { float d = bm_seg_dist(la, lo, 31.2f, 35.75f, 32.6f, 35.8f); mtn += clampf(1.0f - d / 0.25f, 0, 1) * 1.8f; }                  /* the hills beyond Jordan (Nebo) */
    h += mtn;
    float rv = fminf(fminf(bm_line_dist(BM_NILE, BMN(BM_NILE), la, lo), bm_line_dist(BM_NILE2, BMN(BM_NILE2), la, lo)), fminf(bm_line_dist(BM_EUPH, BMN(BM_EUPH), la, lo), bm_line_dist(BM_TIGR, BMN(BM_TIGR), la, lo)));
    rv = fminf(rv, bm_line_dist(BM_JORDAN, BMN(BM_JORDAN), la, lo));
    float g = clampf(1.0f - rv / 0.55f, 0, 1);                                                                                       /* green along the rivers */
    if (la > 31.3f && la < 33.6f && lo > 34.4f && lo < 36.0f) g = fmaxf(g, 0.75f);                                                   /* the land of Canaan */
    if (la > 34.2f && la < 37.5f && lo > 36.0f && lo < 44.0f) g = fmaxf(g, 0.45f * smooth_noise(lo, la) + 0.25f);                    /* the fertile crescent */
    if (la > 30.4f && la < 31.5f && lo > 30.2f && lo < 32.3f) g = fmaxf(g, 0.9f);                                                    /* the delta */
    if (green) *green = g * (mtn > 3.5f ? 0.3f : 1.0f);
    return h - rv * 0.0f; }

/* ---- the drawing ---- */
static Mesh *m_bm_land, *m_bm_sea, *m_bm_rivers, *m_bm_rods;
static void bm_build(void) {
    static Vertex *bv; if (!bv) bv = malloc(sizeof(Vertex) * 90000); Batch b; batch_init(&b, bv, 90000); batch_clear(&b);
    const int NX = 150, NZ = 96; float W = (BM_LON1 - BM_LON0) * BM_K, Hh = (BM_LAT1 - BM_LAT0) * BM_K;
    static float hh[97][151]; static float cr[97][151][3];
    for (int j = 0; j <= NZ; j++) for (int i = 0; i <= NX; i++) {
        float lo = BM_LON0 + (BM_LON1 - BM_LON0) * i / NX, la = BM_LAT1 - (BM_LAT1 - BM_LAT0) * j / NZ, g; int sea = bm_is_sea(la, lo);
        float h = bm_height(la, lo, &g); if (sea) h = -1.2f;
        float sand[3] = {0.86f, 0.72f, 0.48f}, grn[3] = {0.48f, 0.6f, 0.3f}, rock[3] = {0.6f, 0.5f, 0.4f}, snow[3] = {0.95f, 0.94f, 0.9f}, c[3];
        float rk = clampf((h - 3.0f) / 3.0f, 0, 1), sn = clampf((h - 8.0f) / 2.0f, 0, 1);
        for (int k = 0; k < 3; k++) { c[k] = sand[k] * (1 - g) + grn[k] * g; c[k] = c[k] * (1 - rk) + rock[k] * rk; c[k] = c[k] * (1 - sn) + snow[k] * sn; }
        float blot = 0.88f + 0.16f * smooth_noise(lo * 5.3f + 7, la * 5.3f + 3), edge = fminf(fminf((float)i / NX, 1 - (float)i / NX), fminf((float)j / NZ, 1 - (float)j / NZ)), burn = clampf(edge * 14.0f, 0, 1);   /* old parchment: blotches, browned edges */
        for (int k = 0; k < 3; k++) { float parch = k == 0 ? 0.92f : k == 1 ? 0.83f : 0.62f; c[k] = (c[k] * 0.72f + parch * 0.28f) * blot * (0.55f + 0.45f * burn); }
        hh[j][i] = h; cr[j][i][0] = c[0]; cr[j][i][1] = c[1]; cr[j][i][2] = c[2]; }
    for (int j = 0; j < NZ; j++) for (int i = 0; i < NX; i++) {
        int ci[4][2] = {{i, j}, {i, j + 1}, {i + 1, j + 1}, {i + 1, j}}; Vec3 p[4]; Vec3 n[4];
        for (int c = 0; c < 4; c++) { int x = ci[c][0], z = ci[c][1]; p[c] = v3(W * x / NX, hh[z][x], -Hh * 0.5f + Hh * z / NZ);
            int xl = x > 0 ? x - 1 : x, xr = x < NX ? x + 1 : x, zu = z > 0 ? z - 1 : z, zd = z < NZ ? z + 1 : z;
            n[c] = v3_norm(v3((hh[z][xl] - hh[z][xr]) * NX / W * 0.5f, 1.0f, (hh[zu][x] - hh[zd][x]) * NZ / Hh * 0.5f)); }
        int q[6] = {0, 1, 2, 0, 2, 3};
        for (int k = 0; k < 6 && b.n < b.cap; k++) { int c = q[k], x = ci[c][0], z = ci[c][1]; Vertex v = {p[c].x, p[c].y, p[c].z, n[c].x, n[c].y, n[c].z, 0.5f, 0.5f, cr[z][x][0], cr[z][x][1], cr[z][x][2], 1}; b.v[b.n++] = v; } }
    gfx_mesh_free(m_bm_land); m_bm_land = gfx_mesh_static(b.v, b.n);
    /* the sea: one sheet just under the land's edge */
    batch_clear(&b); batch_quad4(&b, v3(0, 0.35f, -Hh * 0.5f), v3(0, 0.35f, Hh * 0.5f), v3(W, 0.35f, Hh * 0.5f), v3(W, 0.35f, -Hh * 0.5f), v3(0, 1, 0), 0.5f, 0.5f, 0.5f, 0.5f, 0.42f, 0.58f, 0.66f, 1);
    gfx_mesh_free(m_bm_sea); m_bm_sea = gfx_mesh_static(b.v, b.n);
    /* the rivers: blue ribbons lying on the land */
    batch_clear(&b);
    const float (*rv[5])[2] = {BM_NILE, BM_NILE2, BM_EUPH, BM_TIGR, BM_JORDAN}; int rn[5] = {BMN(BM_NILE), BMN(BM_NILE2), BMN(BM_EUPH), BMN(BM_TIGR), BMN(BM_JORDAN)};
    for (int r = 0; r < 5; r++) for (int i = 0; i + 1 < rn[r]; i++) for (int s = 0; s < 8; s++) {
        float u0 = s / 8.0f, u1 = (s + 1) / 8.0f, la0 = rv[r][i][0] + (rv[r][i + 1][0] - rv[r][i][0]) * u0, lo0 = rv[r][i][1] + (rv[r][i + 1][1] - rv[r][i][1]) * u0, la1 = rv[r][i][0] + (rv[r][i + 1][0] - rv[r][i][0]) * u1, lo1 = rv[r][i][1] + (rv[r][i + 1][1] - rv[r][i][1]) * u1;
        Vec3 a = bm_pos(la0, lo0), c = bm_pos(la1, lo1); a.y = bm_height(la0, lo0, NULL) + 0.25f; c.y = bm_height(la1, lo1, NULL) + 0.25f; if (bm_is_sea(la0, lo0)) a.y = 0.5f; if (bm_is_sea(la1, lo1)) c.y = 0.5f;
        Vec3 d = v3_norm(v3(c.x - a.x, 0, c.z - a.z)); float w = r == 4 ? 0.9f : 1.6f; Vec3 sd = v3(-d.z * w, 0, d.x * w);
        batch_quad4(&b, v3(a.x - sd.x, a.y, a.z - sd.z), v3(c.x - sd.x, c.y, c.z - sd.z), v3(c.x + sd.x, c.y, c.z + sd.z), v3(a.x + sd.x, a.y, a.z + sd.z), v3(0, 1, 0), 0.5f, 0.5f, 0.5f, 0.5f, 0.32f, 0.52f, 0.66f, 1); }
    gfx_mesh_free(m_bm_rivers); m_bm_rivers = gfx_mesh_static(b.v, b.n);
    /* the scroll's two wooden rods, with gold knobs, and the parchment's underside */
    batch_clear(&b);
    for (int sd = 0; sd < 2; sd++) { float x = sd ? W + 9 : -9; Vec3 a = v3(x, 1, -Hh * 0.5f - 22), c = v3(x, 1, Hh * 0.5f + 22);
        sc_tube(&b, a, c, 9, 9, 18, 0.45f, 0.28f, 0.14f, 1, 1); sc_ball(&b, v3(x, 1, -Hh * 0.5f - 26), 8, 14, 0.95f, 0.75f, 0.3f, 1); sc_ball(&b, v3(x, 1, Hh * 0.5f + 26), 8, 14, 0.95f, 0.75f, 0.3f, 1);
        batch_quad4(&b, v3(sd ? W : 0, -2, -Hh * 0.5f), v3(sd ? W : 0, -2, Hh * 0.5f), v3(x, 9.5f, Hh * 0.5f), v3(x, 9.5f, -Hh * 0.5f), v3(0, 1, 0), 0.5f, 0.5f, 0.5f, 0.5f, 0.72f, 0.6f, 0.42f, 1); }   /* the parchment rolling up onto the rod */
    batch_quad4(&b, v3(-30, -3, -Hh * 0.5f - 40), v3(-30, -3, Hh * 0.5f + 40), v3(W + 30, -3, Hh * 0.5f + 40), v3(W + 30, -3, -Hh * 0.5f - 40), v3(0, 1, 0), 0.5f, 0.5f, 0.5f, 0.5f, 0.26f, 0.16f, 0.1f, 1);   /* the table */
    gfx_mesh_free(m_bm_rods); m_bm_rods = gfx_mesh_static(b.v, b.n); }
static Vec3 bm_place_pos(int p) { const BmPlace *q = &BM_PLACES[p]; Vec3 v = bm_pos(q->lat, q->lon); v.y = bm_is_sea(q->lat, q->lon) ? 0.5f : bm_height(q->lat, q->lon, NULL); return v; }

/* the screen */
static int g_bm_char, g_bm_travels; static float g_bm_t; static Vec3 g_bm_eye, g_bm_at; static int g_bm_built;
static float g_bm_lab[P_NPLACE][3];   /* where each place's name goes on the screen (x, y, visible) */
#define BM_SEG_T 1.6f
#define BM_STOP_T 2.4f
static int bm_trip_n(void) { if (g_bm_travels) return g_save.ntrail; return BM_TRIPS[g_bm_char].n; }
static int bm_trip_place(int i) { if (g_bm_travels) { int r = g_save.trail[i]; return r >= 0 && r < (int)(sizeof BM_ROAD_PLACE / sizeof BM_ROAD_PLACE[0]) ? BM_ROAD_PLACE[r] : P_JERUSALEM; } return BM_TRIPS[g_bm_char].s[i].place; }
static float bm_progress(void) { int n = bm_trip_n(); if (n <= 1) return n; float per = BM_SEG_T + BM_STOP_T, t = g_bm_t - 1.2f; if (t < 0) return 0; return fminf((t + BM_SEG_T) / per, (float)n); }   /* 1.0 = at the first stop; n = done */
static void bm_open(void) { g_bm_t = 0; sc_load_props(); if (!g_bm_built) { bm_build(); g_bm_built = 1; } }
static void bm_view(Vec3 *eye, Vec3 *at) {   /* frame the whole journey */
    int n = bm_trip_n(); float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
    for (int i = 0; i < n; i++) { Vec3 p = bm_place_pos(bm_trip_place(i)); x0 = fminf(x0, p.x); x1 = fmaxf(x1, p.x); z0 = fminf(z0, p.z); z1 = fmaxf(z1, p.z); }
    if (n == 0) { x0 = 0; x1 = (BM_LON1 - BM_LON0) * BM_K; z0 = -(BM_LAT1 - BM_LAT0) * BM_K * 0.5f; z1 = -z0; }
    float ext = fmaxf(fmaxf(x1 - x0, (z1 - z0) * 1.6f), 40.0f), d = ext * 1.05f + 25.0f; *at = v3((x0 + x1) * 0.5f, 0, (z0 + z1) * 0.5f + d * 0.05f);
    *eye = v3(at->x, d * 0.95f, at->z + d * 0.62f); }
static void bm_update(float dt, int pa, int pb, int mx, int pyy) {
    g_bm_t += dt; Vec3 e, a; bm_view(&e, &a); float f = fminf(1.0f, dt * 1.6f); e.x += sinf(g_time * 0.15f) * 6; g_bm_eye = sc_lerp(g_bm_eye, e, f); g_bm_at = sc_lerp(g_bm_at, a, f);
    if (mx && !g_bm_travels) { for (int k = 0; k < NCHARS; k++) { g_bm_char = (g_bm_char + mx + NCHARS) % NCHARS; if (char_unlocked(g_bm_char)) break; } g_bm_t = 0; sfx_play(SFX_MENU_MOVE, 1, 0); }
    if (pyy) { g_bm_travels = !g_bm_travels; g_bm_t = 0; sfx_play(SFX_MENU_OK, 1, 0); }
    if (pa) { g_bm_t = bm_progress() >= bm_trip_n() ? 0 : 1e4f; sfx_play(SFX_MENU_OK, 0.6f, 0); }   /* A: skip to the end, or play it again */
    if (pb) { if (g_story) story_enter(0); else { g_screen = SCR_MAIN; g_screen_t = 0; } sfx_play(SFX_MENU_BACK, 1, 0); } }
static Vec3 bm_arc(Vec3 a, Vec3 c, float u) {   /* the line between two stops arcs up off the map */
    float d = sqrtf((c.x - a.x) * (c.x - a.x) + (c.z - a.z) * (c.z - a.z)), lift = fminf(d * 0.22f, 40.0f) + 0.8f;
    return v3(a.x + (c.x - a.x) * u, a.y + (c.y - a.y) * u + 1.2f + lift * 4.0f * u * (1 - u), a.z + (c.z - a.z) * u); }
static void draw_bible_map(int drc) {
    Mat4 id = m4_identity(); float t = g_time;
    if (!g_bm_built) bm_open();
    gfx_clear_depth(drc);
    float aspect = drc ? 854.0f / 480.0f : 1280.0f / 720.0f; Vec3 eye = g_bm_eye, at = g_bm_at; if (eye.y < 1) { bm_view(&eye, &at); g_bm_eye = eye; g_bm_at = at; }
    Mat4 vp = m4_mul(m4_perspective(0.7f, aspect, 1.0f, 3000.0f), m4_look_at(eye, at, v3(0, 1, 0)));
    { Mat4 lift = m4_identity(); lift.m[13] = 0.12f; vp = m4_mul(lift, vp); }
    gfx_light(v3(1.0f, 0.93f, 0.8f), v3(0.62f, 0.6f, 0.62f), v3(0.4f, 0.34f, 0.28f), eye);
    gfx_camera(&vp, v3_norm(v3(-0.4f, 0.8f, 0.45f)), 0.55f, 0.18f, 0.11f, 0.07f, 4000.0f, 8000.0f);
    gfx_state(ST_DEPTH | ST_DEPTH_WRITE); gfx_bind(NULL);
    gfx_draw_ex(m_bm_rods, &id, 1, 1, 1, 1, 0.3f, 0, 0, 0);
    gfx_draw_ex(m_bm_land, &id, 1, 1, 1, 1, 0.05f, 0, 0, 0);
    { float s = 0.95f + 0.05f * sinf(t * 1.3f); gfx_draw_ex(m_bm_sea, &id, s, s, s, 1, 1.2f, 0, 0, 0); }
    gfx_draw_ex(m_bm_rivers, &id, 1, 1, 1, 1, 1.0f, 0, 0, 0);
    /* the landmarks: each model scaled so its biggest side is the size wanted */
    for (int p = 0; p < P_NPLACE; p++) { const BmPlace *q = &BM_PLACES[p]; Vec3 c = bm_place_pos(p); const Kart3D *k = NULL; float want = 3.0f, yaw = 0.5f + p * 0.7f, r = 1, g = 1, bl = 1;
        switch (q->mark) { case MK_PYRAMID: k = &g_prop[PR_PYRAMID]; want = 7; break; case MK_ZIGG: k = road_prop("@ziggurat"); want = 8; break; case MK_GATE: k = road_prop("@ishtar_gate"); want = 8; break;
            case MK_LAMASSU: k = road_prop("@lamassu"); want = 6; break; case MK_STATUE: k = road_prop("@gold_statue"); want = 4.5f; break; case MK_SPHINX: k = road_prop("@sphinx"); want = 4.5f; yaw = 1.6f; break;
            case MK_OBELISK: k = road_prop("@obelisk"); want = 4; break; case MK_TEMPLE: k = &g_prop[PR_HOUSE]; want = 5; r = 1.15f; g = 1.1f; bl = 0.95f; break; case MK_TOWN: k = &g_prop[PR_HOUSE]; want = q->big ? 4 : 2.6f; break;
            case MK_ARK: k = &g_prop[PR_ARK]; want = 7; break; case MK_BOAT: k = &g_prop[PR_BOAT]; want = 3.5f; break; case MK_PALM: k = &g_prop[PR_PALM]; want = 3.2f; break; case MK_TENT: k = &g_prop[PR_SACKS]; want = 2.2f; break; }
        if (!k || !k->ok) continue; float ext = fmaxf(fmaxf(k->hi.x - k->lo.x, k->hi.y - k->lo.y), k->hi.z - k->lo.z); if (ext < 1e-3f) continue; float s = want / ext;
        Mat4 m = m4_mul(m4_translate(c.x, c.y - k->lo.y * s, c.z), m4_mul(m4_rotate_y(yaw), m4_scale(s, s, s))); gfx_state(ST_DEPTH | ST_DEPTH_WRITE); kart3d_draw(k, &m, 0, 0, r, g, bl, 1); }
    gfx_bind(NULL);
    /* the journey: arcs drawn up to where we have got, the kart riding the newest one, pins at the stops */
    int n = bm_trip_n(); float prog = bm_progress();
    static Mesh *m_line[2], *m_glow[2]; static Vertex lv[9000], gv[3000]; if (!m_line[drc]) { m_line[drc] = gfx_mesh_dynamic(9000); m_glow[drc] = gfx_mesh_dynamic(3000); }
    Batch lb; batch_init(&lb, lv, 9000); Batch gb; batch_init(&gb, gv, 3000);
    float lr = g_bm_travels ? 1.0f : 1.0f, lg = g_bm_travels ? 0.35f : 0.82f, lbb = g_bm_travels ? 0.25f : 0.3f;   /* gold for a character, red for your own travels */
    Vec3 R = v3_norm(v3_cross(v3(0, 1, 0), v3_norm(v3_sub(eye, at)))); Vec3 kpos = v3(0, 0, 0), kdir = v3(0, 0, 1); int krun = 0;
    for (int i = 0; i + 1 < n; i++) { float seg = prog - (i + 1); if (seg <= 0) break; float u = clampf(seg * (BM_SEG_T + BM_STOP_T) / BM_SEG_T, 0, 1);
        Vec3 a = bm_place_pos(bm_trip_place(i)), c = bm_place_pos(bm_trip_place(i + 1)); int K = 24; Vec3 prev = bm_arc(a, c, 0);
        for (int k = 1; k <= K; k++) { float uu = (float)k / K; if (uu > u) uu = u; Vec3 p = bm_arc(a, c, uu); sc_tube(&lb, prev, p, 0.45f, 0.45f, 5, lr, lg, lbb, 1, 1); prev = p; if (uu >= u) break; }
        if (u < 1) { kpos = prev; Vec3 nx = bm_arc(a, c, fminf(u + 0.02f, 1)); kdir = v3_norm(v3_sub(nx, prev)); krun = 1; } }
    if (!krun && n > 0) { int last = (int)fminf(floorf(prog), (float)n) - 1; if (last < 0) last = 0; kpos = bm_place_pos(bm_trip_place(last)); kpos.y += 1.2f; }
    for (int i = 0; i < n; i++) { if (prog < i + 1 - 0.01f) break; Vec3 c = bm_place_pos(bm_trip_place(i)); int first = i == 0, lastp = i == n - 1 && prog >= n;   /* the pins: green start, gold finish */
        float pr2 = first ? 0.3f : lastp ? 1.0f : 0.95f, pg = first ? 0.9f : lastp ? 0.82f : 0.95f, pb2 = first ? 0.35f : lastp ? 0.2f : 0.95f, bob = 0.4f * sinf(t * 3 + i);
        sc_tube(&lb, v3(c.x, c.y, c.z), v3(c.x, c.y + 6, c.z), 0.25f, 0.25f, 5, 0.85f, 0.85f, 0.85f, 1, 1); sc_ball(&lb, v3(c.x, c.y + 7 + bob, c.z), first || lastp ? 1.8f : 1.2f, 8, pr2, pg, pb2, 1);
        batch_quad(&gb, v3(c.x, c.y + 0.6f, c.z), v3(4, 0, 0), v3(0, 0, -4), v3(0, 1, 0), 0, 0, 1, 1, pr2, pg, pb2, 0.8f); }
    if (krun || n > 0) batch_quad(&gb, v3(kpos.x, kpos.y + 1, kpos.z), v3(R.x * 5, 0, R.z * 5), v3(0, 5, 0), v3(0, 0, 1), 0, 0, 1, 1, 1, 0.9f, 0.6f, 0.5f);
    if (lb.n) { gfx_mesh_update(m_line[drc], lb.v, lb.n); gfx_state(ST_DEPTH | ST_DEPTH_WRITE); gfx_bind(NULL); gfx_draw_ex(m_line[drc], &id, 1, 1, 1, 1, 0.8f, 0, 0, 0); }
    /* the character's own kart riding the line (in MY TRAVELS, the kart you drive most: the one you picked) */
    { int ch = g_bm_travels ? g_player_char : g_bm_char; if (ch >= 0 && ch < NCHARS && g_k3d[ch].ok && n > 0) { float yaw = atan2f(kdir.x, kdir.z), pitch = -asinf(clampf(kdir.y, -1, 1));
        Mat4 m = m4_mul(m4_translate(kpos.x, kpos.y + 0.3f, kpos.z), m4_mul(m4_rotate_y(yaw), m4_mul(m4_rotate_x(pitch), m4_scale(2.2f, 2.2f, 2.2f))));
        K3Pose P; memset(&P, 0, sizeof P); P.t = t; if (!krun) { P.arm[1] = K3A_WAVE; P.arm_w[1] = 1; P.look_yaw = 0.3f * sinf(t); }
        gfx_state(ST_DEPTH | ST_DEPTH_WRITE); kart3d_draw_pose(&g_k3d[ch], &m, krun ? t * 10 : 0, 0, &P, 1, 1, 1, 1); } }
    if (gb.n) { gfx_mesh_update(m_glow[drc], gb.v, gb.n); gfx_state(ST_TRANSPARENT | ST_ADD); gfx_bind(tx_blob); gfx_draw_ex(m_glow[drc], &id, 1, 1, 1, 1, 0, 1, 0, 0); gfx_bind(NULL); }
    /* where the names go */
    for (int p = 0; p < P_NPLACE; p++) { Vec3 c = bm_place_pos(p); c.y += 9; const float *M = vp.m; float cx = M[0] * c.x + M[4] * c.y + M[8] * c.z + M[12], cy = M[1] * c.x + M[5] * c.y + M[9] * c.z + M[13], cw = M[3] * c.x + M[7] * c.y + M[11] * c.z + M[15];
        g_bm_lab[p][2] = 0; if (cw < 0.5f) continue; float sx = (cx / cw * 0.5f + 0.5f) * 1280, sy = (1 - (cy / cw * 0.5f + 0.5f)) * 720; if (sx < 20 || sx > 1260 || sy < 70 || sy > 560) continue;
        g_bm_lab[p][0] = sx; g_bm_lab[p][1] = sy; g_bm_lab[p][2] = 1; }
    gfx_camera_2d(); gfx_state(ST_HUD); gfx_bind(NULL); }
static void hud_bible_map(void) {
    static const UiLook UI_MAP = {{0.36f, 0.24f, 0.14f}, {0.16f, 0.1f, 0.06f}, {0.95f, 0.76f, 0.32f}, {0.3f, 0.2f, 0.1f}};
    float st = clampf(g_screen_t / 0.5f, 0, 1); char buf[96]; int n = bm_trip_n(); float prog = bm_progress();
    ui_plaque(-30, -40 - (1 - st) * 70, 1340, 104, 16, 18, &UI_MAP, 0.96f, 0, -1);
    text3d("THE BIBLE MAP", 26, 14, 22, 1, 0.85f, 0.4f, st, 1);
    if (g_bm_travels) { snprintf(buf, sizeof buf, "MY TRAVELS - %d RACES - %d KM DRIVEN", g_save.ntrail, g_save.meters / 1000); }
    else { snprintf(buf, sizeof buf, "%s", CHARACTERS[g_bm_char].name); for (char *c = buf; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32; }
    { float sz = 30; while (sz > 16 && text_width(buf, sz) > 760) sz -= 1; text3d(buf, 640 - text_width(buf, sz) / 2, 16, sz, 1, 1, 1, st, 0); }
    /* the place names: the big cities always, every stop on the journey once it is reached */
    for (int p = 0; p < P_NPLACE; p++) { if (!g_bm_lab[p][2]) continue; int on = 0, num = 0;
        for (int i = 0; i < n; i++) if (bm_trip_place(i) == p && prog >= i + 1 - 0.01f) { on = 1; if (!num) num = i + 1; }
        if (!on && !BM_PLACES[p].big) continue; char nm[48]; if (on) snprintf(nm, sizeof nm, "%d. %s", num, BM_PLACES[p].name); else snprintf(nm, sizeof nm, "%s", BM_PLACES[p].name);
        float sz = on ? 15 : 12, x = g_bm_lab[p][0] - text_width(nm, sz) / 2, y = g_bm_lab[p][1] - sz - 4;
        batch_rect(&g_b, x - 5, y - 3, text_width(nm, sz) + 10, sz + 6, 0.12f, 0.08f, 0.05f, on ? 0.75f : 0.5f); batch_text(&g_b, nm, x, y, sz, on ? 1 : 0.95f, on ? 0.9f : 0.88f, on ? 0.6f : 0.75f, 1); }
    /* the panel at the bottom: the stop we have reached and its verse */
    float py = 720 - 128 * st; ui_plaque(14, py, 1252, 160, 22, 22, &UI_MAP, 0.95f, 0.2f, -1);
    if (n == 0) { text_c("RACE ON ANY ROAD AND IT APPEARS HERE ON THE MAP", py + 40, 18, 1, 0.92f, 0.7f, 1); }
    else { int cur = (int)fminf(floorf(prog), (float)n) - 1; if (cur < 0) cur = 0; int p = bm_trip_place(cur);
        const char *line = g_bm_travels ? TRACK_DEFS[g_save.trail[cur] >= 0 && g_save.trail[cur] < NUM_TRACK_DEFS ? g_save.trail[cur] : 0].name : BM_TRIPS[g_bm_char].s[cur].line;
        snprintf(buf, sizeof buf, "%d. %s", cur + 1, BM_PLACES[p].name); text3d(buf, 44, py + 16, 22, 1, 0.85f, 0.4f, 1, 1);
        char up[96]; snprintf(up, sizeof up, "%s", line); for (char *c = up; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
        { float sz = 22; while (sz > 12 && text_width(up, sz) > 1180) sz -= 1; text3d(up, 44, py + 52, sz, 1, 1, 1, 1, 0); }
        for (int i = 0; i < n && i < 24; i++) { float s = i == cur ? 8 : 6; ui_plaque(1240 - (n - i) * 22 - s, py + 26 - s, s * 2, s * 2, s * 0.9f, 5, i <= cur ? &UI_GOLD : &UI_MAP, 1, i == cur ? 0.5f : 0, -1); } }
    text_c(g_bm_travels ? "Y: CHARACTERS   A: REPLAY   B: BACK" : "LEFT/RIGHT: CHARACTER   Y: MY TRAVELS   A: REPLAY   B: BACK", 694, 15, 0.9f, 0.92f, 1, 0.8f);
    float fade = g_screen_t < 0.4f ? 1 - g_screen_t / 0.4f : 0; if (fade > 0) batch_rect(&g_b, 0, 0, 1280, 720, 0, 0, 0, fade); }
/* a race is over: it goes on the map */
static void bm_trail_add(int road, float meters) {
    if (road < 0 || road >= (int)(sizeof BM_ROAD_PLACE / sizeof BM_ROAD_PLACE[0])) return;
    g_save.meters += (int)fmaxf(meters, 0);
    for (int i = 0; i < g_save.ntrail; i++) if (g_save.trail[i] == road) { g_save.dirty = 1; return; }   /* only the first time each road is raced */
    if (g_save.ntrail < 24) g_save.trail[g_save.ntrail++] = road; g_save.dirty = 1; }
#endif
