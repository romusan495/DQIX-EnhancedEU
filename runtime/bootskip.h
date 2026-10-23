/* Modern: start like a PC game (bootskip.c). The logos (Nintendo, Square
 * Enix, Mobiclip, Level-5) run fast forward behind a black window, and
 * optionally the opening movie is skipped too, so the game opens on its own
 * first screen (the adventure logs over the title picture). The game itself
 * does everything as usual: only time runs faster and START is pressed. */
#pragma once
#include <stdint.h>

enum { BOOTSKIP_OFF, BOOTSKIP_LOGOS, BOOTSKIP_MOVIE };   /* settings.ini skip_intro = 0, 1, 2 */

void bootskip_start(int mode);       /* at launch (not in sessions or replays): from this frame on */
int bootskip_active(void);           /* skipping now: the frontend fast-forwards and shows nothing */
uint16_t bootskip_keys(void);        /* input.c, every frame: the buttons it presses */
void bootskip_card_read(uint32_t addr);   /* card.c: the game reads the ROM there */
