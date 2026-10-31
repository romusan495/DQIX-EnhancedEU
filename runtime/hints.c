/* A button's sign as the player's own controller or keyboard shows it
 * (hints.h). Shapes are drawn from their distance to each pixel, smoothed
 * over one pixel; letters with the system font (text.c); a keyboard key in
 * the game's own pixels. */
#include "hints.h"
#include "text.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "uitext.h"

static uint8_t* g_px; static int g_w, g_h;
static float g_k;                                                /* pixels per unit: the picture is 2 units across */

/* paint colour rgb over pixel (x, y) with coverage a */
static void put(int x, int y, uint32_t rgb, float a) {
    if (a <= 0) return;
    if (a > 1) a = 1;
    uint8_t* p = &g_px[((size_t)y * g_w + x) * 4];
    float k = 1 - a;
    p[0] = (uint8_t)(((rgb >> 16) & 255) * a + p[0] * k); p[1] = (uint8_t)(((rgb >> 8) & 255) * a + p[1] * k);
    p[2] = (uint8_t)((rgb & 255) * a + p[2] * k); p[3] = (uint8_t)(255 * a + p[3] * k);
}

static float seg(float px, float py, float ax, float ay, float bx, float by) {   /* distance to a segment */
    float dx = bx - ax, dy = by - ay, t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy);
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return hypotf(px - ax - t * dx, py - ay - t * dy);
}

enum { S_DISK, S_RING, S_TRI, S_CROSS, S_SQUARE, S_RBOX };
/* one shape (units: the picture from -1 to 1 across, y down), its distance at (x, y): < 0 inside */
static float dist(int s, const float* a, float x, float y) {
    switch (s) {
    case S_DISK: return hypotf(x - a[0], y - a[1]) - a[2];
    case S_RING: return fabsf(hypotf(x - a[0], y - a[1]) - a[2]) - a[3];
    case S_TRI: {                                                /* an upright triangle's outline: centre, radius, half thickness */
        float r = a[2], d = 1e9f, v[3][2];
        for (int i = 0; i < 3; i++) { float t = -1.5707963f + (float)i * 2.0943951f; v[i][0] = a[0] + r * cosf(t); v[i][1] = a[1] + r * 0.92f * sinf(t) + r * 0.12f; }
        for (int i = 0; i < 3; i++) { float e = seg(x, y, v[i][0], v[i][1], v[(i + 1) % 3][0], v[(i + 1) % 3][1]); d = e < d ? e : d; }
        return d - a[3];
    }
    case S_CROSS: {
        float r = a[2], d1 = seg(x, y, a[0] - r, a[1] - r, a[0] + r, a[1] + r), d2 = seg(x, y, a[0] - r, a[1] + r, a[0] + r, a[1] - r);
        return (d1 < d2 ? d1 : d2) - a[3];
    }
    case S_SQUARE: return fabsf(fmaxf(fabsf(x - a[0]), fabsf(y - a[1])) - a[2]) - a[3];
    default: {                                                   /* a rounded box: centre, half sizes, corner radius */
        float dx = fabsf(x - a[0]) - (a[2] - a[4]), dy = fabsf(y - a[1]) - (a[3] - a[4]);
        float ox = dx > 0 ? dx : 0, oy = dy > 0 ? dy : 0;
        return hypotf(ox, oy) + fminf(fmaxf(dx, dy), 0) - a[4];
    }
    }
}

static void shape(int s, uint32_t rgb, float alpha, float a0, float a1, float a2, float a3, float a4) {
    const float a[5] = { a0, a1, a2, a3, a4 };
    for (int y = 0; y < g_h; y++)
        for (int x = 0; x < g_w; x++) {
            float ux = ((float)x + 0.5f) / g_k - (float)g_w / g_k / 2, uy = ((float)y + 0.5f) / g_k - (float)g_h / g_k / 2;
            put(x, y, rgb, alpha * (0.5f - dist(s, a, ux, uy) * g_k));
        }
}

