// music.h - background music: MP3 files packed in the game (content/music/NAME.mp3), streamed and decoded a little
// at a time on the game thread into a ring that the sound thread mixes under the sound effects.
//   music_want("music_beth") every frame says which track should be playing; the player crossfades when it changes.
//   A track loops forever with a short crossfade from its end back to its start. Missing files are simply silent.
#ifndef KK_MUSIC_H
#define KK_MUSIC_H
#include <stdint.h>

void music_init(int sample_rate);                 /* sample rate of the mixer (32000 on the Wii U) */
void music_set_logger(void (*log)(const char *fmt, ...));
int  music_available(const char *name);           /* 1 if content/music/NAME.mp3 exists (result is remembered) */
void music_want(const char *name);                /* track that should be playing now, NULL for silence; call every frame */
void music_set_duck(float gain);                  /* 0..1, lowers the music while paused */
void music_update(int max_frames);                /* decodes ahead; call once per game frame (max_frames bounds the work) */
void music_mix(int16_t *left, int16_t *right, int n);   /* audio thread: adds music to a buffer of sound effects */
const char *music_now(void);                      /* name of the track playing (for the log and tests) */
int  music_buffered(void);                        /* samples decoded and waiting (for tests) */
void music_shutdown(void);
#endif
