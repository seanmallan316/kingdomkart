// net.c - see net.h. Phase 1: find the server, say hello, ping it once a second, report what happened.
#include "net.h"
#include "proto.h"
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef __WIIU__
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <malloc.h>
#else
#include <pthread.h>
#include <time.h>
#endif

#define GAME_BUILD 1
static NetStatus S;
static int g_fd = -1;
static struct sockaddr_in g_srv;
static uint8_t g_session[16], g_player[16];
static uint32_t g_nonce, g_seq, g_rtt_sum;
static float g_resend_t, g_ping_t, g_quiet_t;
static int g_hello_tries;
/* the lookup thread: writes g_found / g_found_addr, then sets g_lookup_done */
static volatile int g_lookup_busy, g_lookup_done, g_found;
static volatile uint32_t g_found_addr;
static char g_lookup_host[64];

static uint32_t now_ms(void) {
#ifdef __WIIU__
    return (uint32_t)OSTicksToMilliseconds(OSGetSystemTime());
#else
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}
static uint32_t mix32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static void make_player_id(void) {   /* Phase 1: new each time the game starts (Phase 2 keeps one on the SD card) */
    static int made; if (made) return; made = 1; uint32_t s = now_ms() ^ (uint32_t)(uintptr_t)&S;
    for (int i = 0; i < 16; i += 4) { s = mix32(s + 0x9e3779b9U + (uint32_t)i); kkp_put32(g_player + i, s); } }

static void lookup(void) {
    struct addrinfo hints, *res = NULL; memset(&hints, 0, sizeof hints); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
    int ok = 0; struct in_addr lit;
    if (inet_pton(AF_INET, g_lookup_host, &lit) == 1) { g_found_addr = lit.s_addr; g_found = 1; g_lookup_done = 1; g_lookup_busy = 0; return; }   /* already a number: nothing to look up */
    if (getaddrinfo(g_lookup_host, NULL, &hints, &res) == 0 && res) {
        for (struct addrinfo *a = res; a; a = a->ai_next) if (a->ai_family == AF_INET) { g_found_addr = ((struct sockaddr_in *)a->ai_addr)->sin_addr.s_addr; ok = 1; break; }
        freeaddrinfo(res); }
    /* (no gethostbyname fallback: the emulator crashes on it, and the Wii U's getaddrinfo is enough) */
#ifdef KK_SERVER_IP
    if (!ok && !strcmp(g_lookup_host, KK_SERVER_HOST) && inet_pton(AF_INET, KK_SERVER_IP, &lit) == 1) { g_found_addr = lit.s_addr; ok = 1; }   /* the server's own number, if the name can't be looked up */
#endif
    g_found = ok; g_lookup_done = 1; g_lookup_busy = 0; }
#ifdef __WIIU__
static OSThread *g_thr; static void *g_stack;
static int lookup_thread(int argc, void *argv) { (void)argc; (void)argv; lookup(); return 0; }
static int start_lookup(void) {
    const uint32_t ss = 64 * 1024;
    if (!g_thr) { g_thr = memalign(16, sizeof(OSThread)); g_stack = memalign(16, ss); if (!g_thr || !g_stack) return 0; }
    memset(g_thr, 0, sizeof(OSThread));
    if (!OSCreateThread(g_thr, lookup_thread, 0, NULL, (uint8_t *)g_stack + ss, ss, 20, OS_THREAD_ATTRIB_AFFINITY_CPU0 | OS_THREAD_ATTRIB_DETACHED)) return 0;
    OSSetThreadName(g_thr, "kk net lookup"); OSResumeThread(g_thr); return 1; }
#else
static void *lookup_thread(void *a) { (void)a; lookup(); return NULL; }
static int start_lookup(void) { pthread_t t; if (pthread_create(&t, NULL, lookup_thread, NULL) != 0) return 0; pthread_detach(t); return 1; }
#endif

static void close_socket(void) { if (g_fd >= 0) { close(g_fd); g_fd = -1; } }
static int open_socket(void) {
    close_socket(); g_fd = socket(AF_INET, SOCK_DGRAM, 0); if (g_fd < 0) return 0;
    int fl = fcntl(g_fd, F_GETFL, 0); fcntl(g_fd, F_SETFL, (fl < 0 ? 0 : fl) | O_NONBLOCK);   /* never wait */
#ifdef SO_NONBLOCK
    int one = 1; setsockopt(g_fd, SOL_SOCKET, SO_NONBLOCK, &one, sizeof one);
#endif
    return 1; }
static void send_pkt(const uint8_t *p, int n) { if (g_fd >= 0) sendto(g_fd, p, n, 0, (struct sockaddr *)&g_srv, sizeof g_srv); }
static void send_hello(void) {
    uint8_t p[KKP_HELLO_LEN]; kkp_hdr(p, KKP_HELLO); memcpy(p + 4, g_player, 16); kkp_put32(p + 20, GAME_BUILD); kkp_put32(p + 24, g_nonce); send_pkt(p, sizeof p); }
static void send_ping(void) {
    uint8_t p[KKP_PING_LEN]; kkp_hdr(p, KKP_PING); memcpy(p + 4, g_session, 16); kkp_put32(p + 20, ++g_seq); kkp_put32(p + 24, now_ms()); send_pkt(p, sizeof p); S.sent++; }

