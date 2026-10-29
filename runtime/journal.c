/* Modern > Journal (journal.h): a panel over the game with a tab per page.
 * The picture has the window's own pixels and the GL renderer lays it over
 * everything (gl_set_menu: the journal and the in-game menu are never open
 * together); pages draw in panel units (JOURNAL_W x JOURNAL_H, centred,
 * scaled to the window) with the helpers below. Read only: the game waits
 * while it is open and never sees any of it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nds.h"
#include "text.h"
#include "romdata.h"
#include "journal.h"

#define MAX_PAGES 16
static const JournalPage* g_page[MAX_PAGES]; static int g_npage;    /* the journal's own first, then the added ones */
static const JournalPage* g_added[MAX_PAGES]; static int g_nadded;
static int g_open, g_tab, g_dirty = 1, g_ready;
static uint8_t* g_pic; static int g_pw, g_ph;
static uint32_t* g_scratch; static int g_scratch_h;               /* one line of text, drawn alone (coverage) */

void journal_add_page(const JournalPage* p) { if (p && g_nadded < MAX_PAGES) g_added[g_nadded++] = p; }

/* the journal's own pages (journal_quests.c, journal_party.c) */
extern const JournalPage g_journal_quests, g_journal_party, g_journal_vocations;

static void ready(void) {
    if (g_ready) return;
    g_ready = 1;
    g_page[g_npage++] = &g_journal_quests;
    g_page[g_npage++] = &g_journal_party;
    g_page[g_npage++] = &g_journal_vocations;
    for (int i = 0; i < g_nadded && g_npage < MAX_PAGES; i++) g_page[g_npage++] = g_added[i];
}

static int lang(void) { return g_fw_language >= 1 && g_fw_language <= 5 ? g_fw_language - 1 : 0; }

int journal_open(void) { return g_open; }
void journal_toggle(void) {
    ready();
    g_open = !g_open;
    g_dirty = 1;
    if (g_open && g_tab < g_npage && g_page[g_tab]->opened) g_page[g_tab]->opened();
}

int journal_input(int k) {
    if (!g_open) return 0;
    if (k == JK_CLOSE) { g_open = 0; g_dirty = 1; return 1; }
    if (k == JK_TAB_PREV || k == JK_TAB_NEXT) {
        if (g_npage < 2) return 0;
        g_tab = (g_tab + (k == JK_TAB_NEXT ? 1 : g_npage - 1)) % g_npage;
        if (g_page[g_tab]->opened) g_page[g_tab]->opened();
        g_dirty = 1;
        return 1;
    }
    if (g_tab < g_npage && g_page[g_tab]->key && g_page[g_tab]->key(k)) { g_dirty = 1; return 1; }
    return 0;
}

int journal_searching(void) { return g_open && g_tab < g_npage && g_page[g_tab]->text; }

int journal_typed(const char* utf8) {
    if (!g_open || g_tab >= g_npage || !g_page[g_tab]->text) return 0;
    if (g_page[g_tab]->text(utf8)) { g_dirty = 1; return 1; }
    return 0;
}

/* ---------------------------------------------------------------- drawing */
static void put(int x, int y, uint32_t rgb, float a) {           /* premultiplied "over" */
    if (x < 0 || y < 0 || x >= g_pw || y >= g_ph || a <= 0) return;
    if (a > 1) a = 1;
    uint8_t* p = &g_pic[((size_t)y * g_pw + x) * 4];
    float k = 1 - a;
    p[0] = (uint8_t)(((rgb >> 16) & 255) * a + p[0] * k + 0.5f);
    p[1] = (uint8_t)(((rgb >> 8) & 255) * a + p[1] * k + 0.5f);
    p[2] = (uint8_t)((rgb & 255) * a + p[2] * k + 0.5f);
    p[3] = (uint8_t)(255 * a + p[3] * k + 0.5f);
}

