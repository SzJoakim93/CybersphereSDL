#include "game.h"
#include "sound.h"

#define SHOT_Y 0x2b80 /* 174 px */

/* 1000:397b */
static void shot_fire(Game *g, Shot *s, uint16_t x)
{
	fx_debris(&g->fx, x - 0x100, SHOT_Y, 0, -16, 0x40, 0xf5);
	if (s->x)
		return;
	s->x = x;
	s->y = SHOT_Y;
	sound_play(SFX_SHOT);
}

static int any_ball(const Game *g)
{
	int i;

	for (i = 0; i < MAX_BALLS; i++)
		if (g->balls[i].speed)
			return 1;
	return 0;
}

/* 1000:37e0 */
void player_update(Game *g)
{
	Player *p = &g->pl;
	int dx = g->in.mouse_dx;
	int x, right;

	if (p->hold_timer && !--p->hold_timer) {
		p->held_ball = 0;
		sound_play(SFX_RELEASE);
	}

	if (dx > 0x2000)
		dx = 0x2000;
	if (dx < -0x2000)
		dx = -0x2000;
	x = (int16_t)(p->x + dx);

	if (x < p->min_x) {
		x = p->min_x;
		if (p->edge_cool_l <= 3) {
			fx_spark(&g->fx, p->min_x - 0x140, 0x2e40);
			sound_play(SFX_PADDLE_EDGE);
		}
		p->edge_cool_l = 0x3c;
	}
	right = p->max_x - 0xc80 - (p->wide ? 0x500 : 0);
	if (x > right) {
		x = right;
		if (p->edge_cool_r <= 3) {
			fx_spark(&g->fx, p->max_x - 0x180, 0x2e40);
			sound_play(SFX_PADDLE_EDGE);
		}
		p->edge_cool_r = 0x3c;
	}
	p->x = (uint16_t)x;

	if (p->edge_cool_l < 0x3c)
		p->edge_cool_l++;
	if (p->edge_cool_r < 0x3c)
		p->edge_cool_r++;
	if ((unsigned)(p->min_x + 0x200) <= p->x)
		p->edge_cool_l = 0;
	if ((unsigned)(right - 0x200) >= p->x)
		p->edge_cool_r = 0;

	if (g->in.click && p->held_ball) {
		g->in.click = 0;
		p->held_ball = 0;
		p->hold_timer = 0;
		sound_play(SFX_RELEASE);
	}

	if (g->in.click && p->fire && any_ball(g)) {
		g->in.click = 0;
		shot_fire(g, &g->shots[0], p->x + 0x180);
		shot_fire(g, &g->shots[1], p->x + 0x940 + (p->wide ? 0x500 : 0));
	}
	g->in.click = 0;
}

/* 1000:3034 */
void shots_update(Game *g)
{
	int i;

	for (i = 0; i < MAX_SHOTS; i++) {
		Shot *s = &g->shots[i];
		HitTest t;

		if (!s->x)
			continue;
		t.count = 0;
		t.pass = 0;
		t.solid = t.breakable = t.diagonal = 0;
		brick_test(g, &t, s->x + 0xc0, s->y);

		if (t.solid || t.diagonal) {
			int px = s->x + 0xc0 - 0x140, py = s->y - 0x80;
			s->x = s->y = 0;
			brick_hit_effect(g, &t, px, py);
			sound_play(SFX_BOUNCE);
			bricks_process_hits(g, &t);
			continue;
		}
		if ((int16_t)(s->y -= 0x280) < 0)
			s->x = s->y = 0;
	}
}

/* 1000:4101 */
void player_draw(const Game *g)
{
	const Player *p = &g->pl;
	PaddleView v;

	v.x = p->x >> 6;
	v.wide = p->wide;
	v.fire = p->fire;
	v.hold = p->hold;
	v.holding = p->held_ball != 0;
	gfx_paddle(&v);
}
