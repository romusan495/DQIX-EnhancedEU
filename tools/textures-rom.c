/* textures-rom: takes every 3D texture out of your own DQIX ROM into
 * textures/dump (the folder the game's "dump textures" fills while you play),
 * so textures-hd can make HD versions of the whole game at once, without
 * playing through it. Double-click it next to the game; it then starts
 * textures-hd.exe if it is there.
 *
 *   textures-rom [--game <game dir>] [--auto] [rom.nds]
 *
 * --auto (the build runs it so, after each update): nothing to do when this
 * ROM's textures were already taken out (textures/dump/rom-textures.txt); no
 * pause at the end, and textures-hd.exe is started without waiting for it.
 *
 * The ROM: the one given, else the first .nds in ..\rom, else the path in
 * ..\rom\last-rom.txt (what BUILD.bat uses).
 *
 * The 3D textures are Nintendo's standard ones (TEX0 blocks of the BMD0 and
 * BTX0 files, inside NARC archives, some LZ-compressed). A texture is drawn
 * with the palette its materials name (MDL0), or the one named "<texture>_pl".
 * Each is decoded as the game decodes it (runtime/gpu3d.c texture_lookup) and
 * named as the game names it (gl_render.c tex_key), so the pack picks them up.
 * Not in the ROM as such, so still left to the dump while playing: the hero's
 * face and hair (put together by the game), letters, far characters' pieces.
 * Nothing taken from the ROM goes into the repository. */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir_(p) _mkdir(p)
#define SEP "\\"
#else
#include <dirent.h>
#include <sys/stat.h>
#define mkdir_(p) mkdir(p, 0755)
#define SEP "/"
#endif

typedef struct { const uint8_t* d; size_t n; } Buf;
static uint32_t rd32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }

/* ---- texture/palette name pairs named by the models' materials ---- */
typedef struct { char tex[17], pal[17]; } Pair;
static Pair* g_pairs; static int g_npairs, g_cappairs;
static void add_pair(const char* t, const char* p) {
    for (int i = g_npairs - 1; i >= 0 && i >= g_npairs - 64; i--) if (!strcmp(g_pairs[i].tex, t) && !strcmp(g_pairs[i].pal, p)) return;   /* (repeats come together) */
    if (g_npairs == g_cappairs) { g_cappairs = g_cappairs ? g_cappairs * 2 : 4096; g_pairs = realloc(g_pairs, sizeof *g_pairs * (size_t)g_cappairs); }
    snprintf(g_pairs[g_npairs].tex, 17, "%s", t); snprintf(g_pairs[g_npairs].pal, 17, "%s", p); g_npairs++;
}
static int cmp_pair(const void* a, const void* b) { int c = strcmp(((const Pair*)a)->tex, ((const Pair*)b)->tex); return c ? c : strcmp(((const Pair*)a)->pal, ((const Pair*)b)->pal); }

/* ---- the textures written ---- */
static uint64_t* g_keys; static size_t g_nkeys, g_capkeys;
static int seen(uint64_t k) {                                   /* open addressing; 0 = empty */
    if (!k) k = 1;
    if ((g_nkeys + 1) * 2 > g_capkeys) {
        size_t nc = g_capkeys ? g_capkeys * 2 : 1 << 15; uint64_t* n = calloc(nc, 8);
        for (size_t i = 0; i < g_capkeys; i++) if (g_keys[i]) { size_t j = g_keys[i] & (nc - 1); while (n[j]) j = (j + 1) & (nc - 1); n[j] = g_keys[i]; }
        free(g_keys); g_keys = n; g_capkeys = nc;
    }
    size_t j = k & (g_capkeys - 1);
    while (g_keys[j]) { if (g_keys[j] == k) return 1; j = (j + 1) & (g_capkeys - 1); }
    g_keys[j] = k; g_nkeys++;
    return 0;
}

static char g_dump[700];
static int g_written, g_already, g_textures;

