/* SDL2 frontend: a window with both screens, keyboard / mouse / gamepad
 * input, audio output and pacing to the DS frame rate (~59.83 fps).
 *
 * Keys (DS button <- keyboard):
 *   D-pad <- arrow keys     A <- X      B <- Z      X <- S      Y <- A
 *   L <- Q                  R <- W      START <- Enter         SELECT <- Right Shift / Backspace
 *   touch screen <- left mouse button on the bottom screen
 *   Tab (hold) = fast forward     F11 or Alt+Enter = fullscreen (mouse at the top edge shows the menu bar)
 *   F2 = layout: stacked, side by side, top screen in the corner, widescreen, widescreen dynamic (two windows)
 *   U = single screen (widescreen: hide the small one)   F9 = swap the screens (widescreen: hold to see the other)
 *   K / I / O = widescreen's small screen: corner, size, opacity    Ctrl+R = reset   - / = = volume
 *   Every key and controller button can be changed (controls.c, Game > Controls).
 *   F3 = window size 1x..4x       F4 = 3D resolution 1x..4x (Enhanced)  F5 = Classic / Enhanced
 *   F6 = DS Lite screen filter    F7 = DS Lite console around the screens   P = pause
 *   F8 = actual size (the screens as big as a real DS Lite's)
 *   F12 = screenshot (dqix-shot-N.bmp)
 *   F10 = bug report (reports/<date-time>/: screenshot, inputs, saves, log)
 *   Esc = leave fullscreen, or quit (the save file is always up to date)
 * On Windows every setting is also in the menu bar (menu_win32 below).
 * A game controller works too (Nintendo layout by position: right face button = A). */
#define SDL_MAIN_HANDLED
#include "SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <stdarg.h>
#include "nds.h"
#include "frontend.h"
#include "hdtext.h"
#include "hdmovie.h"
#include "bootskip.h"
#include "gamemenu.h"
#include "gl_render.h"
#include "menu.h"
#include "farm.h"
#include "autosave.h"
#include "cheats.h"
#include "mapmarks.h"
#include "journal.h"
#include "version.h"
#include "session.h"
#include "air.h"
#include "update.h"
#include "text.h"
#include "controls.h"
#include "uitext.h"
#include "hints.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define DS_FPS 59.8261

static SDL_Window* g_win;
static SDL_Renderer* g_ren;
static SDL_Texture* g_tex;
static SDL_AudioDeviceID g_audio;
static SDL_GameController* g_pad;
static int g_side_by_side, g_corner, g_scale = 3, g_fullscreen;   /* g_corner: the top screen small, beside the bottom one */
static int g_wide = 1;                                            /* widescreen: the 3D screen wider than the DS's, the other small in its corner */
static float g_wide_mix = 1.0f;                                   /* widescreen: 1 = the field wide, 0 = beside the small screen (2D under its corner) */
static int g_top_menu;                                            /* widescreen: a menu with a 3D model on the top screen (wide_update) */
static int g_small_blank;                                         /* widescreen: the small screen shows nothing (all black): not drawn */
static int g_swap;                                                /* the screens trade places (F9) */
static int g_two;                                                 /* two windows, one screen in each (two monitors) */
static SDL_Window* g_win2;                                        /* ...the second one (the top screen, or the bottom one when swapped) */
static int g_win2_full, g_win2_rect[4];                           /* ...fullscreen; where it was as a window (w = 0: not yet) */
static int g_wide_pop = 0;                                        /* widescreen: the small screen in its own window (the field alone; no longer offered) */
static int g_hints = 1;                                           /* the DS buttons the game shows ("X Menu"): the player's own (hints.c) */
static int g_hint_pad = -1;                                       /* ...the last used: 1 the controller, 0 the keyboard, -1 not yet */
static int g_minimap = 1;                                         /* widescreen: the map small in a corner of the one window (no second window), not in battles */
static int g_map_shown = 1;                                       /* ...shown (Modern > Map in the top right corner) */
static float g_mini_vis = 1.0f;                                   /* ...1 shown, 0 gone (the game's 2D under it: a menu) */
/* An adventure is loaded (the field's characters, farm.c): before it (the
 * logos, the opening movie, the title) the minimap isn't shown. */
static int in_adventure(void) { uint32_t v; memcpy(&v, &g_main_ram[0x0F33E0], 4); return v != 0; }
#define MINIMAP_GAP 0                                             /* ...right in the corner (the field's "X Menu" sign moves to the bottom: g_sign_dy) */
static int g_smooth = 1;                                          /* Enhanced: smooth (filtered) textures */
static int g_smooth2d = 1, g_texup = 1, g_outline = 1;                           /* Enhanced: 2D smoothed, textures upscaled (xBR) */
static int g_softshadow, g_ink;                                                       /* Enhanced: soft shadows in corners and under things, thin dark lines round things */
static int g_texpack = 1, g_texdump = 0;                          /* Enhanced: HD textures from textures/, dumping them */
static int g_hdtext = 1;                                          /* Enhanced: the game's text drawn at full resolution (hdtext.h) */
static int g_hdmovie = 1;                                         /* Enhanced: the movies from movies/ in HD (hdmovie.h) */
static char g_hdfont[256] = "SourceSans3.ttf";                    /* ...with this font ("original": the game's letters, smoothed) */
static const char* const g_fonts[N_FONTS] = { "original", "georgia.ttf", "pala.ttf", "constan.ttf", "times.ttf", "segoeui.ttf",   /* MI_FONT0 + i: Windows' */
    "Lora.ttf", "CrimsonPro.ttf", "Alegreya.ttf", "Spectral.ttf", "EBGaramond.ttf", "Philosopher.ttf", "Marcellus.ttf", "Nunito.ttf", "Fredoka.ttf", "SourceSans3.ttf" };   /* ours (fonts/) */
static int g_res_auto = 1;                                        /* 3D resolution: as sharp as the screen shows it (else g_scale3d) */
static int g_small_opacity = 25;                                  /* widescreen: how solid the small screen over the field is (O), % */
static int g_wide_dyn = 1;                                        /* widescreen: 0 basic (the corner stays), 1 dynamic (experimental: slides for menus) */
static int g_small_corner = 1;                                    /* basic widescreen: 0 top left, 1 top right, 2 bottom right, 3 bottom left */
static int g_small_size = 1;                                      /* ...0 small, 1 medium, 2 large */
static int g_small_hide;                                          /* widescreen: no small screen (left trigger / U) */
static int g_peek;                                                /* widescreen: the other screen big while the swap button is held */
static int g_single;                                              /* one screen only (the bottom one, or the top one swapped) */
static int g_hud = 1;                                             /* widescreen battles: the party's panels and the commands over the 3D (hud_update) */
static int g_hud_n, g_hud_band[8][2];                             /* ...the panels found on the commands screen: first row, end row */
static int g_pop_win[4];                                          /* widescreen: a page on the top screen without its backdrop, popped up (pop_update): x, y, w, h (w 0: none) */
static float g_pop_at[3];                                         /* ...where on the canvas: x, y, scale (pop_place) */
static int g_shop;                                                /* widescreen: a shop's buy list on the field, its item's description from the top screen under it (shop_update) */
static int g_shop_cut = 118;                                      /* ...the row between the list and the panels under it */
enum { SHOP_TX = 60, SHOP_TY = 33, SHOP_TW = 113, SHOP_TH = 58, SHOP_PAD = 3, SHOP_GAP = 3 };   /* the card's text, beside its icon; the description's window */
static int g_hud_win[4];                                          /* ...or a window alone there (experience, level up): x, y, w, h (w 0: none) */
static int g_volume = 100, g_mute;                                /* Game > Volume */
static char g_notice[160]; static uint64_t g_notice_until;        /* a short message in the title bar */
static float g_small_alpha = 1.0f;                                /* ...as shown now (solid while the mouse is on it) */
static uint64_t g_next_frame, g_perf_freq;
static int g_fast;
static uint32_t g_fps_frames; static uint64_t g_fps_t0;
static int g_headless;          /* DQIX_NOWINDOW: run without a window (still uses SDL audio off) */
static int g_hidden;            /* another player's console in a session: no window, no sound, no pacing */
static int g_gl;                /* OpenGL renderer active (else SDL_Renderer + software 3D) */
static SDL_GLContext g_glctx;
static int g_scale3d = 2;
extern void (*g_report_capture)(uint32_t out[2][256 * 192]);
extern void (*g_report_perf)(FILE* f);
extern void (*g_report_window)(const char* path);
static void perf_write(FILE* f);
static void report_window(const char* path);
static int g_classic = 0;       /* Classic: the DS's own 3D renderer, exact; Enhanced: 3D on the GPU at 1-4x */
/* Widescreen needs Enhanced: only the GPU's 3D can be drawn wider, and only it
 * says which screen holds the 3D (Classic showed the commands screen big and
 * the battle small in the corner). In Classic the layout waits, stacked. */
#define WIDE_GL (g_gl && !g_classic)
extern int g_comp_gl;
extern int g_winstyle;          /* Enhanced: windows like the DS (0), minimal (1) or modern (2), gpu2d.c */
extern int g_win_outline;       /* ...minimal: the commands screen's windows with their white line */
extern int g_dlg_outline;       /* ...and the 3D screen's (the text box in battles) */
extern int g_comp_win_px[2];    /* gpu2d.c: last frame, pixels of a window's panel per engine */
static int g_paused, g_turbo;   /* Game > Pause, Game > Fast forward (latched) */
static int g_battle_speed = 1;  /* Modern > Battle speed: 1 to 4 times as fast (not in a session) */
static int g_battle_turn_only;  /* ...only while the turn plays out: choosing the commands at the usual pace */
static int g_skip_intro = BOOTSKIP_LOGOS;   /* Modern > Start: skip the logos (1), and the opening movie (2) */
static int g_all_options;       /* every option in the menus and on the keys (settings.ini all_options), else only the project's own setup */

/* How the screens are shown (View and Screen menus). The DS Lite parts need OpenGL. */
static struct {
    int lcd;                     /* DS Lite screen filter */
    int lcd_color, grid, ghost;  /* ... its LCD colours, pixel grid, motion blur */
    int backlight;               /* 1 (low) .. 4 (max) */
    int gap;                     /* the real distance between the screens */
    int shell;                   /* draw the console around the screens */
    int shell_color;             /* index into SHELLS */
    int integer;                 /* whole-number scaling only */
    int actual;                  /* screens as big as a real DS Lite's */
    float monitor_width_mm;      /* settings.ini override for the monitor's width (0 = ask the monitor) */
    int dlc;                     /* switch on the downloadable quests and special guests when the save loads */
} g_opt = { 0, 1, 0, 0, 4, 0, 0, 0, 1, 0, 0, 1 };   /* a new install: plain screens, whole-number scaling, the downloads on */

/* Actual size: a DS Lite pixel is 0.238 mm (a 3-inch 4:3 screen, 256 pixels
 * across = 60.96 mm). Window pixels per DS pixel on the current monitor. */
#define DSLITE_MM_PER_PIXEL (60.96f / 256.0f)
static float g_actual_sc;        /* 0 = monitor size unknown */
static void update_actual(void) {
    g_actual_sc = 0;
    if (!g_win) return;
    float mm = 0;
    if (g_opt.monitor_width_mm > 0) {
        SDL_DisplayMode dm;
        if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(g_win), &dm) == 0 && dm.w > 0) mm = g_opt.monitor_width_mm / (float)dm.w;
    } else mm = display_mm_per_pixel(g_win);
    if (mm > 0) g_actual_sc = DSLITE_MM_PER_PIXEL / mm;
}

/* DS Lite colours: lid, base, buttons, hinge (sRGB). */
typedef struct { const char* name; const char* key; uint32_t lid, base, btn, hinge; } Shell;
static const Shell SHELLS[] = {
    { "Polar White", "white", 0xECECEA, 0xECECEA, 0xDCDCDA, 0xCFCFCC },
    { "Jet Black", "black", 0x222225, 0x222225, 0x2E2E32, 0x161618 },
    { "Ice Blue", "ice_blue", 0xBCD6E6, 0xBCD6E6, 0xE4EDF2, 0xA6C0D0 },
    { "Coral Pink", "coral_pink", 0xF0B7C1, 0xF0B7C1, 0xF6E1E5, 0xDBA0AB },
    { "Enamel Navy", "navy", 0x24314F, 0x24314F, 0x313E5F, 0x1A2440 },
    { "Crimson / Black", "crimson", 0xB21F2B, 0x222225, 0x2E2E32, 0x161618 },
};
#define N_DRAWN ((int)(sizeof SHELLS / sizeof SHELLS[0]))

/* Console photos: skins/<name>.png with skins/<name>.txt giving where the
 * screens are in the picture ("top x y w h", "bottom x y w h", optional
 * "name ..."). Listed after the drawn colours. */
#define MAX_SKINS 10
typedef struct { char file[200], name[64]; float top[4], bot[4]; int w, h; } Skin;
static Skin g_skins[MAX_SKINS];
static int g_nskins;
static int g_skin_loaded = -1;                 /* which photo is in the GPU */
#define N_SHELLS (N_DRAWN + g_nskins)

static void skin_add(const char* png) {
    if (g_nskins >= MAX_SKINS) return;
    Skin* k = &g_skins[g_nskins];
    memset(k, 0, sizeof *k);
    snprintf(k->file, sizeof k->file, "skins/%s", png);
    char txt[220]; snprintf(txt, sizeof txt, "%s", k->file);
    char* dot = strrchr(txt, '.'); if (!dot) return;
    strcpy(dot, ".txt");
    FILE* f = fopen(txt, "r");
    if (!f) return;
    char line[256]; int got = 0;
    snprintf(k->name, sizeof k->name, "%.*s (photo)", (int)(strrchr(png, '.') - png), png);
    while (fgets(line, sizeof line, f)) {
        float a, b, c, d;
        if (sscanf(line, " top %f %f %f %f", &a, &b, &c, &d) == 4) { k->top[0] = a; k->top[1] = b; k->top[2] = c; k->top[3] = d; got |= 1; }
        else if (sscanf(line, " bottom %f %f %f %f", &a, &b, &c, &d) == 4) { k->bot[0] = a; k->bot[1] = b; k->bot[2] = c; k->bot[3] = d; got |= 2; }
        else if (!strncmp(line, "name ", 5)) { snprintf(k->name, sizeof k->name, "%s", line + 5); k->name[strcspn(k->name, "\r\n")] = 0; }
    }
    fclose(f);
    if (got == 3 && k->top[2] > 0) g_nskins++;
}

#ifdef _WIN32
#include <io.h>
static void skins_scan(void) {
    struct _finddata_t fd; intptr_t h = _findfirst("skins/*.png", &fd);
    if (h == -1) return;
    do skin_add(fd.name); while (_findnext(h, &fd) == 0);
    _findclose(h);
}
#else
#include <dirent.h>
static void skins_scan(void) {
    DIR* d = opendir("skins");
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) { size_t n = strlen(e->d_name); if (n > 4 && !strcmp(e->d_name + n - 4, ".png")) skin_add(e->d_name); }
    closedir(d);
}
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO_WARNINGS
#include "stb_image.h"

/* The photo for the chosen console colour (if it is one), in the GPU. */
static const Skin* current_skin(void) {
    int i = g_opt.shell_color - N_DRAWN;
    if (i < 0 || i >= g_nskins || !g_gl) return NULL;
    if (g_skin_loaded != i) {
        int w, h, n;
        uint8_t* px = stbi_load(g_skins[i].file, &w, &h, &n, 4);
        if (!px) { fprintf(stderr, "can't load %s\n", g_skins[i].file); return NULL; }
        gl_set_skin(px, w, h);
        stbi_image_free(px);
        g_skins[i].w = w; g_skins[i].h = h;
        g_skin_loaded = i;
    }
    return &g_skins[i];
}
static const float BACKLIGHT[5] = { 0, 0.36f, 0.56f, 0.78f, 1.0f };
static const char* BACKLIGHT_NAME[5] = { "", "low", "medium", "high", "max" };

/* Canvas (in DS pixels) for the current layout, and where the screens sit on it.
 * The DS Lite: 0.24 mm per pixel; the lid and base are 133 mm wide; the
 * screens are 90 pixels (21.5 mm) apart. */
/* Widescreen: DS pixels to add left and right of the 3D picture. 16:9 in a
 * window; in fullscreen, as wide as the monitor (up to 2:1 more). */
static int wide_side_target(void) {
    if (!(WIDE_GL && g_wide && !g_side_by_side)) return 0;
    int side = 43;                                                /* (192 * 16 / 9 - 256) / 2 */
    if (g_fullscreen && g_win) {
        int w, h; SDL_GetWindowSize(g_win, &w, &h);
        if (h > 0) side = (int)lroundf(((float)w / (float)h * 192.0f - 256.0f) / 2.0f);
    }
    return side < 0 ? 0 : side > 128 ? 128 : side;
}

/* Widescreen with the minimap, in the adventure: a page on the top screen
 * instead of the map. 1: an item's card, a title... (the field on the bottom
 * screen, or no 3D): its windows pop up over the big screen; 2: a menu with
 * its 3D model on the top screen (the monster list): both screens side by
 * side; 0: none. (Not while the other screen is black: a cutscene.) */
static int top_page_mode(void) {
    if (!(WIDE_GL && g_wide && !g_side_by_side && !(g_gl && g_two && !g_single)) || !g_minimap || g_small_blank || !in_adventure()) return 0;
    int side = 0, which = g_gl ? gl_wide_shown(&side) : -1;
    if (which == 0) return g_top_menu && !g_hud_n && !g_hud_win[2] ? 2 : 0;
    return mapmarks_top_page() ? 1 : 0;
}

/* second: the layout of the second window (two windows) */
static void canvas_layout_for(GlPresent* v, int ww, int wh, int second) {
    memset(v, 0, sizeof *v);
    int two = g_gl && g_two && !g_side_by_side && !g_single;
    int wide = WIDE_GL && g_wide && !g_side_by_side && !two;
    int single = g_single && !wide;                               /* one screen (in widescreen the left trigger hides the small one instead) */
    int corner = g_gl && g_corner && !g_side_by_side && !wide && !two && !single;
    int shell = g_gl && g_opt.shell && !g_side_by_side && !corner && !wide && !two && !single;
    int side = 0, which = -1;
    if (g_gl) which = gl_wide_shown(&side);                       /* how the last picture was composed */
    v->topx[0] = v->botx[0] = (float)side; v->topx[1] = v->botx[1] = 256;
    if (wide) {
        /* Modern: the screen with the 3D (the field, or the battle) fills the
         * window, wider than the DS's; the other screen sits small in a corner
         * over it, like a minimap (see-through: O).
         * Basic: in the corner the player picks (K) at the size they pick (I).
         * Dynamic (experimental): top left; when the game shows 2D there
         * (menus, the title) the field slides over to make room: the corner
         * layout, in the same window (g_wide_mix, from wide_update()); in
         * battles the commands sit at half size.
         * Left trigger / U hides the small screen; holding the swap button
         * shows it big in the middle (g_peek). */
        float S = (float)wide_side_target();
        int pop = g_wide_pop && !g_minimap;                       /* the small screen in the second window */
        int dyn = g_wide_dyn && !pop && !g_minimap && !g_small_hide && !g_peek;
        float t = which == 0 || !dyn ? 1.0f : g_wide_mix * g_wide_mix * (3 - 2 * g_wide_mix);   /* eased */
        v->cw = 256 + 2 * S; v->ch = 192;
        float* big = which == 0 ? v->top : v->bot, *small = which == 0 ? v->bot : v->top;
        float* bigx = which == 0 ? v->topx : v->botx;
        float wx0 = S, wtx0 = (float)side, ww = 256;              /* wide: the field (or, without 3D, the DS's picture) */
        if (which >= 0) { wx0 = S - (float)side; wtx0 = 0; ww = 256 + 2 * (float)side; }
        float cx0 = v->cw - 256, ctx0 = (float)side, cww = 256;   /* corner: the DS's picture at the right */
        big[0] = cx0 + (wx0 - cx0) * t; big[2] = cww + (ww - cww) * t;
        bigx[0] = ctx0 + (wtx0 - ctx0) * t; bigx[1] = big[2];
        big[1] = 0; big[3] = 192;
        if (!g_wide_dyn || g_minimap) {
            static const float SW[3] = { 64, 256.0f / 3, 128 }, SH[3] = { 48, 64, 96 };
            int z = g_small_size < 0 || g_small_size > 2 ? 1 : g_small_size, c = g_small_corner & 3;
            if (g_minimap && which == 0) z = 2;                  /* battles: the commands screen when the HUD doesn't take it (spell lists...), large */
            small[2] = SW[z]; small[3] = SH[z];
            float gap = g_minimap && which != 0 ? MINIMAP_GAP : 2;
            small[0] = c == 0 || c == 3 ? gap : v->cw - small[2] - gap;
            small[1] = c < 2 ? gap : 192 - small[3] - gap;
        } else if (which == 0) { small[2] = 128; small[3] = 96; small[0] = small[1] = 2; }   /* battles: top left, over the sky */
        else { small[0] = small[1] = 2 * t; small[2] = 256.0f / 3; small[3] = 64; }
        if (g_small_blank && t > 0.99f) small[2] = small[3] = 0;   /* black (battle intros, fades): leave the field whole */
        if (g_minimap && !in_adventure()) small[2] = small[3] = 0;
        /* battles: nothing to choose on the commands screen (what it shows while
         * the battle ends, the cards between the orders and the turn): not shown */
        if (g_minimap && which == 0 && g_hud && g_winstyle && !g_hud_n && !g_hud_win[2] && g_comp_win_px[1] < 64) small[2] = small[3] = 0;
        v->front = small == v->bot;                               /* the small one in front */
        v->alpha = 1.0f + (g_small_alpha - 1.0f) * t;            /* see-through (O) while it is over the field */
        if (g_minimap && which != 0) {                            /* the minimap: fades out while a menu is under it */
            v->alpha *= g_mini_vis;
            if (g_mini_vis < 0.02f || !g_map_shown) small[2] = small[3] = 0;
        } else if (g_minimap) v->alpha = 1.0f;                   /* battles: solid */
        if (g_small_hide && !pop) small[2] = small[3] = 0;
        if (g_peek && !pop) { small[0] = (v->cw - 256) / 2; small[1] = 0; small[2] = 256; small[3] = 192; v->alpha = 1.0f; }   /* the other screen, whole */
        int page = top_page_mode();
        if (page == 2) {
            /* a menu with its 3D model on the top screen (the monster list):
             * both screens whole, side by side, the top one on the left, until it closes */
            v->cw = 512; v->ch = 192;
            v->top[0] = 0; v->top[1] = 0; v->top[2] = 256; v->top[3] = 192;
            v->bot[0] = 256; v->bot[1] = 0; v->bot[2] = 256; v->bot[3] = 192;
            v->topx[0] = v->botx[0] = (float)side; v->topx[1] = v->botx[1] = 256;
            v->alpha = 1.0f; v->front = 0;
        } else if (page == 1 && g_shop && !second) {
            /* a shop's buy list (the field's screen): one column in the middle,
             * the list (each item's icon before its name, mapmarks_shop()),
             * the item's description (the text of its card on the top screen,
             * in a window of the minimal style as wide as the list's), then
             * the panels under the list (who can equip it, how many held);
             * over the field, which stays whole */
            small[0] = small[1] = small[2] = small[3] = 0;
            float L = (float)g_shop_cut, dh = g_pop_win[2] ? SHOP_TH + 2 * SHOP_PAD : 0, gap = SHOP_GAP;
            float k = 192.0f / (192.0f + (dh > 0 ? dh + 2 * gap : 0)), x = (v->cw - 256 * k) / 2;
            float* q = v->piece[v->npiece++];
            q[4] = 3; q[5] = 0; q[6] = 0; q[7] = 256; q[8] = L;
            q[0] = x; q[1] = 0; q[2] = 256 * k; q[3] = L * k;
            q = v->piece[v->npiece++];
            q[4] = 3; q[5] = 0; q[6] = L; q[7] = 256; q[8] = 192 - L;
            q[0] = x; q[1] = 192 - (192 - L) * k; q[2] = 256 * k; q[3] = (192 - L) * k;
            if (dh > 0) {
                q = v->piece[v->npiece++];                        /* the window */
                q[9] = 2; q[0] = x + 8 * k; q[1] = (L + gap) * k; q[2] = 240 * k; q[3] = dh * k;
                q = v->piece[v->npiece++];                        /* its text */
                q[4] = 0; q[5] = SHOP_TX; q[6] = SHOP_TY; q[7] = SHOP_TW; q[8] = SHOP_TH; q[9] = 1;   /* (the item's icon at its left: mapmarks_shop()) */
                q[0] = x + (SHOP_DESC_X + SHOP_DESC_ICON + 8) * k; q[1] = (L + gap + SHOP_PAD) * k; q[2] = SHOP_TW * k; q[3] = SHOP_TH * k;
            }
            v->icons[0] = x; v->icons[1] = 0; v->icons[2] = 256 * k; v->icons[3] = 192 * k;
        } else if (page == 1) {
            /* a page on the top screen (an item's card in Items, Equipment or a
             * shop, a title): its windows alone, without the page's backdrop,
             * popped up in the middle over the big screen (pop_update()) */
            small[0] = small[1] = small[2] = small[3] = 0;
            if (g_pop_win[2] && v->npiece < GL_MAX_PIECES) {
                float* q = v->piece[v->npiece++], w = (float)g_pop_win[2], h = (float)g_pop_win[3];
                q[4] = 0; q[5] = (float)g_pop_win[0]; q[6] = (float)g_pop_win[1]; q[7] = w; q[8] = h;
                q[2] = w * g_pop_at[2]; q[3] = h * g_pop_at[2]; q[0] = g_pop_at[0]; q[1] = g_pop_at[1];
            }
        }
        if (pop) {                                                /* first window: the field alone; second: the small screen, whole */
            v->alpha = 1.0f;
            if (second) { v->cw = 256; v->ch = 192; big[0] = big[1] = big[2] = big[3] = 0; small[0] = small[1] = 0; small[2] = 256; small[3] = 192; }
            else small[0] = small[1] = small[2] = small[3] = 0;
        }
        if (which == 0 && (g_hud_n > 0 || g_hud_win[2]) && !g_peek && !(g_small_hide && !pop)) {
            /* Battle HUD: the commands screen cut into its panels, over the 3D.
             * The party's panels in a row along the top; the one whose turn it
             * is (the tall panel, with the commands) bigger in the bottom left.
             * The map window keeps the last map (gl_hold() in present_gl). */
            if (second) {
                if (pop) { v->held = 1; v->bot[0] = v->bot[1] = v->bot[2] = v->bot[3] = 0; v->top[0] = v->top[1] = 0; v->top[2] = 256; v->top[3] = 192; v->topx[0] = (float)side; v->topx[1] = 256; v->front = 0; }
            } else {
                small[0] = small[1] = small[2] = small[3] = 0;
                if (g_hud_win[2]) {                                   /* a window alone: big, at the top in the middle */
                    float* q = v->piece[v->npiece++], w = (float)g_hud_win[2], h = (float)g_hud_win[3], kw = fminf(1.0f, fminf(200.0f / w, 110.0f / h));
                    q[4] = 1; q[5] = (float)g_hud_win[0]; q[6] = (float)g_hud_win[1]; q[7] = w; q[8] = h;
                    q[2] = w * kw; q[3] = h * kw; q[0] = (v->cw - q[2]) / 2; q[1] = 6;
                }
                int nrow = 0;
                for (int i = 0; i < g_hud_n; i++) nrow += g_hud_band[i][1] - g_hud_band[i][0] < 50;
                float slot = (v->cw - 4) / (float)(nrow > 4 ? nrow : 4), k = fminf((slot - 2) / 244.0f, 0.5f);
                float x = (v->cw - slot * (float)nrow) / 2;               /* the row centred */
                for (int i = 0; i < g_hud_n && v->npiece < GL_MAX_PIECES; i++) {
                    float h = (float)(g_hud_band[i][1] - g_hud_band[i][0]), *q = v->piece[v->npiece++];
                    q[4] = 1; q[5] = 6; q[6] = (float)g_hud_band[i][0]; q[7] = 244; q[8] = h;
                    if (h < 50) { q[2] = 244 * k; q[3] = h * k; q[0] = x + (slot - q[2]) / 2; q[1] = 2; x += slot; }
                    else {                                                /* at most half the picture's height */
                        float kb = fminf(0.62f, 110.0f / h);
                        q[7] = 249;                                   /* (to the line of a list's window at the screen's edge) */
                        q[2] = q[7] * kb; q[3] = h * kb; q[0] = 2; q[1] = 190 - q[3];
                    }
                }
            }
        }
    } else if (two || single) {
        v->cw = 256; v->ch = 192;                                /* one screen; the other is in the other window, or not shown */
    } else if (corner) {
        /* Modern: the bottom screen (the field) as big as the window allows, the
         * top screen (map, party) a third of its size in the top left corner:
         * the canvas is 16:9 */
        v->cw = 256 + 256.0f / 3; v->ch = 192;
        v->top[0] = 0; v->top[1] = 0; v->bot[0] = 256.0f / 3; v->bot[1] = 0;
    } else if (g_side_by_side && !single) {
        v->cw = 512; v->ch = 192;
        v->top[0] = 0; v->bot[0] = 256; v->top[1] = v->bot[1] = 0;
    } else if (shell && current_skin()) {
        /* the photo: canvas units are DS pixels at the photo's top-screen scale */
        const Skin* k = current_skin();
        float kk = k->top[2] / 256.0f;
        v->cw = (float)k->w / kk; v->ch = (float)k->h / kk;
        v->top[0] = k->top[0] / kk; v->top[1] = k->top[1] / kk;
        v->bot[0] = k->bot[0] / kk; v->bot[1] = k->bot[1] / kk;
        v->skin = 1; v->skin_k = kk;
    } else if (shell) {
        v->cw = 560; v->ch = 630;
        v->top[0] = v->bot[0] = 152; v->top[1] = 78; v->bot[1] = 78 + 192 + 90;
    } else {
        int gap = g_gl && g_opt.gap ? 90 : 0;
        v->cw = 256; v->ch = (float)(384 + gap);
        v->top[0] = v->bot[0] = 0; v->top[1] = 0; v->bot[1] = (float)(192 + gap);
    }
    if (!wide) { v->top[2] = v->bot[2] = 256; v->top[3] = v->bot[3] = 192; }
    if (corner) { v->top[2] = 256.0f / 3; v->top[3] = 64; }
    if (two || single) {                                         /* this window's screen: the bottom one in the first, unless swapped */
        float* off = (second ? !g_swap : g_swap) ? v->bot : v->top;
        off[0] = off[1] = off[2] = off[3] = 0;
    } else if (g_swap && !wide) {                                /* F9: the screens trade places */
        for (int i = 0; i < 4; i++) { float t = v->top[i]; v->top[i] = v->bot[i]; v->bot[i] = t; }
        for (int i = 0; i < 2; i++) { float t = v->topx[i]; v->topx[i] = v->botx[i]; v->botx[i] = t; }
    }
    v->win_w = ww; v->win_h = wh;
    float sc = fminf((float)ww / v->cw, (float)wh / v->ch);
    if (g_opt.actual && g_actual_sc > 0) { if (g_actual_sc < sc) sc = g_actual_sc; }   /* real size (smaller if it doesn't fit) */
    else if (g_opt.integer && sc >= 1.0f) sc = floorf(sc);
    v->sc = sc;
    v->ox = floorf(((float)ww - v->cw * sc) / 2); v->oy = floorf(((float)wh - v->ch * sc) / 2);
    v->shell = shell;
    v->lcd = g_gl && g_opt.lcd;
    v->lcd_color = g_opt.lcd_color; v->grid = g_opt.grid; v->ghost = g_opt.ghost;
    v->backlight = BACKLIGHT[g_opt.backlight];
    const Shell* sh = &SHELLS[g_opt.shell_color < N_DRAWN ? g_opt.shell_color : 1];
    uint32_t cols[4] = { sh->lid, sh->base, sh->btn, sh->hinge };
    float* dst[4] = { v->lid, v->base, v->btn, v->hinge };
    for (int i = 0; i < 4; i++) for (int k = 0; k < 3; k++) dst[i][k] = (float)((cols[i] >> (16 - 8 * k)) & 255) / 255.0f;
}
static void canvas_layout(GlPresent* v, int ww, int wh) { canvas_layout_for(v, ww, wh, 0); }

