#include <stdio.h>
#include <string.h>
#include "menu.h"
#include "screens.h"
#include "exe.h"
#include "sound.h"

static void artwork(uint16_t offset, int width, int height, int x, int y, int base)
{
	const uint8_t *pixels = exe_ptr(0x2674, offset, (size_t)width * height);
	int row, col;
	for (row = 0; row < height; row++)
		for (col = 0; col < width; col++) {
			int dx = x + col, dy = y + row;
			uint8_t value = pixels[row * width + col];
			if (value && (unsigned)dx < SCREEN_W && (unsigned)dy < SCREEN_H)
				gfx_fb[dy * SCREEN_W + dx] = (uint8_t)(value + base);
		}
}

void menu_init(Menu *menu)
{
	memset(menu, 0, sizeof(*menu));
	menu->difficulty = DIFF_NORMAL;
	menu->sensitivity = 3;
	menu->sound = menu->music = 1;
}

int menu_load(Menu *menu, const char *path)
{
	Menu loaded = *menu;
	FILE *file = fopen(path, "r");
	int difficulty, version, row, group;
	if (!file)
		return 0;
	if (fscanf(file, "CYBER_SDL %d %d %d %d %d", &version, &difficulty,
		   &loaded.sensitivity, &loaded.sound, &loaded.music) != 5 || (version != 1 && version != 2) ||
	    difficulty < 0 || difficulty > 2 || loaded.sensitivity < 0 || loaded.sensitivity > 7 ||
	    loaded.sound < 0 || loaded.sound > 1 || loaded.music < 0 || loaded.music > 1) {
		fclose(file);
		return 0;
	}
	loaded.completed = 0;
	if (version == 2 && (fscanf(file, "%u", &loaded.completed) != 1 || loaded.completed > 31u)) {
		fclose(file);
		return 0;
	}
	loaded.difficulty = (Difficulty)difficulty;
	for (group = 0; group < 3; group++)
		for (row = 0; row < 15; row++) {
			MenuScore *entry = &loaded.scores[group][row];
			if (fscanf(file, "%u %16s", &entry->score, entry->name) != 2 ||
			    (row && entry->score > loaded.scores[group][row - 1].score)) {
				fclose(file);
				return 0;
			}
			if (!entry->score)
				memset(entry->name, 0, sizeof(entry->name));
		}
	fclose(file);
	*menu = loaded;
	return 1;
}

int menu_save(const Menu *menu, const char *path)
{
	FILE *file = fopen(path, "w");
	int group, row, ok;
	if (!file)
		return 0;
	ok = fprintf(file, "CYBER_SDL 2 %d %d %d %d %u\n", menu->difficulty,
		     menu->sensitivity, menu->sound, menu->music, menu->completed) >= 0;
	for (group = 0; group < 3; group++)
		for (row = 0; row < 15; row++) {
			const MenuScore *entry = &menu->scores[group][row];
			if (fprintf(file, "%u %s\n", entry->score, entry->name[0] ? entry->name : "-") < 0)
				ok = 0;
		}
	if (fclose(file))
		ok = 0;
	return ok;
}

int menu_submit_score(Menu *menu, unsigned score, Difficulty difficulty)
{
	int rank;
	for (rank = 0; rank < 15; rank++)
		if (score > menu->scores[difficulty][rank].score) {
			memmove(&menu->scores[difficulty][rank + 1], &menu->scores[difficulty][rank],
				(size_t)(14 - rank) * sizeof(MenuScore));
			menu->scores[difficulty][rank].score = score;
			menu->scores[difficulty][rank].name[0] = '\0';
			menu->score_rank = rank;
			menu->score_difficulty = difficulty;
			menu->name[0] = '\0';
			menu_open(menu, MENU_NAME);
			return 1;
		}
	return 0;
}

