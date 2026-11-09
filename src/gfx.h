#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include "assets.h"

/* 320x200 8-bit back buffer, like segment 6622 in the original. */
extern uint8_t gfx_fb[SCREEN_W * SCREEN_H];
/* VGA DAC contents, 6-bit components. */
extern uint8_t gfx_pal[768];

void gfx_clear(void);
void gfx_set_palette(const uint8_t *rgb, int start, int count, int bright);
void gfx_set_palette_whiten(const uint8_t *rgb, int start, int count, int bright);

void gfx_sprite(SpriteId id, int frame, int x, int y);
void gfx_sprite_clipped_h(SpriteId id, int frame, int x, int y, int max_rows);
void gfx_fill(int x, int y, int w, int h, uint8_t color);

/* 8x8 game font; returns x after the text. */
int gfx_text(int x, int y, uint8_t color, const char *s);
void gfx_message(const uint8_t *msg);
void gfx_digit(int x, int y, int d);

/* Level being displayed: brick layer + background layer, as at DS:70D6/7658. */
typedef struct {
	LevelData bricks;
	LevelData layer2;
	StageParams params;
} Level;

int level_setup(Level *lv, int param_index);
void gfx_bricks(const Level *lv);
void gfx_brick_flash(int cell, int shift);

/* Paddle as drawn by 1000:4101. x in pixels. */
typedef struct {
	int x;
	int wide;
	int fire;
	int hold;
	int holding;
} PaddleView;
void gfx_paddle(const PaddleView *p);

void gfx_hud(uint32_t score, int gems, int lives);

/* Palette animation state (1000:2df0, 2ec2, 2f05). */
typedef struct {
	uint8_t base[768];
	PalPulse pulse[NUM_PULSES];
	uint16_t cyc_a, cyc_b, cyc_c;
	uint16_t flash;
	int fire;
} PaletteFx;

void palfx_init(PaletteFx *fx, const Level *lv);
void palfx_apply(PaletteFx *fx);
void palfx_step(PaletteFx *fx);

#endif