/* ---------------------------------------------------------------- settings.ini
 * Remembers the player's choices between launches (next to the game). */
static const char* SETTINGS = "settings.ini";

/* Wireless: the community servers that replace Nintendo Wi-Fi Connection.
 * The DS finds a server through its DNS setting (firmware_set_ap), exactly
 * as players set it up by hand on a real DS. */
static const struct { const char* key; const char* dns; } WFC[] = {
    { "builtin", "10.64.0.2" },          /* wfc.c answers; the DNS is the virtual network's */
    { "altwfc", "172.104.88.237" },
    { "wiimmfi", "164.132.44.106" },
};
static char g_wfc_server[64] = "builtin";       /* a key above or a DNS address */
static char g_player_name[32] = "";              /* sessions: what friends see */
static int g_session_port = SESSION_PORT;
static int g_update_auto = 1;                    /* look for a new stable version at start */
static char g_update_skip[32];                   /* ...but not this one (the player said so) */
static int g_net_want = 1;                     /* the menu's "connect" check: on for a new install (settings.ini keeps each player's choice) */
static const char* wfc_dns(void) {
    for (unsigned i = 0; i < sizeof WFC / sizeof WFC[0]; i++) if (!strcmp(g_wfc_server, WFC[i].key)) return WFC[i].dns;
    return g_wfc_server;
}
static int wfc_custom(void) {
    for (unsigned i = 0; i < sizeof WFC / sizeof WFC[0]; i++) if (!strcmp(g_wfc_server, WFC[i].key)) return 0;
    return 1;
}
static void wireless_apply(int boot) {
    g_wireless.online = g_net_want;
    g_wireless.builtin = !strcmp(g_wfc_server, "builtin");
    snprintf(g_wireless.dns, sizeof g_wireless.dns, "%s", wfc_dns());
    if (!boot) {
        firmware_set_ap(g_wireless.online, g_wireless.dns); net_enable(g_wireless.online);
        wfc_enable(g_wireless.online && g_wireless.builtin, NULL);
    }
}
/* every console has its own MAC address, and the servers tell players apart
 * by it: one is made up the first time (Nintendo's prefix, random rest) */
static void wireless_mac_new(void) {
    uint32_t r = (uint32_t)time(NULL) ^ (uint32_t)SDL_GetPerformanceCounter() ^ (uint32_t)(uintptr_t)&r;
    for (int i = 0; i < 8; i++) r = r * 1103515245u + 12345u;
    g_wireless.mac[0] = 0x00; g_wireless.mac[1] = 0x09; g_wireless.mac[2] = 0xBF;
    for (int i = 3; i < 6; i++) { r = r * 1103515245u + 12345u; g_wireless.mac[i] = (uint8_t)(r >> 16); }
    g_wireless.mac_set = 1;
}
/* the console around the screens until the player picks another: the photo of a real DS Lite */
static int g_color_chosen;
static void default_console_color(void) {
    for (int i = 0; i < g_nskins; i++) if (!strcmp(g_skins[i].file + 6, "dslite_black.png")) g_opt.shell_color = N_DRAWN + i;
}
/* settings.ini's names for g_fw_language (1-5), and the menu's */
static const char* const LANGUAGE_KEY[6] = { "", "english", "french", "german", "italian", "spanish" };
static const char* const LANGUAGE_NAME[6] = { "", "English", "Fran\xc3\xa7" "ais", "Deutsch", "Italiano", "Espa\xc3\xb1ol" };
static int g_ui_language;        /* the menus' and messages' language (uitext.h): 0 the game's, else 1-5 like g_fw_language */
static int g_ui_contrast, g_ui_text_size;   /* View > Readability (the in-game options menu): stronger contrast; text 0 normal, 1 large, 2 larger */
static int ui_language_wanted(void) { return g_ui_language ? g_ui_language : rom_is_eur() ? g_fw_language : 1; }
/* settings_version 2: a new install looks like the project's own setup (Enhanced,
 * widescreen with the map in the top right corner, the battle HUD, the map
 * markers...). An older settings.ini keeps the player's own things (volume,
 * language, wireless, controls, gold farm) but takes these looks once.
 * settings_version 3: the modern windows, once (window_style 2). */
#define SETTINGS_VERSION 3
static const char* const LOOK_KEYS[] = {
    "mode", "render_scale", "resolution_3d", "window_scale", "layout", "single_screen", "swap_screens",
    "small_screen_corner", "small_screen_size", "small_screen_hidden", "small_screen_opacity", "small_screen_opacity_chosen",
    "widescreen_popout", "widescreen_battle_hud", "map_markers", "map_markers_opened",
    "smooth_textures", "smooth_2d", "upscale_textures", "outlines", "window_style", "hd_textures", "hd_text", "hd_text_font", "hd_movies",
    "dslite_filter", "lcd_colors", "pixel_grid", "motion_blur", "backlight", "screen_gap", "console", "console_color",
    "console_color_chosen", "integer_scaling", "actual_size", "downloadable_content" };
static int look_key(const char* key) {
    for (unsigned i = 0; i < sizeof LOOK_KEYS / sizeof LOOK_KEYS[0]; i++) if (!strcmp(key, LOOK_KEYS[i])) return 1;
    return 0;
}
static void settings_load(void) {
    FILE* f = fopen(SETTINGS, "r");
    if (!f) return;
    char line[200], color[64] = "";
    int version = 0;
    while (fgets(line, sizeof line, f)) sscanf(line, " settings_version = %d", &version);
    rewind(f);
    int seen_rs = 0;                                              /* render_scale (v29) wins over the older resolution_3d */
    while (fgets(line, sizeof line, f)) {
        char key[64]; char val[64];
        if (line[0] == '#' || sscanf(line, " %63[^= ] = %63s", key, val) != 2) continue;
        if (version < 2 && look_key(key)) continue;
        if (!strcmp(key, "player_name")) {                       /* the whole rest of the line (may have spaces) */
            const char* v = strchr(line, '=') + 1; while (*v == ' ') v++;
            snprintf(g_player_name, sizeof g_player_name, "%s", v);
            g_player_name[strcspn(g_player_name, "\r\n")] = 0;
            continue;
        }
        if (!strcmp(key, "all_options")) { g_all_options = atoi(val) != 0; continue; }
        if (!strcmp(key, "update_check")) { g_update_auto = atoi(val) != 0; continue; }
        if (!strcmp(key, "update_skip")) { snprintf(g_update_skip, sizeof g_update_skip, "%s", val); continue; }
        if (!strcmp(key, "session_port")) { int v = atoi(val); if (v > 0 && v < 65536) g_session_port = v; continue; }
        if (!strcmp(key, "session_consoles")) { if (!strcmp(val, "processes")) { static char e[] = "DQIX_PROCESSES=1"; putenv(e); } continue; }
        if (!strcmp(key, "mode")) g_classic = strcmp(val, "enhanced") != 0;
        else if (!strcmp(key, "resolution_3d")) { int v = atoi(val); if (v >= 1 && v <= 4) { g_scale3d = v; if (!seen_rs) g_res_auto = v == 4; } }   /* before v29: 4 was the most (now Auto) */
        else if (!strcmp(key, "render_scale")) { seen_rs = 1; int v = atoi(val); g_res_auto = !strcmp(val, "auto") || v < 1; if (!g_res_auto) g_scale3d = v > GL_MAX_SCALE ? GL_MAX_SCALE : v; }
        else if (!strcmp(key, "window_scale")) { int v = atoi(val); if (v >= 1 && v <= 8) g_scale = v; }
        /* (layout, single_screen, swap_screens, the small screen's corner, opacity and window: no longer
         * chosen, the one layout is widescreen with the map in the corner, stacked in Classic) */
        else if (!strcmp(key, "small_screen_size")) { int v = atoi(val); if (v >= 0 && v <= 2) g_small_size = v; }
        else if (!strcmp(key, "minimap")) g_map_shown = atoi(val) != 0;
        else if (!strcmp(key, "volume")) { int v = atoi(val); if (v >= 0 && v <= 100) g_volume = v; }
        else if (!strcmp(key, "mute")) g_mute = atoi(val) != 0;
        else if (!strcmp(key, "widescreen_battle_hud")) g_hud = atoi(val) != 0;
        else if (!strcmp(key, "map_markers")) g_mapmarks = atoi(val) != 0;
        else if (!strcmp(key, "map_markers_opened")) g_mapmarks_opened = atoi(val) != 0;
        else if (!strcmp(key, "smooth_textures")) g_smooth = atoi(val) != 0;
        else if (!strcmp(key, "smooth_2d")) g_smooth2d = atoi(val) != 0;
        else if (!strcmp(key, "outlines")) { int v = atoi(val); if (v >= 0 && v <= 2) g_outline = v; }
        else if (!strcmp(key, "window_style")) { int v = atoi(val); if (v >= 0 && v <= 2 && version >= 3) g_winstyle = v; }
        else if (!strcmp(key, "upscale_textures")) g_texup = atoi(val) != 0;
        else if (!strcmp(key, "soft_shadows")) g_softshadow = atoi(val) != 0;
        else if (!strcmp(key, "outline_lines")) g_ink = atoi(val) != 0;
        else if (!strcmp(key, "hd_textures")) g_texpack = atoi(val) != 0;
        else if (!strcmp(key, "dump_textures")) g_texdump = atoi(val) != 0;
        else if (!strcmp(key, "hd_text")) g_hdtext = atoi(val) != 0;
        else if (!strcmp(key, "hd_movies")) g_hdmovie = atoi(val) != 0;
        else if (!strcmp(key, "hd_text_font")) snprintf(g_hdfont, sizeof g_hdfont, "%s", val);
        else if (!strcmp(key, "farm_gold_target")) { long v = atol(val); if (v > 0 && v <= 9999999) g_farm_target = (int)v; }
        else if (!strcmp(key, "farm_fast_forward")) g_farm_fast = atoi(val) != 0;
        else if (!strcmp(key, "skip_intro")) { int v = atoi(val); if (v >= 0 && v <= 2) g_skip_intro = v; }
        else if (!strcmp(key, "battle_speed")) { int v = atoi(val); if (v >= 1 && v <= 4) g_battle_speed = v; }
        else if (!strcmp(key, "battle_speed_turn_only")) g_battle_turn_only = atoi(val) != 0;
        else if (!strcmp(key, "battle_victory_auto")) g_battle_auto_victory = atoi(val) != 0;
        else if (!strcmp(key, "save_backups")) g_save_backups = atoi(val) != 0;
        else if (!strcmp(key, "autosave")) g_autosave = atoi(val) != 0;
        else if (!strcmp(key, "cheats")) cheats_from_list(val);
        else if (!strcmp(key, "map_monsters")) g_mapmarks_monsters = atoi(val) != 0;
        else if (!strcmp(key, "interior_maps")) g_interior_maps = atoi(val) != 0;
        else if (!strcmp(key, "map_legend")) g_map_legend = atoi(val) != 0;
        else if (!strcmp(key, "button_hints")) g_hints = atoi(val) != 0;
        else if (!strcmp(key, "walk_any_direction")) g_analog_walk = atoi(val) != 0;   /* (the older analog_walk, off by default then, is ignored) */
        else if (!strcmp(key, "fullscreen")) g_fullscreen = atoi(val) != 0;
        else if (!strcmp(key, "dslite_filter")) g_opt.lcd = atoi(val) != 0;
        else if (!strcmp(key, "lcd_colors")) g_opt.lcd_color = atoi(val) != 0;
        else if (!strcmp(key, "pixel_grid")) g_opt.grid = atoi(val) != 0;
        else if (!strcmp(key, "motion_blur")) g_opt.ghost = atoi(val) != 0;
        else if (!strcmp(key, "backlight")) { for (int i = 1; i <= 4; i++) if (!strcmp(val, BACKLIGHT_NAME[i])) g_opt.backlight = i; }
        else if (!strcmp(key, "screen_gap")) g_opt.gap = atoi(val) != 0;
        else if (!strcmp(key, "console")) g_opt.shell = atoi(val) != 0;
        else if (!strcmp(key, "console_color")) snprintf(color, sizeof color, "%s", val);
        else if (!strcmp(key, "console_color_chosen")) g_color_chosen = atoi(val) != 0;
        else if (!strcmp(key, "integer_scaling")) g_opt.integer = atoi(val) != 0;
        else if (!strcmp(key, "actual_size")) g_opt.actual = atoi(val) != 0;
        else if (!strcmp(key, "monitor_width_mm")) g_opt.monitor_width_mm = (float)atof(val);
        else if (!strcmp(key, "downloadable_content")) g_opt.dlc = atoi(val) != 0;
        else if (!strcmp(key, "language")) { for (int i = 1; i <= 5; i++) if (!strcmp(val, LANGUAGE_KEY[i])) g_fw_language = i; }
        else if (!strcmp(key, "menu_contrast")) g_ui_contrast = atoi(val) != 0;
        else if (!strcmp(key, "menu_text_size")) { int v = atoi(val); if (v >= 0 && v <= 2) g_ui_text_size = v; }
        else if (!strcmp(key, "menu_language")) { g_ui_language = 0; for (int i = 1; i <= 5; i++) if (!strcmp(val, LANGUAGE_KEY[i])) g_ui_language = i; }
        else if (!strcmp(key, "wireless")) g_net_want = atoi(val) != 0;
        else if (!strcmp(key, "wfc_server")) snprintf(g_wfc_server, sizeof g_wfc_server, "%s", val);
        else if (!strcmp(key, "wireless_mac")) {
            unsigned b[6];
            if (sscanf(val, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
                for (int i = 0; i < 6; i++) g_wireless.mac[i] = (uint8_t)b[i];
                g_wireless.mac_set = 1;
            }
        }
    }
    fclose(f);
    /* the fonts the menu showed before Source Sans 3 became the only one there (the others: all_options) */
    if (!g_all_options && (!strcmp(g_hdfont, "Marcellus.ttf") || !strcmp(g_hdfont, "original")))
        snprintf(g_hdfont, sizeof g_hdfont, "%s", "SourceSans3.ttf");
    /* the colour counts once the player picked one (before nightly5 it was saved either way) */
    if (g_color_chosen) {
        for (int i = 0; i < N_DRAWN; i++) if (!strcmp(color, SHELLS[i].key)) g_opt.shell_color = i;
        for (int i = 0; i < g_nskins; i++) if (!strcmp(color, g_skins[i].file + 6)) g_opt.shell_color = N_DRAWN + i;   /* a photo, by file name */
    }
}
static void settings_save(void) {
    if (g_headless || getenv("DQIX_NO_SETTINGS")) return;
    FILE* f = fopen(SETTINGS, "w");
    if (!f) return;
    fprintf(f, "# Dragon Quest IX (PC) settings. The game saves this when you change something.\n");
    fprintf(f, "settings_version = %d\n", SETTINGS_VERSION);
    fprintf(f, "# every option in the menus and on the keys (other layouts, DS Lite screen, fonts...): 0 or 1\n");
    fprintf(f, "all_options = %d\n", g_all_options);
    fprintf(f, "# mode: classic (exactly like the DS) or enhanced (sharper 3D on the graphics card). F5 switches.\n");
    fprintf(f, "mode = %s\n", g_classic ? "classic" : "enhanced");
    fprintf(f, "# 3D resolution in enhanced mode: 1 to 12, or auto (as sharp as the window shows it; F4)\n");
    if (g_res_auto) fprintf(f, "render_scale = auto\n"); else fprintf(f, "render_scale = %d\n", g_scale3d);
    fprintf(f, "resolution_3d = %d\n", g_res_auto || g_scale3d > 4 ? 4 : g_scale3d);   /* for versions before v29 */
    fprintf(f, "# enhanced: smooth textures (filtered) instead of the DS's blocky texels: 0 or 1\n");
    fprintf(f, "smooth_textures = %d\n", g_smooth);
    fprintf(f, "# enhanced: the 3D textures upscaled 4x with xBR (smooth slopes and curves): 0 or 1\n");
    fprintf(f, "upscale_textures = %d\n", g_texup);
    fprintf(f, "# enhanced: the 2D screens (menus, maps, sprites) smoothed with xBR: 0 or 1\n");
    fprintf(f, "smooth_2d = %d\n", g_smooth2d);
    fprintf(f, "# enhanced: soft shadows in corners and under things (furniture, shelves, walls meeting the floor): 0 or 1\n");
    fprintf(f, "soft_shadows = %d\n", g_softshadow);
    fprintf(f, "# enhanced: thin dark lines round things, so they stand apart from what is behind: 0 or 1\n");
    fprintf(f, "outline_lines = %d\n", g_ink);
    fprintf(f, "# enhanced: characters' outlines: 0 like the DS, 1 thin, 2 none\n");
    fprintf(f, "outlines = %d\n", g_outline);
    fprintf(f, "# enhanced: menu and dialogue windows: 0 like the DS, 1 minimal (no frame, more see-through), 2 modern (square, dark see-through, a thin grey line)\n");
    fprintf(f, "window_style = %d\n", g_winstyle);
    fprintf(f, "# enhanced: HD textures from the textures folder (textures/<name>.png) instead of the game's: 0 or 1\n");
    fprintf(f, "hd_textures = %d\n", g_texpack);
    fprintf(f, "# enhanced: write every texture the game shows to textures/dump/, to make a pack from: 0 or 1\n");
    fprintf(f, "dump_textures = %d\n", g_texdump);
    fprintf(f, "# enhanced: the game's text at full resolution: 0 or 1\n");
    fprintf(f, "hd_text = %d\n", g_hdtext);
    fprintf(f, "# its font: original (the game's letters, smoothed) or a .ttf file in the fonts folder or Windows' fonts\n");
    fprintf(f, "hd_text_font = %s\n", g_hdfont);
    fprintf(f, "# enhanced: the movies (opening, dragon...) from the movies folder, upscaled: 0 or 1\n");
    fprintf(f, "hd_movies = %d\n", g_hdmovie);
    fprintf(f, "# the map in the top right corner (gone during battles): 0 or 1; its size 0 small, 1 medium, 2 large\n");
    fprintf(f, "minimap = %d\n", g_map_shown);
    fprintf(f, "small_screen_size = %d\n", g_small_size);
    fprintf(f, "# widescreen battles: the party's panels along the top and the commands in the bottom left, over the 3D: 0 or 1\n");
    fprintf(f, "widescreen_battle_hud = %d\n", g_hud);
    fprintf(f, "# the map: the chests, pots and barrels of the place marked (the mouse over one: what is in it): 0 or 1\n");
    fprintf(f, "map_markers = %d\n", g_mapmarks);
    fprintf(f, "# ...also the ones already emptied (greyed): 0 or 1\n");
    fprintf(f, "map_markers_opened = %d\n", g_mapmarks_opened);
    fprintf(f, "# window size: 1 to 8 times the DS screens (F3)\n");
    fprintf(f, "window_scale = %d\n", g_scale);
    fprintf(f, "# volume 0 to 100 (- and =), and mute\n");
    fprintf(f, "volume = %d\n", g_volume);
    fprintf(f, "mute = %d\n", g_mute);
    fprintf(f, "# Modern: the left stick walks in any direction, like the stylus (0 or 1)\n");
    fprintf(f, "walk_any_direction = %d\n", g_analog_walk);
    fprintf(f, "# Modern > Farm gold by itself: stop once the party has this much gold, and fast forward while farming (0 or 1)\n");
    fprintf(f, "farm_gold_target = %d\n", g_farm_target);
    fprintf(f, "farm_fast_forward = %d\n", g_farm_fast);
    fprintf(f, "# Modern > Start: 0 the logos and the opening movie as on a DS, 1 skip the logos, 2 skip the opening movie too\n");
    fprintf(f, "skip_intro = %d\n", g_skip_intro);
    fprintf(f, "# Modern > Battle speed: battles go 1, 2, 3 or 4 times as fast (not while playing with friends);\n");
    fprintf(f, "# turn_only 1: only while the turn plays out (the commands are chosen at the usual pace);\n");
    fprintf(f, "# victory_auto 1: after a win, the messages turn their page by themselves (not after a level up): 0 or 1\n");
    fprintf(f, "battle_speed = %d\n", g_battle_speed);
    fprintf(f, "battle_speed_turn_only = %d\n", g_battle_turn_only);
    fprintf(f, "battle_victory_auto = %d\n", g_battle_auto_victory);
    fprintf(f, "# Modern > Save by itself every 10 minutes (on the field, while free to walk; not while playing with friends): 0 or 1\n");
    fprintf(f, "autosave = %d\n", g_autosave);
    fprintf(f, "# Game > Keep a copy of every save: the save as it was before each new one, in save-backups (the last 20): 0 or 1\n");
    fprintf(f, "save_backups = %d\n", g_save_backups);
    {   char on[400]; cheats_to_list(on, sizeof on);
        fprintf(f, "# Cheats: the ones turned on, by name (empty: none)\n");
        fprintf(f, "cheats = %s\n", on); }
    fprintf(f, "# Modern > Map: the monsters walking nearby, as red dots: 0 or 1\n");
    fprintf(f, "map_monsters = %d\n", g_mapmarks_monsters);
    fprintf(f, "# Modern > Map: inside buildings, a plan of the room drawn from its walls (the game shows the town): 0 or 1\n");
    fprintf(f, "interior_maps = %d\n", g_interior_maps);
    fprintf(f, "# the map: a legend of its marks, under the place's name: 0 or 1\n");
    fprintf(f, "map_legend = %d\n", g_map_legend);
    fprintf(f, "# the game's DS buttons (the field's \"X Menu\") as on your controller or keyboard, whichever you used last: 0 or 1\n");
    fprintf(f, "button_hints = %d\n", g_hints);
    fprintf(f, "# 0 or 1 (F11)\n");
    fprintf(f, "fullscreen = %d\n", g_fullscreen);
    fprintf(f, "# DS Lite screen filter (F6) and its parts: 0 or 1; backlight: low, medium, high, max\n");
    fprintf(f, "dslite_filter = %d\n", g_opt.lcd);
    fprintf(f, "lcd_colors = %d\n", g_opt.lcd_color);
    fprintf(f, "pixel_grid = %d\n", g_opt.grid);
    fprintf(f, "motion_blur = %d\n", g_opt.ghost);
    fprintf(f, "backlight = %s\n", BACKLIGHT_NAME[g_opt.backlight]);
    fprintf(f, "# the real gap between the screens; the DS Lite drawn around them (F7) and its colour:\n");
    fprintf(f, "# white, black, ice_blue, coral_pink, navy, crimson, or a photo in the skins folder by file name\n");
    fprintf(f, "screen_gap = %d\n", g_opt.gap);
    fprintf(f, "console = %d\n", g_opt.shell);
    fprintf(f, "console_color_chosen = %d\n", g_color_chosen);
    fprintf(f, "console_color = %s\n", g_opt.shell_color < N_DRAWN ? SHELLS[g_opt.shell_color].key : g_skins[g_opt.shell_color - N_DRAWN].file + 6);
    fprintf(f, "# whole-number scaling only (every DS pixel the same size): 0 or 1\n");
    fprintf(f, "integer_scaling = %d\n", g_opt.integer);
    fprintf(f, "# screens as big as a real DS Lite's (F8): 0 or 1. The size of your monitor is read from the\n");
    fprintf(f, "# monitor itself; if it comes out wrong, put its visible width in millimetres here (0 = automatic)\n");
    fprintf(f, "actual_size = %d\n", g_opt.actual);
    fprintf(f, "monitor_width_mm = %g\n", g_opt.monitor_width_mm);
    fprintf(f, "# switch on every downloadable quest and special guest at start, without connecting: 0 or 1\n");
    fprintf(f, "downloadable_content = %d\n", g_opt.dlc);
    fprintf(f, "# the game's language (European version; the USA version is in English): english, french, german, italian or spanish\n");
    fprintf(f, "language = %s\n", LANGUAGE_KEY[g_fw_language]);
    fprintf(f, "# the menu bar's, windows' and messages' language: game (the game's own: English with the USA version),\n");
    fprintf(f, "# english, french, german, italian or spanish (their texts: lang folder)\n");
    fprintf(f, "menu_language = %s\n", g_ui_language ? LANGUAGE_KEY[g_ui_language] : "game");
    fprintf(f, "# View > Readability, the options menu in the game (Esc): stronger contrast 0 or 1; text size 0 normal, 1 large, 2 larger\n");
    fprintf(f, "menu_contrast = %d\n", g_ui_contrast);
    fprintf(f, "menu_text_size = %d\n", g_ui_text_size);
    fprintf(f, "# Wireless menu: connect to a community Nintendo Wi-Fi Connection server: 0 or 1\n");
    fprintf(f, "wireless = %d\n", g_net_want);
    fprintf(f, "# which server: builtin (offline, the files in the wfc folder), altwfc, wiimmfi, or another's DNS address\n");
    fprintf(f, "wfc_server = %s\n", g_wfc_server);
    fprintf(f, "# playing with friends far away: your name, and the port friends connect to when you host\n");
    fprintf(f, "player_name = %s\n", g_player_name);
    fprintf(f, "# updates: look for a new stable version when the game starts (Help menu); a version not to offer again\n");
    fprintf(f, "update_check = %d\n", g_update_auto);
    fprintf(f, "update_skip = %s\n", g_update_skip[0] ? g_update_skip : "-");
    fprintf(f, "session_port = %d\n", g_session_port);
    fprintf(f, "# how your friends' DSes run on this PC: thread (one thread, the usual) or processes (if thread misbehaves)\n");
    fprintf(f, "session_consoles = %s\n", getenv("DQIX_PROCESSES") ? "processes" : "thread");
    fprintf(f, "# this copy's own MAC address (made up the first time; the servers know you by it)\n");
    if (g_wireless.mac_set)
        fprintf(f, "wireless_mac = %02x:%02x:%02x:%02x:%02x:%02x\n", g_wireless.mac[0], g_wireless.mac[1],
                g_wireless.mac[2], g_wireless.mac[3], g_wireless.mac[4], g_wireless.mac[5]);
    fclose(f);
}

/* Classic or Enhanced: which 3D renderer runs. Without OpenGL 3.3 only Classic exists. */
static void apply_mode(void) {
    if (!g_gl) g_classic = 1;
    if (g_classic) {
        g_renderer3d = NULL; g_comp_gl = 0; g_report_capture = NULL;
    } else {
        g_renderer3d = gl_renderer3d(); g_comp_gl = 1; g_report_capture = gl_read_screens;
    }
    g_hdtext_on = g_hdtext && !g_classic;
    g_hdmovie_on = g_hdmovie && !g_classic;
}

/* Audio drives the pacing: the game runs ~100 ms ahead of what is being
 * heard, so a slow frame only eats into that cushion instead of causing a
 * dropout. If the buffer still runs dry, output pauses until it refills
 * (a clean gap rather than stutter). */
#define AUDIO_TARGET (32768u / 10)        /* stereo frames queued ahead (~100 ms) */
static volatile int g_audio_priming = 1;

static void audio_cb(void* u, Uint8* stream, int len) {
    (void)u;
    int16_t* out = (int16_t*)stream;
    uint32_t frames = (uint32_t)len / 4;
    if (g_audio_priming) {
        if (sound_buffered() < AUDIO_TARGET / 2) { memset(stream, 0, (size_t)len); return; }
        g_audio_priming = 0;
    }
    uint32_t got = sound_read(out, frames);
    if (got < frames) {                                   /* ran dry: fade the tail, then wait to refill */
        int16_t l = got ? out[(got - 1) * 2] : 0, r = got ? out[(got - 1) * 2 + 1] : 0;
        for (uint32_t i = got; i < frames; i++) {
            int k = (int)(frames - i);
            out[i * 2] = (int16_t)(l * k / (int)(frames - got + 1)); out[i * 2 + 1] = (int16_t)(r * k / (int)(frames - got + 1));
        }
        g_audio_priming = 1;
    }
    int v = g_mute ? 0 : g_volume;                        /* Game > Volume: by ear (loudness follows the square) */
    if (v < 100) {
        int32_t gain = v * v * 65536 / 10000;               /* 16.16 */
        for (uint32_t i = 0; i < frames * 2; i++) out[i] = (int16_t)(((int32_t)out[i] * gain) >> 16);
    }
}

/* Window size = canvas x scale, made smaller if it would not fit on the screen. */
static void menu_sync(void);
static void menu_build(void);
/* keep: the layout changed (swap, single screen, another layout): keep the
 * size the player gave the window - the same if the picture keeps its shape,
 * else the new shape at the same zoom. Otherwise (window size, actual size,
 * the start) the chosen size. */
static float g_last_cw, g_last_ch;                              /* the canvas the window was last sized for */
static void layout_window_keep(int keep) {
    GlPresent v; canvas_layout(&v, 1, 1);
    int w = (int)v.cw, h = (int)v.ch;
    if (!g_gl) SDL_RenderSetLogicalSize(g_ren, w, h);
    float lw = g_last_cw, lh = g_last_ch;
    g_last_cw = v.cw; g_last_ch = v.ch;
    if (g_fullscreen) return;
    float s = g_opt.actual && g_actual_sc > 0 ? g_actual_sc : (float)g_scale;
    if (keep && lw > 0 && lh > 0) {
        if (fabsf(lw - v.cw) < 0.5f && fabsf(lh - v.ch) < 0.5f) return;   /* same shape: nothing to do */
        int ww, wh; SDL_GetWindowSize(g_win, &ww, &wh);
        float cur = fminf((float)ww / lw, (float)wh / lh);       /* the zoom the window has now */
        if (cur > 0.25f) s = cur;
    }
    SDL_Rect r;
    if (SDL_GetDisplayUsableBounds(SDL_GetWindowDisplayIndex(g_win), &r) == 0) {
        float fit = fminf((float)(r.w - 16) / (float)w, (float)(r.h - 64) / (float)h);   /* room for the title and menu bars */
        if (s > fit) s = fit;
    }
    SDL_SetWindowSize(g_win, (int)ceilf((float)w * s), (int)ceilf((float)h * s));
}
static void layout_window(void) { layout_window_keep(0); }

/* OpenGL: a 3.3 core context; on any failure fall back to SDL_Renderer + software 3D. */
static int try_gl(void) {
    if (getenv("DQIX_SOFTWARE")) return 0;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    g_win = SDL_CreateWindow("Dragon Quest IX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 256 * g_scale, 384 * g_scale,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!g_win) return 0;
    g_glctx = SDL_GL_CreateContext(g_win);
    if (!g_glctx) { fprintf(stderr, "OpenGL 3.3 not available (%s): using the software renderer\n", SDL_GetError()); SDL_DestroyWindow(g_win); g_win = NULL; return 0; }
    SDL_GL_SetSwapInterval(0);                                   /* pacing follows the audio clock */
    const char* e = getenv("DQIX_SCALE3D");
    if (e) { g_scale3d = atoi(e); g_res_auto = 0; }
    if (getenv("DQIX_SMOOTH")) g_smooth = atoi(getenv("DQIX_SMOOTH")) != 0;   /* tests */
    gl_set_smooth(g_smooth);
    if (getenv("DQIX_SMOOTH2D")) g_smooth2d = atoi(getenv("DQIX_SMOOTH2D")) != 0;   /* tests */
    if (getenv("DQIX_TEXUP")) g_texup = atoi(getenv("DQIX_TEXUP")) != 0;
    if (getenv("DQIX_OUTLINES")) g_outline = atoi(getenv("DQIX_OUTLINES"));   /* tests */
    if (getenv("DQIX_WINDOW_STYLE")) { int v = atoi(getenv("DQIX_WINDOW_STYLE")); g_winstyle = v >= 0 && v <= 2 ? v : 0; }
    if (getenv("DQIX_SOFT_SHADOWS")) g_softshadow = atoi(getenv("DQIX_SOFT_SHADOWS")) != 0;   /* tests */
    if (getenv("DQIX_OUTLINE_LINES")) g_ink = atoi(getenv("DQIX_OUTLINE_LINES")) != 0;
    gl_set_smooth2d(g_smooth2d); gl_set_tex_upscale(g_texup); gl_set_outline(g_outline); gl_set_soft_shadows(g_softshadow); gl_set_ink_lines(g_ink);
    if (getenv("DQIX_HD_TEXTURES")) g_texpack = atoi(getenv("DQIX_HD_TEXTURES")) != 0;   /* tests */
    if (getenv("DQIX_DUMP_TEXTURES")) g_texdump = atoi(getenv("DQIX_DUMP_TEXTURES")) != 0;
    gl_set_texture_pack(g_texpack); gl_set_texture_dump(g_texdump);
    if (getenv("DQIX_HD_TEXT")) g_hdtext = atoi(getenv("DQIX_HD_TEXT")) != 0;
    if (getenv("DQIX_HD_MOVIES")) g_hdmovie = atoi(getenv("DQIX_HD_MOVIES")) != 0;   /* tests */
    if (getenv("DQIX_HD_FONT")) snprintf(g_hdfont, sizeof g_hdfont, "%s", getenv("DQIX_HD_FONT"));
    hdtext_set_font(g_hdfont);
    if (!gl_render_init(g_res_auto ? (getenv("DQIX_AUTO_FROM") ? 2 : 4) : g_scale3d)) {
        fprintf(stderr, "OpenGL renderer failed to start: using the software renderer\n");
        SDL_GL_DeleteContext(g_glctx); SDL_DestroyWindow(g_win); g_win = NULL; return 0;
    }
    g_gl = 1;
    g_renderer3d_verify = getenv("DQIX_GL_VERIFY") != NULL;
    return 1;
}

/* The window's icon: the game's own, the 32x32 picture the DS menu shows for
 * the cartridge (in the ROM's banner). On Windows, dqix.exe normally carries
 * it already (made from the ROM by the build), and Windows takes it from there
 * at every size; this is for other systems and builds without it. */
static void window_icon_for(SDL_Window* win) {
#ifdef _WIN32
    if (FindResourceA(NULL, MAKEINTRESOURCEA(1), (LPCSTR)RT_GROUP_ICON)) return;
#endif
    extern size_t g_rom_size;
    if (!g_rom || g_rom_size < 0x200 || !win) return;
    uint32_t off; memcpy(&off, g_rom + 0x68, 4);
    if (off == 0 || off + 0x240 > g_rom_size) return;
    const uint8_t* bits = g_rom + off + 0x20;                  /* 4x4 tiles of 8x8, 4 bits a pixel */
    uint32_t pal[16];
    for (int i = 0; i < 16; i++) {                               /* BGR555; colour 0 is see-through */
        uint16_t c; memcpy(&c, g_rom + off + 0x220 + i * 2, 2);
        uint32_t r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
        pal[i] = (i ? 0xFF000000u : 0) | ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2);
    }
    enum { N = 64 };                                             /* doubled, sharp pixels */
    static uint32_t px[N * N];
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            int sx = x / 2, sy = y / 2, t = (sy / 8) * 4 + sx / 8;
            uint8_t v = bits[t * 32 + (sy & 7) * 4 + (sx & 7) / 2];
            px[y * N + x] = pal[(sx & 1) ? v >> 4 : v & 15];
        }
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(px, N, N, 32, N * 4, SDL_PIXELFORMAT_ARGB8888);
    if (s) { SDL_SetWindowIcon(win, s); SDL_FreeSurface(s); }
}
static void window_icon(void) { window_icon_for(g_win); }

