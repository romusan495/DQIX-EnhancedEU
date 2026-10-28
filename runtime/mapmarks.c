/* Modern: markers on the map (Modern > Map markers). The top screen's map
 * gets a mark for every chest, pot, barrel and bookshelf of the place the
 * party is in (chests grey once emptied) and a bubble over everyone with a
 * quest to give, and a badge on every building whose rooms hold some; the
 * mouse over one tells what is in it.
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
 *  - data/map/<place>.ambl (NARC): the .bmbl inside (DS LZ) lists what can
 *    be looked at: tag 0x73 (kind, x, y, z...) then 0x74; kind 8 is a
 *    bookshelf, 0x74's first word its number in the place.
 *  - data/scenario/htana?.gp2: per language, tag 0x66 per bookshelf: place,
 *    number, teaches recipes (1), recipe book, then the texts (the second
 *    one when it teaches recipes) with the book's title between quotes.
 *  - data/pack_lv5/minimap.gp2: <place>.bmmp per map picture: tag 0x66 its
 *    picture's name, 0x6b the places drawn in its units, 0x6c a door (x, z,
 *    then the places behind it: the building's rooms).
 *  - quests: data/scenario/questidtbl.bin (tag 0x68: the quest, its number
 *    in the game), questorder3.bin (tag 0x66: the quest, the place and the
 *    person giving it), questmsg.gp2 (per language, tag 0x67: the number,
 *    the name, the short description), <C01>.npc (NARC, <C01>place.bin:
 *    where each person stands, see load_givers()); their state is a nibble
 *    per quest at 0x02108B10 (func_0206e130/func_0206e174).
 *  - the monsters walking around: the field's characters 0x70..0x7B
 *    (pointers at 0x020F33E0 + 4 * number, farm.c); a record's second word
 *    has the map's number (0x020FB3F0) in its top half while it is there;
 *    its position (x, height, z, 20.12) at +0x44, in the same units as the
 *    spots. A field's map (20001...) shows the whole field, 1 pixel per
 *    unit around the middle of the screen.
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
int g_mapmarks_monsters = 1;
int g_interior_maps = 1;

/* ---------------------------------------------------------------- the game's memory */
#define FIELD_PTR   0x0F33E0       /* 0 until an adventure is loaded (farm.c) */
#define MAP_NUM     0x0FB3F0       /* the place the party is in (u16, maplist9.bin's numbers) */
#define MINIMAP_PTR 0x0FDC5C       /* the map picture shown */
#define FLAGS       0x1088D0       /* the progress flags (bits) */
#define FLAG_CHEST  0x212
#define FLAG_OTHER  0x79E
#define MON_FIRST   0x70           /* the monsters walking around: field characters 0x70..0x7B (farm.c) */
#define MON_COUNT   12

static uint32_t rd32(uint32_t a) { uint32_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 1)], 4); return v; }
static uint16_t rd16(uint32_t a) { uint16_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 1)], 2); return v; }
static int flag_set(int n) { return n >= 0 && (g_main_ram[(FLAGS + (n >> 3)) & (MAIN_RAM_SIZE - 1)] >> (n & 7)) & 1; }