/* the label, centred at (cx, cy) units, at most max_w units wide */
static void label(const char* s, float cx, float cy, float size, float max_w, uint32_t rgb) {
    if (!s || !*s || !text_ready()) return;
    uint32_t* t = calloc((size_t)g_w * g_h, 4);
    if (!t) return;
    float px = size * g_k;
    int tw = text_draw(t, g_w, g_h, 0, 0, px, 0xFFFFFF, s);
    if (tw > max_w * g_k && tw > 0) { px *= max_w * g_k / (float)tw; memset(t, 0, (size_t)g_w * g_h * 4); tw = text_draw(t, g_w, g_h, 0, 0, px, 0xFFFFFF, s); }
    int y0 = g_h, y1 = -1;                                       /* the letters' own height (no room left for accents) */
    for (int y = 0; y < g_h; y++) for (int x = 0; x < tw && x < g_w; x++) if (t[y * g_w + x] >> 16 > 60) { if (y < y0) y0 = y; if (y > y1) y1 = y; }
    if (y1 >= y0) {
        int ox = (int)lroundf((cx + (float)g_w / g_k / 2) * g_k - (float)tw / 2), oy = (int)lroundf((cy + (float)g_h / g_k / 2) * g_k - (float)(y0 + y1 + 1) / 2);
        for (int y = 0; y < g_h; y++)
            for (int x = 0; x < g_w; x++) {
                int sx = x - ox, sy = y - oy;
                if (sx < 0 || sy < 0 || sx >= g_w || sy >= g_h) continue;
                put(x, y, rgb, (float)(t[sy * g_w + sx] >> 16) / 255.0f);
            }
    }
    free(t);
}

/* ------------------------------------------------------- keyboard keys */
/* A keyboard key drawn like the game's own button on the "X Menu" sign, in its
 * pixels: the sign's texture has a 9 x 9 button (four blues: a lighter rim at
 * the top, darker at the bottom) with a 5 x 5 white X outlined in black; the
 * sign's "Menu" is 1-pixel strokes, white outlined in black. Here the same
 * plate, as wide as the key's name needs, and the name in capitals 5 pixels
 * high in the same style (X is the game's own X). Drawn on the DS's pixels,
 * then each one made a square block of the picture. */
enum { K_TEXT, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_ENTER, K_BACK, K_TAB, K_SHIFT, K_CAPS, K_SPACE, K_HOME };
static const struct { const char* sdl; int sym; const char* text; } KEYS[] = {   /* SDL's scancode names */
    { "Up", K_UP, 0 }, { "Down", K_DOWN, 0 }, { "Left", K_LEFT, 0 }, { "Right", K_RIGHT, 0 },
    { "Return", K_ENTER, 0 }, { "Keypad Enter", K_ENTER, 0 }, { "Return2", K_ENTER, 0 },
    { "Backspace", K_BACK, 0 }, { "Tab", K_TAB, 0 }, { "Left Shift", K_SHIFT, 0 }, { "Right Shift", K_SHIFT, 0 },
    { "CapsLock", K_CAPS, 0 }, { "Space", K_SPACE, 0 }, { "Home", K_HOME, 0 },
    { "Escape", K_TEXT, "Esc" }, { "Delete", K_TEXT, "Del" }, { "Insert", K_TEXT, "Ins" },
    { "End", K_TEXT, "End" }, { "PageUp", K_TEXT, "PgUp" }, { "PageDown", K_TEXT, "PgDn" },
    { "Left Ctrl", K_TEXT, "Ctrl" }, { "Right Ctrl", K_TEXT, "Ctrl" }, { "Left Alt", K_TEXT, "Alt" }, { "Right Alt", K_TEXT, "AltGr" },
    { "PrintScreen", K_TEXT, "PrtSc" }, { "ScrollLock", K_TEXT, "ScrLk" }, { "Pause", K_TEXT, "Pause" },
    { "Numlock", K_TEXT, "NumLk" }, { "Application", K_TEXT, "Menu" },
};

static int utf8_next(const char** s) {
    const unsigned char* p = (const unsigned char*)*s;
    int c = *p++;
    if (c >= 0xC0 && c < 0xE0 && (*p & 0xC0) == 0x80) c = ((c & 0x1F) << 6) | (*p++ & 0x3F);
    else if (c >= 0xE0 && c < 0xF0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80) { c = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F); p += 2; }
    *s = (const char*)p;
    return c;
}

