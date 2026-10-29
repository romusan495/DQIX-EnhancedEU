/* Journal > Progress and Journal > Vocations: the party as the game keeps it,
 * easier to read: each one's vocation and level, attributes, skill points
 * (where they went, what comes next and at how many points), and every
 * vocation side by side at a level (its attributes from the game's own
 * tables, and its skills). Read only.
 *
 * The game's memory: the party's four records, 0x964 bytes apart from
 * 0x020F3888 (the leader the fourth): the name at +0, the attributes as the
 * Attributes page shows them at +0x30 (u16: max HP, max MP, attack, defence,
 * agility), each vocation's experience at +0xFC (u32 x 13) and level at
 * +0x130 (u16 x 13), the points put in each skill at +0x428 + the skill
 * (u8, skills 1..26), the points not given out yet at +0x528 (u16), the
 * vocation at +0x914. Their order on screen: the names at 0x020FB258, 0x34
 * apart. The vocations' skills: 5 bytes per vocation in the game's code
 * (found by their values). Checked against the game's Attributes page.
 *
 * The ROM: data/prm/level<vocation>.bin (tag 0x66 per level: the experience
 * needed, strength, resilience, agility, deftness, charm, magical might,
 * magical mending, max HP, max MP, the skill points earnt so far),
 * data/prm/sklname.gp2 (tag 0x66 per skill step: its number, name, ability
 * or trait, the skill, the points), data/bin/menu/sta_skl.gp2 (the steps'
 * short names) and str_sta.gp2 (the vocations', skills' and attributes'
 * names). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nds.h"
#include "romdata.h"
#include "journal.h"

#define PARTY       0x0F3888
#define MEMBER      0x964
#define NAMES       0x0FB258
#define VOCS        13                 /* Guardian (the start), then the twelve */
#define SKILLS      26
#define FIELD_PTR   0x0F33E0

static uint32_t rd32(uint32_t a) { uint32_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 1)], 4); return v; }
static uint16_t rd16(uint32_t a) { uint16_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 1)], 2); return v; }
static uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

/* ---------------------------------------------------------------- the game's tables */
typedef struct { uint32_t exp; uint16_t st[9]; uint16_t pts; } Level;   /* st: str res agi dex charm might mending hp mp */
static Level g_lv[VOCS][99];
typedef struct { int id, ability, skill, points; } Step;
static Step g_step[320]; static int g_nstep;
static uint8_t g_voc_skill[VOCS][5];
static int g_read, g_have_vs;

static void read_tables(void) {
    if (g_read) return;
    g_read = 1;
    Rec r; uint32_t n, p;
    for (int v = 0; v < VOCS; v++) {
        char path[40]; snprintf(path, sizeof path, "data/prm/level%d.bin", v);
        const uint8_t* b = romdata_file(path, &n);
        if (!b || n < 8) continue;
        uint32_t end = le32(b + 4); if (end > n) end = n;
        int k = 0;
        for (p = 8; romdata_rec_next(b, end, &p, &r) && k < 99;)
            if (r.tag == 0x66 && r.n >= 11) {
                g_lv[v][k].exp = r.v[0];
                for (int i = 0; i < 9; i++) g_lv[v][k].st[i] = (uint16_t)r.v[1 + i];
                g_lv[v][k].pts = (uint16_t)r.v[10];
                k++;
            }
    }
    const uint8_t* pack = romdata_file("data/prm/sklname.gp2", &n);
    uint32_t sz = 0;
    uint8_t* b = pack ? romdata_gpc_file(pack, n, "sklname_en.bin", &sz) : NULL;
    if (b && sz >= 16) {
        uint32_t end = le32(b + 4); if (end > sz) end = sz;
        for (p = 16; romdata_rec_next(b, end, &p, &r) && g_nstep < 320;)
            if (r.tag == 0x66 && r.n >= 5 && r.v[3] >= 1 && r.v[3] <= SKILLS && r.v[4] > 0 && r.v[4] <= 100)   /* (0 points: never learnt) */
                g_step[g_nstep++] = (Step){ (int)r.v[0], (int)r.v[2], (int)r.v[3], (int)r.v[4] };
        for (int i = 1; i < g_nstep; i++)                          /* by skill, then points */
            for (int j = i; j > 0 && (g_step[j].skill < g_step[j - 1].skill || (g_step[j].skill == g_step[j - 1].skill && g_step[j].points < g_step[j - 1].points)); j--) {
                Step t = g_step[j]; g_step[j] = g_step[j - 1]; g_step[j - 1] = t;
            }
    }
    free(b);
}