void journal_box(const JournalCanvas* c, int x, int y, int w, int h, uint32_t rgb, float alpha) {
    float x0 = c->ox + x * c->u, y0 = c->oy + y * c->u, x1 = x0 + w * c->u, y1 = y0 + h * c->u, r = 4 * c->u;
    if (r > (x1 - x0) / 2) r = (x1 - x0) / 2;
    if (r > (y1 - y0) / 2) r = (y1 - y0) / 2;
    for (int py = (int)floorf(y0); py < (int)ceilf(y1); py++)
        for (int px = (int)floorf(x0); px < (int)ceilf(x1); px++) {
            float cx = px + 0.5f, cy = py + 0.5f;
            float dx = fmaxf(fmaxf(x0 + r - cx, cx - (x1 - r)), 0), dy = fmaxf(fmaxf(y0 + r - cy, cy - (y1 - r)), 0);
            float d = sqrtf(dx * dx + dy * dy) - r;                 /* < 0 inside the rounded corners */
            float cov = fminf(fminf(cx - x0 + 0.5f, x1 - cx + 0.5f), fminf(cy - y0 + 0.5f, y1 - cy + 0.5f));
            cov = fminf(fmaxf(cov, 0), 1) * fminf(fmaxf(0.5f - d, 0), 1);
            put(px, py, rgb, alpha * cov);
        }
}

/* A thin frame (the game's restyled windows: a grey line). */
static void frame(const JournalCanvas* c, int x, int y, int w, int h, uint32_t rgb) {
    float t = fmaxf(1.0f, 1.2f * c->u);
    int x0 = (int)(c->ox + x * c->u), y0 = (int)(c->oy + y * c->u), x1 = (int)(c->ox + (x + w) * c->u), y1 = (int)(c->oy + (y + h) * c->u), n = (int)(t + 0.5f);
    for (int i = 0; i < n; i++) {
        for (int px = x0; px < x1; px++) { put(px, y0 + i, rgb, 1); put(px, y1 - 1 - i, rgb, 1); }
        for (int py = y0; py < y1; py++) { put(x0 + i, py, rgb, 1); put(x1 - 1 - i, py, rgb, 1); }
    }
}

void journal_bar(const JournalCanvas* c, int x, int y, int w, int h, float part, uint32_t rgb) {
    if (part < 0) part = 0;
    if (part > 1) part = 1;
    journal_box(c, x, y, w, h, 0x3A3F48, 1);
    if (part > 0) journal_box(c, x, y, (int)lroundf(w * part) < 2 ? 2 : (int)lroundf(w * part), h, rgb, 1);
}

int journal_text_width(float px, const char* s) {
    if (!text_ready() || !s || !*s) return 0;
    /* measured at a fixed size (text.c draws, it doesn't measure): scale the width */
    static uint32_t probe[2048 * 64];
    int w = text_draw(probe, 2048, 1, 0, -1000, 64, 0, s);      /* nothing lands in the buffer (y off it) */
    return (int)lroundf(w * px / 64.0f);
}

int journal_text(const JournalCanvas* c, int x, int y, float px, uint32_t rgb, const char* s) {
    if (!s || !*s || !text_ready()) return 0;
    float size = px * c->u;
    int lh = (int)ceilf(size * 1.4f) + 2;
    if (lh > g_scratch_h) { free(g_scratch); g_scratch = malloc(sizeof(uint32_t) * (size_t)g_pw * lh); g_scratch_h = g_scratch ? lh : 0; }
    if (!g_scratch) return 0;
    int X = (int)lroundf(c->ox + x * c->u), Y = (int)lroundf(c->oy + y * c->u);
    memset(g_scratch, 0, sizeof(uint32_t) * (size_t)g_pw * lh);
    int w = text_draw(g_scratch, g_pw, lh, X < 0 ? 0 : X, 0, size, 0xFFFFFF, s);
    int x0 = X < 0 ? 0 : X, x1 = x0 + w + (int)size;
    if (x1 > g_pw) x1 = g_pw;
    for (int j = 0; j < lh; j++)
        for (int i = x0; i < x1; i++) {
            uint32_t t = g_scratch[(size_t)j * g_pw + i];
            if (t) put(i, Y + j, rgb, (float)((t >> 16) & 255) / 255.0f);
        }
    return (int)lroundf(w / c->u);
}

