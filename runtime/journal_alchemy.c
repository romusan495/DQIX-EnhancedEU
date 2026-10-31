/* The journal's Alchemy and Items pages (journal.h). Alchemy: the recipes
 * the party knows, what each needs and how many of it the party has, what
 * is missing, which ones can be made right now, and what else an item goes
 * into. Items: everything the party owns, by kind, and the known recipes
 * each goes into. Both with a search (typed letters) and filters. Read only,
 * like the rest of the journal.
 *
 * Everything comes from the game:
 *  - data/bin/recipe.gp2 (GPC2, recipe_<lang>.bin, the same in every
 *    language): after a 16-byte header, the game's script records; tag
 *    0x67 is one recipe: word 0 its number (1..471), 1 what it makes, then
 *    up to three (ingredient, how many) pairs (0: none).
 *  - main RAM 0x020FAE9C: a byte per recipe (number - 1): 0 unknown; bit 1
 *    made at least once (the Action Replay codes "all recipes" write 5 for
 *    known, 7 for known and made). The count of things made: 0x020FAE98.
 *  - the bag (main RAM, the same in the USA and European versions): per
 *    kind of item a list of item numbers (u16, 0xFFFF after the last) and
 *    right after it their counts (u8): everyday items at 0x020F5DE8, then
 *    weapons, shields, tops, bottoms, heads, arms, feet, accessories.
 *  - what the party carries (items each, u16, 0xFFFF after the last; their
 *    equipment is apart and not counted): the hero's at 0x020F58CC, the
 *    other three 0x964 bytes apart below.
 *  - the items' names: data/prm/itemname.gp2 (romdata_item_name()). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nds.h"
#include "journal.h"
#include "romdata.h"

#define RECIPES     0x0FAE9C       /* a byte per recipe */
#define CARRIED     0x0F58CC       /* the hero's 12 items; the others' 0x964 lower each */
#define MEMBER_GAP  0x964
#define MAX_RECIPES 512

typedef struct { uint16_t num, makes, ing[3]; uint8_t need[3]; } Recipe;
static Recipe g_rec[MAX_RECIPES];
static int g_nrec = -1;                       /* -1: not read yet */

/* the bag: where each kind's list starts and where its counts start */
static const uint32_t BAG[][2] = {
    { 0x0F5DE8, 0x0F5F18 }, { 0x0F5FB0, 0x0F61D0 }, { 0x0F62E0, 0x0F6340 }, { 0x0F6370, 0x0F64F0 }, { 0x0F65B0, 0x0F6670 },
    { 0x0F66D0, 0x0F67F0 }, { 0x0F6880, 0x0F6920 }, { 0x0F6970, 0x0F6A50 }, { 0x0F6AC0, 0x0F6B40 },
};

static uint16_t rd16(uint32_t a) { uint16_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 1)], 2); return v; }
static uint8_t rd8(uint32_t a) { return g_main_ram[a & (MAIN_RAM_SIZE - 1)]; }

static void load_recipes(void) {
    if (g_nrec >= 0) return;
    g_nrec = 0;
    uint32_t n = 0, sz = 0;
    const uint8_t* pack = romdata_file("data/bin/recipe.gp2", &n);
    uint8_t* b = pack ? romdata_gpc_file(pack, n, "recipe_en.bin", &sz) : NULL;
    if (!b || sz < 16) { free(b); return; }
    uint32_t p = 16; Rec r;
    while (g_nrec < MAX_RECIPES && romdata_rec_next(b, sz, &p, &r)) {
        if (r.tag != 0x67 || r.n < 8 || !r.v[0] || r.v[0] > MAX_RECIPES) continue;
        Recipe* x = &g_rec[g_nrec++];
        x->num = (uint16_t)r.v[0]; x->makes = (uint16_t)r.v[1];
        for (int i = 0; i < 3; i++) { x->ing[i] = (uint16_t)r.v[2 + 2 * i]; x->need[i] = (uint8_t)r.v[3 + 2 * i]; }
    }
    free(b);
}

static int state(const Recipe* x) { return rd8(RECIPES + x->num - 1u); }
static int known(const Recipe* x) { return state(x) != 0; }
static int made(const Recipe* x) { return (state(x) & 2) != 0; }

