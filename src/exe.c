#include <stdio.h>
#include <stdlib.h>
#include "exe.h"

static uint8_t *image;
static size_t image_size;
static size_t image_hdr;

int exe_load(const char *path)
{
	FILE *f = fopen(path, "rb");
	uint8_t *buf;
	long size;
	size_t hdr;

	if (!f)
		return 0;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size < 0x200) {
		fclose(f);
		return 0;
	}
	buf = malloc((size_t)size);
	if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
		free(buf);
		fclose(f);
		return 0;
	}
	fclose(f);

	hdr = (size_t)rd16(buf + 8) * 16;
	if (buf[0] != 'M' || buf[1] != 'Z' || hdr >= (size_t)size) {
		fprintf(stderr, "%s is not a DOS MZ executable\n", path);
		free(buf);
		return 0;
	}

	exe_free();
	image = buf;
	image_size = (size_t)size;
	image_hdr = hdr;
	return 1;
}

void exe_free(void)
{
	free(image);
	image = NULL;
	image_size = 0;
	image_hdr = 0;
}

const uint8_t *exe_ptr(uint16_t seg, uint16_t off, size_t len)
{
	size_t lin;

	if (!image || seg < EXE_LOAD_SEG)
		return NULL;
	lin = image_hdr + (size_t)(seg - EXE_LOAD_SEG) * 16 + off;
	if (lin + len > image_size)
		return NULL;
	return image + lin;
}
