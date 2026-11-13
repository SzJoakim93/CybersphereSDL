#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "menu.h"
#include "screens.h"
#include "exe.h"
#include "sound.h"
#include <SDL/SDL.h>
#undef main

static void check_drops(void)
{
	static Game single, linked;
	HitTest hit;
	int seed, single_random, linked_random;

	for (seed = 0; seed < 256; seed++) {
		game_init(&single, DIFF_NORMAL);
		game_init(&linked, DIFF_NORMAL);
		memset(&hit, 0, sizeof(hit));
		hit.count = 1;
		hit.hits[0].cell = 40;
		hit.hits[0].entry = 0x81000001u;
		rng_seed((uint16_t)seed);
		bricks_process_hits(&single, &hit);
		single_random = rng(32768);

		hit.count = 3;
		hit.hits[1].cell = 41;
		hit.hits[1].entry = 0x81000001u;
		hit.hits[2].cell = 42;
		hit.hits[2].entry = 0x81000001u;
		linked.lv.bricks.type[40] = 1;
		linked.lv.bricks.type[41] = 1;
		linked.lv.bricks.type[42] = 1;
		rng_seed((uint16_t)seed);
		bricks_process_hits(&linked, &hit);
		linked_random = rng(32768);
		assert(single_random == linked_random);
		assert(!memcmp(single.bursts, linked.bursts, sizeof(single.bursts)));
		assert(!memcmp(single.capsules, linked.capsules, sizeof(single.capsules)));
		assert(!linked.lv.bricks.type[40]);
		assert(!linked.lv.bricks.type[41]);
		assert(!linked.lv.bricks.type[42]);
	}
}

static void check_level_reset(int restore)
{
	static Game game;
	int cell;

	game_init(&game, DIFF_NORMAL);
	assert(game_start_level(&game, 1, 0, 0));
	game_save_level(&game);
	game.pl.wide = game.pl.fire = game.pl.hold = 1;
	game.pl.gems = 42;
	game.pl.score = 1234;
	game.pl.lives = 5;
	game.guard_timer = -20;
	assert(game_start_level(&game, 1, 1, restore));
	assert(!game.pl.wide && !game.pl.fire && !game.pl.hold);
	assert(!game.pl.gems && !game.guard_timer);
	assert(game.pl.score == 1234 && game.pl.lives == 5);
	for (cell = 304; cell < 320; cell++)
		assert(game.lv.bricks.type[cell] != game.guard_type || !game.guard_type);
}