/* how many of an item the bag holds, and the party carries */
static int in_bag(uint16_t key) {
    for (size_t k = 0; k < sizeof BAG / sizeof BAG[0]; k++) {
        uint32_t ids = BAG[k][0], counts = BAG[k][1], n = (counts - ids) / 2;
        for (uint32_t i = 0; i < n; i++) {
            uint16_t id = rd16(ids + 2 * i);
            if (id == 0xFFFF) break;
            if (id == key) return rd8(counts + i);
        }
    }
    return 0;
}
static int carried(uint16_t key) {
    int c = 0;
    for (int m = 0; m < 4; m++)
        for (int i = 0; i < 12; i++) {
            uint16_t id = rd16(CARRIED - (uint32_t)m * MEMBER_GAP + 2u * (uint32_t)i);
            if (id == 0xFFFF) break;                     /* (after the last one) */
            c += id == key;
        }
    return c;
}
static int owned(uint16_t key) { return in_bag(key) + carried(key); }

/* how many times it can be made now (0: not), and how many ingredients are short */
static int can_make(const Recipe* x, int* short_kinds) {
    int times = 99, miss = 0;
    for (int i = 0; i < 3; i++) {
        if (!x->ing[i] || !x->need[i]) continue;
        int have = owned(x->ing[i]), t = have / x->need[i];
        if (t < times) times = t;
        if (have < x->need[i]) miss++;
    }
    if (short_kinds) *short_kinds = miss;
    return times;
}

/* ---------------------------------------------------------------- texts */
enum { T_TITLE, T_KNOWN, T_MADE, T_FILTER, T_ALL, T_READY, T_ONE, T_NOTMADE, T_SEARCH, T_TYPE, T_NONE, T_NONE_SEARCH,
       T_NEEDS, T_HAVE, T_BAG, T_PARTY, T_CAN, T_CANT, T_GOES, T_NOWHERE, T_RECIPE_OF, T_HINT, T_L_READY, T_L_ONE, T_L_MADE, T_COUNT };
static const char* const TXT[T_COUNT][5] = {
    { "Alchemy", "Alchimie", "Alchemie", "Alchimia", "Alquimia" },
    { "Recipes known", "Recettes connues", "Bekannte Rezepte", "Ricette conosciute", "Recetas conocidas" },
    { "made", "fabriqu\xC3\xA9" "es", "hergestellt", "create", "creadas" },
    { "Show", "Afficher", "Zeigen", "Mostra", "Mostrar" },
    { "all", "toutes", "alle", "tutte", "todas" },
    { "can be made now", "r\xC3\xA9" "alisables maintenant", "jetzt machbar", "realizzabili ora", "posibles ahora" },
    { "one ingredient short", "il manque un ingr\xC3\xA9" "dient", "eine Zutat fehlt", "manca un ingrediente", "falta un ingrediente" },
    { "never made", "jamais fabriqu\xC3\xA9" "es", "nie hergestellt", "mai create", "nunca creadas" },
    { "Search", "Recherche", "Suche", "Cerca", "Buscar" },
    { "type a name", "tapez un nom", "Namen tippen", "digita un nome", "escribe un nombre" },
    { "No recipe known yet: the party learns them from books and people.",
      "Aucune recette connue : l'\xC3\xA9quipe les apprend dans les livres et aupr\xC3\xA8s des gens.",
      "Noch kein Rezept bekannt: Man lernt sie aus B\xC3\xBC" "chern und von Leuten.",
      "Nessuna ricetta conosciuta: si imparano dai libri e dalle persone.",
      "Ninguna receta conocida: se aprenden en libros y de la gente." },
    { "No known recipe matches.", "Aucune recette connue ne correspond.", "Kein bekanntes Rezept passt.", "Nessuna ricetta conosciuta corrisponde.", "Ninguna receta conocida coincide." },
    { "Ingredients", "Ingr\xC3\xA9" "dients", "Zutaten", "Ingredienti", "Ingredientes" },
    { "have", "poss\xC3\xA9" "d\xC3\xA9" "s", "vorhanden", "posseduti", "tienes" },
    { "bag", "sac", "Beutel", "sacca", "bolsa" },
    { "party", "\xC3\xA9" "quipe", "Gruppe", "gruppo", "grupo" },
    { "Can be made now", "R\xC3\xA9" "alisable maintenant", "Jetzt machbar", "Realizzabile ora", "Posible ahora" },
    { "Ingredients short", "Ingr\xC3\xA9" "dients manquants", "Fehlende Zutaten", "Ingredienti mancanti", "Ingredientes que faltan" },
    { "Goes into", "Sert \xC3\xA0 fabriquer", "Wird verwendet f\xC3\xBCr", "Serve per", "Sirve para" },
    { "No other known recipe uses it.", "Aucune autre recette connue ne l'utilise.", "Kein anderes bekanntes Rezept nutzt es.", "Nessun'altra ricetta conosciuta lo usa.", "Ninguna otra receta conocida lo usa." },
    { "known recipe", "recette connue", "bekanntes Rezept", "ricetta conosciuta", "receta conocida" },
    { "Left / Right: what to show     Letters: search     Backspace: erase",
      "Gauche / Droite : quoi afficher     Lettres : recherche     Retour arri\xC3\xA8re : effacer",
      "Links / Rechts: Anzeige     Buchstaben: Suche     R\xC3\xBC" "cktaste: l\xC3\xB6" "schen",
      "Sinistra / Destra: cosa mostrare     Lettere: cerca     Backspace: cancella",
      "Izquierda / Derecha: qu\xC3\xA9 mostrar     Letras: buscar     Retroceso: borrar" },
    { "can be made now", "r\xC3\xA9" "alisable", "machbar", "realizzabile", "posible" },
    { "one ingredient short", "il manque un ingr\xC3\xA9" "dient", "eine Zutat fehlt", "manca un ingrediente", "falta un ingrediente" },
    { "made before", "d\xC3\xA9" "j\xC3\xA0 fabriqu\xC3\xA9" "e", "schon hergestellt", "gi\xC3\xA0 creata", "ya creada" },
};
static int g_lang;
static const char* tx(int t) { return TXT[t][g_lang >= 0 && g_lang < 5 ? g_lang : 0]; }