void menu_name_key(Menu *menu, int key)
{
	size_t length = strlen(menu->name);
	if (key == 8 && length)
		menu->name[length - 1] = '\0';
	else if (length < 16 && ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') ||
		 (key >= '0' && key <= '9') || key == '-')) {
		menu->name[length] = (char)(key >= 'a' && key <= 'z' ? key - 'a' + 'A' : key);
		menu->name[length + 1] = '\0';
	}
}

void menu_open(Menu *menu, MenuPage page)
{
	menu->page = page;
	menu->selected = 0;
	menu->ticks = 0;
	if (page != MENU_NAME)
		sound_music(page == MENU_OPTIONS ? 7 : page == MENU_SECTORS ? 2 : 4);
	if (page == MENU_SCORES)
		menu->selected = 1;
	if (page == MENU_SECTORS) {
		while (menu->selected < 4 && (menu->completed & (1u << menu->selected)))
			menu->selected++;
	}
}

int menu_all_completed(const Menu *menu)
{
	unsigned required = menu->difficulty == DIFF_NOVICE ? 15u : 31u;
	return (menu->completed & required) == required;
}

static int sector_locked(const Menu *menu, int index)
{
	if (menu->completed & (1u << index))
		return 1;
	if (index == 4 && (menu->difficulty == DIFF_NOVICE || !menu->completed))
		return 1;
	if (index == 3) {
		unsigned count = 0, mask = menu->completed & 15u;
		while (mask) {
			count += mask & 1u;
			mask >>= 1;
		}
		return count < 2;
	}
	if (index == 4) {
		unsigned count = 0, mask = menu->completed & 15u;
		while (mask) {
			count += mask & 1u;
			mask >>= 1;
		}
		return count < 3;
	}
	return 0;
}

MenuAction menu_key(Menu *menu, MenuKey key)
{
	int count = menu->page == MENU_OPTIONS ? 7 : menu->page == MENU_SECTORS ? 5 : 3;
	if (menu->page == MENU_NAME) {
		if (key == MENU_ACCEPT || key == MENU_BACK) {
			strcpy(menu->scores[menu->score_difficulty][menu->score_rank].name,
			       menu->name[0] ? menu->name : "PLAYER");
			menu_open(menu, MENU_SCORES);
		}
		return MENU_NONE;
	}
	if (menu->page == MENU_SCORES) {
		if (key == MENU_ACCEPT || key == MENU_BACK)
			menu_open(menu, MENU_MAIN);
		return MENU_NONE;
	}
	menu->ticks = menu->page == MENU_MAIN ? 176 : 0;
	if (key == MENU_BACK) {
		if (menu->page == MENU_MAIN)
			return MENU_EXIT;
		menu_open(menu, MENU_MAIN);
		return MENU_NONE;
	}
	if (key == MENU_UP || key == MENU_DOWN) {
		menu->selected = (menu->selected + (key == MENU_UP ? count - 1 : 1)) % count;
		sound_play(SFX_PAUSE);
		return MENU_NONE;
	}
	if (menu->page == MENU_OPTIONS) {
		int direction = key == MENU_LEFT ? -1 : 1;
		if (key != MENU_LEFT && key != MENU_RIGHT && key != MENU_ACCEPT)
			return MENU_NONE;
		switch (menu->selected) {
		case 0: menu->difficulty = (Difficulty)((menu->difficulty + direction + 3) % 3); break;
		case 1: menu->sensitivity = (menu->sensitivity + direction + 8) % 8; break;
		case 2: menu->sound ^= 1; break;
		case 3: menu->music ^= 1; break;
		case 4:
			if (key == MENU_ACCEPT)
				sound_play(menu->sound_test);
			else
				menu->sound_test = (menu->sound_test + direction + SOUND_EFFECT_COUNT) % SOUND_EFFECT_COUNT;
			break;
		case 5:
			if (key == MENU_ACCEPT)
				sound_music(menu->music_test + 1);
			else
				menu->music_test = (menu->music_test + direction + SOUND_MUSIC_COUNT) % SOUND_MUSIC_COUNT;
			break;
		case 6: menu_open(menu, MENU_MAIN); break;
		}
		sound_enable(menu->sound, menu->music);
		return MENU_NONE;
	}
	if (key != MENU_ACCEPT)
		return MENU_NONE;
	if (menu->page == MENU_SECTORS) {
		if (sector_locked(menu, menu->selected))
			return MENU_NONE;
		menu->sector = menu->selected + 1;
		return MENU_PLAY;
	}
	if (menu->selected == 0)
		return MENU_NEW_GAME;
	if (menu->selected == 1) {
		menu_open(menu, MENU_OPTIONS);
		return MENU_NONE;
	}
	return MENU_EXIT;
}

