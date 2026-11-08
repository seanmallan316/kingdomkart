// kkserver.c - the Kingdom Kart online server (Phase 1: the network test).
// One UDP socket. A console says HELLO and gets a random session number; then it PINGs once a second and the server
// answers with a PONG carrying how many consoles are online. Anything malformed is dropped without a reply; every
// internet address has a packet budget; consoles that go quiet for 30 s are forgotten.
// Later phases add lobbies, invitation codes and races here (the race itself will be the game's own src/sim code).
//   build: cc -O2 -Wall -Isrc server/kkserver.c -o kkserver
//   run:   KK_BIND=0.0.0.0 KK_PORT=7777 ./kkserver      (on Fly.io, UDP must bind to the address "fly-global-services")
#define _POSIX_C_SOURCE 200809L
#include "net/proto.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define SERVER_BUILD 1
#define MAX_CLIENTS 4096
#define IDLE_MS 30000
#define BUDGET_PER_SEC 40          /* packets a second from one address before it is told to slow down */

typedef struct { int used; struct sockaddr_in addr; uint8_t session[16], player[16]; uint64_t last_ms; } Client;
typedef struct { uint32_t ip; uint64_t window_ms; int count; } Budget;
static Client g_cl[MAX_CLIENTS]; static int g_ncl;
static Budget g_budget[1024];
static volatile sig_atomic_t g_stop;
static unsigned long g_rx, g_tx, g_dropped;

static uint64_t now_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; }
static void random_bytes(uint8_t *b, int n) {
    FILE *f = fopen("/dev/urandom", "rb"); size_t got = f ? fread(b, 1, n, f) : 0; if (f) fclose(f);
    if ((int)got != n) { fprintf(stderr, "no secure random source\n"); exit(1); } }
static const char *who(const struct sockaddr_in *a) { static char s[32]; inet_ntop(AF_INET, &a->sin_addr, s, sizeof s); return s; }   /* for the server's own log only: never sent to players */
static int same_addr(const struct sockaddr_in *a, const struct sockaddr_in *b) { return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port; }
static int online_count(void) { return g_ncl; }

/* a packet budget per internet address: protects the server from floods (and codes from guessing, later) */
static int over_budget(const struct sockaddr_in *a, uint64_t now) {
    uint32_t ip = a->sin_addr.s_addr; Budget *b = &g_budget[(ip ^ (ip >> 13)) % 1024];
    if (b->ip != ip || now - b->window_ms >= 1000) { b->ip = ip; b->window_ms = now; b->count = 0; }
    return ++b->count > BUDGET_PER_SEC ? (b->count == BUDGET_PER_SEC + 1 ? 1 : 2) : 0; }   /* 1 = just went over (told once), 2 = still over (ignored) */
static Client *find_client(const struct sockaddr_in *a) { for (int i = 0; i < MAX_CLIENTS; i++) if (g_cl[i].used && same_addr(&g_cl[i].addr, a)) return &g_cl[i]; return NULL; }
static Client *new_client(const struct sockaddr_in *a) {
    for (int i = 0; i < MAX_CLIENTS; i++) if (!g_cl[i].used) { memset(&g_cl[i], 0, sizeof g_cl[i]); g_cl[i].used = 1; g_cl[i].addr = *a; g_ncl++; return &g_cl[i]; }
    return NULL; }
static void send_to(int fd, const struct sockaddr_in *a, const uint8_t *p, int n) { if (sendto(fd, p, n, 0, (const struct sockaddr *)a, sizeof *a) == n) g_tx++; }
static void refuse(int fd, const struct sockaddr_in *a, int why) { uint8_t p[KKP_REFUSED_LEN]; kkp_hdr(p, KKP_REFUSED); p[4] = (uint8_t)why; send_to(fd, a, p, sizeof p); }

