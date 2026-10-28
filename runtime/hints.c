/* A button's sign as the player's own controller or keyboard shows it
 * (hints.h). Shapes are drawn from their distance to each pixel, smoothed
 * over one pixel; letters with the system font (text.c). */
#include "hints.h"
#include "text.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

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

void hint_draw(uint8_t* rgba, int w, int h, int style, int face, const char* lab) {
    g_px = rgba; g_w = w; g_h = h; g_k = (float)(w < h ? w : h) / 2;
    memset(rgba, 0, (size_t)w * h * 4);
    if (style == HINT_KEYBOARD) {                                /* a key's cap: light, its lower edge shaded */
        shape(S_RBOX, 0x101010, 0.6f, 0, 0.08f, 0.94f, 0.86f, 0.3f);
        shape(S_RBOX, 0x9EA3AD, 1, 0, 0.04f, 0.9f, 0.82f, 0.26f);
        shape(S_RBOX, 0xF4F5F7, 1, 0, -0.06f, 0.84f, 0.7f, 0.2f);
        label(lab, 0, -0.06f, 1.0f, 1.45f, 0x23262B);
        return;
    }
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