/* ---------------------------------------------------------------- two windows
 * One screen in each, for two monitors: the first window (with the menu bar)
 * shows the bottom screen, the second the top one; F9 swaps them. The second
 * window shares the first one's OpenGL context and opens on another monitor
 * if there is one (then where it was left). */
static void second_window_title(void) {
    if (g_win2) SDL_SetWindowTitle(g_win2, g_wide ? "Dragon Quest IX - map and commands" : g_swap ? "Dragon Quest IX - bottom screen" : "Dragon Quest IX - top screen");
}
static void second_window_keep_rect(void) {                     /* remember where it is (as a window) */
    if (!g_win2 || g_win2_full) return;
    SDL_GetWindowPosition(g_win2, &g_win2_rect[0], &g_win2_rect[1]);
    SDL_GetWindowSize(g_win2, &g_win2_rect[2], &g_win2_rect[3]);
}
static void second_window_sync(void) {
    int want = g_gl && ((g_two && !g_single) || (g_wide && g_wide_pop && !g_minimap && !g_classic)) && !g_side_by_side && g_win && !g_headless;
    if (!want && g_win2) {
        second_window_keep_rect();
        SDL_GL_MakeCurrent(g_win, g_glctx);
        SDL_DestroyWindow(g_win2); g_win2 = NULL;
        return;
    }
    if (!want || g_win2) { second_window_title(); return; }
    int x = SDL_WINDOWPOS_CENTERED, y = SDL_WINDOWPOS_CENTERED, w = 256 * g_scale, h = 192 * g_scale, placed = 0;
    if (g_win2_rect[2] > 0) {                                    /* where it was, if that is still on a monitor */
        for (int d = 0; d < SDL_GetNumVideoDisplays(); d++) {
            SDL_Rect r;
            if (SDL_GetDisplayBounds(d, &r) == 0 && g_win2_rect[0] + 40 > r.x && g_win2_rect[0] + 40 < r.x + r.w && g_win2_rect[1] + 10 >= r.y && g_win2_rect[1] + 10 < r.y + r.h) placed = 1;
        }
        if (placed) { x = g_win2_rect[0]; y = g_win2_rect[1]; w = g_win2_rect[2]; h = g_win2_rect[3]; }
    }
    if (!placed && SDL_GetNumVideoDisplays() > 1) {              /* another monitor than the game's */
        int d = SDL_GetWindowDisplayIndex(g_win) == 0 ? 1 : 0;
        x = y = SDL_WINDOWPOS_CENTERED_DISPLAY(d); placed = 1;
    }
    if (!placed) {                                                /* one monitor: beside the game's window */
        int mx, my, mw, mh; SDL_GetWindowPosition(g_win, &mx, &my); SDL_GetWindowSize(g_win, &mw, &mh);
        SDL_Rect r;
        if (SDL_GetDisplayUsableBounds(SDL_GetWindowDisplayIndex(g_win), &r) == 0 && mx + mw + 16 + w <= r.x + r.w) { x = mx + mw + 16; y = my; }
    }
    g_win2 = SDL_CreateWindow("Dragon Quest IX", x, y, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!g_win2) { fprintf(stderr, "second window: %s\n", SDL_GetError()); return; }
    window_icon_for(g_win2);
    menu_init_second(g_win2);                                    /* its own bar: Fullscreen */
    second_window_title();
    if (SDL_GL_MakeCurrent(g_win2, g_glctx) == 0) SDL_GL_SetSwapInterval(0);
    SDL_GL_MakeCurrent(g_win, g_glctx);
    if (g_win2_full) { menu_show_second(0); SDL_SetWindowFullscreen(g_win2, SDL_WINDOW_FULLSCREEN_DESKTOP); }
    SDL_RaiseWindow(g_win);                                      /* the keyboard stays with the game's window */
}
static void second_window_fullscreen(void) {
    if (!g_win2) return;
    if (!g_win2_full) second_window_keep_rect();
    g_win2_full = !g_win2_full;
    menu_show_second(!g_win2_full);
    SDL_SetWindowFullscreen(g_win2, g_win2_full ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

static void lobby_hook(void);
static void idle_pump(void);
static void handle_events(void);
static int g_cap_action;
static void capture_key(SDL_Scancode k, Uint16 mod);
static void capture_pad(int pad);
void frontend_init(int argc, char** argv) {
    (void)argc; (void)argv;
    SDL_SetMainReady();
    g_report_perf = perf_write;
    g_report_window = report_window;
    g_headless = getenv("DQIX_NOWINDOW") != NULL;
    if (getenv("DQIX_HIDDEN")) { g_hidden = g_headless = 1; return; }
    g_analog_walk = 1;                                          /* Modern: walking with the stick is on unless turned off */
    g_save_backups = 1;                                         /* Game > Keep a copy of every save: on unless turned off */
    skins_scan();
    default_console_color();
    if (getenv("DQIX_NO_SETTINGS")) controls_defaults(); else controls_load();
    if (!getenv("DQIX_NO_SETTINGS")) {
        settings_load();
        if (!g_wireless.mac_set && !g_headless) { wireless_mac_new(); settings_save(); }
    }
    wireless_apply(1);                                          /* read by firmware_init */
    if (g_opt.dlc) backup_unlock_dlc();                        /* before the game reads the save */
    if (getenv("DQIX_MODE")) g_classic = strcmp(getenv("DQIX_MODE"), "enhanced") != 0;   /* tests */
    /* Modern > Start: not in a session (every console starts alike) nor a replay (its inputs hold the presses) */
    if (getenv("DQIX_SKIP_INTRO")) bootskip_start(atoi(getenv("DQIX_SKIP_INTRO")));   /* tests */
    else bootskip_start(getenv("DQIX_SESSION") || getenv("DQIX_INPUT") || getenv("DQIX_INPUT_FILE") ? BOOTSKIP_OFF : g_skip_intro);
    /* real pixels on high-DPI Windows setups (not stretched by the system), so
     * the pixel grid and actual size are drawn on the monitor's own pixels */
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    /* Controllers: the buttons by position (right = A, bottom = B, like the DS)
     * for every brand. SDL otherwise reports Nintendo controllers (Switch Pro,
     * Joy-Cons) by their labels, which would swap A/B and X/Y on them. */
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS, "1");   /* two Joy-Cons are one controller */
    SDL_SetHint("SDL_QUIT_ON_LAST_WINDOW_CLOSE", "0");             /* closing the window asks first (quit_ask), not a second time */
    if (SDL_Init((g_headless ? 0 : SDL_INIT_VIDEO) | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        exit(1);
    }
    atexit(SDL_Quit);
    if (!g_headless) {
        const char* sc = getenv("DQIX_SCALE");
        if (sc) { g_scale = atoi(sc); if (g_scale < 1) g_scale = 1; if (g_scale > 8) g_scale = 8; }
        if (!try_gl()) {
            g_win = SDL_CreateWindow("Dragon Quest IX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 256 * g_scale, 384 * g_scale,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
            if (!g_win) { fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError()); exit(1); }
            g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED);
            if (!g_ren) g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_SOFTWARE);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
            g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 384);
        }
        window_icon();
        {   /* started again by the program itself (reset, sessions): the window where it was */
            const char* wp = getenv("DQIX_WINPOS"); int x, y;
            if (wp && sscanf(wp, "%d,%d", &x, &y) == 2) SDL_SetWindowPosition(g_win, x, y);
        }
        apply_mode();
        ui_lang_load(ui_language_wanted());
        menu_build();
        if (g_update_auto && !session_requested() && update_project_dir()) update_check_async();
        update_actual();
        menu_sync();
        layout_window();
        if (g_fullscreen) { menu_show(0); SDL_SetWindowFullscreen(g_win, SDL_WINDOW_FULLSCREEN_DESKTOP); }
        second_window_sync();
        atexit(settings_save);
        session_lobby_hook = lobby_hook;
        air_idle_hook = idle_pump;
    }
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 32768; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024; want.callback = audio_cb;
    g_audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);           /* SDL converts to the device's format */
    if (g_audio) SDL_PauseAudioDevice(g_audio, 0);
    else fprintf(stderr, "no audio: %s\n", SDL_GetError());
    for (int i = 0; i < SDL_NumJoysticks(); i++) if (SDL_IsGameController(i)) { g_pad = SDL_GameControllerOpen(i); break; }
    controls_pad_detect(g_pad);                                 /* Game > Controls names its buttons (Cross, Circle, L2...) */
    g_perf_freq = SDL_GetPerformanceFrequency();
    g_next_frame = SDL_GetPerformanceCounter();
    g_fps_t0 = g_next_frame;
}

static uint16_t key_bits(const Uint8* ks) { return controls_ds_keys(ks); }   /* Game > Controls (controls.c) */

static uint16_t pad_bits(void) {
    if (!g_pad) { g_fe_stick_x = g_fe_stick_y = g_fe_rstick_x = 0; g_fe_stick_keys = 0; return 0; }
    SDL_GameController* p = g_pad;
    uint16_t k = controls_ds_pad(p);                           /* the buttons: Game > Controls (by position by default, like a DS) */
    int lx = SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_LEFTX), ly = SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_LEFTY);
    uint16_t s = 0;                                              /* the stick as a D-pad (input.c decides) */
    if (lx > 16000) s |= 1 << 4; if (lx < -16000) s |= 1 << 5;
    if (ly < -16000) s |= 1 << 6; if (ly > 16000) s |= 1 << 7;
    g_fe_stick_x = lx; g_fe_stick_y = ly; g_fe_stick_keys = s;
    g_fe_rstick_x = SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_RIGHTX);
    return k;
}

/* Window pixel -> bottom-screen pixel; returns 0 when outside it, unless
 * clamp is set (a drag that started on the screen stays on its edge, as a
 * stylus would at the rim). */
static int to_touch_in(SDL_Window* win, int wx, int wy, int* tx, int* ty, int clamp) {
    float lx, ly;
    GlPresent v;
    if (!win || (win != g_win && win != g_win2)) win = g_win;
    if (g_gl) {                                                  /* same layout as gl_present() */
        int ww, wh; SDL_GetWindowSize(win, &ww, &wh);
        canvas_layout_for(&v, ww, wh, win == g_win2);
        lx = ((float)wx + 0.5f - v.ox) / v.sc; ly = ((float)wy + 0.5f - v.oy) / v.sc;
    } else { canvas_layout(&v, 1, 1); SDL_RenderWindowToLogical(g_ren, wx, wy, &lx, &ly); }
    if (v.npiece && v.piece[0][4] > 0.5f) {                       /* the battle HUD's panels: back to where they are on the touch screen */
        static int last;                                          /* the panel last touched: a drag off it stays on its edge */
        int hit = -1;
        for (int i = v.npiece - 1; i >= 0 && hit < 0; i--) {
            const float* q = v.piece[i];
            if (q[4] > 0.5f && lx >= q[0] && lx < q[0] + q[2] && ly >= q[1] && ly < q[1] + q[3]) hit = i;   /* (not a piece of the top screen) */
        }
        if (hit < 0 && !(clamp && last < v.npiece)) return 0;   /* the rest of the commands screen isn't shown */
        if (hit >= 0) last = hit;
        const float* q = v.piece[last];
        float px = fminf(fmaxf(lx, q[0]), q[0] + q[2] - 0.01f), py = fminf(fmaxf(ly, q[1]), q[1] + q[3] - 0.01f);
        int x = (int)floorf(q[5] + (px - q[0]) * q[7] / q[2]), y = (int)floorf(q[6] + (py - q[1]) * q[8] / q[3]);
        if (x < 0 || x > 255 || y < 0 || y > 191) return 0;
        *tx = x; *ty = y;
        return 1;
    }
    if (v.front == 0 && v.top[2] < 256 && lx >= v.top[0] && lx < v.top[0] + v.top[2] && ly >= v.top[1] && ly < v.top[1] + v.top[3])
        return 0;                                                 /* on the small top screen in front of it */
    int side = 0; if (g_gl) gl_wide_shown(&side);
    if (g_gl && v.bot[2] <= 0) return 0;                         /* not shown */
    float bw = v.bot[2] > 0 ? v.bot[2] : 256, bh = v.bot[3] > 0 ? v.bot[3] : 192, bx = v.botx[1] > 0 ? v.botx[1] : 256;
    int x = (int)floorf((lx - v.bot[0]) * bx / bw + (g_gl ? v.botx[0] - (float)side : 0)), y = (int)floorf((ly - v.bot[1]) * 192.0f / bh);
    if (g_sign_move && g_frame - g_sign_frame < 4 && x >= g_sign_rect[0] && x < g_sign_rect[2] && y >= g_sign_rect[1] && y < g_sign_rect[3]) {
        x -= g_sign_dx; y -= GX_SIGN_DY;                          /* the "X Menu" sign, moved: where the game has it */
    }
    if (clamp) { x = x < 0 ? 0 : x > 255 ? 255 : x; y = y < 0 ? 0 : y > 191 ? 191 : y; }
    if (x < 0 || x > 255 || y < 0 || y > 191) return 0;
    *tx = x; *ty = y;
    return 1;
}
static int to_touch(int wx, int wy, int* tx, int* ty, int clamp) { return to_touch_in(g_win, wx, wy, tx, ty, clamp); }

static int g_shot_req;
static void save_bmp_screens(const uint32_t scr[2][256 * 192], const char* path) {
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, 256, 384, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!sf) return;
    for (int y = 0; y < 384; y++) memcpy((uint8_t*)sf->pixels + y * sf->pitch, &scr[y / 192][(y % 192) * 256], 256 * 4);
    SDL_SaveBMP(sf, path);
    SDL_FreeSurface(sf);
}

static void save_screenshot(void) {
    if (g_gl) {
        static uint32_t scr[2][256 * 192];
        static int n;
        char path[64];
        gl_read_screens(scr);
        snprintf(path, sizeof path, "dqix-shot-%d.bmp", n++);
        save_bmp_screens(scr, path);
        fprintf(stderr, "screenshot: %s\n", path);
        return;
    }
    int w, h;
    SDL_GetRendererOutputSize(g_ren, &w, &h);
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!sf) return;
    if (SDL_RenderReadPixels(g_ren, NULL, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch) == 0) {
        static int n;
        char path[64];
        snprintf(path, sizeof path, "dqix-shot-%d.bmp", n++);
        SDL_SaveBMP(sf, path);
        fprintf(stderr, "screenshot: %s\n", path);
    }
    SDL_FreeSurface(sf);
}

/* tests: DQIX_MEMFIND="frame,hex,hex...": at that frame, the main RAM / DTCM
 * addresses holding each 32-bit value, and the projection matrix */