int journal_wrap(const JournalCanvas* c, int x, int y, int max_w, float px, uint32_t rgb, const char* s, int max_lines) {
    int line = (int)lroundf(px * 1.3f);
    char buf[512];
    while (s && *s && max_lines-- > 0) {
        while (*s == ' ') s++;
        const char* fit = NULL, *e = s;
        for (;;) {                                              /* the longest run of whole words that fits */
            const char* w = e;
            while (*w && *w != ' ' && *w != '\n') w++;
            snprintf(buf, sizeof buf, "%.*s", (int)(w - s), s);
            if (fit && journal_text_width(px, buf) > max_w) break;
            fit = w;
            if (!*w || *w == '\n') break;
            e = w + 1;
        }
        if (max_lines == 0 && *fit && *fit != '\n') {             /* the last line allowed: "..." */
            snprintf(buf, sizeof buf, "%.*s...", (int)(fit - s), s);
        } else snprintf(buf, sizeof buf, "%.*s", (int)(fit - s), s);
        journal_text(c, x, y, px, rgb, buf);
        y += line;
        s = fit;
        if (*s == '\n') s++;
    }
    return y;
}

void journal_clean(const char* s, char* out, int cap) { romdata_clean_name(s, out, (size_t)cap); }

/* ---------------------------------------------------------------- the panel */
#define C_PANEL 0x0E1218
#define C_LINE 0x8A8A8A
#define C_TEXT 0xF0F0F0
#define C_DIM 0x9AA0A8
#define C_TAB 0x2A3140

static const char* help(int l) {
    static const char* const t[5] = {
        "Q / W (L / R): change tab     \xE2\x86\x91 \xE2\x86\x93: choose     \xE2\x86\x90 \xE2\x86\x92: who / page     J or Esc (B): close",
        "Q / W (L / R) : onglet     \xE2\x86\x91 \xE2\x86\x93 : choisir     \xE2\x86\x90 \xE2\x86\x92 : personnage / page     J ou \xC3\x89" "chap (B) : fermer",
        "Q / W (L / R): Reiter     \xE2\x86\x91 \xE2\x86\x93: w\xC3\xA4hlen     \xE2\x86\x90 \xE2\x86\x92: Person / Seite     J oder Esc (B): schlie\xC3\x9F" "en",
        "Q / W (L / R): scheda     \xE2\x86\x91 \xE2\x86\x93: scegli     \xE2\x86\x90 \xE2\x86\x92: personaggio / pagina     J o Esc (B): chiudi",
        "Q / W (L / R): pesta\xC3\xB1" "a     \xE2\x86\x91 \xE2\x86\x93: elegir     \xE2\x86\x90 \xE2\x86\x92: personaje / p\xC3\xA1gina     J o Esc (B): cerrar",
    };
    return t[l];
}

const uint8_t* journal_picture(int w, int h, int* changed) {
    ready();
    *changed = 0;
    if (w <= 0 || h <= 0) return NULL;
    if (w != g_pw || h != g_ph || !g_pic) {
        free(g_pic); g_pic = malloc((size_t)w * h * 4);
        free(g_scratch); g_scratch = NULL; g_scratch_h = 0;
        if (!g_pic) { g_pw = g_ph = 0; return NULL; }
        g_pw = w; g_ph = h; g_dirty = 1;
    }
    if (!g_dirty) return g_pic;
    g_dirty = 0; *changed = 1;
    for (size_t i = 0; i < (size_t)w * h; i++) { g_pic[i * 4] = g_pic[i * 4 + 1] = g_pic[i * 4 + 2] = 0; g_pic[i * 4 + 3] = 115; }   /* the game dimmed behind */
    JournalCanvas c = { g_pic, w, h, 0, 0, 0, 0, 0, 0, 0, lang() };
    c.u = fminf((float)w / JOURNAL_W, (float)h / JOURNAL_H);
    c.ox = (w - JOURNAL_W * c.u) / 2; c.oy = (h - JOURNAL_H * c.u) / 2;
    journal_box(&c, 24, 18, JOURNAL_W - 48, JOURNAL_H - 36, C_PANEL, 0.96f);
    frame(&c, 24, 18, JOURNAL_W - 48, JOURNAL_H - 36, C_LINE);
    /* the tabs */
    int x = 48;
    for (int i = 0; i < g_npage; i++) {
        const char* t = g_page[i]->title(c.lang);
        int tw = journal_text_width(22, t) + 36;
        if (i == g_tab) { journal_box(&c, x, 34, tw, 40, C_TAB, 1); journal_box(&c, x + 10, 70, tw - 20, 3, 0xF2C94C, 1); }
        journal_text(&c, x + 18, 40, 22, i == g_tab ? C_TEXT : C_DIM, t);
        x += tw + 8;
    }
    journal_box(&c, 40, 82, JOURNAL_W - 80, 1, C_LINE, 0.6f);
    c.x = 48; c.y = 96; c.w = JOURNAL_W - 96; c.h = JOURNAL_H - 96 - 64;
    if (g_tab < g_npage) g_page[g_tab]->draw(&c);
    journal_box(&c, 40, JOURNAL_H - 62, JOURNAL_W - 80, 1, C_LINE, 0.6f);
    journal_text(&c, 48, JOURNAL_H - 52, 15, C_DIM, help(c.lang));
    return g_pic;
}

