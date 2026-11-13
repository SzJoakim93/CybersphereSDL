#include "game.h"
#include "sound.h"

/* Brick type table flags (byte 3 of each DS:5CA4 entry) */
#define BT_BREAKABLE 0x01000000u /* registers hits */
#define BT_LINKED    0x02000000u /* part of a bigger brick; bits 26-27 point to the next part */
#define BT_SHAPE     0x20000000u /* diagonal deflector, only bounces the ball */
#define BT_EXPLODE   0x40000000u /* explosion when hit (for shapes: whole cell is a mirror) */
#define BT_SOLID     0x80000000u

#define ENTRY_NEXT(e)  (((e) >> 8) & 0xff)
#define ENTRY_SCORE(e) ((((e) >> 16) & 0xf) * 10)

/* Level bytes as the original addresses them (DS:70D8 + cell), reads past the grid included. */
static uint8_t *cell_ptr(Game *g, int cell)
{
	return (uint8_t *)&g->lv.bricks + 2 + (cell & 0x1ff);
}

/* 1000:35ba */
void brick_test(Game *g, HitTest *t, uint16_t px, uint16_t py)
{
	int shift = g->lv.bricks.shift != 0;
	int row = ((py >> 6) / 10) & 0x1f;
	int col = (((uint16_t)(px + (shift ? 0x280 : 0)) >> 6) / 20) & 0xf;
	int cell = row * GRID_COLS + col;
	uint32_t e = brick_entry(*cell_ptr(g, cell));

	if (e & BT_SHAPE) {
		int cl = (e >> 26) & 0x3f, lx, ly, sum, hit;

		if (e & BT_EXPLODE) {
			t->diagonal = (cl & 1) + 1;
			return;
		}
		lx = (int16_t)((px >> 6) - (col * BRICK_W - (shift ? 10 : 0)));
		ly = (int16_t)((py >> 6) - row * BRICK_H);
		if (cl & 4)
			lx -= 10;
		sum = lx + ly;
		switch (cl & 3) {
		case 0: hit = sum < 9; break;
		case 2: hit = sum > 9; break;
		case 1: hit = lx > ly; break;
		default: hit = lx < ly; break;
		}
		if (hit)
			t->diagonal = (cl & 1) + 1;
		return;
	}

	if (t->pass == 3 || !(e & BT_SOLID))
		return;
	t->solid = 1;
	if (!(e & BT_BREAKABLE))
		return;
	t->breakable = 1;

	for (;;) {
		int i;
		static const int link_step[4] = { -GRID_COLS, 1, GRID_COLS, -1 };

		for (i = 0; i < t->count; i++)
			if (t->hits[i].cell == cell)
				return;
		if (t->count >= MAX_HITS_PER_TEST)
			return;
		t->hits[t->count].cell = cell;
		t->hits[t->count].entry = e;
		t->count++;

		if (!(e & BT_LINKED))
			return;
		cell = (cell + link_step[(e >> 26) & 3]) & 0x1ff;
		e = brick_entry(*cell_ptr(g, cell));
	}
}

/* 1000:24fd */
void brick_hit_effect(Game *g, const HitTest *t, int x, int y)
{
	if (t->breakable)
		fx_flash(&g->fx, x - 0x80, y - 0x80);
	else
		fx_spark(&g->fx, x, y);
}

/* 1000:23a4 - one of four random explosion styles */
static void brick_explode(Game *g, int cell)
{
	int x = (cell & 15) * 0x500 - (g->lv.bricks.shift ? 0x280 : 0);
	int y = (cell >> 4) * 0x280;
	int vx, vy, speed, fric;

	if ((unsigned)cell > GRID_CELLS)
		return;

	switch (rng(0xb1) & 3) {
	case 0:
		fx_explosion(&g->fx, x - 0x180, y - 0x2c0, rng(0x1c) + 0xc);
		break;
	case 1:
		vx = rng(7) - 3;
		vy = rng(7) - 3;
		speed = rng(0x20) + 0x10;
		fx_debris(&g->fx, x + 0x80, y - 0xc0, vx, vy, speed, 0xff);
		break;
	case 2:
		vx = vy = rng(0x1f) + 0x10;
		fric = rng(0x2f) + 0xb7;
		speed = rng(0x20) + 0x10;
		fx_debris(&g->fx, x + 0x80, y - 0xc0, vx, vy, speed, fric);
		fx_debris(&g->fx, x + 0x80, y - 0xc0, -vx, vy, speed, fric);
		fx_debris(&g->fx, x + 0x80, y - 0xc0, -vx, -vy, speed, fric);
		fx_debris(&g->fx, x + 0x80, y - 0xc0, vx, -vy, speed, fric);
		break;
	default:
		vx = rng(0x27) + 6;
		vy = rng(7) - 3;
		speed = rng(0x20) + 0x10;
		fx_debris(&g->fx, x + 0x80, y - 0xc0, vx, vy, speed, 0xe6);
		fx_debris(&g->fx, x + 0x80, y - 0xc0, -vx, -vy, speed, 0xe6);
		break;
	}
	sound_play(SFX_BRICK_EXPLODE + rng(8));
}

/* 1000:26a6 - a destroyed brick may release gems and a bonus capsule */
static void brick_destroyed(Game *g, int cell)
{
	int n = ds_word((uint16_t)(0x60a4 + rng(0x44) * 2));

	if (n)
		gems_burst(g, cell, n);
	if (rng(0x79) <= 0x14)
		bonus_drop(g, cell);
}

/* 1000:3776 */
void bricks_process_hits(Game *g, HitTest *t)
{
	int i;

	if (!t->count)
		return;
	g->scan_row = 0;
	sound_play(SFX_BRICK_HIT);

	if (!ENTRY_NEXT(t->hits[0].entry))
		brick_destroyed(g, t->hits[0].cell);

	for (i = 0; i < t->count; i++) {
		uint32_t e = t->hits[i].entry;
		int cell = t->hits[i].cell;

		g->pl.score += ENTRY_SCORE(e);
		*cell_ptr(g, cell) = (uint8_t)ENTRY_NEXT(e);
		fx_brick_flash(&g->fx, cell);
		if (e & BT_EXPLODE)
			brick_explode(g, cell);
	}
	t->count = 0;
}

/* Part of 1000:2985: one grid row per frame is checked for bricks that still must be destroyed.
 * Returns 1 once 20 rows in a row were clean. */
int bricks_scan_row(Game *g)
{
	int c;

	for (c = 0; c < GRID_COLS; c++) {
		uint8_t type = *cell_ptr(g, g->scan_row * GRID_COLS + c);
		uint32_t e = brick_entry(type);

		if ((e & 0xff) && (e & BT_BREAKABLE) && ENTRY_NEXT(e) != type) {
			g->scan_row = 0;
			return 0;
		}
	}
	return ++g->scan_row == GRID_ROWS;
}
