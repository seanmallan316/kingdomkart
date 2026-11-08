// net_test.c - the Phase 1 online test, on the PC: starts kkserver on this machine, connects the game's own network
// code (src/net/net.c) to it, and checks hello, pings, junk being ignored, a wrong version being refused, and goodbye.
//   cc -std=gnu11 -Isrc server/kkserver.c -o kkserver && cc -std=gnu11 -Isrc tests/net_test.c src/net/net.c -lpthread -o net_test && ./net_test ./kkserver
#define _POSIX_C_SOURCE 200809L
#include "net/net.h"
#include "net/proto.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PORT 17777
static int fails;
#define CHECK(c, msg) do { if (c) printf("ok   %s\n", msg); else { printf("FAIL %s\n", msg); fails++; } } while (0)
static void nap(int ms) { struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L }; nanosleep(&ts, NULL); }
static void run(float secs) { for (int i = 0; i < (int)(secs * 20); i++) { net_update(0.05f); nap(50); } }

static int raw_exchange(const uint8_t *out, int n, uint8_t *in) {   /* one packet from a fresh socket, one answer back (or 0) */
    int fd = socket(AF_INET, SOCK_DGRAM, 0); struct sockaddr_in a; memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_port = htons(PORT); a.sin_addr.s_addr = htonl(0x7f000001);
    struct timeval tv = {0, 400000}; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    sendto(fd, out, n, 0, (struct sockaddr *)&a, sizeof a); int got = (int)recv(fd, in, KKNET_MAX_PACKET, 0); close(fd); return got > 0 ? got : 0; }

int main(int argc, char **argv) {
    if (argc > 2 && !strcmp(argv[1], "--live")) {   /* the real server on the internet: connect and ping for a few seconds */
        for (int attempt = 0; attempt < 6; attempt++) {
            net_test_start(argv[2], argc > 3 ? atoi(argv[3]) : 0); run(6.0f); const NetStatus *s = net_status();
            printf("try %d: %s, ping %d ms, %d of %d answered, %d online\n", attempt + 1, net_state_words(s->state), s->ping_ms, s->answered, s->sent, s->online);
            if (s->state == NET_CONNECTED && s->answered >= 2) { net_stop(); printf("LIVE SERVER OK\n"); return 0; }
            run(4.0f); }
        printf("LIVE SERVER DID NOT ANSWER\n"); return 1; }
    const char *server = argc > 1 ? argv[1] : "./kkserver";
    pid_t pid = fork();
    if (pid == 0) { setenv("KK_BIND", "127.0.0.1", 1); char port[8]; snprintf(port, sizeof port, "%d", PORT); setenv("KK_PORT", port, 1); execl(server, server, "30", (char *)NULL); _exit(127); }
    nap(400);
    /* a console finds the server, says hello and pings */
    net_test_start("127.0.0.1", PORT); run(4.5f);
    const NetStatus *s = net_status();
    CHECK(s->state == NET_CONNECTED, "connected to the server");
    CHECK(s->answered >= 3 && s->answered <= s->sent, "pings answered");
    CHECK(s->ping_ms >= 0 && s->ping_ms < 200 && s->best_ms <= s->worst_ms, "round-trip times measured");
    CHECK(s->online == 1, "server counts one console online");
    CHECK(!strcmp(s->message, "Welcome to Kingdom Kart Online!"), "welcome message arrived");
    printf("     ping %d ms (best %d, worst %d), %d of %d answered\n", s->ping_ms, s->best_ms, s->worst_ms, s->answered, s->sent);
    /* junk is ignored without an answer, and the server keeps working */
    uint8_t junk[64], in[KKNET_MAX_PACKET]; for (int i = 0; i < 64; i++) junk[i] = (uint8_t)(i * 37 + 11);
    CHECK(raw_exchange(junk, sizeof junk, in) == 0, "junk packet gets no answer");
    uint8_t shortp[6] = {'K', 'K', KKNET_VERSION, KKP_HELLO, 0, 0};
    CHECK(raw_exchange(shortp, sizeof shortp, in) == 0, "too-short hello gets no answer");
    uint8_t ping[KKP_PING_LEN]; kkp_hdr(ping, KKP_PING); memset(ping + 4, 0x55, KKP_PING_LEN - 4);
    int n = raw_exchange(ping, sizeof ping, in);
    CHECK(n == KKP_REFUSED_LEN && in[3] == KKP_REFUSED && in[4] == KKR_NO_SESSION, "ping with a made-up session is refused");
    uint8_t oldv[KKP_HELLO_LEN]; kkp_hdr(oldv, KKP_HELLO); oldv[2] = KKNET_VERSION + 1; memset(oldv + 4, 0, KKP_HELLO_LEN - 4);
    n = raw_exchange(oldv, sizeof oldv, in);
    CHECK(n == KKP_REFUSED_LEN && in[4] == KKR_BAD_VERSION, "a different game version is told so");
    run(2.0f); s = net_status();
    CHECK(s->state == NET_CONNECTED && s->answered >= 4, "still connected after the junk");
    /* goodbye, then a server that isn't there */
    net_stop(); CHECK(net_status()->state == NET_OFF, "goodbye closes the link");
    kill(pid, SIGTERM); waitpid(pid, NULL, 0);
    net_test_start("127.0.0.1", PORT); run(12.0f);
    CHECK(net_status()->state == NET_NO_ANSWER, "no server: says it didn't answer (after 10 tries)");
    net_test_start("no-such-host.invalid", PORT); run(3.0f);
    CHECK(net_status()->state == NET_NO_ADDRESS, "unknown server name: says it can't find it");
    printf(fails ? "NET TEST FAILED (%d)\n" : "NET TEST OK\n", fails);
    return fails ? 1 : 0; }
