/* Cheats (the Cheats menu): Action Replay DS codes, run once a frame like
 * the cartridge does, each one turned on by its own check mark. All off
 * unless the player turns them on; never while playing with friends (every
 * PC must run the same game).
 *
 * Only codes that change the game's data work here: the game's code is
 * compiled to C (recomp/), so a code that rewrites the game's instructions
 * (walk faster, more experience...) changes bytes nobody runs any more.
 * Those are left out.
 *
 * The codes are the public ones from Almar's Guides (almarsguides.com,
 * Dragon Quest IX > Action Replay, USA and Europe pages). The data they
 * change is at the same address in both versions (only the main code moved,
 * see rom_is_eur), checked in both ROMs' memory with the same save; a code
 * written for one version only says so in its own column. The party
 * versions of the hero's codes repeat them for each member's record, 0x964
 * bytes apart (as the guide's "All Party Members x500 Skill Points" does).
 *
 * A cheat that sets a number (gold, mini medals...) lets the player choose
 * it: VVVVVVVV in its codes is that number, %s in its name shows it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nds.h"
#include "uitext.h"
#include "session.h"
#include "cheats.h"

typedef struct {
    const char* key;      /* settings.ini */
    const char* name;     /* the menu */
    const char* usa;      /* the codes: "XXXXXXXX YYYYYYYY" pairs */
    const char* eur;      /* NULL: the same as the USA's; "": none for this version */
    int max;              /* > 0: the player chooses the number (0..max), first the code's own */
    int def;
} Cheat;

static const Cheat CHEATS[] = {
    { "gold", "Gold: always %s...",
      "020F6D48 VVVVVVVV", NULL, 9999999, 9999999 },
    { "bank", "Gold in the bank: always %s...",
      "020F6D44 VVVVVVVV", NULL, 9999999, 9999999 },
    { "medals", "Mini medals: always %s...",
      "020F6D50 VVVVVVVV", NULL, 999, 999 },
    { "hp", "Party: HP stay full in battle",
      "A20F5510 00000000 DA000000 020F5510 D7000000 020F550C D2000000 00000000 "
      "A20F4BAC 00000000 DA000000 020F4BAC D7000000 020F4BA8 D2000000 00000000 "
      "A20F4248 00000000 DA000000 020F4248 D7000000 020F4244 D2000000 00000000 "
      "A20F38E4 00000000 DA000000 020F38E4 D7000000 020F38E0 D2000000 00000000", NULL },
    { "mp", "Party: MP stay full in battle",
      "A20F5512 00000000 DA000000 020F5512 D7000000 020F550E D2000000 00000000 "
      "A20F4BAE 00000000 DA000000 020F4BAE D7000000 020F4BAA D2000000 00000000 "
      "A20F424A 00000000 DA000000 020F424A D7000000 020F4246 D2000000 00000000 "
      "A20F38E6 00000000 DA000000 020F38E6 D7000000 020F38E2 D2000000 00000000", NULL },
    { "skill", "Party: %s skill points to spend (press Select)...",
      "94000130 FFFB0000 C0000000 00000003 120F3DB0 VVVVVVVV DC000000 00000964 D2000000 00000000", NULL, 999, 500 },
    { "jobs", "Every vocation unlocked",
      "12108AF8 00000FC9", NULL },
    { "tricks", "Every party trick learnt",
      "12108A50 0000FFFF 22108A52 000000FF", NULL },
    { "inn", "Quester's Rest: top level and every upgrade (press Select)",
      "94000130 FFFB0000 120FD764 000003E8 220FD76C 00000006 D2000000 00000000", NULL },
    { "time", "Time of day: Start+Up for day, Start+Down for night",
      "94000130 FFB70000 020F37A4 43520000 D2000000 00000000 "
      "94000130 FF770000 020F37A4 43D20000 D2000000 00000000", NULL },
    { "oneshot", "Monsters: HP to 0, any hit finishes them (press B+Start in battle)",
      "94000130 FFF50000 C0000000 00000007 122A4638 00000000 DC000000 000000A4 D2000000 00000000", NULL },
};
#define N_ALL ((int)(sizeof CHEATS / sizeof CHEATS[0]))

static int g_n = -1;               /* this version's cheats: CHEATS[g_list[i]] */
static int g_list[N_ALL];
static int g_on[N_ALL];
static int g_val[N_ALL];           /* the chosen number (cheats with one) */
static int g_dirty = 1;            /* the codes must be read again (cheats_frame) */