/* what the key shows: its symbol, or its text (*text); *keypad for the keypad's keys */
static int key_sign(const char* lab, const char** text, int* keypad) {
    *text = lab; *keypad = 0;
    if (!lab || !*lab) return K_TEXT;
    if (!strncmp(lab, "Keypad ", 7) && strcmp(lab, "Keypad Enter")) { *text = lab + 7; *keypad = 1; return K_TEXT; }
    for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++)
        if (!strcmp(lab, KEYS[i].sdl)) { if (KEYS[i].text) *text = T(KEYS[i].text); return KEYS[i].sym; }   /* "Esc" -> "Échap" (lang/<language>.txt) */
    return K_TEXT;
}

/* ---- the pixels: rows of '#' (white), 5 high; a 6th row first ('^': an
 * accent, over the letter) or last (',': a cedilla, under it) */
static const struct { int c; const char* rows; } GLYPHS[] = {
    { 'A', ".##.|#..#|####|#..#|#..#" }, { 'B', "###.|#..#|###.|#..#|###." }, { 'C', ".###|#...|#...|#...|.###" },
    { 'D', "###.|#..#|#..#|#..#|###." }, { 'E', "####|#...|###.|#...|####" }, { 'F', "####|#...|###.|#...|#..." },
    { 'G', ".###|#...|#.##|#..#|.###" }, { 'H', "#..#|#..#|####|#..#|#..#" }, { 'I', "###|.#.|.#.|.#.|###" },
    { 'J', "..##|...#|...#|#..#|.##." }, { 'K', "#..#|#.#.|##..|#.#.|#..#" }, { 'L', "#...|#...|#...|#...|####" },
    { 'M', "#...#|##.##|#.#.#|#...#|#...#" }, { 'N', "#..#|##.#|#.##|#..#|#..#" }, { 'O', ".##.|#..#|#..#|#..#|.##." },
    { 'P', "###.|#..#|###.|#...|#..." }, { 'Q', ".##.|#..#|#..#|#.#.|.#.#" }, { 'R', "###.|#..#|###.|#.#.|#..#" },
    { 'S', ".##.|#...|.##.|...#|.##." }, { 'T', "###|.#.|.#.|.#.|.#." }, { 'U', "#..#|#..#|#..#|#..#|.##." },
    { 'V', "#...#|#...#|.#.#.|.#.#.|..#.." }, { 'W', "#...#|#...#|#.#.#|##.##|#...#" },
    { 'X', "##.##|.###.|..#..|.###.|##.##" },                       /* the game's own X */
    { 'Y', "#...#|.#.#.|..#..|..#..|..#.." }, { 'Z', "####|...#|.##.|#...|####" },
    { '0', ".##.|#..#|#..#|#..#|.##." }, { '1', ".#.|##.|.#.|.#.|###" }, { '2', "###.|...#|.##.|#...|####" },
    { '3', "###.|...#|.##.|...#|###." }, { '4', "#..#|#..#|####|...#|...#" }, { '5', "####|#...|####|...#|###." },
    { '6', ".##.|#...|###.|#..#|.##." }, { '7', "####|...#|..#.|.#..|.#.." }, { '8', ".##.|#..#|.##.|#..#|.##." },
    { '9', ".##.|#..#|.###|...#|.##." },
    { ';', "..|.#|..|.#|#." }, { ',', "..|..|..|.#|#." }, { '.', ".|.|.|.|#" }, { ':', ".|#|.|#|." },
    { '/', "..#|..#|.#.|#..|#.." }, { '\\', "#..|#..|.#.|..#|..#" }, { '-', "...|...|###|...|..." },
    { '=', "...|###|...|###|..." }, { '+', "...|.#.|###|.#.|..." }, { '\'', "#|#|.|.|." }, { '`', "#.|.#|..|..|.." },
    { '[', "##|#.|#.|#.|##" }, { ']', "##|.#|.#|.#|##" }, { '(', ".#|#.|#.|#.|.#" }, { ')', "#.|.#|.#|.#|#." },
    { '"', "#.#|#.#|...|...|..." }, { '!', "#|#|#|.|#" }, { '?', "##.|..#|.#.|...|.#." }, { '^', ".#.|#.#|...|...|..." },
    { '*', "#.#|.#.|#.#|...|..." }, { '<', "..#|.#.|#..|.#.|..#" }, { '>', "#..|.#.|..#|.#.|#.." },
    { '&', ".#..|#.#.|.#.#|#.#.|.#.#" }, { '$', ".###|##..|.##.|..##|###." }, { '#', ".#.#|####|.#.#|####|.#.#" },
    { '%', "#..#|..#.|.#..|#...|#..#" }, { 0xB2, "##.|..#|.#.|###|..." },          /* ² */
    { 0xA7, ".###|#.#.|.#.#|..#.|###." },                          /* § */
    { 0xB5, "#..#|#..#|#..#|###.|#..." },                          /* µ */
    { 0xDF, ".##.|#..#|#.#.|#..#|#.#." },                          /* ß */
    { 0x20AC, ".###|#...|###.|#...|.###" },                        /* € */
};