/* an item's name, its first letter capitalised (the game writes them for sentences) */
static const char* name_of(uint16_t key, char* out, size_t cap) {
    const char* n = romdata_item_name(key);
    if (!n) { snprintf(out, cap, "#%u", key); return out; }
    snprintf(out, cap, "%s", n);
    if (out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 32);
    else if ((uint8_t)out[0] == 0xC3 && (uint8_t)out[1] >= 0xA0 && (uint8_t)out[1] <= 0xBE) out[1] = (char)(out[1] - 0x20);   /* é -> É */
    return out;
}

/* ---------------------------------------------------------------- the list */
enum { F_ALL, F_READY, F_ONE, F_NOTMADE, F_COUNT };
static int g_filter, g_sel, g_top;
static char g_search[64];
static int g_rows[MAX_RECIPES], g_nrows, g_nknown, g_nmade;

/* ASCII letters in lower case, accented Latin letters as their plain letter: for the search */
static void fold(const char* s, char* out, size_t cap) {
    size_t o = 0;
    while (*s && o + 1 < cap) {
        unsigned char c = (unsigned char)*s++;
        if (c == 0xC3 && *s) {
            unsigned char d = (unsigned char)*s++ | 0x20;      /* the lower case letter */
            static const char* const plain = "aaaaaaaceeeeiiiidnooooo/ouuuuyty";
            c = d >= 0xA0 && d <= 0xBF ? (unsigned char)plain[d - 0xA0] : '?';
        } else if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
        else if (c >= 0x80) continue;
        out[o++] = (char)c;
    }
    out[o] = 0;
}

static int matches(const Recipe* x) {
    if (!g_search[0]) return 1;
    char want[64], n[96], f[96];
    fold(g_search, want, sizeof want);
    uint16_t keys[4] = { x->makes, x->ing[0], x->ing[1], x->ing[2] };
    for (int i = 0; i < 4; i++) {
        if (!keys[i]) continue;
        fold(name_of(keys[i], n, sizeof n), f, sizeof f);
        if (strstr(f, want)) return 1;
    }
    return 0;
}

static int by_name(const void* a, const void* b) {
    char na[96], nb[96], fa[96], fb[96];
    fold(name_of(g_rec[*(const int*)a].makes, na, sizeof na), fa, sizeof fa);
    fold(name_of(g_rec[*(const int*)b].makes, nb, sizeof nb), fb, sizeof fb);
    return strcmp(fa, fb);
}

static void make_rows(void) {
    load_recipes();
    g_nrows = g_nknown = g_nmade = 0;
    for (int i = 0; i < g_nrec; i++) {
        const Recipe* x = &g_rec[i];
        if (!known(x)) continue;
        g_nknown++; g_nmade += made(x);
        int miss = 0, times = can_make(x, &miss);
        int keep = g_filter == F_ALL || (g_filter == F_READY && times > 0) || (g_filter == F_ONE && miss == 1)
                || (g_filter == F_NOTMADE && !made(x));
        if (keep && matches(x)) g_rows[g_nrows++] = i;
    }
    qsort(g_rows, (size_t)g_nrows, sizeof g_rows[0], by_name);
    if (g_sel >= g_nrows) g_sel = g_nrows - 1;
    if (g_sel < 0) g_sel = 0;
}

