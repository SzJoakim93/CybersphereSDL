#include <stdio.h>
#include <stdlib.h>
#include "exe.h"

static uint8_t *image;
static size_t image_size;
static size_t image_hdr;
static int image_plus;

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
	image_plus = hdr + 15 < (size_t)size && buf[hdr] == 0x60 &&
		buf[hdr + 12] == 0xb8 && rd16(buf + hdr + 13) == 0x5620;
	return 1;
}

void exe_free(void)
{
	free(image);
	image = NULL;
	image_size = 0;
	image_hdr = 0;
	image_plus = 0;
}

int exe_is_plus(void)
{
	return image_plus;
}

const char *exe_config_path(void)
{
	return image_plus ? "CyberspherePlus-SDL.cfg" : "Cybersphere-SDL.cfg";
}

uint16_t exe_data_offset(uint16_t off)
{
	if (!image_plus)
		return off;
	if (off >= 0x70d6)
		return (uint16_t)(off - 0x24);
	if (off >= 0x6618)
		return (uint16_t)(off + 0x17);
	if (off >= 0x6537)
		return (uint16_t)(off + 0x13);
	if (off >= 0x5974)
		return (uint16_t)(off + 4);
	return off;
}

const uint8_t *exe_ptr(uint16_t seg, uint16_t off, size_t len)
{
	size_t lin;

	if (!image || seg < EXE_LOAD_SEG)
		return NULL;
	if (image_plus) {
		if (seg > 0x1575)
			seg -= 2;
		else if (seg == 0x1575)
			off = exe_data_offset(off);
	}
	lin = image_hdr + (size_t)(seg - EXE_LOAD_SEG) * 16 + off;
	if (lin + len > image_size)
		return NULL;
	return image + lin;
}
