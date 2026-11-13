#ifndef MENU_H
#define MENU_H

#include "game.h"

typedef enum { MENU_MAIN, MENU_OPTIONS, MENU_SECTORS, MENU_SCORES, MENU_NAME } MenuPage;
typedef enum { MENU_NONE, MENU_NEW_GAME, MENU_PLAY, MENU_EXIT } MenuAction;
typedef enum { MENU_UP, MENU_DOWN, MENU_LEFT, MENU_RIGHT, MENU_ACCEPT, MENU_BACK } MenuKey;

typedef struct {
	unsigned score;
	char name[17];
} MenuScore;

typedef struct {
	MenuPage page;
	int selected;
	int ticks;
	Difficulty difficulty;
	int sensitivity;
	int sound, music, sound_test, music_test;
	unsigned completed;
	int sector;
	MenuScore scores[3][15];
	int score_rank, score_difficulty;
	char name[17];
} Menu;

void menu_init(Menu *menu);
void menu_open(Menu *menu, MenuPage page);
MenuAction menu_key(Menu *menu, MenuKey key);
MenuAction menu_mouse(Menu *menu, int x, int y, int click);
void menu_draw(Menu *menu);
int menu_all_completed(const Menu *menu);
int menu_load(Menu *menu, const char *path);
int menu_save(const Menu *menu, const char *path);
int menu_submit_score(Menu *menu, unsigned score, Difficulty difficulty);
void menu_name_key(Menu *menu, int key);

#endif