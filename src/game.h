#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "assets.h"
#include "gfx.h"
#include "effects.h"

/* Gameplay state recovered from DS:8410..87F6 of CYBER.EXE.
 * Positions of balls, shots, gems, capsules and the paddle are in 1/64 px. */

#define MAX_BALLS 16
#define MAX_SHOTS 2
#define MAX_GEMS 24
#define MAX_CAPSULES 6
#define MAX_GEM_BURSTS 8
#define MAX_HITS_PER_TEST 32

#define PADDLE_Y_TOP 0x2bc0  /* 175 px: top of the paddle hit zone */
#define BALL_LOST_Y  0x3000  /* 192 px */

typedef enum {
	LEVEL_RUNNING = 0,
	LEVEL_CLEARED = 1,
	LEVEL_WARP = 2,      /* bonus 11: go to a bonus stage */
	LEVEL_LOST = -1,     /* every ball destroyed */
	LEVEL_QUIT = -2
} LevelResult;

typedef enum { DIFF_NOVICE, DIFF_NORMAL, DIFF_EXPERT } Difficulty;

/* 1000:1f14 */
typedef struct {
	int lives;        /* 87B0 */
	int start_speed;  /* 87B2 */
	int min_speed;    /* 87B4, floor of the slow bonus */
	int max_speed;    /* 87B6 */
	int accel_frames; /* 87B8, frames between speed-ups */
	int guard_frames; /* 87BA */
	int hold_frames;  /* 87BC, auto-release of a caught ball */
} DiffParams;

/* 85CA, 12 bytes each */
typedef struct {
	uint16_t x, y;
	uint8_t angle;   /* 0 = up, 0x40 = right; sine table at DS:59A4 */
	uint16_t speed;  /* 0 = slot unused */
	uint16_t dying;  /* frames since the ball fell out; explodes at 0xA0 */
	uint16_t tri;    /* touched a diagonal last step */
} Ball;

/* 878A / 878E */
typedef struct {
	uint16_t x, y; /* x == 0: slot unused */
} Shot;

/* 8492, 10 bytes each */
typedef struct {
	uint16_t x, y, anim;
	int16_t vx, vy;
} Gem;

/* 8582, 12 bytes each */
typedef struct {
	uint16_t x;
	int16_t y;
	uint16_t anim;
	int16_t vx, vy;
	uint16_t type;
} Capsule;

/* 8472: n gems popping out of a destroyed brick, one every 8 frames */
typedef struct {
	uint16_t counter, cell;
} GemBurst;

/* Player / paddle (8778..8789, 87A2..87A8) */
typedef struct {
	uint16_t x;         /* 8778 */
	int wide;           /* 877A */
	int fire;           /* 877C */
	int hold;           /* 877E */
	int held_ball;      /* 8780, 1-based ball index, 0 = none */
	int hold_off;       /* 8782 */
	int hold_dir;       /* 8784 */
	int hold_timer;     /* 8786 */
	uint8_t edge_cool_l, edge_cool_r; /* 8788/8789 */
	int min_x, max_x;   /* 879E/87A0 */
	uint32_t score;     /* 87A8 */
	int lives;          /* 87A6 */
	int gems;           /* 87A2 */
} Player;

/* One brick touched during a collision test (869A list). */
typedef struct {
	int cell;
	uint32_t entry; /* raw type table entry at hit time */
} BrickHit;

/* Results of 1000:35ba for one test point/ball. */
typedef struct {
	int solid;      /* 871A */
	int breakable;  /* 871C */
	int diagonal;   /* 871E: 1 or 2 = mirror type */
	int pass;       /* 8168 */
	int count;      /* 8696 */
	BrickHit hits[MAX_HITS_PER_TEST];
} HitTest;

typedef struct {
	int mouse_dx;     /* 70C6, 1/64 px */
	int button;       /* 70CA, fire button held */
	int click;        /* 70CE, fire button pressed this frame */
} Input;

typedef struct Game {
	DiffParams diff;
	Level lv;
	PaletteFx pal;
	Effects fx;
	Player pl;
	Input in;

	Ball balls[MAX_BALLS];
	Shot shots[MAX_SHOTS];
	Gem gems[MAX_GEMS];
	Capsule capsules[MAX_CAPSULES];
	GemBurst bursts[MAX_GEM_BURSTS];

	int result;          /* 8726 */
	int bonus_stage;     /* 872A */
	int spawn_delay;     /* 872C */
	int curtain;         /* 872E */
	int curtain_dir;     /* 8730 */
	int scan_row;        /* 8728 */
	int speedup_timer;   /* 8770 */
	int last_ball;       /* 8772 */
	int balls_saved;     /* 8774, multiball bonus counter */
	int survival_bonus;  /* 8776 */
	int bonus_msg_timer, bonus_msg; /* 8764/8766 */
	int status_timer, status_msg;   /* 876C/876E */
	int guard_timer;     /* 879C: <0 building, >0 active */
	uint8_t guard_type, guard_color; /* 879A/879B */
	int bonus_stages_played; /* 87AE */
	int life_dropped;    /* 87AC */
	int cheats;          /* 87F0 */
	int paused;

	/* brick state kept for a retry after a lost life or a bonus stage (DS:7BDA) */
	LevelData saved;
	uint8_t saved_guard_type, saved_guard_color;
	int has_saved;
} Game;

/* game.c */
void game_init(Game *g, Difficulty d);
int game_start_level(Game *g, int zone, int level, int restore);
void game_save_level(Game *g);
void game_frame(Game *g);
void game_draw(Game *g);
void game_key(Game *g, int key);
int game_level_done(const Game *g);

/* ball.c */
int ball_spawn(Game *g);
void balls_update(Game *g);
void ball_paddle_angle(Game *g, Ball *b);
void balls_remove_all(Game *g);

/* brick.c */
void brick_test(Game *g, HitTest *t, uint16_t x, uint16_t y);
void bricks_process_hits(Game *g, HitTest *t);
void brick_hit_effect(Game *g, const HitTest *t, int x, int y);
int bricks_scan_row(Game *g);

/* bonus.c */
void bonus_apply(Game *g, int type);
void bonus_drop(Game *g, int cell);
void gems_burst(Game *g, int cell, int count);
void pickups_update(Game *g);
int pickups_active(const Game *g);
void pickups_draw(const Game *g);

/* player.c */
void player_update(Game *g);
void shots_update(Game *g);
void player_draw(const Game *g);

#endif
