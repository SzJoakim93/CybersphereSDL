#include <stdio.h>
#include "video.h"
#include "gfx.h"

static SDL_Surface *screen;
static int windowed_width = 640;
static int windowed_height = 400;
static int fullscreen_mode;
static SDL_Rect view; /* where the 320x200 image lands in the window */

static void update_view(void)
{
	int w = screen->w, h = screen->h;

	/* keep 320:200, letterbox the rest */
	if (w * SCREEN_H > h * SCREEN_W) {
		view.h = (Uint16)h;
		view.w = (Uint16)(h * SCREEN_W / SCREEN_H);
	} else {
		view.w = (Uint16)w;
		view.h = (Uint16)(w * SCREEN_H / SCREEN_W);
	}
	view.x = (Sint16)((w - view.w) / 2);
	view.y = (Sint16)((h - view.h) / 2);
}

int video_set_mode(int width, int height, int fullscreen)
{
	Uint32 flags = SDL_SWSURFACE | SDL_ANYFORMAT;
	SDL_Surface *new_screen;

	if (fullscreen)
		flags |= SDL_FULLSCREEN;
	else
		flags |= SDL_RESIZABLE;

	new_screen = SDL_SetVideoMode(width, height, 32, flags);
	if (!new_screen) {
		fprintf(stderr, "Video mode could not be changed: %s\n", SDL_GetError());
		return 0;
	}

	screen = new_screen;
	fullscreen_mode = fullscreen;
	if (!fullscreen) {
		windowed_width = width;
		windowed_height = height;
	}
	SDL_FillRect(screen, NULL, 0);
	update_view();
	return 1;
}

int video_init(int width, int height, int fullscreen)
{
	SDL_WM_SetCaption("Cybersphere SDL", NULL);
	return video_set_mode(width, height, fullscreen);
}

void video_toggle_fullscreen(void)
{
	if (fullscreen_mode) {
		video_set_mode(windowed_width, windowed_height, 0);
	} else {
		const SDL_VideoInfo *vi = SDL_GetVideoInfo();
		int w = vi && vi->current_w > 0 ? vi->current_w : 640;
		int h = vi && vi->current_h > 0 ? vi->current_h : 400;
		if (!video_set_mode(w, h, 1))
			video_set_mode(windowed_width, windowed_height, 0);
	}
}

void video_resize(int width, int height)
{
	if (!fullscreen_mode)
		video_set_mode(width, height, 0);
}

void video_present(int curtain)
{
	Uint32 lut[256];
	int x, y, black_x = 0;

	for (x = 0; x < 256; x++) {
		const uint8_t *c = &gfx_pal[x * 3];
		lut[x] = SDL_MapRGB(screen->format, (Uint8)(c[0] << 2 | c[0] >> 4),
				    (Uint8)(c[1] << 2 | c[1] >> 4), (Uint8)(c[2] << 2 | c[2] >> 4));
	}

	if (curtain >= 40)
		black_x = SCREEN_W / 2;
	else if (curtain > 0)
		black_x = curtain * 4;

	if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0)
		return;

	for (y = 0; y < view.h; y++) {
		int sy = y * SCREEN_H / view.h;
		const uint8_t *src = &gfx_fb[sy * SCREEN_W];
		Uint8 *row = (Uint8 *)screen->pixels + (y + view.y) * screen->pitch;
		int bpp = screen->format->BytesPerPixel;

		for (x = 0; x < view.w; x++) {
			int sx = x * SCREEN_W / view.w;
			Uint32 c = (sx < black_x || sx >= SCREEN_W - black_x) ? lut[0] : lut[src[sx]];
			Uint8 *p = row + (x + view.x) * bpp;

			switch (bpp) {
			case 4: *(Uint32 *)p = c; break;
			case 2: *(Uint16 *)p = (Uint16)c; break;
			case 3:
				p[0] = (Uint8)c; p[1] = (Uint8)(c >> 8); p[2] = (Uint8)(c >> 16);
				break;
			default: *p = (Uint8)c; break;
			}
		}
	}

	if (SDL_MUSTLOCK(screen))
		SDL_UnlockSurface(screen);
	SDL_Flip(screen);
}

int video_save_bmp(const char *path)
{
	return SDL_SaveBMP(screen, path) == 0;
}

void video_to_game(int wx, int wy, int *gx, int *gy)
{
	*gx = view.w ? (wx - view.x) * SCREEN_W / view.w : 0;
	*gy = view.h ? (wy - view.y) * SCREEN_H / view.h : 0;
}

int video_game_dx(int wdx)
{
	return view.w ? wdx * SCREEN_W * 64 / view.w : 0;
}