MenuAction menu_mouse(Menu *menu, int x, int y, int click)
{
	int row = -1;
	if (menu->page == MENU_MAIN && x >= 130 && x < 310 && y >= 128 && y < 182)
		row = (y - 128) / 18;
	else if (menu->page == MENU_SECTORS && x >= 64 && x < 256 && y >= 10 && y < 190)
		row = (y - 10) / 38;
	else if (menu->page == MENU_OPTIONS && x >= 48 && x < 300) {
		static const int positions[] = { 54, 70, 86, 102, 126, 142, 166 };
		int index;
		for (index = 0; index < 7; index++)
			if (y >= positions[index] - 4 && y < positions[index] + 12)
				row = index;
	}
	if (row >= 0 && row < (menu->page == MENU_SECTORS ? 5 : menu->page == MENU_OPTIONS ? 7 : 3)) {
		if (menu->selected != row)
			menu->ticks = menu->page == MENU_MAIN ? 176 : 0;
		menu->selected = row;
		if (click && menu->page == MENU_OPTIONS && (row == 4 || row == 5) && x >= 240)
			return menu_key(menu, MENU_RIGHT);
		if (click && menu->page == MENU_OPTIONS && (row == 4 || row == 5) && x < 152)
			return menu_key(menu, MENU_LEFT);
		if (click)
			return menu_key(menu, MENU_ACCEPT);
	} else if (click && menu->page == MENU_SCORES) {
		return menu_key(menu, MENU_ACCEPT);
	} else if (click && menu->page == MENU_NAME) {
		return menu_key(menu, MENU_ACCEPT);
	}
	return MENU_NONE;
}

static void options_draw(const Menu *menu)
{
	static const char *names[] = { "NOVICE", "NORMAL", "EXPERT" };
	static const int positions[] = { 54, 70, 86, 102, 126, 142, 166 };
	char number[16];
	screen_background(0x3192, 256, 0);
	gfx_message(ds_ptr(0x6b6d, 112));
	gfx_text(152, 54, 0x1d, names[menu->difficulty]);
	snprintf(number, sizeof(number), "%d", menu->sensitivity + 1);
	gfx_text(152, 70, 0x1d, number);
	gfx_text(152, 86, 0x1d, menu->sound ? "ON" : "OFF");
	gfx_text(152, 102, 0x1d, menu->music ? "ON" : "OFF");
	snprintf(number, sizeof(number), "%02d", menu->sound_test + 1);
	gfx_text(152, 126, 0x1c, number);
	snprintf(number, sizeof(number), "%02d", menu->music_test + 1);
	gfx_text(152, 142, 0x1c, number);
	artwork(0x8790, 34, 21, 10, positions[menu->selected] - 7, 0x1d);
}

static void sector_card(int y)
{
	const uint8_t *pixels = exe_ptr(0x2674, 0xad78, 16 * 16);
	int row, col, source_row = 0;
	for (row = 0; row < 28; row++) {
		for (col = 0; col < 128; col++) {
			int source_col = col < 4 ? col : col >= 124 ? col - 112 : 4 + (col & 3);
			gfx_fb[(y + row) * SCREEN_W + 64 + col] =
				(uint8_t)(pixels[source_row * 16 + source_col] + 15);
		}
		if (row < 8 || row >= 20)
			source_row++;
	}
}