/* ---- Nintendo's name dictionaries ---- */
typedef struct { int n, esize; const uint8_t* data; const uint8_t* names; } Dict;
static int dict(Buf b, size_t o, Dict* r) {
    if (o + 8 > b.n) return 0;
    r->n = b.d[o + 1];
    size_t p = o + rd16(b.d + o + 6);
    if (p + 4 > b.n) return 0;
    r->esize = rd16(b.d + p); size_t names = p + rd16(b.d + p + 2);
    r->data = b.d + p + 4; r->names = b.d + names;
    return names + 16 * (size_t)r->n <= b.n && p + 4 + (size_t)r->esize * r->n <= b.n;
}
static void dict_name(const Dict* d, int i, char out[17]) { memcpy(out, d->names + 16 * i, 16); out[16] = 0; }

static int block(Buf b, const char* magic, size_t* off) {       /* a block of a Nitro file */
    if (b.n < 16) return 0;
    int n = rd16(b.d + 14);
    for (int i = 0; i < n && 16 + 4 * i + 4 <= (int)b.n; i++) {
        uint32_t o = rd32(b.d + 16 + 4 * i);
        if (o + 8 <= b.n && !memcmp(b.d + o, magic, 4)) { *off = o; return 1; }
    }
    return 0;
}

static void model_pairs(Buf b, size_t o) {                      /* MDL0: which texture goes with which palette */
    Dict md; if (!dict(b, o + 8, &md)) return;
    for (int m = 0; m < md.n; m++) {
        size_t mo = o + rd32(md.data + md.esize * m);
        if (mo + 12 > b.n) continue;
        size_t mat = mo + rd32(b.d + mo + 8);
        if (mat + 4 > b.n) continue;
        Dict td, pd; int ht = dict(b, mat + rd16(b.d + mat), &td) && td.n, hp = dict(b, mat + rd16(b.d + mat + 2), &pd) && pd.n;
        if (!ht) continue;
        for (int t = 0; t < td.n; t++) {
            const uint8_t* te = td.data + td.esize * t; size_t tl = mat + rd16(te); int tn = te[2];
            char tname[17]; dict_name(&td, t, tname);
            for (int a = 0; a < tn && tl + a < b.n; a++)
                for (int p = 0; hp && p < pd.n; p++) {
                    const uint8_t* pe = pd.data + pd.esize * p; size_t pl = mat + rd16(pe); int pn = pe[2];
                    for (int c = 0; c < pn && pl + c < b.n; c++)
                        if (b.d[pl + c] == b.d[tl + a]) { char pname[17]; dict_name(&pd, p, pname); add_pair(tname, pname); }
                }
        }
    }
}