/* Which five skills each vocation has: in the game's code, found by their values. */
static void find_voc_skills(void) {
    if (g_have_vs) return;
    static const uint8_t sig[15] = { 0, 0, 0, 0, 0, 1, 2, 3, 13, 15, 2, 4, 6, 13, 16 };   /* Guardian (none), Warrior, Priest */
    for (uint32_t a = 0; a + 65 < 0x110000; a++)
        if (!memcmp(&g_main_ram[a], sig, sizeof sig)) { memcpy(g_voc_skill, &g_main_ram[a], sizeof g_voc_skill); g_have_vs = 1; return; }
}

/* ---------------------------------------------------------------- the party */
typedef struct { uint32_t at; char name[24]; } Member;
static Member g_m[4]; static int g_nm, g_who, g_line, g_level_mode;

static int adventure(void) { return rd32(FIELD_PTR) != 0; }

static void read_party(void) {
    g_nm = 0;
    if (!adventure()) return;
    uint32_t rec[4]; int nrec = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t a = PARTY + i * MEMBER;
        if (g_main_ram[a] >= 0x20 && rd32(a + 0x914) < VOCS) rec[nrec++] = a;
    }
    int used[4] = { 0 };
    for (int s = 0; s < 4 && g_nm < 4; s++) {                     /* in the order the game shows them */
        const char* n = (const char*)&g_main_ram[NAMES + s * 0x34];
        if (!n[0]) continue;
        for (int i = 0; i < nrec; i++)
            if (!used[i] && !strncmp(n, (const char*)&g_main_ram[rec[i]], 12)) { used[i] = 1; g_m[g_nm++].at = rec[i]; break; }
    }
    for (int i = nrec - 1; i >= 0 && g_nm < 4; i--) if (!used[i]) g_m[g_nm++].at = rec[i];
    for (int i = 0; i < g_nm; i++) {
        char raw[24]; memcpy(raw, &g_main_ram[g_m[i].at], 16); raw[16] = 0;
        romdata_clean_name(raw, g_m[i].name, sizeof g_m[i].name);
    }
    if (g_who >= g_nm) g_who = 0;
}

static int voc_of(uint32_t a) { return (int)rd32(a + 0x914); }
static int level_of(uint32_t a, int v) { int l = rd16(a + 0x130 + 2 * v); return l < 1 ? 1 : l > 99 ? 99 : l; }
static int points_in(uint32_t a, int skill) { return g_main_ram[(a + 0x428 + skill) & (MAIN_RAM_SIZE - 1)]; }

static const char* menu_text(int id) { return journal_game_string("data/bin/menu/str_sta.gp2", "str_sta", id); }
static const char* voc_name(int v) { const char* s = menu_text(4 + v); return s ? s : "?"; }
static const char* skill_name(int k) { const char* s = menu_text(17 + k); return s ? s : "?"; }
/* "Sword" rather than "Sword Skill": the Attributes page's other names (their order isn't the skills') */
static const char* skill_short(int k) {
    static const short id[SKILLS + 1] = { 0, 415, 416, 417, 418, 420, 419, 421, 422, 423, 424, 426, 425, 427, 428,
                                          429, 430, 431, 432, 433, 434, 435, 437, 436, 439, 440, 438 };
    const char* s = k >= 1 && k <= SKILLS ? menu_text(id[k]) : NULL;
    return s ? s : skill_name(k);
}
static const char* step_name(int id) { const char* s = journal_game_string("data/bin/menu/sta_skl.gp2", "sta_skl", id); return s ? s : "?"; }

/* The skills shown for someone: their vocation's five, then the others they put points in. */
static int skills_shown(uint32_t a, int* out) {
    int n = 0, v = voc_of(a);
    if (g_have_vs) for (int i = 0; i < 5; i++) if (g_voc_skill[v][i]) out[n++] = g_voc_skill[v][i];
    for (int k = 1; k <= SKILLS; k++) {
        int dup = 0; for (int i = 0; i < n; i++) dup |= out[i] == k;
        if (!dup && points_in(a, k)) out[n++] = k;
    }
    return n;
}