/* accented capitals: the letter and its accent's row ('^' over it, ',' under it) */
static int glyph_accent(int c, int* base, const char** acc) {
    static const struct { int c, base; const char* acc; } ACC[] = {
        { 0xC0, 'A', "^.#.." }, { 0xC1, 'A', "^..#." }, { 0xC2, 'A', "^.##." }, { 0xC4, 'A', "^#..#" },
        { 0xC7, 'C', ",.#.." }, { 0xC8, 'E', "^.#.." }, { 0xC9, 'E', "^..#." }, { 0xCA, 'E', "^.##." }, { 0xCB, 'E', "^#..#" },
        { 0xCC, 'I', "^#.." }, { 0xCD, 'I', "^..#" }, { 0xCE, 'I', "^.#." }, { 0xCF, 'I', "^#.#" },
        { 0xD1, 'N', "^.##." }, { 0xD2, 'O', "^.#.." }, { 0xD3, 'O', "^..#." }, { 0xD4, 'O', "^.##." }, { 0xD6, 'O', "^#..#" },
        { 0xD9, 'U', "^.#.." }, { 0xDA, 'U', "^..#." }, { 0xDB, 'U', "^.##." }, { 0xDC, 'U', "^#..#" },
    };
    for (size_t i = 0; i < sizeof ACC / sizeof ACC[0]; i++) if (ACC[i].c == c) { *base = ACC[i].base; *acc = ACC[i].acc; return 1; }
    return 0;
}

static int upper(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32;       /* Latin-1: é -> É */
    return c;
}

static const char* glyph(int c) {
    for (size_t i = 0; i < sizeof GLYPHS / sizeof GLYPHS[0]; i++) if (GLYPHS[i].c == c) return GLYPHS[i].rows;
    return NULL;
}
static int glyph_w(const char* g) { int w = 0; while (g[w] && g[w] != '|') w++; return w; }

/* the symbols, 5 high */
static const char* const SYMBOLS[] = {
    [K_UP] = ".....|..#..|.###.|#####|.....", [K_DOWN] = ".....|#####|.###.|..#..|.....",
    [K_LEFT] = "..#|.##|###|.##|..#", [K_RIGHT] = "#..|##.|###|##.|#..",
    [K_ENTER] = "...#|...#|.#.#|####|.#..", [K_BACK] = "..#...|.##...|######|.##...|..#...",
    [K_TAB] = "..#..#|...#.#|######|...#.#|..#..#", [K_SHIFT] = "..#..|.#.#.|##.##|.#.#.|.###.",
    [K_CAPS] = "..#..|.###.|#####|.....|.###.", [K_SPACE] = ".......|.......|#.....#|#.....#|#######",
    [K_HOME] = "####.|##...|#.#..|#..#.|....#",
};