/* ---- decoding: as runtime/gpu3d.c texture_lookup ---- */
static uint8_t g_rgba[1024 * 1024 * 4];
static void put(int i, uint16_t c, int a5) {
    int r = c & 31, g = (c >> 5) & 31, bl = (c >> 10) & 31;
    uint8_t* o = g_rgba + i * 4;
    o[0] = (uint8_t)(r << 3 | r >> 2); o[1] = (uint8_t)(g << 3 | g >> 2); o[2] = (uint8_t)(bl << 3 | bl >> 2); o[3] = (uint8_t)(a5 << 3 | a5 >> 2);
}
typedef struct { Buf b; size_t texdata, ctexdata, cinfo, paldata; } Tex0;
static int decode(const Tex0* t, uint32_t param, size_t paloff, int* pw, int* ph) {
    int w = 8 << ((param >> 20) & 7), h = 8 << ((param >> 23) & 7), fmt = (param >> 26) & 7, n = w * h;
    size_t off = (size_t)(param & 0xFFFF) << 3; int a0 = (param & (1u << 29)) ? 0 : 31;
    const uint8_t* d = t->b.d; size_t N = t->b.n, P = t->paldata + paloff;
    *pw = w; *ph = h;
#define PAL(i) (P + 2 * (size_t)(i) + 2 <= N ? rd16(d + P + 2 * (size_t)(i)) : 0)
    if (fmt == 5) {
        if (t->ctexdata + off + (size_t)n / 4 > N || t->cinfo + off / 2 + (size_t)n / 8 > N) return 0;
        for (int bi = 0; bi < n / 16; bi++) {
            int bx = bi % (w / 4), by = bi / (w / 4);
            uint32_t v = rd32(d + t->ctexdata + off + 4 * (size_t)bi);
            uint16_t info = rd16(d + t->cinfo + off / 2 + 2 * (size_t)bi);
            size_t po = (size_t)(info & 0x3FFF) * 2; int mode = info >> 14;
            if (P + 2 * po + 4 > N) return 0;
            uint16_t c0 = PAL(po), c1 = PAL(po + 1), c[4]; int al3 = 31;
            c[0] = c0; c[1] = c1;
            if (mode == 1) c[2] = (uint16_t)((((c0 & 0x1F) + (c1 & 0x1F)) >> 1) | ((((c0 & 0x3E0) + (c1 & 0x3E0)) >> 1) & 0x3E0) | ((((c0 & 0x7C00) + (c1 & 0x7C00)) >> 1) & 0x7C00));
            else if (mode == 3) c[2] = (uint16_t)((((c0 & 0x1F) * 5 + (c1 & 0x1F) * 3) >> 3) | ((((c0 & 0x3E0) * 5 + (c1 & 0x3E0) * 3) >> 3) & 0x3E0) | ((((c0 & 0x7C00) * 5 + (c1 & 0x7C00) * 3) >> 3) & 0x7C00));
            else c[2] = PAL(po + 2);
            if (mode == 2) c[3] = PAL(po + 3);
            else if (mode == 3) c[3] = (uint16_t)((((c0 & 0x1F) * 3 + (c1 & 0x1F) * 5) >> 3) | ((((c0 & 0x3E0) * 3 + (c1 & 0x3E0) * 5) >> 3) & 0x3E0) | ((((c0 & 0x7C00) * 3 + (c1 & 0x7C00) * 5) >> 3) & 0x7C00));
            else { c[3] = 0; al3 = 0; }
            for (int yy = 0; yy < 4; yy++)
                for (int xx = 0; xx < 4; xx++) {
                    int k = (v >> (8 * yy + 2 * xx)) & 3;
                    put((by * 4 + yy) * w + bx * 4 + xx, c[k], k == 3 ? al3 : 31);
                }
        }
        return 1;
    }
    static const int bits[8] = { 0, 8, 2, 4, 8, 0, 8, 16 };
    if (!bits[fmt] || t->texdata + off + (size_t)n * bits[fmt] / 8 > N) return 0;
    const uint8_t* s = d + t->texdata + off;
    int maxi = fmt == 1 ? 32 : fmt == 6 ? 8 : fmt == 2 ? 4 : fmt == 3 ? 16 : 256;
    if (fmt != 7 && P + 2 > N) return 0;
    for (int i = 0; i < n; i++) {
        int px, idx, a;
        switch (fmt) {
        case 7: { uint16_t c = rd16(s + 2 * i); put(i, c & 0x7FFF, (c & 0x8000) ? 31 : 0); continue; }
        case 2: px = (s[i >> 2] >> ((i & 3) * 2)) & 3; break;
        case 3: px = (s[i >> 1] >> ((i & 1) * 4)) & 15; break;
        default: px = s[i]; break;
        }
        if (fmt == 1) { idx = px & 31; a = ((px >> 3) & 0x1C) + (px >> 6); }
        else if (fmt == 6) { idx = px & 7; a = px >> 3; }
        else { idx = px; a = px == 0 ? a0 : 31; }
        if (idx >= maxi || P + 2 * (size_t)idx + 2 > N) return 0;
        put(i, PAL(idx), a);
    }
    return 1;
#undef PAL
}

