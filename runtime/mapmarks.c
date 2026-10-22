/* Modern: markers on the map (Modern > Map markers). The top screen's map
 * gets a mark for every chest, pot and barrel of the place the party is in,
 * grey once emptied; the mouse over one tells what is in it.
 *
 * Everything comes from the game itself:
 *  - data/scenario/treasure.nsarc (NARC): one file per place (C01.bin for
 *    Stornway, C01M14.bin for the castle's first floor...), records in the
 *    game's script format (u16 tag, u8 count, 2-bit types, words). Tag 0x67
 *    is one container: word 0 = what is in it (low half) and its flag (high
 *    half), word 1 = the kind (8 chest, 4 gold, 0x40 blue chest, 0x10 pot,
 *    0x20 barrel, 0x30 cupboards and the like: no position given), then
 *    x, height, z (floats, the field's units).
 *  - data/map/maplist9.bin: every place: its number (the game's current one
 *    is at 0x020FB3F0), its name (C01M14) and the map picture it shows.
 *  - data/prm/itemname.gp2: the items' names (GPC2, see gpc_file()).
 *  - main RAM: the map picture shown (0x020FDC5C points at it: its name at
 *    +2, pixels per unit at +0x38 (20.12), the centre of the view at
 *    +0x44/+0x48, map pixels): a point (x, z) of the field shows at
 *    (x * scale - cx + 128, z * scale - cy + 96) on the top screen.
 *  - the progress flags (bits at 0x021088D0, func_0206dfc0): a chest's at
 *    0x212 + its flag, a pot's, barrel's or blue chest's at 0x79E + its flag.
 *
 * Read only: the game never sees any of it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nds.h"
#include "text.h"
#include "mapmarks.h"

int g_mapmarks = 1;
int g_mapmarks_opened = 1;

/* ---------------------------------------------------------------- the game's memory */
#define FIELD_PTR   0x0F33E0       /* 0 until an adventure is loaded (farm.c) */
#define MAP_NUM     0x0FB3F0       /* the place the party is in (u16, maplist9.bin's numbers) */
#define MINIMAP_PTR 0x0FDC5C       /* the map picture shown */
#define FLAGS       0x1088D0       /* the progress flags (bits) */
#define FLAG_CHEST  0x212
#define FLAG_OTHER  0x79E

static uint32_t rd32(uint32_t a) { uint32_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 1)], 4); return v; }
static uint16_t rd16(uint32_t a) { uint16_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 1)], 2); return v; }
static int flag_set(int n) { return n >= 0 && (g_main_ram[(FLAGS + (n >> 3)) & (MAIN_RAM_SIZE - 1)] >> (n & 7)) & 1; }

/* ---------------------------------------------------------------- files in the ROM */
static uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static size_t slen(const char* s, size_t max) { size_t n = 0; while (n < max && s[n]) n++; return n; }

/* A file of the cartridge's file system, by its path ("data/map/maplist9.bin"). */
static const uint8_t* rom_file(const char* path, uint32_t* size) {
    if (!g_rom || g_rom_size < 0x200) return NULL;
    uint32_t fnt = le32(g_rom + 0x40), fat = le32(g_rom + 0x48), fat_size = le32(g_rom + 0x4C);
    if (fnt >= g_rom_size || fat >= g_rom_size) return NULL;
    uint32_t dir = 0;                                            /* the root */
    for (;;) {
        const char* slash = strchr(path, '/');
        size_t len = slash ? (size_t)(slash - path) : strlen(path);
        if (fnt + dir * 8 + 8 > g_rom_size) return NULL;
        uint32_t p = fnt + le32(g_rom + fnt + dir * 8), id = le16(g_rom + fnt + dir * 8 + 4), found = 0;
        while (p < g_rom_size && g_rom[p]) {
            int n = g_rom[p] & 0x7F, sub = g_rom[p] & 0x80;
            if (p + 1 + n + (sub ? 2 : 0) > g_rom_size) return NULL;
            int match = (size_t)n == len && !memcmp(g_rom + p + 1, path, len);
            if (sub) {
                if (match && slash) { dir = le16(g_rom + p + 1 + n) & 0xFFF; found = 1; break; }
                p += 1 + n + 2;
            } else {
                if (match && !slash) {
                    if ((id + 1) * 8 > fat_size) return NULL;
                    uint32_t a = le32(g_rom + fat + id * 8), b = le32(g_rom + fat + id * 8 + 4);
                    if (b < a || b > g_rom_size) return NULL;
                    *size = b - a;
                    return g_rom + a;
                }
                p += 1 + n; id++;
            }
        }
        if (!found) return NULL;
        path = slash + 1;
    }
}