#define KEY_H 9                                                   /* the game's button: 9 DS pixels high */
#define KEY_MAXW 64
/* the key's sign into a grid of KEY_MAXW x KEY_H (1: white), from column x0; returns its width */
static int key_ink(const char* lab, uint8_t* ink, int x0) {
    const char* t; int kp, s = key_sign(lab, &t, &kp), x = x0;
    if (s != K_TEXT) {
        const char* g = SYMBOLS[s];
        for (int r = 0, i = 0; ; r++, i++) {
            for (int c = 0; g[i] && g[i] != '|'; c++, i++) if (g[i] == '#' && ink) ink[(2 + r) * KEY_MAXW + x0 + c] = 1;
            if (!g[i]) break;
        }
        return glyph_w(g);
    }
    char buf[64];
    if (kp) { snprintf(buf, sizeof buf, "%s %s", T("num"), t); t = buf; }   /* keypad: "NUM 5" */
    int first = 1;
    while (*t) {
        int c = upper(utf8_next(&t)), base = c;
        if (c == ' ') { x += 3; first = 1; continue; }
        const char* acc = NULL;
        if (glyph_accent(c, &base, &acc)) c = base;
        const char* g = glyph(c);
        if (!g) g = glyph('?');
        int w = glyph_w(g);
        if (!first) x++;                                          /* a column between letters (their outlines meet there) */
        first = 0;
        if (x + w > KEY_MAXW - 2) break;
        for (int r = 0, i = 0; ; r++, i++) {
            for (int k = 0; g[i] && g[i] != '|'; k++, i++) if (g[i] == '#' && ink) ink[(2 + r) * KEY_MAXW + x + k] = 1;
            if (!g[i]) break;
        }
        if (acc && ink) {                                         /* over it (row 1) or under it (row 7), centred */
            int aw = (int)strlen(acc + 1), ax = x + (w - aw + 1) / 2, row = acc[0] == '^' ? 1 : 7;
            for (int k = 0; k < aw; k++) if (acc[1 + k] == '#') ink[row * KEY_MAXW + ax + k] = 1;
        }
        x += w;
    }
    return x - x0;
}

/* the key's width in DS pixels: its sign, its outline and the plate's rims (at least the game's 9) */
static int key_cols(const char* lab) {
    int w = key_ink(lab, NULL, 0) + 4;
    return w < KEY_H ? KEY_H : w > KEY_MAXW ? KEY_MAXW : w;
}

float hint_key_aspect(const char* lab) { return (float)key_cols(lab) / KEY_H; }

static void key_draw_pixels(uint8_t* out, int w, int h, const char* lab) {
    enum { RIM = 0x52739C, SIDE = 0x4A6B94, LOW = 0x39527B, FILL = 0x5A84AD, OUT = 0x100000, WHITE = 0xFFFFFF };
    int W = key_cols(lab);
    uint32_t px[KEY_H][KEY_MAXW]; uint8_t a[KEY_H][KEY_MAXW];
    memset(a, 0, sizeof a);
    for (int y = 0; y < KEY_H; y++)                               /* the plate: the game's button, as wide as needed */
        for (int x = 0; x < W; x++) {
            int in = y == 0 || y == KEY_H - 1 ? x >= 2 && x <= W - 3 : y == 1 || y == KEY_H - 2 ? x >= 1 && x <= W - 2 : 1;
            if (!in) continue;
            int edge = y <= 1 || y >= KEY_H - 2 || x == 0 || x == W - 1;
            uint32_t c = FILL;
            if (edge) c = y <= 2 ? RIM : y <= 4 ? SIDE : LOW;
            else if (y >= 5 && (x == 1 || x == W - 2)) c = RIM;   /* the lower corners a little darker */
            px[y][x] = c; a[y][x] = 1;
        }
    uint8_t ink[KEY_H * KEY_MAXW]; memset(ink, 0, sizeof ink);
    int iw = key_ink(lab, NULL, 0), x0 = (W - iw) / 2;
    key_ink(lab, ink, x0);
    for (int y = 0; y < KEY_H; y++)                               /* black round the white (its four sides), then the white */
        for (int x = 0; x < W; x++) {
            if (ink[y * KEY_MAXW + x]) continue;
            int n = (x > 0 && ink[y * KEY_MAXW + x - 1]) || (x + 1 < W && ink[y * KEY_MAXW + x + 1]) ||
                    (y > 0 && ink[(y - 1) * KEY_MAXW + x]) || (y + 1 < KEY_H && ink[(y + 1) * KEY_MAXW + x]);
            if (n) { px[y][x] = OUT; a[y][x] = 1; }
        }
    for (int y = 0; y < KEY_H; y++) for (int x = 0; x < W; x++) if (ink[y * KEY_MAXW + x]) { px[y][x] = WHITE; a[y][x] = 1; }
    for (int y = 0; y < h; y++)                                   /* each DS pixel a block of the picture */
        for (int x = 0; x < w; x++) {
            int sx = x * W / w, sy = y * KEY_H / h;
            uint8_t* o = &out[((size_t)y * w + x) * 4];
            if (!a[sy][sx]) { o[0] = o[1] = o[2] = o[3] = 0; continue; }
            o[0] = (uint8_t)(px[sy][sx] >> 16); o[1] = (uint8_t)(px[sy][sx] >> 8); o[2] = (uint8_t)px[sy][sx]; o[3] = 255;
        }
}

