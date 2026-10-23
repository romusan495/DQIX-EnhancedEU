/* Modern: start like a PC game (bootskip.h).
 *
 * From power-on the game shows its logos for about 15 s (they can't be
 * skipped on a DS), then the opening movie, which START skips. Here the
 * frontend runs those frames as fast as the PC can without showing them. The
 * game's own file reads say where it is: the movie starts when it reads
 * data/movie/TD_nouhin_DS_form.mods, and its first screen when it reads
 * data/menu/bg_title_under.pac (the title picture under the adventure logs).
 * The names are looked up in the ROM's file table, so they hold for every
 * version of the game. */
#include <string.h>
#include "nds.h"
#include "bootskip.h"

#define START_KEY   (1 << 3)
#define TITLE_FADE  70           /* frames from reading the title picture until it has faded in */
#define GIVE_UP     4000         /* never skip longer than this (about a minute of DS time) */

static int g_mode, g_on;
static uint32_t g_movie[2], g_title[2];   /* where those files are in the ROM */
static int g_movie_seen, g_title_seen;
static uint64_t g_end;

static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* The ROM file `path` ("data/menu/x.pac"): its start and end in the ROM, 0 if not found. */
static int rom_file(const char* path, uint32_t out[2]) {
    if (!g_rom || g_rom_size < 0x200) return 0;
    uint32_t fnt = rd32(g_rom + 0x40), fnt_n = rd32(g_rom + 0x44), fat = rd32(g_rom + 0x48), fat_n = rd32(g_rom + 0x4C);
    if ((size_t)fnt + fnt_n > g_rom_size || (size_t)fat + fat_n > g_rom_size) return 0;
    int dir = 0;
    for (const char* part = path; ; ) {
        const char* slash = strchr(part, '/');
        size_t len = slash ? (size_t)(slash - part) : strlen(part);
        uint32_t off = rd32(g_rom + fnt + dir * 8);
        int fid = g_rom[fnt + dir * 8 + 4] | g_rom[fnt + dir * 8 + 5] << 8, next = -1;
        for (const uint8_t* p = g_rom + fnt + off; p < g_rom + fnt + fnt_n && *p; ) {
            int b = *p++, n = b & 0x7F;
            int match = (size_t)n == len && !memcmp(p, part, len);
            p += n;
            if (b & 0x80) { int sub = (p[0] | p[1] << 8) & 0xFFF; p += 2; if (match && slash) { next = sub; break; } }
            else { if (match && !slash) {
                    if ((uint32_t)fid >= fat_n / 8) return 0;
                    out[0] = rd32(g_rom + fat + fid * 8); out[1] = rd32(g_rom + fat + fid * 8 + 4);
                    return out[1] > out[0]; }
                fid++; }
        }
        if (next < 0) return 0;
        dir = next; part = slash + 1;
    }
}

void bootskip_start(int mode) {
    g_mode = mode; g_on = 0;
    g_movie_seen = g_title_seen = 0;
    if (mode == BOOTSKIP_OFF) return;
    if (!rom_file("data/movie/TD_nouhin_DS_form.mods", g_movie) || !rom_file("data/menu/bg_title_under.pac", g_title)) {
        LOG_WARN("start: the logos can't be skipped (files not found)\n");
        return;
    }
    g_on = 1;
    g_end = 0;
    LOG_INFO("start: skipping the logos%s\n", mode == BOOTSKIP_MOVIE ? " and the opening movie" : "");
}

static void stop(const char* why) {
    if (!g_on) return;
    g_on = 0;
    LOG_INFO("start: %s at frame %llu\n", why, (unsigned long long)g_frame);
}

int bootskip_active(void) {
    if (!g_on) return 0;
    if (g_frame >= GIVE_UP) stop("gave up skipping");
    else if (g_end && g_frame >= g_end) stop("the game's first screen");
    return g_on;
}

uint16_t bootskip_keys(void) {
    if (!bootskip_active() || g_mode != BOOTSKIP_MOVIE || !g_movie_seen || g_title_seen) return 0;
    return g_frame % 20 < 4 ? START_KEY : 0;     /* START skips the movie (pressed, let go, pressed...) */
}

void bootskip_card_read(uint32_t addr) {
    if (!g_on) return;
    if (!g_movie_seen && addr >= g_movie[0] && addr < g_movie[1]) {
        g_movie_seen = 1;
        if (g_mode == BOOTSKIP_LOGOS) stop("the opening movie");
    }
    if (!g_title_seen && addr >= g_title[0] && addr < g_title[1]) {
        g_title_seen = 1;
        g_end = g_frame + TITLE_FADE;
    }
}
