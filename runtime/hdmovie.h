/* HD movies: the game's Mobiclip movies (the .mods files in data/movie: the opening, the
 * dragon, "to be continued") shown from upscaled copies instead.
 *
 * movies/<name>.mpg is the movie upscaled (MPEG-1, one picture per movie
 * frame) and movies/<name>.sig a small picture of each frame (16x12 colours,
 * tools/upscale_movies.py makes both from the ROM). While the game reads a
 * movie from the card, each screen's picture is compared with that movie's
 * frames; the one it shows is drawn from the HD copy instead, except where
 * the screen differs from it (something drawn over the movie keeps the
 * game's pixels). Nothing in the game changes. */
#pragma once
#include <stdint.h>

extern int g_hdmovie_on;                      /* set by the frontend (Enhanced only) */
extern void (*g_card_read_hook)(uint32_t addr, uint32_t len);   /* card.c: every ROM read (NULL: none) */

void hdmovie_init(void);                      /* look in movies/ (again); how many were found */
int hdmovie_count(void);

/* One screen's picture this frame (0xRRGGBB, 256x192). Returns 1 when it is a
 * frame of an HD movie: *rgb = that frame (*w x *h, 3 bytes a pixel), *mask
 * = 256x192, 255 where the HD frame is drawn; *fresh = 1 when *rgb changed
 * since the last call for this screen. */
int hdmovie_frame(int eng, const uint32_t* px, const uint8_t** rgb, int* w, int* h, const uint8_t** mask, int* fresh);
