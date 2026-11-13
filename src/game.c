#include <string.h>
#include "game.h"
#include "sound.h"

#define GUARD_ROW (19 * GRID_COLS)
#define PADDLE_ROW (18 * GRID_COLS)

/* 1000:1f14 */
static const DiffParams diff_table[3] = {
	{ 3, 0x18, 0x12, 0x50, 0x15e, 0x690, 0x2bc },
	{ 2, 0x20, 0x18, 0x6e, 0x0d2, 0x4ec, 0x118 },
	{ 2, 0x28, 0x1e, 0x78, 0x0d2, 0x348, 0x0d2 },
};

void game_init(Game *g, Difficulty d)
{
	memset(g, 0, sizeof(*g));
	g->diff = diff_table[d];
	g->pl.lives = g->diff.lives;
}

/* 1000:1fab */
int game_start_level(Game *g, int zone, int level, int restore)
{
	Level *lv = &g->lv;
	LevelData *b = &lv->bricks;
	Player *p = &g->pl;
	int i;

	g->bonus_stage = zone == 0;
	if (!stage_params(stage_param_index(zone, level), &lv->params))
		return 0;
	if (restore && g->has_saved) {
		*b = g->saved;
		g->guard_type = g->saved_guard_type;
		g->guard_color = g->saved_guard_color;
	} else if (!layout_load(lv->params.layout, b)) {
		return 0;
	}
	layer2_load(lv->params.layer2, &lv->layer2);

	p->wide = 0;
	p->fire = 0;
	p->hold = 0;
	p->gems = 0;
	g->guard_timer = 0;
	if (!restore || !g->has_saved) {
		g->guard_type = 0;
		g->guard_color = 0;
	}

	/* the guard brick is whatever sits in the middle of the bottom row */
	if (!g->guard_type) {
		g->guard_type = b->type[GUARD_ROW + 8];
		g->guard_color = b->color[GUARD_ROW + 8];
	}
	for (i = 0; i < GRID_COLS; i++)
		if (b->type[GUARD_ROW + i] == g->guard_type)
			b->type[GUARD_ROW + i] = 0;

	palfx_init(&g->pal, lv);

	/* the paddle may travel over the empty cells of row 18 */
	p->min_x = 0x22c0;
	for (i = 6; i >= 1 && !b->type[PADDLE_ROW + i]; i--)
		p->min_x -= 0x500;
	p->max_x = 0x2d40;
	for (i = 9; i <= 15 && !b->type[PADDLE_ROW + i]; i++)
		p->max_x += 0x500;
	if (b->shift) {
		p->min_x -= 0x280;
		p->max_x -= 0x280;
	}
	p->x = (uint16_t)((((uint16_t)(p->max_x - p->min_x - 0xc80)) >> 1) + p->min_x);
	if (p->wide)
		p->x -= 0x280;
	p->edge_cool_l = p->edge_cool_r = 0x3c;
	p->held_ball = 0;
	p->hold_timer = 0;

	memset(g->balls, 0, sizeof(g->balls));
	memset(g->shots, 0, sizeof(g->shots));
	memset(g->gems, 0, sizeof(g->gems));
	memset(g->capsules, 0, sizeof(g->capsules));
	memset(g->bursts, 0, sizeof(g->bursts));
	fx_reset(&g->fx);

	g->result = LEVEL_RUNNING;
	g->spawn_delay = 0x50;
	g->curtain = 0x28;
	g->curtain_dir = -1;
	g->speedup_timer = g->diff.accel_frames;
	g->scan_row = 0;
	g->last_ball = 0;
	g->balls_saved = 0;
	g->bonus_msg_timer = 0;
	g->status_timer = 0xa0;
	g->status_msg = g->bonus_stage ? 1 : 0;
	g->paused = 0;
	memset(&g->in, 0, sizeof(g->in));
	sound_music(lv->params.music);
	return 1;
}

/* end of 1000:1d30 */
void game_save_level(Game *g)
{
	if (g->bonus_stage)
		return;
	g->saved = g->lv.bricks;
	g->saved_guard_type = g->guard_type;
	g->saved_guard_color = g->guard_color;
	g->has_saved = 1;
}

int game_level_done(const Game *g)
{
	return g->result == LEVEL_QUIT || g->curtain > 0x29;
}

/* 1000:2b8d - balls inside a new guard brick are pushed out to y */
static void guard_push_balls(Game *g, int cell, uint16_t y)
{
	uint16_t bx = (uint16_t)((cell & 15) * 0x500 - (g->lv.bricks.shift ? 0x280 : 0));
	uint16_t by = (uint16_t)((cell >> 4) * 0x280);
	int i;

	for (i = 0; i < MAX_BALLS; i++) {
		Ball *b = &g->balls[i];
		uint16_t dx = (uint16_t)(b->x + 0x240), dy = (uint16_t)(b->y + 0x240);

		if (!b->speed)
			continue;
		if (dx < bx || (uint16_t)(dx - 0x740) > bx)
			continue;
		if (dy < by || (uint16_t)(dy - 0x4c0) > by)
			continue;
		b->y = y;
	}
}

