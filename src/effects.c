#include <string.h>
#include "effects.h"
#include "gfx.h"

static uint16_t rng_state;

void rng_seed(uint16_t seed)
{
	rng_state = seed;
}

int rng(int range)
{
	rng_state = (uint16_t)(rng_state * 0x24a1 + 0x24df);
	return (rng_state & 0x7fff) % range;
}

void fx_reset(Effects *e)
{
	memset(e, 0, sizeof(*e));
}

/* 1000:2268 */
void fx_spark(Effects *e, int x, int y)
{
	Spark *s;

	if ((unsigned)x >> 6 >= 0x135 || (unsigned)y >> 6 >= 0xbd)
		return;
	e->spark_next = (e->spark_next + 1) % MAX_SPARKS;
	s = &e->sparks[e->spark_next];
	s->x = (int16_t)(x >> 6);
	s->y = (int16_t)(y >> 6);
	s->anim = 1;
}

/* 1000:22a4 */
void fx_flash(Effects *e, int x, int y)
{
	Spark *s;

	if ((unsigned)x >> 6 >= 0x131 || (unsigned)y >> 6 >= 0xb9)
		return;
	e->flash_next = (e->flash_next + 1) % MAX_FLASHES;
	s = &e->flashes[e->flash_next];
	s->x = (int16_t)(x >> 6);
	s->y = (int16_t)(y >> 6);
	s->anim = 1;
}

/* 1000:22e0 */
void fx_debris(Effects *e, int x, int y, int vx, int vy, int speed, int friction)
{
	Debris *d;

	e->debris_next = (e->debris_next + 1) % MAX_DEBRIS;
	d = &e->debris[e->debris_next];
	d->x = (uint16_t)(x >> 2);
	d->y = (uint16_t)(y >> 2);
	d->anim = 1;
	d->speed = (uint16_t)speed;
	d->vx = (int16_t)vx;
	d->vy = (int16_t)vy;
	d->friction = (uint16_t)friction;
}

/* 1000:232c */
void fx_explosion(Effects *e, int x, int y, int speed)
{
	Explosion *x_;

	if ((unsigned)x >> 6 >= 0x120 || (unsigned)y >> 6 >= 0xa8)
		return;
	e->explosion_next = (e->explosion_next + 1) % MAX_EXPLOSIONS;
	x_ = &e->explosions[e->explosion_next];
	x_->x = (int16_t)(x >> 6);
	x_->y = (int16_t)(y >> 6);
	x_->anim = 1;
	x_->speed = (uint16_t)speed;
}

/* 1000:2372 */
void fx_brick_flash(Effects *e, int cell)
{
	BrickFlash *f;

	if ((unsigned)cell > GRID_CELLS)
		return;
	e->brick_flash_next = (e->brick_flash_next + 1) % MAX_BRICK_FLASHES;
	f = &e->brick_flashes[e->brick_flash_next];
	f->timer = 5;
	f->cell = (uint16_t)cell;
}

static uint16_t advance(uint16_t anim, int step, uint16_t limit)
{
	unsigned v;

	if (!anim)
		return 0;
	v = anim + step;
	return v >= limit ? 0 : (uint16_t)v;
}

void fx_update(Effects *e)
{
	int i;

	for (i = 0; i < MAX_SPARKS; i++)
		e->sparks[i].anim = advance(e->sparks[i].anim, 0x40, 0x500);
	for (i = 0; i < MAX_FLASHES; i++)
		e->flashes[i].anim = advance(e->flashes[i].anim, 0x60, 0x800);

	for (i = 0; i < MAX_DEBRIS; i++) {
		Debris *d = &e->debris[i];
		if (!d->anim)
			continue;
		d->anim = advance(d->anim, d->speed, 0x600);
		d->x += d->vx;
		d->y += d->vy;
		d->vx = (int16_t)((d->vx * (int)d->friction + 0x80) >> 8);
		d->vy = (int16_t)((d->vy * (int)d->friction + 0x80) >> 8);
	}

	for (i = 0; i < MAX_EXPLOSIONS; i++)
		e->explosions[i].anim = advance(e->explosions[i].anim, e->explosions[i].speed, 0x600);

	for (i = 0; i < MAX_BRICK_FLASHES; i++)
		if (e->brick_flashes[i].timer)
			e->brick_flashes[i].timer--;
}

void fx_draw(const Effects *e, int shift)
{
	int i;

	for (i = 0; i < MAX_BRICK_FLASHES; i++)
		if (e->brick_flashes[i].timer)
			gfx_brick_flash(e->brick_flashes[i].cell, shift);

	for (i = 0; i < MAX_SPARKS; i++)
		if (e->sparks[i].anim)
			gfx_sprite(SPR_SPARK, e->sparks[i].anim >> 8, e->sparks[i].x, e->sparks[i].y);

	for (i = 0; i < MAX_FLASHES; i++)
		if (e->flashes[i].anim)
			gfx_sprite(SPR_FLASH, e->flashes[i].anim >> 8, e->flashes[i].x, e->flashes[i].y);

	for (i = 0; i < MAX_DEBRIS; i++) {
		const Debris *d = &e->debris[i];
		if (!d->anim || (d->y >> 4) > 0xb8 || (d->x >> 4) > 0x130)
			continue;
		gfx_sprite(SPR_DEBRIS, d->anim >> 8, d->x >> 4, d->y >> 4);
	}

	for (i = 0; i < MAX_EXPLOSIONS; i++)
		if (e->explosions[i].anim)
			gfx_sprite(SPR_EXPLOSION, e->explosions[i].anim >> 8, e->explosions[i].x,
				   e->explosions[i].y);
}
