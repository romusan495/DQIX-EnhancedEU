/* textures-check: looks over the HD texture pack (every textures/<name>.png
 * the game draws instead of its own, see gl_render.c) and says which ones
 * will look wrong in the game, before playing. Double-click it next to the
 * game (or: textures-check --game <folder>); it writes
 * textures-check/report.html (opened when it's done) and report.txt.
 *
 * Each HD texture is compared with the game's own picture, the one "Dump
 * textures" wrote to textures/dump/ or textures/dump-2d/ under the same name:
 * - the file can't be read, or its name isn't one the game looks for;
 * - two folders hold the same texture (only one of them is used);
 * - its size isn't the original's shape (stretched) or not a whole multiple;
 * - see-through parts lost (a box round the shape) or holes where the
 *   original is solid, or the shape's outline moved;
 * - colours far from the original's (a wrong file), a dark or light fringe
 *   along the outline, see-through pixels of another colour at the edges
 *   (they make a dark halo once the game smooths the texture);
 * - a seam where the texture repeats though the original repeats cleanly.
 * Each one with a problem gets a picture: the original, then the HD one over
 * a checkerboard, black, white and a colour, so halos and holes show.
 *
 * Nothing is changed unless asked:
 *   --disable   each texture with an error gets ".off" added to its name: the
 *               game ignores it and draws its own picture again
 *   --restore   every ".png.off" in the pack gets its name back
 *   --fix-edges see-through pixels take their solid neighbours' colour (what
 *               you see doesn't change; the halos go). The file as it was is
 *               kept in textures-check/backup/ first.
 *   --images    a picture for every texture, not only those with a problem */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <direct.h>
#include <io.h>
#define mkdir_(p) _mkdir(p)
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define mkdir_(p) mkdir(p, 0755)
#endif

#define OUT_DIR "textures-check"
#define MAX_SIDE 4096                                          /* bigger: a lot of video memory for one texture */

enum { OK, INFO, WARN, ERR };
static const char* LEVEL[] = { "ok", "info", "warning", "error" };

typedef struct {
    char path[520], rel[512], name[64];                        /* rel: from textures/ */
    int valid_name, disabled, dup;                             /* dup: another folder has the same name */
    int level, w, h, ow, oh;                                   /* HD size, original size (0: no original) */
    char orig_dir[16];
    char issues[1200];
    char image[96];
} Entry;

static Entry* g_e; static int g_n, g_cap;
static int g_opt_disable, g_opt_restore, g_opt_fix, g_opt_images;

static void issue(Entry* e, int level, const char* fmt, ...) {
    char b[400]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
    if (level > e->level) e->level = level;
    size_t l = strlen(e->issues);
    snprintf(e->issues + l, sizeof e->issues - l, "%s%s: %s", l ? "\n" : "", LEVEL[level], b);
}

static int exists(const char* p) { FILE* f = fopen(p, "rb"); if (f) fclose(f); return f != NULL; }

/* the game's name for a picture (gl_render.c tex_key): FNV-1a 64 of w, h and
 * the pixels, see-through ones as 0 */
