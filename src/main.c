#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include "assets.h"
#include "gfx.h"
#include "game.h"
#include "screens.h"
#include "video.h"
#include "menu.h"
#include "sound.h"

#define FPS 70 /* mode 13h refresh, the game is vsync-locked */
#define KEY_SPEED 0x180

static const char *exe_candidates[] = {
	"CYBER.EXE"
};

/* Levels per zone; zone 0 holds the bonus stages (checks at 1000:028b) */
static const int zone_levels[6] = { 10, 9, 8, 7, 8, 9 };

typedef enum { NEXT_PLAY, NEXT_PLAY_RESTORE, NEXT_PLAY_BONUS, NEXT_ADVANCE, NEXT_NEW_GAME, NEXT_MENU } Next;

typedef struct {
	Game g;
	Menu menu;
	int menu_active, direct_start;
	Difficulty diff;
	int cheats, autoplay;
	int zone, level;        /* 87D2/87D4 */
	int start_zone, start_level;
	int bonus_index;        /* 87D6 */
	int in_bonus;
	int saved_bonus_played;
	int playing;
	uint16_t block1, block2; /* text screen */
	int fade, fade_dir;
	Next next;
	int sheet;
	int mouse_x, mouse_y;
} App;

static void open_menu(App *a, MenuPage page)
{
	a->playing = 0;
	a->menu_active = 1;
	menu_open(&a->menu, page);
	SDL_WM_GrabInput(SDL_GRAB_OFF);
	SDL_ShowCursor(SDL_ENABLE);
}

static void show_screen(App *a, uint16_t block1, uint16_t block2, Next next)
{
	a->playing = 0;
	a->menu_active = 0;
	a->block1 = block1;
	a->block2 = block2;
	a->fade = 0;
	a->fade_dir = 0x10;
	a->next = next;
	SDL_WM_GrabInput(SDL_GRAB_OFF);
}

static void start_play(App *a, int restore, int bonus)
{
	Game *g = &a->g;

	a->in_bonus = bonus;
	if (bonus) {
		a->saved_bonus_played = g->bonus_stages_played;
		g->bonus_stages_played = 0;
	}
	if (!game_start_level(g, bonus ? 0 : a->zone, bonus ? a->bonus_index : a->level, restore)) {
		fprintf(stderr, "level %d-%d could not be loaded\n", a->zone, a->level + 1);
		exit(1);
	}
	g->cheats = a->cheats;
	a->playing = 1;
	a->menu_active = 0;
	SDL_ShowCursor(SDL_DISABLE);
	SDL_WM_GrabInput(a->autoplay ? SDL_GRAB_OFF : SDL_GRAB_ON);
}

static void show_intro(App *a)
{
	text_score(TXT_LEVEL_SCORE, a->g.pl.score);
	*text_at(TXT_LEVEL_ZONE) = (uint8_t)('0' + a->zone);
	*text_at(TXT_LEVEL_NUMBER) = (uint8_t)('1' + a->level);
	show_screen(a, TXT_LEVEL, TXT_PRESS_FIRE, NEXT_PLAY);
}

/* 1000:0129 */
static void new_game(App *a)
{
	a->diff = a->menu.difficulty;
	game_init(&a->g, a->diff);
	text_reset();
	a->zone = a->start_zone;
	a->level = a->start_level;
	a->bonus_index = rng(10);
	if (a->direct_start || a->autoplay)
	show_intro(a);
	else
		open_menu(a, MENU_SECTORS);
}

static void advance(App *a)
{
	Game *g = &a->g;

	if (++a->level < zone_levels[a->zone]) {
		show_intro(a);
		return;
	}
	a->menu.completed |= 1u << (a->zone - 1);
	if (!menu_save(&a->menu, exe_config_path()))
		fprintf(stderr, "Could not save completed sectors.\n");
	a->level = 0;
	g->life_dropped = 0;
	g->bonus_stages_played = 0;
	g->has_saved = 0;
	if (menu_all_completed(&a->menu)) {
		text_score(a->diff == DIFF_NOVICE ? 0x66eb : 0x6618, g->pl.score);
		show_screen(a, a->diff == DIFF_NOVICE ? 0x6622 : 0x657d, TXT_PRESS_FIRE, NEXT_MENU);
		return;
	}
	if (a->autoplay) {
		if (++a->zone > 5)
			a->zone = 1;
	show_intro(a);
	} else {
		open_menu(a, MENU_SECTORS);
	}
}