static void memfind_hook(void) {
    static long long at = -2; static unsigned vals[8]; static int nv;
    if (at == -2) {
        at = -1; const char* e = getenv("DQIX_MEMFIND");
        if (e) { at = atoll(e); const char* c = e; while ((c = strchr(c, ',')) && nv < 8) { c++; vals[nv++] = (unsigned)strtoul(c, NULL, 16); } }
    }
    if (at < 0 || g_frame != (uint64_t)at) return;
    extern void gpu3d_debug_proj(void);
    gpu3d_debug_proj();
    {   const char* d = getenv("DQIX_MEMDUMP");                  /* "addr,addr,...": 0x20 bytes before and after each */
        while (d && *d) { uint32_t a = (uint32_t)strtoul(d, NULL, 16) & 0x3FFFFC;
            fprintf(stderr, "dump %08x:", 0x02000000u + a - 0x20); for (int i = -8; i < 8; i++) { uint32_t v; memcpy(&v, g_main_ram + a + i * 4, 4); fprintf(stderr, " %08x", v); } fprintf(stderr, "\n");
            d = strchr(d, ','); if (d) d++; } }
    for (int k = 0; k < nv; k++) {
        int n = 0;
        for (uint32_t a = 0; a + 4 <= MAIN_RAM_SIZE && n < 200; a += 2) { uint32_t v; memcpy(&v, g_main_ram + a, 4); if (v == vals[k]) { fprintf(stderr, "memfind %08x at %08x\n", vals[k], 0x02000000u + a); n++; } }
        for (uint32_t a = 0; a + 4 <= DTCM_SIZE; a += 2) { uint32_t v; memcpy(&v, g_dtcm + a, 4); if (v == vals[k]) fprintf(stderr, "memfind %08x at dtcm+%04x\n", vals[k], a); }
    }
}

static void journal_ui_toggle(void);
static void do_action(int id);
/* tests: DQIX_TEST_ACTIONS="frame:menu id;frame:resize:w:h;...": menu actions and window resizes at given frames */
static void test_actions_hook(void) {
    static const char* e = (const char*)-1; if (e == (const char*)-1) e = getenv("DQIX_TEST_ACTIONS");
    if (!e) return;
    for (const char* c = e; c && *c; ) {
        unsigned long long f = strtoull(c, NULL, 10); const char* a = strchr(c, ':');
        if (a && f == g_frame) {
            int w, h;
            if (!strncmp(a + 1, "resize:", 7) && sscanf(a + 8, "%d:%d", &w, &h) == 2) SDL_SetWindowSize(g_win, w, h);
            else do_action(atoi(a + 1));
        }
        c = strchr(c, ';'); if (c) c++;
    }
}

static void test_hooks(void) {
    memfind_hook();
    test_actions_hook();
    static long long report_at = -2;
    if (report_at == -2) { const char* e = getenv("DQIX_REPORT_AT"); report_at = e ? atoll(e) : -1; }
    if (report_at >= 0 && g_frame == (uint64_t)report_at) report_write("DQIX_REPORT_AT (test)");
    static long long shot_at = -2;
    if (shot_at == -2) { const char* e = getenv("DQIX_SHOT_AT"); shot_at = e ? atoll(e) : -1; }
    if (g_shot_req || (shot_at >= 0 && g_frame == (uint64_t)shot_at)) { save_screenshot(); g_shot_req = 0; }
}

/* DQIX_GL_DUMP="dir,every": write the GL-composed screens (256x384) like DQIX_DUMP (tests) */
static void gl_dump_hook(void) {
    static int every = -1; static char dir[400];
    if (every < 0) {
        const char* e = getenv("DQIX_GL_DUMP");
        every = 0;
        if (e) { const char* c = strchr(e, ','); size_t n = c ? (size_t)(c - e) : strlen(e); if (n >= sizeof dir) n = sizeof dir - 1; memcpy(dir, e, n); dir[n] = 0; every = c ? atoi(c + 1) : 60; }
    }
    if (every <= 0 || g_frame % (uint64_t)every) return;
    char path[600]; snprintf(path, sizeof path, "%s/f%06llu.ppm", dir, (unsigned long long)g_frame);
    FILE* f = fopen(path, "wb");
    if (!f) return;
    if (getenv("DQIX_GL_DUMP_FULL")) {                       /* at the rendering resolution */
        int w, h; uint8_t* rgb = gl_read_full(&w, &h);
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        fwrite(rgb, 1, (size_t)w * h * 3, f);
        free(rgb);
    } else {                                                 /* native 256x384, for comparisons */
        static uint32_t scr[2][256 * 192];
        gl_read_screens(scr);
        fprintf(f, "P6\n256 384\n255\n");
        for (int sc = 0; sc < 2; sc++) for (int i = 0; i < 256 * 192; i++) { uint32_t c = scr[sc][i]; uint8_t rgb[3] = { (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c }; fwrite(rgb, 1, 3, f); }
    }
    fclose(f);
}

/* ---------------------------------------------------------------- frame timings (for bug reports) */
#define PERF_N 1800                                          /* the last 30 seconds */
typedef struct { uint64_t frame; float busy, gl, upload, present, wait, d2; int uploads, polys, audio_ms; uint8_t fast; } PerfRec;
static PerfRec g_perf[PERF_N];
static unsigned g_perf_pos;
static uint64_t g_perf_end;                                  /* end of the previous frame (after pacing) */
static double ticks_ms(uint64_t t) { return (double)t * 1000.0 / (double)g_perf_freq; }

static void perf_write(FILE* f) {
    fprintf(f, "mode: %s\n", g_classic ? "classic (DS 3D renderer)" : "enhanced (3D on the GPU)");
    fprintf(f, "renderer: %s\n", g_gl ? gl_renderer_name() : "software (no OpenGL 3.3)");
    if (g_gl && !g_classic) fprintf(f, "3D resolution: %dx\n", gl_render_scale());
    fprintf(f, "audio: %s\n\n", g_audio ? "on" : "off");
    fprintf(f, "Per frame, in ms. A frame must take under 16.7 ms in total (busy + present) to keep full speed.\n");
    fprintf(f, "busy = all work for the frame: game = game code, 2d = 2D layers, gl = 3D on the GPU, upload = new textures (inside gl),\n");
    fprintf(f, "present = showing the frame, wait = idle time to keep the pace, audio = sound queued.\n\n");
    unsigned n = g_perf_pos < PERF_N ? g_perf_pos : PERF_N - 1;     /* the slot at g_perf_pos is the frame in progress */
    unsigned slow = 0; double worst = 0; uint64_t worst_f = 0;
    for (unsigned k = n; k > 0; k--) {
        const PerfRec* r = &g_perf[(g_perf_pos - k) % PERF_N];
        double t = r->busy + r->present;
        if (t > 16.7) slow++;
        if (t > worst) { worst = t; worst_f = r->frame; }
    }
    fprintf(f, "frames: %u, over 16.7 ms: %u, worst %.1f ms at frame %llu\n\n", n, slow, worst, (unsigned long long)worst_f);
    fprintf(f, "  frame    busy    game      2d      gl  upload (n)   polys  present    wait  audio\n");
    for (unsigned k = n; k > 0; k--) {
        const PerfRec* r = &g_perf[(g_perf_pos - k) % PERF_N];
        fprintf(f, "%7llu %7.1f %7.1f %7.1f %7.1f %7.1f (%3d) %6d %8.1f %7.1f %6d%s%s\n", (unsigned long long)r->frame, r->busy,
                r->busy - r->d2 - r->gl, r->d2, r->gl, r->upload, r->uploads,
                r->polys, r->present, r->wait, r->audio_ms, r->fast ? " fast" : "", r->busy + r->present > 16.7 ? "  <-- slow" : "");
    }
}

/* DQIX_WINSHOT_AT=frame[,frame...]: save the whole window as shown (tests) */
static char g_shot_next[32];                                    /* tests: save the next frame drawn there (DQIX_MENU_TEST) */
static void window_shot_hook(int w, int h, int second) {
    if (g_shot_next[0] && !second) {
        uint8_t* rgb = gl_read_window(w, h);
        FILE* f = fopen(g_shot_next, "wb");
        if (f) { fprintf(f, "P6\n%d %d\n255\n", w, h); fwrite(rgb, 1, (size_t)w * h * 3, f); fclose(f); }
        free(rgb);
        g_shot_next[0] = 0;
    }
    static char list[20000]; static int init;
    if (!init) { init = 1; const char* e = getenv("DQIX_WINSHOT_AT"); if (e) snprintf(list, sizeof list, ",%s,", e); }
    if (!list[0]) return;
    char key[32]; snprintf(key, sizeof key, ",%llu,", (unsigned long long)g_frame);
    if (!strstr(list, key)) return;
    uint8_t* rgb = gl_read_window(w, h);
    char path[64]; snprintf(path, sizeof path, second ? "win%06llu_2.ppm" : "win%06llu.ppm", (unsigned long long)g_frame);
    FILE* f = fopen(path, "wb");
    if (f) { fprintf(f, "P6\n%d %d\n255\n", w, h); fwrite(rgb, 1, (size_t)w * h * 3, f); fclose(f); }
    free(rgb);
}

/* Bug reports: the whole window as shown (the layout, the console photo...),
 * drawn again from the last picture and read back; halved until it is at most
 * 1600 wide so the report stays small. */
void report_write_bmp_rgb(const char* path, const uint8_t* rgb, int w, int h);
static void report_window(const char* path) {
    if (!g_gl || !g_win || g_headless) return;
    int w, h; SDL_GL_GetDrawableSize(g_win, &w, &h);
    if (w <= 0 || h <= 0) return;
    GlPresent v; canvas_layout(&v, w, h); v.again = 1;          /* (again: the motion blur as it is) */
    gl_present(&v);
    uint8_t* rgb = gl_read_window(w, h);
    while (w > 1600 && rgb) {                                     /* average 2x2 blocks */
        int nw = w / 2, nh = h / 2;
        for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) for (int c = 0; c < 3; c++) {
            const uint8_t* q = rgb + ((size_t)(2 * y) * w + 2 * x) * 3 + c;
            rgb[((size_t)y * nw + x) * 3 + c] = (uint8_t)((q[0] + q[3] + q[(size_t)w * 3] + q[(size_t)w * 3 + 3] + 2) / 4);
        }
        w = nw; h = nh;
    }
    if (rgb) report_write_bmp_rgb(path, rgb, w, h);
    free(rgb);
}

/* Widescreen: is there 2D (a menu, the title) where the small screen sits on
 * the big one? Then slide to the corner layout, and back when it's only the
 * field again. From the 3D screen's last picture: pixels where no 2D layer
 * covers the 3D are the field. */
extern uint8_t g_comp_info[256 * 192 * 4];
extern int g_comp_has3d, g_comp_3d_on2d;
extern uint32_t g_engine_out[2][256 * 192];
static void wide_update(int a_on_top) {
    if (!g_wide || !WIDE_GL || g_wide_pop) { g_wide_mix = 1.0f; if (!g_wide || !WIDE_GL) return; }
    int side = 0, which = gl_wide_shown(&side);
    {   /* the small screen: the one without the 3D (engine B), or the top one */
        int small_top = which != 0;
        int eng = small_top == a_on_top ? 0 : 1;
        const uint32_t* px = g_engine_out[eng];
        int blank = 1;
        for (int i = 0; i < 256 * 192 && blank; i += 7) if (px[i] & 0xFFFFFF) blank = 0;
        g_small_blank = blank;
    }
    if (g_minimap && which == 1 && g_comp_has3d) {   /* the minimap: is the game's 2D (a menu) under it? Then it fades out, and back once that's gone */
        /* (only while it can show: no popping back over a shop's menu after its card) */
        static const int SW[3] = { 64, 85, 128 }, SH[3] = { 48, 64, 96 };
        int z = g_small_size < 0 || g_small_size > 2 ? 1 : g_small_size, c = g_small_corner & 3, S = wide_side_target();
        int x0 = c == 0 || c == 3 ? MINIMAP_GAP - S : 256 + S - SW[z] - MINIMAP_GAP, y0 = c < 2 ? MINIMAP_GAP : 192 - SH[z] - MINIMAP_GAP;   /* on the DS's picture (S more at the canvas' left) */
        int n = 0, n2d = 0, nmix = 0;
        for (int y = y0 > 0 ? y0 : 0; y < y0 + SH[z] && y < 192; y += 2)
            for (int x = x0 > 0 ? x0 : 0; x < x0 + SW[z] && x < 256; x += 2) {
                int mode = g_comp_info[(y * 256 + x) * 4] & 3;
                n++; n2d += mode == 0; nmix += mode == 2;
            }
        int under = n > 0 && (n2d * 20 > n || nmix * 8 > n);     /* more than 5%, or 12% of a see-through window (a shop's) */
        static int clear;                                         /* frames without 2D under it */
        clear = under ? 0 : clear + 1;
        if (under) g_mini_vis = fmaxf(0.0f, g_mini_vis - 1.0f / 6);
        else if (clear >= 20) g_mini_vis = fminf(1.0f, g_mini_vis + 1.0f / 12);
    }
    /* 3D on the top screen in a menu (the monster list, battle records: a model
     * over a 2D page), not a battle (the 3D over the backdrop) */
    g_top_menu = which == 0 && g_comp_has3d && g_comp_3d_on2d * 4 > 256 * 192;
    int covered = 1;
    if (which == 0) covered = 0;                                  /* 3D on the top screen: no sliding */
    else if (which == 1 && g_comp_has3d) {
        int n2d = 0, n = 0;
        for (int y = 2; y < 66; y += 2)
            for (int x = 0; x < 46; x += 2) { n++; if ((g_comp_info[(y * 256 + x) * 4] & 3) == 0) n2d++; }
        covered = n2d * 20 > n;                                   /* more than 5% */
    }
    static int frames;                                            /* how long it has been like this */
    static int last = -1;
    if (covered != last) { last = covered; frames = 0; } else frames++;
    float target = g_wide_mix;
    if (covered && frames >= 1) target = 0.0f;                   /* make room quickly */
    if (!covered && frames >= 20) target = 1.0f;                  /* back to wide once it stays clear */
    float step = 1.0f / 12.0f;
    if (g_wide_mix < target) g_wide_mix = fminf(target, g_wide_mix + step);
    else if (g_wide_mix > target) g_wide_mix = fmaxf(target, g_wide_mix - step);
}

/* Widescreen battles (3D on the top screen): find the party's panels on the
 * commands screen. They are stacked across it, each framed in white with a
 * grey line (0x515151) between them; the one whose turn it is is taller and
 * holds the commands. Anything else there (no such lines) keeps the usual
 * small screen. In the field, keep the map for the map window. */
