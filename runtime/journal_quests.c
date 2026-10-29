/* Journal > Quests: the quests the game has offered, under way and done, in
 * the game's own words: the request (what the quest list shows), the game's
 * hint only when asked for (H / Y), what came of it once done, and the quest
 * that has to be done first. Nothing about quests the game hasn't offered yet.
 *
 * From the ROM: data/scenario/questidtbl.bin (tag 0x68: the quest, its
 * number), questorder3.bin (tag 0x66: the quest, the giver's place and
 * person, ..., word 6 the quest to do first or -1), questmsg.gp2 (per
 * language, tag 0x67: the number, the name, the request, the request with its
 * hint, the steps, word 11 what came of it); data/menu/str_ql10.gp2 (the
 * quest list's own words). Their state: a nibble per quest at 0x02108B10
 * (as mapmarks.c). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nds.h"
#include "romdata.h"
#include "journal.h"

#define QUESTS 0xCC
#define QUEST_STATE 0x108B10
#define FIELD_PTR 0x0F33E0            /* 0 until an adventure is loaded */

typedef struct { int num, before; char* name; char* ask; char* hint; char* done; } Quest;
static Quest g_q[QUESTS];
static int g_read, g_lang = -1;
static int g_list[QUESTS], g_nlist, g_sel, g_top, g_hint;     /* the quests shown, in order; the chosen one; hint shown */
static int g_count[4];

static uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static int lang(void) { return g_fw_language >= 1 && g_fw_language <= 5 ? g_fw_language - 1 : 0; }

static int state(int q) {                                         /* 0 not offered, 1 offered, 2 under way, 3 done */
    int nib = (g_main_ram[(QUEST_STATE + (q >> 1)) & (MAIN_RAM_SIZE - 1)] >> (q & 1 ? 4 : 0)) & 15;
    return nib & 8 ? 0 : nib & 3;                                /* 8: from the Wi-Fi shop, not downloaded */
}

static void load(void) {
    if (!g_read) {
        g_read = 1;
        for (int q = 0; q < QUESTS; q++) g_q[q].before = -1;
        uint32_t n = 0, p; Rec r;
        const uint8_t* b = romdata_file("data/scenario/questidtbl.bin", &n);
        if (b && n >= 8) {
            uint32_t end = le32(b + 4); if (end > n) end = n;
            for (p = 8; romdata_rec_next(b, end, &p, &r);)
                if (r.tag == 0x68 && r.n >= 2 && r.v[0] < QUESTS && r.v[1] < 255) g_q[r.v[0]].num = (int)r.v[1];
        }
        b = romdata_file("data/scenario/questorder3.bin", &n);
        if (b && n >= 8) {
            uint32_t end = le32(b + 4); if (end > n) end = n;
            for (p = 8; romdata_rec_next(b, end, &p, &r);)
                if (r.tag == 0x66 && r.n >= 7 && r.v[0] < QUESTS && r.v[6] < QUESTS) g_q[r.v[0]].before = (int)r.v[6];
        }
    }
    int l = lang();
    if (l == g_lang) return;
    g_lang = l;
    static const char* const suffix[5] = { "en", "fr", "de", "it", "es" };
    for (int q = 0; q < QUESTS; q++) {
        free(g_q[q].name); free(g_q[q].ask); free(g_q[q].hint); free(g_q[q].done);
        g_q[q].name = g_q[q].ask = g_q[q].hint = g_q[q].done = NULL;
    }
    uint32_t n = 0, sz = 0;
    const uint8_t* pack = romdata_file("data/scenario/questmsg.gp2", &n);
    char fname[32]; snprintf(fname, sizeof fname, "questmsg_%s.bin", suffix[l]);
    uint8_t* b = pack ? romdata_gpc_file(pack, n, fname, &sz) : NULL;
    if (!b && pack) b = romdata_gpc_file(pack, n, "questmsg_en.bin", &sz);
    if (!b || sz < 16) { free(b); return; }
    uint32_t end = le32(b + 4), p = 16; Rec r;
    if (end > sz) end = sz;
    while (romdata_rec_next(b, end, &p, &r)) {
        if (r.tag != 0x67 || r.n < 12 || r.v[0] == 0) continue;
        for (int q = 0; q < QUESTS; q++) {
            if (g_q[q].num != (int)r.v[0]) continue;
            static const int field[4] = { 1, 2, 3, 11 };
            char** dst[4] = { &g_q[q].name, &g_q[q].ask, &g_q[q].hint, &g_q[q].done };
            for (int k = 0; k < 4; k++) {
                int f = field[k];
                if (r.type[f] != 0 || r.v[f] >= sz - end) continue;
                const char* s = (const char*)b + end + r.v[f];
                char out[1200];
                romdata_clean_name(s, out, sizeof out);
                if (out[0]) *dst[k] = strdup(out);
            }
        }
    }
    free(b);
}

static int adventure(void) { uint32_t v; memcpy(&v, &g_main_ram[FIELD_PTR], 4); return v != 0; }

