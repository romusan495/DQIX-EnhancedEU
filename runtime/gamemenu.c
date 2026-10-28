/* The in-game menu (gamemenu.h): the menu bar's tree, drawn over the game in
 * the style of the game's own windows (dark blue, white rounded frame, the
 * white arrow on the chosen line), into a picture the GL renderer lays over
 * the window. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gamemenu.h"
#include "uitext.h"
#include "menu.h"
#include "controls.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "stb_truetype.h"
#pragma GCC diagnostic pop

#define MAX_ROWS 200
#define MAX_DEPTH 8

typedef struct { int kind, id; const char* text; const char* key; int on; } Row;

static int g_open, g_dirty = 1;
static int g_stack[MAX_DEPTH], g_sel[MAX_DEPTH], g_top[MAX_DEPTH], g_depth;   /* the submenus opened, from the bar down */
static Row g_rows[MAX_ROWS];
static int g_nrows;
static uint8_t* g_pic; static int g_pw, g_ph;
/* where the last picture put things (drawable pixels), for the mouse */
static float g_px0, g_px1, g_py0, g_py1, g_rows_y0, g_rowh;
static int g_vis;                                 /* rows shown at once */
static float g_row_y[MAX_ROWS + 1];               /* ...where each one starts (separators are thinner) */
#define SEP_H 0.45f                               /* a separator's height, in rows */

static stbtt_fontinfo g_font;
static unsigned char* g_font_data;
static char g_font_name[256] = "Marcellus.ttf";
static int g_font_state;                          /* 0 not tried, 1 ok, -1 none */

/* ---------------------------------------------------------------- the rows */
static int selectable(const Row* r) { return (r->kind == MENU_ITEM && r->on) || r->kind == MENU_SUB; }

static void make_rows(void) {
    const MenuNode* n; int nn = menu_nodes(&n);
    int cur = g_stack[g_depth];
    g_nrows = 0;
    if (g_depth == 0) g_rows[g_nrows++] = (Row){ MENU_ITEM, GM_RESUME, T("Resume"), T("Esc"), 1 };
    for (int i = 1; i < nn && g_nrows < MAX_ROWS; i++) {
        if (n[i].parent != cur) continue;
        if (n[i].kind == MENU_SEP && (g_nrows == 0 || g_rows[g_nrows - 1].kind == MENU_SEP)) continue;
        g_rows[g_nrows++] = (Row){ n[i].kind, n[i].kind == MENU_SUB ? i : n[i].id, n[i].text, n[i].key,
                                   n[i].kind == MENU_ITEM ? menu_enabled(n[i].id) : 1 };
    }
    while (g_nrows && g_rows[g_nrows - 1].kind == MENU_SEP) g_nrows--;
    int* s = &g_sel[g_depth];
    if (*s >= g_nrows) *s = g_nrows - 1;
    if (*s < 0) *s = 0;
    if (g_nrows && !selectable(&g_rows[*s]))     /* the nearest line that can be chosen */
        for (int d = 1; d < g_nrows; d++) {
            if (*s + d < g_nrows && selectable(&g_rows[*s + d])) { *s += d; break; }
            if (*s - d >= 0 && selectable(&g_rows[*s - d])) { *s -= d; break; }
        }
}

/* the chosen line in view */
static void scroll_to_sel(void) {
    int s = g_sel[g_depth], *t = &g_top[g_depth];
    if (g_vis <= 0) return;
    if (s < *t) *t = s;
    if (s >= *t + g_vis) *t = s - g_vis + 1;
    if (*t > g_nrows - g_vis) *t = g_nrows - g_vis;
    if (*t < 0) *t = 0;
}

int gamemenu_is_open(void) { return g_open; }
void gamemenu_open(void) {
    if (g_open) return;
    g_open = 1; g_dirty = 1;
    g_depth = 0; g_stack[0] = 0; g_sel[0] = 0; g_top[0] = 0;
}
void gamemenu_close(void) { g_open = 0; g_dirty = 1; }
void gamemenu_changed(void) { g_dirty = 1; }
void gamemenu_set_font(const char* name) {
    if (!name || !strcmp(name, g_font_name)) return;
    snprintf(g_font_name, sizeof g_font_name, "%s", name);
    g_font_state = 0; g_dirty = 1;
}