static void hud_update(int a_on_top) {
    g_hud_n = 0; g_hud_win[2] = 0;
    if (!WIDE_GL || !g_hud || !g_wide || g_side_by_side || (g_two && !g_single)) return;
    int side = 0, which = gl_wide_shown(&side);
    if (which == 1 && g_wide_pop && !g_minimap) {                 /* the field: the map the window keeps during battles */
        uint16_t mb; memcpy(&mb, &g_io[(a_on_top ? 0 : 0x1000) + 0x6C], 2);   /* the top screen's engine (the map) */
        static unsigned n;                                        /* a copy of the whole picture: a few times a second is enough */
        if (n++ % 10 == 0 && !g_small_blank && (!(mb >> 14) || !(mb & 0x1F))) gl_hold();   /* not while it fades out (into a battle) */
        return;
    }
    if (which != 0) return;
    const uint32_t* px = g_engine_out[a_on_top ? 1 : 0];          /* the bottom screen's engine */
    int sep[192], cut[192], nsep = 0;                             /* cut: a line the list's description box covers on the right (Spells, Items...) */
    for (int y = 0; y < 192; y++) {
        int n = 0, nr = 0;
        for (int x = 10; x < 120; x++) if ((px[y * 256 + x] & 0xFFFFFF) == 0x515151) n++;
        for (int x = 130; x < 246; x++) if ((px[y * 256 + x] & 0xFFFFFF) == 0x515151) nr++;
        sep[y] = n > 100; cut[y] = sep[y] && nr < 50; nsep += sep[y] && !cut[y];
    }
    if (nsep < 1) {                                               /* no panels: one window alone (experience, level up)? */
        if (g_comp_3d_on2d * 4 > 256 * 192) return;              /* (not a menu with a 3D model on top: Attributes, Equip, whose list is that grey too) */
        int x0 = 256, y0 = 192, x1 = -1, y1 = -1, n = 0;              /* its inside: dark grey 0x303030, in both styles (one line of experience: 33 rows) */
        for (int y = 0; y < 192; y++)
            for (int x = 0; x < 256; x++)
                if ((px[y * 256 + x] & 0xFFFFFF) == 0x303030) { n++; if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
        if (x1 - x0 < 100 || y1 - y0 < 24 || n * 10 < (x1 - x0 + 1) * (y1 - y0 + 1) * 7) return;   /* (mostly that grey: one window) */
        x0 = x0 < 3 ? 0 : x0 - 3; y0 = y0 < 3 ? 0 : y0 - 3; x1 = x1 > 252 ? 255 : x1 + 3; y1 = y1 > 188 ? 191 : y1 + 3;   /* its frame */
        g_hud_win[0] = x0; g_hud_win[1] = y0; g_hud_win[2] = x1 - x0 + 1; g_hud_win[3] = y1 - y0 + 1;
        return;
    }
    int n = 0, tall = 0;
    for (int y = 0; y < 192;) {
        if (sep[y]) { y++; continue; }
        int y0 = y;
        while (y < 192 && !sep[y]) y++;
        if (y - y0 < 8) continue;                                 /* a border, not a panel */
        /* a member's panel: 30-45 rows (the one whose turn it is: up to 80), framed in white */
        int h = y - y0, mid = (y0 + y) / 2, framed = 0;
        for (int x = 0; x < 16; x++) if ((px[mid * 256 + x] & 0xFFFFFF) == 0xFBFBFB) framed = 1;
        if (!framed && n == 0) continue;                          /* above the panels (while the turn plays out): the backdrop */
        if (!framed && n >= 2) break;                             /* below them: nothing more */
        if (n == 8 || h < 30 || h > 80 || !framed || (h >= 50 && tall++)) return;
        g_hud_band[n][0] = y0; g_hud_band[n][1] = y; n++;
    }
    if (n < 2) return;
    /* a list open (Spells, Items...): its description box covers the right of
     * the panels under the one whose turn it is. Those and the tall one in
     * one piece, as on the DS; the others in the row. */
    int a = -1, b = -1;
    for (int i = 0; i < n; i++) if (g_hud_band[i][1] - g_hud_band[i][0] >= 50) a = b = i;
    for (int i = 0; a >= 0 && i + 1 < n; i++)
        for (int y = g_hud_band[i][1]; y < g_hud_band[i + 1][0]; y++)
            if (cut[y]) { if (i < a) a = i; if (i + 1 > b) b = i + 1; break; }
    if (a >= 0 && b > a) {
        g_hud_band[a][1] = g_hud_band[b][1];
        for (int i = b + 1; i < n; i++) { g_hud_band[i - (b - a)][0] = g_hud_band[i][0]; g_hud_band[i - (b - a)][1] = g_hud_band[i][1]; }
        n -= b - a;
    }
    if (n >= 1) g_hud_n = n;
}

/* Battles: someone's commands are on the commands screen, the game waits for
 * the player (their panel is the tall one between the grey lines, as
 * hud_update finds it; while the turn plays out every panel is short). Read
 * from the commands screen's engine itself, so in every layout. */
static int battle_commands_shown(void) {
    const uint32_t* px = g_engine_out[1];                         /* in battles the 3D is on the top screen: engine B is the bottom one */
    int y0 = -1;
    for (int y = 0; y <= 192; y++) {
        int n = 0;
        if (y < 192) for (int x = 10; x < 120; x++) if ((px[y * 256 + x] & 0xFFFFFF) == 0x515151) n++;
        if (y < 192 && n <= 100) { if (y0 < 0) y0 = y; continue; }
        if (y0 >= 0 && y - y0 >= 50 && y - y0 <= 80) {             /* a band this tall: framed in white? */
            int mid = (y0 + y) / 2;
            for (int x = 0; x < 16; x++) if ((px[mid * 256 + x] & 0xFFFFFF) == 0xFBFBFB) return 1;
        }
        y0 = -1;
    }
    return 0;
}

/* 3D resolution Auto: render at 1.5 times the size the 3D is shown at (at
 * least 2x, at most what the GPU holds), so it is never stretched and the
 * extra detail smooths the edges. Changes after the size has stayed the same
 * for half a second (resizing a window doesn't rebuild it every frame). */
static void auto_scale(void) {
    if (!g_res_auto || g_classic || getenv("DQIX_SCALE3D")) return;
    { static long long from = -2; if (from == -2) { const char* e = getenv("DQIX_AUTO_FROM"); from = e ? atoll(e) : -1; }   /* tests: 2x until then */
      if (from >= 0 && g_frame < (uint64_t)from) return; }
    float need = 0;
    SDL_Window* ws[2] = { g_win, g_win2 };
    for (int k = 0; k < 2; k++) {
        if (!ws[k]) continue;
        int w, h; SDL_GL_GetDrawableSize(ws[k], &w, &h);
        GlPresent v; canvas_layout_for(&v, w, h, k);
        for (int i = 0; i < 2; i++) {
            const float* r = i ? v.bot : v.top, *x = i ? v.botx : v.topx;
            if (r[2] > 0 && x[1] > 0) { float p = v.sc * r[2] / x[1]; if (p > need) need = p; }
        }
    }
    int want = (int)ceilf(need * 1.5f - 0.05f), top = gl_render_max_scale();
    if (want < 2) want = 2;
    if (want > top) want = top;
    static int pending, frames;
    if (want == gl_render_scale()) { pending = 0; frames = 0; return; }
    if (want != pending) { pending = want; frames = 0; return; }
    if (++frames >= 30) { gl_render_set_scale(want); pending = frames = 0; }
}

/* Where the pop-up goes (g_pop_at): in the middle at 80%, unless that hides
 * the game's menus or text on the field (2D in front of the 3D: g_comp_info,
 * and the dialogue window, 3D: gl_dialog_box());
 * then the place, as near the middle and as big (down to 55%) as hides least
 * of them. It stays put until a place clearly better shows up. */
static int g_pop_sum[193][257];                                   /* menu pixels above and left of each point of the DS's picture */
static int g_pop_held;                                            /* g_pop_at is where the page was last frame */
static float pop_cost(float x, float y, float k, float S, float w, float h, float k0) {
    int a0 = (int)fmaxf(0, x - S), a1 = (int)fminf(256, x + w * k - S), b0 = (int)fmaxf(0, y), b1 = (int)fminf(192, y + h * k);   /* (canvas -> the DS's picture: S less) */
    int n = a1 > a0 && b1 > b0 ? g_pop_sum[b1][a1] - g_pop_sum[b0][a1] - g_pop_sum[b1][a0] + g_pop_sum[b0][a0] : 0;
    /* each step away from the middle, and each 5% smaller, counts as a few hidden pixels */
    return (float)n + 2.0f * (fabsf(x + w * k / 2 - (128 + S)) + fabsf(y + h * k / 2 - 96)) + 400.0f * (k0 - k) / 0.05f;
}
static void pop_place(void) {
    float S = (float)wide_side_target(), cw = 256 + 2 * S, w = (float)g_pop_win[2], h = (float)g_pop_win[3];
    int side = 0, field = gl_wide_shown(&side) == 1 && g_comp_has3d, d[4], dlg = field && gl_dialog_box(d);
    for (int y = 0; y < 192; y++)
        for (int x = 0, row = 0; x < 256; x++) {
            row += field && ((g_comp_info[(y * 256 + x) * 4] & 3) != 1 || (dlg && x >= d[0] && x < d[2] && y >= d[1] && y < d[3]));   /* (the dialogue window is 3D) */
            g_pop_sum[y + 1][x + 1] = g_pop_sum[y][x + 1] + row;
        }
    float k0 = fminf(1.0f, fminf(0.8f * cw / w, 0.8f * 192.0f / h));
    float bx = (cw - w * k0) / 2, by = (192 - h * k0) / 2, bk = k0, best = pop_cost(bx, by, bk, S, w, h, k0);
    if (best > 0)
        for (float k = k0; k >= 0.55f - 1e-3f; k -= 0.05f)
            for (float y = 0; y + h * k <= 192.001f; y += 4)
                for (float x = 0; x + w * k <= cw + 0.001f; x += 4) {
                    float c = pop_cost(x, y, k, S, w, h, k0);
                    if (c < best) { best = c; bx = x; by = y; bk = k; }
                }
    if (g_pop_held) {                                             /* where it was: still good enough? */
        float px = g_pop_at[0], py = g_pop_at[1], pk = g_pop_at[2];
        if (py + h * pk <= 192.5f && px + w * pk <= cw + 0.5f && pop_cost(px, py, pk, S, w, h, k0) <= best + 300.0f) return;
    }
    g_pop_at[0] = bx; g_pop_at[1] = by; g_pop_at[2] = bk;
    g_pop_held = 1;
}

/* The top screen's page (top_page_mode() 1): which of its pixels are its
 * backdrop, left out of the pop-up (tagged 1 in the top byte, gl_set_top_cut()),
 * and the rest's bounds (g_pop_win). A backdrop of its own (the curtain behind
 * a title: one layer under the windows' layers) goes whole; a page drawn as one
 * picture (an item's card) is cut into its parts by colour: the parchment
 * around them, and what touches the edge (the sky, the torn border), go. */
static void pop_update(int a_on_top) {
    if (!g_pop_win[2]) g_pop_held = 0;                            /* (not up last frame: placed afresh) */
    g_pop_win[2] = 0;
    if (top_page_mode() != 1) return;
    enum { N = 256 * 192 };
    static uint8_t bg[N];
    static int lab[N], q[N], size[N], ring[N];
    static uint8_t flags[N];                                      /* 1 touches the edge, 2 reaches inside */
    int eng = a_on_top ? 0 : 1;
    uint32_t* px = g_engine_out[eng];
    const uint8_t* ly = g_engine_layer[eng];
    int n[8] = { 0 }, big = 0;
    for (int i = 0; i < N; i++) n[ly[i] & 7]++;
    for (int L = 1; L < 4; L++) if (n[L] > n[big]) big = L;
    if (n[big] < N * 9 / 10) {
        for (int i = 0; i < N; i++) bg[i] = ly[i] == big || ly[i] == LAYER_BACKDROP;
    } else {
        /* parts: neighbours within 24 levels of each other; the parchment is
         * the part with most pixels in a ring a little inside the edge */
        int ncomp = 0, best = -1;
        for (int i = 0; i < N; i++) lab[i] = -1;
        for (int i = 0; i < N; i++) {
            if (lab[i] >= 0) continue;
            int c = ncomp++, head = 0, tail = 0;
            size[c] = ring[c] = 0; flags[c] = 0;
            lab[i] = c; q[tail++] = i;
            while (head < tail) {
                int j = q[head++], x = j % 256, y = j / 256, d = x < y ? x : y;
                if (255 - x < d) d = 255 - x;
                if (191 - y < d) d = 191 - y;
                size[c]++;
                if (d == 0) flags[c] |= 1;
                if (d > 8) flags[c] |= 2;
                if (d >= 4 && d <= 14) ring[c]++;
                uint32_t a = px[j];
                int nb[4] = { x > 0 ? j - 1 : -1, x < 255 ? j + 1 : -1, y > 0 ? j - 256 : -1, y < 191 ? j + 256 : -1 };
                for (int k = 0; k < 4; k++) {
                    int m = nb[k];
                    if (m < 0 || lab[m] >= 0) continue;
                    uint32_t b = px[m];
                    int dr = abs((int)((a >> 16) & 255) - (int)((b >> 16) & 255)), dg = abs((int)((a >> 8) & 255) - (int)((b >> 8) & 255)), db = abs((int)(a & 255) - (int)(b & 255));
                    if (dr <= 24 && dg <= 24 && db <= 24) { lab[m] = c; q[tail++] = m; }
                }
            }
            if (!(flags[c] & 1) && (best < 0 || ring[c] > ring[best])) best = c;
        }
        for (int i = 0; i < N; i++) { int c = lab[i]; bg[i] = c == best || (flags[c] & 1) || !(flags[c] & 2); }
    }
    if (g_shop)                                                   /* a shop: the card's description alone (its parchment box) */
        for (int i = 0; i < N; i++) { int x = i % 256, y = i / 256; if (x < 8 || x > 184 || y < 25 || y > 101) bg[i] = 1; }
    int x0 = 256, y0 = 192, x1 = -1, y1 = -1, kept = 0;
    for (int i = 0; i < N; i++)
        if (!bg[i]) { int x = i % 256, y = i / 256; kept++; if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
    if (kept * (g_shop ? 400 : 50) < N) return;                   /* (nothing much left: not a page with windows) */
    for (int i = 0; i < N; i++) px[i] = (px[i] & 0xFFFFFF) | (bg[i] ? 1u << 24 : 0);
    g_pop_win[0] = x0; g_pop_win[1] = y0; g_pop_win[2] = x1 - x0 + 1; g_pop_win[3] = y1 - y0 + 1;
    if (!g_shop) pop_place();
}

/* A shop's buy list on the field's screen (mapmarks_shop()): its signs
 * become the items' icons, and the screen is laid out as one column
 * (canvas_layout_for()); the row under the list: the first one below
 * y 100 with nothing of the 2D across it (the page number is just above). */
static void shop_update(int a_on_top) {
    int side = 0, ok = top_page_mode() == 1 && !a_on_top && g_comp_has3d && gl_wide_shown(&side) == 1;
    const uint8_t* rgba; int changed;
    g_shop_cut = 118;
    for (int y = 100; ok && y < 140; y++) {
        int n = 0;
        for (int x = 8; x < 248; x++) n += (g_comp_info[(y * 256 + x) * 4] & 3) == 1;
        if (n == 240) { g_shop_cut = y; break; }
    }
    /* (the description's window, canvas_layout_for(), in the list's pixels; while the card shows) */
    int desc_y = g_pop_win[2] ? g_shop_cut + SHOP_GAP + SHOP_PAD + (SHOP_TH - SHOP_DESC_ICON) / 2 : -1;
    g_shop = mapmarks_shop(ok ? g_engine_out[0] : NULL, g_comp_info, desc_y, &rgba, &changed);
    if (changed) gl_set_icons(g_shop ? rgba : NULL, MAPMARKS_W, MAPMARKS_H);
}

/* The map's markers (mapmarks.c) over the top screen, in whichever window
 * shows it whole, while it shows the map (the 3D on the bottom screen, or the
 * map kept during a battle); the mouse over that window points at them. */
static void map_overlay(GlPresent* v, SDL_Window* win, int a_on_top) {
    const float* t = v->top;
    float least = g_minimap ? 48 : 128;                          /* the minimap: small, but it's the map */
    if (t[2] < least || t[3] < least * 0.75f || v->topx[1] != 256 || (a_on_top && !v->held) || (g_minimap && g_mini_vis < 0.99f) || mapmarks_top_page()) return;
    memcpy(v->overlay, t, sizeof v->overlay);
    const char* test = getenv("DQIX_MAPMARKS_MOUSE");            /* tests: "x,y" on the top screen */
    if (test) { float x, y; if (sscanf(test, "%f,%f", &x, &y) == 2) mapmarks_hover(x, y); return; }
    if (SDL_GetMouseFocus() != win) return;
    int ww, wh, dw, dh, mx, my; SDL_GetWindowSize(win, &ww, &wh); SDL_GL_GetDrawableSize(win, &dw, &dh);
    SDL_GetMouseState(&mx, &my);
    float lx = ((float)mx + 0.5f) * (float)dw / (float)ww, ly = ((float)my + 0.5f) * (float)dh / (float)wh;   /* drawable pixels, as v is */
    lx = (lx - v->ox) / v->sc; ly = (ly - v->oy) / v->sc;
    if (lx >= t[0] && lx < t[0] + t[2] && ly >= t[1] && ly < t[1] + t[3]) mapmarks_hover((lx - t[0]) * 256 / t[2], (ly - t[1]) * 192 / t[3]);
}

/* The field's "X Menu" sign (moved to the bottom right, gpu3d.c): its X
 * button drawn as the one the DS's X is on (Game > Controls), on the
 * controller or the keyboard, whichever was used last (hints.c). Where the
 * button is on the sign: in the sign's own pixels, from its top left. */
#define HINT_PX 128
#define SIGN_BTN_X 6.4f
#define SIGN_BTN_Y 23.8f
#define SIGN_BTN_R 4.8f
static void hint_update(GlPresent* v) {
    static uint8_t img[HINT_PX * HINT_PX * 4];
    static char shown[96];
    int side = 0, pad = g_hint_pad < 0 ? g_pad != NULL : g_hint_pad && g_pad;
    if (!g_hints || !g_sign_move || g_frame - g_sign_frame >= 4 || gl_wide_shown(&side) != 1 || v->bot[2] <= 0 || v->botx[1] <= 0) return;
    int style = HINT_KEYBOARD, face = HINT_OTHER, b = pad ? g_ctl_pad[CA_X] : CTL_NONE;
    char lab[64] = "", key[96];
    if (b != CTL_NONE) {
        style = g_ctl_pad_style == CTL_STYLE_PLAYSTATION ? HINT_PLAYSTATION : g_ctl_pad_style == CTL_STYLE_XBOX ? HINT_XBOX :
                g_ctl_pad_style == CTL_STYLE_NINTENDO ? HINT_NINTENDO : HINT_GENERIC;
        face = b == SDL_CONTROLLER_BUTTON_B ? HINT_RIGHT : b == SDL_CONTROLLER_BUTTON_A ? HINT_BOTTOM : b == SDL_CONTROLLER_BUTTON_Y ? HINT_TOP :
               b == SDL_CONTROLLER_BUTTON_X ? HINT_LEFT : HINT_OTHER;
        if (face == HINT_OTHER) controls_pad_name(b, lab, sizeof lab);
    } else controls_key_name(g_ctl_key[CA_X][g_ctl_key[CA_X][0] ? 0 : 1], lab, sizeof lab);
    {   /* tests: DQIX_HINT_STYLE=keyboard|generic|playstation|xbox|nintendo */
        static const char* t = (const char*)-1; if (t == (const char*)-1) t = getenv("DQIX_HINT_STYLE");
        static const char* const names[5] = { "keyboard", "generic", "playstation", "xbox", "nintendo" };
        for (int i = 0; t && i < 5; i++) if (!strcmp(t, names[i])) { style = i; face = i ? HINT_TOP : HINT_OTHER; if (!i) controls_key_name(g_ctl_key[CA_X][0], lab, sizeof lab); }
    }
    snprintf(key, sizeof key, "%d %d %s", style, face, lab);
    if (strcmp(key, shown)) { snprintf(shown, sizeof shown, "%s", key); hint_draw(img, HINT_PX, HINT_PX, style, face, lab); gl_set_layer(GL_LAYER_HINT, img, HINT_PX, HINT_PX); }
    float kx = v->bot[2] / v->botx[1], ky = v->bot[3] / 192.0f, r = SIGN_BTN_R + 0.5f;
    float cx = (float)g_sign_rect[0] + SIGN_BTN_X, cy = (float)g_sign_rect[1] + SIGN_BTN_Y;
    float* q = v->layer[GL_LAYER_HINT];
    q[0] = v->bot[0] + (cx - r - v->botx[0] + (float)side) * kx; q[1] = v->bot[1] + (cy - r) * ky;
    q[2] = 2 * r * kx; q[3] = 2 * r * ky;
}

static void present_gl(void) {
    auto_scale();
    g_wide_side = g_classic ? 0 : wide_side_target();            /* the next 3D frames (only the picture changes) */
    g_sign_move = g_minimap && g_wide_side > 0;                  /* the map's corner: the "X Menu" sign at the bottom right (even with the map hidden) */
    {   /* ...and what the game counts as in view (not in a session: every PC must decide alike) */
        static int off = -1; if (off < 0) off = getenv("DQIX_NO_WIDE_CULL") != NULL;
        g_wide_cull = off || session_requested() ? 0 : g_wide_side;
        extern unsigned g_box_tests, g_box_in, g_box_widened;
        static int lg = -1; if (lg < 0) lg = getenv("DQIX_BOXLOG") != NULL;
        if (lg && g_frame % 60 == 0) fprintf(stderr, "box: frame %llu tests %u in %u widened %u\n", (unsigned long long)g_frame, g_box_tests, g_box_in, g_box_widened);
        if (lg && g_frame % 60 == 0) g_box_tests = g_box_in = g_box_widened = 0;
    }
    uint16_t pow; memcpy(&pow, &g_io[0x304], 2);
    hud_update((pow >> 15) & 1);                                 /* (before composing: the minimal style cuts the cards out for it) */
    gl_set_hud_cut(g_winstyle && (g_hud_n > 0 || g_hud_win[2]));
    shop_update((pow >> 15) & 1);
    pop_update((pow >> 15) & 1);
    gl_set_top_cut(g_pop_win[2] != 0);
    gl_set_split(g_shop);
    g_win_outline = g_hud_win[2] != 0;                           /* (the next frames: the experience and level-up windows outlined) */
    g_dlg_outline = (pow >> 15) & 1;                             /* battles (the 3D on the top screen): the text box too */
    {   /* the 3D gone for a single frame in widescreen (a shop's menu after an item's
         * card): the game's backdrop would flash (pink); the last picture stays */
        static int held, side;
        if (!held && (pow & 1) && gl_wide_shown(&side) >= 0 && !gl_3d_ready()) { held = 1; return; }
        held = 0;
    }
    gl_compose((pow >> 15) & 1, pow & 1);
    wide_update((pow >> 15) & 1);
    gl_dump_hook();
    test_hooks();
    {   /* widescreen: the small screen see-through (O), but solid while the mouse is on it */
        float target = g_minimap ? 0.85f : (float)g_small_opacity / 100.0f;   /* the minimap: nearly solid, to read it */
        int ww, wh, mx, my; SDL_GetWindowSize(g_win, &ww, &wh);
        if (target < 1.0f && SDL_GetMouseFocus() == g_win) {
            GlPresent m; canvas_layout(&m, ww, wh);
            SDL_GetMouseState(&mx, &my);
            float lx = ((float)mx + 0.5f - m.ox) / m.sc, ly = ((float)my + 0.5f - m.oy) / m.sc;
            const float* r = m.front ? m.bot : m.top;
            if (lx >= r[0] && lx < r[0] + r[2] && ly >= r[1] && ly < r[1] + r[3]) target = 1.0f;
        }
        g_small_alpha += (target - g_small_alpha) * 0.2f;
        if (fabsf(target - g_small_alpha) < 0.01f) g_small_alpha = target;
    }
    {   /* the map's markers: drawn when they change */
        const uint8_t* rgba;
        if (mapmarks_update(&rgba)) gl_set_overlay(rgba, MAPMARKS_W, MAPMARKS_H);
    }
    int w, h; SDL_GL_GetDrawableSize(g_win, &w, &h);
    {   /* the in-game menu (Esc) or the journal (J), over everything */
        static int shown;                                        /* 1 the menu, 2 the journal */
        if (gamemenu_is_open()) { int ch; const uint8_t* p = gamemenu_picture(w, h, &ch); if (ch || shown != 1) { gl_set_menu(p, w, h); gl_set_menu_box(gamemenu_box()); } shown = 1; }
        else if (journal_open()) { int ch; const uint8_t* p = journal_picture(w, h, &ch); if (ch || shown != 2) { gl_set_menu(p, w, h); gl_set_menu_box(NULL); } shown = 2; }
        else if (shown) { gl_set_menu(NULL, 0, 0); gl_set_menu_box(NULL); shown = 0; }
    }
    GlPresent v; canvas_layout(&v, w, h);
    hint_update(&v);
    mapmarks_hover(-1, -1);
    map_overlay(&v, g_win, (pow >> 15) & 1);
    gl_present(&v);
    window_shot_hook(w, h, 0);
    SDL_GL_SwapWindow(g_win);
    if (g_win2 && SDL_GL_MakeCurrent(g_win2, g_glctx) == 0) {   /* two windows: the other screen */
        int w2, h2; SDL_GL_GetDrawableSize(g_win2, &w2, &h2);
        GlPresent v2; canvas_layout_for(&v2, w2, h2, 1); v2.again = 1;
        map_overlay(&v2, g_win2, (pow >> 15) & 1);
        gl_present(&v2);
        window_shot_hook(w2, h2, 1);
        SDL_GL_SwapWindow(g_win2);
        SDL_GL_MakeCurrent(g_win, g_glctx);
    }
}

static void present(void) {
    static long long skip = -2;                               /* DQIX_PRESENT_FROM=frame: don't draw before it (tests) */
    if (skip == -2) { const char* e = getenv("DQIX_PRESENT_FROM"); skip = e ? atoll(e) : -1; }
    if (skip >= 0 && g_frame < (uint64_t)skip) return;
    if (g_gl) { present_gl(); return; }
    void* pix; int pitch;
    if (SDL_LockTexture(g_tex, NULL, &pix, &pitch) == 0) {
        for (int s = 0; s < 2; s++)
            for (int y = 0; y < 192; y++)
                memcpy((uint8_t*)pix + (size_t)(s * 192 + y) * pitch, &g_screen[s][y * 256], 256 * 4);
        SDL_UnlockTexture(g_tex);
    }
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_ren);
    {   /* each screen where the layout puts it (stacked or side by side, maybe swapped) */
        GlPresent v; canvas_layout(&v, 1, 1);
        SDL_Rect top = { 0, 0, 256, 192 }, bot = { 0, 192, 256, 192 };
        SDL_Rect dt = { (int)v.top[0], (int)v.top[1], (int)v.top[2], (int)v.top[3] }, db = { (int)v.bot[0], (int)v.bot[1], (int)v.bot[2], (int)v.bot[3] };
        if (dt.w > 0) SDL_RenderCopy(g_ren, g_tex, &top, &dt);
        if (db.w > 0) SDL_RenderCopy(g_ren, g_tex, &bot, &db);
    }
    test_hooks();
    SDL_RenderPresent(g_ren);
}

/* ---------------------------------------------------------------- settings changes (keys and menu) */
static void update_offer(const UpdateInfo* u);
static double g_last_fps;
static void update_title(double fps);
/* a short message in the title bar for two seconds ("Volume 70%"), in the player's language */
static void notice(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); vsnprintf(g_notice, sizeof g_notice, T(fmt), ap); va_end(ap);
    g_notice_until = SDL_GetPerformanceCounter() + g_perf_freq * 2;
    if (g_win) update_title(g_last_fps);
}
static void update_title(double fps) {
    if (fps > 0) g_last_fps = fps;
    { UpdateInfo u; if (update_poll(&u)) update_offer(&u); }
    session_window_close();                                     /* the lobby is over */
    char title[400], ses[300] = "";
    if (session_running()) {
        SessionInfo si; session_info(&si);
        int here = 0; for (int k = 0; k < si.n; k++) here += !si.gone[k];
        if (si.desync) snprintf(ses, sizeof ses, " - %s", T("OUT OF SYNC: leave the session (Wireless menu)"));
        else if (si.state == 4) snprintf(ses, sizeof ses, T(" - session over: %s"), si.message);
        else snprintf(ses, sizeof ses, T(" - together: %d of %d players, delay %d"), here, si.n, si.delay);
        static char last[256]; static uint64_t shown;
        if (strcmp(last, si.message)) { snprintf(last, sizeof last, "%s", si.message); shown = SDL_GetPerformanceCounter(); }
        if (!si.desync && si.state == 3 && SDL_GetPerformanceCounter() - shown < g_perf_freq * 8) snprintf(ses + strlen(ses), sizeof ses - strlen(ses), " - %s", si.message);
    }
    char what[300];                                             /* after "Dragon Quest IX <version> - ", in the player's language */
    if (g_notice[0] && SDL_GetPerformanceCounter() < g_notice_until) snprintf(what, sizeof what, "%s", g_notice);
    else if (g_paused) snprintf(what, sizeof what, T("paused (P to continue)%s"), ses);
    else if (g_farm_on) { int g = farm_gold();
        if (g >= 0) snprintf(what, sizeof what, T("farming gold: %d of %d (Ctrl+G stops)%s"), g, g_farm_target, ses);
        else snprintf(what, sizeof what, T("farming gold (Ctrl+G stops)%s"), ses); }
    else snprintf(what, sizeof what, "%s%s - %.1f fps%s%s", T(g_classic ? "Classic" : "Enhanced"),
                  g_gl && g_opt.lcd ? T(" - DS Lite screen") : "", fps, g_fast ? T(" (fast)") : "", ses);
    snprintf(title, sizeof title, "Dragon Quest IX " DQIX_VERSION " - %s", what);
    SDL_SetWindowTitle(g_win, title);
}

/* the menu bar, in the language loaded (built again when it changes) */
static void menu_build(void) {
    const char* names[N_DRAWN + MAX_SKINS];
    for (int i = 0; i < N_DRAWN; i++) names[i] = SHELLS[i].name;
    for (int i = 0; i < g_nskins; i++) names[N_DRAWN + i] = g_skins[i].name;
    menu_init(g_win, names, N_SHELLS, g_all_options);
}
static void menu_sync(void) {
    {
        SessionInfo si; session_info(&si);
        int in = session_requested();
        menu_enable(MI_SES_START, in && si.host && si.state == 2 && si.n >= 2);
        menu_enable(MI_SES_LEAVE, in);
        menu_enable(MI_SES_HOST, !in); menu_enable(MI_SES_JOIN, !in);
        menu_enable(MI_NET, !in);                            /* a session uses the built-in Wi-Fi only (the PCs must agree) */
    }
    int stacked = !g_side_by_side && !g_single && !((g_corner || g_two) && g_gl) && !(g_wide && WIDE_GL);
    int wide = g_wide && WIDE_GL && !g_side_by_side;
    menu_check(MI_PAUSE, g_paused);
    menu_check(MI_UPDATE_AUTO, g_update_auto);
    menu_check(MI_TURBO, g_turbo);
    menu_check(MI_DLC, g_opt.dlc);
    menu_check(MI_NET, g_net_want);
    menu_check(MI_SRV_BUILTIN, !strcmp(g_wfc_server, "builtin"));
    menu_check(MI_SRV_ALTWFC, !strcmp(g_wfc_server, "altwfc"));
    menu_check(MI_SRV_WIIMMFI, !strcmp(g_wfc_server, "wiimmfi"));
    menu_check(MI_SRV_CUSTOM, wfc_custom()); menu_enable(MI_SRV_CUSTOM, wfc_custom());
    menu_check(MI_CLASSIC, g_classic); menu_check(MI_ENHANCED, !g_classic);
    menu_enable(MI_ENHANCED, g_gl);
    {
        static const int ids[7] = { MI_RES1, MI_RES2, MI_RES3, MI_RES4, MI_RES6, MI_RES8, MI_RES_AUTO }, sc[7] = { 1, 2, 3, 4, 6, 8, 0 };
        for (int i = 0; i < 7; i++) { menu_check(ids[i], sc[i] ? !g_res_auto && g_scale3d == sc[i] : g_res_auto); menu_enable(ids[i], g_gl && !g_classic); }
    }
    menu_check(MI_MINIMAP, g_map_shown); menu_enable(MI_MINIMAP, WIDE_GL);
    menu_check(MI_HUD, g_hud && wide); menu_enable(MI_HUD, g_gl);
    menu_check(MI_MAPMARKS, g_mapmarks); menu_enable(MI_MAPMARKS, g_gl);
    menu_check(MI_MAPMARKS_OPENED, g_mapmarks_opened); menu_enable(MI_MAPMARKS_OPENED, g_gl && g_mapmarks);
    menu_check(MI_MAPMONSTERS, g_mapmarks_monsters); menu_enable(MI_MAPMONSTERS, g_gl && g_mapmarks);
    menu_check(MI_INTERIOR_MAPS, g_interior_maps); menu_enable(MI_INTERIOR_MAPS, g_gl);
    menu_check(MI_MAP_LEGEND, g_map_legend); menu_enable(MI_MAP_LEGEND, g_gl && g_mapmarks);
    menu_check(MI_HINTS, g_hints); menu_enable(MI_HINTS, g_gl);
    menu_check(MI_CONFIRM_BOTTOM, g_ctl_pad[CA_A] == SDL_CONTROLLER_BUTTON_A);
    for (int i = 0; i < 3; i++) menu_check(MI_SKIP0 + i, g_skip_intro == i);
    for (int i = 0; i < 4; i++) { int id = i < 3 ? MI_BSPEED1 + i : MI_BSPEED4; menu_check(id, g_battle_speed == i + 1); menu_enable(id, !session_requested()); }
    menu_check(MI_BSPEED_TURN, g_battle_turn_only); menu_enable(MI_BSPEED_TURN, !session_requested() && g_battle_speed > 1);
    menu_check(MI_BVICTORY, g_battle_auto_victory); menu_enable(MI_BVICTORY, !session_requested());
    menu_check(MI_SAVEBAK, g_save_backups);
    menu_check(MI_AUTOSAVE, g_autosave); menu_enable(MI_AUTOSAVE, !session_requested());
    for (int i = 0; i < cheats_count(); i++) {
        menu_check(MI_CHEAT0 + i, cheats_on(i)); menu_enable(MI_CHEAT0 + i, !session_requested());
        if (cheats_max(i)) menu_set_text(MI_CHEAT0 + i, cheats_name(i));       /* with the chosen number */
    }
    menu_check(MI_SMOOTH, g_smooth); menu_enable(MI_SMOOTH, g_gl && !g_classic);
    menu_check(MI_SMOOTH2D, g_smooth2d); menu_enable(MI_SMOOTH2D, g_gl && !g_classic);
    for (int i = 0; i < 3; i++) { menu_check(MI_OUTLINE0 + i, g_outline == i); menu_enable(MI_OUTLINE0 + i, g_gl && !g_classic); }
    for (int i = 0; i < 3; i++) { menu_check(MI_WINSTYLE0 + i, g_winstyle == i); menu_enable(MI_WINSTYLE0 + i, g_gl && !g_classic); }
    menu_check(MI_TEXUP, g_texup); menu_enable(MI_TEXUP, g_gl && !g_classic);
    menu_check(MI_SOFTSHADOW, g_softshadow); menu_enable(MI_SOFTSHADOW, g_gl && !g_classic);
    menu_check(MI_INKLINES, g_ink); menu_enable(MI_INKLINES, g_gl && !g_classic);
    menu_check(MI_TEXPACK, g_texpack); menu_enable(MI_TEXPACK, g_gl && !g_classic);
    menu_enable(MI_TEXRELOAD, g_gl && !g_classic && g_texpack);
    menu_check(MI_TEXDUMP, g_texdump); menu_enable(MI_TEXDUMP, g_gl && !g_classic);
    menu_check(MI_HDTEXT, g_hdtext); menu_enable(MI_HDTEXT, g_gl && !g_classic);
    menu_check(MI_HDMOVIE, g_hdmovie); menu_enable(MI_HDMOVIE, g_gl && !g_classic);
    for (int i = 0; i < N_FONTS; i++) { menu_check(MI_FONT0 + i, !strcmp(g_hdfont, g_fonts[i])); menu_enable(MI_FONT0 + i, g_gl && !g_classic && g_hdtext); }
    menu_check(MI_LCD, g_opt.lcd); menu_enable(MI_LCD, g_gl);
    int lcd = g_gl && g_opt.lcd;
    menu_check(MI_LCDCOL, g_opt.lcd_color); menu_enable(MI_LCDCOL, lcd);
    menu_check(MI_GRID, g_opt.grid); menu_enable(MI_GRID, lcd);
    menu_check(MI_GHOST, g_opt.ghost); menu_enable(MI_GHOST, lcd);
    for (int i = 0; i < 4; i++) { menu_check(MI_BL1 + i, g_opt.backlight == i + 1); menu_enable(MI_BL1 + i, lcd); }
    menu_check(MI_SHELL, g_opt.shell); menu_enable(MI_SHELL, g_gl && stacked);
    for (int i = 0; i < N_SHELLS; i++) { menu_check(MI_SHELLC0 + i, g_opt.shell_color == i); menu_enable(MI_SHELLC0 + i, g_gl && stacked && g_opt.shell); }
    menu_check(MI_GAP, g_opt.gap || g_opt.shell); menu_enable(MI_GAP, g_gl && stacked && !g_opt.shell);
    menu_check(MI_STACKED, stacked); menu_check(MI_SIDE, g_side_by_side);
    menu_check(MI_CORNER, g_corner && !g_wide && !g_side_by_side); menu_enable(MI_CORNER, g_gl);
    menu_check(MI_WIDE, g_wide && !g_wide_dyn && !g_side_by_side); menu_enable(MI_WIDE, WIDE_GL);
    menu_check(MI_WIDE_DYN, g_wide && g_wide_dyn && !g_side_by_side); menu_enable(MI_WIDE_DYN, WIDE_GL);
    menu_check(MI_SINGLE, wide ? g_small_hide : g_single);
    for (int i = 0; i < 4; i++) { menu_check(MI_CORNER0 + i, g_small_corner == i); menu_enable(MI_CORNER0 + i, g_gl && !g_wide_dyn); }
    for (int i = 0; i < 3; i++) { menu_check(MI_SSIZE0 + i, g_small_size == i); menu_enable(MI_SSIZE0 + i, WIDE_GL && g_map_shown); }
    menu_check(MI_MUTE, g_mute);
    menu_enable(MI_RESET, !session_requested());
    menu_check(MI_TWO, g_two && !g_side_by_side && g_gl); menu_enable(MI_TWO, g_gl);
    menu_check(MI_SWAP, g_swap && !wide); menu_enable(MI_SWAP, !wide);
    for (int i = 0; i < 4; i++) { menu_check(MI_OPACITY1 + i, g_small_opacity == 100 - 25 * i); menu_enable(MI_OPACITY1 + i, g_gl); }
    menu_check(MI_ANALOG, g_analog_walk);
    menu_check(MI_FARM, g_farm_on); menu_check(MI_FARM_FAST, g_farm_fast); menu_enable(MI_FARM, !session_requested());
    for (int i = 0; i < 5; i++) { menu_check(MI_LANG1 + i, g_fw_language == i + 1); menu_enable(MI_LANG1 + i, rom_is_eur()); }
    for (int i = 0; i <= 5; i++) menu_check(MI_UILANG0 + i, g_ui_language == i);
    menu_check(MI_UICONTRAST, g_ui_contrast);
    for (int i = 0; i < 3; i++) menu_check(MI_UISIZE0 + i, g_ui_text_size == i);
    for (int i = 0; i < 4; i++) { menu_check(MI_SIZE1 + i, g_scale == i + 1); menu_enable(MI_SIZE1 + i, !g_opt.actual); }
    menu_check(MI_INTEGER, g_opt.integer); menu_enable(MI_INTEGER, !g_opt.actual);
    menu_check(MI_ACTUAL, g_opt.actual); menu_enable(MI_ACTUAL, g_gl);
    menu_check(MI_FULL, g_fullscreen);
}

