#include "game.h"
#include "sound.h"

/* 1000:3477 - launch angle from where the ball touches the paddle */
void ball_paddle_angle(Game *g, Ball *b)
{
	int off = (uint16_t)(b->x + 0x200 - g->pl.x) >> 6;
	int8_t a;

	if (g->pl.wide)
		a = (int8_t)((int8_t)((int8_t)(off - 0x26) * 3) >> 1);
	else
		a = (int8_t)((off - 0x1c) * 2);
	if (a < -0x34)
		a = -0x34;
	if (a > 0x34)
		a = 0x34;
	b->angle = (uint8_t)a;
}

/* 1000:26f1 - the game pauses with FATAL BALL when only one ball is left */
static void check_last_ball(Game *g)
{
	int i, alive = 0;

	for (i = 0; i < MAX_BALLS; i++)
		if (g->balls[i].speed && !g->balls[i].dying)
			alive++;
	if (alive == 1)
		g->last_ball = 1;
}

/* 1000:21fc - new ball held on the paddle; returns 1 if no slot was free */
int ball_spawn(Game *g)
{
	int i;

	if (g->result != LEVEL_RUNNING)
		return 0;
	for (i = 0; i < MAX_BALLS; i++) {
		Ball *b = &g->balls[i];
		if (b->speed)
			continue;
		b->x = (uint16_t)(g->pl.x + 0x500);
		b->y = PADDLE_Y_TOP;
		b->angle = 0xe8;
		b->speed = (uint16_t)g->diff.start_speed;
		b->dying = 0;
		g->pl.held_ball = i + 1;
		g->pl.hold_off = 0x500;
		g->pl.hold_dir = 0x20;
		g->pl.hold_timer = g->diff.hold_frames;
		return 0;
	}
	return 1;
}

/* 1000:2524 - level over: every ball turns into a flash and counts for the multiball bonus */
void balls_remove_all(Game *g)
{
	int i;

	for (i = 0; i < MAX_BALLS; i++) {
		Ball *b = &g->balls[i];
		if (b->speed) {
			fx_flash(&g->fx, b->x - 0xc0, b->y - 0xc0);
			g->balls_saved++;
		}
		b->x = b->y = 0;
		b->angle = 0;
		b->speed = 0;
		b->dying = 0;
	}
	g->pl.held_ball = 0;
	g->pl.hold_timer = 0;
}

/* 1000:34b7. pass 0 moves, 1 tests x only, 2 tests y only, 3 tests both for diagonals. */
static void ball_pass(Game *g, Ball *b, int pass, HitTest *t, int *wrapped)
{
	uint16_t x = b->x, y = b->y;

	t->solid = t->breakable = t->diagonal = 0;

	if (pass != 2) {
		int16_t nx = (int16_t)(x + ((int16_t)(sine(b->angle) * b->speed + 0x20) >> 6));
		if (nx <= 0) {
			nx += 0x4dc0;
			*wrapped = 1;
		}
		if (nx >= 0x4e00) {
			nx -= 0x4dc0;
			*wrapped = 1;
		}
		x = (uint16_t)nx;
		if (pass == 0)
			b->x = x;
	}

	if (pass != 1) {
		int16_t ny = (int16_t)(y + ((int16_t)(sine(b->angle - 0x40) * b->speed + 0x20) >> 6));
		if (ny >= BALL_LOST_Y) {
			sound_play(SFX_BALL_LOST);
			b->speed = 0xc;
			b->angle = (uint8_t)(rng(0x54) - 0x2a);
			b->y = 0x2f00;
			if (!b->dying) {
				b->dying = 1;
				check_last_ball(g);
			}
			return;
		}
		y = (uint16_t)ny;
		if (pass == 0) {
			b->y = y;
			return;
		}
	}

	t->pass = pass;
	brick_test(g, t, x, y);
	brick_test(g, t, x + 0x200, y);
	brick_test(g, t, x + 0x200, y + 0x200);
	brick_test(g, t, x, y + 0x200);
}

/* Ball currently stuck to the paddle slides from side to side (1000:3182). */
static void ball_held(Game *g, Ball *b)
{
	Player *p = &g->pl;
	int off = p->hold_off + p->hold_dir;

	if (off <= 0xc0) {
		off = 0xc0;
		p->hold_dir = 0x20;
	}
	if (off >= 0xebf) {
		off = 0xe80;
		p->hold_dir = -0x20;
	}
	if (!p->wide && off >= 0x9bf) {
		off = 0x980;
		p->hold_dir = -0x20;
	}
	p->hold_off = off;
	b->y = PADDLE_Y_TOP;
	b->x = (uint16_t)((p->x & 0xffc0) + off);
	b->angle = 0x80;
	ball_paddle_angle(g, b);
}