static uint64_t tex_key(const uint8_t* rgba, int w, int h) {   /* = gl_render.c tex_key */
    uint64_t k = 0xCBF29CE484222325ull;
    uint32_t wh[2] = { (uint32_t)w, (uint32_t)h };
    const uint8_t* b = (const uint8_t*)wh;
    for (int i = 0; i < 8; i++) k = (k ^ b[i]) * 0x100000001B3ull;
    for (size_t i = 0; i < (size_t)w * h; i++) {
        const uint8_t* p = rgba + i * 4;
        uint32_t v = p[3] ? (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24 : 0;
        for (int j = 0; j < 4; j++, v >>= 8) k = (k ^ (v & 0xFF)) * 0x100000001B3ull;
    }
    return k;
}

static int has_pair(const char* t, const char* p) {
    Pair k; snprintf(k.tex, 17, "%s", t); snprintf(k.pal, 17, "%s", p);
    return bsearch(&k, g_pairs, (size_t)g_npairs, sizeof k, cmp_pair) != NULL;
}

static void write_one(const Tex0* t, uint32_t param, size_t paloff) {
    int w, h;
    if (!decode(t, param, paloff, &w, &h)) return;
    uint64_t k = tex_key(g_rgba, w, h);
    if (seen(k)) return;
    char p[800]; snprintf(p, sizeof p, "%s" SEP "%016llx.png", g_dump, (unsigned long long)k);
    FILE* f = fopen(p, "rb");
    if (f) { fclose(f); g_already++; return; }
    if (stbi_write_png(p, w, h, 4, g_rgba, w * 4)) g_written++;
}

static void textures(Buf b, size_t o) {                         /* TEX0: every texture, with its palette(s) */
    if (o + 0x3C > b.n) return;
    Tex0 t = { b, o + rd32(b.d + o + 0x14), o + rd32(b.d + o + 0x24), o + rd32(b.d + o + 0x28), o + rd32(b.d + o + 0x38) };
    Dict td, pd; int npal = 0;
    if (!dict(b, o + rd16(b.d + o + 0xE), &td) || !td.n) return;
    if (dict(b, o + rd32(b.d + o + 0x34), &pd)) npal = pd.n;
    for (int i = 0; i < td.n; i++) {
        uint32_t param = rd32(td.data + td.esize * i);
        char tn[17], want[24]; dict_name(&td, i, tn); snprintf(want, sizeof want, "%s_pl", tn);
        g_textures++;
        if (((param >> 26) & 7) == 7) { write_one(&t, param, 0); continue; }
        int any = 0;
        for (int p = 0; p < npal; p++) {
            char pn[17]; dict_name(&pd, p, pn);
            if (has_pair(tn, pn) || !strcmp(pn, want) || !strcmp(pn, tn)) { write_one(&t, param, (size_t)rd16(pd.data + pd.esize * p) << 3); any = 1; }
        }
        if (!any && npal == 1) write_one(&t, param, (size_t)rd16(pd.data) << 3);
    }
}

/* ---- archives ---- */
static uint8_t* lz(Buf b, size_t* outn) {                       /* LZ10 / LZ11 */
    int type = b.d[0]; size_t n = rd32(b.d) >> 8, o = 4;
    if (!n) { if (b.n < 8) return NULL; n = rd32(b.d + 4); o = 8; }
    if (n < b.n / 2 || n > b.n * 20 + 64) return NULL;
    uint8_t* out = malloc(n); size_t q = 0;
    while (q < n) {
        if (o >= b.n) { free(out); return NULL; }
        int f = b.d[o++];
        for (int i = 0; i < 8 && q < n; i++, f <<= 1) {
            if (!(f & 0x80)) { if (o >= b.n) { free(out); return NULL; } out[q++] = b.d[o++]; continue; }
            size_t L, D;
            if (o + 1 >= b.n) { free(out); return NULL; }
            if (type == 0x10) { int v = b.d[o] << 8 | b.d[o + 1]; o += 2; L = (v >> 12) + 3; D = (v & 0xFFF) + 1; }
            else {
                int x = b.d[o], ind = x >> 4;
                if (ind == 0) { if (o + 2 >= b.n) { free(out); return NULL; } L = (((x & 15) << 4) | b.d[o + 1] >> 4) + 0x11; D = (((b.d[o + 1] & 15) << 8) | b.d[o + 2]) + 1; o += 3; }
                else if (ind == 1) { if (o + 3 >= b.n) { free(out); return NULL; } L = (((x & 15) << 12) | b.d[o + 1] << 4 | b.d[o + 2] >> 4) + 0x111; D = (((b.d[o + 2] & 15) << 8) | b.d[o + 3]) + 1; o += 4; }
                else { L = ind + 1; D = (((x & 15) << 8) | b.d[o + 1]) + 1; o += 2; }
            }
            if (D > q) { free(out); return NULL; }
            for (size_t j = 0; j < L && q < n; j++, q++) out[q] = out[q - D];
        }
    }
    *outn = n;
    return out;
}

static void walk(Buf b, int pass, int depth) {
    if (b.n < 16 || depth > 6) return;
    if (!memcmp(b.d, "NARC", 4)) {
        size_t o = rd16(b.d + 12);
        if (o + 12 > b.n || memcmp(b.d + o, "BTAF", 4)) return;
        uint32_t n = rd32(b.d + o + 8); size_t fat = o + 12;
        size_t o2 = o + rd32(b.d + o + 4); if (o2 + 8 > b.n) return;
        size_t o3 = o2 + rd32(b.d + o2 + 4); if (o3 + 8 > b.n || memcmp(b.d + o3, "GMIF", 4)) return;
        size_t base = o3 + 8;
        for (uint32_t i = 0; i < n && fat + 8 * i + 8 <= b.n; i++) {
            size_t s = base + rd32(b.d + fat + 8 * i), e = base + rd32(b.d + fat + 8 * i + 4);
            if (s <= e && e <= b.n) { Buf sub = { b.d + s, e - s }; walk(sub, pass, depth + 1); }
        }
        return;
    }
    if (!memcmp(b.d, "BMD0", 4) || !memcmp(b.d, "BTX0", 4)) {
        size_t o;
        if (pass == 0 && block(b, "MDL0", &o)) model_pairs(b, o);
        if (pass == 1 && block(b, "TEX0", &o)) textures(b, o);
        return;
    }
    if ((b.d[0] == 0x10 || b.d[0] == 0x11) && depth < 5) {
        size_t n; uint8_t* x = lz(b, &n);
        if (x) { Buf sub = { x, n }; walk(sub, pass, depth + 1); free(x); }
    }
}

static void walk_rom(Buf rom, int pass) {                       /* every file of the ROM's file system */
    uint32_t fat = rd32(rom.d + 0x48), fatn = rd32(rom.d + 0x4C) / 8;
    for (uint32_t i = 0; i < fatn; i++) {
        uint32_t s = rd32(rom.d + fat + 8 * i), e = rd32(rom.d + fat + 8 * i + 4);
        if (s < e && e <= rom.n) { Buf f = { rom.d + s, e - s }; walk(f, pass, 0); }
    }
}

static int find_rom(const char* game, char* out, size_t n) {
    char dir[700]; snprintf(dir, sizeof dir, "%s" SEP ".." SEP "rom", game);
#ifdef _WIN32
    char pat[760]; snprintf(pat, sizeof pat, "%s\\*.nds", dir);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) { snprintf(out, n, "%s\\%s", dir, fd.cFileName); FindClose(h); return 1; }
#else
    DIR* d = opendir(dir); struct dirent* e;
    while (d && (e = readdir(d))) { size_t l = strlen(e->d_name); if (l > 4 && !strcmp(e->d_name + l - 4, ".nds")) { snprintf(out, n, "%s/%s", dir, e->d_name); closedir(d); return 1; } }
    if (d) closedir(d);
#endif
    char p[760]; snprintf(p, sizeof p, "%s" SEP "last-rom.txt", dir);
    FILE* f = fopen(p, "rb");
    if (!f) return 0;
    size_t l = fread(out, 1, n - 1, f); fclose(f); out[l] = 0;
    while (l && (out[l - 1] == '\n' || out[l - 1] == '\r' || out[l - 1] == ' ')) out[--l] = 0;
    return l > 0;
}

