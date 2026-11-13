#include <stdio.h>
#include <string.h>
#include "assets.h"
#include "exe.h"

typedef char level_size_check[(sizeof(LevelData) == LEVEL_SIZE) ? 1 : -1];

#define DS_BRICK_TYPES 0x5ca4
#define DS_FONT        0x5aa4
#define DS_STAGE_TABLE 0x87fe
#define DS_STAGE_PARAM 0x883a
#define STAGE_PARAM_SIZE 0x38
#define DS_MSG_BONUS   0x6849
#define DS_MSG_STATUS  0x69e9
#define MSG_SIZE       0x1a
#define LAYER2_BASE    0x4218

/* Recovered from the draw routines at 1000:3a0c..42b4. */
static const SpriteDef sprite_defs[SPR_COUNT] = {
	[SPR_PADDLE]     = { "PADDLE",    0x0000, 50, 12, 50,      0,  1, 0xd8, -1 },
	[SPR_CLAW_L]     = { "CLAW L",    0x0258,  4,  7,  4,      0,  1, 0xd8, -1 },
	[SPR_CLAW_R]     = { "CLAW R",    0x0274,  4,  7,  4,      0,  1, 0xd8, -1 },
	[SPR_CANNON]     = { "CANNON",    0x02c8,  9,  6,  9,      0,  1, 0xd8, -1 },
	[SPR_HOLD_BAR]   = { "HOLD BAR",  0x0334, 44,  5, 44,      0,  1, 0xd8, -1 },
	[SPR_BALL]       = { "BALL",      0x0410,  9,  9,  9,      0,  1, 0xfa, -1 },
	[SPR_SHOT]       = { "SHOT",      0x0462,  7, 15,  7,      0,  1, 0xfa, -1 },
	[SPR_GEM]        = { "GEM",       0x04cc, 15, 15, 15,  0x0e1,  4, 0xf3, -1 },
	[SPR_CAPSULE]    = { "CAPSULE",   0x0850, 24, 22, 24,  0x210, 10, 0xf3, -1 },
	[SPR_BONUS_ICON] = { "ICON",      0x1cf0, 15, 15, 15,  0x0e1, 11, 0xf3, 0x0b },
	[SPR_SPARK]      = { "SPARK",     0x269b, 11, 11, 11,  0x079,  5, 0xf3, -1 },
	[SPR_FLASH]      = { "FLASH",     0x28f9, 15, 15, 16,  0x100,  8, 0xf3, -1 },
	[SPR_DEBRIS]     = { "DEBRIS",    0x30f9, 16, 16, 16,  0x100,  6, 0xf6, -1 },
	[SPR_EXPLOSION]  = { "EXPLOSION", 0x36f9, 32, 32, 32,  0x400,  6, 0xf6, -1 },
	[SPR_LIFE]       = { "LIFE",      0x7359, 19,  8, 19,      0,  1, 0xf3, 0x01 },
	[SPR_GEM_ICON]   = { "GEM ICON",  0x73f1, 20, 11, 20,      0,  1, 0xf3, 0x0b },
	[SPR_DIGIT]      = { "DIGITS",    0x74cd,  9,  9,  9,  0x051, 10, 0xf3, 0x0b },
	[SPR_BIG_LETTER] = { "LETTERS",   0x4ef9, 26, 32, 26,  0x340,  5, 0xf3, 0x0b },
	[SPR_PAUSED]     = { "PAUSED",    0x5f39,161, 32,161,      0,  1, 0xf3, 0x0b },
};

static Sprite sprites[SPR_COUNT];

int assets_init(const char *exe_path)
{
	int i;

	if (!exe_load(exe_path))
		return 0;

	for (i = 0; i < SPR_COUNT; i++) {
		const SpriteDef *d = &sprite_defs[i];
		size_t len = (size_t)d->frame_size * (d->frames - 1) + (size_t)d->stride * d->h;

		sprites[i].def = d;
		sprites[i].pixels = exe_ptr(SEG_SPRITES, d->off, len);
		if (!sprites[i].pixels) {
			fprintf(stderr, "sprite %s out of image bounds\n", d->name);
			return 0;
		}
	}

	if (!exe_ptr(SEG_BRICKS, 0, NUM_BRICK_GFX * BRICK_BYTES) ||
	    !exe_ptr(SEG_TILES, 0, NUM_TILE_GFX * BRICK_BYTES) ||
	    !exe_ptr(SEG_DATA, DS_STAGE_PARAM, NUM_STAGE_PARAMS * STAGE_PARAM_SIZE) ||
	    !exe_ptr(SEG_LEVELS_A, 0, 40 * LEVEL_SIZE) ||
	    !exe_ptr(SEG_LEVELS_B, 0, (NUM_LAYOUTS - 40) * LEVEL_SIZE) ||
	    !exe_ptr(SEG_LAYER2, LAYER2_BASE, NUM_LAYERS2 * LEVEL_SIZE)) {
		fprintf(stderr, "%s does not look like the Cybersphere executable\n", exe_path);
		return 0;
	}
	return 1;
}

