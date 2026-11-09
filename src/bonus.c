#include "game.h"
#include "sound.h"

#define DS_CAPSULE_TYPES 0x612e /* 0x26 words: what a destroyed brick drops */
#define DS_RANDOM_BONUS  0x617c /* 0x1d words: outcome of the "?" capsule */

enum {
	BONUS_NONE, BONUS_GEMS, BONUS_WIDE, BONUS_FIRE, BONUS_SLOW, BONUS_HALF_GEMS,
	BONUS_MULTIBALL, BONUS_GUARD, BONUS_HOLD, BONUS_RANDOM, BONUS_LIFE, BONUS_WARP
};

/* Message indices into the DS:6849 table */
enum {
	MSG_MULTIBALL_FULL = 0x0c, MSG_LIVES_FULL = 0x0d, MSG_BONUS_WARP = 0x0e, MSG_WARP_SCORE = 0x0f
};

static int16_t slow_down(int16_t v)
{
	if (v < 0)
		return v + 5 > 0 ? 0 : v + 5;
	return v - 5 < 0 ? 0 : v - 5;
}

/* Sideways kick of gems and capsules leaving a brick in column col */
static int16_t kick(int col)
{
	return (int16_t)(rng(0x100) - 0x80 - (col * 8 - 0x40));
}

/* 1000:2572 */
static void gem_spawn(Game *g, int cell)
{
	int i, col = cell & 15;

	for (i = 0; i < MAX_GEMS; i++) {
		Gem *m = &g->gems[i];
		if (m->anim)
			continue;
		if ((unsigned)cell >= GRID_CELLS)
			return;
		m->y = (uint16_t)((cell >> 4) * 0x280 - 0x80);
		m->x = (uint16_t)(col * 0x500 + 0x80 - (g->lv.bricks.shift ? 0x280 : 0));
		m->anim = 1;
		m->vx = kick(col);
		m->vy = -0x60;
		return;
	}
}

/* Part of 1000:26a6 */
void gems_burst(Game *g, int cell, int count)
{
	int i;

	for (i = 0; i < MAX_GEM_BURSTS; i++)
		if (!g->bursts[i].counter) {
			g->bursts[i].counter = (uint16_t)(count * 8 - 7);
			g->bursts[i].cell = (uint16_t)cell;
			return;
		}
}

/* 1000:2964 */
static void bursts_update(Game *g)
{
	int i;

	for (i = 0; i < MAX_GEM_BURSTS; i++) {
		GemBurst *b = &g->bursts[i];
		if (b->counter && !(--b->counter & 7))
			gem_spawn(g, b->cell);
	}
}

/* 1000:25e9 */
void bonus_drop(Game *g, int cell)
{
	int i, col = cell & 15;

	for (i = 0; i < MAX_CAPSULES; i++) {
		Capsule *c = &g->capsules[i];
		int type;

		if (c->anim)
			continue;
		if ((unsigned)cell >= GRID_CELLS)
			return;
		c->y = (int16_t)((cell >> 4) * 0x280 - 0x180);
		c->x = (uint16_t)(col * 0x500 - 0x40 - (g->lv.bricks.shift ? 0x280 : 0));
		c->anim = 1;

		type = ds_word((uint16_t)(DS_CAPSULE_TYPES + rng(0x26) * 2));
		if (type == BONUS_WARP && g->bonus_stages_played >= 2)
			type = BONUS_NONE;
		if (type == BONUS_LIFE) {
			if (g->life_dropped)
				type = BONUS_NONE;
			g->life_dropped = 1;
		}
		c->type = (uint16_t)type;
		c->vx = kick(col);
		c->vy = -0x60;
		return;
	}
}

/* Bonus effects of 1000:2720, indexed by capsule type */

static void fx_none(Game *g)
{
	(void)g;
}

static void fx_gems(Game *g)
{
	g->pl.gems += 10;
	if (g->pl.gems > 500)
		g->pl.gems = 500;
}

