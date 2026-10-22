/* Modern: markers on the map (the top screen) for the chests, pots and
 * barrels of the place the party is in (mapmarks.c). */
#pragma once
#include <stdint.h>

extern int g_mapmarks;             /* show them */
extern int g_mapmarks_opened;      /* ...also the ones already emptied (greyed) */

#define MAPMARKS_R 3               /* overlay pixels per DS pixel */
#define MAPMARKS_W (256 * MAPMARKS_R)
#define MAPMARKS_H (192 * MAPMARKS_R)

/* Once a frame: what the map shows now. 1 if the overlay picture changed
 * (*rgba: premultiplied RGBA, MAPMARKS_W x MAPMARKS_H, rows top-down, or
 * NULL when there is nothing to draw). */
int mapmarks_update(const uint8_t** rgba);
/* The mouse over the map, in top-screen pixels (x < 0: not over it). */
void mapmarks_hover(float x, float y);