/* A file in a NARC archive, by name. */
static const uint8_t* narc_file(const uint8_t* d, uint32_t n, const char* name, uint32_t* size) {
    if (!d || n < 16 || memcmp(d, "NARC", 4)) return NULL;
    uint32_t p = le16(d + 12), btaf = 0, btnf = 0, gmif = 0;
    while (p + 8 <= n) {
        uint32_t len = le32(d + p + 4);
        if (!memcmp(d + p, "BTAF", 4)) btaf = p;
        else if (!memcmp(d + p, "BTNF", 4)) btnf = p;
        else if (!memcmp(d + p, "GMIF", 4)) gmif = p;
        if (len < 8) break;
        p += len;
    }
    if (!btaf || !btnf || !gmif) return NULL;
    uint32_t count = le32(d + btaf + 8), q = btnf + 8 + le32(d + btnf + 8), size_name = (uint32_t)strlen(name);
    for (uint32_t i = 0; i < count && q < n && d[q] && d[q] < 0x80; i++) {
        uint32_t len = d[q];
        if (len == size_name && q + 1 + len <= n && !memcmp(d + q + 1, name, len)) {
            if (btaf + 12 + i * 8 + 8 > n) return NULL;
            uint32_t a = le32(d + btaf + 12 + i * 8), b = le32(d + btaf + 16 + i * 8);
            if (b < a || gmif + 8 + b > n) return NULL;
            *size = b - a;
            return d + gmif + 8 + a;
        }
        q += 1 + len;
    }
    return NULL;
}

/* Level-5's compression: a u32 (size << 3 | method) then the data. Methods:
 * 0 stored, 1 LZ (the DS BIOS's LZ77), 2 and 3 Huffman (4 and 8 bits), 4 RLE.
 * Returns a new buffer (caller frees), NULL if it doesn't make sense. */
static uint8_t* l5_unpack(const uint8_t* d, uint32_t n, uint32_t* out_n) {
    if (n < 4) return NULL;
    uint32_t h = le32(d), size = h >> 3, p = 4, o = 0;
    if (size > 16u << 20) return NULL;
    uint8_t* out = malloc(size ? size : 1);
    if (!out) return NULL;
    switch (h & 7) {
    case 0:
        if (p + size > n) goto bad;
        memcpy(out, d + p, size); o = size;
        break;
    case 1:
        while (o < size) {
            if (p >= n) goto bad;
            uint8_t f = d[p++];
            for (int b = 0; b < 8 && o < size; b++) {
                if (f & (0x80 >> b)) {
                    if (p + 2 > n) goto bad;
                    uint32_t x = (uint32_t)d[p] << 8 | d[p + 1], len = (x >> 12) + 3, dist = (x & 0xFFF) + 1; p += 2;
                    if (dist > o) goto bad;
                    while (len-- && o < size) { out[o] = out[o - dist]; o++; }
                } else {
                    if (p >= n) goto bad;
                    out[o++] = d[p++];
                }
            }
        }
        break;
    case 2: case 3: {
        int bits = (h & 7) == 2 ? 4 : 8;
        if (p >= n) goto bad;
        uint32_t tree = p, q = p + ((uint32_t)d[p] + 1) * 2, pos = tree + 1, half = 0;
        while (o < size) {
            if (q + 4 > n) goto bad;
            uint32_t w = le32(d + q); q += 4;
            for (int i = 0; i < 32 && o < size; i++) {
                int bit = (w >> (31 - i)) & 1;
                if (pos >= n) goto bad;
                uint8_t node = d[pos];
                uint32_t next = (pos & ~1u) + (node & 0x3F) * 2u + 2 + (uint32_t)bit;
                if (next >= n) goto bad;
                if (node & (0x80 >> bit)) {
                    uint8_t v = d[next];
                    if (bits == 8) out[o++] = v;
                    else if (!half) { out[o] = v & 15; half = 1; }
                    else { out[o++] |= (uint8_t)(v << 4); half = 0; }
                    pos = tree + 1;
                } else pos = next;
            }
        }
        break;
    }
    case 4:
        while (o < size) {
            if (p >= n) goto bad;
            uint8_t f = d[p++];
            if (f & 0x80) {
                if (p >= n) goto bad;
                for (int k = 0; k < (f & 0x7F) + 3 && o < size; k++) out[o++] = d[p];
                p++;
            } else {
                for (int k = 0; k < (f & 0x7F) + 1 && o < size; k++) { if (p >= n) goto bad; out[o++] = d[p++]; }
            }
        }
        break;
    default:
        goto bad;
    }
    *out_n = size;
    return out;
bad:
    free(out);
    return NULL;
}