static const char* CONTROLS_TEXT =
    "These are the usual keys; Game > Controls changes any of them.\n\n"
    "DS button      Keyboard              Controller\n"
    "D-pad          arrow keys            D-pad / left stick\n"
    "A / B          X / Z                 right / bottom button\n"
    "X / Y          S / A                 top / left button\n"
    "L / R          Q / W                 shoulder buttons\n"
    "               (turn the camera)     or the right stick, while walking\n"
    "START          Enter                 Start\n"
    "SELECT         Right Shift           Back\n"
    "Touch screen   left mouse button on the bottom screen\n\n"
    "Screens: F2 layout   U single screen (left trigger)   F9 swap (right trigger; widescreen: hold to see the other)\n"
    "Widescreen's small screen: K corner (right stick click)   I size   O opacity (left stick click)\n"
    "Tab (hold) fast forward   P pause   Ctrl+R reset (controller: hold Start+Select+L+R)   - / = volume   Ctrl+G farm gold\n"
    "F5 Classic / Enhanced   F4 3D resolution   F6 DS Lite screen   F7 DS Lite console   F8 actual size   F3 window size\n"
    "F11 or Alt+Enter fullscreen (mouse at the top: menu bar)   F12 screenshot   F10 bug report   Esc leave fullscreen / quit\n"
    "In fullscreen, double-click outside the touch screen to go back to the window.";

/* A message box whose long lines are wrapped (the system boxes size themselves
 * to the longest line, which made the help as wide as the screen). */
static int info_box(Uint32 flags, const char* title, const char* text, SDL_Window* win) {
    title = T(title); text = T(text);                           /* in the player's language */
    size_t n = strlen(text);
    char* out = malloc(n * 2 + 1);
    if (!out) return SDL_ShowSimpleMessageBox(flags, title, text, win);
    const int W = 76;
    size_t o = 0; int col = 0, wrapped = 0;
    for (size_t i = 0; i < n;) {
        if (text[i] == '\n') { out[o++] = '\n'; col = 0; i++; continue; }
        size_t j = i; while (j < n && text[j] != ' ' && text[j] != '\n') j++;    /* a word */
        int len = (int)(j - i);
        if (col > 0 && col + len > W) { while (o > 0 && out[o - 1] == ' ') o--; out[o++] = '\n'; col = 0; wrapped = 1; }
        memcpy(out + o, text + i, (size_t)len); o += (size_t)len; col += len; i = j;
        if (len) wrapped = 0;
        while (i < n && text[i] == ' ') { if (!(wrapped && col == 0)) { out[o++] = ' '; col++; } i++; }
    }
    out[o] = 0;
    int r = SDL_ShowSimpleMessageBox(flags, title, out, win);
    free(out);
    return r;
}

static int g_menu_peek;          /* fullscreen: the menu bar is showing because the mouse is at the top */
/* Language > Menus and messages, or the game's language when the menus follow it */
static void ui_language_changed(void) {
    if (ui_language_wanted() == ui_lang()) return;
    ui_lang_load(ui_language_wanted());
    menu_build();
    menu_sync();
    if (g_fullscreen && !g_menu_peek) menu_show(0);
}

static void settings_save(void);

/* ---------------------------------------------------------------- playing with friends far away */
static const char SESSION_HELP[] =
    "Tag mode, visiting other worlds and inviting friends (Pavo) need DS systems close to each other. "
    "This lets friends anywhere play those parts together, up to 4 players.\n\n"
    "How: one of you hosts (Wireless > Play with friends far away > Host), the others join with the code "
    "the host's game shows. When everyone is in, the host presses Start the game. All the games restart together, "
    "each from its own save, and from then on every PC runs every player's DS: yours in the window, your "
    "friends' unseen. Only the buttons travel between the PCs, so the DSes really are side by side - "
    "on every PC at once.\n\n"
    "What to expect:\n"
    "- Save your adventure before hosting or joining: the game restarts.\n"
    "- Everyone needs the same version of this game.\n"
    "- Buttons react a little later than usual (the input delay, a few frames, depending on the distance).\n"
    "- If someone's PC is slow, everyone waits for it while your DSes are talking wirelessly.\n"
    "- The built-in Wi-Fi works as usual (DQVC, the downloads), with the host's files for everyone. Other online servers are off during a session.\n"
    "- Your game saves to your own save as always. Leaving (or closing the game) switches your DS off for the others.\n\n"
    "Getting in: the code works on any internet - it connects through a small relay on the internet when the "
    "host's router doesn't let friends in directly (apartment and mobile internet, for example). Where the router "
    "does, the host's game also shows an address that connects directly, with a little less delay. "
    "Windows may ask whether to allow the game on networks: allow it.";

/* start this program again, in a session or not (the save is written first) */
static void restart_with(const char* session) {
    backup_flush();
    settings_save();
    char pos[48] = "";                                           /* the new window where this one is */
    if (g_win && !g_fullscreen) { int x, y; SDL_GetWindowPosition(g_win, &x, &y); snprintf(pos, sizeof pos, "%d,%d", x, y); }
#ifdef _WIN32
    SetEnvironmentVariableA("DQIX_WINPOS", pos[0] ? pos : NULL);
    SetEnvironmentVariableA("DQIX_SESSION", session);
    SetEnvironmentVariableA("DQIX_PLAYER_NAME", g_player_name[0] ? g_player_name : NULL);
    STARTUPINFOW si; memset(&si, 0, sizeof si); si.cb = sizeof si;
    PROCESS_INFORMATION pi;
    wchar_t* cmd = _wcsdup(GetCommandLineW());
    DWORD flags = GetConsoleWindow() ? 0 : CREATE_NO_WINDOW;   /* started without a console (dqix.exe double-clicked): stay without */
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        exit(0);
    }
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Dragon Quest IX", T("The game couldn't start again by itself. Close it and start it again."), g_win);
#else
    if (pos[0]) setenv("DQIX_WINPOS", pos, 1); else unsetenv("DQIX_WINPOS");
    if (session) setenv("DQIX_SESSION", session, 1); else unsetenv("DQIX_SESSION");
    if (g_player_name[0]) setenv("DQIX_PLAYER_NAME", g_player_name, 1);
    extern char** g_argv;
    execv("/proc/self/exe", g_argv);
#endif
}

static int choose(const char* title, const char* text, const char* const* buttons, int n, int def, int cancel);
static int ask_yes(const char* title, const char* text, const char* yes) {
    const char* b[2] = { yes, "Cancel" };
    return choose(title, text, b, 2, 0, 1) == 0;
}

/* Escape, closing the window and Game > Exit ask first: a stray Escape
 * shouldn't end the game. DQIX_QUIT_NOASK=1 skips the question (tests). */
static void quit_ask(void) {
    static int asking;
    if (asking) return;                                        /* already on screen */
    if (!g_win || (getenv("DQIX_QUIT_NOASK") && atoi(getenv("DQIX_QUIT_NOASK")))) exit(0);
    asking = 1;
    int yes = ask_yes("Exit", "Quit the game?\n\nAnything since you last saved in the game is lost.", "Quit");
    asking = 0;
    if (yes) exit(0);
    g_next_frame = SDL_GetPerformanceCounter();                /* the game waited for the answer */
}

static int ask_name(void) {
    char* t = g_win ? ask_text(g_win, "Your name", "The name your friends see in the session (your hero's name, say):") : NULL;
    if (!t) return g_player_name[0] != 0;
    t[strcspn(t, "\r\n")] = 0;
    if (t[0]) snprintf(g_player_name, sizeof g_player_name, "%s", t);
    free(t);
    settings_save();
    return g_player_name[0] != 0;
}

static void session_action(int id) {
    static const char RESTART[] = "\n\nThe game restarts for this: save your adventure first (in the game) if you haven't.";
    switch (id) {
    case MI_SES_HOST: {
        if (!g_player_name[0] && !ask_name()) return;
        char text[400]; snprintf(text, sizeof text, T("Host a session: your friends join you (up to 3 of them).%s"), T(RESTART));
        if (!ask_yes("Host a session", text, "Host")) return;
        char ses[64]; snprintf(ses, sizeof ses, "host:%d", g_session_port);
        restart_with(ses);
        break;
    }
    case MI_SES_JOIN: {
        if (!g_player_name[0] && !ask_name()) return;
        char* t = g_win ? ask_text(g_win, "Join a friend",
            "The code your friend's game shows (like DQ-7KF2-QM4P), or their address (like 203.0.113.7:27900). "
            "The game restarts and connects: save your adventure first if you haven't.") : NULL;
        if (!t) return;
        char* a = t; while (*a == ' ') a++;
        a[strcspn(a, " \r\n")] = 0;
        if (!a[0]) { free(t); return; }
        char ses[200]; snprintf(ses, sizeof ses, "join:%s", a);
        free(t);
        restart_with(ses);
        break;
    }
    case MI_SES_LEAVE:
        if (!ask_yes("Leave the session", "Leave and play alone? Your DS is switched off for the others; your game restarts "
                     "(it was saved whenever you saved in the game).", "Leave")) return;
        restart_with(NULL);
        break;
    case MI_SES_START: session_start_now(); break;
    case MI_SES_NAME: ask_name(); break;
    case MI_SES_HELP: info_box(SDL_MESSAGEBOX_INFORMATION, "Playing with friends far away", SESSION_HELP, g_win); break;
    }
}

/* ---------------------------------------------------------------- updates (update.c) */
/* ---------------------------------------------------------------- sending a bug report (GitHub)
 * Nothing is uploaded by the game: it packs the report into one .zip, opens a
 * new GitHub issue in the browser with the details filled in, and opens the
 * folder with the zip so the player can drag it in. Only after a clear
 * warning (GitHub is public), with the save files optional. */
static void url_add(char* out, size_t n, const char* s) {
    size_t k = strlen(out);
    for (; *s && k + 4 < n; s++) {
        unsigned char c = (unsigned char)*s;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') out[k++] = (char)c;
        else { snprintf(out + k, n - k, "%%%02X", c); k += 3; }
    }
    out[k] = 0;
}

static void show_in_folder(const char* path) {
#ifdef _WIN32
    char full[MAX_PATH]; if (!GetFullPathNameA(path, sizeof full, full, NULL)) return;
    for (char* p = full; *p; p++) if (*p == '/') *p = '\\';
    char cmd[MAX_PATH + 40]; snprintf(cmd, sizeof cmd, "explorer.exe /select,\"%s\"", full);
    STARTUPINFOA si; memset(&si, 0, sizeof si); si.cb = sizeof si;
    PROCESS_INFORMATION pi;
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
#else
    char dir[600]; snprintf(dir, sizeof dir, "%s", path);
    char* s = strrchr(dir, '/'); if (s) *s = 0;
    char url[700]; snprintf(url, sizeof url, "file://%s", dir);
    SDL_OpenURL(url);
#endif
}

static int choose(const char* title, const char* text, const char* const* buttons, int n, int def, int cancel);
static void report_send(const char* dir, const char* description) {
    static const char* WARN =
        "Send this report to the people working on the game?\n\n"
        "Your browser opens a new bug report on the project's GitHub page, already filled in, and the report's folder opens "
        "with one file in it to drag into that page. Then press Submit. You need a free GitHub account.\n\n"
        "Everything on GitHub is public: anyone can see the report and download its file. The file contains:\n"
        "  - a screenshot of the game, and a picture of the whole game window\n"
        "  - every button you pressed since the game started\n"
        "  - your save file (your characters' names and progress), at the start and now, if ticked below\n"
        "  - your description\n"
        "  - the game's version, log and settings (settings.ini and controls.ini): your graphics card, the Wi-Fi settings and the console's made-up ID; "
        "after playing with friends also their names and the host's internet address\n"
        "  - timing measurements\n\n"
        "Nothing is sent until you press Submit on GitHub.";
    int with_saves = 1;
    const char* b[2] = { "Open GitHub", "Don't send" };
    int r = choice_box(g_win, "Send the bug report", WARN, b, 2, 0, 1, "Include my save file (it makes the problem much easier to find)", &with_saves);
    if (r < 0) {                                                  /* no Windows dialog: three buttons instead of the tick */
        const char* b3[3] = { "Send with my save", "Send without my save", "Don't send" };
        int r3 = choose("Send the bug report", WARN, b3, 3, 0, 2);
        if (r3 == 2) return;
        with_saves = r3 == 0; r = 0;
    }
    if (r != 0) return;
    char zip[400];
    if (!report_zip(dir, with_saves, zip, sizeof zip)) { info_box(SDL_MESSAGEBOX_ERROR, "Send the bug report", "The report couldn't be packed. It is still in the reports folder.", g_win); return; }
    const char* zname = strrchr(zip, '/'); zname = zname ? zname + 1 : zip;
    char title[160] = "Bug report (" DQIX_VERSION ")";             /* on GitHub: stays in English */
    if (description && description[0]) {
        char first[90]; snprintf(first, sizeof first, "%s", description);
        first[strcspn(first, "\r\n")] = 0;
        if (strlen(first) > 70) strcpy(first + 67, "...");
        snprintf(title, sizeof title, "%s (" DQIX_VERSION ")", first);
    }
    static char body[6000];
    snprintf(body, sizeof body,
             "**Version:** %s (%s)\n**Mode:** %s\n**Graphics:** %s\n\n**What happened:**\n%s\n\n"
             "**Report file:** drag `%s` from the folder that just opened into this box.%s\n",
             DQIX_VERSION,
#ifdef _WIN32
             "Windows",
#else
             "Linux",
#endif
             g_classic ? "Classic" : "Enhanced", g_gl ? gl_renderer_name() : "software",
             description && description[0] ? description : "(please describe what you did and what went wrong)",
             zname, with_saves ? "" : " (without the save file)");
    static char url[16000];
    snprintf(url, sizeof url, "https://github.com/" UPDATE_REPO "/issues/new?labels=bug&title=");
    url_add(url, sizeof url, title);
    strncat(url, "&body=", sizeof url - strlen(url) - 1);
    url_add(url, sizeof url, body);
    show_in_folder(zip);
    if (SDL_OpenURL(url) != 0) {
        char m[600]; snprintf(m, sizeof m, T("The browser didn't open. Go to https://github.com/%s/issues/new and attach %s."), UPDATE_REPO, zip);
        info_box(SDL_MESSAGEBOX_INFORMATION, "Send the bug report", m, g_win);
    }
}

/* A message with buttons: Windows' own dialog (buttons sized to their words),
 * SDL's elsewhere. Returns the chosen button's index, or `cancel`. */
static int choose(const char* title, const char* text, const char* const* buttons, int n, int def, int cancel) {
    int r = choice_box(g_win, title, text, buttons, n, def, cancel, NULL, NULL);   /* (translates them itself) */
    if (r >= 0) return r;
    SDL_MessageBoxButtonData b[8];
    if (n > 8) n = 8;
    for (int i = 0; i < n; i++) {
        b[i].flags = (i == def ? SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT : 0) | (i == cancel ? SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT : 0);
        b[i].buttonid = i; b[i].text = T(buttons[i]);
    }
    title = T(title); text = T(text);
    const SDL_MessageBoxData d = { SDL_MESSAGEBOX_INFORMATION, g_win, title, text, n, b, NULL };
    int out = cancel;
    if (SDL_ShowMessageBox(&d, &out) != 0 || out < 0) return cancel;
    return out;
}

static void update_go(const char* branch, const char* version) {
    if (session_requested()) { info_box(SDL_MESSAGEBOX_INFORMATION, "Update", "Leave the session first (Wireless menu), then update.", g_win); return; }
    if (!update_project_dir()) {
        char m[400];
        snprintf(m, sizeof m, T("This copy of the game wasn't made with BUILD.bat, so it can't update itself. "
                 "Download the new version from https://github.com/%s instead."), UPDATE_REPO);
        info_box(SDL_MESSAGEBOX_INFORMATION, "Update", m, g_win);
        return;
    }
    char text[700];
    snprintf(text, sizeof text, T("Update to %s?\n\nThe game closes, downloads %s, builds it from your ROM again (a few minutes; "
             "a window shows how far it is) and starts again. Your saves, settings and extras are kept, and the current "
             "version is kept in the update folder in case you want to go back (update\\rollback.bat)."), version, version);
    { const char* b[2] = { "Update", "Cancel" }; if (choose("Update", text, b, 2, 0, 1) != 0) return; }
    backup_flush();
    settings_save();
    if (g_win) { char t[200]; snprintf(t, sizeof t, "Dragon Quest IX - %s", T("downloading the update...")); SDL_SetWindowTitle(g_win, t); }
    char err[200] = "";
    if (update_install(branch, version, err, sizeof err)) exit(0);
    char m[400]; snprintf(m, sizeof m, T("The update couldn't be downloaded: %s."), err);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, T("Update"), m, g_win);
}

/* at start: a newer stable version */
static void update_offer(const UpdateInfo* u) {
    if (!u->ok || !update_newer(u->stable, DQIX_VERSION) || !strcmp(u->stable, g_update_skip) || session_requested()) return;
    /* the nightly too, when it's newer still: an optional second choice */
    int nightly = u->nightly[0] && strcmp(u->nightly, u->stable) && update_newer(u->nightly, u->stable);
    char text[500];
    snprintf(text, sizeof text, T("Dragon Quest IX %s is available (you have %s).\n\nUpdate now?%s"), u->stable, DQIX_VERSION,
             nightly ? T("\n\nOr try the nightly version: the newest work, but less tested.") : "");
    char upd[64], ngt[64];
    snprintf(upd, sizeof upd, T("Update to %s"), u->stable);
    snprintf(ngt, sizeof ngt, T("Install nightly %s"), u->nightly);
    const char* b[4]; int id[4], nb = 0;
    id[nb] = 1; b[nb++] = upd;
    if (nightly) { id[nb] = 2; b[nb++] = ngt; }
    id[nb] = 3; b[nb++] = "Skip this version";
    id[nb] = 0; b[nb++] = "Later";
    int r = choose("Update available", text, b, nb, 0, nb - 1);
    if (r < 0 || r >= nb) return;
    if (id[r] == 1) update_go("stable", u->stable);
    else if (id[r] == 2) update_go("main", u->nightly);
    else if (id[r] == 3) { snprintf(g_update_skip, sizeof g_update_skip, "%s", u->stable); settings_save(); }
}

/* Help > Check for updates */
static void update_menu(void) {
    if (g_win) { char t[200]; snprintf(t, sizeof t, "Dragon Quest IX - %s", T("checking for updates...")); SDL_SetWindowTitle(g_win, t); }
    UpdateInfo u; update_check_now(&u);
    if (!u.ok) {
        char m[300]; snprintf(m, sizeof m, T("Couldn't check for updates: %s."), u.error[0] ? u.error : T("no answer"));
        info_box(SDL_MESSAGEBOX_INFORMATION, "Updates", m, g_win);
        return;
    }
    const char* tag_s = T(update_newer(u.stable, DQIX_VERSION) ? " (newer)" : !strcmp(u.stable, DQIX_VERSION) ? " (this one)" : " (older)");
    const char* tag_n = T(update_newer(u.nightly, DQIX_VERSION) ? " (newer)" : !strcmp(u.nightly, DQIX_VERSION) ? " (this one)" : " (older)");
    char back[200] = "";
    if (update_newer(DQIX_VERSION, u.stable))
        snprintf(back, sizeof back, T("\n\nInstalling stable goes back to %s, an older version than this one. Your saves and settings are kept."), u.stable);
    char text[900];
    snprintf(text, sizeof text,
             T("This copy: %s\n\nStable: %s%s\nTested; recommended for everyone.\n\nNightly: %s%s\nThe newest work: new features "
               "and fixes first, but less tested.%s\n\nFriends playing together need the same version."),
             DQIX_VERSION, u.stable, tag_s, u.nightly, tag_n, back);
    char bs[64], bn[64];
    snprintf(bs, sizeof bs, T("Install stable %s"), u.stable);
    snprintf(bn, sizeof bn, T("Install nightly %s"), u.nightly);
    const char* b[3]; int id[3], nb = 0;
    if (strcmp(u.stable, DQIX_VERSION)) { id[nb] = 1; b[nb++] = bs; }
    if (strcmp(u.nightly, DQIX_VERSION) && strcmp(u.nightly, u.stable)) { id[nb] = 2; b[nb++] = bn; }
    id[nb] = 0; b[nb++] = "Close";
    int r = choose("Updates", text, b, nb, nb - 1, nb - 1);
    if (r < 0 || r >= nb) return;
    if (id[r] == 1) update_go("stable", u.stable);
    else if (id[r] == 2) update_go("main", u.nightly);
}

/* the lobby: until everyone is in */
static void present(void);
static void do_action(int id);
static void lobby_draw(void) {
    SessionInfo si; session_info(&si);
    static int copied;
    if (si.host && si.ready && !copied) { if (si.reach != 3) SDL_SetClipboardText(si.address); copied = 1; }
    for (int s = 0; s < 2; s++) for (int i = 0; i < 256 * 192; i++) g_screen[s][i] = s ? 0x10203Au : 0x0B1830u;
    uint32_t* top = g_screen[0]; uint32_t* bot = g_screen[1];
    const uint32_t W = 0xFFFFFFu, Y = 0xFFD860u, G = 0xA8B8D8u, R = 0xFF8080u;
    if (!text_ready()) {                                     /* no font: the title bar says it */
        char t[300]; snprintf(t, sizeof t, "Session: %s (Enter = start, Esc = cancel)", si.state == 2 ? (si.host ? si.address : "waiting for the host") : si.message);
        if (g_win) SDL_SetWindowTitle(g_win, t);
        present(); return;
    }
    int y = 6;
    text_draw(top, 256, 192, 8, y, 15, Y, "Playing with friends far away"); y += 22;
    if (si.state == 5) { text_wrap(top, 256, 192, 8, y, 240, 12, R, si.message); text_wrap(bot, 256, 192, 8, 8, 240, 12, G, "Esc: back (the game restarts on its own)."); present(); return; }
    if (si.host) {
        if (!si.ready) { text_draw(top, 256, 192, 8, y, 12, W, "Getting ready for friends..."); y += 18; }
        else {
            text_draw(top, 256, 192, 8, y, 12, G, si.reach == 2 ? "Give your friends this code:" :
                                                  si.reach == 3 ? "Same house only:" : "Give your friends this address:"); y += 15;
            text_draw(top, 256, 192, 12, y, 15, W, si.address); y += 19;
            text_draw(top, 256, 192, 12, y, 10, G, "(copied - paste it into a message)"); y += 15;
            y = text_wrap(top, 256, 192, 8, y, 240, 10, G, si.hint);
        }
    } else {
        text_draw(top, 256, 192, 8, y, 12, G, si.state == 1 ? "Connecting to" : "Connected to"); y += 15;
        text_draw(top, 256, 192, 12, y, 14, W, si.address); y += 20;
        if (si.state == 2) text_wrap(top, 256, 192, 8, y, 240, 12, G, "You're in. Waiting for the host to start the session.");
    }
    int by = 6;
    text_draw(bot, 256, 192, 8, by, 13, Y, "Players"); by += 18;
    for (int k = 0; k < si.n && si.state >= 2; k++) {
        char line[96];
        if (k == si.me) snprintf(line, sizeof line, "%d.  %s  (you)", k + 1, si.names[k]);
        else if (si.host && k > 0) snprintf(line, sizeof line, "%d.  %s  (%d ms)", k + 1, si.names[k], si.ping_ms[k]);
        else snprintf(line, sizeof line, "%d.  %s", k + 1, si.names[k]);
        text_draw(bot, 256, 192, 14, by, 13, W, line); by += 17;
    }
    by = 128;
    if (si.message[0]) by = text_wrap(bot, 256, 192, 8, by, 240, 10, G, si.message);
    text_wrap(bot, 256, 192, 8, 164, 240, 11, Y, si.host ? (si.n >= 2 ? "Enter: start (everyone is here)    Esc: cancel" : "Waiting for friends...    Esc: cancel")
                                                         : "Esc: cancel");
    { static int calls; const char* shot = getenv("DQIX_LOBBY_SHOT");     /* tests: the lobby after a few seconds */
      if (shot && ++calls == 120) save_bmp_screens((const uint32_t(*)[256 * 192])g_screen, shot); }
    present();
}

/* Windows: the same in a window of its own, with buttons */
static void lobby_window(void) {
    SessionInfo si; session_info(&si);
    if (!g_win || si.state == 3 || si.state == 4 || !session_window_open(g_win)) { session_window_close(); return; }
    SessionWindow w; memset(&w, 0, sizeof w);
    w.host = si.host; w.failed = si.state == 5; w.can_start = si.host && si.state == 2 && si.n >= 2;
    if (si.host) {
        snprintf(w.heading, sizeof w.heading, "%s", !si.ready ? "Getting ready for friends..." :
                 si.reach == 2 ? "Give your friends this code (they choose Join a friend and type it):" :
                 si.reach == 3 ? "Only friends in the same house can join (this PC's home address):" :
                                 "Give your friends this address (they choose Join a friend and paste it):");
        if (si.ready) snprintf(w.address, sizeof w.address, "%s", si.address);
        snprintf(w.hint, sizeof w.hint, "%s", si.ready ? si.hint : "");
    } else {
        snprintf(w.heading, sizeof w.heading, "%s", si.state == 1 ? "Connecting to:" : "Connected to:");
        snprintf(w.address, sizeof w.address, "%s", si.address);
        snprintf(w.hint, sizeof w.hint, "%s", si.state == 2 ? "You're in. The game starts when the host presses Start." : "");
    }
    for (int k = 0; k < si.n && si.state >= 2 && k < 4; k++) {
        if (k == si.me) snprintf(w.players[k], sizeof w.players[k], "%d.   %s   (you)", k + 1, si.names[k]);
        else if (si.host && k > 0) snprintf(w.players[k], sizeof w.players[k], "%d.   %s   (ping %d ms)", k + 1, si.names[k], si.ping_ms[k]);
        else snprintf(w.players[k], sizeof w.players[k], "%d.   %s%s", k + 1, si.names[k], k == 0 ? "   (host)" : "");
        w.n_players = k + 1;
    }
    if (si.state == 5) snprintf(w.status, sizeof w.status, "%s", si.message);
    else if (si.host) snprintf(w.status, sizeof w.status, "%s%s%s", si.message, si.message[0] ? "\n" : "",
                               si.n >= 2 ? "When everyone is here, press Start the game." : "Waiting for friends to join (up to 3)...");
    else snprintf(w.status, sizeof w.status, "%s", si.message);
    session_window_update(&w);
    switch (session_window_action()) {
    case 1: if (w.can_start) session_start_now(); break;
    case 2: if (si.state == 5) restart_with(NULL); else if (ask_yes("Cancel", "Stop and play alone? The game restarts.", "Stop")) restart_with(NULL); break;
    }
}

static void lobby_hook(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        int mi = menu_command(&e);
        if (mi) { do_action(mi); continue; }
        if (e.type == SDL_QUIT) exit(0);
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            SDL_Scancode k = e.key.keysym.scancode;
            SessionInfo si; session_info(&si);
            if (k == SDL_SCANCODE_ESCAPE) { if (si.state == 5) restart_with(NULL); else if (ask_yes("Cancel", "Stop and play alone? The game restarts.", "Stop")) restart_with(NULL); }
            if ((k == SDL_SCANCODE_RETURN || k == SDL_SCANCODE_KP_ENTER) && si.host && si.n >= 2) session_start_now();
        }
    }
    menu_sync();
    lobby_draw();
    lobby_window();
    SDL_Delay(30);
}

/* waiting for a friend's DS: keep the window alive */
/* while this console waits for another (sessions): keep the window alive and say why it stands still */
static void idle_pump(void) {
    if (!g_win) return;
    SDL_PumpEvents();
    static uint64_t last;
    uint64_t now = SDL_GetPerformanceCounter();
    if (now - last < g_perf_freq / 2) return;
    last = now;
    double w = air_waiting_ms();
    if (w < 1500) return;
    char d[300], t[400]; int k;
    if (!air_wait_describe(d, sizeof d, &k)) return;
    char* paren = strstr(d, " ("); if (paren) *paren = 0;                /* the numbers are for the log */
    snprintf(t, sizeof t, "Dragon Quest IX - %s (%.0f s) - details in session\\front.log", d, w / 1000.0);
    SDL_SetWindowTitle(g_win, t);
}

