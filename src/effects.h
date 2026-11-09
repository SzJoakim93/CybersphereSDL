#ifndef EFFECTS_H
#define EFFECTS_H

#include <stdint.h>

/* Purely visual animation pools as laid out at DS:8170..8470 of the original.
 * Spawn coordinates are 1/64 px; stored as debris 1/16 px, the rest whole px. */

#define MAX_SPARKS 10
#define MAX_FLASHES 10
#define MAX_DEBRIS 32
#define MAX_EXPLOSIONS 12
#define MAX_BRICK_FLASHES 24

typedef struct { int16_t x, y; uint16_t anim; } Spark;
typedef struct { uint16_t x, y, anim, speed; int16_t vx, vy; uint16_t friction; } Debris;
typedef struct { int16_t x, y; uint16_t anim, speed; } Explosion;
typedef struct { uint16_t timer, cell; } BrickFlash;

typedef struct {
	Spark sparks[MAX_SPARKS];
	Spark flashes[MAX_FLASHES];
	Debris debris[MAX_DEBRIS];
	Explosion explosions[MAX_EXPLOSIONS];
	BrickFlash brick_flashes[MAX_BRICK_FLASHES];
	int spark_next, flash_next, debris_next, explosion_next, brick_flash_next;
} Effects;

/* 1000:4506 */
void rng_seed(uint16_t seed);
int rng(int range);

void fx_reset(Effects *e);

void fx_spark(Effects *e, int x, int y);
void fx_flash(Effects *e, int x, int y);
void fx_debris(Effects *e, int x, int y, int vx, int vy, int speed, int friction);
void fx_explosion(Effects *e, int x, int y, int speed);
void fx_brick_flash(Effects *e, int cell);

/* 1000:2f54 */
void fx_update(Effects *e);
/* 1000:3e72 */
void fx_draw(const Effects *e, int shift);

#endif