static uint64_t tex_key(const uint8_t* rgba, int w, int h) {
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

static int hex_name(const char* n, uint64_t* key) {             /* <16 hex digits>.png, like the game */
    if (strlen(n) != 20 || strcmp(n + 16, ".png") != 0) return 0;
    uint64_t k = 0;
    for (int i = 0; i < 16; i++) {
        char c = n[i]; int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (v < 0) return 0;
        k = k << 4 | (uint64_t)v;
    }
    if (key) *key = k;
    return 1;
}

static int ends_with(const char* s, const char* t) { size_t a = strlen(s), b = strlen(t); return a >= b && !strcmp(s + a - b, t); }
static int ieq_png(const char* n) {
    size_t l = strlen(n);
    return l > 4 && n[l - 4] == '.' && (n[l - 3] | 32) == 'p' && (n[l - 2] | 32) == 'n' && (n[l - 1] | 32) == 'g';
}

static void add_file(const char* dir, const char* rel_dir, const char* name) {
    int off = ends_with(name, ".png.off");
    if (!off && !ieq_png(name)) return;
    if (g_n == g_cap) { g_cap = g_cap ? g_cap * 2 : 1024; g_e = realloc(g_e, sizeof *g_e * (size_t)g_cap); }
    Entry* e = &g_e[g_n++]; memset(e, 0, sizeof *e);
    snprintf(e->path, sizeof e->path, "%s/%s", dir, name);
    snprintf(e->rel, sizeof e->rel, "%s%s%s", rel_dir, *rel_dir ? "/" : "", name);
    snprintf(e->name, sizeof e->name, "%.*s", off ? (int)strlen(name) - 4 : (int)strlen(name), name);
    e->disabled = off;
    e->valid_name = hex_name(e->name, NULL);
}

/* every folder under textures/ the game looks in: all but dump/ and dump-2d/ */
static void scan(const char* dir, const char* rel, int depth) {
    if (depth > 8) return;
#ifdef _WIN32
    char pat[1024]; snprintf(pat, sizeof pat, "%s/*", dir);
    struct _finddata_t fd; intptr_t h = _findfirst(pat, &fd);
    if (h == -1) return;
    do {
        if (fd.name[0] == '.') continue;
        if (fd.attrib & _A_SUBDIR) {
            if (depth == 0 && (!strcmp(fd.name, "dump") || !strcmp(fd.name, "dump-2d"))) continue;
            char sub[1024], srel[1024]; snprintf(sub, sizeof sub, "%s/%s", dir, fd.name);
            snprintf(srel, sizeof srel, "%s%s%s", rel, *rel ? "/" : "", fd.name);
            scan(sub, srel, depth + 1);
        } else add_file(dir, rel, fd.name);
    } while (_findnext(h, &fd) == 0);
    _findclose(h);
#else
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char sub[1024]; snprintf(sub, sizeof sub, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(sub, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (depth == 0 && (!strcmp(e->d_name, "dump") || !strcmp(e->d_name, "dump-2d"))) continue;
            char srel[1024]; snprintf(srel, sizeof srel, "%s%s%s", rel, *rel ? "/" : "", e->d_name);
            scan(sub, srel, depth + 1);
        } else add_file(dir, rel, e->d_name);
    }
    closedir(d);
#endif
}

static int count_dir_png(const char* dir, int (*has_hd)(const char*)) {   /* dumped pictures without an HD one */
    int n = 0;
#ifdef _WIN32
    char pat[600]; snprintf(pat, sizeof pat, "%s\\*.png", dir);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do { if (hex_name(fd.cFileName, NULL) && !has_hd(fd.cFileName)) n++; } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(dir); struct dirent* e;
    if (!d) return 0;
    while ((e = readdir(d))) if (hex_name(e->d_name, NULL) && !has_hd(e->d_name)) n++;
    closedir(d);
#endif
    return n;
}

static int cmp_name(const void* a, const void* b) {
    const Entry *x = a, *y = b;
    int c = strcmp(x->name, y->name);
    return c ? c : strcmp(x->rel, y->rel);
}
static int has_hd(const char* name) {                           /* (g_e sorted by name) */
    int lo = 0, hi = g_n - 1;
    while (lo <= hi) { int m = (lo + hi) / 2, c = strcmp(g_e[m].name, name); if (!c) return !g_e[m].disabled; if (c < 0) lo = m + 1; else hi = m - 1; }
    return 0;
}

/* see-through pixels take the average colour of their solid (or already filled) neighbours (textures-hd.c) */
static void bleed(uint8_t* px, int w, int h) {
    int n = w * h, any = 0, all = 1;
    for (int i = 0; i < n; i++) { if (px[i * 4 + 3]) any = 1; else all = 0; }
    if (!any || all) return;
    uint8_t* filled = malloc((size_t)n); uint8_t* nf = malloc((size_t)n);
    for (int i = 0; i < n; i++) filled[i] = px[i * 4 + 3] != 0;
    for (int it = 0; it < 64; it++) {
        int left = 0; memcpy(nf, filled, (size_t)n);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int i = y * w + x; if (filled[i]) continue;
                int s[3] = {0, 0, 0}, c = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int j = ((y + dy + h) % h) * w + (x + dx + w) % w;      /* wraps, like the texture */
                        if (filled[j]) { for (int k = 0; k < 3; k++) s[k] += px[j * 4 + k]; c++; }
                    }
                if (c) { for (int k = 0; k < 3; k++) px[i * 4 + k] = (uint8_t)((s[k] + c / 2) / c); nf[i] = 1; }
                else left = 1;
            }
        memcpy(filled, nf, (size_t)n);
        if (!left) break;
    }
    free(filled); free(nf);
}