static void fx_wide(Game *g)
{
	Player *p = &g->pl;

	if (p->wide) {
		p->wide = 0;
		p->x += 0x280;
		return;
	}
	p->wide = 1;
	p->x -= 0x280;
	if ((int16_t)p->x < p->min_x)
		p->x = (uint16_t)p->min_x;
	else if ((int16_t)p->x > p->max_x - 0x1180)
		p->x = (uint16_t)(p->max_x - 0x1180);
}

static void fx_fire(Game *g)
{
	g->pl.fire = !g->pl.fire;
}

static void fx_slow(Game *g)
{
	int i;

	for (i = 0; i < MAX_BALLS; i++) {
		Ball *b = &g->balls[i];
		if (!b->speed)
			continue;
		b->speed = (uint16_t)(b->speed * 2 / 3);
		if (b->speed < g->diff.min_speed)
			b->speed = (uint16_t)g->diff.min_speed;
	}
}

static void fx_half_gems(Game *g)
{
	g->pl.gems >>= 1;
}

static void fx_multiball(Game *g)
{
	if (ball_spawn(g))
		g->bonus_msg = MSG_MULTIBALL_FULL;
}

static void fx_guard(Game *g)
{
	if (g->guard_timer < 0)
		return;
	g->guard_timer = -1;
	sound_play(SFX_GUARD_ON);
}

static void fx_hold(Game *g)
{
	g->pl.hold = !g->pl.hold;
}

static void fx_life(Game *g)
{
	if (g->pl.lives >= 8) {
		g->bonus_msg = MSG_LIVES_FULL;
		return;
	}
	g->pl.lives++;
	sound_play(SFX_EXTRA_LIFE);
}

static void fx_warp(Game *g)
{
	if (g->result > 0) {
		g->bonus_msg = MSG_WARP_SCORE;
		g->pl.score += 2000;
		return;
	}
	if (g->bonus_stage)
		g->bonus_msg = MSG_BONUS_WARP;
	g->result = LEVEL_WARP;
}

static void (*const bonus_effects[])(Game *) = {
	[BONUS_NONE] = fx_none,
	[BONUS_GEMS] = fx_gems,
	[BONUS_WIDE] = fx_wide,
	[BONUS_FIRE] = fx_fire,
	[BONUS_SLOW] = fx_slow,
	[BONUS_HALF_GEMS] = fx_half_gems,
	[BONUS_MULTIBALL] = fx_multiball,
	[BONUS_GUARD] = fx_guard,
	[BONUS_HOLD] = fx_hold,
	[BONUS_RANDOM] = fx_none, /* resolved in bonus_apply */
	[BONUS_LIFE] = fx_life,
	[BONUS_WARP] = fx_warp,
};

/* 1000:2720 */
void bonus_apply(Game *g, int type)
{
	if (g->pl.held_ball) {
		g->pl.held_ball = 0;
		g->pl.hold_timer = 0;
		sound_play(SFX_RELEASE);
	}
	g->pl.score += 100;
	g->bonus_msg_timer = 0x80;
	g->bonus_msg = BONUS_NONE;

	if (type == BONUS_NONE)
		return;
	g->pal.flash = 1;
	if (type == BONUS_RANDOM)
		type = ds_word((uint16_t)(DS_RANDOM_BONUS + rng(0x1d) * 2));
	if (type < 0 || type > BONUS_WARP)
		return;
	g->bonus_msg = type;
	bonus_effects[type](g);
}