static void move(int dir) {
    make_rows();
    int s = g_sel[g_depth];
    for (int k = 0; k < g_nrows; k++) {
        s = (s + dir + g_nrows) % g_nrows;
        if (selectable(&g_rows[s])) break;
    }
    g_sel[g_depth] = s;
    scroll_to_sel();
    g_dirty = 1;
}

static int choose(void) {
    make_rows();
    if (!g_nrows) return 0;
    const Row* r = &g_rows[g_sel[g_depth]];
    if (!selectable(r)) return 0;
    if (r->kind == MENU_SUB) {
        if (g_depth + 1 >= MAX_DEPTH) return 0;
        g_depth++; g_stack[g_depth] = r->id; g_sel[g_depth] = 0; g_top[g_depth] = 0;
        make_rows();
        int on = -1, n_on = 0;                        /* a list of choices (a size, a speed...): on the one made */
        for (int i = 0; i < g_nrows; i++)
            if (g_rows[i].kind == MENU_ITEM && g_rows[i].id > 0 && menu_checked(g_rows[i].id)) { if (on < 0) on = i; n_on++; }
        if (n_on == 1) g_sel[g_depth] = on;
        g_dirty = 1;
        return 0;
    }
    if (r->id == GM_RESUME) gamemenu_close();
    g_dirty = 1;
    return r->id;
}

int gamemenu_key(int key) {
    if (!g_open) return 0;
    switch (key) {
    case GM_UP: move(-1); break;
    case GM_DOWN: move(1); break;
    case GM_PAGEUP: for (int i = 0; i < 5; i++) move(-1); break;
    case GM_PAGEDOWN: for (int i = 0; i < 5; i++) move(1); break;
    case GM_OK: return choose();
    case GM_BACK: if (g_depth > 0) { g_depth--; g_dirty = 1; } else gamemenu_close(); break;
    case GM_CLOSE: gamemenu_close(); break;
    }
    return 0;
}

int gamemenu_mouse(float x, float y, int click) {
    if (!g_open || g_rowh <= 0) return 0;
    if (click == 2) return gamemenu_key(GM_BACK);
    int inside = x >= g_px0 && x < g_px1 && y >= g_py0 && y < g_py1;
    if (!inside) { if (click == 1) return gamemenu_key(GM_BACK); return 0; }
    make_rows();
    int row = -1;
    for (int k = 0; k < g_vis; k++) if (y >= g_row_y[k] && y < g_row_y[k + 1]) row = g_top[g_depth] + k;
    if (row < 0 || row >= g_nrows || !selectable(&g_rows[row])) return 0;
    if (row != g_sel[g_depth]) { g_sel[g_depth] = row; g_dirty = 1; }
    return click == 1 ? choose() : 0;
}

void gamemenu_wheel(int dy) {
    if (!g_open) return;
    make_rows();
    int* t = &g_top[g_depth];
    *t -= dy * 2;
    if (*t > g_nrows - g_vis) *t = g_nrows - g_vis;
    if (*t < 0) *t = 0;
    int* s = &g_sel[g_depth];                     /* the chosen line stays in view */
    if (*s < *t) *s = *t;
    if (*s >= *t + g_vis) *s = *t + g_vis - 1;
    make_rows();
    g_dirty = 1;
}