void hint_draw(uint8_t* rgba, int w, int h, int style, int face, const char* lab) {
    g_px = rgba; g_w = w; g_h = h; g_k = (float)(w < h ? w : h) / 2;
    memset(rgba, 0, (size_t)w * h * 4);
    if (style == HINT_KEYBOARD) { key_draw_pixels(rgba, w, h, lab); return; }   /* a key, in the style of the game's button */
    if (face == HINT_OTHER) {                                    /* a shoulder, trigger, stick...: its name on a plate */
        shape(S_RBOX, 0xF0F0F0, 0.95f, 0, 0, 0.96f, 0.7f, 0.42f);
        shape(S_RBOX, 0x1E2127, 0.95f, 0, 0, 0.88f, 0.62f, 0.36f);
        label(lab, 0, 0, 0.8f, 1.5f, 0xFFFFFF);
        return;
    }
    shape(S_DISK, 0xF0F0F0, 0.95f, 0, 0, 0.96f, 0, 0);          /* a round button: light rim, dark face */
    shape(S_DISK, 0x1E2127, 0.95f, 0, 0, 0.88f, 0, 0);
    if (style == HINT_PLAYSTATION) {                             /* the symbols, in their colours */
        static const uint32_t col[4] = { 0xFF6A6A, 0x8AB8FF, 0x4FE0B4, 0xF39BDB };   /* circle, cross, triangle, square */
        uint32_t c = col[face];
        if (face == HINT_RIGHT) shape(S_RING, c, 1, 0, 0, 0.42f, 0.085f, 0);
        else if (face == HINT_BOTTOM) shape(S_CROSS, c, 1, 0, 0, 0.36f, 0.085f, 0);
        else if (face == HINT_TOP) shape(S_TRI, c, 1, 0, 0, 0.5f, 0.085f, 0);
        else shape(S_SQUARE, c, 1, 0, 0, 0.36f, 0.085f, 0);
    } else if (style == HINT_XBOX || style == HINT_NINTENDO) {   /* the letter printed on it */
        static const char* const xb[4] = { "B", "A", "Y", "X" }, *const nin[4] = { "A", "B", "X", "Y" };
        static const uint32_t col[4] = { 0xF0605A, 0x7ED45A, 0xF4CC3A, 0x4E9CF0 };   /* B red, A green, Y yellow, X blue */
        label(style == HINT_XBOX ? xb[face] : nin[face], 0, 0, 1.15f, 1.2f, style == HINT_XBOX ? col[face] : 0xFFFFFF);
    } else {                                                     /* any other controller: four dots, the button's lit */
        static const float at[4][2] = { { 0.42f, 0 }, { 0, 0.42f }, { 0, -0.42f }, { -0.42f, 0 } };
        for (int i = 0; i < 4; i++) {
            if (i == face) shape(S_DISK, 0xFFFFFF, 1, at[i][0], at[i][1], 0.22f, 0, 0);
            else shape(S_RING, 0x8A8F99, 1, at[i][0], at[i][1], 0.17f, 0.04f, 0);
        }
    }
}