/* ---------------------------------------------------------------- drawing */
#define C_TEXT   0xFFFFFF
#define C_DIM    0x9AA2BC
#define C_GOLD   0xF9DB80
#define C_OK     0x8CF29A
#define C_SHORT  0xFFB45E
#define C_NO     0xFF7B7B
#define C_SELBOX 0xFFFFFF

static int g_vis = 10;                     /* rows shown (set when drawn) */

/* which lines the list shows, under it at its right edge x, when not all of them */
static void more(const JournalCanvas* c, int x, int y, int first, int shown, int all) {
    if (shown >= all) return;
    char s[64]; snprintf(s, sizeof s, "%d-%d / %d", first + 1, first + shown, all);
    journal_text(c, x - 30 - journal_text_width(17, s), y, 17, C_DIM, s);
}

/* the dots' meaning, at the bottom right */
static void legend(const JournalCanvas* c, int right, int y) {
    const struct { uint32_t c; int t; } k[3] = { { C_OK, T_L_READY }, { C_SHORT, T_L_ONE }, { C_GOLD, T_L_MADE } };
    int x = right;
    for (int i = 2; i >= 0; i--) {
        x -= journal_text_width(17, tx(k[i].t));
        journal_text(c, x, y, 17, C_DIM, tx(k[i].t));
        x -= 18; journal_box(c, x, y + 6, 10, 10, k[i].c, 1.0f); x -= 26;
    }
}