const Sprite *sprite_get(SpriteId id)
{
	return &sprites[id];
}

const uint8_t *sprite_frame(SpriteId id, int frame)
{
	const Sprite *s = &sprites[id];

	if (frame < 0)
		frame = 0;
	if (frame >= s->def->frames)
		frame = s->def->frames - 1;
	return s->pixels + (size_t)frame * s->def->frame_size;
}

const uint8_t *brick_gfx(int index)
{
	return exe_ptr(SEG_BRICKS, (uint16_t)((index & 0xff) * BRICK_BYTES), BRICK_BYTES);
}

const uint8_t *tile_gfx(int index)
{
	return exe_ptr(SEG_TILES, (uint16_t)((index & 0xff) * BRICK_BYTES), BRICK_BYTES);
}

const BrickType *brick_type(uint8_t type)
{
	return (const BrickType *)exe_ptr(SEG_DATA, (uint16_t)(DS_BRICK_TYPES + type * 4), 4);
}

const uint8_t *font_glyph(char c)
{
	int idx = ((unsigned char)c - 0x20) & 0x3f;
	return exe_ptr(SEG_DATA, (uint16_t)(DS_FONT + idx * 8), 8);
}

/* Table at DS:87FE, as indexed by 1000:1fab. */
int stage_param_index(int episode, int level)
{
	const uint8_t *t = exe_ptr(SEG_DATA, DS_STAGE_TABLE, NUM_EPISODES * LEVELS_PER_EPISODE);
	int v;
	if (!t || episode < 0 || episode >= NUM_EPISODES || level < 0 || level >= LEVELS_PER_EPISODE)
		return -1;
	v = t[episode * LEVELS_PER_EPISODE + level] - 1;
	return v < 0 ? 0 : v;
}

int stage_params(int index, StageParams *out)
{
	const uint8_t *p;
	int i;

	if (index < 0 || index >= NUM_STAGE_PARAMS)
		return 0;
	p = exe_ptr(SEG_DATA, (uint16_t)(DS_STAGE_PARAM + index * STAGE_PARAM_SIZE), STAGE_PARAM_SIZE);
	if (!p)
		return 0;
	out->layout = rd16(p + 0);
	out->layer2 = rd16(p + 2);
	out->music = rd16(p + 4);
	if (out->layout < 1 || out->layout > NUM_LAYOUTS || out->layer2 > NUM_LAYERS2 || out->music > 9)
		return 0;
	for (i = 0; i < NUM_PULSES; i++) {
		const uint8_t *q = p + 6 + i * 10;
		out->pulse[i].phase = rd16(q + 0);
		out->pulse[i].speed = rd16(q + 2);
		out->pulse[i].start = rd16(q + 4);
		out->pulse[i].count = rd16(q + 6);
		out->pulse[i].min_bright = rd16(q + 8);
		if (out->pulse[i].start + out->pulse[i].count > 256 || out->pulse[i].min_bright > 256)
			return 0;
	}
	return 1;
}

int layout_load(int layout, LevelData *out)
{
	const uint8_t *p;

	if (layout < 1 || layout > NUM_LAYOUTS)
		return 0;
	if (layout <= 40)
		p = exe_ptr(SEG_LEVELS_A, (uint16_t)((layout - 1) * LEVEL_SIZE), LEVEL_SIZE);
	else
		p = exe_ptr(SEG_LEVELS_B, (uint16_t)((layout - 41) * LEVEL_SIZE), LEVEL_SIZE);
	if (!p)
		return 0;
	memcpy(out, p, LEVEL_SIZE);
	return 1;
}

int layer2_load(int layer, LevelData *out)
{
	if (layer < 1 || layer > NUM_LAYERS2) {
		memset(out, 0, sizeof(*out));
		return layer == 0;
	}
	memcpy(out, exe_ptr(SEG_LAYER2, (uint16_t)(LAYER2_BASE + (layer - 1) * LEVEL_SIZE), LEVEL_SIZE),
	       LEVEL_SIZE);
	return 1;
}

const uint8_t *game_message(int bank, int index)
{
	uint16_t base = bank ? DS_MSG_STATUS : DS_MSG_BONUS;
	return exe_ptr(SEG_DATA, (uint16_t)(base + index * MSG_SIZE), MSG_SIZE);
}

const uint8_t *ds_ptr(uint16_t off, uint16_t len)
{
	return exe_ptr(SEG_DATA, off, len);
}

uint16_t ds_word(uint16_t off)
{
	return rd16(exe_ptr(SEG_DATA, off, 2));
}

int sine(int angle)
{
	return (int8_t)exe_ptr(SEG_DATA, (uint16_t)(0x59a4 + (angle & 0xff)), 1)[0];
}

uint32_t brick_entry(uint8_t type)
{
	const uint8_t *p = exe_ptr(SEG_DATA, (uint16_t)(DS_BRICK_TYPES + type * 4), 4);
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
