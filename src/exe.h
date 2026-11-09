#ifndef EXE_H
#define EXE_H

#include <stdint.h>
#include <stddef.h>

/* Access to the load image of the original CYBER.EXE (DOS MZ).
 * Segments are given as Ghidra shows them (load segment 0x1000). */

#define EXE_LOAD_SEG 0x1000

int exe_load(const char *path);
void exe_free(void);

/* Returns NULL if [seg:off, seg:off+len) is outside the load image. */
const uint8_t *exe_ptr(uint16_t seg, uint16_t off, size_t len);

static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

#endif