static void draw(const JournalCanvas* c) {
    g_lang = c->lang;
    make_rows();
    int x0 = c->x, y0 = c->y, w = c->w, h = c->h;
    float px = 22, small = 17;
    int lh = 34, line = 0;
    char s[256], n[96];

    /* the head: counts, what is shown, the search */
    snprintf(s, sizeof s, "%s : %d / %d     %s : %d", tx(T_KNOWN), g_nknown, g_nrec > 0 ? g_nrec : 0, tx(T_MADE), g_nmade);
    if (g_lang == 0 || g_lang == 2) snprintf(s, sizeof s, "%s: %d / %d     %s: %d", tx(T_KNOWN), g_nknown, g_nrec > 0 ? g_nrec : 0, tx(T_MADE), g_nmade);
    journal_text(c, x0, y0, small, C_DIM, s);
    static const int FT[F_COUNT] = { T_ALL, T_READY, T_ONE, T_NOTMADE };
    snprintf(s, sizeof s, "%s%s <  %s  >", tx(T_FILTER), g_lang == 1 ? " :" : ":", tx(FT[g_filter]));
    journal_text(c, x0, y0 + 26, px, C_GOLD, s);
    int sx = x0 + w * 46 / 100 + 20;
    snprintf(s, sizeof s, "%s%s %s", tx(T_SEARCH), g_lang == 1 ? " :" : ":", g_search[0] ? g_search : tx(T_TYPE));
    int sw = journal_text_width(px, s) + 24;
    journal_box(c, sx - 12, y0 + 20, sw, 36, 0xFFFFFF, 0.08f);
    journal_text(c, sx, y0 + 26, px, g_search[0] ? C_TEXT : C_DIM, s);

    int top = y0 + 70, bottom = y0 + h - 30, lw = w * 46 / 100;
    if (!g_nrows) {
        journal_wrap(c, x0, top + 10, w, px, C_DIM, tx(g_nknown ? T_NONE_SEARCH : T_NONE), 3);
        journal_text(c, x0, bottom + 6, small, C_DIM, tx(T_HINT));
        return;
    }

    /* the list */
    g_vis = (bottom - top - 24) / lh;
    if (g_vis < 1) g_vis = 1;
    if (g_sel < g_top) g_top = g_sel;
    if (g_sel >= g_top + g_vis) g_top = g_sel - g_vis + 1;
    if (g_top > g_nrows - g_vis) g_top = g_nrows - g_vis;
    if (g_top < 0) g_top = 0;
    for (int k = 0; k < g_vis && g_top + k < g_nrows; k++, line++) {
        const Recipe* x = &g_rec[g_rows[g_top + k]];
        int y = top + k * lh, miss = 0, times = can_make(x, &miss);
        if (g_top + k == g_sel) journal_box(c, x0 - 8, y - 4, lw, lh - 2, C_SELBOX, 0.12f);
        uint32_t dot = times > 0 ? C_OK : miss == 1 ? C_SHORT : 0x5A607A;
        journal_box(c, x0 + 2, y + 9, 12, 12, dot, 1.0f);
        name_of(x->makes, n, sizeof n);
        int nw = journal_text(c, x0 + 26, y, px, times > 0 ? C_TEXT : 0xD8DCEA, n);
        if (made(x)) journal_box(c, x0 + 26 + nw + 10, y + 11, 8, 8, C_GOLD, 1.0f);   /* made before */
        if (times > 0) { snprintf(s, sizeof s, "\xC3\x97%d", times); journal_text(c, x0 + lw - 30 - journal_text_width(small, s), y + 4, small, C_OK, s); }
    }
    more(c, x0 + lw, top + g_vis * lh, g_top, g_vis < g_nrows ? g_vis : g_nrows, g_nrows);

    /* the chosen recipe */
    const Recipe* x = &g_rec[g_rows[g_sel]];
    int dx = x0 + lw + 20, dw = w - lw - 20, y = top;
    journal_box(c, dx - 14, top - 8, dw + 8, bottom - top + 8, 0x000000, 0.18f);
    name_of(x->makes, n, sizeof n);
    journal_text(c, dx, y, 28, C_GOLD, n); y += 44;
    int miss = 0, times = can_make(x, &miss);
    if (times > 0) snprintf(s, sizeof s, "%s  \xC3\x97%d", tx(T_CAN), times);
    else snprintf(s, sizeof s, "%s%s %d", tx(T_CANT), g_lang == 1 ? " :" : ":", miss);
    journal_text(c, dx, y, px, times > 0 ? C_OK : C_NO, s); y += 40;
    journal_text(c, dx, y, small, C_DIM, tx(T_NEEDS)); y += 28;
    for (int i = 0; i < 3; i++) {
        if (!x->ing[i] || !x->need[i]) continue;
        int bag = in_bag(x->ing[i]), party = carried(x->ing[i]), have = bag + party;
        name_of(x->ing[i], n, sizeof n);
        snprintf(s, sizeof s, "%d \xC3\x97 %s", x->need[i], n);
        journal_text(c, dx + 10, y, px, C_TEXT, s);
        snprintf(s, sizeof s, "%d / %d", have, x->need[i]);
        journal_text(c, dx + dw - 30 - journal_text_width(px, s), y, px, have >= x->need[i] ? C_OK : C_NO, s);
        y += 28;
        if (party) snprintf(s, sizeof s, "%s %d, %s %d", tx(T_BAG), bag, tx(T_PARTY), party);
        else snprintf(s, sizeof s, "%s %d", tx(T_BAG), bag);
        /* an ingredient short that a known recipe makes */
        for (int j = 0; have < x->need[i] && j < g_nrec; j++)
            if (g_rec[j].makes == x->ing[i] && known(&g_rec[j])) { size_t l = strlen(s); snprintf(s + l, sizeof s - l, "  (%s)", tx(T_RECIPE_OF)); break; }
        journal_text(c, dx + 34, y, small, C_DIM, s);
        y += 34;
    }
    y += 10;
    journal_text(c, dx, y, small, C_DIM, tx(T_GOES)); y += 28;
    int uses = 0;
    for (int j = 0; j < g_nrec && y < bottom - 26; j++) {
        const Recipe* u = &g_rec[j];
        if (!known(u) || (u->ing[0] != x->makes && u->ing[1] != x->makes && u->ing[2] != x->makes)) continue;
        name_of(u->makes, n, sizeof n);
        journal_text(c, dx + 10, y, px, C_TEXT, n);
        y += 28; uses++;
    }
    if (!uses) journal_text(c, dx + 10, y, px, C_DIM, tx(T_NOWHERE));
    journal_text(c, x0, bottom + 6, small, C_DIM, tx(T_HINT));
    legend(c, x0 + w, bottom + 6);
}