/* 1000:2c32 */
void pickups_update(Game *g)
{
	Player *p = &g->pl;
	int i;

	for (i = 0; i < MAX_GEMS; i++) {
		Gem *m = &g->gems[i];
		int16_t right = (int16_t)(p->x + 0xb40 + (p->wide ? 0x500 : 0));

		if (!m->anim)
			continue;
		m->anim = m->anim + 0x3f >= 0x600 ? 1 : m->anim + 0x3f;

		if (m->y >= 0x2b80 && (int16_t)m->x >= (int16_t)(p->x - 0x280) && (int16_t)m->x <= right) {
			m->anim = 0;
			fx_flash(&g->fx, m->x, 0x2c40);
			sound_play(SFX_GEM);
			if (p->gems < 500)
				p->gems++;
			continue;
		}

		m->vy = m->vy + 5 > 0x80 ? 0x80 : m->vy + 5;
		if (m->vx) {
			m->vx = slow_down(m->vx);
			m->x += m->vx;
		}
		m->y += m->vy;
		if (m->y >= 0x2e40)
			m->anim = 0;
	}

	for (i = 0; i < MAX_CAPSULES; i++) {
		Capsule *c = &g->capsules[i];
		int16_t right = (int16_t)(p->x - 0x4c0 + 0x1000 + (p->wide ? 0x500 : 0));

		if (!c->anim)
			continue;
		c->anim = c->anim + 0x3f >= 0xa00 ? 0x401 : c->anim + 0x3f;

		if (c->y >= 0x2a80 && c->y <= 0x2e00 && (int16_t)c->x >= (int16_t)(p->x - 0x4c0) &&
		    (int16_t)c->x <= right) {
			sound_play(SFX_CAPSULE);
			bonus_apply(g, c->type);
			c->anim = 0;
			fx_flash(&g->fx, c->x + 0x100, 0x2c40);
			continue;
		}

		c->vy = c->vy + 5 > 0x40 ? 0x40 : c->vy + 5;
		if (c->vx) {
			c->vx = slow_down(c->vx);
			c->x += c->vx;
		}
		c->y += c->vy;
		if (c->y > 0x3180 || c->y <= -0x500)
			c->anim = 0;
	}

	bursts_update(g);
}

/* 1000:2c02 */
int pickups_active(const Game *g)
{
	int i;

	for (i = 0; i < MAX_GEMS; i++)
		if (g->gems[i].anim)
			return 1;
	for (i = 0; i < MAX_CAPSULES; i++)
		if (g->capsules[i].anim)
			return 1;
	return 0;
}

static void draw_capsule(const Capsule *c)
{
	const Sprite *s = sprite_get(SPR_CAPSULE);
	const int h = s->def->h;
	int y = c->y >> 6, x = c->x >> 6;
	int rows, skip = 0, i, j;
	const uint8_t *src;

	if (y < 0) {
		rows = h + y;
		skip = -y;
		y = 0;
	} else {
		rows = SCREEN_H - y < h ? SCREEN_H - y : h;
	}
	if (x > 0x128 || rows <= 0)
		return;

	src = sprite_frame(SPR_CAPSULE, c->anim >> 8) + skip * s->def->stride;
	for (j = 0; j < rows; j++)
		for (i = 0; i < s->def->w; i++) {
			uint8_t v = src[j * s->def->stride + i];
			if (v)
				gfx_fb[(y + j) * SCREEN_W + x + i] = (uint8_t)(v + s->def->base);
		}

	if (c->anim >= 0x700 && c->type) {
		int iy = (c->y >> 6) + 3;
		if (iy >= 0 && iy <= 0xb9)
			gfx_sprite(SPR_BONUS_ICON, c->type - 1, x + 4, iy);
	}
}

/* 1000:3ce6 */
void pickups_draw(const Game *g)
{
	int i;

	for (i = 0; i < MAX_GEMS; i++) {
		const Gem *m = &g->gems[i];
		int f;

		if (!m->anim)
			continue;
		f = m->anim >> 8;
		if (f >= 4)
			f = 6 - f;
		if ((m->y >> 6) > 0xb9 || (m->x >> 6) > 0x131)
			continue;
		gfx_sprite(SPR_GEM, f, m->x >> 6, m->y >> 6);
	}

	for (i = 0; i < MAX_CAPSULES; i++)
		if (g->capsules[i].anim)
			draw_capsule(&g->capsules[i]);
}
