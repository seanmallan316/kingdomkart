// net.h - the game's link to the Kingdom Kart server (Phase 1: the ONLINE TEST screen).
// net_test_start() looks up the server (on a helper thread, so the game never freezes), says hello and then pings it
// once a second; net_update() is called every frame and never waits. The same code builds for the Wii U and for the PC
// tests (both use the standard socket calls; on the Wii U, wut switches the network on by itself).
#ifndef KK_NET_H
#define KK_NET_H
#include <stdint.h>

enum { NET_OFF, NET_LOOKING_UP, NET_NO_ADDRESS, NET_SAYING_HELLO, NET_CONNECTED, NET_NO_ANSWER, NET_REFUSED, NET_NO_SOCKET };
typedef struct {
    int state, refused_why;
    char host[64]; int port;
    int ping_ms, best_ms, worst_ms, avg_ms;     /* round trip to the server; -1 until the first answer */
    int sent, answered;                          /* pings sent, pongs back (the difference = lost) */
    int online;                                  /* consoles the server can see right now */
    unsigned server_build; char message[33];
    float connected_s;                           /* how long it has been connected */
} NetStatus;

void net_test_start(const char *host, int port);   /* NULL/0 = the Kingdom Kart server */
void net_update(float dt);
void net_stop(void);                               /* says goodbye and closes */
const NetStatus *net_status(void);
const char *net_state_words(int state);
#endif