/* ---------------------------------------------------------------- input */
static int key(int k) {
    int before = g_sel, f = g_filter;
    switch (k) {
    case JK_UP: if (g_sel > 0) g_sel--; break;
    case JK_DOWN: if (g_sel + 1 < g_nrows) g_sel++; break;
    case JK_PAGE_UP: g_sel -= g_vis; if (g_sel < 0) g_sel = 0; break;
    case JK_PAGE_DOWN: g_sel += g_vis; if (g_sel >= g_nrows) g_sel = g_nrows - 1; if (g_sel < 0) g_sel = 0; break;
    case JK_LEFT: g_filter = (g_filter + F_COUNT - 1) % F_COUNT; break;
    case JK_RIGHT: g_filter = (g_filter + 1) % F_COUNT; break;
    default: return 0;
    }
    if (f != g_filter) g_sel = g_top = 0;
    return before != g_sel || f != g_filter;
}

/* letters typed into a search; "\b" erases the last letter */
static void edit(char* buf, size_t cap, const char* utf8) {
    size_t l = strlen(buf);
    for (const char* p = utf8; *p; p++) {
        if (*p == '\b') {
            while (l > 0 && ((unsigned char)buf[l - 1] & 0xC0) == 0x80) l--;   /* a whole UTF-8 letter */
            if (l > 0) l--;
            buf[l] = 0;
        } else if ((unsigned char)*p >= 0x20 && l + 1 < cap) {
            buf[l++] = *p; buf[l] = 0;
        }
    }
}
static int text(const char* utf8) {
    edit(g_search, sizeof g_search, utf8);
    g_sel = g_top = 0;
    return 1;
}

static void opened(void) { load_recipes(); }
static const char* title(int lang) { return TXT[T_TITLE][lang >= 0 && lang < 5 ? lang : 0]; }

static const JournalPage PAGE = { title, draw, key, opened, text };

/* ---------------------------------------------------------------- the Items page: what the party owns */
enum { I_TITLE, I_OWNED, I_NONE, I_NONE_SEARCH, I_MAKES_IT, I_HINT, I_COUNT };
static const char* const ITXT[I_COUNT][5] = {
    { "Items", "Objets", "Gegenst\xC3\xA4" "nde", "Oggetti", "Objetos" },
    { "Items owned", "Objets poss\xC3\xA9" "d\xC3\xA9" "s", "Gegenst\xC3\xA4" "nde im Besitz", "Oggetti posseduti", "Objetos que tienes" },
    { "The bag is empty.", "Le sac est vide.", "Der Beutel ist leer.", "La sacca \xC3\xA8 vuota.", "La bolsa est\xC3\xA1 vac\xC3\xAD" "a." },
    { "Nothing owned matches.", "Aucun objet poss\xC3\xA9" "d\xC3\xA9 ne correspond.", "Nichts im Besitz passt.", "Nessun oggetto posseduto corrisponde.", "Ning\xC3\xBAn objeto que tienes coincide." },
    { "Made by a known recipe", "Une recette connue le fabrique", "Ein bekanntes Rezept stellt es her", "Una ricetta conosciuta lo crea", "Una receta conocida lo crea" },
    { "Left / Right: kind     Letters: search     Backspace: erase",
      "Gauche / Droite : type     Lettres : recherche     Retour arri\xC3\xA8re : effacer",
      "Links / Rechts: Art     Buchstaben: Suche     R\xC3\xBC" "cktaste: l\xC3\xB6" "schen",
      "Sinistra / Destra: tipo     Lettere: cerca     Backspace: cancella",
      "Izquierda / Derecha: tipo     Letras: buscar     Retroceso: borrar" },
};
#define N_KINDS (int)(sizeof BAG / sizeof BAG[0])
static const char* const KIND[N_KINDS + 1][5] = {   /* the bag's kinds, in its order; then all of them */
    { "everyday items", "objets courants", "Alltagsgegenst\xC3\xA4" "nde", "oggetti comuni", "objetos comunes" },
    { "weapons", "armes", "Waffen", "armi", "armas" },
    { "shields", "boucliers", "Schilde", "scudi", "escudos" },
    { "torso", "torse", "Oberk\xC3\xB6rper", "busto", "torso" },
    { "legs", "jambes", "Beine", "gambe", "piernas" },
    { "head", "t\xC3\xAA" "te", "Kopf", "testa", "cabeza" },
    { "arms", "bras", "Arme", "braccia", "brazos" },
    { "feet", "pieds", "F\xC3\xBC\xC3\x9F" "e", "piedi", "pies" },
    { "accessories", "accessoires", "Accessoires", "accessori", "accesorios" },
    { "everything", "tout", "alles", "tutto", "todo" },
};
static const char* itx(int t) { return ITXT[t][g_lang >= 0 && g_lang < 5 ? g_lang : 0]; }
static const char* kind_name(int k) { return KIND[k][g_lang >= 0 && g_lang < 5 ? g_lang : 0]; }