/* ---------------------------------------------------------------- drawing */
static int font_load(void) {
    if (g_font_state) return g_font_state > 0;
    g_font_state = -1;
    char paths[6][300]; int n = 0;
    if (strstr(g_font_name, ".ttf")) snprintf(paths[n++], 300, "fonts/%s", g_font_name);
    snprintf(paths[n++], 300, "fonts/Marcellus.ttf");
    const char* win = getenv("WINDIR");
    if (win) snprintf(paths[n++], 300, "%s\\Fonts\\segoeui.ttf", win);
    snprintf(paths[n++], 300, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    for (int i = 0; i < n; i++) {
        FILE* f = fopen(paths[i], "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        unsigned char* d = sz > 0 ? malloc((size_t)sz) : NULL;
        if (d && fread(d, 1, (size_t)sz, f) == (size_t)sz && stbtt_InitFont(&g_font, d, stbtt_GetFontOffsetForIndex(d, 0))) {
            fclose(f); free(g_font_data); g_font_data = d; g_font_state = 1; return 1;
        }
        fclose(f); free(d);
    }
    return 0;
}

static int utf8_next(const char** s) {
    const unsigned char* p = (const unsigned char*)*s;
    int c = *p++;
    if (c >= 0xC0 && c < 0xE0 && (*p & 0xC0) == 0x80) c = ((c & 0x1F) << 6) | (*p++ & 0x3F);
    else if (c >= 0xE0 && c < 0xF0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80) { c = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F); p += 2; }
    *s = (const char*)p;
    return c;
}

/* one premultiplied colour over a pixel, with coverage a (0..1) */
static void over(int x, int y, const float c[4], float a) {
    if (x < 0 || y < 0 || x >= g_pw || y >= g_ph || a <= 0) return;
    uint8_t* p = g_pic + ((size_t)y * g_pw + x) * 4;
    float k = c[3] * a;
    for (int i = 0; i < 3; i++) p[i] = (uint8_t)(c[i] * k * 255.0f + p[i] * (1 - k) + 0.5f);
    p[3] = (uint8_t)(k * 255.0f + p[3] * (1 - k) + 0.5f);
}

static float text_width(const char* s, float px) {
    if (!font_load()) return 0;
    float sc = stbtt_ScaleForPixelHeight(&g_font, px), x = 0; int prev = 0;
    while (*s) {
        int c = utf8_next(&s), adv, lsb;
        stbtt_GetCodepointHMetrics(&g_font, c, &adv, &lsb);
        if (prev) x += sc * (float)stbtt_GetCodepointKernAdvance(&g_font, prev, c);
        x += sc * (float)adv; prev = c;
    }
    return x;
}

/* text with its top at y */
static void text(float x, float y, float px, const float c[4], const char* s) {
    if (!font_load()) return;
    float sc = stbtt_ScaleForPixelHeight(&g_font, px);
    int asc, desc, gap; stbtt_GetFontVMetrics(&g_font, &asc, &desc, &gap);
    int base = (int)(y + asc * sc + 0.5f), prev = 0;
    while (*s) {
        int ch = utf8_next(&s), adv, lsb;
        if (prev) x += sc * (float)stbtt_GetCodepointKernAdvance(&g_font, prev, ch);
        stbtt_GetCodepointHMetrics(&g_font, ch, &adv, &lsb);
        int bw = 0, bh = 0, bx = 0, by = 0;
        unsigned char* bm = stbtt_GetCodepointBitmapSubpixel(&g_font, sc, sc, x - floorf(x), 0, ch, &bw, &bh, &bx, &by);
        for (int j = 0; j < bh; j++) for (int i = 0; i < bw; i++)
            if (bm[j * bw + i]) over((int)floorf(x) + bx + i, base + by + j, c, bm[j * bw + i] / 255.0f);
        stbtt_FreeBitmap(bm, NULL);
        x += sc * (float)adv; prev = ch;
    }
}

/* how much of the pixel at (px, py) a rounded box covers (signed distance) */
static float box_cover(float px, float py, float x0, float y0, float x1, float y1, float r) {
    float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, hx = (x1 - x0) / 2 - r, hy = (y1 - y0) / 2 - r;
    float dx = fabsf(px - cx) - hx, dy = fabsf(py - cy) - hy;
    float ox = dx > 0 ? dx : 0, oy = dy > 0 ? dy : 0;
    float d = sqrtf(ox * ox + oy * oy) + fminf(fmaxf(dx, dy), 0) - r;
    return fminf(fmaxf(0.5f - d, 0), 1);
}

static void box(float x0, float y0, float x1, float y1, float r, const float c[4]) {
    for (int y = (int)y0 - 1; y <= (int)y1 + 1; y++) for (int x = (int)x0 - 1; x <= (int)x1 + 1; x++)
        over(x, y, c, box_cover(x + 0.5f, y + 0.5f, x0, y0, x1, y1, r));
}

/* the game's cursor: a small arrow pointing right, tip at (x, y) */
static void arrow(float x, float y, float s, const float c[4]) {
    for (int py = (int)(y - s) - 1; py <= (int)(y + s) + 1; py++) for (int px = (int)(x - s * 1.4f) - 1; px <= (int)x + 1; px++) {
        int in = 0;
        for (int sy = 0; sy < 4; sy++) for (int sx = 0; sx < 4; sx++) {
            float fx = px + (sx + 0.5f) / 4, fy = py + (sy + 0.5f) / 4;
            float t = (x - fx) / (s * 1.4f);              /* 0 at the tip, 1 at the back */
            if (t >= 0 && t <= 1 && fabsf(fy - y) <= s * t) in++;
        }
        over(px, py, c, in / 16.0f);
    }
}

/* a small triangle pointing up or down, centred on (x, y): more lines above / below */
static void updown(float x, float y, float s, int up, const float c[4]) {
    for (int py = (int)(y - s) - 1; py <= (int)(y + s) + 1; py++) for (int px = (int)(x - s) - 1; px <= (int)(x + s) + 1; px++) {
        int in = 0;
        for (int sy = 0; sy < 4; sy++) for (int sx = 0; sx < 4; sx++) {
            float fx = px + (sx + 0.5f) / 4 - x, fy = py + (sy + 0.5f) / 4 - y;
            float t = up ? (fy + s * 0.6f) / (s * 1.2f) : (s * 0.6f - fy) / (s * 1.2f);   /* 0 at the point, 1 at the base */
            if (t >= 0 && t <= 1 && fabsf(fx) <= s * t) in++;
        }
        over(px, py, c, in / 16.0f);
    }
}

/* a check mark in the box (x, y, size s) */
static void check(float x, float y, float s, const float c[4]) {
    const float seg[2][4] = { { x + s * 0.12f, y + s * 0.52f, x + s * 0.4f, y + s * 0.8f }, { x + s * 0.4f, y + s * 0.8f, x + s * 0.9f, y + s * 0.18f } };
    float w = s * 0.09f;
    for (int py = (int)y - 1; py <= (int)(y + s) + 1; py++) for (int px = (int)x - 1; px <= (int)(x + s) + 1; px++) {
        float best = 1e9f;
        for (int k = 0; k < 2; k++) {
            float ax = seg[k][0], ay = seg[k][1], bx = seg[k][2] - ax, by = seg[k][3] - ay;
            float t = ((px + 0.5f - ax) * bx + (py + 0.5f - ay) * by) / (bx * bx + by * by);
            t = fminf(fmaxf(t, 0), 1);
            float dx = px + 0.5f - (ax + t * bx), dy = py + 0.5f - (ay + t * by), d = sqrtf(dx * dx + dy * dy);
            if (d < best) best = d;
        }
        over(px, py, c, fminf(fmaxf(w - best + 0.5f, 0), 1));
    }
}

/* The window itself is drawn by the renderer in the minimal style of the
 * game's restyled windows (gamemenu_box, gl_set_menu_box): see-through black
 * with a grey line. Here: the veil and what is written in it. */
static float C_VEIL[4] = { 0, 0, 0, 0.45f }, C_WHITE[4] = { 1, 1, 1, 1 };
static float C_DIM[4] = { 0.60f, 0.60f, 0.60f, 1 }, C_KEY[4] = { 0.75f, 0.75f, 0.75f, 1 }, C_SEL[4] = { 1, 1, 1, 0.12f };
static float C_TITLE[4] = { 0.93f, 0.93f, 0.93f, 1 }, C_LINE[4] = { 0.5f, 0.5f, 0.5f, 1 };
static int g_contrast, g_size;                    /* View > Readability: stronger contrast; text normal, large, larger */
void gamemenu_set_readability(int contrast, int size) {
    if (contrast == g_contrast && size == g_size) return;
    g_contrast = contrast != 0; g_size = size < 0 ? 0 : size > 2 ? 2 : size;
    /* stronger contrast: the greys nearly white, a darker veil (under the text too), a clearer cursor */
    float dim = g_contrast ? 0.88f : 0.60f, key = g_contrast ? 0.95f : 0.75f, line = g_contrast ? 0.85f : 0.5f;
    for (int i = 0; i < 3; i++) { C_DIM[i] = dim; C_KEY[i] = key; C_LINE[i] = line; C_TITLE[i] = g_contrast ? 1 : 0.93f; }
    C_VEIL[3] = g_contrast ? 0.72f : 0.45f; C_SEL[3] = g_contrast ? 0.30f : 0.12f;
    g_dirty = 1;
}
static float g_box[5];                            /* the window: x0 y0 x1 y1, line width (drawable pixels) */

const float* gamemenu_box(void) { return g_open && g_box[4] > 0 ? g_box : NULL; }

const uint8_t* gamemenu_picture(int w, int h, int* changed) {
    *changed = 0;
    if (w <= 0 || h <= 0) return g_pic;
    if (!g_dirty && w == g_pw && h == g_ph && g_pic) return g_pic;
    if (w != g_pw || h != g_ph || !g_pic) {
        free(g_pic); g_pic = malloc((size_t)w * h * 4); g_pw = w; g_ph = h;
        if (!g_pic) { g_pw = g_ph = 0; return NULL; }
    }
    g_dirty = 0; *changed = 1;
    make_rows();
    float u = (float)h / 720.0f; if (u < 0.6f) u = 0.6f;
    if (u * 1100 > w * 0.92f) u = w * 0.92f / 1100;   /* narrow windows */
    float z = g_size == 2 ? 1.4f : g_size ? 1.2f : 1;  /* larger text: larger rows */
    float pw = fminf(w - 40 * u, 980 * u), title_px = 30 * u * z, row_px = 23 * u * z;
    g_rowh = 40 * u * z;
    float head = 76 * u * z, foot = 52 * u * z;
    float room = h - 2 * 30 * u - head - foot, all = 0;
    for (int i = 0; i < g_nrows; i++) all += g_rows[i].kind == MENU_SEP ? SEP_H : 1;
    int fit = (int)(room / g_rowh);                   /* whole rows that fit (separators make it a little more) */
    if (fit < 1) fit = 1;
    g_vis = g_nrows;
    if (all * g_rowh > room) { g_vis = fit; scroll_to_sel(); }
    else g_top[g_depth] = 0;
    float rows_h = 0;
    for (int k = 0; k < g_vis && g_top[g_depth] + k < g_nrows; k++) rows_h += (g_rows[g_top[g_depth] + k].kind == MENU_SEP ? SEP_H : 1) * g_rowh;
    float ph = head + rows_h + foot;
    g_px0 = (w - pw) / 2; g_px1 = g_px0 + pw; g_py0 = (h - ph) / 2; g_py1 = g_py0 + ph;
    g_px0 = floorf(g_px0); g_py0 = floorf(g_py0); g_px1 = ceilf(g_px1); g_py1 = ceilf(g_py1);
    g_box[0] = g_px0; g_box[1] = g_py0; g_box[2] = g_px1; g_box[3] = g_py1; g_box[4] = 3.5f * u;
    /* the game darkened all over, except under the window's frame line: inside,
     * the veil over the window's own see-through black keeps a long list readable */
    float e = ceilf(1.6f * g_box[4]);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        uint8_t* p = g_pic + ((size_t)y * w + x) * 4;
        int in = x >= g_px0 && x < g_px1 && y >= g_py0 && y < g_py1;
        int inner = x >= g_px0 + e && x < g_px1 - e && y >= g_py0 + e && y < g_py1 - e;
        p[0] = p[1] = p[2] = 0; p[3] = in && !inner ? 0 : (uint8_t)(C_VEIL[3] * 255);
    }

    /* the title: where we are */
    char title[400];
    snprintf(title, sizeof title, "%s", T("Options"));
    const MenuNode* n; menu_nodes(&n);
    for (int d = 1; d <= g_depth; d++) { size_t l = strlen(title); snprintf(title + l, sizeof title - l, "  \xE2\x80\xBA  %s", n[g_stack[d]].text); }
    float tx = g_px0 + 40 * u;
    while (text_width(title, title_px) > pw - 80 * u && title_px > 14 * u) title_px *= 0.92f;
    text(tx, g_py0 + 22 * u, title_px, C_TITLE, title);
    for (int x = (int)(g_px0 + 18 * u); x < (int)(g_px1 - 18 * u); x++)   /* a line under it, as under the game's column titles */
        for (int t = 0; t < (int)(1.5f * u + 0.5f); t++) over(x, (int)(g_py0 + head - 8 * u) + t, C_LINE, 1);

    /* the lines */
    g_rows_y0 = g_py0 + head;
    float left = g_px0 + 58 * u, right = g_px1 - 36 * u;
    float y = g_rows_y0;
    for (int k = 0; k < g_vis && g_top[g_depth] + k < g_nrows; k++) {
        int i = g_top[g_depth] + k;
        const Row* r = &g_rows[i];
        g_row_y[k] = y;
        float ty = y + (g_rowh - row_px * 1.25f) / 2;
        if (r->kind == MENU_SEP) {
            for (int x = (int)left; x < (int)right; x++) over(x, (int)(y + g_rowh * SEP_H / 2), (const float[4]){ 1, 1, 1, 0.22f }, 1);
            y += g_rowh * SEP_H; g_row_y[k + 1] = y;
            continue;
        }
        int sel = i == g_sel[g_depth];
        if (sel) {
            box(g_px0 + 18 * u, y + 3 * u, g_px1 - 18 * u, y + g_rowh - 3 * u, 0, C_SEL);
            arrow(g_px0 + 46 * u, y + g_rowh / 2, 8 * u, C_WHITE);
        }
        const float* col = r->kind == MENU_LABEL || !r->on ? C_DIM : C_WHITE;
        float kw = 0;
        if (r->kind == MENU_SUB) kw = 24 * u;
        else if (r->key && r->key[0]) kw = text_width(r->key, row_px * 0.85f) + 16 * u;
        float cw = r->kind == MENU_ITEM && r->id > 0 && menu_checked(r->id) ? 34 * u : 0;
        float avail = right - left - kw - 40 * u, px = row_px;
        while (text_width(r->text, px) > avail && px > row_px * 0.62f) px *= 0.95f;
        text(left, ty + (row_px - px) * 0.6f, px, col, r->text);
        if (cw > 0) check(right - kw - cw + 4 * u, y + (g_rowh - 22 * u) / 2, 22 * u, (const float[4]){ 0.55f, 0.95f, 0.6f, 1 });
        if (r->kind == MENU_SUB) arrow(right - 2 * u, y + g_rowh / 2, 6 * u, C_KEY);
        else if (kw > 0) text(right - kw + 16 * u, ty + row_px * 0.08f, row_px * 0.85f, C_KEY, r->key);
        y += g_rowh; g_row_y[k + 1] = y;
    }
    if (g_top[g_depth] > 0) updown(g_px1 - 48 * u, g_rows_y0 - 14 * u, 9 * u, 1, C_KEY);   /* more above / below */
    if (g_top[g_depth] + g_vis < g_nrows) updown(g_px1 - 48 * u, g_py1 - foot + 14 * u, 9 * u, 0, C_KEY);

    /* how to use it */
    char hint[300];
    if (g_ctl_pad_label[0]) {                       /* a controller: its own buttons' names (a PlayStation's Circle and Cross...) */
        char ok[64], back[64], menu[64];
        controls_pad_name(g_ctl_pad[CA_A], ok, sizeof ok); controls_pad_name(g_ctl_pad[CA_B], back, sizeof back);
        controls_pad_name(g_ctl_pad[CA_MENU], menu, sizeof menu);
        if (!strcmp(menu, "-")) snprintf(menu, sizeof menu, "%s", T("Esc"));   /* no button for it */
        if (g_depth) snprintf(hint, sizeof hint, T("%s: choose     %s: back     %s: back to the game"), T(ok), T(back), T(menu));
        else snprintf(hint, sizeof hint, T("%s: choose     %s / %s: back to the game"), T(ok), T(back), T(menu));
    } else snprintf(hint, sizeof hint, "%s", g_depth ? T("A / Enter: choose     B / Backspace: back     Esc: back to the game")
                                                     : T("A / Enter: choose     B / Esc: back to the game"));
    float hp = 17 * u * (g_size ? 1.15f : 1);
    while (text_width(hint, hp) > pw - 80 * u && hp > 10 * u) hp *= 0.92f;
    text(g_px0 + 40 * u, g_py1 - foot + (foot - hp * 1.25f) / 2, hp, C_DIM, hint);
    return g_pic;
}