/* see-through pixels next to solid ones whose colour is far from those
 * neighbours': the game filters HD textures (straight alpha), so that colour
 * leaks into the outline as a fringe. Returns their share (0..1) */
static float edge_bleed_share(const uint8_t* px, int w, int h) {
    int edge = 0, bad = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const uint8_t* p = px + ((size_t)y * w + x) * 4;
            if (p[3]) continue;
            int s[3] = {0, 0, 0}, c = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                    const uint8_t* q = px + ((size_t)yy * w + xx) * 4;
                    if (q[3] >= 128) { for (int k = 0; k < 3; k++) s[k] += q[k]; c++; }
                }
            if (!c) continue;
            edge++;
            int d = 0; for (int k = 0; k < 3; k++) d += abs(p[k] - s[k] / c);
            if (d > 3 * 64) bad++;
        }
    return edge >= 8 ? (float)bad / (float)edge : 0.0f;
}

static int lum(const uint8_t* c) { return (c[0] * 77 + c[1] * 150 + c[2] * 29) >> 8; }

static void check_hd(Entry* e, const uint8_t* o, int ow, int oh, const uint8_t* hd, int W, int H) {
    /* size */
    if ((long)W * oh != (long)H * ow) issue(e, ERR, "stretched: %dx%d isn't the shape of the original %dx%d", W, H, ow, oh);
    else if (W % ow || H % oh) issue(e, WARN, "%dx%d isn't a whole multiple of the original %dx%d (blurry or uneven pixels)", W, H, ow, oh);
    if (W < ow || H < oh) issue(e, WARN, "smaller than the original (%dx%d < %dx%d)", W, H, ow, oh);
    if (W > MAX_SIDE || H > MAX_SIDE) issue(e, WARN, "very big (%dx%d): a lot of video memory for one texture", W, H);

    /* each original pixel and the block of HD pixels over it */
    int on = ow * oh, osolid = 1, otrans = 0;
    for (int i = 0; i < on; i++) { if (o[i * 4 + 3] != 255) osolid = 0; if (!o[i * 4 + 3]) otrans++; }
    long hn = (long)W * H, hsee = 0, hsolid = 1;
    for (long i = 0; i < hn; i++) { if (hd[i * 4 + 3] < 250) hsee++; if (hd[i * 4 + 3] != 255) hsolid = 0; }
    if (osolid && hsee > hn / 200) issue(e, ERR, "see-through holes (%.1f%% of it) where the original is solid", 100.0 * hsee / hn);
    if (otrans && hsolid) issue(e, ERR, "lost its see-through parts: a box round the shape in the game");

    long spill = 0, eaten = 0, outline = 0, cn = 0, en = 0;
    double cdiff = 0, edl = 0;
    for (int y = 0; y < oh; y++)
        for (int x = 0; x < ow; x++) {
            int x0 = (int)((long)x * W / ow), x1 = (int)((long)(x + 1) * W / ow), y0 = (int)((long)y * H / oh), y1 = (int)((long)(y + 1) * H / oh);
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            long sa = 0, sc[3] = {0, 0, 0}, k = 0;
            for (int yy = y0; yy < y1 && yy < H; yy++)
                for (int xx = x0; xx < x1 && xx < W; xx++) {
                    const uint8_t* q = hd + ((size_t)yy * W + xx) * 4;
                    sa += q[3]; for (int c = 0; c < 3; c++) sc[c] += (long)q[c] * q[3]; k++;
                }
            if (!k) continue;
            const uint8_t* p = o + ((size_t)y * ow + x) * 4;
            int ba = (int)(sa / k);
            int border = 0;                                     /* a solid/see-through boundary of the original */
            static const int d4[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
            for (int d = 0; d < 4; d++) {
                int xx = x + d4[d][0], yy = y + d4[d][1];
                if (xx < 0 || yy < 0 || xx >= ow || yy >= oh) continue;
                if ((o[((size_t)yy * ow + xx) * 4 + 3] == 0) != (p[3] == 0)) border = 1;
            }
            outline += border;
            if (p[3] == 0 && ba > 160) spill++;
            if (p[3] == 255 && ba < 96) eaten++;
            if (p[3] == 255 && ba > 200 && sa > 0) {
                uint8_t m[3]; int d = 0;
                for (int c = 0; c < 3; c++) { m[c] = (uint8_t)(sc[c] / sa); d += abs(m[c] - p[c]); }
                cdiff += d / 3.0; cn++;
                if (border) { edl += lum(m) - lum(p); en++; }
            }
        }
    if (!osolid && !hsolid && outline >= 4 && spill + eaten >= 3) {
        double share = (double)(spill + eaten) / outline;
        if (share > 0.35) issue(e, WARN, "the shape's outline moved: %ld pixel(s) of the original %s (%.0f%% of the outline)",
                                spill + eaten, spill >= eaten ? "spill over into see-through parts" : "eaten away", 100.0 * share);
    }
    if (cn) {
        double m = cdiff / cn;
        if (m > 45) issue(e, ERR, "doesn't look like the original (colours %.0f/255 away on average): a wrong file?", m);
        else if (m > 30) issue(e, WARN, "colours %.0f/255 away from the original's on average", m);
    }
    if (en >= 4) {
        double m = edl / en;
        if (m < -40) issue(e, WARN, "dark fringe along the outline (%.0f/255 darker than the original there)", -m);
        else if (m > 40) issue(e, WARN, "light fringe along the outline (%.0f/255 lighter than the original there)", m);
    }
    if (!hsolid) {
        float s = edge_bleed_share(hd, W, H);
        if (s > 0.25f) issue(e, WARN, "%.0f%% of the see-through pixels round the shape are another colour: a fringe of it once the game smooths the texture (--fix-edges)", 100.0 * s);
    }

    /* where it repeats: right edge against left, bottom against top */
    if (osolid && hsolid) {
        for (int axis = 0; axis < 2; axis++) {
            double so = 0, sh = 0;
            int n1 = axis ? ow : oh, N1 = axis ? W : H;
            for (int i = 0; i < n1; i++) {
                const uint8_t* a = axis ? o + (size_t)i * 4 : o + (size_t)i * ow * 4;
                const uint8_t* b = axis ? o + ((size_t)(oh - 1) * ow + i) * 4 : o + ((size_t)i * ow + ow - 1) * 4;
                for (int c = 0; c < 3; c++) so += abs(a[c] - b[c]);
            }
            for (int i = 0; i < N1; i++) {
                const uint8_t* a = axis ? hd + (size_t)i * 4 : hd + (size_t)i * W * 4;
                const uint8_t* b = axis ? hd + ((size_t)(H - 1) * W + i) * 4 : hd + ((size_t)i * W + W - 1) * 4;
                for (int c = 0; c < 3; c++) sh += abs(a[c] - b[c]);
            }
            so /= 3.0 * n1; sh /= 3.0 * N1;
            if (so < 14 && sh > 32 && sh > so * 2.5) issue(e, WARN, "a seam where it repeats %s (edges %.0f/255 apart; the original's %.0f)", axis ? "up and down" : "side by side", sh, so);
        }
    }
}

/* ---- the picture of one texture: original | HD over checkerboard, black, white, a colour */
static void blit(uint8_t* dst, int DW, int dx, int dy, int pw, int ph, const uint8_t* src, int sw, int sh, int bg) {
    for (int y = 0; y < ph; y++)
        for (int x = 0; x < pw; x++) {
            const uint8_t* s = src + ((size_t)(y * sh / ph) * sw + (size_t)(x * sw / pw)) * 4;
            uint8_t b[3];
            if (bg == 0) { int c = ((x >> 3) + (y >> 3)) & 1 ? 200 : 140; b[0] = b[1] = b[2] = (uint8_t)c; }
            else if (bg == 1) b[0] = b[1] = b[2] = 0;
            else if (bg == 2) b[0] = b[1] = b[2] = 255;
            else { b[0] = 40; b[1] = 150; b[2] = 90; }
            uint8_t* d = dst + ((size_t)(dy + y) * DW + dx + x) * 3;
            for (int c = 0; c < 3; c++) d[c] = (uint8_t)((s[c] * s[3] + b[c] * (255 - s[3]) + 127) / 255);
        }
}

static void make_image(Entry* e, const uint8_t* o, int ow, int oh, const uint8_t* hd, int W, int H) {
    int ph = 160, pw = (int)((long)ph * W / (H ? H : 1));
    if (pw > 240) { pw = 240; ph = (int)((long)pw * H / W); if (ph < 8) ph = 8; }
    if (pw < 8) pw = 8;
    int panels = o ? 5 : 4, gap = 4, DW = panels * pw + (panels + 1) * gap, DH = ph + 2 * gap;
    uint8_t* img = malloc((size_t)DW * DH * 3);
    memset(img, 64, (size_t)DW * DH * 3);
    int x = gap;
    if (o) { blit(img, DW, x, gap, pw, ph, o, ow, oh, 0); x += pw + gap; }
    for (int bg = 0; bg < 4; bg++) { blit(img, DW, x, gap, pw, ph, hd, W, H, bg); x += pw + gap; }
    char p[700];
    snprintf(e->image, sizeof e->image, "img/%s", e->name);
    snprintf(p, sizeof p, OUT_DIR "/%s", e->image);
    stbi_write_png(p, DW, DH, 3, img, DW * 3);
    free(img);
}

static int backup_copy(const Entry* e) {                          /* textures-check/backup/<rel>, folders made as needed */
    char dst[1100]; snprintf(dst, sizeof dst, OUT_DIR "/backup/%s", e->rel);
    for (char* s = dst + strlen(OUT_DIR "/"); *s; s++) if (*s == '/') { *s = 0; mkdir_(dst); *s = '/'; }
    if (exists(dst)) return 1;                                   /* the first one kept is the one before any fix */
    FILE* i = fopen(e->path, "rb"); FILE* o = fopen(dst, "wb");
    int ok = i && o;
    if (ok) { char b[65536]; size_t n; while ((n = fread(b, 1, sizeof b, i)) > 0) if (fwrite(b, 1, n, o) != n) ok = 0; }
    if (i) fclose(i);
    if (o && fclose(o) != 0) ok = 0;
    if (!ok) remove(dst);
    return ok;
}

static int turn_off(Entry* e) {                                  /* <name>.png.off: the game draws its own picture */
    char np[600]; snprintf(np, sizeof np, "%s.off", e->path);
    if (rename(e->path, np) != 0) return 0;
    size_t l = strlen(e->issues); snprintf(e->issues + l, sizeof e->issues - l, "\nturned off: renamed to %s.off", e->name);
    return 1;
}

static void html_text(FILE* f, const char* s) {
    for (; *s; s++) {
        if (*s == '<') fputs("&lt;", f); else if (*s == '>') fputs("&gt;", f); else if (*s == '&') fputs("&amp;", f);
        else if (*s == '\n') fputs("<br>", f); else fputc(*s, f);
    }
}

int main(int argc, char** argv) {
    char game[600] = ".";
#ifdef _WIN32
    GetModuleFileNameA(NULL, game, sizeof game);
    char* sl = strrchr(game, '\\'); if (sl) *sl = 0;
#endif
    int quiet = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--game") && i + 1 < argc) snprintf(game, sizeof game, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--disable")) g_opt_disable = 1;
        else if (!strcmp(argv[i], "--restore")) g_opt_restore = 1;
        else if (!strcmp(argv[i], "--fix-edges")) g_opt_fix = 1;
        else if (!strcmp(argv[i], "--images")) g_opt_images = 1;
        else if (!strcmp(argv[i], "--quiet")) quiet = 1;
        else { fprintf(stderr, "usage: textures-check [--game <folder>] [--disable] [--restore] [--fix-edges] [--images] [--quiet]\n"); return 2; }
    }