/* Result handling of the main loop at 1000:01cb / 1000:03a2 */
static void level_finished(App *a)
{
	Game *g = &a->g;
	int result = g->result;

	SDL_WM_GrabInput(SDL_GRAB_OFF);
	if (result == LEVEL_QUIT) {
		open_menu(a, MENU_MAIN);
		return;
	}
	game_save_level(g);

	if (a->in_bonus) {
		g->bonus_stages_played = a->saved_bonus_played;
		g->survival_bonus = result == LEVEL_CLEARED ? 5000 : result == LEVEL_WARP ? 2500 : 0;
		a->bonus_index = (a->bonus_index + 1) % 10;
		text_tally(g, 0x6f, a->bonus_index);
		show_screen(a, TXT_SURVIVAL, TXT_TALLY, NEXT_PLAY_RESTORE);
		return;
	}

	switch (result) {
	case LEVEL_LOST:
		if (!g->pl.lives) {
			text_score(TXT_GAME_OVER_SCORE, g->pl.score);
			show_screen(a, TXT_GAME_OVER, TXT_PRESS_FIRE, NEXT_MENU);
			return;
		}
		*text_at(TXT_LIVES_DIGIT) = (uint8_t)('0' + g->pl.lives);
		g->pl.lives--;
		show_screen(a, TXT_LIVES, TXT_PRESS_FIRE, NEXT_PLAY_RESTORE);
		break;
	case LEVEL_WARP:
		g->bonus_stages_played++;
		g->survival_bonus = 0;
		text_tally(g, 0x5b, a->bonus_index);
		show_screen(a, TXT_ENTER_BONUS, TXT_TALLY, NEXT_PLAY_BONUS);
		break;
	default:
		g->survival_bonus = 0;
		text_tally(g, 0x6f, a->bonus_index);
		show_screen(a, TXT_STAGE_COMPLETE, TXT_TALLY, NEXT_ADVANCE);
		break;
	}
}

static void run_next(App *a)
{
	switch (a->next) {
	case NEXT_PLAY: start_play(a, 0, 0); break;
	case NEXT_PLAY_RESTORE: start_play(a, 1, 0); break;
	case NEXT_PLAY_BONUS: start_play(a, 0, 1); break;
	case NEXT_ADVANCE: advance(a); break;
	case NEXT_NEW_GAME: new_game(a); break;
	case NEXT_MENU:
		if (a->autoplay) {
			new_game(a);
			break;
		}
		open_menu(a, MENU_MAIN);
		if (!a->autoplay)
			menu_submit_score(&a->menu, a->g.pl.score, a->diff);
		break;
	}
}

static void menu_action(App *a, MenuAction action)
{
	if (!menu_save(&a->menu, exe_config_path()))
		fprintf(stderr, "Could not save SDL menu settings.\n");
	if (action == MENU_EXIT) {
		SDL_Quit();
		exit(0);
	}
	if (action == MENU_NEW_GAME)
		new_game(a);
	else if (action == MENU_PLAY) {
		a->zone = a->menu.sector;
		a->level = 0;
		show_intro(a);
	}
}

/* Demo/test input: follow the lowest falling ball and fire whenever possible. */
static void autopilot(App *a)
{
	Game *g = &a->g;
	static unsigned tick;
	int i, target = -1, best = -1;

	for (i = 0; i < MAX_BALLS; i++) {
		const Ball *b = &g->balls[i];
		if (b->speed && !b->dying && b->y > best) {
			best = b->y;
			target = i;
		}
	}
	if (target >= 0) {
		int want = g->balls[target].x - (g->pl.wide ? 0x780 : 0x500);
		g->in.mouse_dx = want - g->pl.x;
	}
	if ((++tick / 35) % 2)
		g->in.click = 1;
}

static void draw_sheet(App *a)
{
	const char *hover = NULL;
	int id, x = 2, y = 2, row_h = 0;

	gfx_clear();
	for (id = 0; id < SPR_COUNT; id++) {
		const SpriteDef *d = sprite_get((SpriteId)id)->def;
		int f, w = d->frames * (d->w + 1) + 4;

		if (x + w > SCREEN_W) {
			x = 2;
			y += row_h + 3;
			row_h = 0;
		}
		for (f = 0; f < d->frames; f++)
			gfx_sprite((SpriteId)id, f, x + f * (d->w + 1), y);
		if (a->mouse_x >= x && a->mouse_x < x + w - 4 && a->mouse_y >= y && a->mouse_y < y + d->h)
			hover = d->name;
		if (d->h > row_h)
			row_h = d->h;
		x += w;
	}
	gfx_text(2, SCREEN_H - 9, 0xff, hover ? hover : "TAB: BACK TO THE GAME");
}

