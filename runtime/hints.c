/* A button's sign as the player's own controller or keyboard shows it
 * (hints.h). Shapes are drawn from their distance to each pixel, smoothed
 * over one pixel; letters with the system font (text.c), a keyboard key's
 * with pixel fonts (fonts/). */
#include "hints.h"
#include "text.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "uitext.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "stb_truetype.h"
#pragma GCC diagnostic pop

static uint8_t* g_px; static int g_w, g_h;
static float g_k;                                                /* pixels per unit: the picture is 2 units across */
static int g_hard;                                               /* a key in pixels: every coverage solid or clear */

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
/* A keyboard key drawn like the game's own button on the "X Menu" sign: a blue
 * octagon, lighter at the top, the key's sign in white with a dark outline, in
 * pixels (KEY_GRID high, every pixel solid or clear) like the game's pictures.
 * One or two characters: an octagon; a longer name: a long octagon (its width
 * follows the name). The keys every keyboard prints with a symbol get it. */
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

static int utf8_len(const char* s) { int n = 0; for (; *s; s++) n += (*s & 0xC0) != 0x80; return n; }
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

/* a polyline (closed or not) stroked half thick, or filled */
static void poly(const float* p, int n, int closed, float half, int fill, uint32_t rgb) {
    for (int y = 0; y < g_h; y++)
        for (int x = 0; x < g_w; x++) {
            float ux = ((float)x + 0.5f) / g_k - (float)g_w / g_k / 2, uy = ((float)y + 0.5f) / g_k - (float)g_h / g_k / 2;
            float d = 1e9f; int in = 0;
            for (int i = 0; i < n - !closed; i++) {
                const float* a = &p[i * 2], *b = &p[((i + 1) % n) * 2];
                float e = seg(ux, uy, a[0], a[1], b[0], b[1]); d = e < d ? e : d;
                if (fill && ((a[1] > uy) != (b[1] > uy)) && ux < (b[0] - a[0]) * (uy - a[1]) / (b[1] - a[1]) + a[0]) in = !in;
            }
            d -= half;
            if (in) d = -fabsf(d) - half;
            put(x, y, rgb, 0.5f - d * g_k);
        }
}

static void key_symbol(int s, uint32_t rgb) {
    const float t = 0.085f;
    switch (s) {
    case K_UP: case K_DOWN: case K_LEFT: case K_RIGHT: {         /* a solid triangle, like the DS's cross keys */
        float v[6] = { 0, -0.42f, 0.4f, 0.26f, -0.4f, 0.26f };
        for (int i = 0; i < 3; i++) {
            float x = v[i * 2], y = v[i * 2 + 1];
            if (s == K_DOWN) y = -y; else if (s == K_LEFT) { float k = x; x = y; y = k; } else if (s == K_RIGHT) { float k = x; x = -y; y = k; }
            v[i * 2] = x; v[i * 2 + 1] = y;
        }
        poly(v, 3, 1, 0.04f, 1, rgb); break;
    }
    case K_ENTER: {                                               /* down from the top right, then back to the left */
        const float a[] = { 0.38f, -0.42f, 0.38f, 0.14f, -0.36f, 0.14f }; poly(a, 3, 0, t, 0, rgb);
        const float h[] = { -0.14f, -0.08f, -0.38f, 0.14f, -0.14f, 0.36f }; poly(h, 3, 0, t, 0, rgb); break;
    }
    case K_BACK: {                                                /* a long arrow to the left */
        const float a[] = { 0.46f, 0, -0.44f, 0 }; poly(a, 2, 0, t, 0, rgb);
        const float h[] = { -0.2f, -0.24f, -0.46f, 0, -0.2f, 0.24f }; poly(h, 3, 0, t, 0, rgb); break;
    }
    case K_TAB: {                                                 /* an arrow to the right, against a bar */
        const float a[] = { -0.46f, 0, 0.34f, 0 }; poly(a, 2, 0, t, 0, rgb);
        const float h[] = { 0.1f, -0.24f, 0.34f, 0, 0.1f, 0.24f }; poly(h, 3, 0, t, 0, rgb);
        const float b[] = { 0.46f, -0.3f, 0.46f, 0.3f }; poly(b, 2, 0, t, 0, rgb); break;
    }
    case K_SHIFT: case K_CAPS: {                                  /* a hollow arrow up (and a bar under it: caps lock) */
        float y = s == K_CAPS ? -0.1f : 0;
        const float a[] = { 0, -0.5f + y, 0.44f, -0.02f + y, 0.2f, -0.02f + y, 0.2f, 0.4f + y, -0.2f, 0.4f + y, -0.2f, -0.02f + y, -0.44f, -0.02f + y };
        poly(a, 7, 1, 0.07f, 0, rgb);
        if (s == K_CAPS) { const float b[] = { -0.2f, 0.5f, 0.2f, 0.5f }; poly(b, 2, 0, 0.07f, 0, rgb); }
        break;
    }
    case K_SPACE: { const float a[] = { -0.5f, -0.1f, -0.5f, 0.2f, 0.5f, 0.2f, 0.5f, -0.1f }; poly(a, 4, 0, t, 0, rgb); break; }
    case K_HOME: {                                                /* an arrow to the top left corner */
        const float a[] = { 0.36f, 0.36f, -0.3f, -0.3f }; poly(a, 2, 0, t, 0, rgb);
        const float h[] = { -0.3f, 0.02f, -0.3f, -0.3f, 0.02f, -0.3f }; poly(h, 3, 0, t, 0, rgb);
        const float b[] = { -0.48f, 0.3f, -0.48f, -0.48f, 0.3f, -0.48f }; poly(b, 3, 0, t * 0.8f, 0, rgb); break;
    }
    }
}