/* ---------------------------------------------------------------- the game's texts */
typedef struct { char pack[64], stem[24]; int lang, n; int* id; char** s; } Table;
static Table g_tables[16]; static int g_ntables;

const char* journal_game_string(const char* pack, const char* stem, int id) {
    static const char* const suffix[5] = { "en", "fr", "de", "it", "es" };
    int l = lang();
    Table* t = NULL;
    for (int i = 0; i < g_ntables && !t; i++)
        if (!strcmp(g_tables[i].pack, pack) && !strcmp(g_tables[i].stem, stem)) t = &g_tables[i];
    if (!t) {
        if (g_ntables >= 16) return NULL;
        t = &g_tables[g_ntables++];
        snprintf(t->pack, sizeof t->pack, "%s", pack); snprintf(t->stem, sizeof t->stem, "%s", stem);
        t->lang = -1;
    }
    if (t->lang != l) {
        for (int i = 0; i < t->n; i++) free(t->s[i]);
        free(t->s); free(t->id); t->s = NULL; t->id = NULL; t->n = 0;
        t->lang = l;
        uint32_t n = 0, sz = 0;
        const uint8_t* d = romdata_file(pack, &n);
        char name[48]; snprintf(name, sizeof name, "%s_%s.bin", stem, suffix[l]);
        uint8_t* b = d ? romdata_gpc_file(d, n, name, &sz) : NULL;
        if (!b && d) { snprintf(name, sizeof name, "%s_en.bin", stem); b = romdata_gpc_file(d, n, name, &sz); }
        if (b && sz >= 16) {
            uint32_t end = (uint32_t)(b[4] | b[5] << 8 | b[6] << 16 | (uint32_t)b[7] << 24), p = 16, cap = 0;
            if (end > sz) end = sz;
            Rec r;
            while (romdata_rec_next(b, end, &p, &r)) {
                if (r.tag != 0x67 || r.n < 2 || r.type[1] != 0 || r.v[1] >= sz - end) continue;
                const char* raw = (const char*)b + end + r.v[1];
                size_t len = strnlen(raw, sz - end - r.v[1]);
                char in[1024], out[1024], *o = in;
                for (size_t i = 0; i < len && o < in + sizeof in - 4; i++) {   /* <u_arrow> -> an arrow (clean_name drops codes it doesn't know) */
                    if (!strncmp(raw + i, "<u_arrow>", 9)) { memcpy(o, "\xE2\x86\x91", 3); o += 3; i += 8; }
                    else if (!strncmp(raw + i, "<d_arrow>", 9)) { memcpy(o, "\xE2\x86\x93", 3); o += 3; i += 8; }
                    else *o++ = raw[i];
                }
                *o = 0;
                romdata_clean_name(in, out, sizeof out);
                if ((uint32_t)t->n >= cap) {
                    cap = cap ? cap * 2 : 64;
                    t->id = realloc(t->id, sizeof(int) * cap); t->s = realloc(t->s, sizeof(char*) * cap);
                }
                t->id[t->n] = (int)r.v[0]; t->s[t->n++] = strdup(out);
            }
        }
        free(b);
    }
    for (int i = 0; i < t->n; i++) if (t->id[i] == id) return t->s[i];
    return NULL;
}
