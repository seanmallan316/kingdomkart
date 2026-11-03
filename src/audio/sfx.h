// sfx.h - the game's sound effects, made from simple waves at run time (no sound files). Platform independent:
// the Wii U side pulls samples from sfx_render() inside the audio frame callback; tests render to a WAV file.
#ifndef KK_SFX_H
#define KK_SFX_H
#include <stdint.h>

enum {
    SFX_MENU_MOVE, SFX_MENU_OK, SFX_MENU_BACK,
    SFX_COUNT_TICK, SFX_GO,
    SFX_PICKUP, SFX_USE_ITEM, SFX_THROW,
    SFX_HIT, SFX_BLOCK, SFX_BUMP,
    SFX_LAP, SFX_FINAL_LAP, SFX_FINISH, SFX_WIN,
    SFX_MINI_TURBO, SFX_SUPER_TURBO, SFX_BOOST_PAD, SFX_BOUNCE, SFX_LAND, SFX_SPLASH,
    SFX_SHOFAR, SFX_FIRE, SFX_LOCUSTS, SFX_DOVE,
    SFX_UNLOCK, SFX_WELCOME_HOME, SFX_WRONG_WAY, SFX_COIN,
    SFX_CRUMBLE, SFX_SHOUT,                     /* Jericho: a wall section falls; the great shout */
    SFX_COUNTDOWN, SFX_ENGINE_START,            /* only sound when recorded (content/sfx/sting_countdown.wav, kart_engine_start.wav) */
    SFX_COUNT
};

void sfx_init(int sample_rate);
void sfx_play(int id, float volume, float pan);          /* pan -1 left .. 1 right; safe to call from the game thread */
/* the player's kart, updated every frame: speed 0..1 of top speed, flags for the continuous sounds */
void sfx_engine(float speed, int drifting, int boosting, int offroad, int in_air, int water, int active, int gas, int brake);
void sfx_set_volume(float vol);                            /* 0..1, the player's sound volume setting */
void sfx_set_logger(void (*log)(const char *fmt, ...));
void sfx_load_samples(void);                               /* reads the recorded sounds from content/sfx once; without them everything is synthesized */
int  sfx_has_sample(int id);                               /* 1 if a recording replaces this effect */
void sfx_render(int16_t *left, int16_t *right, int n);  /* called from the audio thread */
int  sfx_voices_active(void);                            /* for tests */
#endif