/* ---------------------------------------------------------------- files in the ROM */
static uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static size_t slen(const char* s, size_t max) { size_t n = 0; while (n < max && s[n]) n++; return n; }
static int up(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
static int same_name(const uint8_t* a, const char* b, size_t n) {   /* ignoring case: a few files are named in lower case */
    for (size_t i = 0; i < n; i++) if (up(a[i]) != up((uint8_t)b[i])) return 0;
    return 1;
}

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
            int match = (size_t)n == len && same_name(g_rom + p + 1, path, len);
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

/* A file in a NARC archive, by name ("*.ext": the first one ending so). */
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
        int match = name[0] == '*' ? len >= size_name - 1 && q + 1 + len <= n && !memcmp(d + q + 1 + len - (size_name - 1), name + 1, size_name - 1)
                                   : len == size_name && q + 1 + len <= n && !memcmp(d + q + 1, name, len);
        if (match) {
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

/* The DS BIOS's LZ77 from d + p into out (size bytes); 0 if it doesn't make sense. */
static int lz(const uint8_t* d, uint32_t n, uint32_t p, uint8_t* out, uint32_t size) {
    uint32_t o = 0;
    while (o < size) {
        if (p >= n) return 0;
        uint8_t f = d[p++];
        for (int b = 0; b < 8 && o < size; b++) {
            if (f & (0x80 >> b)) {
                if (p + 2 > n) return 0;
                uint32_t x = (uint32_t)d[p] << 8 | d[p + 1], len = (x >> 12) + 3, dist = (x & 0xFFF) + 1; p += 2;
                if (dist > o) return 0;
                while (len-- && o < size) { out[o] = out[o - dist]; o++; }
            } else {
                if (p >= n) return 0;
                out[o++] = d[p++];
            }
        }
    }
    return 1;
}

/* A file packed the DS way (0x10, then the size in 24 bits, then LZ77). New buffer (caller frees). */
static uint8_t* ds_unpack(const uint8_t* d, uint32_t n, uint32_t* out_n) {
    if (n < 4 || d[0] != 0x10) return NULL;
    uint32_t size = d[1] | d[2] << 8 | (uint32_t)d[3] << 16;
    uint8_t* out = malloc(size ? size : 1);
    if (out && !lz(d, n, 4, out, size)) { free(out); out = NULL; }
    if (out) *out_n = size;
    return out;
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
        if (!lz(d, n, p, out, size)) goto bad;
        o = size;
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

/* A GPC2 archive (Level-5's packs). Header: at +8 the names' offset / 4, at
 * +10 the data's / 4; at +0x14 the list (packed): per file the CRC-32 of its
 * name, its data's offset / 4 (low 24 bits), a size. */
typedef struct { const uint8_t* d; uint32_t n, data, ln; uint8_t* list; } Gpc;

static int gpc_open(Gpc* g, const uint8_t* d, uint32_t n) {
    memset(g, 0, sizeof *g);
    if (!d || n < 0x18 || memcmp(d, "GPC2", 4)) return 0;
    g->d = d; g->n = n; g->data = (uint32_t)le16(d + 10) * 4;
    g->list = l5_unpack(d + 0x14, n - 0x14, &g->ln);
    return g->list != NULL;
}
static void gpc_close(Gpc* g) { free(g->list); g->list = NULL; }

/* A file in it, unpacked (caller frees). */
static uint8_t* gpc_get(const Gpc* g, const char* name, uint32_t* size) {
    uint32_t want = crc32_of(name);
    for (uint32_t i = 0; g->list && i + 12 <= g->ln; i += 12)
        if (le32(g->list + i) == want) {
            uint32_t at = g->data + (le32(g->list + i + 4) & 0xFFFFFF) * 4;
            return at < g->n ? l5_unpack(g->d + at, g->n - at, size) : NULL;
        }
    return NULL;
}

/* Its files' names, one after the other, each ended by a 0 (caller frees). */
static uint8_t* gpc_names(const Gpc* g, uint32_t* size) {
    uint32_t at = (uint32_t)le16(g->d + 8) * 4;
    return at < g->n ? l5_unpack(g->d + at, g->n - at, size) : NULL;
}

static uint8_t* gpc_file(const uint8_t* d, uint32_t n, const char* name, uint32_t* size) {
    Gpc g;
    if (!gpc_open(&g, d, n)) return NULL;
    uint8_t* out = gpc_get(&g, name, size);
    gpc_close(&g);
    return out;
}

/* ---------------------------------------------------------------- the script format */
#define REC_MAX 64
typedef struct { int tag, n; uint8_t type[REC_MAX]; uint32_t v[REC_MAX]; } Rec;   /* n: the words kept (at most REC_MAX) */

/* The next record from *p (stops at `end` or the 0xFFFF end mark). */
static int rec_next(const uint8_t* b, uint32_t end, uint32_t* p, Rec* r) {
    if (*p + 4 > end) return 0;
    int count = b[*p + 2];
    r->tag = le16(b + *p); r->n = count < REC_MAX ? count : REC_MAX;
    if (r->tag == 0xFFFF) return 0;
    uint32_t types = (uint32_t)(count + 3) / 4, q = *p + ((3 + types + 3) & ~3u);
    if (*p + 3 + types > end || q + 4u * (uint32_t)count > end) return 0;
    for (int i = 0; i < r->n; i++) {
        r->type[i] = (b[*p + 3 + i / 4] >> (2 * (i % 4))) & 3;     /* 0 a string, 1 a number, 2 a float */
        r->v[i] = le32(b + q + 4u * (uint32_t)i);
    }
    *p = q + 4u * (uint32_t)count;
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
        uint32_t c = end + r.v[4], m = end + r.v[11];
        if (r.v[4] >= 0xFFFF || r.v[11] >= 0xFFFF || c >= n || m >= n) continue;
        Place* np = realloc(g_places, sizeof(Place) * (size_t)(g_nplaces + 1));
        if (!np) return;
        g_places = np;
        Place* pl = &g_places[g_nplaces++];
        memset(pl, 0, sizeof *pl);
        pl->num = (uint16_t)r.v[0];
        snprintf(pl->code, sizeof pl->code, "%.*s", (int)slen((const char*)b + c, n - c), (const char*)b + c);
        snprintf(pl->map, sizeof pl->map, "%.*s", (int)slen((const char*)b + m, n - m), (const char*)b + m);
    }
}

static int place_index(uint32_t num) {
    for (int i = 0; i < g_nplaces; i++) if (g_places[i].num == num) return i;
    return -1;
}

/* ---------------------------------------------------------------- the game's texts */
typedef struct { uint16_t key; char name[48]; } Name;
static Name* g_names; static int g_nnames, g_names_lang = -1;
static const char* const LANG_SUFFIX[5] = { "en", "fr", "de", "it", "es" };
static int lang_index(void) { return g_fw_language >= 1 && g_fw_language <= 5 ? g_fw_language - 1 : 0; }

/* The game's letters (<'e>, <1>...) as UTF-8; [notes for the grammar] and other <codes> left out. */
static void clean_name(const char* s, char* out, size_t cap) {
    static const struct { const char* esc; const char* utf8; } k[] = {
        { "<'e>", "\xC3\xA9" }, { "<'E>", "\xC3\x89" }, { "<`e>", "\xC3\xA8" }, { "<^e>", "\xC3\xAA" }, { "<:e>", "\xC3\xAB" },
        { "<'a>", "\xC3\xA1" }, { "<'A>", "\xC3\x81" }, { "<`a>", "\xC3\xA0" }, { "<`A>", "\xC3\x80" }, { "<^a>", "\xC3\xA2" }, { "<:a>", "\xC3\xA4" }, { "<:A>", "\xC3\x84" },
        { "<'i>", "\xC3\xAD" }, { "<^i>", "\xC3\xAE" }, { "<:i>", "\xC3\xAF" }, { "<'o>", "\xC3\xB3" }, { "<'O>", "\xC3\x93" },
        { "<^o>", "\xC3\xB4" }, { "<:o>", "\xC3\xB6" }, { "<:O>", "\xC3\x96" }, { "<'u>", "\xC3\xBA" }, { "<`u>", "\xC3\xB9" }, { "<^u>", "\xC3\xBB" },
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
    if (lang == g_names_lang) return;
    g_names_lang = lang;
    free(g_names); g_names = NULL; g_nnames = 0;
    uint32_t n = 0, sz = 0;
    const uint8_t* pack = rom_file("data/prm/itemname.gp2", &n);
    char fname[40];
    snprintf(fname, sizeof fname, "itemname_%s.nat", LANG_SUFFIX[lang >= 1 && lang <= 5 ? lang - 1 : 0]);
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

/* ---------------------------------------------------------------- the books */
typedef struct { uint16_t place; int16_t idx; uint8_t recipes, dull; char title[96]; } Book;
static Book* g_books; static int g_nbooks, g_books_lang = -1;

/* A shelf's text -> the book's title (between the language's quotes), or
 * else its first sentence ("There's a letter sandwiched between two books"). */
static void book_title(const char* s, char* out, size_t cap) {
    static const char* const opening[3] = { "<6>", "<66>", "<DE99>" }, *const closing[3] = { "<9>", "<99>", "<DE66>" };
    const char* a = NULL, *e = NULL; size_t al = 0;
    for (int i = 0; i < 3; i++) { const char* f = strstr(s, opening[i]); if (f && (!a || f < a)) { a = f; al = strlen(opening[i]); } }
    if (a) for (int i = 0; i < 3; i++) { const char* f = strstr(a + al, closing[i]); if (f && (!e || f < e)) e = f; }
    char raw[200];
    if (a && e) snprintf(raw, sizeof raw, "%.*s", (int)(e - a - (ptrdiff_t)al), a + al);
    else {
        size_t n = 0;
        while (s[n] && n < sizeof raw - 1 && s[n] != '.' && s[n] != '!' && strncmp(s + n, "<PAGE>", 6)) n++;
        snprintf(raw, sizeof raw, "%.*s", (int)n, s);
    }
    clean_name(raw, out, cap);
}

static void load_books(int lang) {
    if (lang == g_books_lang) return;
    g_books_lang = lang;
    free(g_books); g_books = NULL; g_nbooks = 0;
    const char* suffix = LANG_SUFFIX[lang >= 1 && lang <= 5 ? lang - 1 : 0];
    for (const char* L = "CDHMRSX"; *L; L++) {
        char path[48], fname[32];
        snprintf(path, sizeof path, "data/scenario/htana%c.gp2", *L);
        snprintf(fname, sizeof fname, "htana%c_%s.bin", *L, suffix);
        uint32_t n = 0, sz = 0;
        const uint8_t* pack = rom_file(path, &n);
        uint8_t* b = gpc_file(pack, n, fname, &sz);
        if (!b || sz < 16) { free(b); continue; }
        uint32_t end = le32(b + 4), p = 16, first = (uint32_t)g_nbooks;
        if (end > sz) end = sz;
        Rec r;
        uint32_t* offs = NULL;
        while (rec_next(b, end, &p, &r)) {
            if (r.tag != 0x66 || r.n < 6) continue;
            uint32_t off = r.v[2] == 1 ? r.v[5] : r.v[4];             /* teaching recipes: the second text */
            Book* nb = realloc(g_books, sizeof(Book) * (size_t)(g_nbooks + 1));
            uint32_t* no = realloc(offs, sizeof(uint32_t) * (size_t)(g_nbooks - first + 1));
            if (!nb || !no) { if (nb) g_books = nb; if (no) offs = no; break; }
            g_books = nb; offs = no;
            Book* bk = &g_books[g_nbooks];
            memset(bk, 0, sizeof *bk);
            bk->place = (uint16_t)r.v[0]; bk->idx = (int16_t)r.v[1]; bk->recipes = r.v[2] == 1;
            if (off < sz - end) book_title((const char*)b + end + off, bk->title, sizeof bk->title);
            offs[g_nbooks - first] = off;
            g_nbooks++;
        }
        /* the text most of them share is "nothing of interest here" (whatever the language) */
        uint32_t dull = 0xFFFFFFFF; int most = 0, count = g_nbooks - (int)first;
        for (int i = 0; i < count; i++) {
            int c = 0;
            for (int j = 0; j < count; j++) c += offs[j] == offs[i];
            if (c > most) { most = c; dull = offs[i]; }
        }
        for (int i = 0; i < count; i++) g_books[first + i].dull = (offs[i] == dull && !g_books[first + i].recipes) || offs[i] >= sz - end;
        free(offs); free(b);
    }
}

static const Book* find_book(uint16_t place, int idx) {
    for (int i = 0; i < g_nbooks; i++) if (g_books[i].place == place && g_books[i].idx == idx) return &g_books[i];
    return NULL;
}

/* ---------------------------------------------------------------- the quests */
#define QUESTS      0xCC            /* the game's limit (func_0206e130) */
#define QUEST_STATE 0x108B10        /* a nibble per quest: see quest_state() */
typedef struct { uint16_t place, npc; uint8_t num; char name[64], desc[240]; } Quest;   /* num: as the game shows it, 0 none */
static Quest g_quest[QUESTS]; static int g_quests_read, g_quests_lang = -1;

/* 0 not offered yet (or not downloaded), 1 offered, 2 under way, 3 done. */
static int quest_state(int q) {
    if (q < 0 || q >= QUESTS) return 0;
    int nib = (g_main_ram[(QUEST_STATE + (q >> 1)) & (MAIN_RAM_SIZE - 1)] >> (q & 1 ? 4 : 0)) & 15;
    return nib & 8 ? 0 : nib & 3;                                /* 8: a quest from the Wi-Fi shop not downloaded */
}

static void load_quests(void) {
    if (g_quests_read) return;
    g_quests_read = 1;
    uint32_t n = 0;
    const uint8_t* b = rom_file("data/scenario/questidtbl.bin", &n);     /* tag 0x68: the quest, its number in the game */
    Rec r;
    if (b && n >= 8) {
        uint32_t end = le32(b + 4), p = 8;
        if (end > n) end = n;
        while (rec_next(b, end, &p, &r))
            if (r.tag == 0x68 && r.n >= 2 && r.v[0] < QUESTS && r.v[1] < 255) g_quest[r.v[0]].num = (uint8_t)r.v[1];
    }
    b = rom_file("data/scenario/questorder3.bin", &n);              /* tag 0x66: the quest, the giver's place and person */
    if (b && n >= 8) {
        uint32_t end = le32(b + 4), p = 8;
        if (end > n) end = n;
        while (rec_next(b, end, &p, &r))
            if (r.tag == 0x66 && r.n >= 3 && r.v[0] < QUESTS) { g_quest[r.v[0]].place = (uint16_t)r.v[1]; g_quest[r.v[0]].npc = (uint16_t)r.v[2]; }
    }
}

static void load_quest_texts(int lang) {
    if (lang == g_quests_lang) return;
    g_quests_lang = lang;
    for (int q = 0; q < QUESTS; q++) g_quest[q].name[0] = g_quest[q].desc[0] = 0;
    uint32_t n = 0, sz = 0;
    const uint8_t* pack = rom_file("data/scenario/questmsg.gp2", &n);
    char fname[32]; snprintf(fname, sizeof fname, "questmsg_%s.bin", LANG_SUFFIX[lang >= 1 && lang <= 5 ? lang - 1 : 0]);
    uint8_t* b = gpc_file(pack, n, fname, &sz);
    if (!b) b = gpc_file(pack, n, "questmsg_en.bin", &sz);
    if (!b || sz < 16) { free(b); return; }
    uint32_t end = le32(b + 4), p = 16;
    if (end > sz) end = sz;
    Rec r;
    while (rec_next(b, end, &p, &r)) {                           /* tag 0x67: the number, the name, the short description... */
        if (r.tag != 0x67 || r.n < 3 || r.v[0] == 0) continue;
        for (int q = 0; q < QUESTS; q++) {
            if (g_quest[q].num != r.v[0]) continue;
            for (int k = 1; k <= 2; k++) {
                if (r.type[k] != 0 || r.v[k] >= sz - end) continue;
                const char* s = (const char*)b + end + r.v[k];
                char raw[600]; snprintf(raw, sizeof raw, "%.*s", (int)slen(s, sz - end - r.v[k]), s);
                if (k == 1) clean_name(raw, g_quest[q].name, sizeof g_quest[q].name);
                else clean_name(raw, g_quest[q].desc, sizeof g_quest[q].desc);
            }
        }
    }
    free(b);
}

/* ---------------------------------------------------------------- what each place holds */
enum { K_CHEST, K_GOLD, K_BLUE, K_POT, K_BARREL, K_UNKNOWN, K_SHELF, K_QUEST };
typedef struct { float x, z; int kind, flag, idx; uint16_t what; } Spot;   /* flag: a container's; idx: a shelf's number, a quest */
typedef struct { int loaded, n; Spot* s; } Held;
static Held* g_held;                                             /* per place (g_places' order), read when first needed */

static void add_spot(Held* h, const Spot* s) {
    Spot* ns = realloc(h->s, sizeof(Spot) * (size_t)(h->n + 1));
    if (!ns) return;
    h->s = ns; h->s[h->n++] = *s;
}

static void load_treasure(Held* h, const Place* pl) {
    char name[20]; snprintf(name, sizeof name, "%s.bin", pl->code);
    uint32_t n = 0;
    const uint8_t* b = narc_file(g_tbox, g_tbox_n, name, &n);
    if (!b || n < 8) return;
    uint32_t end = le32(b + 4), p = 8;
    if (end > n) end = n;
    Rec r;
    while (rec_next(b, end, &p, &r)) {
        if (r.tag != 0x67 || r.n < 5 || r.type[2] != 2 || r.type[4] != 2) continue;   /* the ones with a place on the map */
        Spot x = { rec_float(&r, 2), rec_float(&r, 4), K_UNKNOWN, (int)(r.v[0] >> 16), -1, (uint16_t)r.v[0] };
        if (!isfinite(x.x) || !isfinite(x.z)) continue;
        switch (r.v[1]) {
        case 0x08: case 0x09: x.kind = K_CHEST; break;
        case 0x04: x.kind = K_GOLD; break;
        case 0x40: x.kind = K_BLUE; break;
        case 0x10: x.kind = K_POT; break;
        case 0x20: x.kind = K_BARREL; break;
        }
        x.flag += x.kind == K_CHEST || x.kind == K_GOLD || x.kind == K_UNKNOWN ? FLAG_CHEST : FLAG_OTHER;
        add_spot(h, &x);
    }
}

static void load_shelves(Held* h, const Place* pl) {
    char path[48]; snprintf(path, sizeof path, "data/map/%s.ambl", pl->code);
    uint32_t n = 0, sz = 0;
    const uint8_t* ambl = rom_file(path, &n), *packed = narc_file(ambl, n, "*.bmbl", &sz);
    uint8_t* b = packed ? ds_unpack(packed, sz, &sz) : NULL;
    if (!b || sz < 8) { free(b); return; }
    uint32_t end = le32(b + 4), p = 8;
    if (end > sz) end = sz;
    Rec r;
    Spot shelf; int pending = 0;
    while (rec_next(b, end, &p, &r)) {
        if (pending) { if (r.tag == 0x74 && r.n >= 1) shelf.idx = (int)r.v[0]; add_spot(h, &shelf); pending = 0; }
        if (r.tag == 0x73 && r.n >= 4 && r.v[0] == 8 && r.type[1] == 2 && r.type[3] == 2) {   /* something to look at: 8 a bookshelf */
            shelf = (Spot){ rec_float(&r, 1), rec_float(&r, 3), K_SHELF, -1, -1, 0 };
            pending = isfinite(shelf.x) && isfinite(shelf.z);
        }
    }
    if (pending) add_spot(h, &shelf);
    free(b);
}

/* Where a quest's giver stands: data/scenario/<C01>.npc (or F.npc for the
 * fields), <C01>place.bin: a record holding (place, person, x, y, z), the
 * person's own (tag 3) first; else the quest's own scenes (tag 0xE: quest,
 * ?, ?, place, person, x, y, z) at that place. */
static void load_givers(Held* h, const Place* pl) {
    char path[48], pfx[4];
    uint32_t n = 0, sz = 0;
    const uint8_t* d = NULL;
    snprintf(pfx, sizeof pfx, "%.3s", pl->code);
    for (int len = 3; len >= 1 && !d; len -= 2) {
        pfx[len] = 0;
        snprintf(path, sizeof path, "data/scenario/%s.npc", pfx);
        d = rom_file(path, &n);
    }
    const uint8_t* b = narc_file(d, n, "*place.bin", &sz);
    if (!b || sz < 16) return;
    uint32_t end = le32(b + 4);
    if (end > sz) end = sz;
    for (int q = 0; q < QUESTS; q++) {
        if (!g_quest[q].num || g_quest[q].place != pl->num) continue;
        Spot x = { 0, 0, K_QUEST, -1, q, 0 };
        int best = 0;                                            /* 3: the person's own place, 2: another, 1: a scene */
        uint32_t p = 16;
        Rec r;
        while (best < 3 && rec_next(b, end, &p, &r)) {
            for (int i = 0; i + 4 < r.n; i++) {
                if (r.type[i] != 1 || r.type[i + 1] != 1 || r.type[i + 2] != 2 || r.type[i + 4] != 2) continue;
                int mine = r.v[i] == pl->num && r.v[i + 1] == g_quest[q].npc;
                int score = mine ? (r.tag == 3 ? 3 : 2) : r.tag == 0xE && i == 3 && r.v[0] == (uint32_t)q && r.v[i] == pl->num ? 1 : 0;
                if (score > best) { best = score; x.x = rec_float(&r, i + 2); x.z = rec_float(&r, i + 4); }
                break;
            }
        }
        if (best && isfinite(x.x) && isfinite(x.z)) add_spot(h, &x);
    }
}

static const Held* held(int pi) {
    if (pi < 0 || pi >= g_nplaces) return NULL;
    if (!g_held && !(g_held = calloc((size_t)g_nplaces, sizeof(Held)))) return NULL;
    Held* h = &g_held[pi];
    if (!h->loaded) { h->loaded = 1; load_treasure(h, &g_places[pi]); load_shelves(h, &g_places[pi]); load_givers(h, &g_places[pi]); }
    return h;
}

/* Already dealt with: a container emptied, a quest done (a shelf never). */
static int spot_done(const Spot* b) { return b->kind == K_QUEST ? quest_state(b->idx) == 3 : b->kind != K_SHELF && flag_set(b->flag); }
/* Shown at all: a quest once the game offers it. */
static int spot_shown(const Spot* b) { return b->kind != K_QUEST || quest_state(b->idx) > 0; }
static int same_spot(const Spot* a, const Spot* b) { return fabsf(a->x - b->x) < 0.5f && fabsf(a->z - b->z) < 0.5f; }

/* ---------------------------------------------------------------- the map pictures */
typedef struct { float x, z; int n; int* rooms; } Door;          /* rooms: places (indices) behind it */
typedef struct { char name[12]; int nsame, ndoor; int* same; Door* door; } Pic;
static Pic* g_pics; static int g_npics;

static int in_list(const int* l, int n, int v) { for (int i = 0; i < n; i++) if (l[i] == v) return 1; return 0; }

static void add_int(int** l, int* n, int v) {
    if (v < 0) return;
    int* nl = realloc(*l, sizeof(int) * (size_t)(*n + 1));
    if (nl) { *l = nl; (*l)[(*n)++] = v; }
}

static void load_pics(void) {
    uint32_t n = 0, nn = 0;
    const uint8_t* pack = rom_file("data/pack_lv5/minimap.gp2", &n);
    Gpc g;
    if (!gpc_open(&g, pack, n)) return;
    uint8_t* names = gpc_names(&g, &nn);
    for (uint32_t q = 0; names && q < nn; q += (uint32_t)slen((const char*)names + q, nn - q) + 1) {
        char nm[40]; snprintf(nm, sizeof nm, "%.*s", (int)slen((const char*)names + q, nn - q), (const char*)names + q);
        size_t l = strlen(nm);
        if (l < 6 || strcmp(nm + l - 5, ".bmmp")) continue;
        uint32_t sz = 0;
        uint8_t* b = gpc_get(&g, nm, &sz);
        if (!b || sz < 8) { free(b); continue; }
        Pic* np = realloc(g_pics, sizeof(Pic) * (size_t)(g_npics + 1));
        if (!np) { free(b); break; }
        g_pics = np;
        Pic* pic = &g_pics[g_npics++];
        memset(pic, 0, sizeof *pic);
        uint32_t end = le32(b + 4), p = 8;
        if (end > sz) end = sz;
        Rec r;
        while (rec_next(b, end, &p, &r)) {
            if (r.tag == 0x66 && r.n >= 2 && r.type[1] == 0 && end + r.v[1] < sz)
                snprintf(pic->name, sizeof pic->name, "%.*s", (int)slen((const char*)b + end + r.v[1], sz - end - r.v[1]), (const char*)b + end + r.v[1]);
            else if (r.tag == 0x6b && r.n >= 1) add_int(&pic->same, &pic->nsame, place_index(r.v[0]));
            else if (r.tag == 0x6c && r.n >= 3 && r.type[0] == 2 && r.type[1] == 2) {
                Door* nd = realloc(pic->door, sizeof(Door) * (size_t)(pic->ndoor + 1));
                if (!nd) break;
                pic->door = nd;
                Door* d = &pic->door[pic->ndoor++];
                memset(d, 0, sizeof *d);
                d->x = rec_float(&r, 0); d->z = rec_float(&r, 1);
                for (int i = 2; i < r.n; i++) add_int(&d->rooms, &d->n, place_index(r.v[i]));
            }
        }
        free(b);
    }
    free(names);
    gpc_close(&g);
}

/* The picture shown (its name from the game's memory) as drawn for place pi:
 * of those with that name, the one drawing pi, else the one with pi behind a door. */
static const Pic* pic_for(int pi, const char* shown) {
    const Pic* best = NULL; int score = 0;
    for (int i = 0; i < g_npics; i++) {
        const Pic* p = &g_pics[i];
        if (strncmp(p->name, shown, 6)) continue;
        int s = in_list(p->same, p->nsame, pi) ? 3 : 1;
        for (int d = 0; s == 1 && d < p->ndoor; d++) if (in_list(p->door[d].rooms, p->door[d].n, pi)) s = 2;
        if (s > score) { score = s; best = p; }
    }
    return best;
}

/* ---------------------------------------------------------------- what is shown */
typedef struct {
    float sx, sy;
    int door;                       /* -1: a spot of this place (spot), -2: a monster, -3: the hero, else a building's door on the picture */
    int spot, opened;               /* a spot: which, emptied */
    int closed, emptied, shelves, recipes, quests;   /* a door: what its rooms hold (closed, emptied: chests) */
    int jars, jars_done, barrels;   /* a door: its pots and barrels, closed, emptied, how many are barrels */
} Mark;
#define MAX_MARKS 256
static Mark g_mark[MAX_MARKS]; static int g_nmark;
static float g_hx = -1, g_hy = -1; static int g_hover = -1;
static int g_here = -1, g_own, g_plan_on; static const Pic* g_pic;   /* this frame's place, drawn in its own units, on its plan, and picture */

void mapmarks_hover(float x, float y) { g_hx = x; g_hy = y; }

static int is_box(int kind) { return kind == K_CHEST || kind == K_GOLD || kind == K_BLUE || kind == K_UNKNOWN; }

static const char* kind_name(int kind) {
    static const char* const t[8][5] = {
        { "Treasure chest", "Coffre", "Schatztruhe", "Forziere", "Cofre" },
        { "Treasure chest", "Coffre", "Schatztruhe", "Forziere", "Cofre" },
        { "Blue chest", "Coffre bleu", "Blaue Truhe", "Forziere blu", "Cofre azul" },
        { "Pot", "Jarre", "Krug", "Giara", "Vasija" },
        { "Barrel", "Tonneau", "Fass", "Barile", "Barril" },
        { "Treasure chest", "Coffre", "Schatztruhe", "Forziere", "Cofre" },
        { "Bookshelf", "Biblioth\xC3\xA8que", "B\xC3\xBC" "cherregal", "Libreria", "Estanter\xC3\xAD" "a" },
        { "Quest", "Qu\xC3\xAA" "te", "Auftrag", "Missione", "Misi\xC3\xB3n" },
    };
    return t[kind][lang_index()];
}

/* What is in it, as the game would say it. */
static void content(const Spot* b, char* out, size_t cap) {
    static const char* const random_item[5] = { "Random item", "Objet au hasard", "Zuf\xC3\xA4lliger Gegenstand", "Oggetto a caso", "Objeto al azar" };
    static const char* const unknown[5] = { "Contents unknown", "Contenu inconnu", "Inhalt unbekannt", "Contenuto ignoto", "Contenido desconocido" };
    const char* nm = item_name(b->what);
    if (b->kind == K_GOLD) snprintf(out, cap, "%u G", (unsigned)b->what);
    else if (nm) snprintf(out, cap, "%s", nm);
    else if (b->what > 0 && b->what < 0x100) snprintf(out, cap, "%s", random_item[lang_index()]);
    else snprintf(out, cap, "%s", unknown[lang_index()]);
}

static const char* status(const Spot* b, int opened) {
    static const char* const open_chest[5] = { "Already opened", "D\xC3\xA9j\xC3\xA0 ouvert", "Schon ge\xC3\xB6" "ffnet", "Gi\xC3\xA0 aperto", "Ya abierto" };
    static const char* const closed_chest[5] = { "Not opened yet", "Pas encore ouvert", "Noch nicht ge\xC3\xB6" "ffnet", "Non ancora aperto", "A\xC3\xBA" "n sin abrir" };
    static const char* const searched[5] = { "Already searched", "D\xC3\xA9j\xC3\xA0 fouill\xC3\xA9", "Schon durchsucht", "Gi\xC3\xA0 frugato", "Ya registrado" };
    static const char* const not_searched[5] = { "Not searched yet", "Pas encore fouill\xC3\xA9", "Noch nicht durchsucht", "Non ancora frugato", "A\xC3\xBA" "n sin registrar" };
    return (is_box(b->kind) ? (opened ? open_chest : closed_chest) : (opened ? searched : not_searched))[lang_index()];
}

static const char* quest_status(int state) {
    static const char* const t[3][5] = {
        { "Not taken yet", "Pas encore accept\xC3\xA9" "e", "Noch nicht angenommen", "Non ancora accettata", "A\xC3\xBA" "n sin aceptar" },
        { "In progress", "En cours", "In Arbeit", "In corso", "En curso" },
        { "Completed", "Termin\xC3\xA9" "e", "Erledigt", "Completata", "Completada" },
    };
    return t[state < 1 ? 0 : state > 3 ? 2 : state - 1][lang_index()];
}

static const char* teaches(void) {
    static const char* const t[5] = { "Teaches alchemy recipes", "Apprend des recettes d'alchimie", "Lehrt Alchemie-Rezepte", "Insegna ricette alchemiche", "Ense\xC3\xB1" "a recetas de alquimia" };
    return t[lang_index()];
}

/* ---------------------------------------------------------------- drawing (premultiplied RGBA) */
static uint8_t g_img[MAPMARKS_W * MAPMARKS_H * 4];
#define TXT_H 440
static uint32_t g_txt[MAPMARKS_W * TXT_H];                       /* the tooltip's text, drawn alone */

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

#define EDGE 0x201008
static void glyph_chest(float cx, float cy, float R, uint32_t body, uint32_t band, float a) {   /* lid, band, lock */
    rbox(cx, cy, 3.2f * R, 2.5f * R, 0.8f * R, EDGE, a);
    rbox(cx, cy, 3.2f * R - 1.5f, 2.5f * R - 1.5f, 0.8f * R - 1, body, a);
    rbox(cx, cy - 0.5f * R, 3.2f * R - 1.5f, 0.3f * R, 0, band, a);
    rbox(cx, cy + 0.3f * R, 0.55f * R, 0.75f * R, 0.25f * R, band, a);
}
static void glyph_pot(float cx, float cy, float R, int opened, float a) {
    rbox(cx, cy, 2.4f * R, 2.4f * R, 2.4f * R, EDGE, a);
    rbox(cx, cy, 2.4f * R - 1.5f, 2.4f * R - 1.5f, 2.4f * R - 1.5f, opened ? 0x8A8A8A : 0xC07A44, a);
    rbox(cx, cy - 1.6f * R, 1.1f * R, 0.5f * R, 0.4f * R, opened ? 0x5A5A5A : 0x6A3A1A, a);   /* the mouth */
}
static void glyph_barrel(float cx, float cy, float R, int opened, float a) {
    rbox(cx, cy, 2.2f * R, 2.7f * R, 1.0f * R, EDGE, a);
    rbox(cx, cy, 2.2f * R - 1.5f, 2.7f * R - 1.5f, 1.0f * R - 1, opened ? 0x8A8A8A : 0xB98A4E, a);
    rbox(cx, cy - 1.1f * R, 2.0f * R, 0.25f * R, 0, EDGE, a * 0.8f);   /* the hoops */
    rbox(cx, cy + 1.1f * R, 2.0f * R, 0.25f * R, 0, EDGE, a * 0.8f);
}
static void glyph_book(float cx, float cy, float R, uint32_t cover, float a) {   /* cover, pages, spine */
    rbox(cx, cy, 2.7f * R, 3.0f * R, 0.5f * R, EDGE, a);
    rbox(cx, cy, 2.7f * R - 1.5f, 3.0f * R - 1.5f, 0.5f * R - 1, cover, a);
    rbox(cx + 1.75f * R, cy, 0.35f * R, 2.4f * R, 0, 0xF4ECD8, a);
    rbox(cx - 1.9f * R, cy, 0.3f * R, 2.7f * R, 0, EDGE, a * 0.5f);
    rbox(cx - 0.1f * R, cy - 0.8f * R, 1.0f * R, 0.3f * R, 0, 0xF2C94C, a);   /* the title */
}

/* A speech bubble (like the game's over the giver's head): "!" offered, "..." under way, a tick done. */
static void glyph_quest(float cx, float cy, float R, int state, float a) {
    uint32_t fill = state == 1 ? 0xF2C94C : state == 2 ? 0x4AA3E8 : 0x9A9A9A;
    for (int k = 0; k < 2; k++) {                                /* the edge, then the inside */
        float e = k ? 1.5f : 0;
        uint32_t c = k ? fill : EDGE;
        rbox(cx, cy - 0.4f * R, 3.0f * R - e, 2.4f * R - e, 1.2f * R - e, c, a);
        for (int i = 0; i < 3; i++)                              /* the tail, down to the left */
            rbox(cx - 1.2f * R - 0.45f * R * (float)i, cy + 2.0f * R + 0.45f * R * (float)i, 0.9f * R - 0.25f * R * (float)i - e * 0.5f, 0.45f * R - e * 0.3f, 0, c, a);
    }
    if (state == 1) {
        rbox(cx, cy - 1.0f * R, 0.42f * R, 1.1f * R, 0.3f * R, EDGE, a);
        rbox(cx, cy + 1.0f * R, 0.45f * R, 0.45f * R, 0.45f * R, EDGE, a);
    } else if (state == 2) {
        for (int i = -1; i <= 1; i++) rbox(cx + 1.3f * R * (float)i, cy - 0.4f * R, 0.45f * R, 0.45f * R, 0.45f * R, EDGE, a);
    } else {
        rbox(cx - 0.8f * R, cy - 0.1f * R, 0.35f * R, 0.8f * R, 0.2f * R, EDGE, a);
        rbox(cx + 0.4f * R, cy - 0.6f * R, 0.35f * R, 1.4f * R, 0.2f * R, EDGE, a);
    }
}

static uint32_t book_color(int recipes, int dull) { return dull ? 0x9A9A9A : recipes ? 0x8E55D8 : 0x2F9A5E; }

static void draw_mark(const Mark* m, int hover) {
    float R = MAPMARKS_R * 1.4f, cx = m->sx * MAPMARKS_R, cy = m->sy * MAPMARKS_R;
    if (m->door == -3) {                                         /* the hero: a blue dot */
        rbox(cx, cy, 2.4f * R, 2.4f * R, 2.4f * R, 0xFFFFFF, 1);
        rbox(cx, cy, 2.4f * R - 2, 2.4f * R - 2, 2.4f * R - 2, 0x2A6CE0, 1);
        return;
    }
    if (m->door == -2) {                                         /* a monster: a red dot */
        rbox(cx, cy, 1.9f * R, 1.9f * R, 1.9f * R, 0xFFF8E0, 0.8f);
        rbox(cx, cy, 1.9f * R - 1.5f, 1.9f * R - 1.5f, 1.9f * R - 1.5f, 0xE0302A, 1);
        return;
    }
    if (m->door >= 0) {                                          /* a building: its contents on a dark badge */
        int box = m->closed || m->emptied, jar = m->jars || m->jars_done, n = box + jar + (m->shelves > 0) + (m->quests > 0);
        float k = 0.75f, step = 5.6f * R, hw = (1.0f + 2.8f * (float)n) * R, hh = 3.4f * R;
        if (hover) rbox(cx, cy, hw + 0.8f * R, hh + 0.8f * R, 1.8f * R, 0xFFFFFF, 0.95f);
        rbox(cx, cy, hw, hh, 1.4f * R, 0xFFF8E0, 0.9f);
        rbox(cx, cy, hw - 2, hh - 2, 1.4f * R - 2, 0x3A2A1A, 0.92f);
        float x = cx - step * (float)(n - 1) / 2;
        if (m->quests) { glyph_quest(x, cy, R * k, m->opened, 1); x += step; }
        if (box) { glyph_chest(x, cy, R * k, m->closed ? 0xD0402E : 0x8A8A8A, m->closed ? 0xF2C94C : 0xB0B0B0, 1); x += step; }
        if (jar) {                                               /* the most common of the two */
            if (m->barrels * 2 >= m->jars + m->jars_done) glyph_barrel(x, cy, R * k, !m->jars, 1);
            else glyph_pot(x, cy, R * k, !m->jars, 1);
            x += step;
        }
        if (m->shelves) glyph_book(x, cy, R * k, book_color(m->recipes > 0, 0), 1);
        return;
    }
    const Spot* b = &held(g_here)->s[m->spot];
    float a = m->opened ? 0.6f : 1.0f;
    const Book* bk = b->kind == K_SHELF ? find_book(g_places[g_here].num, b->idx) : NULL;
    if (b->kind == K_SHELF && (!bk || bk->dull)) a = 0.75f;
    if (b->kind == K_QUEST) a = m->opened == 3 ? 0.6f : 1.0f;     /* opened: the quests' state there */
    /* a light halo: seen on the map's sand as on its grass */
    if (hover) rbox(cx, cy, 4.6f * R, 4.0f * R, 2.0f * R, 0xFFFFFF, 0.95f);
    else rbox(cx, cy, 3.9f * R, 3.3f * R, 1.6f * R, 0xFFF8E0, 0.55f * a);
    switch (b->kind) {
    case K_POT:
        glyph_pot(cx, cy, R, m->opened, a);
        break;
    case K_BARREL:
        glyph_barrel(cx, cy, R, m->opened, a);
        break;
    case K_SHELF:
        glyph_book(cx, cy, R, book_color(bk && bk->recipes, !bk || bk->dull), a);
        break;
    case K_QUEST:
        glyph_quest(cx, cy, R, m->opened, a);
        break;
    default:
        glyph_chest(cx, cy, R, m->opened ? 0x8A8A8A : b->kind == K_BLUE ? 0x3C78E0 : 0xD0402E, m->opened ? 0xB0B0B0 : 0xF2C94C, a);
        break;
    }
}

/* ---------------------------------------------------------------- the tooltip */
typedef struct { char s[140]; uint32_t c; } Line;
#define MAX_LINES 16
static Line g_line[MAX_LINES]; static int g_nline;
#define C_TEXT 0xFFFFFF
#define C_TODO 0x8EE28E
#define C_DONE 0xA8A8A8
#define C_RECIPE 0xC9A6FF
#define C_HEAD 0xF2C94C
#define C_QUEST 0x8CC8F2

static void add_line(uint32_t c, const char* s) {
    if (g_nline < MAX_LINES) { snprintf(g_line[g_nline].s, sizeof g_line[0].s, "%s", s); g_line[g_nline++].c = c; }
}

/* Text cut at the spaces into lines of about cols letters, at most max lines. */
static void add_wrapped(uint32_t c, const char* s, int cols, int max) {
    while (*s && max-- > 0) {
        int letters = 0; size_t cut = 0, i = 0;
        for (; s[i] && letters < cols; i++) {
            if (((unsigned char)s[i] & 0xC0) != 0x80) letters++;   /* not inside a UTF-8 letter */
            if (s[i] == ' ') cut = i;
        }
        if (!s[i] || !cut) cut = i;
        char line[140];
        if (max == 0 && s[cut]) snprintf(line, sizeof line, "%.*s...", (int)cut, s);
        else snprintf(line, sizeof line, "%.*s", (int)cut, s);
        add_line(c, line);
        s += cut;
        while (*s == ' ') s++;
    }
}

static void quest_title(int q, char* out, size_t cap) {
    static const char* const number[5] = { "Quest #%d", "Qu\xC3\xAA" "te n\xC2\xB0%d", "Auftrag Nr. %d", "Missione n. %d", "Misi\xC3\xB3n n.\xC2\xBA %d" };
    char head[40]; snprintf(head, sizeof head, number[lang_index()], g_quest[q].num);
    snprintf(out, cap, "%s: %s", head, g_quest[q].name[0] ? g_quest[q].name : "?");
}

static uint32_t quest_color(int state) { return state == 1 ? C_TODO : state == 2 ? C_QUEST : C_DONE; }

/* One spot's line(s); short: one line, for a building's list. */
static void spot_lines(int pi, const Spot* b, int opened, int is_short) {
    char what[100], line[140];
    if (b->kind == K_QUEST) {
        if (is_short) {
            int st = quest_state(b->idx);
            quest_title(b->idx, what, sizeof what);
            snprintf(line, sizeof line, "%s (%s)", what, quest_status(st));
            add_line(quest_color(st), line);
            return;
        }
        const Held* h = held(pi);                                /* everyone's quests asked there */
        for (int i = 0; h && i < h->n; i++) {
            const Spot* o = &h->s[i];
            int st = o->kind == K_QUEST && same_spot(o, b) ? quest_state(o->idx) : 0;
            if (!st || (st == 3 && !g_mapmarks_opened)) continue;
            if (g_nline) add_line(C_TEXT, "");
            quest_title(o->idx, line, sizeof line);
            add_line(st == 3 ? C_DONE : C_HEAD, line);
            if (st != 3 && g_quest[o->idx].desc[0]) add_wrapped(C_TEXT, g_quest[o->idx].desc, 52, 3);
            add_line(quest_color(st), quest_status(st));
        }
        return;
    }
    if (b->kind == K_SHELF) {
        const Book* bk = find_book(g_places[pi].num, b->idx);
        if (is_short) {
            snprintf(line, sizeof line, "%s: %s", kind_name(K_SHELF), bk ? bk->title : "?");
            add_line(bk && bk->recipes ? C_RECIPE : C_TEXT, line);
            return;
        }
        if (!bk || bk->dull) { add_line(C_TEXT, kind_name(K_SHELF)); if (bk) add_line(C_DONE, bk->title); return; }
        snprintf(line, sizeof line, "%s: %s", kind_name(K_SHELF), bk->title);
        add_line(C_TEXT, line);
        if (bk->recipes) add_line(C_RECIPE, teaches());
        return;
    }
    content(b, what, sizeof what);
    snprintf(line, sizeof line, "%s: %s", kind_name(b->kind), what);
    if (is_short) { add_line(opened ? C_DONE : C_TODO, line); return; }
    add_line(C_TEXT, line);
    add_line(opened ? C_DONE : C_TODO, status(b, opened));
}

/* A building: what its rooms hold, the best first (quests, chests, books,
 * then pots and barrels, then what was already taken or done), the same
 * ones counted once. */
static void door_lines(const Mark* m) {
    static const char* const inside[5] = { "Inside", "\xC3\x80 l'int\xC3\xA9rieur", "Drinnen", "All'interno", "Dentro" };
    static const char* const more[5] = { "+%d more", "+%d de plus", "+%d weitere", "+%d altri", "+%d m\xC3\xA1s" };
    enum { MAX_ITEMS = 64 };
    static struct { Line l; int rank, n; } item[MAX_ITEMS];
    enum { MAX_SEEN = 512 };
    static const Spot* seen[MAX_SEEN];
    int nitem = 0, extra = 0, nseen = 0;
    const Door* d = &g_pic->door[m->door];
    for (int r = 0; r < d->n; r++) {
        if (g_own && d->rooms[r] == g_here) continue;            /* already shown */
        const Held* h = held(d->rooms[r]);
        for (int i = 0; h && i < h->n; i++) {
            const Spot* b = &h->s[i];
            /* the same room in its other versions (the inn grows as guests
             * come): the same chest (its flag) or shelf (where it stands) */
            int dup = !spot_shown(b);
            for (int k = 0; k < nseen && !dup; k++)
                dup = seen[k]->kind == b->kind && (b->kind == K_SHELF ? same_spot(seen[k], b)
                                                   : b->kind == K_QUEST ? seen[k]->idx == b->idx : seen[k]->flag == b->flag);
            if (dup) continue;
            if (nseen < MAX_SEEN) seen[nseen++] = b;
            int opened = spot_done(b), rank;
            if (b->kind == K_QUEST && !opened) rank = 0;
            else if (b->kind == K_SHELF) {
                const Book* bk = find_book(g_places[d->rooms[r]].num, b->idx);
                if (!bk || bk->dull) continue;
                rank = 2;
            } else if (opened) {
                if (!g_mapmarks_opened) continue;
                rank = 4;
            } else rank = b->kind == K_CHEST || b->kind == K_GOLD ? 1 : 3;
            g_nline = 0;
            spot_lines(d->rooms[r], b, opened, 1);
            int k;
            for (k = 0; k < nitem; k++) if (item[k].rank == rank && !strcmp(item[k].l.s, g_line[0].s)) break;
            if (k < nitem) item[k].n++;
            else if (nitem < MAX_ITEMS) { item[nitem].l = g_line[0]; item[nitem].rank = rank; item[nitem++].n = 1; }
            else extra++;
        }
    }
    g_nline = 0;
    add_line(C_HEAD, inside[lang_index()]);
    for (int rank = 0; rank < 5; rank++)
        for (int k = 0; k < nitem; k++) {
            if (item[k].rank != rank) continue;
            if (g_nline >= MAX_LINES - 1) { extra += item[k].n; continue; }
            char s[160];
            if (item[k].n > 1) snprintf(s, sizeof s, "%s  \xC3\x97%d", item[k].l.s, item[k].n);
            else snprintf(s, sizeof s, "%s", item[k].l.s);
            add_line(item[k].l.c, s);
        }
    if (extra) { char s[40]; snprintf(s, sizeof s, more[lang_index()], extra); add_line(C_DONE, s); }
}

static void draw_tooltip(const Mark* m) {
    g_nline = 0;
    if (m->door >= 0) door_lines(m);
    else spot_lines(g_here, &held(g_here)->s[m->spot], m->opened, 0);
    if (!g_nline) return;
    float px = 7.5f * MAPMARKS_R;                                /* text height */
    int lh = (int)(px * 1.2f), pad = 2 * MAPMARKS_R, tw = 0, th;
    if (g_nline * lh > TXT_H) g_nline = TXT_H / lh;
    memset(g_txt, 0, sizeof(uint32_t) * MAPMARKS_W * (size_t)(g_nline * lh));
    for (int i = 0; i < g_nline; i++) {
        int w = text_draw(g_txt, MAPMARKS_W, g_nline * lh, 0, i * lh, px, 0xFFFFFF, g_line[i].s);
        if (w > tw) tw = w;
    }
    tw += 2 * pad; th = g_nline * lh + 2 * pad;
    if (tw > MAPMARKS_W - 4) tw = MAPMARKS_W - 4;
    int x0 = (int)(m->sx * MAPMARKS_R) - tw / 2, y0 = (int)((m->sy - 8) * MAPMARKS_R) - th;
    if (y0 < 2) y0 = (int)((m->sy + 8) * MAPMARKS_R);           /* no room above: below */
    if (y0 + th > MAPMARKS_H - 2) y0 = MAPMARKS_H - 2 - th;      /* ...nor there: over it */
    if (y0 < 2) y0 = 2;
    if (x0 < 2) x0 = 2;
    if (x0 + tw > MAPMARKS_W - 2) x0 = MAPMARKS_W - 2 - tw;
    rbox(x0 + tw / 2.0f, y0 + th / 2.0f, tw / 2.0f, th / 2.0f, 2.0f * MAPMARKS_R, 0x101418, 0.88f);
    for (int y = 0; y < g_nline * lh; y++)
        for (int x = 0; x < tw - 2 * pad; x++) {
            uint32_t t = g_txt[y * MAPMARKS_W + x];
            if (t) blend(x0 + pad + x, y0 + pad + y, g_line[y / lh].c, (float)((t >> 16) & 255) / 255.0f);
        }
}

/* ---------------------------------------------------------------- a building's plan */
/* Inside a house the game shows the town around it. The room's walls are in
 * its collision: data/map/<C01M05>.amdj (NARC) holds C01A0500.col2 (DS LZ):
 * at +20 the triangles' count, at +36 where they start, 28 bytes each: three
 * corners (x, height, z: s16, 1024 per field unit), the normal (s16, 4.12)
 * and two attributes. The upright ones (the walls) seen from above outline
 * the floor: everything the outside doesn't reach. */
#define PLAN_GAP 3                 /* overlay pixels: gaps in the walls up to twice this wide are closed */
#define EDGE_EPS 300.0f            /* collision units (1024 per field unit): a wall ending this close to the plan's edge ends on it */
#define EXIT_W (10 * 1024.0f)      /* ...a way out between two of them at most this wide */
#define EXITS_MAX 256
#define PLAN_TOP 15                /* top-screen rows: the place's name above (on the right), the party's names below, */
#define PLAN_BOTTOM 176            /* left uncovered */
#define PLAN_NAME_X 198
#define PLAN_WALL 2.5f             /* ...the walls' half thickness */
static uint8_t g_plan[MAPMARKS_W * MAPMARKS_H];                 /* 0 outside, 1 floor, 2 wall */
static uint8_t g_wall[MAPMARKS_W * MAPMARKS_H];
static int* g_fill;
static int g_plan_of = -1, g_plan_ok;                            /* the place it is of, drawn at all */
static float g_plan_k, g_plan_ox, g_plan_oy;                     /* a point (x, z) of the field at (x * k + ox, z * k + oy) on the top screen */

static void plan_line(uint8_t* m, float x0, float y0, float x1, float y1, float r) {   /* overlay pixels */
    int ax = (int)floorf(fminf(x0, x1) - r), bx = (int)ceilf(fmaxf(x0, x1) + r);
    int ay = (int)floorf(fminf(y0, y1) - r), by = (int)ceilf(fmaxf(y0, y1) + r);
    float dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy;
    for (int y = ay < 0 ? 0 : ay; y <= by && y < MAPMARKS_H; y++)
        for (int x = ax < 0 ? 0 : ax; x <= bx && x < MAPMARKS_W; x++) {
            float px = (float)x + 0.5f - x0, py = (float)y + 0.5f - y0;
            float t = l2 > 0 ? (px * dx + py * dy) / l2 : 0;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            float ex = px - t * dx, ey = py - t * dy;
            if (ex * ex + ey * ey <= r * r) m[y * MAPMARKS_W + x] = 1;
        }
}

/* The outside (3): from the edges over the pixels left 0. */
static void plan_outside(void) {
    int n = 0;
    for (int i = 0; i < MAPMARKS_W * MAPMARKS_H; i++) {
        int x = i % MAPMARKS_W, y = i / MAPMARKS_W;
        if ((x == 0 || y == 0 || x == MAPMARKS_W - 1 || y == MAPMARKS_H - 1) && !g_plan[i]) { g_plan[i] = 3; g_fill[n++] = i; }
    }
    while (n) {
        int i = g_fill[--n], x = i % MAPMARKS_W, y = i / MAPMARKS_W;
        int nb[4] = { x > 0 ? i - 1 : -1, x < MAPMARKS_W - 1 ? i + 1 : -1, y > 0 ? i - MAPMARKS_W : -1, y < MAPMARKS_H - 1 ? i + MAPMARKS_W : -1 };
        for (int k = 0; k < 4; k++) if (nb[k] >= 0 && !g_plan[nb[k]]) { g_plan[nb[k]] = 3; g_fill[n++] = nb[k]; }
    }
}

/* The floor (0) grown back over what the thick walls took (2, not a real
 * wall), up to PLAN_GAP pixels: doorways and gaps then read as floor. */
static void plan_regrow(void) {
    int head = 0, tail = 0;
    static uint8_t dist[MAPMARKS_W * MAPMARKS_H];
    for (int i = 0; i < MAPMARKS_W * MAPMARKS_H; i++) { dist[i] = 0; if (!g_plan[i]) g_fill[tail++] = i; }
    while (head < tail) {
        int i = g_fill[head++], x = i % MAPMARKS_W, y = i / MAPMARKS_W;
        if (dist[i] >= PLAN_GAP) continue;
        int nb[4] = { x > 0 ? i - 1 : -1, x < MAPMARKS_W - 1 ? i + 1 : -1, y > 0 ? i - MAPMARKS_W : -1, y < MAPMARKS_H - 1 ? i + MAPMARKS_W : -1 };
        for (int k = 0; k < 4; k++) {
            int j = nb[k];
            if (j < 0 || g_plan[j] != 2 || g_wall[j]) continue;
            g_plan[j] = 0; dist[j] = (uint8_t)(dist[i] + 1); g_fill[tail++] = j;
        }
    }
}

static int build_plan(const Place* pl) {
    size_t len = strlen(pl->code);
    if (len < 6 || pl->code[3] != 'M') return 0;                 /* a town or a field: it has its map */
    char path[48], name[24];
    snprintf(path, sizeof path, "data/map/%s.amdj", pl->code);
    snprintf(name, sizeof name, "%.3sA%s00.col2", pl->code, pl->code + 4);
    uint32_t n = 0, sz = 0;
    const uint8_t* amdj = rom_file(path, &n), *packed = narc_file(amdj, n, name, &sz);
    uint8_t* c = packed ? ds_unpack(packed, sz, &sz) : NULL;
    if (!c || sz < 60) { free(c); return 0; }
    uint32_t count = le32(c + 20), start = le32(c + 36);
    if (start < 40 || start > sz || count > (sz - start) / 28) count = 0;
    /* the walls: each upright triangle's two farthest corners, from above */
    float* seg = malloc(sizeof(float) * 4 * (count + EXITS_MAX));
    int ns = 0;
    float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
    for (uint32_t t = 0; seg && t < count; t++) {
        const uint8_t* p = c + start + 28 * t;
        int16_t v[12];
        for (int k = 0; k < 12; k++) v[k] = (int16_t)le16(p + 2 * k);
        if (abs(v[10]) > 1200) continue;                         /* a floor, a slope, a ceiling */
        int a = 0, b = 1; float best = -1;
        for (int i = 0; i < 3; i++)
            for (int j = i + 1; j < 3; j++) {
                float dx = (float)(v[3 * i] - v[3 * j]), dz = (float)(v[3 * i + 2] - v[3 * j + 2]);
                if (dx * dx + dz * dz > best) { best = dx * dx + dz * dz; a = i; b = j; }
            }
        if (best < 1) continue;
        float* s = &seg[4 * ns++];
        s[0] = v[3 * a]; s[1] = v[3 * a + 2]; s[2] = v[3 * b]; s[3] = v[3 * b + 2];
        x0 = fminf(x0, fminf(s[0], s[2])); x1 = fmaxf(x1, fmaxf(s[0], s[2]));
        z0 = fminf(z0, fminf(s[1], s[3])); z1 = fmaxf(z1, fmaxf(s[1], s[3]));
    }
    free(c);
    /* a way out ends on the plan's edge, open: closed there, between two
     * walls ending on the same side less than EXIT_W apart */
    for (int side = 0; seg && ns >= 4 && side < 4; side++) {
        float e[64]; int ne = 0;
        int ns0 = ns;
        for (int i = 0; i < ns0 && ne < 64; i++)
            for (int j = 0; j < 2 && ne < 64; j++) {
                float x = seg[4 * i + 2 * j], z = seg[4 * i + 2 * j + 1];
                float d = side == 0 ? x - x0 : side == 1 ? x1 - x : side == 2 ? z - z0 : z1 - z;
                if (d < EDGE_EPS) e[ne++] = side < 2 ? z : x;
            }
        for (int i = 1; i < ne; i++) for (int j = i; j > 0 && e[j] < e[j - 1]; j--) { float t = e[j]; e[j] = e[j - 1]; e[j - 1] = t; }
        for (int i = 0; i + 1 < ne && ns < (int)count + EXITS_MAX; i++) {
            if (e[i + 1] - e[i] < 64 || e[i + 1] - e[i] > EXIT_W) continue;
            float* q = &seg[4 * ns++];
            float at = side == 0 ? x0 : side == 1 ? x1 : side == 2 ? z0 : z1;
            if (side < 2) { q[0] = at; q[1] = e[i]; q[2] = at; q[3] = e[i + 1]; }
            else { q[0] = e[i]; q[1] = at; q[2] = e[i + 1]; q[3] = at; }
        }
    }
    if (ns < 4 || x1 - x0 < 1024 || z1 - z0 < 1024 || (!g_fill && !(g_fill = malloc(sizeof(int) * MAPMARKS_W * MAPMARKS_H)))) { free(seg); return 0; }
    /* as big as the top screen shows it between the place's name and the party's names */
    float k = fminf((256 - 28) / (x1 - x0), (PLAN_BOTTOM - PLAN_TOP - 12) / (z1 - z0));   /* DS pixels per collision unit */
    g_plan_k = k * 1024;
    g_plan_ox = 128 - (x0 + x1) / 2 * k; g_plan_oy = (PLAN_TOP + PLAN_BOTTOM) / 2.0f - (z0 + z1) / 2 * k;
    /* the walls drawn thick (the outside can't slip through a doorway), the
     * outside found, then the walls thin again: what they gave up goes to
     * the outside where it touches it, to the floor elsewhere */
    memset(g_plan, 0, sizeof g_plan); memset(g_wall, 0, sizeof g_wall);
    float R = MAPMARKS_R;
    for (int i = 0; i < ns; i++) {
        const float* s = &seg[4 * i];
        float ax = (s[0] * k + g_plan_ox) * R, ay = (s[1] * k + g_plan_oy) * R, bx = (s[2] * k + g_plan_ox) * R, by = (s[3] * k + g_plan_oy) * R;
        plan_line(g_plan, ax, ay, bx, by, PLAN_GAP);
        plan_line(g_wall, ax, ay, bx, by, PLAN_WALL);
    }
    free(seg);
    for (int i = 0; i < MAPMARKS_W * MAPMARKS_H; i++) g_plan[i] *= 2;
    plan_outside();
    int floor = 0;
    for (int i = 0; i < MAPMARKS_W * MAPMARKS_H; i++) floor += g_plan[i] == 0;
    if (floor < MAPMARKS_W * MAPMARKS_H / 40) return 0;          /* the walls don't close: no plan */
    plan_regrow();
    for (int i = 0; i < MAPMARKS_W * MAPMARKS_H; i++) g_plan[i] = g_wall[i] ? 2 : g_plan[i] == 0 ? 1 : 0;
    return 1;
}

/* The plan in the town maps' style: a lighter plaque on mottled parchment,
 * a soft shadow under it, a darker band along the walls inside, a thin
 * brown line for the walls, the furniture (floor the room's main part
 * doesn't reach) a shade darker. Drawn once per room into g_plan_img. */
static uint8_t g_plan_img[MAPMARKS_W * MAPMARKS_H * 4];
static float g_fa[MAPMARKS_W * MAPMARKS_H], g_fb[MAPMARKS_W * MAPMARKS_H];

static float hash01(int x, int y, int seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}
/* Smooth value noise, about -0.5..0.5, features `cell` pixels wide. */
static float vnoise(float x, float y, float cell, int seed) {
    x /= cell; y /= cell;
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - (float)ix, fy = y - (float)iy;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    float a = hash01(ix, iy, seed), b = hash01(ix + 1, iy, seed), c = hash01(ix, iy + 1, seed), d = hash01(ix + 1, iy + 1, seed);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy - 0.5f;
}

/* A box blur of radius r over m (3 passes make it close to a Gaussian), using g_fb. */
static void blur_box(float* m, int r) {
    for (int pass = 0; pass < 3; pass++) {
        for (int y = 0; y < MAPMARKS_H; y++) {                   /* rows into g_fb */
            const float* row = &m[y * MAPMARKS_W];
            float sum = 0;
            for (int x = -r; x <= r; x++) sum += row[x < 0 ? 0 : x >= MAPMARKS_W ? MAPMARKS_W - 1 : x];
            for (int x = 0; x < MAPMARKS_W; x++) {
                g_fb[y * MAPMARKS_W + x] = sum / (float)(2 * r + 1);
                int out = x - r, in = x + r + 1;
                sum += row[in >= MAPMARKS_W ? MAPMARKS_W - 1 : in] - row[out < 0 ? 0 : out];
            }
        }
        for (int x = 0; x < MAPMARKS_W; x++) {                   /* columns back into m */
            float sum = 0;
            for (int y = -r; y <= r; y++) sum += g_fb[(y < 0 ? 0 : y >= MAPMARKS_H ? MAPMARKS_H - 1 : y) * MAPMARKS_W + x];
            for (int y = 0; y < MAPMARKS_H; y++) {
                m[y * MAPMARKS_W + x] = sum / (float)(2 * r + 1);
                int out = y - r, in = y + r + 1;
                sum += g_fb[(in >= MAPMARKS_H ? MAPMARKS_H - 1 : in) * MAPMARKS_W + x] - g_fb[(out < 0 ? 0 : out) * MAPMARKS_W + x];
            }
        }
    }
}

/* Marks 3 the floor (1) apart from the room's biggest part: furniture and the like. */
static void plan_furniture(void) {
    static int lab[MAPMARKS_W * MAPMARKS_H];
    int n = 0, best = 0, best_size = 0;
    memset(lab, 0, sizeof lab);
    for (int s = 0; s < MAPMARKS_W * MAPMARKS_H; s++) {
        if (g_plan[s] != 1 || lab[s]) continue;
        int size = 0, top = 0;
        lab[s] = ++n; g_fill[top++] = s;
        while (top) {
            int i = g_fill[--top], x = i % MAPMARKS_W, y = i / MAPMARKS_W;
            size++;
            int nb[4] = { x > 0 ? i - 1 : -1, x < MAPMARKS_W - 1 ? i + 1 : -1, y > 0 ? i - MAPMARKS_W : -1, y < MAPMARKS_H - 1 ? i + MAPMARKS_W : -1 };
            for (int k = 0; k < 4; k++) if (nb[k] >= 0 && g_plan[nb[k]] == 1 && !lab[nb[k]]) { lab[nb[k]] = n; g_fill[top++] = nb[k]; }
        }
        if (size > best_size) { best_size = size; best = n; }
    }
    for (int i = 0; i < MAPMARKS_W * MAPMARKS_H; i++) if (lab[i] && lab[i] != best) g_plan[i] = 3;
}

static void paint_plan(void) {
    const int N = MAPMARKS_W * MAPMARKS_H;
    plan_furniture();
    /* the shadow: the plan's shape moved down right, blurred, outside it only */
    for (int y = 0; y < MAPMARKS_H; y++)
        for (int x = 0; x < MAPMARKS_W; x++) {
            int sx = x - 5, sy = y - 6;
            g_fa[y * MAPMARKS_W + x] = sx >= 0 && sy >= 0 && g_plan[sy * MAPMARKS_W + sx] ? 1.0f : 0.0f;
        }
    blur_box(g_fa, 3);
    static float band[MAPMARKS_W * MAPMARKS_H];
    for (int i = 0; i < N; i++) band[i] = g_plan[i] == 2 ? 1.0f : 0.0f;
    blur_box(band, 2);
    for (int y = 0; y < MAPMARKS_H; y++)
        for (int x = 0; x < MAPMARKS_W; x++) {
            int i = y * MAPMARKS_W + x;
            uint8_t v = g_plan[i];
            float fx = (float)x, fy = (float)y, r, g, b;
            if (v == 2) { r = 120; g = 86; b = 46; }             /* the walls */
            else if (v == 3) { r = 208; g = 164; b = 92; }       /* furniture */
            else if (v == 1) {                                   /* the floor: light, a fine grain, darker along the walls */
                float k = (1 + 0.05f * vnoise(fx, fy, 20, 3)) * (1 - 0.16f * band[i]);
                r = 236 * k; g = 204 * k; b = 138 * k;
            } else {                                             /* the parchment: mottled, stains, the shadow */
                float n = 0.5f * vnoise(fx, fy, 96, 1) + 0.3f * vnoise(fx, fy, 32, 2) + 0.2f * vnoise(fx, fy, 8, 4);
                float st = vnoise(fx, fy, 128, 5) * 3; st = st < 0 ? 0 : st > 1 ? 1 : st;
                float k = (1 + 0.10f * n) * (1 - 0.10f * st) * (1 - 0.30f * g_fa[i]);
                r = 214 * k; g = 163 * k; b = 70 * k;
            }
            uint8_t* p = &g_plan_img[(size_t)i * 4];
            p[0] = (uint8_t)(r > 255 ? 255 : r); p[1] = (uint8_t)(g > 255 ? 255 : g); p[2] = (uint8_t)(b > 255 ? 255 : b); p[3] = 255;
        }
}

/* The plan as the picture's background; the game's place name and party names stay seen. */
static void draw_plan(void) {
    memcpy(g_img, g_plan_img, sizeof g_img);
    for (int y = 0; y < MAPMARKS_H; y++)
        for (int x = 0; x < MAPMARKS_W; x++)
            if (y >= PLAN_BOTTOM * MAPMARKS_R || (y < PLAN_TOP * MAPMARKS_R && x >= PLAN_NAME_X * MAPMARKS_R))
                memset(&g_img[((size_t)y * MAPMARKS_W + x) * 4], 0, 4);
}

/* ---------------------------------------------------------------- once a frame */
static void add_mark(const Mark* m) { if (g_nmark < MAX_MARKS) g_mark[g_nmark++] = *m; }

static int on_screen(float sx, float sy) { return sx >= -4 && sx <= 260 && sy >= -4 && sy <= 196; }

/* The top screen shows a page instead of the map: an item's card (Items,
 * Equipment...). The map (towns: BG0CNT 4E01, BG1CNT 0D02; the fields:
 * 0F01, 1002) leaves BG2CNT at 0; the cards set it (0F00, 0E03, 0002).
 * A battle leaves BG2CNT set (0704) once back on the map: the map's BG0 is
 * the one at priority 1, a card's at 0. */
int mapmarks_top_page(void) {
    uint32_t top = (IO16(0x304) & 0x8000) ? 0x0000 : 0x1000;
    return rd32(FIELD_PTR) != 0 && IO16(top + 0x0C) != 0 && (IO16(top + 0x08) & 3) != 1;
}

int mapmarks_update(const uint8_t** rgba) {
    static int loaded, had;
    static uint32_t last_key;
    *rgba = NULL;
    g_nmark = 0; g_here = -1; g_own = 0; g_plan_on = 0; g_pic = NULL;
    int ok = (g_mapmarks || g_interior_maps) && rd32(FIELD_PTR) != 0;
    uint32_t mp = rd32(MINIMAP_PTR);
    if (ok && (mp < 0x02000000 || mp >= 0x02400000)) ok = 0;
    if (ok && mapmarks_top_page()) ok = 0;                       /* an item's card: nothing of the map over it */
    /* the map is the top screen's BG1: off when a page covers it (the quest log...) */
    uint32_t top = (IO16(0x304) & 0x8000) ? 0x0000 : 0x1000, bright = IO16(top + 0x6C);
    int field = rd16(MAP_NUM) >= 20000;                          /* the fields (20001...) draw their map on BG0 */
    if (ok && !(IO32(top) & (field ? 0x100 : 0x200))) ok = 0;
    if (ok && (bright >> 14) == 2 && (bright & 0x1F) >= 16) ok = 0;   /* faded to black */
    if (ok && !loaded) {
        loaded = 1;
        load_places();
        g_tbox = rom_file("data/scenario/treasure.nsarc", &g_tbox_n);
        load_pics();
        LOG_INFO("mapmarks: %d places, %d map pictures\n", g_nplaces, g_npics);
    }
    int pi = ok && !field ? place_index(rd16(MAP_NUM)) : -1;     /* (a field's map isn't drawn like a town's: no spots there) */
    if (pi >= 0) {
        char shown[10];
        memcpy(shown, &g_main_ram[(mp + 2) & (MAIN_RAM_SIZE - 1)], 9); shown[9] = 0;
        float scale = (float)(int32_t)rd32(mp + 0x38) / 4096.0f;
        float cx = (float)(int32_t)rd32(mp + 0x44), cy = (float)(int32_t)rd32(mp + 0x48);
        if (scale > 0.1f && scale < 64) {
            g_here = pi;
            g_pic = pic_for(pi, shown);
            load_names(g_fw_language);
            load_books(g_fw_language);
            load_quests();
            load_quest_texts(g_fw_language);
            /* this place's own spots: when the picture is drawn in its units
             * (not a house's: it shows the town around it) */
            const Place* pl = &g_places[pi];
            int own = g_own = g_pic ? in_list(g_pic->same, g_pic->nsame, pi) : strlen(pl->map) >= 6 && !strncmp(shown, pl->map, 6);
            /* a house's: drawn on the room's own plan instead, if it has one */
            if (!own && g_interior_maps) {
                if (g_plan_of != pi) {
                    g_plan_of = pi; g_plan_ok = build_plan(pl);
                    if (g_plan_ok) paint_plan();
                    LOG_INFO("mapmarks: %s, %s\n", pl->code, g_plan_ok ? "plan drawn from its walls" : "no plan");
                }
                g_plan_on = g_plan_ok;
            }
            float k = g_plan_on ? g_plan_k : scale, ox = g_plan_on ? g_plan_ox : 128 - cx, oy = g_plan_on ? g_plan_oy : 96 - cy;
            const Held* h = (own || g_plan_on) && g_mapmarks ? held(pi) : NULL;
            for (int i = 0; h && i < h->n; i++) {
                const Spot* b = &h->s[i];
                Mark m = { b->x * k + ox, b->z * k + oy, -1, i, spot_done(b), 0, 0, 0, 0, 0 };
                if (!on_screen(m.sx, m.sy) || !spot_shown(b) || (m.opened && !g_mapmarks_opened)) continue;
                if (b->kind == K_QUEST) {                            /* one bubble per person: offered first, then under way */
                    int st = quest_state(b->idx), k;
                    m.opened = st;
                    for (k = 0; k < g_nmark; k++) {
                        const Spot* o = g_mark[k].door < 0 ? &h->s[g_mark[k].spot] : NULL;
                        if (o && o->kind == K_QUEST && same_spot(o, b)) break;
                    }
                    if (k < g_nmark) { if (st < g_mark[k].opened) g_mark[k].opened = st; continue; }
                }
                add_mark(&m);
            }
            /* the buildings: a badge on the door if their rooms hold anything */
            for (int d = 0; g_pic && g_mapmarks && !g_plan_on && d < g_pic->ndoor; d++) {
                const Door* door = &g_pic->door[d];
                Mark m = { door->x * scale - cx + 128, door->z * scale - cy + 96, d, -1, 0, 0, 0, 0, 0, 0 };
                if (!on_screen(m.sx, m.sy)) continue;
                for (int r = 0; r < door->n; r++) {
                    if (own && door->rooms[r] == pi) continue;       /* already shown */
                    const Held* hr = held(door->rooms[r]);
                    for (int i = 0; hr && i < hr->n; i++) {
                        const Spot* b = &hr->s[i];
                        if (b->kind == K_SHELF) {
                            const Book* bk = find_book(g_places[door->rooms[r]].num, b->idx);
                            if (bk && !bk->dull) { m.shelves++; m.recipes += bk->recipes; }
                        } else if (b->kind == K_QUEST) {
                            int st = quest_state(b->idx);
                            if (st == 1 || st == 2) { m.quests++; if (!m.opened || st < m.opened) m.opened = st; }   /* a door's opened: its best quest state */
                        } else if (!is_box(b->kind)) {                       /* a pot or a barrel */
                            if (flag_set(b->flag)) m.jars_done++; else m.jars++;
                            if (b->kind == K_BARREL) m.barrels++;
                        } else if (flag_set(b->flag)) m.emptied++;
                        else m.closed++;
                    }
                }
                if (!g_mapmarks_opened) m.emptied = m.jars_done = 0;
                if (m.closed || m.emptied || m.jars || m.jars_done || m.shelves || m.quests) add_mark(&m);
            }
            /* on a plan, the hero: field character 0 (farm.c) */
            uint32_t a = rd32(FIELD_PTR);
            if (g_plan_on && a >= 0x02000000 && a < 0x02400000) {
                float x = (float)(int32_t)rd32(a + 0x44) / 4096.0f, z = (float)(int32_t)rd32(a + 0x4C) / 4096.0f;
                Mark m = { x * k + ox, z * k + oy, -3, -1, 0, 0, 0, 0, 0, 0 };
                if (on_screen(m.sx, m.sy)) add_mark(&m);
            }
        }
    }
    /* the monsters walking around: a town's picture scrolls (its centre at
     * +0x44/+0x48), a field's shows the whole field around its origin */
    if (ok && g_mapmarks && g_mapmarks_monsters && !g_plan_on) {
        uint16_t here = rd16(MAP_NUM);
        float scale = (float)(int32_t)rd32(mp + 0x38) / 4096.0f;
        float cx = field ? 0 : (float)(int32_t)rd32(mp + 0x44), cy = field ? 0 : (float)(int32_t)rd32(mp + 0x48);
        for (int k = 0; scale > 0.1f && scale < 64 && k < MON_COUNT; k++) {
            uint32_t a = rd32(FIELD_PTR + 4 * (MON_FIRST + k));
            if (a < 0x02000000 || a >= 0x02400000 || (rd32(a + 4) >> 16) != here) continue;   /* there, on this map */
            float x = (float)(int32_t)rd32(a + 0x44) / 4096.0f, z = (float)(int32_t)rd32(a + 0x4C) / 4096.0f;
            Mark m = { x * scale - cx + 128, z * scale - cy + 96, -2, -1, 0, 0, 0, 0, 0, 0 };
            if (on_screen(m.sx, m.sy)) add_mark(&m);
        }
    }
    /* the mark under the mouse (the nearest) */
    g_hover = -1;
    float best = 64;                                             /* within 8 pixels */
    for (int i = 0; i < g_nmark; i++) {
        float dx = g_mark[i].sx - g_hx, dy = g_mark[i].sy - g_hy;
        if (g_hx >= 0 && g_mark[i].door > -2 && dx * dx + dy * dy < best) { best = dx * dx + dy * dy; g_hover = i; }
    }
    /* redraw only when something changed */
    uint32_t key = 2166136261u;
    for (int i = 0; i < g_nmark; i++) {
        const Mark* m = &g_mark[i];
        int v[11] = { (int)lroundf(m->sx * MAPMARKS_R), (int)lroundf(m->sy * MAPMARKS_R), m->door, m->spot, m->opened, m->closed, m->emptied, m->shelves, m->quests, m->jars, m->jars_done };
        for (int k = 0; k < 11; k++) key = (key ^ (uint32_t)v[k]) * 16777619u;
    }
    key = (key ^ (uint32_t)(g_hover + 2)) * 16777619u;
    key = (key ^ (uint32_t)g_fw_language) * 16777619u;
    key = (key ^ (uint32_t)g_here) * 16777619u;
    key = (key ^ (uint32_t)g_plan_on) * 16777619u;
    if (!g_nmark && !g_plan_on) { int changed = had; had = 0; last_key = 0; return changed; }
    if (had && key == last_key) return 0;
    had = 1; last_key = key;
    if (g_plan_on) draw_plan();
    else memset(g_img, 0, sizeof g_img);
    for (int pass = 0; pass < 2; pass++)                          /* the monsters first, under the rest */
        for (int i = 0; i < g_nmark; i++) if (i != g_hover && (g_mark[i].door == -2) == !pass) draw_mark(&g_mark[i], 0);
    if (g_hover >= 0) { draw_mark(&g_mark[g_hover], 1); draw_tooltip(&g_mark[g_hover]); }
    *rgba = g_img;
    return 1;
}

/* ---------------------------------------------------------------- a shop's list
 * Widescreen (one screen): a shop's buy list shows each item's own colour
 * icon before its name, instead of its kind's white sign.
 *  - main RAM: the shop (0x021155CC points at it, USA and Europe alike): its
 *    items (u16, the names' keys) at +0x70, how many at +0x5D, the page shown
 *    at +0x83C (u16, 6 lines a page).
 *  - data/prm/itemdt.gp2, itemdt_en.nat: 32 bytes an item from +0x0C, its key
 *    at +0x18; at +0x10 its icon: the letter (bits 20..27) and the number
 *    (bits 0..9; 999: bits 10..19) of data/ani/d_<letter><number>.spr.
 *  - .spr: u16 1, 3, width, height, cells, 0; per cell u16 x, y, and its
 *    width and height (8 << n), then its pixels (4 bits each, by rows); then
 *    u32 colours and the palette (RGB555; colour 0 see-through).
 *  - the list on the bottom screen: line i from y 22 + 14 i, 14 high; the
 *    kind's sign in x 20..33. */
#define SHOP_PTR    0x1155CC
#define SHOP_LINES  6
#define SHOP_Y0     22
#define SHOP_DY     14
#define SHOP_X0     20
#define SHOP_X1     34

static uint8_t* g_itemdt; static uint32_t g_itemdt_n;
static uint8_t g_icons[MAPMARKS_W * MAPMARKS_H * 4];

static int icon_of(uint16_t key, char* letter, int* num) {
    if (!g_itemdt) {
        uint32_t n = 0;
        const uint8_t* pack = rom_file("data/prm/itemdt.gp2", &n);
        g_itemdt = gpc_file(pack, n, "itemdt_en.nat", &g_itemdt_n);
        if (!g_itemdt) g_itemdt = gpc_file(pack, n, "itemdt.nat", &g_itemdt_n);
        if (!g_itemdt) { g_itemdt_n = 0; g_itemdt = malloc(1); }
    }
    for (uint32_t o = 12; o + 32 <= g_itemdt_n; o += 32) {
        if (le16(g_itemdt + o + 0x18) != key) continue;
        uint32_t w = le32(g_itemdt + o + 0x10);
        int c = (int)((w >> 20) & 0xFF), k = (int)(w & 0x3FF);
        if (!c || !strchr("bcgimprsw", c)) continue;
        if (k == 999) k = (int)((w >> 10) & 0x3FF);
        *letter = (char)c; *num = k;
        return 1;
    }
    return 0;
}

/* An item's icon into g_icons, in the square (x, y, size) of overlay pixels. */
static void draw_icon(uint16_t key, int x0, int y0, int size) {
    char c, path[32]; int num;
    if (!icon_of(key, &c, &num)) return;
    snprintf(path, sizeof path, "data/ani/d_%c%03d.spr", c, num);
    uint32_t n = 0;
    const uint8_t* b = rom_file(path, &n);
    if (!b || n < 12 || le16(b) != 1) return;
    int W = le16(b + 4), H = le16(b + 6), cells = le16(b + 8);
    if (W <= 0 || H <= 0 || W > 64 || H > 64) return;
    uint8_t idx[64 * 64] = { 0 };
    uint32_t p = 12;
    for (int i = 0; i < cells; i++) {
        if (p + 8 > n) return;
        int cx = le16(b + p), cy = le16(b + p + 2), cw = 8 << (le16(b + p + 4) & 3), ch = 8 << (le16(b + p + 6) & 3);
        p += 8;
        if (p + (uint32_t)(cw * ch / 2) > n) return;
        for (int j = 0; j < cw * ch; j++) {
            int x = cx + j % cw, y = cy + j / cw, v = (b[p + j / 2] >> ((j & 1) * 4)) & 15;
            if (x < W && y < H) idx[y * 64 + x] = (uint8_t)v;
        }
        p += (uint32_t)(cw * ch / 2);
    }
    if (p + 4 > n) return;
    uint32_t ncol = le32(b + p); p += 4;
    if (ncol > 16) ncol = 16;
    if (p + ncol * 2 > n) return;
    /* each overlay pixel: the average of the icon's pixels under it (premultiplied) */
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float sx0 = (float)x * W / size, sx1 = (float)(x + 1) * W / size, sy0 = (float)y * H / size, sy1 = (float)(y + 1) * H / size;
            float acc[4] = { 0 }, area = 0;
            for (int v = (int)sy0; v < H && v < sy1; v++)
                for (int u = (int)sx0; u < W && u < sx1; u++) {
                    float a = (fminf(sx1, (float)u + 1) - fmaxf(sx0, (float)u)) * (fminf(sy1, (float)v + 1) - fmaxf(sy0, (float)v));
                    int i = idx[v * 64 + u];
                    area += a;
                    if (!i || (uint32_t)i >= ncol) continue;
                    uint16_t col = le16(b + p + 2 * i);
                    acc[0] += a * (float)(col & 31) / 31; acc[1] += a * (float)((col >> 5) & 31) / 31; acc[2] += a * (float)((col >> 10) & 31) / 31; acc[3] += a;
                }
            int X = x0 + x, Y = y0 + y;
            if (area <= 0 || acc[3] <= 0 || X < 0 || Y < 0 || X >= MAPMARKS_W || Y >= MAPMARKS_H) continue;
            uint8_t* d = &g_icons[(Y * MAPMARKS_W + X) * 4];
            for (int k = 0; k < 4; k++) d[k] = (uint8_t)(acc[k] / area * 255 + 0.5f);
        }
}

int mapmarks_shop(uint32_t* px, uint8_t* info, int desc_y, const uint8_t** rgba, int* changed) {
    static uint32_t last_key;
    static int sel;                                               /* the line the hand points at (it blinks: kept while hidden) */
    *rgba = g_icons; *changed = 0;
    uint32_t shop = rd32(SHOP_PTR);
    int n = 0, page = 0, lines = 0;
    if (px && rd32(FIELD_PTR) != 0 && mapmarks_top_page() && shop >= 0x02000000 && shop < 0x02400000) {
        n = g_main_ram[(shop + 0x5D) & (MAIN_RAM_SIZE - 1)];
        page = rd16(shop + 0x83C);
        lines = n - page * SHOP_LINES;
        if (lines > SHOP_LINES) lines = SHOP_LINES;
    }
    /* every line of the page shows its kind's sign (white) where it should */
    for (int i = 0; i < lines; i++) {
        int white = 0;
        for (int y = SHOP_Y0 + SHOP_DY * i + 2; y < SHOP_Y0 + SHOP_DY * i + SHOP_DY - 1; y++)
            for (int x = SHOP_X0 + 1; x < SHOP_X1 - 1; x++) {
                uint32_t c = px[y * 256 + x];
                white += ((c >> 16) & 255) > 200 && ((c >> 8) & 255) > 200 && (c & 255) > 200;
            }
        if (white < 6) { lines = 0; break; }
    }
    if (lines <= 0) { if (last_key) { last_key = 0; memset(g_icons, 0, sizeof g_icons); *changed = 1; } return 0; }
    for (int i = 0; i < lines; i++) {
        int yellow = 0;
        for (int y = SHOP_Y0 + SHOP_DY * i; y < SHOP_Y0 + SHOP_DY * i + SHOP_DY; y++)
            for (int x = 0; x < SHOP_X0; x++) {
                uint32_t c = px[y * 256 + x];
                yellow += ((c >> 16) & 255) > 180 && ((c >> 8) & 255) > 140 && (c & 255) < 100;
            }
        if (yellow >= 6) sel = i;
    }
    if (sel >= lines) sel = lines - 1;
    uint32_t key = 2166136261u;
    for (int i = 0; i < lines; i++) key = (key ^ rd16(shop + 0x70 + 2 * (page * SHOP_LINES + i))) * 16777619u;
    key = (key ^ (uint32_t)lines) * 16777619u;
    key = (key ^ (uint32_t)sel) * 16777619u;
    key = (key ^ (uint32_t)(desc_y + 1)) * 16777619u;
    if (!key) key = 1;
    if (key != last_key) {
        last_key = key; *changed = 1;
        memset(g_icons, 0, sizeof g_icons);
        for (int i = 0; i < lines; i++) {                         /* a little bigger than the sign, centred on it */
            int s = (SHOP_DY + 1) * MAPMARKS_R, cx = (SHOP_X0 + SHOP_X1) * MAPMARKS_R / 2, cy = (SHOP_Y0 + SHOP_DY * i) * MAPMARKS_R + SHOP_DY * MAPMARKS_R / 2;
            draw_icon(rd16(shop + 0x70 + 2 * (page * SHOP_LINES + i)), cx - s / 2, cy - s / 2, s);
        }
        if (desc_y >= 0)                                          /* the chosen one big, at the left of its description */
            draw_icon(rd16(shop + 0x70 + 2 * (page * SHOP_LINES + sel)), SHOP_DESC_X * MAPMARKS_R, desc_y * MAPMARKS_R, SHOP_DESC_ICON * MAPMARKS_R);
    }
    /* the signs rubbed out: the window's own pixels from the line's first row */
    for (int i = 0; i < lines; i++) {
        int top = SHOP_Y0 + SHOP_DY * i;
        for (int y = top + 1; y < top + SHOP_DY; y++)
            for (int x = SHOP_X0; x < SHOP_X1; x++) {
                px[y * 256 + x] = px[top * 256 + x];
                if (info) memcpy(&info[(y * 256 + x) * 4], &info[(top * 256 + x) * 4], 4);
            }
    }
    return 1;
}