static void check_menus(void)
{
	Menu menu, loaded;
	FILE *invalid;
	int index;
	menu_init(&menu);
	menu_open(&menu, MENU_MAIN);
	assert(menu_key(&menu, MENU_ACCEPT) == MENU_NEW_GAME);
	menu_key(&menu, MENU_DOWN);
	assert(menu_key(&menu, MENU_ACCEPT) == MENU_NONE);
	assert(menu.page == MENU_OPTIONS);
	menu_key(&menu, MENU_RIGHT);
	assert(menu.difficulty == DIFF_EXPERT);
	menu_key(&menu, MENU_DOWN);
	menu_key(&menu, MENU_LEFT);
	assert(menu.sensitivity == 2);
	menu_key(&menu, MENU_BACK);
	assert(menu.page == MENU_MAIN);

	menu_open(&menu, MENU_SECTORS);
	menu.selected = 3;
	assert(menu_key(&menu, MENU_ACCEPT) == MENU_NONE);
	menu.completed = 3;
	assert(menu_key(&menu, MENU_ACCEPT) == MENU_PLAY);
	assert(menu.sector == 4);
	menu.selected = 4;
	assert(menu_key(&menu, MENU_ACCEPT) == MENU_NONE);
	menu.completed = 7;
	assert(menu_key(&menu, MENU_ACCEPT) == MENU_PLAY);
	menu.difficulty = DIFF_NOVICE;
	assert(menu_key(&menu, MENU_ACCEPT) == MENU_NONE);
	menu.completed = 15;
	assert(menu_all_completed(&menu));
	menu.difficulty = DIFF_EXPERT;
	assert(!menu_all_completed(&menu));
	menu.completed = 31;
	assert(menu_all_completed(&menu));

	assert(menu_submit_score(&menu, 100, DIFF_NORMAL));
	menu_name_key(&menu, 'a');
	menu_name_key(&menu, 'b');
	menu_name_key(&menu, 8);
	menu_name_key(&menu, 'c');
	menu_key(&menu, MENU_ACCEPT);
	assert(!strcmp(menu.scores[DIFF_NORMAL][0].name, "AC"));
	assert(menu_submit_score(&menu, 200, DIFF_NORMAL));
	menu_key(&menu, MENU_ACCEPT);
	assert(menu.scores[DIFF_NORMAL][0].score == 200);
	assert(menu.scores[DIFF_NORMAL][1].score == 100);
	for (index = 0; index < 16; index++)
		menu_name_key(&menu, 'z');
	assert(strlen(menu.name) == 16);
	assert(menu_save(&menu, "menu-test.cfg"));
	menu_init(&loaded);
	assert(menu_load(&loaded, "menu-test.cfg"));
	assert(loaded.difficulty == menu.difficulty);
	assert(loaded.sensitivity == menu.sensitivity);
	assert(loaded.completed == 31u);
	assert(!memcmp(menu.scores, loaded.scores, sizeof(menu.scores)));
	invalid = fopen("menu-test.cfg", "w");
	assert(invalid);
	assert(fputs("CYBER_SDL 1 1 3 1 1\n", invalid) >= 0);
	for (index = 0; index < 45; index++)
		assert(fputs("0 -\n", invalid) >= 0);
	assert(fclose(invalid) == 0);
	assert(menu_load(&loaded, "menu-test.cfg"));
	assert(loaded.completed == 0);
	assert(loaded.difficulty == DIFF_NORMAL);
	invalid = fopen("menu-test.cfg", "w");
	assert(invalid);
	assert(fputs("CYBER_SDL 2 1 3 1 1 32\n", invalid) >= 0);
	assert(fclose(invalid) == 0);
	assert(!menu_load(&loaded, "menu-test.cfg"));
	assert(loaded.completed == 0);
	assert(menu_load(&menu, "missing-menu-test.cfg") == 0);
	loaded = menu;
	invalid = fopen("menu-test.cfg", "w");
	assert(invalid);
	assert(fputs("CYBER_SDL 1 99 0 1 1\n", invalid) >= 0);
	assert(fclose(invalid) == 0);
	assert(!menu_load(&loaded, "menu-test.cfg"));
	assert(loaded.difficulty == menu.difficulty);
	assert(loaded.sensitivity == menu.sensitivity);
	assert(remove("menu-test.cfg") == 0);
}

static void check_title_shine(void)
{
	Menu menu;
	const uint8_t *palette = exe_ptr(SEG_LAYER2, 0x282, 768);
	int letter, age, color;
	menu_init(&menu);
	for (letter = 0; letter < 11; letter++) {
		int position = ds_word((uint16_t)(0x5974 + letter * 4));
		int frame = ds_word((uint16_t)(0x5976 + letter * 4));
		const uint8_t *pixels = exe_ptr(0x2674, (uint16_t)(0x1a20 + frame * 0xaf0), 50 * 56);
		int pixel = 0, destination;
		while (pixel < 50 * 56 && !pixels[pixel])
			pixel++;
		assert(pixel < 50 * 56);
		assert(pixels[pixel] <= 8);
		destination = (position / SCREEN_W + pixel / 50) * SCREEN_W +
			position % SCREEN_W + pixel % 50;
		for (age = 0; age <= 12; age += 6) {
			menu.ticks = 32 + letter * 12 + age;
			menu_draw(&menu);
			assert(gfx_fb[destination] == pixels[pixel] + (age < 12 ? 0x0e : 0x16));
			for (color = 0; color < 3; color++) {
				int normal = palette[(pixels[pixel] + 0x16) * 3 + color] & 63;
				int expected = 63 - ((63 - normal) * (age * 256 / 12) >> 8);
				assert(gfx_pal[gfx_fb[destination] * 3 + color] == expected);
			}
		}
	}
}

static void check_screen_art(void)
{
	static const uint16_t backgrounds[] = { 0, 0x582, 0xb04, 0x1086, 0x1608, 0x1b8a,
		0x210c, 0x268e, 0x2c10, 0x3192, 0x3c96 };
	Menu menu;
	unsigned scene;
	int pixel, colored;
	text_reset();
	for (scene = 0; scene < sizeof(backgrounds) / sizeof(backgrounds[0]); scene++) {
		const uint8_t *palette = exe_ptr(SEG_LAYER2, backgrounds[scene] + 0x282, 768);
		screen_background(backgrounds[scene], 256, 0);
		colored = 0;
		for (pixel = 0; pixel < SCREEN_W * SCREEN_H; pixel++)
			colored += gfx_fb[pixel] != 0;
		assert(colored > 1000);
		for (pixel = 0; pixel < 768; pixel++)
			assert(gfx_pal[pixel] == (palette[pixel] & 63));
	}
	menu_init(&menu);
	for (scene = MENU_MAIN; scene <= MENU_NAME; scene++) {
		menu_open(&menu, (MenuPage)scene);
		menu_draw(&menu);
	}
	screen_render(TXT_LEVEL, TXT_PRESS_FIRE, 0);
	for (pixel = 0; pixel < 768; pixel++)
		assert(!gfx_pal[pixel]);
}

