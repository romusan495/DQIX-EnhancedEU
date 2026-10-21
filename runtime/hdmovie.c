/* HD movies (hdmovie.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nds.h"
#include "hdmovie.h"
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

int g_hdmovie_on;

#define SIG_W 16
#define SIG_H 12
#define SIG_N (SIG_W * SIG_H * 3)
#define ACTIVE_FRAMES 150         /* a movie counts as playing this long after the game last read it */
#define MATCH_MAX 7               /* signatures this close (mean difference per colour, 0..255) are the same frame */
#define CONTRAST_MIN 48           /* a frame this flat (brightest minus darkest block, r+g+b) is a fade */
#define NCC_MIN 0.9                /* ...and its shapes this alike (correlation) to start following a movie */
#define PIXEL_MAX 72              /* a screen pixel this far from the HD frame (r+g+b) keeps the game's pixel */

typedef struct {
    char name[64];
    uint32_t rom0, rom1;          /* where the .mods is in the ROM */
    uint64_t last_read; int read;
    uint8_t* sig; int n;          /* n frames */
    uint32_t* gop; int ngop;      /* .mpg offsets where a group of pictures starts (a sequence header) */
    uint8_t* part;                /* ...in which file: <name>.mpg (0), or <name>.1.mpg, .2... (1, 2...) */
    int parts;
    int* first;                   /* ...the frame it starts with */
    plm_buffer_t* buf; plm_video_t* vid;
    int next;                     /* the frame plm_video_decode returns next */
    uint8_t* rgb; int w, h, have; /* the frame in rgb (have = its index, -1 none) */
    uint8_t* small;               /* ...made 256x192 (to compare with the screen) */
} Movie;

static Movie g_mov[16];
static int g_nmov, g_inited;
static struct { int m, idx, shown_m, shown_idx; uint8_t mask[256 * 192]; } g_eng[2] = { { -1, 0, -1, -1, { 0 } }, { -1, 0, -1, -1, { 0 } } };

static void on_card_read(uint32_t addr, uint32_t len) {
    (void)len;
    for (int i = 0; i < g_nmov; i++)
        if (addr >= g_mov[i].rom0 && addr < g_mov[i].rom1) { g_mov[i].last_read = g_frame; g_mov[i].read = 1; }
}

static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static void close_movie(Movie* m) {
    if (m->vid) plm_video_destroy(m->vid);       /* destroys the buffer too */
    m->vid = NULL; m->buf = NULL; m->have = -1;
}

static void add_movie(const char* name, uint32_t rom0, uint32_t rom1) {
    if (g_nmov >= (int)(sizeof g_mov / sizeof g_mov[0])) return;
    char path[128];
    snprintf(path, sizeof path, "movies/%s.sig", name);
    FILE* f = fopen(path, "rb");
    if (!f) return;
    uint8_t hd[8]; Movie m; memset(&m, 0, sizeof m);
    if (fread(hd, 1, 8, f) == 8 && !memcmp(hd, "DQMS", 4) && (m.n = (int)rd32(hd + 4)) > 0 && m.n < 100000) {
        m.sig = malloc((size_t)m.n * SIG_N);
        if (fread(m.sig, SIG_N, (size_t)m.n, f) != (size_t)m.n) { free(m.sig); m.sig = NULL; }
    }
    fclose(f);
    if (!m.sig) return;
    /* the groups of pictures (each starts with a sequence header: 00 00 01 B3),
     * in one file or in parts (each under 30 MB, to copy them more easily) */
    static uint8_t chunk[1 << 16];
    int cap = 0, pics = 0;
    for (int part = 0; part < 100; part++) {
    if (part == 0) snprintf(path, sizeof path, "movies/%s.mpg", name);
    else snprintf(path, sizeof path, "movies/%s.%d.mpg", name, part);
    if (!(f = fopen(path, "rb"))) { if (part == 0) continue; break; }
    m.parts = part + 1;
    uint32_t pos = 0, st = 0xFFFFFFFF; size_t got;
    while ((got = fread(chunk, 1, sizeof chunk, f)) > 0) {
        for (size_t i = 0; i < got; i++) {
            st = st << 8 | chunk[i];
            if (st == 0x000001B3) {
                if (m.ngop == cap) { cap = cap ? cap * 2 : 64; m.gop = realloc(m.gop, sizeof(uint32_t) * (size_t)cap); m.first = realloc(m.first, sizeof(int) * (size_t)cap); m.part = realloc(m.part, (size_t)cap); }
                m.first[m.ngop] = pics; m.part[m.ngop] = (uint8_t)part;
                m.gop[m.ngop++] = pos + (uint32_t)i - 3;
            } else if (st == 0x00000100) pics++;
        }
        pos += (uint32_t)got;
    }
    fclose(f);
    if (part == 0) break;                                        /* one file */
    }
    if (!m.ngop || m.first[0] != 0) { free(m.sig); free(m.gop); free(m.first); free(m.part); return; }
    snprintf(m.name, sizeof m.name, "%s", name);
    m.rom0 = rom0; m.rom1 = rom1; m.have = -1;
    g_mov[g_nmov++] = m;
    LOG_INFO("HD movie %s: %d frames, %d groups, %d file(s)\n", name, m.n, m.ngop, m.parts);
}