static uint32_t crc32_of(const char* s) {
    uint32_t c = 0xFFFFFFFFu;
    for (; *s; s++) {
        c ^= (uint8_t)*s;
        for (int k = 0; k < 8; k++) c = c & 1 ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return ~c;
}

/* A file in a GPC2 archive (Level-5's packs), unpacked (caller frees).
 * Header: at +8 the names' offset / 4, at +10 the data's / 4; at +0x14 the
 * list (packed): per file the CRC-32 of its name, its data's offset / 4 (low
 * 24 bits), a size. */
static uint8_t* gpc_file(const uint8_t* d, uint32_t n, const char* name, uint32_t* size) {
    if (!d || n < 0x18 || memcmp(d, "GPC2", 4)) return NULL;
    uint32_t data = (uint32_t)le16(d + 10) * 4, ln = 0, want = crc32_of(name);
    uint8_t* list = l5_unpack(d + 0x14, n - 0x14, &ln), *out = NULL;
    if (!list) return NULL;
    for (uint32_t i = 0; i + 12 <= ln; i += 12)
        if (le32(list + i) == want) {
            uint32_t at = data + (le32(list + i + 4) & 0xFFFFFF) * 4;
            if (at < n) out = l5_unpack(d + at, n - at, size);
            break;
        }
    free(list);
    return out;
}

/* ---------------------------------------------------------------- the script format */
typedef struct { int tag, n; uint8_t type[24]; uint32_t v[24]; } Rec;

/* The next record from *p (stops at `end` or the 0xFFFF end mark). */
static int rec_next(const uint8_t* b, uint32_t end, uint32_t* p, Rec* r) {
    if (*p + 4 > end) return 0;
    r->tag = le16(b + *p); r->n = b[*p + 2];
    if (r->tag == 0xFFFF || r->n > 24) return 0;
    uint32_t types = (uint32_t)(r->n + 3) / 4, q = *p + ((3 + types + 3) & ~3u);
    if (*p + 3 + types > end || q + 4u * (uint32_t)r->n > end) return 0;
    for (int i = 0; i < r->n; i++) {
        r->type[i] = (b[*p + 3 + i / 4] >> (2 * (i % 4))) & 3;     /* 0 a string, 1 a number, 2 a float */
        r->v[i] = le32(b + q + 4u * (uint32_t)i);
    }
    *p = q + 4u * (uint32_t)r->n;
    return 1;
}
static float rec_float(const Rec* r, int i) { float f; memcpy(&f, &r->v[i], 4); return f; }

/* ---------------------------------------------------------------- the places */
typedef struct { uint16_t num; char code[12], map[12]; } Place;
static Place* g_places; static int g_nplaces;
static const uint8_t* g_tbox; static uint32_t g_tbox_n;           /* treasure.nsarc */

static void load_places(void) {
    uint32_t n = 0;
    const uint8_t* b = rom_file("data/map/maplist9.bin", &n);
    if (!b || n < 8) return;
    uint32_t end = le32(b + 4), p = 8;                            /* the strings follow the records */
    if (end > n) return;
    Rec r;
    while (rec_next(b, end, &p, &r)) {
        if (r.tag != 0x67 || r.n < 12 || r.type[4] != 0 || r.type[11] != 0) continue;
        Place* np = realloc(g_places, sizeof(Place) * (size_t)(g_nplaces + 1));
        if (!np) return;
        g_places = np;
        Place* pl = &g_places[g_nplaces];
        memset(pl, 0, sizeof *pl);
        pl->num = (uint16_t)r.v[0];
        uint32_t c = end + r.v[4], m = end + r.v[11];
        if (r.v[4] >= 0xFFFF || r.v[11] >= 0xFFFF || c >= n || m >= n) continue;
        snprintf(pl->code, sizeof pl->code, "%.*s", (int)slen((const char*)b + c, n - c), (const char*)b + c);
        snprintf(pl->map, sizeof pl->map, "%.*s", (int)slen((const char*)b + m, n - m), (const char*)b + m);
        g_nplaces++;
    }
}

static const Place* place(uint16_t num) {
    for (int i = 0; i < g_nplaces; i++) if (g_places[i].num == num) return &g_places[i];
    return NULL;
}

/* ---------------------------------------------------------------- the items' names */
typedef struct { uint16_t key; char name[48]; } Name;
static Name* g_names; static int g_nnames, g_names_lang = -1;

/* The game's letters (<'e>, <1>...) as UTF-8; [notes for the grammar] left out. */
static void clean_name(const char* s, char* out, size_t cap) {
    static const struct { const char* esc; const char* utf8; } k[] = {
        { "<'e>", "\xC3\xA9" }, { "<'E>", "\xC3\x89" }, { "<`e>", "\xC3\xA8" }, { "<^e>", "\xC3\xAA" }, { "<:e>", "\xC3\xAB" },
        { "<'a>", "\xC3\xA1" }, { "<'A>", "\xC3\x81" }, { "<`a>", "\xC3\xA0" }, { "<^a>", "\xC3\xA2" }, { "<:a>", "\xC3\xA4" }, { "<:A>", "\xC3\x84" },
        { "<'i>", "\xC3\xAD" }, { "<^i>", "\xC3\xAE" }, { "<:i>", "\xC3\xAF" }, { "<'o>", "\xC3\xB3" }, { "<'O>", "\xC3\x93" },
        { "<^o>", "\xC3\xB4" }, { "<:o>", "\xC3\xB6" }, { "<'u>", "\xC3\xBA" }, { "<`u>", "\xC3\xB9" }, { "<^u>", "\xC3\xBB" },
        { "<:u>", "\xC3\xBC" }, { "<:U>", "\xC3\x9C" }, { "<,c>", "\xC3\xA7" }, { "<~n>", "\xC3\xB1" }, { "<ss>", "\xC3\x9F" },
        { "<oe>", "\xC5\x93" }, { "<1>", "'" }, { "<66>", "\"" }, { "<99>", "\"" }, { "<6>", "\"" }, { "<9>", "\"" }, { "<,>", "," },
    };
    size_t o = 0;
    while (*s && o + 4 < cap) {
        if (*s == '[') { const char* e = strchr(s, ']'); if (e) { s = e + 1; continue; } }
        if (*s == '<') {
            size_t i, nk = sizeof k / sizeof k[0];
            for (i = 0; i < nk; i++) { size_t l = strlen(k[i].esc); if (!strncmp(s, k[i].esc, l)) { size_t u = strlen(k[i].utf8); memcpy(out + o, k[i].utf8, u); o += u; s += l; break; } }
            if (i < nk) continue;
            const char* e = strchr(s, '>'); if (e) { s = e + 1; continue; }
        }
        out[o++] = *s++;
    }
    out[o] = 0;
}

static void load_names(int lang) {
    static const char* const suffix[5] = { "en", "fr", "de", "it", "es" };
    if (lang == g_names_lang) return;
    g_names_lang = lang;
    free(g_names); g_names = NULL; g_nnames = 0;
    uint32_t n = 0, sz = 0;
    const uint8_t* pack = rom_file("data/prm/itemname.gp2", &n);
    char fname[40];
    snprintf(fname, sizeof fname, "itemname_%s.nat", suffix[lang >= 1 && lang <= 5 ? lang - 1 : 0]);
    uint8_t* b = gpc_file(pack, n, fname, &sz);
    if (!b) b = gpc_file(pack, n, "itemname_en.nat", &sz);
    if (!b) b = gpc_file(pack, n, "itemname.nat", &sz);
    if (!b || sz < 4) { free(b); return; }
    uint32_t count = le32(b) & 0xFFF, base = 4 + 16 * count;     /* (strings' size << 12) | count; 16 bytes each */
    if (base > sz) { free(b); return; }
    g_names = calloc(count ? count : 1, sizeof(Name));
    for (uint32_t i = 0; g_names && i < count; i++) {
        uint32_t off = le32(b + 4 + 16 * i), at = base + off;
        if (at >= sz) continue;
        Name* nm = &g_names[g_nnames++];
        nm->key = (uint16_t)le32(b + 4 + 16 * i + 12);
        char raw[96]; snprintf(raw, sizeof raw, "%.*s", (int)slen((const char*)b + at, sz - at), (const char*)b + at);
        clean_name(raw, nm->name, sizeof nm->name);
    }
    free(b);
}

static const char* item_name(uint16_t key) {
    for (int i = 0; i < g_nnames; i++) if (g_names[i].key == key) return g_names[i].name;
    return NULL;
}

/* ---------------------------------------------------------------- this place's containers */
enum { K_CHEST, K_GOLD, K_BLUE, K_POT, K_BARREL, K_UNKNOWN };
typedef struct { float x, z; int kind, flag; uint16_t what; } Box;
static Box g_box[160]; static int g_nbox; static int g_box_place = -1;

static void load_boxes(const Place* pl) {
    g_nbox = 0;
    if (!pl) return;
    char name[20]; snprintf(name, sizeof name, "%s.bin", pl->code);
    uint32_t n = 0;
    const uint8_t* b = narc_file(g_tbox, g_tbox_n, name, &n);
    if (!b || n < 8) return;
    uint32_t end = le32(b + 4), p = 8;
    if (end > n) end = n;
    Rec r;
    while (rec_next(b, end, &p, &r) && g_nbox < (int)(sizeof g_box / sizeof g_box[0])) {
        if (r.tag != 0x67 || r.n < 5 || r.type[2] != 2 || r.type[4] != 2) continue;   /* the ones with a place on the map */
        Box* x = &g_box[g_nbox];
        uint32_t kind = r.v[1];
        x->what = (uint16_t)r.v[0]; x->flag = (int)(r.v[0] >> 16);
        x->x = rec_float(&r, 2); x->z = rec_float(&r, 4);
        if (!isfinite(x->x) || !isfinite(x->z)) continue;
        switch (kind) {
        case 0x08: case 0x09: x->kind = K_CHEST; break;
        case 0x04: x->kind = K_GOLD; break;
        case 0x40: x->kind = K_BLUE; break;
        case 0x10: x->kind = K_POT; break;
        case 0x20: x->kind = K_BARREL; break;
        default: x->kind = K_UNKNOWN; break;
        }
        x->flag += x->kind == K_CHEST || x->kind == K_GOLD || x->kind == K_UNKNOWN ? FLAG_CHEST : FLAG_OTHER;
        g_nbox++;
    }
}

/* ---------------------------------------------------------------- what is shown */
typedef struct { float sx, sy; int box, opened; } Mark;
static Mark g_mark[160]; static int g_nmark;
static float g_hx = -1, g_hy = -1; static int g_hover = -1;

void mapmarks_hover(float x, float y) { g_hx = x; g_hy = y; }

static int lang_index(void) { return g_fw_language >= 1 && g_fw_language <= 5 ? g_fw_language - 1 : 0; }

static const char* kind_name(int kind) {
    static const char* const t[6][5] = {
        { "Treasure chest", "Coffre", "Schatztruhe", "Forziere", "Cofre" },
        { "Treasure chest", "Coffre", "Schatztruhe", "Forziere", "Cofre" },
        { "Blue chest", "Coffre bleu", "Blaue Truhe", "Forziere blu", "Cofre azul" },
        { "Pot", "Jarre", "Krug", "Giara", "Vasija" },
        { "Barrel", "Tonneau", "Fass", "Barile", "Barril" },
        { "Treasure chest", "Coffre", "Schatztruhe", "Forziere", "Cofre" },
    };
    return t[kind][lang_index()];
}

/* What is in it, as the game would say it. */
static void content(const Box* b, char* out, size_t cap) {
    static const char* const random_item[5] = { "Random item", "Objet au hasard", "Zuf\xC3\xA4lliger Gegenstand", "Oggetto a caso", "Objeto al azar" };
    static const char* const unknown[5] = { "Contents unknown", "Contenu inconnu", "Inhalt unbekannt", "Contenuto ignoto", "Contenido desconocido" };
    const char* nm = item_name(b->what);
    if (b->kind == K_GOLD) snprintf(out, cap, "%u G", (unsigned)b->what);
    else if (nm) snprintf(out, cap, "%s", nm);
    else if (b->what > 0 && b->what < 0x100) snprintf(out, cap, "%s", random_item[lang_index()]);
    else snprintf(out, cap, "%s", unknown[lang_index()]);
}

static const char* status(const Box* b, int opened) {
    static const char* const open_chest[5] = { "Already opened", "D\xC3\xA9j\xC3\xA0 ouvert", "Schon ge\xC3\xB6" "ffnet", "Gi\xC3\xA0 aperto", "Ya abierto" };
    static const char* const closed_chest[5] = { "Not opened yet", "Pas encore ouvert", "Noch nicht ge\xC3\xB6" "ffnet", "Non ancora aperto", "A\xC3\xBA" "n sin abrir" };
    static const char* const searched[5] = { "Already searched", "D\xC3\xA9j\xC3\xA0 fouill\xC3\xA9", "Schon durchsucht", "Gi\xC3\xA0 frugato", "Ya registrado" };
    static const char* const not_searched[5] = { "Not searched yet", "Pas encore fouill\xC3\xA9", "Noch nicht durchsucht", "Non ancora frugato", "A\xC3\xBA" "n sin registrar" };
    int box = b->kind == K_CHEST || b->kind == K_GOLD || b->kind == K_BLUE || b->kind == K_UNKNOWN;
    return (box ? (opened ? open_chest : closed_chest) : (opened ? searched : not_searched))[lang_index()];
}

/* ---------------------------------------------------------------- drawing (premultiplied RGBA) */
static uint8_t g_img[MAPMARKS_W * MAPMARKS_H * 4];
static uint32_t g_txt[MAPMARKS_W * 64];                         /* the tooltip's text, drawn alone */

static void blend(int x, int y, uint32_t rgb, float a) {
    if (x < 0 || y < 0 || x >= MAPMARKS_W || y >= MAPMARKS_H || a <= 0) return;
    if (a > 1) a = 1;
    uint8_t* p = &g_img[((size_t)y * MAPMARKS_W + x) * 4];
    float r = (float)((rgb >> 16) & 255), g = (float)((rgb >> 8) & 255), b = (float)(rgb & 255), k = 1 - a;
    p[0] = (uint8_t)(r * a + p[0] * k); p[1] = (uint8_t)(g * a + p[1] * k); p[2] = (uint8_t)(b * a + p[2] * k);
    p[3] = (uint8_t)(255 * a + p[3] * k);
}

/* A rounded box, smoothed edges: centre, half sizes, corner radius (overlay pixels). */
static void rbox(float cx, float cy, float hw, float hh, float rad, uint32_t rgb, float alpha) {
    for (int y = (int)floorf(cy - hh - 1); y <= (int)ceilf(cy + hh + 1); y++)
        for (int x = (int)floorf(cx - hw - 1); x <= (int)ceilf(cx + hw + 1); x++) {
            float dx = fabsf((float)x + 0.5f - cx) - (hw - rad), dy = fabsf((float)y + 0.5f - cy) - (hh - rad);
            float ox = dx > 0 ? dx : 0, oy = dy > 0 ? dy : 0;
            float d = sqrtf(ox * ox + oy * oy) + (dx > dy ? (dx < 0 ? dx : 0) : (dy < 0 ? dy : 0)) - rad;
            blend(x, y, rgb, alpha * (0.5f - d));
        }
}

static void draw_mark(const Mark* m, int hover) {
    const Box* b = &g_box[m->box];
    float R = MAPMARKS_R * 1.4f, cx = m->sx * MAPMARKS_R, cy = m->sy * MAPMARKS_R, a = m->opened ? 0.6f : 1.0f;
    uint32_t edge = 0x201008;
    /* a light halo: seen on the map's sand as on its grass */
    if (hover) rbox(cx, cy, 4.6f * R, 4.0f * R, 2.0f * R, 0xFFFFFF, 0.95f);
    else rbox(cx, cy, 3.9f * R, 3.3f * R, 1.6f * R, 0xFFF8E0, 0.55f * a);
    switch (b->kind) {
    case K_POT:
        rbox(cx, cy, 2.4f * R, 2.4f * R, 2.4f * R, edge, a);
        rbox(cx, cy, 2.4f * R - 1.5f, 2.4f * R - 1.5f, 2.4f * R - 1.5f, m->opened ? 0x8A8A8A : 0xC07A44, a);
        rbox(cx, cy - 1.6f * R, 1.1f * R, 0.5f * R, 0.4f * R, m->opened ? 0x5A5A5A : 0x6A3A1A, a);   /* the mouth */
        break;
    case K_BARREL:
        rbox(cx, cy, 2.2f * R, 2.7f * R, 1.0f * R, edge, a);
        rbox(cx, cy, 2.2f * R - 1.5f, 2.7f * R - 1.5f, 1.0f * R - 1, m->opened ? 0x8A8A8A : 0xB98A4E, a);
        rbox(cx, cy - 1.1f * R, 2.0f * R, 0.25f * R, 0, edge, a * 0.8f);   /* the hoops */
        rbox(cx, cy + 1.1f * R, 2.0f * R, 0.25f * R, 0, edge, a * 0.8f);
        break;
    default: {                                                   /* a chest: lid, band, lock */
        uint32_t body = m->opened ? 0x8A8A8A : b->kind == K_BLUE ? 0x3C78E0 : 0xD0402E;
        uint32_t band = m->opened ? 0xB0B0B0 : 0xF2C94C;
        rbox(cx, cy, 3.2f * R, 2.5f * R, 0.8f * R, edge, a);
        rbox(cx, cy, 3.2f * R - 1.5f, 2.5f * R - 1.5f, 0.8f * R - 1, body, a);
        rbox(cx, cy - 0.5f * R, 3.2f * R - 1.5f, 0.3f * R, 0, band, a);
        rbox(cx, cy + 0.3f * R, 0.55f * R, 0.75f * R, 0.25f * R, band, a);
        break;
    }
    }
}

static void draw_tooltip(const Mark* m) {
    const Box* b = &g_box[m->box];
    char what[80], line1[140];
    content(b, what, sizeof what);
    snprintf(line1, sizeof line1, "%s: %s", kind_name(b->kind), what);
    const char* line2 = status(b, m->opened);
    float px = 7.5f * MAPMARKS_R;                                /* text height */
    int lh = (int)(px * 1.2f), pad = 2 * MAPMARKS_R;
    memset(g_txt, 0, sizeof g_txt);
    int w1 = text_draw(g_txt, MAPMARKS_W, 64, 0, 0, px, 0xFFFFFF, line1);
    int w2 = text_draw(g_txt, MAPMARKS_W, 64, 0, lh, px, 0x00FF00, line2);   /* green channel: the second line */
    int tw = (w1 > w2 ? w1 : w2) + 2 * pad, th = 2 * lh + 2 * pad;
    if (tw > MAPMARKS_W) tw = MAPMARKS_W;
    int x0 = (int)(m->sx * MAPMARKS_R) - tw / 2, y0 = (int)((m->sy - 8) * MAPMARKS_R) - th;
    if (y0 < 0) y0 = (int)((m->sy + 8) * MAPMARKS_R);          /* no room above: below */
    if (x0 < 2) x0 = 2;
    if (x0 + tw > MAPMARKS_W - 2) x0 = MAPMARKS_W - 2 - tw;
    rbox(x0 + tw / 2.0f, y0 + th / 2.0f, tw / 2.0f, th / 2.0f, 2.0f * MAPMARKS_R, 0x101418, 0.88f);
    uint32_t c2 = m->opened ? 0xA8A8A8 : 0x8EE28E;
    for (int y = 0; y < 2 * lh && y < 64; y++)
        for (int x = 0; x < tw - 2 * pad; x++) {
            uint32_t t = g_txt[y * MAPMARKS_W + x];
            if (!t) continue;
            int first = y < lh;
            float cov = (float)(first ? (t >> 16) & 255 : (t >> 8) & 255) / 255.0f;
            blend(x0 + pad + x, y0 + pad + y, first ? 0xFFFFFF : c2, cov);
        }
}

/* ---------------------------------------------------------------- once a frame */
int mapmarks_update(const uint8_t** rgba) {
    static int loaded, had;
    static uint32_t last_key;
    *rgba = NULL;
    if (!loaded) {
        loaded = 1;
        load_places();
        g_tbox = rom_file("data/scenario/treasure.nsarc", &g_tbox_n);
        if (g_tbox) LOG_INFO("mapmarks: %d places\n", g_nplaces);
    }
    g_nmark = 0;
    int ok = g_mapmarks && g_tbox && g_nplaces && rd32(FIELD_PTR) != 0;
    uint32_t mp = rd32(MINIMAP_PTR);
    if (ok && (mp < 0x02000000 || mp >= 0x02400000)) ok = 0;
    const Place* pl = ok ? place(rd16(MAP_NUM)) : NULL;
    if (pl) {
        /* only the map of the place itself: inside a house the map is the
         * town's (another picture), and the house's units aren't the town's */
        char shown[10];
        memcpy(shown, &g_main_ram[(mp + 2) & (MAIN_RAM_SIZE - 1)], 9); shown[9] = 0;
        if (strlen(pl->map) < 6 || strncmp(shown, pl->map, 6)) pl = NULL;
    }
    if (pl) {
        if (g_box_place != pl->num) { load_boxes(pl); g_box_place = pl->num; }
        load_names(g_fw_language);
        float scale = (float)(int32_t)rd32(mp + 0x38) / 4096.0f;
        float cx = (float)(int32_t)rd32(mp + 0x44), cy = (float)(int32_t)rd32(mp + 0x48);
        if (!(scale > 0.1f && scale < 64)) g_nbox = 0;
        for (int i = 0; i < g_nbox; i++) {
            const Box* b = &g_box[i];
            float sx = b->x * scale - cx + 128, sy = b->z * scale - cy + 96;
            if (sx < -4 || sx > 260 || sy < -4 || sy > 196) continue;
            int opened = flag_set(b->flag);
            if (opened && !g_mapmarks_opened) continue;
            g_mark[g_nmark++] = (Mark){ sx, sy, i, opened };
        }
    }
    /* the mark under the mouse (the nearest) */
    g_hover = -1;
    float best = 64;                                             /* within 8 pixels */
    for (int i = 0; i < g_nmark; i++) {
        float dx = g_mark[i].sx - g_hx, dy = g_mark[i].sy - g_hy;
        if (g_hx >= 0 && dx * dx + dy * dy < best) { best = dx * dx + dy * dy; g_hover = i; }
    }
    /* redraw only when something moved */
    uint32_t key = 2166136261u;
    for (int i = 0; i < g_nmark; i++) {
        int v[4] = { (int)lroundf(g_mark[i].sx * MAPMARKS_R), (int)lroundf(g_mark[i].sy * MAPMARKS_R), g_mark[i].box, g_mark[i].opened };
        for (int k = 0; k < 4; k++) key = (key ^ (uint32_t)v[k]) * 16777619u;
    }
    key = (key ^ (uint32_t)(g_hover + 2)) * 16777619u;
    key = (key ^ (uint32_t)g_fw_language) * 16777619u;
    if (!g_nmark) { int changed = had; had = 0; last_key = 0; return changed; }
    if (had && key == last_key) return 0;
    had = 1; last_key = key;
    memset(g_img, 0, sizeof g_img);
    for (int i = 0; i < g_nmark; i++) if (i != g_hover) draw_mark(&g_mark[i], 0);
    if (g_hover >= 0) { draw_mark(&g_mark[g_hover], 1); draw_tooltip(&g_mark[g_hover]); }
    *rgba = g_img;
    return 1;
}