static void opened(void) { read_tables(); find_voc_skills(); read_party(); }

/* ---------------------------------------------------------------- shared bits */
static void member_tabs(const JournalCanvas* c, int y) {
    int x = c->x;
    for (int i = 0; i < g_nm; i++) {
        int w = journal_text_width(18, g_m[i].name) + 30;
        journal_box(c, x, y, w, 32, i == g_who ? 0x3A4A66 : 0x252B36, 1);
        journal_text(c, x + 15, y + 5, 18, i == g_who ? 0xFFFFFF : 0xA8B0BC, g_m[i].name);
        x += w + 8;
    }
}

static const char* T(const char* const t[5], int l) { return t[l]; }

/* ---------------------------------------------------------------- Progress */
static const char* p_title(int l) { static const char* const t[5] = { "Progress", "Progression", "Fortschritt", "Progressi", "Progreso" }; return t[l]; }

static int p_key(int k) {
    if (!g_nm) return 0;
    int list[SKILLS + 5], n = skills_shown(g_m[g_who].at, list);
    switch (k) {
    case JK_LEFT: g_who = (g_who + g_nm - 1) % g_nm; g_line = 0; return 1;
    case JK_RIGHT: g_who = (g_who + 1) % g_nm; g_line = 0; return 1;
    case JK_UP: if (g_line > 0) { g_line--; return 1; } return 0;
    case JK_DOWN: if (g_line < n - 1) { g_line++; return 1; } return 0;
    }
    return 0;
}