/* Paddle contact of a moving ball; returns 1 if it got caught (hold bonus). */
static int ball_paddle(Game *g, Ball *b, int index)
{
	Player *p = &g->pl;
	unsigned right = p->x + 0xc40 + (p->wide ? 0x500 : 0);

	if (b->y < PADDLE_Y_TOP || b->y > 0x2e00)
		return 0;
	if ((uint8_t)(b->angle - 0x40) > 0x80)
		return 0;
	if (p->x > (uint16_t)(b->x + 0x200) || b->x > (uint16_t)right)
		return 0;

	if (!g->in.button && p->hold && !p->held_ball && !b->dying) {
		p->held_ball = index + 1;
		p->hold_off = (b->x - p->x) & 0xffc0;
		p->hold_dir = b->angle >= 0x80 ? -0x20 : 0x20;
		p->hold_timer = g->diff.hold_frames;
		sound_play(SFX_CATCH);
		ball_held(g, b);
		return 1;
	}

	fx_spark(&g->fx, b->x - 0x40, 0x2cc0);
	sound_play(SFX_PADDLE);
	ball_paddle_angle(g, b);
	return 0;
}

/* 1000:3163 - one movement step of every ball (the game runs it twice per frame) */
void balls_update(Game *g)
{
	int i;

	for (i = 0; i < MAX_BALLS; i++) {
		Ball *b = &g->balls[i];
		HitTest t;
		int prev_tri = b->tri, bounced = 0, wrapped = 0;
		int px, py;

		b->tri = 0;

		if (g->pl.held_ball == i + 1) {
			ball_held(g, b);
			continue;
		}
		if (!b->speed)
			continue;
		if (ball_paddle(g, b, i))
			continue;

		t.count = 0;

		ball_pass(g, b, 1, &t, &wrapped);
		if (t.solid) {
			bounced = 1;
			px = b->x - 0x180;
			py = b->y - 0x40;
			b->angle = (uint8_t)-b->angle;
			if ((int8_t)b->angle <= 0)
				px += 0x280;
			brick_hit_effect(g, &t, px, py);
			sound_play(SFX_BOUNCE);
		}

		ball_pass(g, b, 2, &t, &wrapped);
		if (t.solid) {
			int8_t ah = (int8_t)-(uint8_t)(b->angle + 0x40);
			bounced = 1;
			px = b->x - 0x40;
			py = b->y - 0x180;
			b->angle = (uint8_t)(ah - 0x40);
			if (ah >= 0)
				py += 0x280;
			brick_hit_effect(g, &t, px, py);
			sound_play(SFX_BOUNCE);
		}

		ball_pass(g, b, 3, &t, &wrapped);
		if (t.diagonal) {
			int c = t.diagonal == 1 ? 0x60 : 0x20;
			int8_t ah = (int8_t)-(uint8_t)(b->angle + c);
			bounced = 1;
			px = b->x - 0x140;
			py = b->y - 0x140;
			b->angle = (uint8_t)(ah - c);
			if (c == 0x20) {
				if (ah <= 0)
					px += 0x200;
				else
					py += 0x200;
			} else if (ah >= 0) {
				px += 0x200;
				py += 0x200;
			}
			b->tri = 1;
			if (!prev_tri) {
				fx_spark(&g->fx, px, py);
				sound_play(SFX_BOUNCE);
			}
		}

		ball_pass(g, b, 0, &t, &wrapped);
		if (wrapped || bounced)
			b->angle = (uint8_t)(b->angle + rng(3) - 1);

		bricks_process_hits(g, &t);

		if (b->dying) {
			b->dying++;
			if (b->dying == 0xa0) {
				fx_explosion(&g->fx, b->x - 0x2c0, b->y - 0x2c0, 0x18);
				sound_play(SFX_BALL_EXPLODE);
				b->x = b->y = 0;
				b->angle = 0;
				b->speed = 0;
				b->dying = 0;
			} else if (!(b->dying & 0xf)) {
				fx_debris(&g->fx, b->x - 0x100, b->y - 0x100, 0, 0, 0x1c, 0xff);
				sound_play(SFX_BALL_SMOKE);
			}
		}
	}
}