/* Guard part of 1000:2985: built brick by brick, then removed brick by brick near the end */
static void guard_update(Game *g)
{
	uint8_t *type = g->lv.bricks.type;

	if (g->guard_timer < 0) {
		int n;

		if (g->guard_timer == -0x3c) {
			g->guard_timer = g->diff.guard_frames;
			return;
		}
		n = -(--g->guard_timer);
		if (n & 3)
			return;
		n = GUARD_ROW + (n >> 2);
		if (type[n] != g->guard_type) {
			if (type[n])
				return;
			type[n] = g->guard_type;
			g->lv.bricks.color[n] = g->guard_color;
		}
		fx_brick_flash(&g->fx, n);
		guard_push_balls(g, n, 0x2d00);
	} else if (g->guard_timer > 0) {
		int a = 0xf0 - --g->guard_timer;

		if (a < 0x10 || (a & 0xf))
			return;
		a = (a >> 4) * 2;
		if (a >= 0x10)
			a -= 0xf;
		if (type[GUARD_ROW + a] != g->guard_type)
			return;
		type[GUARD_ROW + a] = 0;
		fx_brick_flash(&g->fx, GUARD_ROW + a);
		sound_play(SFX_GUARD_OFF);
	}
}

/* 1000:2985 */
static void rules_update(Game *g)
{
	int i, any = 0;

	if (g->bonus_msg_timer)
		g->bonus_msg_timer--;
	if (g->status_timer)
		g->status_timer--;

	guard_update(g);

	if (!--g->speedup_timer) {
		g->speedup_timer = g->diff.accel_frames;
		for (i = 0; i < MAX_BALLS; i++) {
			Ball *b = &g->balls[i];
			if (b->speed && b->speed < g->diff.max_speed && !b->dying)
				b->speed++;
		}
	}

	g->curtain += g->curtain_dir;
	if (g->curtain < -100) {
		g->curtain_dir = 0;
		g->curtain = -98;
	}

	if (g->spawn_delay == 1)
		ball_spawn(g);
	if (g->spawn_delay) {
		g->spawn_delay--;
	} else {
		for (i = 0; i < MAX_BALLS; i++)
			if (g->balls[i].speed)
				any = 1;
		if (!any && g->result == LEVEL_RUNNING)
			g->result = LEVEL_LOST;
	}

	if (bricks_scan_row(g) && g->result <= 0)
		g->result = LEVEL_CLEARED;

	/* level over: clear the balls, wait for falling items, close the curtain */
	if (!g->spawn_delay && g->result != LEVEL_RUNNING && g->curtain_dir != 1) {
		if (g->result != LEVEL_LOST) {
			balls_remove_all(g);
			if (pickups_active(g))
				return;
		}
		g->curtain = -40;
		g->curtain_dir = 1;
	}
}

/* Body of the play loop at 1000:1d44 */
void game_frame(Game *g)
{
	if (g->paused)
		return;
	if (g->last_ball) {
		if (g->last_ball == 1)
			sound_play(SFX_LAST_BALL);
		/* frozen while FATAL BALL blinks */
		if (++g->last_ball >= 0x54)
			g->last_ball = 0;
		g->in.click = 0;
		return;
	}

	balls_update(g);
	player_update(g);
	balls_update(g);
	shots_update(g);
	fx_update(&g->fx);
	pickups_update(g);
	g->pal.fire = g->pl.fire;
	palfx_step(&g->pal);
	rules_update(g);
}

/* 1000:3c28 */
static void draw_fatal_ball(void)
{
	static const int line1[] = { 0, 1, 2, 1, 3 }, line2[] = { 4, 1, 3, 3 };
	int i;

	for (i = 0; i < 5; i++)
		gfx_sprite(SPR_BIG_LETTER, line1[i], 93 + i * 27, 66);
	for (i = 0; i < 4; i++)
		gfx_sprite(SPR_BIG_LETTER, line2[i], 106 + i * 27, 101);
}

void game_draw(Game *g)
{
	int i;

	if (!g->paused)
		palfx_apply(&g->pal);
	gfx_bricks(&g->lv);
	player_draw(g);
	pickups_draw(g);
	fx_draw(&g->fx, g->lv.bricks.shift);

	for (i = 0; i < MAX_SHOTS; i++)
		if (g->shots[i].x)
			gfx_sprite(SPR_SHOT, 0, g->shots[i].x >> 6, g->shots[i].y >> 6);
	for (i = 0; i < MAX_BALLS; i++)
		if (g->balls[i].speed)
			gfx_sprite(SPR_BALL, 0, g->balls[i].x >> 6, g->balls[i].y >> 6);

	if ((g->bonus_msg_timer & 0x1f) >= 0x10)
		gfx_message(game_message(0, g->bonus_msg));
	if ((g->status_timer & 0x1f) >= 0x10)
		gfx_message(game_message(1, g->status_msg));
	gfx_hud(g->pl.score, g->pl.gems, g->pl.lives);

	if (g->last_ball && (g->last_ball & 0x1f) <= 0x14)
		draw_fatal_ball();
	if (g->paused)
		gfx_sprite(SPR_PAUSED, 0, 79, 84);
}

/* Keys handled inside the play loop */
void game_key(Game *g, int key)
{
	if (key == 27) {
		g->result = LEVEL_QUIT;
		return;
	}
	if (key == ' ' || key == 'p') {
		g->paused = !g->paused;
		g->in.click = 0;
		sound_play(SFX_PAUSE);
		return;
	}
	if (!g->cheats || g->paused)
		return;
	if (key == 'q')
		g->result = LEVEL_CLEARED;
	else if (key == 'b')
		g->result = LEVEL_WARP;
	else if (key == '1')
		ball_spawn(g);
}