static void handle_event(App *a, const SDL_Event *e)
{
	Game *g = &a->g;

	switch (e->type) {
	case SDL_QUIT:
		SDL_Quit();
		exit(0);
	case SDL_VIDEORESIZE:
		video_resize(e->resize.w, e->resize.h);
		break;
	case SDL_MOUSEMOTION:
		video_to_game(e->motion.x, e->motion.y, &a->mouse_x, &a->mouse_y);
		if (a->menu_active)
			menu_mouse(&a->menu, a->mouse_x, a->mouse_y, 0);
		else if (a->playing && !g->paused)
			g->in.mouse_dx += video_game_dx(e->motion.xrel) * (a->menu.sensitivity + 1) / 4;
		break;
	case SDL_MOUSEBUTTONDOWN:
		if (e->button.button != SDL_BUTTON_LEFT)
			break;
		if (a->menu_active) {
			video_to_game(e->button.x, e->button.y, &a->mouse_x, &a->mouse_y);
			menu_action(a, menu_mouse(&a->menu, a->mouse_x, a->mouse_y, 1));
		} else if (a->playing) {
			g->in.click = 1;
			g->in.button = 1;
		} else if (a->fade_dir >= 0) {
			a->fade_dir = -8;
		}
		break;
	case SDL_MOUSEBUTTONUP:
		if (e->button.button == SDL_BUTTON_LEFT)
			g->in.button = 0;
		break;
	case SDL_KEYDOWN: {
		SDLKey k = e->key.keysym.sym;

		if (k == SDLK_F11 || (k == SDLK_RETURN && (e->key.keysym.mod & KMOD_ALT))) {
			video_toggle_fullscreen();
			break;
		}
		if (k == SDLK_F12) {
			video_save_bmp("screenshot.bmp");
			break;
		}
		if (a->menu_active) {
			if (a->menu.page == MENU_NAME && k != SDLK_RETURN && k != SDLK_ESCAPE)
				menu_name_key(&a->menu, (int)k);
			else if (k == SDLK_h && a->menu.page == MENU_MAIN)
				menu_open(&a->menu, MENU_SCORES);
			else if (k == SDLK_UP)
				menu_action(a, menu_key(&a->menu, MENU_UP));
			else if (k == SDLK_DOWN)
				menu_action(a, menu_key(&a->menu, MENU_DOWN));
			else if (k == SDLK_LEFT)
				menu_action(a, menu_key(&a->menu, MENU_LEFT));
			else if (k == SDLK_RIGHT)
				menu_action(a, menu_key(&a->menu, MENU_RIGHT));
			else if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_LCTRL)
				menu_action(a, menu_key(&a->menu, MENU_ACCEPT));
			else if (k == SDLK_ESCAPE)
				menu_action(a, menu_key(&a->menu, MENU_BACK));
			break;
		}
		if (k == SDLK_TAB) {
			a->sheet = !a->sheet;
			break;
		}
		if (!a->playing) {
			if (k == SDLK_ESCAPE) {
				SDL_Quit();
				exit(0);
			}
			if ((k == SDLK_SPACE || k == SDLK_RETURN || k == SDLK_LCTRL) && a->fade_dir >= 0)
				a->fade_dir = -8;
			break;
		}
		if (k == SDLK_LCTRL || k == SDLK_RCTRL) {
			g->in.click = 1;
			g->in.button = 1;
			break;
		}
		if (k < 128)
			game_key(g, (int)k);
		SDL_WM_GrabInput(g->paused || a->autoplay ? SDL_GRAB_OFF : SDL_GRAB_ON);
		break;
	}
	case SDL_KEYUP:
		if (e->key.keysym.sym == SDLK_LCTRL || e->key.keysym.sym == SDLK_RCTRL)
			g->in.button = 0;
		break;
	}
}

static void step(App *a)
{
	Game *g = &a->g;

	if (a->menu_active) {
		menu_draw(&a->menu);
		return;
	}
	if (!a->playing) {
		a->fade += a->fade_dir;
		if (a->fade >= 0x100) {
			a->fade = 0x100;
			a->fade_dir = 0;
			if (a->autoplay)
				a->fade_dir = -8;
		}
		if (a->fade <= 0 && a->fade_dir < 0) {
			run_next(a);
			return;
		}
		screen_render(a->block1, a->block2, a->fade);
		return;
	}

	if (!g->paused) {
		Uint8 *keys = SDL_GetKeyState(NULL);
		if (keys[SDLK_LEFT])
			g->in.mouse_dx -= KEY_SPEED;
		if (keys[SDLK_RIGHT])
			g->in.mouse_dx += KEY_SPEED;
		if (a->autoplay)
			autopilot(a);
	}

	game_frame(g);
	g->in.mouse_dx = 0;
	if (game_level_done(g)) {
		level_finished(a);
		return;
	}
	if (a->sheet)
		draw_sheet(a);
	else
		game_draw(g);
}