/* The list: under way, then offered, then done; each by number. */
static void make_list(void) {
    g_nlist = 0; memset(g_count, 0, sizeof g_count);
    if (!adventure()) return;
    static const int order[3] = { 2, 1, 3 };
    for (int k = 0; k < 3; k++) {
        int first = g_nlist;
        for (int q = 0; q < QUESTS; q++)
            if (g_q[q].num > 0 && state(q) == order[k]) g_list[g_nlist++] = q;
        for (int i = first + 1; i < g_nlist; i++)                 /* by number */
            for (int j = i; j > first && g_q[g_list[j]].num < g_q[g_list[j - 1]].num; j--) { int t = g_list[j]; g_list[j] = g_list[j - 1]; g_list[j - 1] = t; }
        g_count[order[k]] = g_nlist - first;
    }
    if (g_sel >= g_nlist) g_sel = g_nlist - 1;
    if (g_sel < 0) g_sel = 0;
}

static void opened(void) { load(); make_list(); g_hint = 0; }

static const char* title(int l) {
    const char* t = journal_game_string("data/menu/str_ql10.gp2", "str_ql10", 0);   /* "Quest List" */
    static const char* const own[5] = { "Quests", "Qu\xC3\xAA" "tes", "Auftr\xC3\xA4" "ge", "Missioni", "Misiones" };
    (void)t;
    return own[l];
}

static const char* status_name(int st) {                          /* the game's quest list: Not Yet Accepted, Underway, Complete */
    const char* s = journal_game_string("data/menu/str_ql10.gp2", "str_ql10", st == 1 ? 5 : st == 2 ? 6 : 7);
    static const char* const en[3] = { "Not yet accepted", "Underway", "Complete" };
    return s ? s : en[st == 1 ? 0 : st == 2 ? 1 : 2];
}

static uint32_t status_color(int st) { return st == 2 ? 0x8CC8F2 : st == 1 ? 0x8EE28E : 0xA8A8A8; }

static int key(int k) {
    int rows = 17;
    switch (k) {
    case JK_UP: if (g_sel > 0) { g_sel--; g_hint = 0; return 1; } return 0;
    case JK_DOWN: if (g_sel < g_nlist - 1) { g_sel++; g_hint = 0; return 1; } return 0;
    case JK_PAGE_UP: case JK_LEFT: g_sel = g_sel > rows ? g_sel - rows : 0; g_hint = 0; return 1;
    case JK_PAGE_DOWN: case JK_RIGHT: g_sel = g_sel + rows < g_nlist ? g_sel + rows : g_nlist - 1; if (g_sel < 0) g_sel = 0; g_hint = 0; return 1;
    case JK_MORE: case JK_OK: g_hint = !g_hint; return 1;
    }
    return 0;
}

static void quest_head(int q, char* out, size_t cap) {
    static const char* const number[5] = { "Quest #%d", "Qu\xC3\xAA" "te n\xC2\xB0%d", "Auftrag Nr. %d", "Missione n. %d", "Misi\xC3\xB3n n.\xC2\xBA %d" };
    char h[40]; snprintf(h, sizeof h, number[lang()], g_q[q].num);
    snprintf(out, cap, "%s : %s", h, g_q[q].name ? g_q[q].name : "?");
    if (lang() != 1) { char* c = strstr(out, " : "); if (c) memmove(c, c + 1, strlen(c)); }   /* French puts a space before the colon */
}