static void do_action(int id) {
    int relayout = 0;
    switch (id) {
    case MI_PAUSE: g_paused = !g_paused; break;
    case MI_TURBO: g_turbo = !g_turbo; break;
    case MI_DLC:
        g_opt.dlc = !g_opt.dlc;
        info_box(SDL_MESSAGEBOX_INFORMATION, "Downloadable content", g_opt.dlc ?
            "The 64 downloadable quests and the 23 special guests will be switched on in your save the next "
            "time the game starts (they come with the save, as if downloaded from Nintendo Wi-Fi Connection).\n\n"
            "Close the game and start it again, load your adventure log, and save once in the game to keep them.\n\n"
            "The quests are offered by their quest givers once you reach them in the story; the guests visit "
            "Quester's Rest." :
            "Off. A save that already has them keeps them; this only stops switching them on at start.", g_win);
        break;
    case MI_NET:
        g_net_want = !g_net_want;
        wireless_apply(0);
        if (g_net_want) info_box(SDL_MESSAGEBOX_INFORMATION, "Wireless",
            "Connected mode is on. In the game, talk to Sellma at the DQVC counter in Stornway's Quester's Rest "
            "and the DS will connect through this PC "
            "to the server chosen under Wireless > Server.\n\n"
            "If the connection fails, press F10 right after: the bug report records how far it got.", g_win);
        break;
    case MI_SRV_BUILTIN: case MI_SRV_ALTWFC: case MI_SRV_WIIMMFI:
        snprintf(g_wfc_server, sizeof g_wfc_server, "%s", WFC[id == MI_SRV_BUILTIN ? 0 : id == MI_SRV_ALTWFC ? 1 : 2].key);
        wireless_apply(0);
        break;
    case MI_SRV_CUSTOM: break;
    case MI_NET_HELP:
        info_box(SDL_MESSAGEBOX_INFORMATION, "Wireless",
            "Nintendo Wi-Fi Connection closed in 2014. Fans run replacement servers (AltWFC, Wiimmfi) that a "
            "real DS reaches by changing its DNS setting; this game does the same.\n\n"
            "The PC plays the part of a wireless access point called DQIX-PC and passes the DS's traffic to the "
            "internet. Nothing else on your PC is changed.\n\n"
            "Server > Built-in: nothing goes to the internet. The PC answers the game itself with DQIX's "
            "downloads as Nintendo served them (preserved by fans, in the wfc folder).\n\n"
            "Connect to the internet: the DS gets online when the game asks for it, e.g. when Sellma downloads "
            "DQVC items and new quests. Turn it off and the game behaves like a DS with no access point.\n\n"
            "Unlock all downloadable content now: switches on every downloadable quest and special guest in "
            "your save when the game starts, without visiting Sellma.", g_win);
        break;
    case MI_SES_HOST: case MI_SES_JOIN: case MI_SES_LEAVE: case MI_SES_NAME: case MI_SES_START: case MI_SES_HELP:
        session_action(id);
        break;
    case MI_SHOT: g_shot_req = 1; break;
    case MI_SEND_REPORT:                                          /* Help > Send a bug report: the same as F10 */
    case MI_REPORT: {
        extern char g_report_last_dir[300];
        report_write("F10 pressed");                               /* captured first: the moment you pressed it */
        char rt[100]; snprintf(rt, sizeof rt, "%s " DQIX_VERSION, T("Bug report"));
        char* text = g_win ? ask_text(g_win, rt,
            "The report is saved. Describe what went wrong (what you did, what you expected, what happened) - "
            "as much as you like. Save adds it to the report; Skip keeps the report without a description.") : NULL;
        if (text && g_report_last_dir[0]) {
            char path[400]; snprintf(path, sizeof path, "%s/description.txt", g_report_last_dir);
            FILE* f = fopen(path, "wb");
            if (f) { fputs(text, f); fputs("\n", f); fclose(f); }
        }
        if (g_win && g_report_last_dir[0]) report_send(g_report_last_dir, text);   /* send it? (asks first) */
        free(text);
        if (g_win) { char t[200]; snprintf(t, sizeof t, "Dragon Quest IX - %s", T("report saved (see the reports folder)")); SDL_SetWindowTitle(g_win, t); }
        g_fps_t0 = SDL_GetPerformanceCounter();                    /* keep that title up for a second */
        g_next_frame = SDL_GetPerformanceCounter();                /* the game waited while you typed */
        break;
    }
    case MI_EXIT: quit_ask(); break;
    case MI_CLASSIC: case MI_ENHANCED:
        if (!g_gl) break;
        g_classic = id == MI_CLASSIC; apply_mode(); g_fps_t0 = 0;
        if (g_wide && !g_side_by_side) { relayout = 1; if (g_classic) notice("Widescreen needs Enhanced: stacked until then"); }
        break;
    case MI_RES1: case MI_RES2: case MI_RES3: case MI_RES4: case MI_RES6: case MI_RES8:
        if (!g_gl || g_classic) break;
        g_res_auto = 0;
        g_scale3d = id == MI_RES6 ? 6 : id == MI_RES8 ? 8 : id - MI_RES1 + 1; gl_render_set_scale(g_scale3d);
        break;
    case MI_RES_AUTO: if (g_gl && !g_classic) g_res_auto = 1; break;          /* auto_scale() picks it */
    case MI_SMOOTH: if (g_gl) { g_smooth = !g_smooth; gl_set_smooth(g_smooth); } break;
    case MI_SMOOTH2D: if (g_gl) { g_smooth2d = !g_smooth2d; gl_set_smooth2d(g_smooth2d); } break;
    case MI_OUTLINE0: case MI_OUTLINE0 + 1: case MI_OUTLINE0 + 2: if (g_gl) { g_outline = id - MI_OUTLINE0; gl_set_outline(g_outline); } break;
    case MI_WINSTYLE0: case MI_WINSTYLE0 + 1: case MI_WINSTYLE0 + 2: if (g_gl) g_winstyle = id - MI_WINSTYLE0; break;
    case MI_TEXUP: if (g_gl) { g_texup = !g_texup; gl_set_tex_upscale(g_texup); } break;
    case MI_SOFTSHADOW: if (g_gl) { g_softshadow = !g_softshadow; gl_set_soft_shadows(g_softshadow); } break;
    case MI_INKLINES: if (g_gl) { g_ink = !g_ink; gl_set_ink_lines(g_ink); } break;
    case MI_TEXPACK: if (g_gl) { g_texpack = !g_texpack; gl_set_texture_pack(g_texpack); } break;
    case MI_TEXRELOAD: if (g_gl && g_texpack) gl_reload_texture_pack(); break;
    case MI_TEXDUMP: if (g_gl) { g_texdump = !g_texdump; gl_set_texture_dump(g_texdump); } break;
    case MI_HDTEXT: if (g_gl) { g_hdtext = !g_hdtext; apply_mode(); } break;
    case MI_HDMOVIE: if (g_gl) { g_hdmovie = !g_hdmovie; if (g_hdmovie) hdmovie_init(); apply_mode(); } break;   /* on: look in movies/ again */
    case MI_MINIMAP: g_map_shown = !g_map_shown; break;
    case MI_JOURNAL: journal_ui_toggle(); return;
    case MI_HUD: g_hud = !g_hud; break;
    case MI_MAPMARKS: g_mapmarks = !g_mapmarks; break;
    case MI_MAPMARKS_OPENED: g_mapmarks_opened = !g_mapmarks_opened; break;
    case MI_MAPMONSTERS: g_mapmarks_monsters = !g_mapmarks_monsters; break;
    case MI_INTERIOR_MAPS: g_interior_maps = !g_interior_maps; break;
    case MI_MAP_LEGEND: g_map_legend = !g_map_legend; break;
    case MI_HINTS: g_hints = !g_hints; break;
    case MI_CONFIRM_BOTTOM:                                       /* the controller's A and B (confirm, cancel) trade buttons */
        controls_set_pad(CA_A, g_ctl_pad[CA_A] == SDL_CONTROLLER_BUTTON_A ? SDL_CONTROLLER_BUTTON_B : SDL_CONTROLLER_BUTTON_A);
        if (g_ctl_pad[CA_B] == g_ctl_pad[CA_A] || g_ctl_pad[CA_B] == CTL_NONE)
            g_ctl_pad[CA_B] = g_ctl_pad[CA_A] == SDL_CONTROLLER_BUTTON_A ? SDL_CONTROLLER_BUTTON_B : SDL_CONTROLLER_BUTTON_A;
        controls_save();
        break;
    case MI_SKIP0: case MI_SKIP0 + 1: case MI_SKIP0 + 2: g_skip_intro = id - MI_SKIP0; break;
    case MI_BSPEED1: case MI_BSPEED1 + 1: case MI_BSPEED1 + 2: g_battle_speed = id - MI_BSPEED1 + 1; break;
    case MI_BSPEED4: g_battle_speed = 4; break;
    case MI_BSPEED_TURN: g_battle_turn_only = !g_battle_turn_only; break;
    case MI_BVICTORY: g_battle_auto_victory = !g_battle_auto_victory; break;
    case MI_SAVEBAK: g_save_backups = !g_save_backups; break;
    case MI_AUTOSAVE: g_autosave = !g_autosave; break;
    case MI_FULL2: second_window_fullscreen(); break;
    case MI_LCD: if (g_gl) g_opt.lcd = !g_opt.lcd; g_fps_t0 = 0; break;
    case MI_LCDCOL: g_opt.lcd_color = !g_opt.lcd_color; break;
    case MI_GRID: g_opt.grid = !g_opt.grid; break;
    case MI_GHOST: g_opt.ghost = !g_opt.ghost; break;
    case MI_BL1: case MI_BL2: case MI_BL3: case MI_BL4: g_opt.backlight = id - MI_BL1 + 1; break;
    case MI_SHELL: if (g_gl && !g_side_by_side) { g_opt.shell = !g_opt.shell; relayout = 1; } break;
    case MI_GAP: if (g_gl) { g_opt.gap = !g_opt.gap; relayout = 1; } break;
    case MI_STACKED: case MI_SIDE: g_side_by_side = id == MI_SIDE; g_corner = g_wide = g_two = g_single = 0; relayout = 1; break;
    case MI_CORNER: if (g_gl) { g_corner = !(g_corner && !g_wide && !g_side_by_side && !g_single); g_wide = g_side_by_side = g_two = g_single = 0; relayout = 1; } break;
    case MI_WIDE: case MI_WIDE_DYN:                               /* basic or dynamic widescreen (again: off) */
        if (WIDE_GL) {
            int dyn = id == MI_WIDE_DYN, on = g_wide && !g_side_by_side && g_wide_dyn == dyn;
            g_wide = !on; g_wide_dyn = dyn; g_corner = g_side_by_side = g_two = g_single = 0; relayout = 1;
        }
        break;
    case MI_TWO: if (g_gl) { g_two = !(g_two && !g_side_by_side && !g_single); g_corner = g_wide = g_side_by_side = g_single = 0; relayout = 1; } break;
    case MI_SINGLE:                                               /* U / left trigger: one screen; in widescreen the small one hides */
        if (g_wide && WIDE_GL && !g_side_by_side) { g_small_hide = !g_small_hide; notice(g_small_hide ? "Small screen hidden" : "Small screen shown"); }
        else { g_single = !g_single; relayout = 1; notice(g_single ? "Single screen (%s)" : "Both screens", T(g_swap ? "top" : "bottom")); }
        break;
    case MI_SWAP: if (!(g_wide && WIDE_GL && !g_side_by_side)) { g_swap = !g_swap; relayout = 1; } break;
    case MI_OPACITY1: case MI_OPACITY1 + 1: case MI_OPACITY1 + 2: case MI_OPACITY1 + 3:
        g_small_opacity = 100 - 25 * (id - MI_OPACITY1); notice("Small screen: %d%%", g_small_opacity); break;
    case MI_CORNER0: case MI_CORNER0 + 1: case MI_CORNER0 + 2: case MI_CORNER0 + 3: {
        static const char* const N[4] = { "top left", "top right", "bottom right", "bottom left" };
        g_small_corner = id - MI_CORNER0; notice("Small screen: %s", T(N[g_small_corner])); break;
    }
    case MI_SSIZE0: case MI_SSIZE0 + 1: case MI_SSIZE0 + 2: {
        static const char* const N[3] = { "small", "medium", "large" };
        g_small_size = id - MI_SSIZE0; notice("Small screen: %s", T(N[g_small_size])); break;
    }
    case MI_RESET:
        if (session_requested()) { info_box(SDL_MESSAGEBOX_INFORMATION, "Reset", "Not while playing with friends: it would leave them out of step. Leave the session first (Wireless menu).", g_win); break; }
        if (ask_yes("Reset", "Start the game again from the beginning, like switching the DS off and on?\n\nAnything since you last saved in the game is lost.", "Reset")) restart_with(NULL);
        break;
    case MI_VOLUME:
        if (g_win) volume_window_open(g_win, &g_volume, &g_mute);
#ifndef _WIN32
        notice("Volume %d%% (- and = change it)", g_volume);
#endif
        break;
    case MI_MUTE: g_mute = !g_mute; volume_window_refresh(); notice(g_mute ? "Sound off" : "Volume %d%%", g_volume); break;
    case MI_CONTROLS_EDIT:
#ifdef _WIN32
        if (g_win) controls_window_open(g_win);
#else
        info_box(SDL_MESSAGEBOX_INFORMATION, "Controls", "Change the keys and controller buttons in controls.ini, next to the game (the game writes it with every action listed), then start the game again.", g_win);
#endif
        break;
    case MI_ANALOG: g_analog_walk = !g_analog_walk; break;
    case MI_FARM:
        if (g_farm_on) { farm_cancel("You stopped it."); g_farm_stopped = 0; notice("Farming stopped"); break; }
        if (session_requested()) break;
        {   const char* why = farm_start();
            if (why) info_box(SDL_MESSAGEBOX_INFORMATION, "Farm gold by itself", why, g_win);
            else { g_paused = 0; notice("Farming until the party has %d gold (Ctrl+G or any button stops)", g_farm_target); } }
        break;
    case MI_FARM_TARGET: {
        char prompt[200];
        snprintf(prompt, sizeof prompt, T("Stop farming once the party has this much gold (now %d):"), g_farm_target);
        char* t = g_win ? ask_text(g_win, "Gold target", prompt) : NULL;
        if (!t) break;
        long v = 0; for (char* c = t; *c; c++) if (*c >= '0' && *c <= '9' && v < 100000000) v = v * 10 + (*c - '0');   /* "10 000", "10.000 G" */
        free(t);
        if (v > 0 && v <= 9999999) { g_farm_target = (int)v; notice("Gold target: %d", g_farm_target); }
        break;
    }
    case MI_FARM_FAST: g_farm_fast = !g_farm_fast; break;
    case MI_FARM_HELP:
        info_box(SDL_MESSAGEBOX_INFORMATION, "Farm gold by itself",
            "Go to a place with monsters, stand on the field with nothing open, and choose Farm here now (or press Ctrl+G).\n\n"
            "The hero goes for the monsters walking nearby, staying around the place where you started it (or walks around there until one shows up), and fights every battle by pressing A: Fight, "
            "then each member's first command (Attack), or what their tactics say if you set tactics (Tactics in the battle menu). "
            "Set tactics such as Show no mercy for the whole party and the battles go by themselves.\n\n"
            "It stops, and pauses the game, when the party has the gold you asked for (Gold target), or when someone in the party is "
            "knocked out or low on HP (a wipe-out loses half your gold), when the party leaves the area, when no battle comes for 3 minutes, "
            "or when the game stays in a menu or conversation too long. Ctrl+G, or any button you press, stops it at once.\n\n"
            "With Fast forward while farming on, it runs as fast as your PC can.", g_win);
        break;
    case MI_CHEATS_HELP:
        info_box(SDL_MESSAGEBOX_INFORMATION, "Cheats",
            "Action Replay codes, the ones made for this ROM's version (USA or European): tick one to turn it on, tick it again to turn it off. "
            "They are all off at first, and never run while playing with friends.\n\n"
            "The ones with a number (gold, mini medals, skill points) ask for it when you tick them: "
            "to change it, untick the cheat and tick it again.\n\n"
            "What they change goes into your adventure log when you save (gold, skill points, vocations...): "
            "save to another slot first, or keep Game > Keep a copy of every save on.\n\n"
            "Only the codes that change the game's data are here: on PC the game's code is compiled, so the codes that rewrite "
            "the game's instructions (walk faster, more experience...) can't work.\n\n"
            "The codes are the public ones from Almar's Guides (almarsguides.com, Dragon Quest IX, Action Replay).", g_win);
        break;
    case MI_LANG1: case MI_LANG1 + 1: case MI_LANG1 + 2: case MI_LANG1 + 3: case MI_LANG1 + 4: {
        int lang = id - MI_LANG1 + 1;
        if (lang == g_fw_language) break;
        g_fw_language = lang;
        ui_language_changed();                                  /* the menus follow at once, when they follow the game */
        char text[300];
        snprintf(text, sizeof text, T("The game will be in %s the next time it starts: close it and start it again.\n\n"
                 "Your adventure log is the same in every language."), LANGUAGE_NAME[lang]);
        info_box(SDL_MESSAGEBOX_INFORMATION, T("Language"), text, g_win);
        break;
    }
    case MI_UICONTRAST: g_ui_contrast = !g_ui_contrast; gamemenu_set_readability(g_ui_contrast, g_ui_text_size); break;
    case MI_UISIZE0: case MI_UISIZE0 + 1: case MI_UISIZE0 + 2: g_ui_text_size = id - MI_UISIZE0; gamemenu_set_readability(g_ui_contrast, g_ui_text_size); break;
    case MI_UILANG0: case MI_UILANG0 + 1: case MI_UILANG0 + 2: case MI_UILANG0 + 3: case MI_UILANG0 + 4: case MI_UILANG0 + 5:
        g_ui_language = id - MI_UILANG0;
        ui_language_changed();
        break;
    case MI_MODERN_HELP:
        info_box(SDL_MESSAGEBOX_INFORMATION, "Modern",
            "Optional ways to play that the DS didn't have. Walking with the stick is on at first; the others are off. With them all off the game is exactly the original.\n\n"
            "Walk in any direction with the left stick (on at first): on the DS the D-pad walks in 8 directions and only the stylus walks in any. With this on, "
            "a controller's left stick steers like the stylus while you walk around (the game's own stylus walking). In menus, dialogue and "
            "battles the stick stays a D-pad. Friends in a session don't need it on.\n\n"
            "Widescreen: the action (the field, or a battle) is drawn wider than the DS's screen, so you see more of the world left and right, "
            "and the other screen (the map, or the battle commands) sits small in a corner over it. K (or the right stick click) moves it to "
            "the next corner, I changes its size, O (or the left stick click) how see-through it is; it turns solid while the mouse is on it. "
            "U (or the left trigger) hides it; holding F9 (or the right trigger) shows it big until you let go. In a window it is 16:9; in "
            "fullscreen it fills the monitor. Text, menus and the touch screen's buttons stay in the middle, as on the DS. People and things "
            "at the sides stay in view (except while playing with friends far away). Enhanced mode only (the 3D on the graphics card).\n\n"
            "Widescreen, dynamic (experimental): the small screen stays top left, and the field slides over to make room when a menu "
            "would be under it.\n\n"
            "Top screen in the corner: the field as on the DS (4:3), the top screen beside it.\n\n"
            "Two windows: one screen in each, to put them on two monitors (each can be fullscreen).\n\n"
            "Single screen (U or the left trigger): only one screen, as big as it gets; F9 or the right trigger shows the other one.\n\n"
            "Battle speed: battles (and their victory messages) go 2, 3 or 4 times as fast; the music keeps its pitch and skips ahead. "
            "The rest of the game stays at its own speed. Off while playing with friends far away. Only while the turn plays out: "
            "the attacks, spells and messages go faster, the commands are chosen at the usual pace. Victory messages go by themselves: "
            "after a win, the messages turn their page by themselves (one every 0.8 s) without pressing A, except after a level up (yours to read). "
            "Nothing else changes: the same battles, the same rewards.\n\n"
            "Map: show the monsters walking nearby: red dots on the map, to go for them or around them.\n\n"
            "Save by itself every 10 minutes: the game saves as the priest would, wherever the party is, once the hero is free to walk "
            "(never during a battle, a dialogue, a menu or a cut scene); Continue takes you back there. The 10 minutes count from the last "
            "save, the church's too. It needs a save made once at a church first, and is off while playing with friends. "
            "Game > Keep a copy of every save keeps the earlier ones in save-backups.\n\n"
            "All are also under View; F2 goes through the layouts.", g_win);
        break;
    case MI_SIZE1: case MI_SIZE2: case MI_SIZE3: case MI_SIZE4: g_scale = id - MI_SIZE1 + 1; relayout = 1; break;
    case MI_INTEGER: g_opt.integer = !g_opt.integer; break;
    case MI_ACTUAL:
        if (!g_gl) break;
        g_opt.actual = !g_opt.actual; relayout = 1;
        update_actual();
        if (g_opt.actual && g_actual_sc <= 0)
            info_box(SDL_MESSAGEBOX_INFORMATION, "Actual size",
                "Your monitor didn't say how big it is, so the real size can't be worked out.\n\n"
                "Measure the width of its picture area in millimetres and put it in settings.ini\n"
                "(next to the game) as monitor_width_mm, e.g. monitor_width_mm = 597", g_win);
        break;
    case MI_FULL:
        g_fullscreen = !g_fullscreen;
        g_menu_peek = 0;
        menu_show(!g_fullscreen);
        SDL_SetWindowFullscreen(g_win, g_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        relayout = !g_fullscreen;
        break;
    case MI_CONTROLS: info_box(SDL_MESSAGEBOX_INFORMATION, "Controls", CONTROLS_TEXT, g_win); break;
    case MI_UPDATE: update_menu(); break;
    case MI_UPDATE_AUTO: g_update_auto = !g_update_auto; settings_save(); break;
    case MI_ABOUT: {
        char text[600];
        snprintf(text, sizeof text, T("Dragon Quest IX for PC, %s (%s), built %s\n\n"
            "Classic draws the game exactly like the DS; Enhanced draws its 3D\n"
            "on the graphics card at up to 4x.\n\n"
            "DS Lite screen colours: Pokefan531's measured DS Lite profile."), DQIX_VERSION, DQIX_VERSION_DATE, __DATE__);
        info_box(SDL_MESSAGEBOX_INFORMATION, "About", text, g_win);
        break;
    }
    default:
        if (id >= MI_FONT0 && id < MI_FONT0 + N_FONTS) { snprintf(g_hdfont, sizeof g_hdfont, "%s", g_fonts[id - MI_FONT0]); hdtext_set_font(g_hdfont); }
        else if (id >= MI_SHELLC0 && id < MI_SHELLC0 + N_SHELLS) { g_opt.shell_color = id - MI_SHELLC0; g_color_chosen = 1; relayout = 1; }
        else if (id >= MI_CHEAT0 && id < MI_CHEAT0 + cheats_count()) {
            int i = id - MI_CHEAT0;
            if (cheats_max(i) && !cheats_on(i)) {           /* turning on one with a number: which number */
                char prompt[200], name[120];
                snprintf(name, sizeof name, "%s", cheats_name(i));
                size_t n = strlen(name); if (n > 3 && !strcmp(name + n - 3, "...")) name[n - 3] = 0;
                snprintf(prompt, sizeof prompt, T("%s\n\nType the number you want, from 0 to %d:"), name, cheats_max(i));
                char* t = g_win ? ask_text(g_win, "Cheats", prompt) : NULL;
                if (!t) return;                              /* Skip: stays off */
                long v = -1; for (char* c = t; *c; c++) if (*c >= '0' && *c <= '9' && v < 100000000) v = (v < 0 ? 0 : v * 10) + (*c - '0');   /* "50 000", "50.000 G" */
                free(t);
                if (v > cheats_max(i)) v = cheats_max(i);
                if (v >= 0) cheats_set_value(i, (int)v);     /* nothing typed: the number it had */
            }
            cheats_set(i, !cheats_on(i));
        }
        else return;
    }
    if (relayout && g_win) {                                   /* a new window size only when one was asked for */
        int sized = (id >= MI_SIZE1 && id <= MI_SIZE4) || id == MI_ACTUAL;
        layout_window_keep(!sized);
        second_window_sync();
    }
    menu_sync();
    if (id != MI_SHOT && id != MI_REPORT && id != MI_SEND_REPORT && id != MI_PAUSE && id != MI_TURBO) settings_save();
}

/* ---------------------------------------------------------------- the in-game menu (gamemenu.h) */
static int g_pad_resync;                                     /* (below: a button just given out) */
static void game_menu_toggle(void) {
    if (!g_gl) { quit_ask(); return; }                       /* (it needs OpenGL; without it Esc asks to quit, as before) */
    if (gamemenu_is_open()) { gamemenu_close(); return; }
    menu_sync();
    gamemenu_set_font(g_hdfont);
    gamemenu_set_readability(g_ui_contrast, g_ui_text_size);
    gamemenu_open();
}

static void game_menu_chose(int id) {
    if (id <= 0) return;                                     /* Resume: closed already */
    do_action(id);
    gamemenu_changed();                                      /* new check marks */
}

static void hotkey(int a);
/* An event while the menu is open: 1 if the menu took it. */
static int game_menu_event(const SDL_Event* e) {
    int id = 0;
    switch (e->type) {
    case SDL_KEYDOWN: {
        if (g_cap_action >= 0) return 0;                     /* Controls is waiting for a key */
        SDL_Scancode k = e->key.keysym.scancode;
        int a = controls_key_action(k, e->key.keysym.mod);
        if (k == SDL_SCANCODE_ESCAPE) { if (!e->key.repeat) gamemenu_close(); }
        else if (k == SDL_SCANCODE_UP || a == CA_UP) id = gamemenu_key(GM_UP);
        else if (k == SDL_SCANCODE_DOWN || a == CA_DOWN) id = gamemenu_key(GM_DOWN);
        else if (k == SDL_SCANCODE_PAGEUP) id = gamemenu_key(GM_PAGEUP);
        else if (k == SDL_SCANCODE_PAGEDOWN) id = gamemenu_key(GM_PAGEDOWN);
        else if (e->key.repeat) break;
        else if (k == SDL_SCANCODE_RETURN || k == SDL_SCANCODE_KP_ENTER || k == SDL_SCANCODE_SPACE || a == CA_A) {
            if (k == SDL_SCANCODE_RETURN && (e->key.keysym.mod & KMOD_ALT)) do_action(MI_FULL);
            else id = gamemenu_key(GM_OK);
        }
        else if (k == SDL_SCANCODE_BACKSPACE || k == SDL_SCANCODE_LEFT || a == CA_B) id = gamemenu_key(GM_BACK);
        else if (a == CA_MENU) gamemenu_close();
        else if (a == CA_FULL || a == CA_SHOT || a == CA_VOLDOWN || a == CA_VOLUP) { hotkey(a); gamemenu_changed(); }
        break;
    }
    case SDL_CONTROLLERBUTTONDOWN: {
        if (g_cap_action >= 0) return 0;
        int b = e->cbutton.button;
        if (b == g_ctl_pad[CA_A]) id = gamemenu_key(GM_OK);
        else if (b == g_ctl_pad[CA_B]) id = gamemenu_key(GM_BACK);
        else if (b == SDL_CONTROLLER_BUTTON_DPAD_UP || b == g_ctl_pad[CA_UP]) id = gamemenu_key(GM_UP);
        else if (b == SDL_CONTROLLER_BUTTON_DPAD_DOWN || b == g_ctl_pad[CA_DOWN]) id = gamemenu_key(GM_DOWN);
        else if (b == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) id = gamemenu_key(GM_PAGEUP);
        else if (b == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) id = gamemenu_key(GM_PAGEDOWN);
        else if (b == g_ctl_pad[CA_MENU] || b == SDL_CONTROLLER_BUTTON_GUIDE || b == SDL_CONTROLLER_BUTTON_START) gamemenu_close();
        g_pad_resync = 1;                                    /* (not a shortcut too, once the menu is closed) */
        break;
    }
    case SDL_MOUSEMOTION: case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEWHEEL: {
        if (!g_win || e->window.windowID != SDL_GetWindowID(g_win)) return 0;
        if (e->type == SDL_MOUSEWHEEL) { gamemenu_wheel(e->wheel.y); break; }
        int ww, wh, dw, dh; SDL_GetWindowSize(g_win, &ww, &wh); SDL_GL_GetDrawableSize(g_win, &dw, &dh);
        int mx = e->type == SDL_MOUSEMOTION ? e->motion.x : e->button.x, my = e->type == SDL_MOUSEMOTION ? e->motion.y : e->button.y;
        int click = e->type != SDL_MOUSEBUTTONDOWN ? 0 : e->button.button == SDL_BUTTON_LEFT ? 1 : e->button.button == SDL_BUTTON_RIGHT ? 2 : -1;
        if (click < 0) break;
        id = gamemenu_mouse(((float)mx + 0.5f) * (float)dw / (float)(ww > 0 ? ww : 1), ((float)my + 0.5f) * (float)dh / (float)(wh > 0 ? wh : 1), click);
        break;
    }
    default: return 0;
    }
    game_menu_chose(id);
    return 1;
}

/* the left stick moves through the menu (repeating while held) */
static void game_menu_stick(void) {
    static int dir; static Uint32 next;
    int v = g_pad ? SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTY) : 0;
    int d = v < -20000 ? -1 : v > 20000 ? 1 : 0;
    Uint32 now = SDL_GetTicks();
    if (!d) { dir = 0; return; }
    if (d != dir) { dir = d; next = now + 350; }
    else if ((Sint32)(now - next) < 0) return;
    else next = now + 110;
    gamemenu_key(d < 0 ? GM_UP : GM_DOWN);
}