int main(int argc, char *argv[])
{
	static App app;
	App *a = &app;
	SDL_Event event;
	Uint32 next_tick;
	const char *exe_path = NULL, *shot_path = NULL;
	const char *audio_path = NULL;
	int audio_track = -1, audio_effect = -1, audio_seconds = 10;
	int shot_frames = 0, frame = 0;
	int i, loaded = 0;
	int difficulty_override = -1;

	a->diff = DIFF_NORMAL;
	a->start_zone = 1;
	menu_init(&a->menu);

	/* [CYBER.EXE|CYBPLUS.EXE] [--stage zone level] [--novice|--expert] [--cheat] [--auto] [--sheet] [--shot out.bmp frames] */
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--shot") && i + 2 < argc) {
			shot_path = argv[++i];
			shot_frames = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--stage") && i + 2 < argc) {
			a->direct_start = 1;
			a->start_zone = atoi(argv[++i]);
			a->start_level = atoi(argv[++i]) - 1;
			if (a->start_zone < 0 || a->start_zone > 5)
				a->start_zone = 1;
			if (a->start_level < 0 || a->start_level >= zone_levels[a->start_zone])
				a->start_level = 0;
		} else if (!strcmp(argv[i], "--novice")) {
			difficulty_override = DIFF_NOVICE;
		} else if (!strcmp(argv[i], "--expert")) {
			difficulty_override = DIFF_EXPERT;
		} else if (!strcmp(argv[i], "--cheat")) {
			a->cheats = 1;
		} else if (!strcmp(argv[i], "--auto")) {
			a->autoplay = 1;
		} else if (!strcmp(argv[i], "--sheet")) {
			a->sheet = 1;
		} else if (!strcmp(argv[i], "--menu") && i + 1 < argc) {
			const char *page = argv[++i];
			a->menu.page = !strcmp(page, "options") ? MENU_OPTIONS :
				!strcmp(page, "sectors") ? MENU_SECTORS :
				!strcmp(page, "scores") ? MENU_SCORES : MENU_MAIN;
		} else if (!strcmp(argv[i], "--audio-out") && i + 1 < argc) {
			audio_path = argv[++i];
		} else if (!strcmp(argv[i], "--music-track") && i + 1 < argc) {
			audio_track = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--sound-effect") && i + 1 < argc) {
			audio_effect = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) {
			audio_seconds = atoi(argv[++i]);
		} else {
			exe_path = argv[i];
		}
	}

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	if (exe_path)
		loaded = assets_init(exe_path);
	for (i = 0; !loaded && !exe_path && i < (int)(sizeof(exe_candidates) / sizeof(*exe_candidates)); i++)
		loaded = assets_init(exe_candidates[i]);
	if (!loaded) {
		fprintf(stderr, "CYBER.EXE not found. Pass its path as the first argument.\n");
		SDL_Quit();
		return 1;
	}
	menu_load(&a->menu, exe_config_path());
	a->diff = difficulty_override >= 0 ? (Difficulty)difficulty_override : a->menu.difficulty;
	if (audio_path) {
		int ok = sound_export_wav(audio_path, audio_track, audio_effect, audio_seconds);
		if (!ok)
			fprintf(stderr, "Audio export failed.\n");
		SDL_Quit();
		return ok ? 0 : 1;
	}

	if (!video_init(640, 400, 0)) {
		SDL_Quit();
		return 1;
	}
	SDL_ShowCursor(SDL_DISABLE);
	if (!shot_path) {
		if (!sound_init())
			fprintf(stderr, "Continuing without audio.\n");
		atexit(sound_shutdown);
		sound_enable(a->menu.sound, a->menu.music);
	}

	rng_seed(shot_path ? 1 : (uint16_t)SDL_GetTicks());
	a->menu.difficulty = a->diff;
	text_reset();
	if (a->direct_start || a->autoplay)
	new_game(a);
	else
		open_menu(a, a->menu.page);

	next_tick = SDL_GetTicks();
	for (;;) {
		while (SDL_PollEvent(&event))
			handle_event(a, &event);

		step(a);
		video_present(a->playing && !a->sheet ? a->g.curtain : 0);

		if (shot_path) {
			/* captures run unthrottled */
			if (++frame >= shot_frames) {
				video_save_bmp(shot_path);
				SDL_Quit();
				return 0;
			}
			continue;
		}

		next_tick += 1000 / FPS;
		if ((Sint32)(next_tick - SDL_GetTicks()) > 0)
			SDL_Delay(next_tick - SDL_GetTicks());
		else
			next_tick = SDL_GetTicks();
	}
}
