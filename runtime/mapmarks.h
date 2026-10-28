/* Modern: markers on the map (the top screen) for the chests, pots,
 * barrels, bookshelves and quest givers of the place the party is in
 * (mapmarks.c). */
#pragma once
#include <stdint.h>

extern int g_mapmarks;             /* show them */
extern int g_mapmarks_opened;      /* ...also the ones already emptied or done (greyed) */
extern int g_mapmarks_monsters;    /* the monsters walking nearby, as red dots */
extern int g_interior_maps;        /* inside a house (the game shows the town): the room's plan instead, from its walls */

#define MAPMARKS_R 3               /* overlay pixels per DS pixel */
#define MAPMARKS_W (256 * MAPMARKS_R)
#define MAPMARKS_H (192 * MAPMARKS_R)

/* Once a frame: what the map shows now. 1 if the overlay picture changed
 * (*rgba: premultiplied RGBA, MAPMARKS_W x MAPMARKS_H, rows top-down, or
 * NULL when there is nothing to draw). */
int mapmarks_update(const uint8_t** rgba);
/* 1 while the top screen shows a page (an item's card) instead of the map. */
int mapmarks_top_page(void);
/* The mouse over the map, in top-screen pixels (x < 0: not over it). */
void mapmarks_hover(float x, float y);
/* Once a frame, with the field's screen (its 2D picture and how it is
 * composed with the 3D, gpu2d.c): 1 while a shop's buy list is on it. Its
 * lines' kind signs are rubbed out of px/info; *rgba: the items' icons over
 * that screen (premultiplied RGBA, MAPMARKS_W x MAPMARKS_H), *changed 1 when
 * that picture changed. */
int mapmarks_shop(uint32_t* px, uint8_t* info, const uint8_t** rgba, int* changed);