static void sectors_draw(const Menu *menu)
{
	int sector;
	screen_background(0x268e, 256, 0);
	for (sector = 0; sector < 5; sector++) {
		int y = 10 + sector * 38;
		char label[16];
		sector_card(y);
		snprintf(label, sizeof(label), "SECTOR %d", sector + 1);
		gfx_text(72, y + 4, 0x1f, label);
		gfx_text(72, y + 16, 0x1d, (menu->completed & (1u << sector)) ? "COMPLETE" :
			 sector_locked(menu, sector) ? "LOCKED" : "AVAILABLE");
	}
	artwork(0x8790, 34, 21, 20, 13 + menu->selected * 38, 0x1d);
}

static void scores_draw(const Menu *menu)
{
	static const char *names[] = { "NOVICE", "NORMAL", "EXPERT" };
	int rank;
	screen_background(0x3c96, 256, 0);
	gfx_text(32, 12, 1, "HIGH SCORES");
	gfx_text(184, 12, 1, names[menu->difficulty]);
	for (rank = 0; rank < 15; rank++) {
		const MenuScore *entry = &menu->scores[menu->difficulty][rank];
		char line[40];
		snprintf(line, sizeof(line), "%2d. %-16s %8u", rank + 1,
			 entry->score ? entry->name : "----------------", entry->score);
		gfx_text(32, 35 + rank * 10, 0x18 - rank / 3, line);
	}
}

void menu_draw(Menu *menu)
{
	if (menu->page == MENU_OPTIONS)
		options_draw(menu);
	else if (menu->page == MENU_SECTORS)
		sectors_draw(menu);
	else if (menu->page == MENU_SCORES)
		scores_draw(menu);
	else if (menu->page == MENU_NAME) {
		screen_background(0x3c96, 256, 0);
		gfx_text(64, 64, 1, "NEW HIGH SCORE");
		gfx_text(64, 96, 1, "NAME");
		gfx_text(64, 112, 1, menu->name);
		if (menu->ticks % 32 < 16)
			gfx_fill(64 + (int)strlen(menu->name) * 8, 120, 8, 1, 1);
	}
	else {
		int letter;
		const uint8_t *palette = exe_ptr(SEG_LAYER2, 0x282, 768);
		screen_background(0, menu->ticks < 32 ? menu->ticks * 8 : 256, 0);
		for (letter = 0; letter < 11; letter++) {
			int position = ds_word((uint16_t)(0x5974 + letter * 4));
			int frame = ds_word((uint16_t)(0x5976 + letter * 4));
			int age = menu->ticks - (32 + letter * 12);
			int base = 0x16;
			if (age < 0)
				break;
			if (age == 0)
				sound_play(SFX_BALL_SMOKE);
			if (age < 12) {
				base = 0x0e;
				gfx_set_palette_whiten(palette + 0x17 * 3, 0x0f, 8, age * 256 / 12);
			}
			artwork((uint16_t)(0x1a20 + frame * 0xaf0), 50, 56,
				position % SCREEN_W, position / SCREEN_W, base);
		}
		artwork(0x71a0, 108, 52, 122, 130, 0x20);
		artwork(0x8790, 34, 21, 66, 128 + menu->selected * 18, 0x3d);
		gfx_set_palette(exe_ptr(SEG_LAYER2, 0x282 + 0xf4 * 3, 12 * 3), 0xf4, 12, 256);
		gfx_message(ds_ptr(0x64ed, 74));
		{
			int phase = menu->ticks % 64;
			gfx_set_palette_whiten(palette + 0x3d * 3, 0x3d, 4,
					       128 + (phase < 32 ? phase : 64 - phase) * 4);
		}
	}
	menu->ticks++;
	if (menu->page == MENU_MAIN && menu->ticks >= 1050) {
		menu_open(menu, MENU_SCORES);
		menu->selected = 0;
	} else if (menu->page == MENU_SCORES && !menu->selected && menu->ticks >= 280) {
		menu_open(menu, MENU_MAIN);
	}
}