static void check_all_stages(void)
{
	static Game game;
	int zone, level, frame;
	for (zone = 0; zone < NUM_EPISODES; zone++)
		for (level = 0; level < LEVELS_PER_EPISODE; level++) {
			game_init(&game, DIFF_NORMAL);
			assert(game_start_level(&game, zone, level, 0));
			for (frame = 0; frame < 600 && !game_level_done(&game); frame++) {
				game_frame(&game);
				if (!(frame % 120))
					game_draw(&game);
			}
		}
	assert(stage_param_index(-1, 0) == -1);
	assert(stage_param_index(NUM_EPISODES, 0) == -1);
	assert(stage_param_index(0, LEVELS_PER_EPISODE) == -1);
}

static void check_audio(void)
{
	static int16_t samples[44100 * 2];
	static int16_t repeated[44100 * 2];
	int track, effect, sample, second;
	unsigned nonzero;
	for (track = 1; track <= SOUND_MUSIC_COUNT; track++) {
		assert(sound_prepare(44100));
		sound_music(track);
		sound_render(samples, 44100);
		nonzero = 0;
		for (sample = 0; sample < 44100 * 2; sample++)
			nonzero += samples[sample] != 0;
		printf("Music %d: %u nonzero samples\n", track, nonzero);
		assert(nonzero > 100);
		for (second = 0; second < 5; second++)
			sound_render(samples, 44100);
		assert(sound_faults() == 0);
	}
	for (effect = 0; effect < SOUND_EFFECT_COUNT; effect++) {
		assert(sound_prepare(44100));
		sound_play(effect);
		sound_render(samples, 44100);
		nonzero = 0;
		for (sample = 0; sample < 44100 * 2; sample++)
			nonzero += samples[sample] != 0;
		printf("Effect %d: %u nonzero samples\n", effect, nonzero);
		assert(nonzero > 100);
		assert(sound_faults() == 0);
	}
	assert(sound_prepare(44100));
	sound_render(samples, 44100);
	for (sample = 0; sample < 44100 * 2; sample++)
		assert(!samples[sample]);
	assert(sound_prepare(44100));
	sound_music(1);
	sound_render(samples, 44100);
	assert(sound_prepare(44100));
	sound_music(1);
	sound_render(repeated, 44100);
	assert(!memcmp(samples, repeated, sizeof(samples)));
	sound_enable(0, 0);
	sound_play(SFX_GEM);
	sound_render(samples, 44100);
	for (sample = 0; sample < 44100 * 2; sample++)
		assert(!samples[sample]);
	assert(sound_faults() == 0);
	sound_shutdown();
}

int main(int argc, char **argv)
{
	assert(assets_init(argc > 1 ? argv[1] : "bin/CYBER.EXE"));
	assert(!strcmp(exe_config_path(), exe_is_plus() ? "CyberspherePlus-SDL.cfg" : "Cybersphere-SDL.cfg"));
	if (argc > 2 && !strcmp(argv[2], "--layout-only")) {
		StageParams parameters;
		const uint8_t *ball = sprite_frame(SPR_BALL, 0);
		assert(ball[0] == 0 && ball[3] == 4 && ball[4] == 4);
		assert(sine(0) == 0 && sine(64) == 127 && sine(192) == -127);
		assert(stage_params(stage_param_index(1, 0), &parameters));
		assert(parameters.layout >= 1 && parameters.layout <= NUM_LAYOUTS);
		assert(parameters.layer2 <= NUM_LAYERS2);
		printf("%s executable layout check passed.\n", exe_is_plus() ? "Plus" : "Original");
		return 0;
	}
	check_drops();
	check_level_reset(0);
	check_level_reset(1);
	check_menus();
	check_title_shine();
	check_screen_art();
	check_all_stages();
	check_audio();
	if (argc > 2 && !strcmp(argv[2], "--live-audio")) {
		assert(SDL_Init(0) == 0);
		assert(sound_init());
		sound_music(1);
		SDL_Delay(250);
		assert(sound_faults() == 0);
		sound_shutdown();
		SDL_Quit();
		puts("Live SDL audio callback check passed.");
	}
	puts("Gameplay, menus and recovered audio checks passed.");
	return 0;
}