void net_test_start(const char *host, int port) {
    net_stop(); memset(&S, 0, sizeof S); make_player_id();
    snprintf(S.host, sizeof S.host, "%s", host && host[0] ? host : KK_SERVER_HOST); S.port = port > 0 ? port : KKNET_PORT;
    S.ping_ms = S.best_ms = S.worst_ms = S.avg_ms = -1; g_seq = 0; g_rtt_sum = 0;
    if (g_lookup_busy) { S.state = NET_LOOKING_UP; return; }   /* the last lookup is still going: wait for it */
    snprintf(g_lookup_host, sizeof g_lookup_host, "%s", S.host); g_lookup_done = 0; g_found = 0; g_lookup_busy = 1;
    if (!start_lookup()) { g_lookup_busy = 0; lookup(); }   /* no thread: look it up here */
    S.state = NET_LOOKING_UP; }

void net_update(float dt) {
    if (S.state == NET_OFF || S.state == NET_NO_ADDRESS || S.state == NET_REFUSED || S.state == NET_NO_SOCKET) return;
    if (S.state == NET_LOOKING_UP) {
        if (!g_lookup_done) return;
        if (strcmp(g_lookup_host, S.host)) { net_test_start(S.host, S.port); return; }
        if (!g_found) { S.state = NET_NO_ADDRESS; return; }
        memset(&g_srv, 0, sizeof g_srv); g_srv.sin_family = AF_INET; g_srv.sin_port = htons((uint16_t)S.port); g_srv.sin_addr.s_addr = g_found_addr;
        if (!open_socket()) { S.state = NET_NO_SOCKET; return; }
        g_nonce = mix32(now_ms() * 2654435761U + 7); g_hello_tries = 1; g_resend_t = 1.0f; send_hello(); S.state = NET_SAYING_HELLO; return; }
    /* read everything that has arrived */
    for (int k = 0; k < 32; k++) {
        uint8_t p[KKNET_MAX_PACKET + 1]; struct sockaddr_in from; socklen_t fl = sizeof from;
        int n = (int)recvfrom(g_fd, p, sizeof p, 0, (struct sockaddr *)&from, &fl);
        if (n <= 0) break;
        if (from.sin_addr.s_addr != g_srv.sin_addr.s_addr || from.sin_port != g_srv.sin_port) continue;   /* only the server is listened to */
        int t = kkp_type(p, n); if (!t) continue;
        g_quiet_t = 0;
        if (t == KKP_WELCOME && S.state == NET_SAYING_HELLO && kkp_get32(p + 4) == g_nonce) {
            memcpy(g_session, p + 8, 16); S.online = kkp_get16(p + 24); S.server_build = kkp_get32(p + 26); memcpy(S.message, p + 30, 32); S.message[32] = 0;
            S.state = NET_CONNECTED; S.connected_s = 0; g_ping_t = 0; }
        else if (t == KKP_PONG && S.state == NET_CONNECTED) {
            uint32_t rtt = now_ms() - kkp_get32(p + 8); if (rtt > 60000) continue;
            S.answered++; S.online = kkp_get16(p + 12); S.ping_ms = (int)rtt; g_rtt_sum += rtt; S.avg_ms = (int)(g_rtt_sum / (uint32_t)S.answered);
            if (S.best_ms < 0 || (int)rtt < S.best_ms) S.best_ms = (int)rtt;
            if ((int)rtt > S.worst_ms) S.worst_ms = (int)rtt; }
        else if (t == KKP_REFUSED) {
            int why = p[4];
            if (why == KKR_NO_SESSION && S.state == NET_CONNECTED) { S.state = NET_SAYING_HELLO; g_hello_tries = 1; g_resend_t = 1.0f; send_hello(); }   /* the server restarted: hello again */
            else if (why != KKR_SLOW_DOWN) { S.state = NET_REFUSED; S.refused_why = why; close_socket(); return; } } }
    if (S.state == NET_SAYING_HELLO) {
        g_resend_t -= dt;
        if (g_resend_t <= 0) { if (g_hello_tries >= 10) { S.state = NET_NO_ANSWER; close_socket(); return; } g_hello_tries++; g_resend_t = 1.0f; send_hello(); } }
    else if (S.state == NET_CONNECTED) {
        S.connected_s += dt; g_ping_t -= dt; g_quiet_t += dt;
        if (g_ping_t <= 0) { g_ping_t = 1.0f; send_ping(); }
        if (g_quiet_t > 10.0f) { S.state = NET_NO_ANSWER; close_socket(); } } }

void net_stop(void) {
    if (g_fd >= 0 && S.state == NET_CONNECTED) { uint8_t p[KKP_BYE_LEN]; kkp_hdr(p, KKP_BYE); memcpy(p + 4, g_session, 16); send_pkt(p, sizeof p); }
    close_socket(); S.state = NET_OFF; }
const NetStatus *net_status(void) { return &S; }
const char *net_state_words(int st) {
    switch (st) {
    case NET_LOOKING_UP: return "FINDING THE SERVER...";
    case NET_NO_ADDRESS: return "CAN'T FIND THE SERVER";
    case NET_SAYING_HELLO: return "SAYING HELLO...";
    case NET_CONNECTED: return "CONNECTED";
    case NET_NO_ANSWER: return "THE SERVER DIDN'T ANSWER";
    case NET_REFUSED: return "THE SERVER SAID NO";
    case NET_NO_SOCKET: return "NO INTERNET CONNECTION";
    default: return "OFF"; } }