/* the game's button plate: an octagon (a long one for a name), blue, lighter at the top */
static void plate(float hx, float hy, float c) {
    for (int y = 0; y < g_h; y++)
        for (int x = 0; x < g_w; x++) {
            float ux = ((float)x + 0.5f) / g_k - (float)g_w / g_k / 2, uy = ((float)y + 0.5f) / g_k - (float)g_h / g_k / 2;
            float ax = fabsf(ux), ay = fabsf(uy);
            float d = fmaxf(fmaxf(ax - hx, ay - hy), (ax + ay - (hx + hy - c)) * 0.70710678f);
            float k = (uy / hy + 1) / 2; k = k < 0 ? 0 : k > 1 ? 1 : k;           /* 0 top, 1 bottom */
            uint32_t r = (uint32_t)(0x5A + (0x37 - 0x5A) * k), g = (uint32_t)(0x84 + (0x4D - 0x84) * k), b = (uint32_t)(0xAD + (0x70 - 0xAD) * k);
            put(x, y, r << 16 | g << 8 | b, 0.5f - d * g_k);
        }
}

/* the coverage of src grown by r pixels (round), into dst (alpha only) */
static void grow(const uint8_t* src, uint8_t* dst, float r) {
    int ri = (int)ceilf(r);
    for (int y = 0; y < g_h; y++)
        for (int x = 0; x < g_w; x++) {
            float o = 0;
            for (int dy = -ri; dy <= ri; dy++) {
                int yy = y + dy; if (yy < 0 || yy >= g_h) continue;
                for (int dx = -ri; dx <= ri; dx++) {
                    int xx = x + dx; if (xx < 0 || xx >= g_w) continue;
                    float e = r + 0.5f - sqrtf((float)(dx * dx + dy * dy));
                    if (e <= 0) continue;
                    float a = src[((size_t)yy * g_w + xx) * 4 + 3] / 255.0f * (e < 1 ? e : 1);
                    if (a > o) o = a;
                }
            }
            dst[((size_t)y * g_w + x) * 4 + 3] = (uint8_t)(o * 255 + 0.5f);
        }
}

/* the sign (white over a clear picture, tmp): laid on g_px in a dark outline r pixels round it */
static void outline_onto(uint8_t* tmp, float r) {
    uint8_t* b = calloc((size_t)g_w * g_h, 4);
    if (!b) return;
    memcpy(b, tmp, (size_t)g_w * g_h * 4);
    grow(b, tmp, r);                                              /* tmp: the outline's coverage */
    if (g_hard) for (size_t i = 0; i < (size_t)g_w * g_h; i++) tmp[i * 4 + 3] = tmp[i * 4 + 3] >= 128 ? 255 : 0;
    for (size_t i = 0; i < (size_t)g_w * g_h; i++) put((int)(i % (size_t)g_w), (int)(i / (size_t)g_w), 0x100000, tmp[i * 4 + 3] / 255.0f);
    for (size_t i = 0; i < (size_t)g_w * g_h; i++) put((int)(i % (size_t)g_w), (int)(i / (size_t)g_w), 0xFFFFFF, b[i * 4 + 3] / 255.0f);
    free(b);
}

