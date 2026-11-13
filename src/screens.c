#include <string.h>
#include "screens.h"
#include "exe.h"

#define TEXT_FIRST 0x6537
#define TEXT_LAST  0x684c

static uint8_t text[TEXT_LAST - TEXT_FIRST + 32];

void text_reset(void)
{
	size_t length = exe_data_offset(TEXT_LAST) - exe_data_offset(TEXT_FIRST);
	memset(text, 0, sizeof(text));
	memcpy(text, ds_ptr(TEXT_FIRST, (uint16_t)length), length);
}

uint8_t *text_at(uint16_t off)
{
	return &text[exe_data_offset(off) - exe_data_offset(TEXT_FIRST)];
}

void text_score(uint16_t off, uint32_t v)
{
	uint8_t *t = text_at(off);
	uint32_t div = 10000000;
	int i, started = 0;

	for (i = 0; i < 8; i++, div /= 10) {
		int d = (int)(v / div % 10);
		if (d || started || i == 7) {
			t[i] = (uint8_t)('0' + d);
			started = 1;
		} else {
			t[i] = ' ';
		}
	}
}

/* Five characters: the upper three digits are blanked while zero, the last two always shown. */
static void put5(uint16_t off, unsigned v)
{
	uint8_t *t = text_at(off);
	unsigned div = 10000;
	int i, started = 0;

	for (i = 0; i < 5; i++, div /= 10) {
		int d = (int)(v / div % 10);
		if (d || started || i >= 3) {
			t[i] = (uint8_t)('0' + d);
			started = 1;
		} else {
			t[i] = ' ';
		}
	}
}

void text_tally(Game *g, int y, int bonus_index)
{
	uint16_t total = (uint16_t)g->survival_bonus;
	unsigned gem_bonus = (unsigned)g->pl.gems * 50;
	int n;
	uint8_t *t;

	*text_at(0x679e) = (uint8_t)y;
	*text_at(0x67bb) = (uint8_t)(y + 10);
	*text_at(0x67d8) = (uint8_t)(y + 20);
	*text_at(0x67f5) = (uint8_t)(y + 30);
	*text_at(0x6812) = (uint8_t)(y + 45);

	/* "ENTERING BONUS STAGE n" */
	if (bonus_index < 9) {
		*text_at(0x6763) = 0x24;
		*text_at(0x677b) = (uint8_t)('1' + bonus_index);
		*text_at(0x677c) = ' ';
	} else {
		*text_at(0x6763) = 0x22;
		*text_at(0x677b) = '1';
		*text_at(0x677c) = '0';
	}

	put5(0x6796, total);

	/* gem count, 3 digits */
	t = text_at(0x67ae);
	n = g->pl.gems;
	t[0] = n / 100 ? (uint8_t)('0' + n / 100) : ' ';
	t[1] = (n / 100 || n / 10 % 10) ? (uint8_t)('0' + n / 10 % 10) : ' ';
	t[2] = (uint8_t)('0' + n % 10);

	total = (uint16_t)(total + gem_bonus);
	put5(0x67b4, gem_bonus & 0xffff);

	t = text_at(0x67d2);
	memcpy(t, "  00", 4);
	n = g->pl.hold + g->pl.fire;
	if (n) {
		t[0] = (uint8_t)('0' + n);
		t[1] = '0';
		total = (uint16_t)(total + n * 1000);
	}

	t = text_at(0x67ef);
	memcpy(t, "  00", 4);
	if (g->pl.wide) {
		t[0] = '1';
		t[1] = '0';
		total = (uint16_t)(total + 1000);
	}

	t = text_at(0x680b);
	memcpy(t, "   00", 5);
	n = g->balls_saved - 1;
	if (n > 0) {
		t[2] = '0';
		if (n / 10)
			t[0] = (uint8_t)('0' + n / 10 % 10);
		t[1] = (uint8_t)('0' + n % 10);
		total = (uint16_t)(total + n * 1000);
	}

	put5(0x6828, total);
	g->survival_bonus = total;
	g->pl.score += total;
}

void screen_background(uint16_t offset, int bright, int logo)
{
	Level background;
	const uint8_t *record = exe_ptr(SEG_LAYER2, offset, LEVEL_SIZE);
	int x, y;

	memset(&background, 0, sizeof(background));
	memcpy(&background.layer2, record, LEVEL_SIZE);
	background.bricks.shift = background.layer2.shift;
	gfx_bricks(&background);
	gfx_set_palette(background.layer2.pal, 0, 256, bright);
	if (logo) {
		const uint8_t *pixels = exe_ptr(0x2674, 0x8a5a, 155 * 58);
		for (y = 0; y < 58; y++)
			for (x = 0; x < 155; x++)
				if (pixels[y * 155 + x])
					gfx_fb[(y + 16) * SCREEN_W + x + 82] = pixels[y * 155 + x];
	}
}

void screen_render(uint16_t block1, uint16_t block2, int bright)
{
	uint16_t background = 0x0b04;
	if (block1 == TXT_GAME_OVER)
		background = 0x0582;
	else if (block1 == TXT_LIVES)
		background = 0x1086;
	else if (block1 == TXT_STAGE_COMPLETE)
		background = 0x1608;
	else if (block1 == TXT_ENTER_BONUS)
		background = 0x1b8a;
	else if (block1 == TXT_SURVIVAL)
		background = 0x210c;
	else if (block1 == 0x657d || block1 == 0x6622)
		background = 0x2c10;
	screen_background(background, bright, 1);
	gfx_message(text_at(block1));
	gfx_message(text_at(block2));
}
