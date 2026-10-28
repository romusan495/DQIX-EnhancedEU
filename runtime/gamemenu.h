/* The in-game menu (Esc, or the controller's PS / Xbox / Home button): every
 * setting of the menu bar, drawn over the game in the window and used with
 * the controller, the keyboard or the mouse. It lists the same tree as the
 * bar (menu.h, menu_nodes) with the same check marks; choosing an item gives
 * its MI_ id back to the frontend, which does what the bar would. */
#pragma once
#include <stdint.h>

enum { GM_UP, GM_DOWN, GM_OK, GM_BACK, GM_CLOSE, GM_PAGEUP, GM_PAGEDOWN };
#define GM_RESUME (-1)            /* "Resume": the frontend closes the menu */

int gamemenu_is_open(void);
void gamemenu_open(void);
void gamemenu_close(void);
void gamemenu_set_font(const char* name);   /* a font in fonts/ (the HD text's), else a system one */
void gamemenu_changed(void);                /* check marks or texts changed: draw again */
void gamemenu_set_readability(int contrast, int size);   /* View > Readability: stronger contrast (0/1); text size 0 normal, 1 large, 2 larger */
/* Navigation; returns the chosen item's MI_ id (or GM_RESUME), else 0. */
int gamemenu_key(int key);
/* The mouse at (x, y) in the window's drawable pixels; click 1 = left, 2 = right (back). */
int gamemenu_mouse(float x, float y, int click);
void gamemenu_wheel(int dy);
/* The picture to draw over the whole window (premultiplied RGBA, w x h, rows
 * top-down): *changed says whether it differs from the last call's. */
const uint8_t* gamemenu_picture(int w, int h, int* changed);
/* ...and where its window is, for the renderer to draw it under the picture:
 * x0 y0 x1 y1 and the frame's line width (drawable pixels); NULL when closed. */
const float* gamemenu_box(void);