/* ---- the keys' pixel fonts (fonts/, SIL OFL): Pixelify Sans Bold, wide and bold
 * like the game's letters, for one or two characters; Jersey 10, narrow, for a
 * name. Drawn at their own pixel size, every pixel solid or clear. */
#define KEY_GRID 22                                               /* a key's height in its pixels (the game's button: about 11 DS pixels; twice as fine) */
static struct { const char* file; float em; stbtt_fontinfo f; unsigned char* data; int state; } PXF[2] = {
    { "fonts/PixelifySans-Bold.ttf", 16, { 0 }, 0, 0 }, { "fonts/Jersey10.ttf", 16, { 0 }, 0, 0 } };

static stbtt_fontinfo* pxf(int i) {
    if (!PXF[i].state) {
        PXF[i].state = -1;
        FILE* f = fopen(PXF[i].file, "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
            PXF[i].data = n > 0 ? malloc((size_t)n) : NULL;
            if (PXF[i].data && fread(PXF[i].data, 1, (size_t)n, f) == (size_t)n && stbtt_InitFont(&PXF[i].f, PXF[i].data, 0)) PXF[i].state = 1;
            fclose(f);
        }
    }
    return PXF[i].state > 0 ? &PXF[i].f : NULL;
}

/* s in pixel font i (big: times its size) into cov (alpha only, w x h) from x0, its baseline at y;
 * cov NULL: measures. A character the font lacks comes from the other one. Returns the width. */
static int pxf_text(int i, float big, const char* s, uint8_t* cov, int w, int h, int x0, int y) {
    int x = x0;
    while (*s) {
        int c = utf8_next(&s), k = i;
        if (!pxf(k) || !stbtt_FindGlyphIndex(pxf(k), c)) k = !i;
        stbtt_fontinfo* f = pxf(k);
        if (!f) return x - x0;
        float sc = stbtt_ScaleForMappingEmToPixels(f, PXF[k].em * big);
        int adv, lsb; stbtt_GetCodepointHMetrics(f, c, &adv, &lsb);
        if (cov) {
            int bw, bh, bx, by;
            unsigned char* bm = stbtt_GetCodepointBitmap(f, sc, sc, c, &bw, &bh, &bx, &by);
            for (int j = 0; bm && j < bh; j++) for (int q = 0; q < bw; q++) {
                int px = x + bx + q, py = y + by + j;
                if (px >= 0 && py >= 0 && px < w && py < h && bm[j * bw + q] >= 128) cov[((size_t)py * w + px) * 4 + 3] = 255;
            }
            stbtt_FreeBitmap(bm, NULL);
        }
        x += (int)lroundf((float)adv * sc);
    }
    return x - x0;
}

static float key_big(const char* t) { return utf8_len(t) == 1 ? 1.25f : 1.0f; }   /* one character: a size larger (punctuation stays readable) */

/* the text a key shows (NULL: a symbol), and the font for it */
static const char* key_text(const char* lab, int* font, char* buf, size_t n) {
    const char* t; int kp, s = key_sign(lab, &t, &kp);
    if (s != K_TEXT) return NULL;
    if (kp) { snprintf(buf, n, "%s %s", T("num"), t); t = buf; }  /* keypad: "num 5" */
    int digit = 0;
    for (const char* c = t; *c; c++) digit |= *c >= '0' && *c <= '9';
    *font = utf8_len(t) == 1 || (utf8_len(t) == 2 && !digit) ? 0 : 1;   /* Pixelify's 5 reads as an S: "F5" in Jersey */
    return t;
}

float hint_key_aspect(const char* lab) {
    const char* t; int kp, s = key_sign(lab, &t, &kp), font = 0;
    if (s == K_SPACE) return 1.4f;
    char buf[64]; t = key_text(lab, &font, buf, sizeof buf);
    if (!t || utf8_len(t) == 1) return 1.0f;                      /* a symbol or a character: the game's square button */
    float a = (float)(pxf_text(font, key_big(t), t, NULL, 0, 0, 0, 0) + 14) / KEY_GRID;   /* the name, and room for the octagon's corners */
    return a < 1 ? 1 : a > 3.0f ? 3.0f : a;
}

/* A key in pixels: names in the pixel fonts, symbols drawn finer and averaged
 * down to the grid; the plate's edge pixels part see-through and blown up
 * smoothly (it melts into the scene like the game's own sign), the sign and
 * its outline square pixels. */
