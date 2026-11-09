#ifndef ASSETS_H
#define ASSETS_H

#include <stdint.h>

/* Segments of the CYBER.EXE load image (as relocated by Ghidra). */
#define SEG_DATA      0x1575 /* DS: tables, font, strings */
#define SEG_SPRITES   0x1ef4 /* paddle, ball, gems, capsules, effects, HUD */
#define SEG_BRICKS    0x315c /* 200 brick bitmaps, 20x10 */
#define SEG_TILES     0x3b20 /* 136 background tiles, 20x10 */
#define SEG_LAYER2    0x41c4 /* background layers at +0x4218 */
#define SEG_LEVELS_A  0x47f7 /* layouts 1..40 */
#define SEG_LEVELS_B  0x55bc /* layouts 41..51 */

#define SCREEN_W 320
#define SCREEN_H 200

#define GRID_COLS 16
#define GRID_ROWS 20
#define GRID_CELLS (GRID_COLS * GRID_ROWS)
#define BRICK_W 20
#define BRICK_H 10
#define BRICK_BYTES (BRICK_W * BRICK_H)

#define LEVEL_SIZE 0x582
#define NUM_LAYOUTS 51
#define NUM_LAYERS2 6
#define NUM_BRICK_GFX 200
#define NUM_TILE_GFX 136
#define NUM_EPISODES 6
#define LEVELS_PER_EPISODE 10
#define NUM_STAGE_PARAMS 51
#define NUM_PULSES 5
#define NUM_BONUS_TYPES 11

typedef enum {
	SPR_PADDLE,     /* 50x12, wide variant is stitched from it */
	SPR_CLAW_L,     /* hold bonus claws on paddle ends */
	SPR_CLAW_R,
	SPR_CANNON,     /* fire bonus cannons */
	SPR_HOLD_BAR,   /* bar shown while a ball is held */
	SPR_BALL,
	SPR_SHOT,
	SPR_GEM,        /* 4 frames, played 0,1,2,3,2,1 */
	SPR_CAPSULE,    /* 10 frames growing bonus bubble */
	SPR_BONUS_ICON, /* 11 icons, one per bonus type */
	SPR_SPARK,
	SPR_FLASH,
	SPR_DEBRIS,
	SPR_EXPLOSION,
	SPR_LIFE,
	SPR_GEM_ICON,
	SPR_DIGIT,
	SPR_BIG_LETTER, /* 26x32 italic letters F,A,T,L,B of FATAL BALL */
	SPR_PAUSED,
	SPR_COUNT
} SpriteId;

typedef struct {
	const char *name;
	uint16_t off;        /* offset in SEG_SPRITES */
	uint8_t w, h;
	uint8_t stride;      /* source row pitch */
	uint16_t frame_size; /* bytes between frames */
	uint8_t frames;
	uint8_t base;        /* added to each non-zero pixel */
	int16_t hole;        /* source value drawn as black (0), -1 if none */
} SpriteDef;

typedef struct {
	const SpriteDef *def;
	const uint8_t *pixels; /* first frame */
} Sprite;

/* One cell of the brick type table (DS:5CA4, 256 entries). */
typedef struct {
	uint8_t gfx;   /* brick bitmap index, 0 = background only */
	uint8_t next;  /* type after being hit */
	uint8_t mode;  /* high nibble: 0 plain, 1 tile+overlay, 2 cut corners */
	uint8_t flags;
} BrickType;

/* Raw 0x582-byte level/layer record. */
typedef struct {
	uint8_t shift;          /* odd layout: grid shifted 10px left with wrap */
	uint8_t unused;
	uint8_t type[GRID_CELLS];
	uint8_t color[GRID_CELLS]; /* palette offset added to brick pixels */
	uint8_t pal[768];          /* VGA 6-bit RGB */
} LevelData;

typedef struct {
	uint16_t phase, speed, start, count, min_bright;
} PalPulse;

typedef struct {
	uint16_t layout; /* 1-based, 0 = custom */
	uint16_t layer2; /* 1-based, 0 = none */
	uint16_t music;
	PalPulse pulse[NUM_PULSES];
} StageParams;

int assets_init(const char *exe_path);

const Sprite *sprite_get(SpriteId id);
const uint8_t *sprite_frame(SpriteId id, int frame);

const uint8_t *brick_gfx(int index);
const uint8_t *tile_gfx(int index);
const BrickType *brick_type(uint8_t type);
const uint8_t *font_glyph(char c);

int stage_param_index(int episode, int level);
int stage_params(int index, StageParams *out);
int layout_load(int layout, LevelData *out);
int layer2_load(int layer, LevelData *out);

/* Text block in the original format: [x/2][y]{color chars... 0 color ...}0xFF ... 0xFF */
const uint8_t *game_message(int bank, int index);

/* Raw access to the data segment (tables, text blocks). */
const uint8_t *ds_ptr(uint16_t off, uint16_t len);
uint16_t ds_word(uint16_t off);
/* 256-entry signed sine table at DS:59A4, amplitude 127 */
int sine(int angle);
/* Raw 32-bit brick type table entry */
uint32_t brick_entry(uint8_t type);

#endif