/* data/movie/ in the ROM's file table */
static void walk(const uint8_t* fnt, const uint8_t* fat, uint32_t fat_n, int dir, const char* path, int depth) {
    if (depth > 8) return;
    uint32_t off = rd32(fnt + (dir & 0xFFF) * 8); int fid = fnt[(dir & 0xFFF) * 8 + 4] | fnt[(dir & 0xFFF) * 8 + 5] << 8;
    const uint8_t* p = fnt + off;
    for (;;) {
        int b = *p++;
        if (!b) break;
        char name[130]; int n = b & 0x7F;
        memcpy(name, p, (size_t)n); name[n] = 0; p += n;
        if (b & 0x80) {
            int sub = p[0] | p[1] << 8; p += 2;
            char sp[300]; snprintf(sp, sizeof sp, "%s%s/", path, name);
            walk(fnt, fat, fat_n, sub, sp, depth + 1);
        } else {
            if (!strcmp(path, "data/movie/") && n > 5 && !strcmp(name + n - 5, ".mods") && (uint32_t)fid < fat_n) {
                name[n - 5] = 0;
                add_movie(name, rd32(fat + fid * 8), rd32(fat + fid * 8 + 4));
            }
            fid++;
        }
    }
}

void hdmovie_init(void) {
    for (int i = 0; i < g_nmov; i++) { close_movie(&g_mov[i]); free(g_mov[i].sig); free(g_mov[i].gop); free(g_mov[i].first); free(g_mov[i].part); free(g_mov[i].rgb); free(g_mov[i].small); }
    g_nmov = 0; g_inited = 1;
    g_eng[0].m = g_eng[1].m = -1;
    if (!g_rom || g_rom_size < 0x200) return;
    uint32_t fnt = rd32(g_rom + 0x40), fnt_n = rd32(g_rom + 0x44), fat = rd32(g_rom + 0x48), fat_n = rd32(g_rom + 0x4C);
    if ((size_t)fnt + fnt_n > g_rom_size || (size_t)fat + fat_n > g_rom_size) return;
    walk(g_rom + fnt, g_rom + fat, fat_n / 8, 0xF000, "", 0);
    g_card_read_hook = g_nmov ? on_card_read : NULL;
}
int hdmovie_count(void) { return g_nmov; }

static int sig_contrast(const int* s);

