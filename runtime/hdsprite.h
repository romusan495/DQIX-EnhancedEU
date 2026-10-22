/* HD sprites (Enhanced, OpenGL): a sprite whose picture is in the HD texture
 * pack (textures/<name>.png, gl_render.c) is left out of the 2D picture and
 * listed here; the GL renderer draws the HD picture over it at full
 * resolution. "Dump textures" also writes each sprite to textures/dump-2d/. */
#pragma once
#include <stdint.h>

typedef struct { int x, y, w, h, hflip, vflip; uint64_t key; } HdSprite;   /* DS pixels on its screen */
#define HDSPRITE_MAX 128

/* set by the renderer: on (the pack or the dump), and the check of one
 * sprite's picture (w x h RGBA, see-through pixels 0): 1 if it has an HD version */
extern int g_hdsprite_on;
extern int (*g_hdsprite_check)(const uint8_t* rgba, int w, int h, uint64_t* key);

/* this frame's replaced sprites, per engine, in drawing order (back to front) */
extern HdSprite g_hdsprites[2][HDSPRITE_MAX];
extern int g_hdsprite_n[2];