/* tests: DQIX_MENU_TEST=frame,keys (U D O B; ',' = save the window as menuN.ppm, also done at the end) */
static void game_menu_test(void) {
    static long long at = -2; static const char* keys;
    if (at == -2) { const char* e = getenv("DQIX_MENU_TEST"); at = e ? atoll(e) : -1; keys = e ? strchr(e, ',') : NULL; }
    if (at < 0 || g_frame != (uint64_t)at || !g_gl) return;
    game_menu_toggle();
    int shot = 0;
    for (const char* c = keys ? keys + 1 : ""; ; c++) {
        if (*c == ',' || !*c) {
            snprintf(g_shot_next, sizeof g_shot_next, "menu%d.ppm", shot++);
            present();
            if (!*c) break;
            continue;
        }
        game_menu_chose(gamemenu_key(*c == 'U' ? GM_UP : *c == 'D' ? GM_DOWN : *c == 'O' ? GM_OK : GM_BACK));
    }
    gamemenu_close();
}

/* ---------------------------------------------------------------- the journal (journal.h) */
static void journal_ui_toggle(void) {
    if (!journal_open() && !g_gl) { notice("The journal needs the graphics card (OpenGL)"); return; }
    if (gamemenu_is_open()) gamemenu_close();
    journal_toggle();
}

/* An event while the journal is open: 1 if it took it. */
static int journal_event(const SDL_Event* e) {
    switch (e->type) {
    case SDL_KEYDOWN: {
        if (g_cap_action >= 0) return 0;
        SDL_Scancode k = e->key.keysym.scancode;
        int a = controls_key_action(k, e->key.keysym.mod);
        if (journal_searching()) {   /* a page with a search: letters, digits, space go to it (SDL_TEXTINPUT), not to the shortcuts */
            if (k == SDL_SCANCODE_BACKSPACE) { journal_typed("\b"); break; }
            if (k == SDL_SCANCODE_SPACE || (k >= SDL_SCANCODE_A && k <= SDL_SCANCODE_0) || (k >= SDL_SCANCODE_MINUS && k <= SDL_SCANCODE_SLASH) || k == SDL_SCANCODE_NONUSBACKSLASH)
                break;
        }
        if (k == SDL_SCANCODE_UP || a == CA_UP) journal_input(JK_UP);
        else if (k == SDL_SCANCODE_DOWN || a == CA_DOWN) journal_input(JK_DOWN);
        else if (k == SDL_SCANCODE_LEFT || a == CA_LEFT) journal_input(JK_LEFT);
        else if (k == SDL_SCANCODE_RIGHT || a == CA_RIGHT) journal_input(JK_RIGHT);
        else if (k == SDL_SCANCODE_PAGEUP) journal_input(JK_PAGE_UP);
        else if (k == SDL_SCANCODE_PAGEDOWN) journal_input(JK_PAGE_DOWN);
        else if (e->key.repeat) break;
        else if (k == SDL_SCANCODE_ESCAPE || k == SDL_SCANCODE_BACKSPACE || a == CA_B || a == CA_JOURNAL) journal_input(JK_CLOSE);
        else if (k == SDL_SCANCODE_TAB || a == CA_R) journal_input((e->key.keysym.mod & KMOD_SHIFT) && k == SDL_SCANCODE_TAB ? JK_TAB_PREV : JK_TAB_NEXT);
        else if (a == CA_L) journal_input(JK_TAB_PREV);
        else if (k == SDL_SCANCODE_H || a == CA_Y) journal_input(JK_MORE);
        else if (k == SDL_SCANCODE_RETURN || k == SDL_SCANCODE_KP_ENTER || k == SDL_SCANCODE_SPACE || a == CA_A) {
            if (k == SDL_SCANCODE_RETURN && (e->key.keysym.mod & KMOD_ALT)) do_action(MI_FULL);
            else journal_input(JK_OK);
        }
        else if (a == CA_FULL || a == CA_SHOT || a == CA_VOLDOWN || a == CA_VOLUP) hotkey(a);
        break;
    }
    case SDL_TEXTINPUT: journal_typed(e->text.text); break;
    case SDL_CONTROLLERBUTTONDOWN: {
        if (g_cap_action >= 0) return 0;
        int b = e->cbutton.button;
        if (b == g_ctl_pad[CA_B] || b == g_ctl_pad[CA_JOURNAL] || b == SDL_CONTROLLER_BUTTON_START) journal_input(JK_CLOSE);
        else if (b == g_ctl_pad[CA_A]) journal_input(JK_OK);
        else if (b == g_ctl_pad[CA_Y]) journal_input(JK_MORE);
        else if (b == SDL_CONTROLLER_BUTTON_DPAD_UP || b == g_ctl_pad[CA_UP]) journal_input(JK_UP);
        else if (b == SDL_CONTROLLER_BUTTON_DPAD_DOWN || b == g_ctl_pad[CA_DOWN]) journal_input(JK_DOWN);
        else if (b == SDL_CONTROLLER_BUTTON_DPAD_LEFT || b == g_ctl_pad[CA_LEFT]) journal_input(JK_LEFT);
        else if (b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT || b == g_ctl_pad[CA_RIGHT]) journal_input(JK_RIGHT);
        else if (b == SDL_CONTROLLER_BUTTON_LEFTSHOULDER || b == g_ctl_pad[CA_L]) journal_input(JK_TAB_PREV);
        else if (b == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER || b == g_ctl_pad[CA_R]) journal_input(JK_TAB_NEXT);
        else if (b == g_ctl_pad[CA_MENU] || b == SDL_CONTROLLER_BUTTON_GUIDE) { journal_input(JK_CLOSE); game_menu_toggle(); }
        g_pad_resync = 1;
        break;
    }
    case SDL_MOUSEWHEEL: journal_input(e->wheel.y > 0 ? JK_UP : JK_DOWN); break;
    case SDL_MOUSEBUTTONDOWN: if (e->button.button == SDL_BUTTON_RIGHT) journal_input(JK_CLOSE); break;
    default: return 0;
    }
    return 1;
}

/* the left stick moves through the journal (repeating while held) */
static void journal_stick(void) {
    static int dir; static Uint32 next;
    int vy = g_pad ? SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTY) : 0, vx = g_pad ? SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX) : 0;
    int d = vy < -20000 ? JK_UP + 1 : vy > 20000 ? JK_DOWN + 1 : vx < -20000 ? JK_LEFT + 1 : vx > 20000 ? JK_RIGHT + 1 : 0;
    Uint32 now = SDL_GetTicks();
    if (!d) { dir = 0; return; }
    if (d != dir) { dir = d; next = now + 350; }
    else if ((Sint32)(now - next) < 0) return;
    else next = now + 110;
    journal_input(d - 1);
}

/* tests: DQIX_JOURNAL_TEST=frame,keys (U D L R, > next tab, < previous tab, H more; ',' = save the window as journalN.ppm, also done at the end) */
static void journal_test(void) {
    static long long at = -2; static const char* keys;
    if (at == -2) { const char* e = getenv("DQIX_JOURNAL_TEST"); at = e ? atoll(e) : -1; keys = e ? strchr(e, ',') : NULL; }
    if (at < 0 || g_frame != (uint64_t)at || !g_gl) return;
    journal_ui_toggle();
    int shot = 0;
    for (const char* c = keys ? keys + 1 : ""; ; c++) {
        if (*c == ',' || !*c) {
            snprintf(g_shot_next, sizeof g_shot_next, "journal%d.ppm", shot++);
            present();
            if (!*c) break;
            continue;
        }
        journal_input(*c == 'U' ? JK_UP : *c == 'D' ? JK_DOWN : *c == 'L' ? JK_LEFT : *c == 'R' ? JK_RIGHT : *c == '>' ? JK_TAB_NEXT :
                      *c == '<' ? JK_TAB_PREV : *c == 'H' ? JK_MORE : *c == 'O' ? JK_OK : JK_PAGE_DOWN);
    }
    journal_input(JK_CLOSE);
}

/* ---------------------------------------------------------------- hotkeys (Game > Controls) */
static int wide_now(void) { return g_wide && WIDE_GL && !g_side_by_side; }
static void hotkey(int a) {
    if (!g_all_options) switch (a) {                             /* their menu items are hidden too (menu_init) */
    case CA_RES3D: case CA_MODE: case CA_LCD: case CA_SHELL: case CA_ACTUAL: return;
    }
    switch (a) {
    case CA_PAUSE: do_action(MI_PAUSE); break;
    case CA_FULL: do_action(MI_FULL); break;
    case CA_MINIMAP: do_action(MI_MINIMAP); notice(g_map_shown ? "Map shown" : "Map hidden"); break;
    case CA_JOURNAL: journal_ui_toggle(); break;
    case CA_WINSIZE: do_action(MI_SIZE1 + g_scale % 4); break;
    case CA_RES3D:
        if (g_gl && !g_classic) {                                /* 1x 2x 3x 4x 6x 8x Auto */
            static const int ids[7] = { MI_RES1, MI_RES2, MI_RES3, MI_RES4, MI_RES6, MI_RES8, MI_RES_AUTO }, sc[7] = { 1, 2, 3, 4, 6, 8, 0 };
            int cur = 6; for (int i = 0; i < 6; i++) if (!g_res_auto && g_scale3d == sc[i]) cur = i;
            do_action(ids[(cur + 1) % 7]);
        }
        break;
    case CA_MODE: if (g_gl) do_action(g_classic ? MI_ENHANCED : MI_CLASSIC); break;
    case CA_LCD: do_action(MI_LCD); break;
    case CA_SHELL: do_action(MI_SHELL); break;
    case CA_ACTUAL: do_action(MI_ACTUAL); break;
    case CA_VOLDOWN: case CA_VOLUP:
        g_volume += a == CA_VOLUP ? 10 : -10;
        g_volume = g_volume < 0 ? 0 : g_volume > 100 ? 100 : g_volume;
        g_mute = 0;
        volume_window_refresh(); notice("Volume %d%%", g_volume); settings_save(); menu_sync();
        break;
    case CA_RESET: do_action(MI_RESET); break;
    case CA_SHOT: do_action(MI_SHOT); break;
    case CA_REPORT: do_action(MI_REPORT); break;
    case CA_MENU: game_menu_toggle(); break;
    }
}

/* Game > Controls: waiting for the key or button the player presses for an action */
static int g_cap_action = -1, g_cap_what;                    /* what: 0 key, 1 second key, 2 controller button */
static int g_pad_resync;                                     /* a button just given out: don't fire it while still held */
static void capture_done(const char* what) {
    if (what) notice("%s: %s", T(g_ctl[g_cap_action].name), T(what)); else notice("Not changed");
    g_cap_action = -1;
    controls_save();
    controls_window_refresh();
}
static void capture_key(SDL_Scancode k, Uint16 mod) {
    if (k == SDL_SCANCODE_ESCAPE) { capture_done(NULL); return; }
    if (g_cap_what == 2) return;                                 /* a controller button is wanted */
    int is_mod = k == SDL_SCANCODE_LCTRL || k == SDL_SCANCODE_RCTRL || k == SDL_SCANCODE_LALT || k == SDL_SCANCODE_RALT ||
                 k == SDL_SCANCODE_LSHIFT || k == SDL_SCANCODE_RSHIFT;
    int key = (int)k;
    if (!is_mod && g_cap_action >= CA_N_DS) key |= (mod & KMOD_CTRL ? CTL_MOD_CTRL : 0) | (mod & KMOD_ALT ? CTL_MOD_ALT : 0) | (mod & KMOD_SHIFT ? CTL_MOD_SHIFT : 0);
    controls_set_key(g_cap_action, g_cap_what, key);
    char n[64]; controls_key_name(key, n, sizeof n);
    capture_done(n);
}
static void capture_pad(int pad) {
    if (g_cap_what != 2) return;
    controls_set_pad(g_cap_action, pad);
    char n[64]; controls_pad_name(pad, n, sizeof n);
    capture_done(n);
}
static void capture_wait(void) {                                /* the game waits meanwhile */
    SDL_RaiseWindow(g_win);
    while (g_cap_action >= 0) {
        snprintf(g_notice, sizeof g_notice, g_cap_what == 2 ? "Press a controller button for: %s (Esc cancels)" : "Press a key for: %s (Esc cancels)", g_ctl[g_cap_action].name);
        g_notice_until = SDL_GetPerformanceCounter() + g_perf_freq;
        update_title(g_last_fps);
        handle_events(); present(); SDL_Delay(15);
    }
    g_next_frame = SDL_GetPerformanceCounter();
    g_pad_resync = 1;
}

static void handle_events(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        int mi = menu_command(&e);
        if (mi) { do_action(mi); gamemenu_changed(); continue; }
        if (gamemenu_is_open() && game_menu_event(&e)) continue;
        if (journal_open() && journal_event(&e)) continue;
        switch (e.type) {
        case SDL_QUIT: exit(0);                                   /* Ctrl+C, the system shutting down */
        case SDL_KEYDOWN: {
            if (e.key.repeat || !g_win) break;
            SDL_Scancode k = e.key.keysym.scancode;
            if (g_cap_action >= 0) { capture_key(k, e.key.keysym.mod); break; }   /* Game > Controls: the new key */
            int in2 = g_win2 && e.key.windowID == SDL_GetWindowID(g_win2);   /* typed in the second window */
            if (k == SDL_SCANCODE_ESCAPE) { if (in2 && g_win2_full) second_window_fullscreen(); else game_menu_toggle(); break; }   /* the in-game menu */
            if (k == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT)) { if (in2) { second_window_fullscreen(); settings_save(); } else do_action(MI_FULL); break; }
            if (k == SDL_SCANCODE_G && (e.key.keysym.mod & KMOD_CTRL)) { do_action(MI_FARM); break; }   /* Modern > Farm gold by itself */
            int a = controls_key_action(k, e.key.keysym.mod);
            { int o = controls_key_owner((int)k, NULL); if (o >= 0 && o < CA_N_DS) g_hint_pad = 0; }   /* a DS button on the keyboard */
            if (a == CA_FULL && in2) { second_window_fullscreen(); settings_save(); break; }
            if (a >= 0) hotkey(a);
            break;
        }
        case SDL_CONTROLLERBUTTONDOWN:
            g_hint_pad = 1;
            if (g_cap_action >= 0) capture_pad(e.cbutton.button);
            break;
        case SDL_CONTROLLERAXISMOTION:
            if (e.caxis.value > 16000 || e.caxis.value < -16000) g_hint_pad = 1;
            if (g_cap_action >= 0 && e.caxis.value > 16000 && (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT))
                capture_pad(e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? CTL_PAD_LT : CTL_PAD_RT);
            break;
        case SDL_MOUSEBUTTONDOWN:
            /* fullscreen: a double click outside the touch screen (the top screen, the
             * console, the edges) goes back to the window; on the touch screen
             * clicks are the stylus */
            if (e.button.button == SDL_BUTTON_LEFT && e.button.clicks == 2) {
                int tx, ty, in2 = g_win2 && e.button.windowID == SDL_GetWindowID(g_win2);
                if (in2 && g_win2_full && !to_touch_in(g_win2, e.button.x, e.button.y, &tx, &ty, 0)) { second_window_fullscreen(); settings_save(); }
                else if (!in2 && g_fullscreen && !to_touch(e.button.x, e.button.y, &tx, &ty, 0)) do_action(MI_FULL);
            }
            break;
        case SDL_MOUSEMOTION:
            /* fullscreen: the menu bar appears while the mouse is at the top edge */
            if (g_win2 && g_win2_full && e.motion.windowID == SDL_GetWindowID(g_win2)) {   /* the second window's bar, the same way */
                static int peek2;
                if (!peek2 && e.motion.y <= 1) { peek2 = 1; menu_show_second(1); }
                else if (peek2 && e.motion.y > 80) { peek2 = 0; menu_show_second(0); }
            }
            if (g_fullscreen && e.motion.windowID == SDL_GetWindowID(g_win)) {
                if (!g_menu_peek && e.motion.y <= 1) { g_menu_peek = 1; menu_show(1); }
                else if (g_menu_peek && e.motion.y > 80) { g_menu_peek = 0; menu_show(0); }
            }
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_CLOSE) {           /* two windows: closing the second one goes back to one */
                if (g_win2 && e.window.windowID == SDL_GetWindowID(g_win2)) do_action(MI_STACKED);
                else quit_ask();
                break;
            }
            if (g_win2 && e.window.windowID == SDL_GetWindowID(g_win2)) {
                if ((e.window.event == SDL_WINDOWEVENT_MOVED || e.window.event == SDL_WINDOWEVENT_RESIZED) && !g_win2_full) second_window_keep_rect();
                break;
            }
            if (e.window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED && g_opt.actual) { update_actual(); if (!g_fullscreen) layout_window(); }
            break;
        case SDL_CONTROLLERDEVICEADDED:
            if (!g_pad) { g_pad = SDL_GameControllerOpen(e.cdevice.which); controls_pad_detect(g_pad); }
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (g_pad && SDL_GameControllerGetJoystick(g_pad) == SDL_JoystickFromInstanceID(e.cdevice.which)) { SDL_GameControllerClose(g_pad); g_pad = NULL;
                for (int i = 0; i < SDL_NumJoysticks() && !g_pad; i++) if (SDL_IsGameController(i)) g_pad = SDL_GameControllerOpen(i);   /* another one still plugged in */
                controls_pad_detect(g_pad);
            }
            break;
        }
    }
}

void frontend_frame(void) {
    if (g_hidden) { sound_discard(); return; }
    uint64_t t_start = SDL_GetPerformanceCounter();
    if (!g_perf_end) g_perf_end = t_start;
    PerfRec* pr = &g_perf[g_perf_pos % PERF_N];
    pr->frame = g_frame; pr->busy = (float)ticks_ms(t_start - g_perf_end);
    pr->gl = (float)g_gl_stats.render_ms; pr->upload = (float)g_gl_stats.upload_ms; pr->uploads = g_gl_stats.uploads; pr->polys = g_gl_stats.polys;
    memset(&g_gl_stats, 0, sizeof g_gl_stats);
    { extern double g_t2d_ms; static double last; pr->d2 = (float)(g_t2d_ms - last); last = g_t2d_ms; }
    pr->audio_ms = (int)(sound_buffered() * 1000 / 32768);
    handle_events();
    if (g_win) {
        const Uint8* ks = SDL_GetKeyboardState(NULL);
        g_fe_keys = key_bits(ks) | pad_bits();
        static int always_fast = -1; if (always_fast < 0) always_fast = getenv("DQIX_FAST") != NULL;   /* tests */
        g_fast = controls_key_held(CA_FAST, ks) || controls_pad_held(CA_FAST, g_pad) || always_fast || g_turbo || (g_farm_on && g_farm_fast) || bootskip_active();
        {   /* the controller's own shortcuts (stick clicks, triggers...): when pressed */
            static uint8_t was[CA_N];
            for (int a = CA_N_DS; a < CA_N; a++) {
                int now = a != CA_FAST && controls_pad_held(a, g_pad);
                if (now && !was[a] && !g_pad_resync) hotkey(a);
                was[a] = (uint8_t)now;
            }
            g_pad_resync = 0;
            if (!wide_now()) g_peek = 0;
            { static int tp = -1; if (tp < 0) tp = getenv("DQIX_TEST_PEEK") != NULL; if (tp && wide_now()) g_peek = 1; }   /* tests */
            /* reset: Start + Select + L + R held for a second on the controller (wherever they are mapped) */
            static int held_for;
            int combo = g_pad && controls_pad_held(CA_START, g_pad) && controls_pad_held(CA_SELECT, g_pad) &&
                        controls_pad_held(CA_L, g_pad) && controls_pad_held(CA_R, g_pad) && !session_requested();
            if (combo) {
                if (++held_for == 1) notice("Resetting... (keep holding Start + Select + L + R)");
                if (held_for >= 60) { held_for = 0; restart_with(NULL); }
            } else if (held_for) { if (held_for < 60) notice("Reset cancelled"); held_for = 0; }
            int ca, cw;                                           /* Game > Controls asked for a new key */
            if (controls_window_request(&ca, &cw) && ca >= 0 && ca < CA_N) { g_cap_action = ca; g_cap_what = cw; capture_wait(); }
        }
        int mx, my;
        Uint32 mb = SDL_GetMouseState(&mx, &my);
        int tx, ty;
        static int dragging;                                  /* the press began on the touch screen */
        int held = (mb & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        g_fe_touch = held && !gamemenu_is_open() && !journal_open() && to_touch_in(SDL_GetMouseFocus(), mx, my, &tx, &ty, dragging);
        dragging = g_fe_touch;
        if (g_fe_touch) { g_fe_tx = tx; g_fe_ty = ty; }
        game_menu_test();
        journal_test();
        if (gamemenu_is_open() || journal_open()) g_fe_keys = 0;              /* the menu has the buttons (in a session the game goes on) */
        uint64_t tp = SDL_GetPerformanceCounter();
        static int shown;                                     /* Modern > Start: the logos run unseen (the window stays as first drawn: black) */
        if (!shown || !bootskip_active()) { present(); shown = 1; }
        pr->present = (float)ticks_ms(SDL_GetPerformanceCounter() - tp);
        if (g_farm_stopped) {                                 /* Modern > Farm gold: it stopped by itself: pause and say why */
            g_farm_stopped = 0;
            g_paused = 1;
            menu_sync();
            info_box(SDL_MESSAGEBOX_INFORMATION, "Farm gold by itself", g_farm_why, g_win);
            g_paused = 0;
        }
        if (g_autosave_done) { g_autosave_done = 0; notice("Saved"); }   /* Modern > Save by itself */
        if (gamemenu_is_open() && !session_requested()) {      /* the in-game menu: the game waits */
            update_title(0);
            while (gamemenu_is_open()) { handle_events(); game_menu_stick(); present(); SDL_Delay(15); }
            g_next_frame = SDL_GetPerformanceCounter();
            g_fps_t0 = 0;
            g_pad_resync = 1;
        }
        if (journal_open() && !session_requested()) {          /* the journal: the game waits */
            while (journal_open()) { handle_events(); journal_stick(); present(); SDL_Delay(15); }
            g_next_frame = SDL_GetPerformanceCounter();
            g_fps_t0 = 0;
            g_pad_resync = 1;
        }
        if (g_paused) {                                       /* Game > Pause: keep the window alive, the game waits */
            update_title(0);
            while (g_paused) { handle_events(); present(); SDL_Delay(15); }
            g_next_frame = SDL_GetPerformanceCounter();
            g_fps_t0 = 0;
        }
    } else g_fe_keys = pad_bits();
    uint64_t t_wait = SDL_GetPerformanceCounter();

    /* pacing */
    uint64_t now = SDL_GetPerformanceCounter();
    uint64_t period = (uint64_t)(g_perf_freq / DS_FPS);
    int battle = party_in_battle();
    { static int was; if (battle != was) { LOG_INFO("battle %s\n", battle ? "begins" : "over"); was = battle; } }
    {   /* ...only while the turn plays out: the usual pace from the moment someone's
         * commands show, faster again once they have been gone a few frames */
        static int gone = 1000;
        int shown = battle && battle_commands_shown();
        if (g_battle_turn_only && shown != (gone == 0)) LOG_INFO("battle: commands %s\n", shown ? "shown" : "gone");
        gone = shown ? 0 : gone + (gone < 1000);
        if (g_battle_turn_only && gone < 8) battle = 0;
    }
    int speed = battle && g_battle_speed > 1 && !g_fast && !session_requested() ? g_battle_speed : 1;   /* Modern > Battle speed */
    sound_set_speed(speed);
    g_battle_pace = speed;
    if (g_fast) {
        sound_discard();
        g_next_frame = now;
    } else {
        /* An even beat of one frame per 1/59.83 s from the PC's precise clock.
         * The sound card's clock drifts slightly from it, so the amount of sound
         * queued (averaged, since the card takes it in ~31 ms chunks) nudges the
         * beat by at most 0.5%: too much queued -> slightly slower, too little ->
         * slightly faster. Pacing by the queue directly released frames in pairs. */
        double adjust = 1.0;
        if (g_audio) {
            static double avg = -1;
            double q = (double)sound_buffered();
            avg = avg < 0 ? q : avg + (q - avg) * 0.02;
            double err = (avg - (double)AUDIO_TARGET) / (double)AUDIO_TARGET;
            adjust = 1.0 + fmax(-0.005, fmin(0.005, err * 0.02));
        }
        g_next_frame += (uint64_t)((double)period * adjust / speed);
        if (now > g_next_frame + period * 6) g_next_frame = now;          /* fell far behind: don't try to catch up */
        else {
            while ((now = SDL_GetPerformanceCounter()) < g_next_frame) {
                uint64_t left_us = (g_next_frame - now) * 1000000 / g_perf_freq;
                if (left_us > 2500) SDL_Delay((Uint32)(left_us / 1000 - 2));   /* sleep, then wait out the last ~2 ms precisely */
            }
        }
    }
    if (!g_audio) sound_discard();
    g_perf_end = SDL_GetPerformanceCounter();
    pr->wait = (float)ticks_ms(g_perf_end - t_wait); pr->fast = (uint8_t)g_fast;
    g_perf_pos++;
    g_fps_frames++;
    if (g_win && now - g_fps_t0 > g_perf_freq) {
        update_title(g_fps_frames * (double)g_perf_freq / (double)(now - g_fps_t0));
        g_fps_frames = 0; g_fps_t0 = now;
    }
}

void frontend_shutdown(void) {}