static void p_draw(const JournalCanvas* c) {
    static const char* const no_game[5] = { "Load an adventure first.", "Chargez d'abord une aventure.", "Lade zuerst ein Abenteuer.", "Carica prima un'avventura.", "Carga primero una aventura." };
    static const char* const t_now[5] = { "Now (with equipment)", "Actuellement (avec l'\xC3\xA9quipement)", "Jetzt (mit Ausr\xC3\xBCstung)", "Ora (con l'equipaggiamento)", "Ahora (con el equipo)" };
    static const char* const t_base[5] = { "%s Lv. %d: its own attributes", "%s niv. %d : attributs de base", "%s St. %d: Grundwerte", "%s Liv. %d: attributi di base", "%s Nv. %d: atributos base" };
    static const char* const t_base2[5] = { "(without equipment, seeds or skill bonuses)", "(sans \xC3\xA9quipement, graines ni bonus de comp\xC3\xA9tences)", "(ohne Ausr\xC3\xBCstung, Samen und Talentboni)", "(senza equipaggiamento, semi n\xC3\xA9 bonus delle abilit\xC3\xA0)", "(sin equipo, semillas ni bonos de habilidades)" };
    static const char* const t_next[5] = { "Next level in %u EXP", "Niveau suivant dans %u EXP", "N\xC3\xA4" "chste Stufe in %u EP", "Livello seguente tra %u PE", "Siguiente nivel en %u EXP" };
    static const char* const t_free[5] = { "Skill points to give out: %d", "Points de comp\xC3\xA9tence \xC3\xA0 r\xC3\xA9partir : %d", "Freie Talentpunkte: %d", "Punti abilit\xC3\xA0 da assegnare: %d", "Puntos de habilidad por repartir: %d" };
    static const char* const t_skills[5] = { "Skills", "Comp\xC3\xA9tences", "Talente", "Abilit\xC3\xA0", "Habilidades" };
    static const char* const t_nextstep[5] = { "next: %s at %d (%d more)", "suivant : %s \xC3\xA0 %d (encore %d)", "n\xC3\xA4" "chstes: %s bei %d (noch %d)", "prossimo: %s a %d (ancora %d)", "siguiente: %s a %d (faltan %d)" };
    static const char* const t_all[5] = { "all learnt", "tout appris", "alles gelernt", "tutto appreso", "todo aprendido" };
    static const char* const t_ability[5] = { "ability", "technique", "F\xC3\xA4higkeit", "tecnica", "t\xC3\xA9" "cnica" };
    static const char* const t_trait[5] = { "trait", "atout", "Eigenschaft", "dote", "rasgo" };
    static const char* const t_other[5] = { "(not this vocation's: its bonuses apply with every vocation)", "(pas de cette vocation : ses bonus restent acquis avec toutes)",
        "(nicht von diesem Beruf: die Boni gelten in jedem Beruf)", "(non di questa vocazione: i bonus valgono con tutte)", "(no es de esta vocaci\xC3\xB3n: sus bonos valen con todas)" };
    int l = c->lang;
    read_tables(); find_voc_skills(); read_party();
    if (!g_nm) { journal_text(c, c->x, c->y + 8, 20, 0xC8C8C8, no_game[l]); return; }
    member_tabs(c, c->y);
    uint32_t a = g_m[g_who].at;
    int v = voc_of(a), lv = level_of(a, v), y = c->y + 50, x = c->x;
    char s[200];
    snprintf(s, sizeof s, "%s  \xE2\x80\x94  %s  %s %d", g_m[g_who].name, voc_name(v), menu_text(44) ? menu_text(44) : "Lv", lv);
    journal_text(c, x, y, 24, 0xF2C94C, s);
    y += 38;
    uint32_t exp = rd32(a + 0xFC + 4 * v);
    if (lv < 99 && g_lv[v][lv].exp > exp) { snprintf(s, sizeof s, t_next[l], g_lv[v][lv].exp - exp); journal_text(c, x, y, 17, 0xC8C8C8, s); }
    int unspent = rd16(a + 0x528);
    snprintf(s, sizeof s, t_free[l], unspent);
    journal_text(c, x + 420, y, 17, unspent ? 0x8EE28E : 0xC8C8C8, s);
    y += 40;
    /* the attributes (left) */
    int ly = y, lw = 380;
    journal_text(c, x, ly, 17, 0x9AA0A8, t_now[l]); ly += 28;
    static const int now_id[5] = { 53, 54, 55, 56, 47 };          /* Max HP, Max MP, Attack, Defence, Agility */
    for (int i = 0; i < 5; i++) {
        journal_text(c, x + 10, ly, 18, 0xF0F0F0, menu_text(now_id[i]) ? menu_text(now_id[i]) : "?");
        snprintf(s, sizeof s, "%d", rd16(a + 0x30 + 2 * i));
        journal_text(c, x + 300 - journal_text_width(18, s), ly, 18, 0xFFFFFF, s);
        ly += 27;
    }
    ly += 14;
    snprintf(s, sizeof s, t_base[l], voc_name(v), lv);
    journal_text(c, x, ly, 17, 0x9AA0A8, s); ly += 22;
    journal_text(c, x, ly, 13, 0x7F8790, t_base2[l]); ly += 24;
    static const int base_id[9] = { 46, 48, 47, 49, 50, 52, 51, 53, 54 };   /* the level tables' order */
    for (int i = 0; i < 9; i++) {
        journal_text(c, x + 10, ly, 17, 0xD8D8D8, menu_text(base_id[i]) ? menu_text(base_id[i]) : "?");
        snprintf(s, sizeof s, "%d", g_lv[v][lv - 1].st[i]);
        journal_text(c, x + 300 - journal_text_width(17, s), ly, 17, 0xD8D8D8, s);
        ly += 24;
    }
    journal_box(c, x + lw, y - 6, 1, c->y + c->h - y + 6, 0x8A8A8A, 0.5f);
    /* the skills (right) */
    int sx = x + lw + 26, sw = c->x + c->w - sx, sy = y;
    journal_text(c, sx, sy, 17, 0x9AA0A8, T(t_skills, l)); sy += 28;
    int list[SKILLS + 5], n = skills_shown(a, list), mine = 0;
    if (g_have_vs) for (int i = 0; i < 5; i++) mine += g_voc_skill[v][i] != 0;
    if (g_line >= n) g_line = n ? n - 1 : 0;
    for (int i = 0; i < n; i++) {
        int k = list[i], pts = points_in(a, k), row = 30;
        if (i == mine && i > 0) { journal_text(c, sx, sy + 2, 13, 0x7F8790, t_other[l]); sy += 22; }
        if (i == g_line) journal_box(c, sx - 6, sy - 3, sw + 6, row, 0xFFFFFF, 0.10f);
        journal_text(c, sx, sy, 17, 0xF0F0F0, skill_name(k));
        journal_bar(c, sx + 200, sy + 8, 150, 10, pts / 100.0f, pts >= 100 ? 0xF2C94C : 0x6FA8DC);
        snprintf(s, sizeof s, "%d / 100", pts);
        journal_text(c, sx + 360, sy, 16, 0xD8D8D8, s);
        const Step* nx = NULL;
        for (int j = 0; j < g_nstep && !nx; j++) if (g_step[j].skill == k && g_step[j].points > pts) nx = &g_step[j];
        if (nx) snprintf(s, sizeof s, t_nextstep[l], step_name(nx->id), nx->points, nx->points - pts);
        else snprintf(s, sizeof s, "%s", t_all[l]);
        journal_text(c, sx + 450, sy + 1, 15, nx ? 0xC8C8C8 : 0xF2C94C, s);
        sy += row;
    }
    /* the chosen skill's steps */
    if (n) {
        int k = list[g_line], pts = points_in(a, k);
        sy += 12;
        journal_box(c, sx, sy, sw - 10, 1, 0x8A8A8A, 0.4f); sy += 10;
        journal_text(c, sx, sy, 17, 0xF2C94C, skill_name(k)); sy += 28;
        int col = 0, cy = sy, cw = (sw - 10) / 2;
        for (int j = 0; j < g_nstep; j++) {
            if (g_step[j].skill != k) continue;
            int got = pts >= g_step[j].points;
            snprintf(s, sizeof s, "%3d", g_step[j].points);
            journal_text(c, sx + col * cw, cy, 15, got ? 0x8EE28E : 0x9AA0A8, got ? "\xE2\x9C\x93" : "\xC2\xB7");
            journal_text(c, sx + col * cw + 20, cy, 15, got ? 0x8EE28E : 0xB8B8B8, s);
            snprintf(s, sizeof s, "%s  (%s)", step_name(g_step[j].id), g_step[j].ability ? t_ability[l] : t_trait[l]);
            journal_text(c, sx + col * cw + 60, cy, 15, got ? 0xF0F0F0 : 0xB8B8B8, s);
            cy += 24;
            if (cy > c->y + c->h - 24) { if (col) break; col = 1; cy = sy; }
        }
    }
}

