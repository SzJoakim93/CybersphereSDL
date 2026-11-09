#ifndef VIDEO_H
#define VIDEO_H

#include <SDL/SDL.h>

int video_init(int width, int height, int fullscreen);
int video_set_mode(int width, int height, int fullscreen);
void video_toggle_fullscreen(void);
void video_resize(int width, int height);

/* Copies gfx_fb through gfx_pal to the window. curtain mirrors DS:872E
 * (1000:39a8): 1..39 blanks curtain*4 px on both sides, >=40 blanks all. */
void video_present(int curtain);

int video_save_bmp(const char *path);

/* Window coordinates -> 320x200 coordinates. */
void video_to_game(int wx, int wy, int *gx, int *gy);
/* Horizontal window motion -> game motion in 1/64 px. */
int video_game_dx(int wdx);

#endif