#define KEY_SS 6
static void key_draw_pixels(uint8_t* out, int w, int h, const char* lab) {
    int sh = KEY_GRID, sw = (int)lroundf((float)w * KEY_GRID / (float)h);
    uint8_t* sm = calloc((size_t)sw * sh, 4), *sign = calloc((size_t)sw * sh, 4);
    if (!sm || !sign) { free(sm); free(sign); return; }
    int font = 0; char buf[64];
    const char* t = key_text(lab, &font, buf, sizeof buf);
    if (t) {                                                      /* the name, centred on its ink */
        int tw = sw * 2, th = sh * 2;
        uint8_t* tmp = calloc((size_t)tw * th, 4);
        if (tmp) {
            pxf_text(font, key_big(t), t, tmp, tw, th, sw / 2, sh);
            int x0 = tw, x1 = -1, y0 = th, y1 = -1;
            for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) if (tmp[((size_t)y * tw + x) * 4 + 3]) { x0 = x < x0 ? x : x0; x1 = x > x1 ? x : x1; y0 = y < y0 ? y : y0; y1 = y > y1 ? y : y1; }
            if (x1 >= x0) {
                int ox = (sw - (x1 - x0 + 1)) / 2 - x0, oy = (sh - (y1 - y0 + 1) + 1) / 2 - y0;
                for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++)
                    if (tmp[((size_t)y * tw + x) * 4 + 3] && x + ox >= 0 && x + ox < sw && y + oy >= 0 && y + oy < sh) sign[((size_t)(y + oy) * sw + x + ox) * 4 + 3] = 255;
            }
            free(tmp);
        }
    } else {                                                      /* a symbol: drawn finer, made bolder, averaged down */
        uint8_t* big = calloc((size_t)sw * sh * KEY_SS * KEY_SS, 4), *bold = calloc((size_t)sw * sh * KEY_SS * KEY_SS, 4);
        if (big && bold) {
            g_px = big; g_w = sw * KEY_SS; g_h = sh * KEY_SS; g_k = (float)g_h / 2;
            const char* tt; int kp; key_symbol(key_sign(lab, &tt, &kp), 0xFFFFFF);
            grow(big, bold, KEY_SS * 0.3f);
            for (int y = 0; y < sh; y++)
                for (int x = 0; x < sw; x++) {
                    int sum = 0;
                    for (int j = 0; j < KEY_SS; j++) for (int i = 0; i < KEY_SS; i++) sum += bold[(((size_t)(y * KEY_SS + j)) * g_w + x * KEY_SS + i) * 4 + 3];
                    sign[((size_t)y * sw + x) * 4 + 3] = sum / (KEY_SS * KEY_SS) >= 110 ? 255 : 0;
                }
        }
        free(big); free(bold);
    }
    g_px = sm; g_w = sw; g_h = sh; g_k = (float)sh / 2;
    plate((float)sw / (float)sh - 0.04f, 0.96f, 0.56f);
    uint8_t* sg = calloc((size_t)sw * sh, 4);                     /* the sign and its outline, solid pixels */
    if (sg) { g_px = sg; g_hard = 1; outline_onto(sign, 1.5f); g_hard = 0; g_px = sm; }
    for (int y = 0; y < h; y++)                                   /* blown up: the plate smoothly, the sign square */
        for (int x = 0; x < w; x++) {
            float fx = ((float)x + 0.5f) * sw / w - 0.5f, fy = ((float)y + 0.5f) * sh / h - 0.5f;
            int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
            float tx = fx - x0, ty = fy - y0, acc[4] = { 0, 0, 0, 0 };
            for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {
                int xx = x0 + i, yy = y0 + j;
                if (xx < 0 || yy < 0 || xx >= sw || yy >= sh) continue;
                float wgt = (i ? tx : 1 - tx) * (j ? ty : 1 - ty);
                for (int c = 0; c < 4; c++) acc[c] += sm[((size_t)yy * sw + xx) * 4 + c] * wgt;
            }
            uint8_t* o = &out[((size_t)y * w + x) * 4];
            const uint8_t* q = sg ? &sg[((size_t)(y * sh / h) * sw + (x * sw / w)) * 4] : NULL;
            float k = q ? 1 - q[3] / 255.0f : 1;                  /* premultiplied: the sign over the plate */
            for (int c = 0; c < 4; c++) { float v = (q ? q[c] : 0) + acc[c] * k; o[c] = (uint8_t)(v > 255 ? 255 : v + 0.5f); }
        }
    free(sg); free(sm); free(sign);
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