/* the screen made into a signature, like tools/upscale_movies.py does */
static void screen_sig(const uint32_t* px, int* out) {
    memset(out, 0, sizeof(int) * SIG_N);
    for (int y = 0; y < 192; y++)
        for (int x = 0; x < 256; x++) {
            uint32_t c = px[y * 256 + x]; int* o = &out[((y / 16) * SIG_W + x / 16) * 3];
            o[0] += (c >> 16) & 255; o[1] += (c >> 8) & 255; o[2] += c & 255;
        }
    for (int i = 0; i < SIG_N; i++) out[i] = (out[i] + 128) / 256;
}
static int sig_contrast8(const uint8_t* s) {
    int t[SIG_N]; for (int i = 0; i < SIG_N; i++) t[i] = s[i];
    return sig_contrast(t);
}
static double sig_ncc(const int* a, const uint8_t* b) {        /* the same shapes (1) whatever the colours' level */
    double ma = 0, mb = 0;
    for (int i = 0; i < SIG_N; i++) { ma += a[i]; mb += b[i]; }
    ma /= SIG_N; mb /= SIG_N;
    double ab = 0, aa = 0, bb = 0;
    for (int i = 0; i < SIG_N; i++) { double x = a[i] - ma, y = b[i] - mb; ab += x * y; aa += x * x; bb += y * y; }
    return aa > 0 && bb > 0 ? ab / sqrt(aa * bb) : 0;
}
static int sig_dist(const int* a, const uint8_t* b) {          /* sum of differences (/SIG_N: mean) */
    int d = 0;
    for (int i = 0; i < SIG_N; i++) { int t = a[i] - b[i]; d += t < 0 ? -t : t; }
    return d;
}
static int sig_contrast(const int* s) {                        /* how much the picture varies (0: one colour) */
    int lo = 255 * 3, hi = 0;
    for (int i = 0; i < SIG_W * SIG_H; i++) { int l = s[i * 3] + s[i * 3 + 1] + s[i * 3 + 2]; if (l < lo) lo = l; if (l > hi) hi = l; }
    return hi - lo;
}

/* decoding from group of pictures g on */
static int open_at(Movie* m, int g) {
    close_movie(m);
    char path[128];
    if (m->part[g] == 0) snprintf(path, sizeof path, "movies/%s.mpg", m->name);
    else snprintf(path, sizeof path, "movies/%s.%d.mpg", m->name, m->part[g]);
    m->buf = plm_buffer_create_with_filename(path);
    if (!m->buf) return 0;
    plm_buffer_seek(m->buf, m->gop[g]);
    m->vid = plm_video_create_with_buffer(m->buf, 1);
    plm_video_set_no_delay(m->vid, 1);                           /* no B-frames: each picture as it is decoded */
    m->next = m->first[g];
    return 1;
}

/* frame idx of movie m into m->rgb / m->small */
static int decode_to(Movie* m, int idx) {
    if (m->have == idx) return 1;
    if (!m->vid || idx < m->next || idx >= m->next + 48) {     /* start again from the group of pictures it is in */
        int g = m->ngop - 1;
        while (g > 0 && m->first[g] > idx) g--;
        if (!open_at(m, g)) return 0;
    }
    plm_frame_t* fr = NULL;
    while (m->next <= idx) {
        fr = plm_video_decode(m->vid);
        if (!fr) {                                               /* the end of a part: on in the next one */
            int g = 0;
            while (g < m->ngop && m->first[g] != m->next) g++;
            if (g == m->ngop || m->part[g] == 0 || (g > 0 && m->part[g] == m->part[g - 1]) || !open_at(m, g)) { close_movie(m); return 0; }
            continue;
        }
        m->next++;
    }
    int w = (int)fr->width, h = (int)fr->height;
    if (w < 256 || h < 192 || w % 256 || h % 192 || w / 256 != h / 192) { close_movie(m); return 0; }
    if (!m->rgb || m->w != w || m->h != h) {
        free(m->rgb); m->rgb = malloc((size_t)w * h * 3); m->w = w; m->h = h;
        if (!m->small) m->small = malloc(256 * 192 * 3);
    }
    plm_frame_to_rgb(fr, m->rgb, w * 3);
    int k = w / 256;
    for (int y = 0; y < 192; y++)
        for (int x = 0; x < 256; x++)
            for (int c = 0; c < 3; c++) {
                int s = 0;
                for (int j = 0; j < k; j++) { const uint8_t* r = m->rgb + ((size_t)(y * k + j) * w + (size_t)x * k) * 3 + c; for (int i = 0; i < k; i++) s += r[i * 3]; }
                m->small[(y * 256 + x) * 3 + c] = (uint8_t)(s / (k * k));
            }
    m->have = idx;
    return 1;
}