#ifdef _WIN32
    SetCurrentDirectoryA(game);
#else
    if (chdir(game) != 0) { fprintf(stderr, "can't open %s\n", game); return 2; }
#endif
    scan("textures", "", 0);
    if (g_n) qsort(g_e, (size_t)g_n, sizeof *g_e, cmp_name);
    mkdir_(OUT_DIR); { char p[64]; snprintf(p, sizeof p, OUT_DIR "/img"); mkdir_(p); }

    int restored = 0, disabled = 0, fixed = 0;
    if (g_opt_restore)
        for (int i = 0; i < g_n; i++) {
            Entry* e = &g_e[i]; if (!e->disabled) continue;
            char np[600]; snprintf(np, sizeof np, "%.*s", (int)strlen(e->path) - 4, e->path);
            if (!exists(np) && rename(e->path, np) == 0) { snprintf(e->path, sizeof e->path, "%.519s", np); e->rel[strlen(e->rel) - 4] = 0; e->disabled = 0; restored++; }
        }
    for (int i = 0; i < g_n; i++) {                            /* the same name twice (enabled ones) */
        if (g_e[i].disabled || !g_e[i].valid_name) continue;
        for (int j = i + 1; j < g_n && !strcmp(g_e[j].name, g_e[i].name); j++) if (!g_e[j].disabled) g_e[i].dup = g_e[j].dup = 1;
    }

    int counts[4] = {0, 0, 0, 0}, n_off = 0;
    for (int i = 0; i < g_n; i++) {
        Entry* e = &g_e[i];
        if (e->disabled) { n_off++; continue; }
        if (!e->valid_name) { issue(e, INFO, "not used by the game: the name must be the 16 hex digits of a dumped texture + .png"); counts[e->level]++; continue; }
        if (e->dup) issue(e, WARN, "the same texture is in another folder too: only one of them is used");
        int W, H, c;
        uint8_t* hd = stbi_load(e->path, &W, &H, &c, 4);
        if (!hd) {
            issue(e, ERR, "can't be read as a PNG (%s): the game draws its own picture", stbi_failure_reason());
            if (g_opt_disable) disabled += turn_off(e);
            counts[e->level]++; continue;
        }
        e->w = W; e->h = H;
        char op[200]; uint8_t* o = NULL; int ow = 0, oh = 0;
        static const char* dumps[2] = { "dump", "dump-2d" };
        for (int d = 0; d < 2 && !o; d++) {
            snprintf(op, sizeof op, "textures/%s/%s", dumps[d], e->name);
            if (exists(op) && (o = stbi_load(op, &ow, &oh, &c, 4))) snprintf(e->orig_dir, sizeof e->orig_dir, "%s", dumps[d]);
        }
        if (!o) issue(e, INFO, "no original in textures/dump to compare with (turn on Dump textures and play where it shows)");
        else {
            uint64_t key = 0; hex_name(e->name, &key);
            e->ow = ow; e->oh = oh;
            if (tex_key(o, ow, oh) != key) issue(e, WARN, "textures/%s/%s isn't the game's picture of that name (edited?): compared anyway", e->orig_dir, e->name);
            check_hd(e, o, ow, oh, hd, W, H);
        }
        if (!o && W > MAX_SIDE) issue(e, WARN, "very big (%dx%d)", W, H);
        int off = g_opt_disable && e->level == ERR;
        if (g_opt_fix && !off && strstr(e->issues, "--fix-edges")) {
            if (!backup_copy(e)) issue(e, WARN, "not fixed: couldn't keep a copy in " OUT_DIR "/backup/");
            else if (bleed(hd, W, H), stbi_write_png(e->path, W, H, 4, hd, W * 4)) {
                fixed++;
                size_t l = strlen(e->issues); snprintf(e->issues + l, sizeof e->issues - l, "\nfixed: see-through pixels recoloured (the file before is in " OUT_DIR "/backup/)");
            }
        }
        if (e->level >= WARN || g_opt_images) make_image(e, o, ow, oh, hd, W, H);
        stbi_image_free(hd); stbi_image_free(o);
        if (off) disabled += turn_off(e);
        counts[e->level]++;
    }
    int missing = count_dir_png("textures/dump", has_hd) + count_dir_png("textures/dump-2d", has_hd);

    /* the report */
    FILE* t = fopen(OUT_DIR "/report.txt", "w");
    FILE* h = fopen(OUT_DIR "/report.html", "w");
    if (!t || !h) { fprintf(stderr, "can't write in " OUT_DIR "/\n"); return 1; }
    time_t now = time(NULL);
    char when[64]; strftime(when, sizeof when, "%Y-%m-%d %H:%M", localtime(&now));
    int used = g_n - n_off;
    fprintf(t, "HD texture check, %s\n%d texture file(s) in textures/ (%d turned off)\n%d error(s), %d warning(s), %d note(s), %d fine\n%d dumped texture(s) without an HD one\n",
            when, used, n_off, counts[ERR], counts[WARN], counts[INFO], counts[OK], missing);
    if (restored) fprintf(t, "%d texture(s) turned back on\n", restored);
    if (disabled) fprintf(t, "%d texture(s) with an error turned off (.png.off)\n", disabled);
    if (fixed) fprintf(t, "%d texture(s) with coloured see-through edges fixed (copies in " OUT_DIR "/backup/)\n", fixed);
    fprintf(h, "<!doctype html><html><head><meta charset=\"utf-8\"><title>HD texture check</title><style>"
               "body{font:14px system-ui,sans-serif;background:#1d1f24;color:#e6e6e6;margin:16px}"
               "h1{font-size:20px}table{border-collapse:collapse;width:100%%}td,th{border-bottom:1px solid #3a3d44;padding:6px;vertical-align:top;text-align:left}"
               ".error{color:#ff7b72}.warning{color:#f2cc60}.info{color:#9aa4b2}img{max-width:100%%;image-rendering:pixelated}"
               "code{color:#a5d6ff}.sum span{display:inline-block;margin-right:18px}</style></head><body>\n");
    fprintf(h, "<h1>HD texture check</h1><p>%s &middot; %d texture file(s) in <code>textures/</code> (%d turned off)</p>\n", when, used, n_off);
    fprintf(h, "<p class=\"sum\"><span class=\"error\">%d error(s)</span><span class=\"warning\">%d warning(s)</span><span class=\"info\">%d note(s)</span><span>%d fine</span><span>%d dumped without an HD one</span></p>\n",
            counts[ERR], counts[WARN], counts[INFO], counts[OK], missing);
    if (restored || disabled || fixed) fprintf(h, "<p>%d turned back on &middot; %d turned off &middot; %d edges fixed</p>\n", restored, disabled, fixed);
    fprintf(h, "<p>Pictures: the game's original, then the HD texture over a checkerboard, black, white and green (halos and holes show there). "
               "An error makes the texture look wrong in the game: <code>textures-check --disable</code> turns those off (the game draws its own picture again), "
               "<code>--restore</code> turns them back on.</p>\n<table><tr><th>Texture</th><th>Size</th><th>What's wrong</th></tr>\n");
    for (int lv = ERR; lv >= INFO; lv--)
        for (int i = 0; i < g_n; i++) {
            Entry* e = &g_e[i];
            if (e->disabled || e->level != lv) continue;
            fprintf(t, "\n[%s] textures/%s", LEVEL[lv], e->rel);
            if (e->w) fprintf(t, "  %dx%d", e->w, e->h);
            if (e->ow) fprintf(t, " (original %dx%d, %s)", e->ow, e->oh, e->orig_dir);
            fprintf(t, "\n  %s\n", e->issues);
            fprintf(h, "<tr><td><code>%s</code>", e->rel);
            if (e->image[0]) fprintf(h, "<br><img src=\"%s\" alt=\"\">", e->image);
            fprintf(h, "</td><td>");
            if (e->w) fprintf(h, "%d&times;%d", e->w, e->h);
            if (e->ow) fprintf(h, "<br>original %d&times;%d", e->ow, e->oh);
            fprintf(h, "</td><td class=\"%s\">", LEVEL[lv]);
            html_text(h, e->issues);
            fprintf(h, "</td></tr>\n");
        }
    if (n_off) {
        fprintf(t, "\nTurned off (rename back, or textures-check --restore):\n");
        fprintf(h, "<tr><th colspan=\"3\">Turned off (<code>--restore</code> turns them back on)</th></tr>\n");
        for (int i = 0; i < g_n; i++) if (g_e[i].disabled) { fprintf(t, "  textures/%s\n", g_e[i].rel); fprintf(h, "<tr><td colspan=\"3\"><code>%s</code></td></tr>\n", g_e[i].rel); }
    }
    if (g_opt_images) {
        fprintf(h, "<tr><th colspan=\"3\">Fine</th></tr>\n");
        for (int i = 0; i < g_n; i++) if (!g_e[i].disabled && g_e[i].level == OK && g_e[i].image[0])
            fprintf(h, "<tr><td><code>%s</code><br><img src=\"%s\" alt=\"\"></td><td>%d&times;%d<br>original %d&times;%d</td><td>fine</td></tr>\n", g_e[i].rel, g_e[i].image, g_e[i].w, g_e[i].h, g_e[i].ow, g_e[i].oh);
    }
    fprintf(h, "</table></body></html>\n");
    fclose(t); fclose(h);
    printf("%d texture(s): %d error(s), %d warning(s), %d note(s), %d fine; %d dumped without an HD one. See " OUT_DIR "/report.html\n",
           used, counts[ERR], counts[WARN], counts[INFO], counts[OK], missing);
#ifdef _WIN32
    if (!quiet) ShellExecuteA(NULL, "open", OUT_DIR "\\report.html", NULL, NULL, SW_SHOWNORMAL);
#else
    (void)quiet;
#endif
    return 0;
}