static void handle(int fd, const uint8_t *p, int n, const struct sockaddr_in *from, uint64_t now) {
    if (n >= KKP_HDR && p[0] == 'K' && p[1] == 'K' && p[2] != KKNET_VERSION) { refuse(fd, from, KKR_BAD_VERSION); return; }   /* an older or newer game */
    int t = kkp_type(p, n); if (!t) { g_dropped++; return; }
    Client *c = find_client(from);
    switch (t) {
    case KKP_HELLO: {
        if (!c) { c = new_client(from); if (!c) { refuse(fd, from, KKR_BUSY); return; } random_bytes(c->session, 16);
                  memcpy(c->player, p + 4, 16); printf("hello from %s (game build %u), %d online\n", who(from), kkp_get32(p + 20), g_ncl); }
        c->last_ms = now;
        uint8_t w[KKP_WELCOME_LEN]; memset(w, 0, sizeof w); kkp_hdr(w, KKP_WELCOME);
        memcpy(w + 4, p + 24, 4); memcpy(w + 8, c->session, 16); kkp_put16(w + 24, (uint16_t)online_count()); kkp_put32(w + 26, SERVER_BUILD);
        static const char motd[] = "Welcome to Kingdom Kart Online!"; memcpy(w + 30, motd, sizeof motd < 32 ? sizeof motd : 31);
        send_to(fd, from, w, sizeof w); } break;
    case KKP_PING: {
        if (!c || memcmp(c->session, p + 4, 16)) { refuse(fd, from, KKR_NO_SESSION); return; }   /* forgotten or forged: say hello again */
        c->last_ms = now;
        uint8_t o[KKP_PONG_LEN]; kkp_hdr(o, KKP_PONG); memcpy(o + 4, p + 20, 8); kkp_put16(o + 12, (uint16_t)online_count()); send_to(fd, from, o, sizeof o); } break;
    case KKP_BYE:
        if (c && !memcmp(c->session, p + 4, 16)) { c->used = 0; g_ncl--; printf("bye from %s, %d online\n", who(from), g_ncl); } break;
    default: g_dropped++; break;   /* server-to-console messages are not accepted from consoles */
    } }

static void on_signal(int s) { (void)s; g_stop = 1; }

int main(int argc, char **argv) {
    const char *bind_host = getenv("KK_BIND") ? getenv("KK_BIND") : "0.0.0.0"; int port = getenv("KK_PORT") ? atoi(getenv("KK_PORT")) : KKNET_PORT;
    double run_for = argc > 1 ? atof(argv[1]) : 0;   /* seconds, for tests; 0 = forever */
    signal(SIGINT, on_signal); signal(SIGTERM, on_signal); setvbuf(stdout, NULL, _IOLBF, 0);
    struct addrinfo hints, *res = NULL; memset(&hints, 0, sizeof hints); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM; hints.ai_flags = AI_PASSIVE;
    char ps[16]; snprintf(ps, sizeof ps, "%d", port);
    int e = getaddrinfo(bind_host, ps, &hints, &res);
    if (e || !res) { fprintf(stderr, "can't find %s (%s): listening on every address instead\n", bind_host, gai_strerror(e)); bind_host = "0.0.0.0"; e = getaddrinfo(bind_host, ps, &hints, &res); }
    if (e || !res) { fprintf(stderr, "can't listen: %s\n", gai_strerror(e)); return 1; }
    int fd = socket(AF_INET, SOCK_DGRAM, 0); if (fd < 0) { perror("socket"); return 1; }
    if (bind(fd, res->ai_addr, res->ai_addrlen) < 0) { perror("bind"); return 1; }
    printf("kkserver build %d listening on %s:%d (UDP)\n", SERVER_BUILD, bind_host, port); freeaddrinfo(res);
    uint64_t start = now_ms(), last_tidy = start, last_report = start;
    while (!g_stop) {
        uint64_t now = now_ms(); if (run_for > 0 && now - start > run_for * 1000) break;
        fd_set rs; FD_ZERO(&rs); FD_SET(fd, &rs); struct timeval tv = {0, 200000};
        if (select(fd + 1, &rs, NULL, NULL, &tv) > 0) {
            uint8_t buf[KKNET_MAX_PACKET + 1]; struct sockaddr_in from; socklen_t fl = sizeof from;
            int n = (int)recvfrom(fd, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl); now = now_ms();
            if (n > 0 && fl == sizeof from && from.sin_family == AF_INET) { g_rx++;
                int ob = over_budget(&from, now);
                if (ob) { g_dropped++; if (ob == 1 && kkp_type(buf, n)) refuse(fd, &from, KKR_SLOW_DOWN); }   /* one short note, never a flood back */
                else handle(fd, buf, n, &from, now); } }
        if (now - last_tidy > 1000) { last_tidy = now;
            for (int i = 0; i < MAX_CLIENTS; i++) if (g_cl[i].used && now - g_cl[i].last_ms > IDLE_MS) { g_cl[i].used = 0; g_ncl--; } }
        if (now - last_report > 60000) { last_report = now; printf("%d online, %lu in, %lu out, %lu dropped\n", g_ncl, g_rx, g_tx, g_dropped); }
    }
    printf("stopping: %lu in, %lu out, %lu dropped\n", g_rx, g_tx, g_dropped); close(fd); return 0; }