int hdmovie_frame(int eng, const uint32_t* px, const uint8_t** rgb, int* w, int* h, const uint8_t** mask, int* fresh) {
    if (!g_inited) hdmovie_init();
    if (!g_hdmovie_on || !g_nmov || eng < 0 || eng > 1) return 0;
    int active = 0;
    for (int i = 0; i < g_nmov; i++) {
        Movie* m = &g_mov[i];
        if (m->read && g_frame - m->last_read <= ACTIVE_FRAMES) active |= 1 << i;
        else if (m->vid && g_eng[0].m != i && g_eng[1].m != i) close_movie(m);   /* finished: the file is closed */
    }
    if (!active) { g_eng[eng].m = -1; return 0; }
    int s[SIG_N]; screen_sig(px, s);
    int bm = -1, bi = 0, bd = MATCH_MAX * SIG_N + 1;
    int cm = g_eng[eng].m;
    if (cm >= 0 && (active >> cm & 1)) {                         /* the frames after the one shown last */
        int from = g_eng[eng].idx - 1 < 0 ? 0 : g_eng[eng].idx - 1;
        for (int i = g_eng[eng].idx; i < g_mov[cm].n && i <= g_eng[eng].idx + 12; i++) { int d = sig_dist(s, g_mov[cm].sig + (size_t)i * SIG_N); if (d < bd) { bd = d; bm = cm; bi = i; } }
        if (bm < 0) { int d = sig_dist(s, g_mov[cm].sig + (size_t)from * SIG_N); if (d < bd) { bd = d; bm = cm; bi = from; } }
    }
    if (bm < 0) {                                                /* anywhere in the movies being read */
        /* not from a frame of one colour (a fade): a black or white screen with
         * anything on it would match those. Once found, the movie is followed
         * through them. */
        if (sig_contrast(s) < CONTRAST_MIN) { g_eng[eng].m = -1; return 0; }
        for (int j = 0; j < g_nmov; j++) if (active >> j & 1)
            for (int i = 0; i < g_mov[j].n; i++) {
                const uint8_t* fs = g_mov[j].sig + (size_t)i * SIG_N;
                int d = sig_dist(s, fs);
                if (d < bd && sig_contrast8(fs) >= CONTRAST_MIN) { bd = d; bm = j; bi = i; }
            }
        /* a dark screen with a few things on it is close to a dark frame with
         * others: the shapes must be the same too */
        if (bm >= 0 && sig_ncc(s, g_mov[bm].sig + (size_t)bi * SIG_N) < NCC_MIN) bm = -1;
    }
    if (bm < 0) { g_eng[eng].m = -1; return 0; }
    Movie* m = &g_mov[bm];
    g_eng[eng].m = bm; g_eng[eng].idx = bi;
    if (!decode_to(m, bi)) { g_eng[eng].m = -1; return 0; }
    /* the HD frame where the screen shows the movie; the game's pixels where it differs */
    uint8_t* mk = g_eng[eng].mask; int shown = 0;
    for (int i = 0; i < 256 * 192; i++) {
        uint32_t c = px[i]; const uint8_t* q = m->small + i * 3;
        int d = abs((int)((c >> 16) & 255) - q[0]) + abs((int)((c >> 8) & 255) - q[1]) + abs((int)(c & 255) - q[2]);
        mk[i] = d <= PIXEL_MAX ? 255 : 0; shown += mk[i] != 0;
    }
    {   /* a lone pixel off (a sharp edge the upscale moved) stays HD */
        static uint8_t tmp[256 * 192];
        memcpy(tmp, mk, sizeof tmp);
        for (int y = 1; y < 191; y++) for (int x = 1; x < 255; x++) {
            int i = y * 256 + x; if (tmp[i]) continue;
            int on = !!tmp[i - 1] + !!tmp[i + 1] + !!tmp[i - 256] + !!tmp[i + 256];
            if (on >= 3) mk[i] = 255;
        }
    }
    { static int lg = -1; if (lg < 0) lg = getenv("DQIX_HDMOVIE_LOG") != NULL;
      if (lg) fprintf(stderr, "hdmovie f%llu eng %d: %s frame %d (diff %.1f), %d%% HD\n", (unsigned long long)g_frame, eng, m->name, bi, (double)bd / SIG_N, shown * 100 / (256 * 192)); }
    *rgb = m->rgb; *w = m->w; *h = m->h; *mask = mk;
    *fresh = g_eng[eng].shown_m != bm || g_eng[eng].shown_idx != bi;
    g_eng[eng].shown_m = bm; g_eng[eng].shown_idx = bi;
    return 1;
}
