/* OpenGL renderer interface (gl_render.c). The SDL frontend creates the GL
 * context, then calls these. */
#pragma once
#include <stdint.h>
#include "gpu3d.h"

/* Pictures drawn over the screens besides the map's and the shop's (gl_set_layer):
 * one per user, in drawing order. Each is placed every frame by GlPresent.layer. */
enum { GL_LAYER_HINT,                                 /* a button's sign over the game's (hints.c) */
       GL_LAYERS };

int gl_render_init(int scale);                        /* 0 on failure (use the software path) */
void gl_render_set_scale(int scale);
#define GL_MAX_SCALE 12                               /* the highest 3D resolution offered */
int gl_render_max_scale(void);                        /* ...that this GPU can hold */
void gl_set_smooth(int on);                           /* smooth textures (filtered) instead of the DS's blocks */
void gl_set_texture_pack(int on);                     /* HD textures: textures/<name>.png drawn instead of the game's */
void gl_set_texture_dump(int on);                     /* write every new texture to textures/dump/ (for making a pack) */
void gl_set_outline(int mode);                         /* Enhanced: characters' outlines: 0 the DS's, 1 thin, 2 none */
void gl_set_smooth2d(int on);                         /* Enhanced: the 2D screens smoothed (xBR) */
void gl_set_tex_upscale(int on);                      /* Enhanced: the 3D textures upscaled (xBR, 4x) */
int gl_reload_texture_pack(void);                     /* look in textures/ again; how many were found */
const Renderer3D* gl_renderer3d(void);
void gl_compose(int a_on_top, int lcd_on);           /* at VBlank: 2D + 3D -> both screens */
/* How to show the screens: canvas (DS pixels) placed in the window at
 * (ox, oy) with sc window pixels per DS pixel. */
typedef struct {
    int win_w, win_h;
    float ox, oy, sc, cw, ch;
    float top[4], bot[4];                             /* screen rects on the canvas: x, y, w, h */
    float topx[2], botx[2];                           /* each screen in the composed picture: x offset, width (DS pixels) */
    int front;                                        /* screens overlapping: 1 = the bottom one in front */
    float alpha;                                      /* ...how solid the one in front is (0 or 1 = solid) */
    int again;                                        /* another window, same frame: don't advance the motion blur */
    int lcd, lcd_color, grid, ghost;                  /* DS Lite screen filter and its parts */
    float backlight;                                  /* 0..1 */
    int shell;                                        /* draw the console around the screens */
    float lid[3], base[3], btn[3], hinge[3];          /* its colours (linear 0..1, as shown) */
    int skin;                                         /* draw the console photo (gl_set_skin) instead */
    float skin_k;                                     /* photo pixels per canvas unit */
    int npiece;                                       /* parts of a screen drawn over the canvas (widescreen battle HUD) */
    float piece[8][10];                               /* ...each: x, y, w, h on the canvas; screen (0 top, 1 bottom; 2, 3: its 2D alone, gl_set_split()); x, y, w, h on it;
                                                       * restyled (1 its dark letters alone, made light; 2 no picture: a window of the minimal style) */
    int held;                                         /* show the picture kept by gl_hold() instead of this frame's */
    float overlay[4];                                 /* where to draw gl_set_overlay()'s picture on the canvas (w 0: not) */
    float icons[4];                                   /* ...and gl_set_icons()' */
    float layer[GL_LAYERS][4];                        /* ...and each gl_set_layer()'s (w 0: not this frame) */
} GlPresent;
#define GL_MAX_PIECES 8
void gl_hold(void);                                   /* keep this frame's picture (the map window during battles) */
void gl_set_skin(const uint8_t* rgba, int w, int h);
void gl_set_menu(const uint8_t* rgba, int w, int h);      /* the in-game menu over the whole window (premultiplied RGBA, rows top-down; NULL: none) */
void gl_set_menu_box(const float* b);                     /* ...its window drawn under it in the minimal style: x0 y0 x1 y1 (window pixels, y down), line width (NULL: none) */
void gl_set_overlay(const uint8_t* rgba, int w, int h);   /* a picture over the screens (premultiplied RGBA, rows top-down; NULL: none) */
void gl_set_icons(const uint8_t* rgba, int w, int h);     /* another (a shop's icons), drawn after it */
void gl_set_layer(int id, const uint8_t* rgba, int w, int h);   /* more pictures (GL_LAYER_...), drawn after those in id order, under the in-game menu; NULL: none */
void gl_present(const GlPresent* v);
uint8_t* gl_read_window(int w, int h);
int gl_wide_shown(int* side);                         /* widescreen: the screen composed wide (0 top, 1 bottom, -1 none) */
int gl_3d_ready(void);                                /* this frame's 3D is there to compose */
void gl_set_hud_cut(int on);                          /* the next gl_compose(): the commands screen's card backdrops see-through (battle HUD, minimal windows) */
void gl_set_split(int on);                            /* the next gl_compose(): the 3D screen (widescreen) drawn without its 2D, which pieces 2/3 show apart */
void gl_set_top_cut(int on);                          /* the next gl_compose(): the top screen's pixels tagged 1 (top byte) see-through (its pop-up) */
int gl_dialog_box(int r[4]);                          /* the dialogue window on the last 3D frame: x0, y0, x1, y1 on the DS's picture; 0 when none */
uint8_t* gl_read_full(int* w, int* h);
void gl_read_screens(uint32_t out[2][256 * 192]);
int gl_read_3d(uint8_t* out, int* w, int* h);

/* Per-frame counters (the frontend reads and clears them). */
typedef struct { double render_ms, upload_ms; int uploads, polys; long upload_texels; } GlStats;
extern GlStats g_gl_stats;
const char* gl_renderer_name(void);
int gl_render_scale(void);
