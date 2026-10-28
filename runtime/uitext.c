/* The PC interface's texts in the player's language (uitext.h).
 *
 * lang/<language>.txt, UTF-8, is a list of pairs:
 *
 *     en = &Pause\tP
 *     fr = &Pause\tP
 *
 * the English text (exactly as in the code), then its translation on the next
 * line; \n is a new line, \t a tab (menus: before the shortcut), \\ a
 * backslash. Lines starting with # and blank lines are skipped. */
#include "uitext.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

const char* const UI_LANG_KEY[UI_LANGS + 1] = { "", "english", "french", "german", "italian", "spanish" };

typedef struct { char* en; char* tr; } Pair;
static Pair* g_tab;                  /* open addressing, g_cap a power of 2 */
static unsigned g_cap, g_n;
static int g_lang = 1;

static uint32_t hash(const char* s) {                       /* FNV-1a */
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

static void clear(void) {
    for (unsigned i = 0; i < g_cap; i++) { free(g_tab[i].en); free(g_tab[i].tr); }
    free(g_tab); g_tab = NULL; g_cap = g_n = 0;
}

static void put(char* en, char* tr) {
    if ((g_n + 1) * 2 > g_cap) {                             /* grow, at most half full */
        unsigned cap = g_cap ? g_cap * 2 : 1024;
        Pair* t = calloc(cap, sizeof *t);
        if (!t) { free(en); free(tr); return; }
        for (unsigned i = 0; i < g_cap; i++)
            if (g_tab[i].en) { unsigned j = hash(g_tab[i].en) & (cap - 1); while (t[j].en) j = (j + 1) & (cap - 1); t[j] = g_tab[i]; }
        free(g_tab); g_tab = t; g_cap = cap;
    }
    unsigned j = hash(en) & (g_cap - 1);
    while (g_tab[j].en) {
        if (!strcmp(g_tab[j].en, en)) { free(g_tab[j].tr); g_tab[j].tr = tr; free(en); return; }   /* the last one wins */
        j = (j + 1) & (g_cap - 1);
    }
    g_tab[j].en = en; g_tab[j].tr = tr; g_n++;
}

/* the printf conversions of a text, in order ("%d", "%s"...: the letters) */
static void conversions(const char* s, char* out, size_t n) {
    size_t k = 0;
    for (; *s; s++) {
        if (*s != '%') continue;
        s++;
        if (*s == '%') continue;
        while (*s && strchr("-+ #0123456789.*hlLzjt", *s)) { if (*s == '*' && k + 1 < n) out[k++] = '*'; s++; }
        if (!*s) break;
        if (k + 1 < n) out[k++] = *s;
    }
    out[k] = 0;
}

static char* unescape(const char* s) {
    size_t n = strlen(s);
    char* o = malloc(n + 1);
    if (!o) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\\' && i + 1 < n) {
            char c = s[++i];
            o[k++] = c == 'n' ? '\n' : c == 't' ? '\t' : c;
        } else o[k++] = s[i];
    }
    o[k] = 0;
    return o;
}

/* "xx = text": the text, or NULL */
static const char* value(char* line, const char* code) {
    size_t n = strlen(line);
    while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
    if (code && strncmp(line, code, 2)) return NULL;
    if (!code && (line[0] == '#' || line[0] == 0 || !strncmp(line, "en", 2))) return NULL;
    if (strncmp(line + 2, " = ", 3)) return NULL;
    return line + 5;
}

void ui_lang_load(int lang) {
    if (lang < 1 || lang > UI_LANGS) lang = 1;
    clear();
    g_lang = lang;
    if (lang == 1) return;
    char path[64];
    snprintf(path, sizeof path, "lang/%s.txt", UI_LANG_KEY[lang]);
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "uitext: no %s, the menus stay in English\n", path); return; }
    static char line[16384];
    char* en = NULL;
    int lineno = 0, bad = 0;
    while (fgets(line, sizeof line, f)) {
        lineno++;
        if (lineno == 1 && !memcmp(line, "\xEF\xBB\xBF", 3)) memmove(line, line + 3, strlen(line + 3) + 1);   /* a BOM */
        const char* v;
        if ((v = value(line, "en"))) { free(en); en = unescape(v); continue; }
        if (!en || !(v = value(line, NULL))) continue;
        char* tr = unescape(v);
        char a[64], b[64];
        conversions(en, a, sizeof a);
        if (tr) conversions(tr, b, sizeof b);
        if (!tr || !tr[0] || strcmp(a, b)) {                 /* empty, or %d / %s not the same: not used */
            if (tr && tr[0]) { fprintf(stderr, "uitext: %s line %d: not the same %%-conversions as the English, skipped\n", path, lineno); bad++; }
            free(tr); free(en); en = NULL; continue;
        }
        put(en, tr); en = NULL;
    }
    free(en);
    fclose(f);
    fprintf(stderr, "uitext: %s, %u texts%s\n", path, g_n, bad ? " (some skipped)" : "");
}

int ui_lang(void) { return g_lang; }

const char* ui_tr(const char* en) {
    if (!g_n || !en) return en;
    unsigned j = hash(en) & (g_cap - 1);
    while (g_tab[j].en) {
        if (!strcmp(g_tab[j].en, en)) return g_tab[j].tr;
        j = (j + 1) & (g_cap - 1);
    }
    return en;
}
