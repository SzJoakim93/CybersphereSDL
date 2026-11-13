#ifndef SCREENS_H
#define SCREENS_H

#include <stdint.h>
#include "game.h"

/* Text blocks of the between-level screens (DS offsets, drawn by 1000:45b3) */
#define TXT_GAME_OVER      0x6537
#define TXT_GAME_OVER_SCORE 0x6573
#define TXT_LEVEL          0x66f5
#define TXT_LEVEL_ZONE     0x6709
#define TXT_LEVEL_NUMBER   0x670d
#define TXT_LEVEL_SCORE    0x6723
#define TXT_LIVES          0x672d
#define TXT_LIVES_DIGIT    0x674d
#define TXT_STAGE_COMPLETE 0x6750
#define TXT_ENTER_BONUS    0x6763
#define TXT_SURVIVAL       0x677f
#define TXT_TALLY          0x679d
#define TXT_PRESS_FIRE     0x682e

void text_reset(void);
uint8_t *text_at(uint16_t off);
/* 1000:0bc5 - 8 characters, leading blanks */
void text_score(uint16_t off, uint32_t v);
/* 1000:0ccf - fills the bonus table at row y and adds the total to the score */
void text_tally(Game *g, int y, int bonus_index);

void screen_render(uint16_t block1, uint16_t block2, int bright);
void screen_background(uint16_t offset, int bright, int logo);

#endif