const JournalPage g_journal_party = { p_title, p_draw, p_key, opened, NULL };

/* ---------------------------------------------------------------- Vocations */
static const char* v_title(int l) { static const char* const t[5] = { "Vocations", "Vocations", "Berufe", "Vocazioni", "Vocaciones" }; return t[l]; }

static int v_key(int k) {
    if (!g_nm) return 0;
    switch (k) {
    case JK_LEFT: g_who = (g_who + g_nm - 1) % g_nm; return 1;
    case JK_RIGHT: g_who = (g_who + 1) % g_nm; return 1;
    case JK_MORE: case JK_OK: case JK_UP: case JK_DOWN: g_level_mode = (g_level_mode + (k == JK_UP ? 2 : 1)) % 3; return 1;
    }
    return 0;
}

static void v_draw(const JournalCanvas* c) {
    static const char* const no_game[5] = { "Load an adventure first.", "Chargez d'abord une aventure.", "Lade zuerst ein Abenteuer.", "Carica prima un'avventura.", "Carga primero una aventura." };
    static const char* const t_mode[3][5] = {
        { "Each vocation at the level of the current one (%d)", "Chaque vocation au niveau de l'actuelle (%d)", "Jeder Beruf auf der Stufe des jetzigen (%d)", "Ogni vocazione al livello dell'attuale (%d)", "Cada vocaci\xC3\xB3n al nivel de la actual (%d)" },
        { "Each vocation at the level reached in it", "Chaque vocation au niveau atteint", "Jeder Beruf auf der erreichten Stufe", "Ogni vocazione al livello raggiunto", "Cada vocaci\xC3\xB3n al nivel alcanzado" },
        { "Each vocation at level 99", "Chaque vocation au niveau 99", "Jeder Beruf auf Stufe 99", "Ogni vocazione al livello 99", "Cada vocaci\xC3\xB3n al nivel 99" },
    };
    static const char* const t_keys[5] = { "\xE2\x86\x91 \xE2\x86\x93 or H: which level     base attributes (no equipment); green: the best", "\xE2\x86\x91 \xE2\x86\x93 ou H : quel niveau     attributs de base (sans \xC3\xA9quipement) ; en vert : le meilleur",
        "\xE2\x86\x91 \xE2\x86\x93 oder H: welche Stufe     Grundwerte (ohne Ausr\xC3\xBCstung); gr\xC3\xBCn: der beste", "\xE2\x86\x91 \xE2\x86\x93 o H: quale livello     attributi di base (senza equipaggiamento); in verde: il migliore",
        "\xE2\x86\x91 \xE2\x86\x93 o H: qu\xC3\xA9 nivel     atributos base (sin equipo); en verde: el mejor" };
    static const char* const t_skills[5] = { "Skills", "Comp\xC3\xA9tences", "Talente", "Abilit\xC3\xA0", "Habilidades" };
    int l = c->lang;
    read_tables(); find_voc_skills(); read_party();
    if (!g_nm) { journal_text(c, c->x, c->y + 8, 20, 0xC8C8C8, no_game[l]); return; }
    member_tabs(c, c->y);
    uint32_t a = g_m[g_who].at;
    int cur = voc_of(a), curlv = level_of(a, cur), y = c->y + 48;
    char s[200];
    snprintf(s, sizeof s, t_mode[g_level_mode][l], curlv);
    journal_text(c, c->x, y, 19, 0xF2C94C, s);
    journal_text(c, c->x + 640, y + 3, 14, 0x9AA0A8, t_keys[l]);
    y += 34;
    int cx[9], x0 = c->x + 220, cwid = 62;
    for (int i = 0; i < 9; i++) cx[i] = x0 + i * cwid;
    int skx = x0 + 9 * cwid + 14;
    journal_text(c, c->x + 150, y, 13, 0x9AA0A8, menu_text(44) ? menu_text(44) : "Lv");
    static const char* const short_name[5][9] = {                 /* the attributes' names, short (the game has only the long ones) */
        { "Str", "Res", "Agi", "Dex", "Cha", "Might", "Mend", "HP", "MP" },
        { "For", "Vit", "Agi", "Adr", "Cha", "Pui.M", "Soin", "PV", "PM" },
        { "St\xC3\xA4", "Wid", "Bew", "Ges", "Cha", "Mag", "Heil", "LP", "MP" },
        { "For", "Res", "Agi", "Des", "Fas", "Pot.M", "Cura", "PV", "PM" },
        { "Fue", "Res", "Agi", "Des", "Enc", "Pod.M", "Cur", "PV", "PM" },
    };
    for (int i = 0; i < 9; i++) journal_text(c, cx[i] + cwid - 10 - journal_text_width(13, short_name[l][i]), y, 13, 0x9AA0A8, short_name[l][i]);
    journal_text(c, skx, y, 13, 0x9AA0A8, T(t_skills, l));
    y += 24;
    int lvs[VOCS], best[9] = { 0 }, first = 1;
    for (int v = first; v < VOCS; v++) {
        lvs[v] = g_level_mode == 0 ? curlv : g_level_mode == 1 ? level_of(a, v) : 99;
        for (int i = 0; i < 9; i++) if (g_lv[v][lvs[v] - 1].st[i] > best[i]) best[i] = g_lv[v][lvs[v] - 1].st[i];
    }
    int row = (c->y + c->h - y) / 12; if (row > 34) row = 34;
    for (int v = first; v < VOCS; v++) {
        if (v == cur) journal_box(c, c->x - 6, y - 3, c->w + 6, row - 2, 0x3A4A66, 0.8f);
        journal_text(c, c->x, y, 16, v == cur ? 0xFFFFFF : 0xE0E0E0, voc_name(v));
        snprintf(s, sizeof s, "%d", level_of(a, v));
        journal_text(c, c->x + 150, y, 16, level_of(a, v) > 1 ? 0xF0F0F0 : 0x8A8A8A, s);
        for (int i = 0; i < 9; i++) {
            int val = g_lv[v][lvs[v] - 1].st[i];
            snprintf(s, sizeof s, "%d", val);
            journal_text(c, cx[i] + cwid - 10 - journal_text_width(16, s), y, 16, val == best[i] && val ? 0x8EE28E : 0xD8D8D8, s);
        }
        s[0] = 0;
        if (g_have_vs) for (int i = 0; i < 5; i++) {
            int k = g_voc_skill[v][i];
            if (!k) continue;
            snprintf(s + strlen(s), sizeof s - strlen(s), "%s%s", s[0] ? ", " : "", skill_short(k));
        }
        journal_text(c, skx, y + 1, 14, 0xB8C0CC, s);
        y += row;
    }
}

const JournalPage g_journal_vocations = { v_title, v_draw, v_key, opened, NULL };