#define MAX_OWNED 1200
typedef struct { uint16_t key; uint8_t kind; int bag, party; } Owned;
static Owned g_own[MAX_OWNED];
static int g_nown, g_kind = N_KINDS, g_isel, g_itop, g_ivis = 10;
static char g_isearch[64];

static int by_item_name(const void* a, const void* b) {
    char na[96], nb[96], fa[96], fb[96];
    fold(name_of(((const Owned*)a)->key, na, sizeof na), fa, sizeof fa);
    fold(name_of(((const Owned*)b)->key, nb, sizeof nb), fb, sizeof fb);
    return strcmp(fa, fb);
}

/* everything in the bag and in the party's hands, of the kind shown, matching the search */
static void make_owned(void) {
    char want[64], n[96], f[96];
    fold(g_isearch, want, sizeof want);
    g_nown = 0;
    for (int k = 0; k < N_KINDS; k++) {
        uint32_t ids = BAG[k][0], counts = BAG[k][1], cnt = (counts - ids) / 2;
        for (uint32_t i = 0; i < cnt && g_nown < MAX_OWNED; i++) {
            uint16_t id = rd16(ids + 2 * i);
            if (id == 0xFFFF) break;
            g_own[g_nown++] = (Owned){ id, (uint8_t)k, rd8(counts + i), 0 };
        }
    }
    for (int m = 0; m < 4; m++)                              /* carried: added to the bag's line, or a line of their own */
        for (int i = 0; i < 12; i++) {
            uint16_t id = rd16(CARRIED - (uint32_t)m * MEMBER_GAP + 2u * (uint32_t)i);
            if (id == 0xFFFF) break;
            int j;
            for (j = 0; j < g_nown && g_own[j].key != id; j++) {}
            if (j < g_nown) g_own[j].party++;
            else if (g_nown < MAX_OWNED && romdata_item_name(id)) g_own[g_nown++] = (Owned){ id, 0, 0, 1 };
        }
    int o = 0;
    for (int i = 0; i < g_nown; i++) {
        if (g_kind < N_KINDS && g_own[i].kind != g_kind) continue;
        if (want[0]) { fold(name_of(g_own[i].key, n, sizeof n), f, sizeof f); if (!strstr(f, want)) continue; }
        g_own[o++] = g_own[i];
    }
    g_nown = o;
    qsort(g_own, (size_t)g_nown, sizeof g_own[0], by_item_name);
    if (g_isel >= g_nown) g_isel = g_nown - 1;
    if (g_isel < 0) g_isel = 0;
}

