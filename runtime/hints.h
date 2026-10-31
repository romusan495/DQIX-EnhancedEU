/* A button's sign as the player's own controller or keyboard shows it
 * (hints.c): drawn over the game's DS buttons ("X Menu" in the field). */
#pragma once
#include <stdint.h>

enum { HINT_KEYBOARD, HINT_GENERIC, HINT_PLAYSTATION, HINT_XBOX, HINT_NINTENDO };
enum { HINT_RIGHT, HINT_BOTTOM, HINT_TOP, HINT_LEFT, HINT_OTHER };   /* a face button by position, or another one (its label) */

/* Draws the sign over the whole of rgba (w x h, premultiplied RGBA, rows
 * top-down): a keyboard key like the game's own button (a blue octagon), or the controller's button (face:
 * which one by position; HINT_OTHER: label on a dark plate). */
void hint_draw(uint8_t* rgba, int w, int h, int style, int face, const char* label);

/* A keyboard key's picture, width / height (1: the game's 9 x 9 button; more:
 * a long one for a name). label: SDL's scancode name, or the character the key
 * types. It is drawn on the DS's pixels: 9 high, width 9 * this. */
float hint_key_aspect(const char* label);
