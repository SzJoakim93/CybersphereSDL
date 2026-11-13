#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

#define SOUND_EFFECT_COUNT 26
#define SOUND_MUSIC_COUNT 9

/* Sound effect ids passed to 1000:1d23 in the original. */
enum {
	SFX_BRICK_EXPLODE = 0,   /* 0..7 random variants */
	SFX_BALL_EXPLODE = 5,
	SFX_GUARD_OFF = 6,
	SFX_GEM = 8,
	SFX_CAPSULE = 9,
	SFX_BOUNCE = 0x0a,
	SFX_PADDLE = 0x0b,
	SFX_BRICK_HIT = 0x0c,
	SFX_PAUSE = 0x0d,
	SFX_PADDLE_EDGE = 0x0e,
	SFX_RELEASE = 0x0f,
	SFX_SHOT = 0x10,
	SFX_CATCH = 0x11,
	SFX_BALL_LOST = 0x12,
	SFX_BALL_SMOKE = 0x13,
	SFX_LAST_BALL = 0x15,
	SFX_GUARD_ON = 0x18,
	SFX_EXTRA_LIFE = 0x19
};

int sound_init(void);
int sound_prepare(int rate);
void sound_shutdown(void);
void sound_play(int id);
void sound_music(int track);
void sound_enable(int effects, int music);
void sound_render(int16_t *samples, int frames);
int sound_export_wav(const char *path, int track, int effect, int seconds);
unsigned sound_faults(void);

#endif