static void items_draw(const JournalCanvas* c) {
    g_lang = c->lang;
    load_recipes();
    make_owned();
    int x0 = c->x, y0 = c->y, w = c->w, h = c->h;
    float px = 22, small = 17;
    int lh = 34;
    char s[256], n[96];
    const char* colon = g_lang == 1 ? " :" : ":";
    snprintf(s, sizeof s, "%s%s %d", itx(I_OWNED), colon, g_nown);
    journal_text(c, x0, y0, small, C_DIM, s);
    snprintf(s, sizeof s, "%s%s <  %s  >", tx(T_FILTER), colon, kind_name(g_kind));
    journal_text(c, x0, y0 + 26, px, C_GOLD, s);
    int sx = x0 + w * 46 / 100 + 20;
    snprintf(s, sizeof s, "%s%s %s", tx(T_SEARCH), colon, g_isearch[0] ? g_isearch : tx(T_TYPE));
    journal_box(c, sx - 12, y0 + 20, journal_text_width(px, s) + 24, 36, 0xFFFFFF, 0.08f);
    journal_text(c, sx, y0 + 26, px, g_isearch[0] ? C_TEXT : C_DIM, s);

    int top = y0 + 70, bottom = y0 + h - 30, lw = w * 46 / 100;
    if (!g_nown) {
        journal_wrap(c, x0, top + 10, w, px, C_DIM, itx(g_isearch[0] || g_kind < N_KINDS ? I_NONE_SEARCH : I_NONE), 3);
        journal_text(c, x0, bottom + 6, small, C_DIM, itx(I_HINT));
        return;
    }
    g_ivis = (bottom - top - 24) / lh;
    if (g_ivis < 1) g_ivis = 1;
    if (g_isel < g_itop) g_itop = g_isel;
    if (g_isel >= g_itop + g_ivis) g_itop = g_isel - g_ivis + 1;
    if (g_itop > g_nown - g_ivis) g_itop = g_nown - g_ivis;
    if (g_itop < 0) g_itop = 0;
    for (int k = 0; k < g_ivis && g_itop + k < g_nown; k++) {
        const Owned* o = &g_own[g_itop + k];
        int y = top + k * lh;
        if (g_itop + k == g_isel) journal_box(c, x0 - 8, y - 4, lw, lh - 2, C_SELBOX, 0.12f);
        journal_text(c, x0 + 6, y, px, C_TEXT, name_of(o->key, n, sizeof n));
        snprintf(s, sizeof s, "%d", o->bag + o->party);
        journal_text(c, x0 + lw - 30 - journal_text_width(px, s), y, px, C_TEXT, s);
    }
    more(c, x0 + lw, top + g_ivis * lh, g_itop, g_ivis < g_nown ? g_ivis : g_nown, g_nown);

    /* the chosen item */
    const Owned* o = &g_own[g_isel];
    int dx = x0 + lw + 20, dw = w - lw - 20, y = top;
    journal_box(c, dx - 14, top - 8, dw + 8, bottom - top + 8, 0x000000, 0.18f);
    journal_text(c, dx, y, 28, C_GOLD, name_of(o->key, n, sizeof n)); y += 44;
    if (o->bag && o->party) snprintf(s, sizeof s, "%s %d, %s %d", tx(T_BAG), o->bag, tx(T_PARTY), o->party);
    else if (o->party) snprintf(s, sizeof s, "%s %d", tx(T_PARTY), o->party);
    else snprintf(s, sizeof s, "%s %d", tx(T_BAG), o->bag);
    journal_text(c, dx, y, px, C_TEXT, s); y += 40;
    for (int j = 0; j < g_nrec; j++)                         /* a known recipe that makes it */
        if (g_rec[j].makes == o->key && known(&g_rec[j])) { journal_text(c, dx, y, small, C_OK, itx(I_MAKES_IT)); y += 34; break; }
    journal_text(c, dx, y, small, C_DIM, tx(T_GOES)); y += 28;
    int uses = 0;
    for (int j = 0; j < g_nrec && y < bottom - 26; j++) {
        const Recipe* u = &g_rec[j];
        int need = 0;
        for (int i = 0; i < 3; i++) if (u->ing[i] == o->key) need = u->need[i];
        if (!known(u) || !need) continue;
        snprintf(s, sizeof s, "%s  (\xC3\x97%d)", name_of(u->makes, n, sizeof n), need);
        journal_text(c, dx + 10, y, px, can_make(u, NULL) > 0 ? C_OK : C_TEXT, s);
        y += 28; uses++;
    }
    if (!uses) journal_text(c, dx + 10, y, px, C_DIM, tx(T_NOWHERE));
    journal_text(c, x0, bottom + 6, small, C_DIM, itx(I_HINT));
}

static int items_key(int k) {
    int before = g_isel, f = g_kind;
    switch (k) {
    case JK_UP: if (g_isel > 0) g_isel--; break;
    case JK_DOWN: if (g_isel + 1 < g_nown) g_isel++; break;
    case JK_PAGE_UP: g_isel -= g_ivis; if (g_isel < 0) g_isel = 0; break;
    case JK_PAGE_DOWN: g_isel += g_ivis; if (g_isel >= g_nown) g_isel = g_nown - 1; if (g_isel < 0) g_isel = 0; break;
    case JK_LEFT: g_kind = (g_kind + N_KINDS) % (N_KINDS + 1); break;
    case JK_RIGHT: g_kind = (g_kind + 1) % (N_KINDS + 1); break;
    default: return 0;
    }
    if (f != g_kind) g_isel = g_itop = 0;
    return before != g_isel || f != g_kind;
}

static int items_text(const char* utf8) {
    edit(g_isearch, sizeof g_isearch, utf8);
    g_isel = g_itop = 0;
    return 1;
}
static const char* items_title(int lang) { return ITXT[I_TITLE][lang >= 0 && lang < 5 ? lang : 0]; }

static const JournalPage ITEMS = { items_title, items_draw, items_key, opened, items_text };

__attribute__((constructor)) static void add_pages(void) { journal_add_page(&PAGE); journal_add_page(&ITEMS); }   /* (the journal lists them from the start) */