int main(int argc, char** argv) {
    char game[600] = ".", rompath[1000] = ""; int autorun = 0;
#ifdef _WIN32
    GetModuleFileNameA(NULL, game, sizeof game);
    char* sl = strrchr(game, '\\'); if (sl) *sl = 0;
    SetCurrentDirectoryA(game);
#endif
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--game") && i + 1 < argc) snprintf(game, sizeof game, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--auto")) autorun = 1;
        else snprintf(rompath, sizeof rompath, "%s", argv[i]);
    }
    if (!rompath[0] && !find_rom(game, rompath, sizeof rompath)) { printf("No ROM found: give its path (textures-rom <rom.nds>) or put it in the rom folder.\n"); return 1; }
    FILE* f = fopen(rompath, "rb");
    if (!f) { printf("Can't open %s\n", rompath); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* d = n > 0x200 ? malloc((size_t)n) : NULL;
    if (!d || fread(d, 1, (size_t)n, f) != (size_t)n) { printf("Can't read %s\n", rompath); fclose(f); return 1; }
    fclose(f);
    Buf rom = { d, (size_t)n };
    if (rd32(d + 0x48) + rd32(d + 0x4C) > rom.n) { printf("%s is not a DS ROM\n", rompath); return 1; }
    char tex[700]; snprintf(tex, sizeof tex, "%s" SEP "textures", game); mkdir_(tex);
    snprintf(g_dump, sizeof g_dump, "%s" SEP "dump", tex); mkdir_(g_dump);
    char mark[800], sig[128] = "", old[128] = "";                 /* which ROM was done: its game code and size (bump the 1 if what is taken out changes) */
    snprintf(mark, sizeof mark, "%s" SEP "rom-textures.txt", g_dump);
    snprintf(sig, sizeof sig, "textures-rom 1 %.4s %ld", (const char*)d + 12, n);
    f = fopen(mark, "rb");
    if (f) { size_t l = fread(old, 1, sizeof old - 1, f); old[l] = 0; fclose(f); }
    if (autorun && !strncmp(old, sig, strlen(sig))) printf("Textures of this ROM: already taken out.\n");
    else {
        printf("Reading %s...\n", rompath);
        walk_rom(rom, 0);
        qsort(g_pairs, (size_t)g_npairs, sizeof *g_pairs, cmp_pair);
        walk_rom(rom, 1);
        printf("%d textures in the ROM: %d new in textures" SEP "dump, %d already there.\n", g_textures, g_written, g_already);
        f = fopen(mark, "wb"); if (f) { fprintf(f, "%s\n", sig); fclose(f); }
    }
#ifdef _WIN32
    char hd[700]; snprintf(hd, sizeof hd, "%s\\textures-hd.exe", game);
    if (autorun && GetFileAttributesA(hd) != INVALID_FILE_ATTRIBUTES) {   /* (it does nothing when every one has its HD version) */
        STARTUPINFOA si; PROCESS_INFORMATION pi; memset(&si, 0, sizeof si); si.cb = sizeof si;
        si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_SHOWMINNOACTIVE;
        if (CreateProcessA(hd, NULL, NULL, NULL, FALSE, CREATE_NEW_CONSOLE | BELOW_NORMAL_PRIORITY_CLASS, NULL, game, &si, &pi)) {
            printf("Making the HD versions of the new ones in the background (textures-hd.exe, see textures\\hd-auto.log).\n");
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
    } else if (g_written && GetFileAttributesA(hd) != INVALID_FILE_ATTRIBUTES) {
        printf("Making the HD versions (textures-hd.exe): this can take a while...\n");
        STARTUPINFOA si; PROCESS_INFORMATION pi; memset(&si, 0, sizeof si); si.cb = sizeof si;
        if (CreateProcessA(hd, NULL, NULL, NULL, FALSE, 0, NULL, game, &si, &pi)) { WaitForSingleObject(pi.hProcess, INFINITE); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); printf("Done: see textures\\hd-auto.log.\n"); }
    }
    if (!autorun) { printf("Press Enter to close.\n"); getchar(); }
#endif
    free(d);
    return 0;
}