static void draw(const JournalCanvas* c) {
    static const char* const none[5] = { "No quest yet.", "Aucune qu\xC3\xAA" "te pour l'instant.", "Noch keine Auftr\xC3\xA4" "ge.", "Ancora nessuna missione.", "Todav\xC3\xAD" "a no hay misiones." };
    static const char* const no_game[5] = { "Load an adventure first.", "Chargez d'abord une aventure.", "Lade zuerst ein Abenteuer.", "Carica prima un'avventura.", "Carga primero una aventura." };
    static const char* const t_ask[5] = { "Request", "Demande", "Auftrag", "Richiesta", "Petici\xC3\xB3n" };
    static const char* const t_hint[5] = { "The game's hint", "Indice du jeu", "Hinweis des Spiels", "Suggerimento del gioco", "Pista del juego" };
    static const char* const t_ask_hint[5] = { "H (Y): show the game's hint (it may tell you how)", "H (Y) : afficher l'indice du jeu (il peut donner la solution)",
        "H (Y): Hinweis des Spiels zeigen (er kann die L\xC3\xB6sung verraten)", "H (Y): mostra il suggerimento del gioco (pu\xC3\xB2 svelare la soluzione)",
        "H (Y): ver la pista del juego (puede revelar la soluci\xC3\xB3n)" };
    static const char* const t_done[5] = { "What came of it", "Ce qui s'est pass\xC3\xA9", "Ergebnis", "Com'\xC3\xA8 andata", "Resultado" };
    static const char* const t_before[5] = { "First: %s", "D'abord : %s", "Zuerst: %s", "Prima: %s", "Antes: %s" };
    static const char* const t_before_ok[5] = { "(done)", "(termin\xC3\xA9" "e)", "(erledigt)", "(completata)", "(completada)" };
    static const char* const t_before_no[5] = { "(not done yet)", "(pas encore termin\xC3\xA9" "e)", "(noch nicht erledigt)", "(non ancora completata)", "(a\xC3\xBAn sin completar)" };
    int l = c->lang;
    load(); make_list();
    if (!adventure()) { journal_text(c, c->x, c->y + 8, 20, 0xC8C8C8, no_game[l]); return; }
    if (!g_nlist) { journal_text(c, c->x, c->y + 8, 20, 0xC8C8C8, none[l]); return; }
    /* the list (left) */
    int lw = 470, row = 30, rows = (c->h - 10) / row, y = c->y;
    if (g_sel < g_top) g_top = g_sel;
    if (g_sel >= g_top + rows - 3) g_top = g_sel - rows + 4;          /* (room for the section titles) */
    if (g_top < 0) g_top = 0;
    int prev_st = -1, shown = 0;
    for (int i = 0; i < g_nlist && y + row <= c->y + c->h; i++) {
        int q = g_list[i], st = state(q);
        if (st != prev_st) {
            prev_st = st;
            if (i >= g_top) {
                char h[96]; snprintf(h, sizeof h, "%s  (%d)", status_name(st), g_count[st]);
                journal_text(c, c->x, y + 2, 17, status_color(st), h);
                journal_box(c, c->x, y + row - 4, lw - 20, 1, status_color(st), 0.5f);
                y += row;
            }
        }
        if (i < g_top) continue;
        char line[160]; quest_head(q, line, sizeof line);
        if (i == g_sel) journal_box(c, c->x - 6, y - 2, lw - 8, row - 2, 0xFFFFFF, 0.12f);
        char cut[160]; snprintf(cut, sizeof cut, "%s", line);
        while (journal_text_width(17, cut) > lw - 40 && strlen(cut) > 4) { size_t n = strlen(cut); do n--; while (n && (cut[n] & 0xC0) == 0x80); strcpy(cut + n, "..."); }
        journal_text(c, c->x + 14, y + 2, 17, st == 3 ? 0xA8A8A8 : 0xF0F0F0, cut);
        y += row; shown++;
    }
    if (g_top > 0) journal_text(c, c->x + lw - 40, c->y - 4, 16, 0x9AA0A8, "\xE2\x96\xB2");
    if (y + row > c->y + c->h && shown < g_nlist - g_top) journal_text(c, c->x + lw - 40, c->y + c->h - 22, 16, 0x9AA0A8, "\xE2\x96\xBC");
    journal_box(c, c->x + lw, c->y, 1, c->h, 0x8A8A8A, 0.5f);
    /* the chosen quest (right) */
    int q = g_list[g_sel], st = state(q), x = c->x + lw + 30, w = c->x + c->w - x;
    char head[200]; quest_head(q, head, sizeof head);
    y = c->y + 4;
    y = journal_wrap(c, x, y, w, 24, 0xF2C94C, head, 2) + 6;
    journal_box(c, x, y, journal_text_width(16, status_name(st)) + 24, 28, status_color(st), 0.25f);
    journal_text(c, x + 12, y + 3, 16, status_color(st), status_name(st));
    y += 44;
    if (g_q[q].before >= 0 && g_q[g_q[q].before].num > 0) {
        int b = g_q[q].before, done = state(b) == 3;
        char bh[200], s[260]; quest_head(b, bh, sizeof bh);
        snprintf(s, sizeof s, t_before[l], bh);
        y = journal_wrap(c, x, y, w, 17, 0xC8C8C8, s, 2);
        journal_text(c, x, y, 16, done ? 0x8EE28E : 0xF2A65A, done ? t_before_ok[l] : t_before_no[l]);
        y += 22;
        y += 14;
    }
    if (st == 3 && g_q[q].done) {
        journal_text(c, x, y, 18, 0x9AA0A8, t_done[l]); y += 30;
        y = journal_wrap(c, x, y, w, 19, 0xF0F0F0, g_q[q].done, 9) + 12;
    } else if (g_q[q].ask) {
        journal_text(c, x, y, 18, 0x9AA0A8, t_ask[l]); y += 30;
        y = journal_wrap(c, x, y, w, 19, 0xF0F0F0, g_q[q].ask, 6) + 16;
        if (st == 2 && g_q[q].hint && strcmp(g_q[q].hint, g_q[q].ask)) {
            if (g_hint) {
                journal_text(c, x, y, 18, 0x9AA0A8, t_hint[l]); y += 30;
                journal_wrap(c, x, y, w, 19, 0xFFE7A8, g_q[q].hint, 8);
            } else journal_text(c, x, y, 16, 0x8CC8F2, t_ask_hint[l]);
        }
    }
}

const JournalPage g_journal_quests = { title, draw, key, opened, NULL };