static const char* codes_of(const Cheat* c) { return rom_is_eur() && c->eur ? c->eur : c->usa; }
static void list_init(void) {
    if (g_n >= 0) return;
    g_n = 0;
    for (int i = 0; i < N_ALL; i++) if (*codes_of(&CHEATS[i])) { g_val[g_n] = CHEATS[i].def; g_list[g_n++] = i; }
}
int cheats_european(void) { return rom_is_eur(); }
int cheats_count(void) { list_init(); return g_n; }
const char* cheats_name(int i) {
    static char names[N_ALL][120];
    list_init();
    if (i < 0 || i >= g_n) return "";
    char num[16] = "", grouped[24]; int n = snprintf(num, sizeof num, "%d", g_val[i]), k = 0;
    for (int j = 0; j < n; j++) { if (j && (n - j) % 3 == 0) grouped[k++] = ','; grouped[k++] = num[j]; }   /* 9,999,999 */
    grouped[k] = 0;
    snprintf(names[i], sizeof names[i], T(CHEATS[g_list[i]].name), grouped);   /* in the player's language (uitext.h) */
    return names[i];
}
int cheats_max(int i) { list_init(); return i >= 0 && i < g_n ? CHEATS[g_list[i]].max : 0; }
int cheats_value(int i) { list_init(); return i >= 0 && i < g_n ? g_val[i] : 0; }
void cheats_set_value(int i, int v) {
    list_init();
    if (i < 0 || i >= g_n || v < 0 || v > CHEATS[g_list[i]].max) return;
    g_val[i] = v; g_dirty = 1;
    LOG_INFO("cheat %s: %d\n", CHEATS[g_list[i]].key, v);
}
const char* cheats_key(int i) { list_init(); return i >= 0 && i < g_n ? CHEATS[g_list[i]].key : ""; }
int cheats_on(int i) { list_init(); return i >= 0 && i < g_n && g_on[i]; }
void cheats_set(int i, int on) {
    list_init();
    if (i < 0 || i >= g_n || g_on[i] == !!on) return;
    g_on[i] = !!on;
    LOG_INFO("cheat %s: %s\n", CHEATS[g_list[i]].key, on ? "on" : "off");
}

void cheats_from_list(const char* list) {
    list_init();
    for (int i = 0; i < g_n; i++) {
        const char* k = CHEATS[g_list[i]].key; size_t n = strlen(k); int on = 0;
        for (const char* p = list; (p = strstr(p, k)); p += n)
            if ((p == list || p[-1] == ',' || p[-1] == ' ') && (p[n] == 0 || p[n] == ',' || p[n] == ' ' || p[n] == '=' || p[n] == '\r' || p[n] == '\n')) {
                on = 1;
                if (p[n] == '=') cheats_set_value(i, atoi(p + n + 1));       /* gold=50000 */
                break;
            }
        g_on[i] = on;
    }
}
void cheats_to_list(char* out, int size) {
    list_init();
    int len = 0; out[0] = 0;
    for (int i = 0; i < g_n && len < size; i++)
        if (g_on[i]) len += CHEATS[g_list[i]].max ? snprintf(out + len, size - len, "%s%s=%d", len ? "," : "", CHEATS[g_list[i]].key, g_val[i])
                                                  : snprintf(out + len, size - len, "%s%s", len ? "," : "", CHEATS[g_list[i]].key);
}

/* ---------------------------------------------------------------- the Action Replay DS code types */
/* "XXXXXXXX YYYYYYYY ..." -> words; returns how many (pairs * 2) */
static int parse(const char* s, uint32_t* w, int max) {
    int n = 0;
    while (*s && n < max) {
        char* end; uint32_t v = (uint32_t)strtoul(s, &end, 16);
        if (end == s) { s++; continue; }
        w[n++] = v; s = end;
    }
    return n & ~1;
}

