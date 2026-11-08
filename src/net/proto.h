// proto.h - the messages between a Kingdom Kart console and the Kingdom Kart server (kkserver), shared by both.
// Everything travels as UDP packets. Every packet starts with 'K' 'K', the protocol version and its type; numbers are
// written big-endian byte by byte, so the Wii U (big-endian) and the server (little-endian) read them the same way.
// Phase 1 (network test): HELLO -> WELCOME, then PING -> PONG once a second.
#ifndef KK_PROTO_H
#define KK_PROTO_H
#include <stdint.h>
#include <string.h>

#define KKNET_PORT 7777
#define KKNET_VERSION 1
#ifndef KK_SERVER_HOST
#define KK_SERVER_HOST "kingdomkart-rtg-online.fly.dev"
#endif
#define KKNET_MAX_PACKET 1200     /* stays under every home router's packet size */

enum { KKP_HELLO = 1, KKP_WELCOME = 2, KKP_PING = 3, KKP_PONG = 4, KKP_BYE = 5, KKP_REFUSED = 6 };
enum { KKR_BAD_VERSION = 1, KKR_BUSY = 2, KKR_SLOW_DOWN = 3, KKR_NO_SESSION = 4 };

/* sizes of each message, header included */
#define KKP_HDR 4
#define KKP_HELLO_LEN   (KKP_HDR + 16 + 4 + 4)          /* player id, game build, nonce */
#define KKP_WELCOME_LEN (KKP_HDR + 4 + 16 + 2 + 4 + 32) /* nonce echoed, session, players online, server build, message */
#define KKP_PING_LEN    (KKP_HDR + 16 + 4 + 4)          /* session, sequence, client time (ms) */
#define KKP_PONG_LEN    (KKP_HDR + 4 + 4 + 2)           /* sequence, client time echoed, players online */
#define KKP_BYE_LEN     (KKP_HDR + 16)
#define KKP_REFUSED_LEN (KKP_HDR + 1)

static inline void kkp_put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static inline void kkp_put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static inline uint32_t kkp_get32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static inline uint16_t kkp_get16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static inline void kkp_hdr(uint8_t *p, int type) { p[0] = 'K'; p[1] = 'K'; p[2] = KKNET_VERSION; p[3] = (uint8_t)type; }
/* the type of a well-formed packet of this version, or 0 */
static inline int kkp_type(const uint8_t *p, int n) {
    if (n < KKP_HDR || n > KKNET_MAX_PACKET || p[0] != 'K' || p[1] != 'K' || p[2] != KKNET_VERSION) return 0;
    int t = p[3], need = t == KKP_HELLO ? KKP_HELLO_LEN : t == KKP_WELCOME ? KKP_WELCOME_LEN : t == KKP_PING ? KKP_PING_LEN : t == KKP_PONG ? KKP_PONG_LEN
                       : t == KKP_BYE ? KKP_BYE_LEN : t == KKP_REFUSED ? KKP_REFUSED_LEN : -1;
    return need > 0 && n == need ? t : 0; }
#endif
