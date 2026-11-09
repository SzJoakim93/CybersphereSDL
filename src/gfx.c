#include <string.h>
#include "gfx.h"

uint8_t gfx_fb[SCREEN_W * SCREEN_H];
uint8_t gfx_pal[768];

#define PADDLE_Y 184

void gfx_clear(void)
{
	memset(gfx_fb, 0, sizeof(gfx_fb));
}

/* 1000:451d */
void gfx_set_palette(const uint8_t *rgb, int start, int count, int bright)
{
	int i;

	for (i = 0; i < count * 3; i++)
		gfx_pal[(start * 3 + i) % 768] = (uint8_t)(((rgb[i] & 0x3f) * bright) >> 8);
}

/* 1000:4559 - fades towards white instead of black */
void gfx_set_palette_whiten(const uint8_t *rgb, int start, int count, int bright)
{
	int i;

	for (i = 0; i < count * 3; i++)
		gfx_pal[(start * 3 + i) % 768] = (uint8_t)(0x3f - (((0x3f - (rgb[i] & 0x3f)) * bright) >> 8));
}

static inline void plot(int x, int y, uint8_t c)
{
	if ((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H)
		gfx_fb[y * SCREEN_W + x] = c;
}

static void blit(const SpriteDef *d, const uint8_t *src, int x, int y, int w, int h)
{
	int sx, sy;

	for (sy = 0; sy < h; sy++) {
		const uint8_t *row = src + sy * d->stride;
		for (sx = 0; sx < w; sx++) {
			uint8_t v = row[sx];
			if (d->hole >= 0 && v == d->hole)
				plot(x + sx, y + sy, 0);
			else if (v)
				plot(x + sx, y + sy, (uint8_t)(v + d->base));
		}
	}
}

void gfx_sprite(SpriteId id, int frame, int x, int y)
{
	const Sprite *s = sprite_get(id);
	blit(s->def, sprite_frame(id, frame), x, y, s->def->w, s->def->h);
}

void gfx_sprite_clipped_h(SpriteId id, int frame, int x, int y, int max_rows)
{
	const Sprite *s = sprite_get(id);
	int h = s->def->h < max_rows ? s->def->h : max_rows;
	blit(s->def, sprite_frame(id, frame), x, y, s->def->w, h);
}

void gfx_fill(int x, int y, int w, int h, uint8_t color)
{
	int i, j;

	for (j = 0; j < h; j++)
		for (i = 0; i < w; i++)
			plot(x + i, y + j, color);
}

/* 1000:45ea */
static void glyph(int x, int y, uint8_t color, char c)
{
	const uint8_t *g = font_glyph(c);
	int r, b;

	for (r = 0; r < 8; r++)
		for (b = 0; b < 8; b++)
			if (g[r] & (0x80 >> b))
				plot(x + b, y + r, color);
}

int gfx_text(int x, int y, uint8_t color, const char *s)
{
	for (; *s; s++, x += 8) {
		char c = *s;
		if (c >= 'a' && c <= 'z')
			c -= 'a' - 'A';
		glyph(x, y, color, c);
	}
	return x;
}

/* 1000:45b3 */
void gfx_message(const uint8_t *m)
{
	while (*m != 0xff) {
		int x = m[0] * 2, y = m[1];
		uint8_t color;

		m += 2;
		color = *m++;
		for (;;) {
			uint8_t c = *m++;
			if (c == 0) {
				color = *m++;
				continue;
			}
			if (c == 0xff)
				break;
			glyph(x, y, color, (char)c);
			x += 8;
		}
	}
}

void gfx_digit(int x, int y, int d)
{
	gfx_sprite(SPR_DIGIT, d, x, y);
}

int level_setup(Level *lv, int param_index)
{
	if (!stage_params(param_index, &lv->params))
		return 0;
	if (!layout_load(lv->params.layout, &lv->bricks))
		return 0;
	layer2_load(lv->params.layer2, &lv->layer2);
	return 1;
}

/* Brick cells wrap horizontally when the layout is shifted by half a brick. */
static void cell_opaque(int x, int y, const uint8_t *src, uint8_t add)
{
	int i, j;

	for (j = 0; j < BRICK_H; j++)
		for (i = 0; i < BRICK_W; i++)
			plot((x + i + SCREEN_W) % SCREEN_W, y + j, (uint8_t)(src[j * BRICK_W + i] + add));
}

static void cell_masked(int x, int y, const uint8_t *src, uint8_t add)
{
	int i, j;

	for (j = 0; j < BRICK_H; j++)
		for (i = 0; i < BRICK_W; i++) {
			uint8_t v = src[j * BRICK_W + i];
			if (v)
				plot(x + i, y + j, (uint8_t)(v + add));
		}
}

/* 1000:4301 / 1000:4338 */
void gfx_bricks(const Level *lv)
{
	const LevelData *b = &lv->bricks, *l2 = &lv->layer2;
	int shift = b->shift ? 10 : 0;
	int r, c;

	for (r = 0; r < GRID_ROWS; r++) {
		for (c = 0; c < GRID_COLS; c++) {
			int i = r * GRID_COLS + c;
			int x = c * BRICK_W - shift, y = r * BRICK_H;
			const BrickType *t = brick_type(b->type[i]);
			int mode = t->mode >> 4;
			const uint8_t *src;

			if (mode == 1 || t->gfx == 0) {
				src = tile_gfx(l2->type[i]);
				cell_opaque(x, y, src, l2->color[i]);
			} else {
				src = brick_gfx(t->gfx);
				cell_opaque(x, y, src, b->color[i]);
			}

			if (mode == 1 && !(shift && c == 0)) {
				cell_masked(x, y, brick_gfx(t->gfx), b->color[i]);
			} else if (mode == 2 && c != 0) {
				if (!src[0])
					plot(x, y, 0);
				if (!src[BRICK_W - 1])
					plot(x + BRICK_W - 1, y, 0);
				if (!src[BRICK_BYTES - BRICK_W])
					plot(x, y + BRICK_H - 1, 0);
				if (!src[BRICK_BYTES - 1])
					plot(x + BRICK_W - 1, y + BRICK_H - 1, 0);
			}
		}
	}
}

/* 1000:3e72, first loop: hit bricks blink white */
void gfx_brick_flash(int cell, int shift)
{
	gfx_fill((cell & 15) * BRICK_W - (shift ? 10 : 0), (cell >> 4) * BRICK_H, BRICK_W, BRICK_H, 0xff);
}

/* 1000:4101 */
void gfx_paddle(const PaddleView *p)
{
	const Sprite *pad = sprite_get(SPR_PADDLE);
	const SpriteDef *d = pad->def;
	int x = p->x;

	if (!p->wide) {
		gfx_sprite(SPR_PADDLE, 0, x, PADDLE_Y);
	} else {
		/* 70px paddle: columns 0..34 then 15..49 of each row */
		blit(d, pad->pixels, x, PADDLE_Y, 35, d->h);
		blit(d, pad->pixels + 15, x + 35, PADDLE_Y, 35, d->h);
	}

	if (p->holding || p->hold) {
		gfx_sprite(SPR_CLAW_L, 0, x, 180);
		gfx_sprite(SPR_CLAW_R, 0, x + 46 + (p->wide ? 20 : 0), 180);
	}

	if (p->fire) {
		gfx_sprite(SPR_CANNON, 0, x + 5, 178);
		gfx_sprite(SPR_CANNON, 0, x + 5 + 31 + (p->wide ? 20 : 0), 178);
	}

	if (p->holding) {
		const Sprite *bar = sprite_get(SPR_HOLD_BAR);
		if (!p->wide) {
			gfx_sprite(SPR_HOLD_BAR, 0, x + 3, 178);
		} else {
			blit(bar->def, bar->pixels, x + 3, 178, 30, bar->def->h);
			blit(bar->def, bar->pixels + 10, x + 33, 178, 34, bar->def->h);
		}
	}
}

/* Number printing of 1000:3a0c: leading zeros suppressed, last digit always shown. */
static void number(int x, int y, uint32_t v, int digits)
{
	uint32_t div = 1;
	int i, started = 0;

	for (i = 1; i < digits; i++)
		div *= 10;
	for (; div; div /= 10) {
		int d = (int)((v / div) % 10);
		if (d || started || div == 1) {
			gfx_digit(x, y, d);
			x += 8;
			started = 1;
		}
	}
}

/* 1000:3a0c (messages are drawn by the caller) */
void gfx_hud(uint32_t score, int gems, int lives)
{
	int i, y = 173;

	gfx_sprite(SPR_GEM_ICON, 0, 4, 183);
	number(25, 184, (uint32_t)gems, 3);

	for (i = 0; i < lives; i++, y -= 8)
		gfx_sprite(SPR_LIFE, 0, 4, y);

	number(6, 6, score, 8);
}

void palfx_init(PaletteFx *fx, const Level *lv)
{
	memcpy(fx->base, lv->bricks.pal, 768);
	memcpy(fx->pulse, lv->params.pulse, sizeof(fx->pulse));
	fx->cyc_a = fx->cyc_b = fx->cyc_c = 0;
	fx->flash = 0;
	gfx_set_palette(fx->base, 0, 256, 0x100);
}

/* 0..0x1ff -> triangle 0..0x100..0 */
static int tri(uint16_t v)
{
	v &= 0x1ff;
	return v > 0x100 ? 0x200 - v : v;
}

/* 1000:2df0 + 1000:2ec2 */
void palfx_apply(PaletteFx *fx)
{
	int i;

	if (fx->flash) {
		int b = fx->flash;
		if (b > 0x100) {
			b = 0x100;
			fx->flash = 0;
		}
		gfx_set_palette_whiten(fx->base + 0xd9 * 3, 0xd9, 0x17, b);
	} else {
		gfx_set_palette(fx->base + 0xef * 3, 0xef, 1, fx->cyc_a & 0xff);
		gfx_set_palette(fx->base + 0xe5 * 3, 0xe5, 3, tri(fx->cyc_b) / 2 + 0x80);
		gfx_set_palette(fx->base + 0xe8 * 3, 0xe8, 3, tri(fx->cyc_b + 0x100) / 2 + 0x80);
		gfx_set_palette(fx->base + 0xd9 * 3, 0xd9, 3, tri(fx->cyc_c) / 2 + 0x80);
	}

	for (i = 0; i < NUM_PULSES; i++) {
		const PalPulse *p = &fx->pulse[i];
		int bright;

		if (!p->count)
			continue;
		/* 16-bit MUL keeping only AX, as in the original */
		bright = (uint16_t)((0x100 - p->min_bright) * tri(p->phase)) >> 8;
		bright += p->min_bright;
		gfx_set_palette(fx->base + p->start * 3, p->start, p->count, bright);
	}
}

/* 1000:2f05 */
void palfx_step(PaletteFx *fx)
{
	int i;

	if (fx->flash)
		fx->flash += 3;
	fx->cyc_a += 8;
	if (fx->fire)
		fx->cyc_a += 6;
	fx->cyc_b += 7;
	fx->cyc_c += 13;
	for (i = 0; i < NUM_PULSES; i++)
		fx->pulse[i].phase += fx->pulse[i].speed;
}