static void run(const uint32_t* code, int n) {
    uint32_t offset = 0, data = 0;
    int cond = 1;                          /* the lines run now */
    uint32_t stack = 0; int depth = 0;     /* the conditions around this one (D0 goes back out) */
    int loop_at = -1; uint32_t loop_left = 0; int loop_cond = 1; uint32_t loop_stack = 0; int loop_depth = 0;
    for (int i = 0; i + 1 < n; i += 2) {
        uint32_t a = code[i], b = code[i + 1];
        uint32_t op = a >> 28, x = a & 0x0FFFFFFF;
        if (op == 0xE) {                    /* EXXXXXXX YYYYYYYY: Y bytes follow, copied to X+offset */
            int words = (int)((b + 7) / 8) * 2;
            if (cond) for (uint32_t k = 0; k < b && i + 2 + (int)(k / 4) < n; k++)
                mem_write8(x + offset + k, (uint8_t)(code[i + 2 + k / 4] >> (8 * (k & 3))));
            i += words;
            continue;
        }
        if (op >= 0x3 && op <= 0xA) {       /* conditions: the lines after run while it holds (until D0/D1/D2) */
            uint32_t addr = x ? x : offset;
            int ok;
            if (op <= 0x6) {
                uint32_t v = mem_read32(addr);
                ok = op == 3 ? b > v : op == 4 ? b < v : op == 5 ? b == v : b != v;
            } else {
                uint32_t v = mem_read16(addr) & ~(b >> 16) & 0xFFFF, want = b & 0xFFFF;
                ok = op == 7 ? want > v : op == 8 ? want < v : op == 9 ? want == v : want != v;
            }
            if (depth < 32) { stack = (stack << 1) | (uint32_t)cond; depth++; }
            cond = cond && ok;
            continue;
        }
        if (a == 0xD0000000) {              /* end of a condition */
            if (depth > 0) { cond = (int)(stack & 1); stack >>= 1; depth--; } else cond = 1;
            continue;
        }
        if (a == 0xD1000000 || a == 0xD2000000) {   /* next turn of the loop; D2 then starts over clean */
            if (loop_at >= 0 && loop_left > 0) { loop_left--; i = loop_at; cond = loop_cond; stack = loop_stack; depth = loop_depth; continue; }
            loop_at = -1;
            if (a == 0xD2000000) { offset = data = 0; cond = 1; stack = 0; depth = 0; }
            else { cond = loop_cond; stack = loop_stack; depth = loop_depth; }
            continue;
        }
        if (!cond) continue;
        switch (op) {
        case 0x0: mem_write32(x + offset, b); break;
        case 0x1: mem_write16(x + offset, (uint16_t)b); break;
        case 0x2: mem_write8(x + offset, (uint8_t)b); break;
        case 0xB: offset = mem_read32(x + offset); break;
        case 0xC:
            if ((a >> 24) == 0xC0) { loop_at = i; loop_left = b; loop_cond = cond; loop_stack = stack; loop_depth = depth; }
            break;
        case 0xD:
            switch (a >> 24) {
            case 0xD3: offset = b; break;
            case 0xD4: data += b; break;
            case 0xD5: data = b; break;
            case 0xD6: mem_write32(b + offset, data); offset += 4; break;
            case 0xD7: mem_write16(b + offset, (uint16_t)data); offset += 2; break;
            case 0xD8: mem_write8(b + offset, (uint8_t)data); offset += 1; break;
            case 0xD9: data = mem_read32(b + offset); break;
            case 0xDA: data = mem_read16(b + offset); break;
            case 0xDB: data = mem_read8(b + offset); break;
            case 0xDC: offset += b; break;
            }
            break;
        case 0xF:                            /* FXXXXXXX YYYYYYYY: copy Y bytes from offset to X */
            for (uint32_t k = 0; k < b; k++) mem_write8(x + k, mem_read8(offset + k));
            break;
        }
    }
}

void cheats_frame(void) {
    static uint32_t words[N_ALL][96]; static int nwords[N_ALL]; static int env;
    list_init();
    if (!env) {
        env = 1;
        const char* e = getenv("DQIX_CHEATS");           /* tests: the keys of the ones to turn on (gold=500...) */
        if (e) cheats_from_list(e);
    }
    if (g_dirty) {                                       /* the codes, with the chosen numbers in them */
        g_dirty = 0;
        for (int i = 0; i < g_n; i++) {
            char text[1024], num[12]; const char* c = codes_of(&CHEATS[g_list[i]]); const char* v;
            snprintf(num, sizeof num, "%08X", (unsigned)g_val[i]);
            snprintf(text, sizeof text, "%s", c);
            while ((v = strstr(text, "VVVVVVVV"))) memcpy(text + (v - text), num, 8);
            nwords[i] = parse(text, words[i], 96);
        }
    }
    if (session_requested()) return;
    for (int i = 0; i < g_n; i++) if (g_on[i]) run(words[i], nwords[i]);
    static uint32_t test[96]; static int ntest = -1;    /* tests: DQIX_CHEAT_CODE="XXXXXXXX YYYYYYYY ...", run after them */
    if (ntest < 0) { const char* e = getenv("DQIX_CHEAT_CODE"); ntest = e ? parse(e, test, 96) : 0; }
    if (ntest) run(test, ntest);
}
