/* textures-hd: makes HD versions (textures/hd) of the textures the game dumped
 * (textures/dump) that don't have one yet, with Real-ESRGAN. The game starts it
 * when it closes after dumping new ones; double-clicking it works too. The new
 * HD textures show the next time you play.
 *
 * Real-ESRGAN (realesrgan-ncnn-vulkan, on the graphics card) lives in
 * textures-hd-outil/ next to the game. What it can't do well by itself:
 * - DS textures repeat: each one is padded by wrapping before and cropped
 *   after, so there are no seams where it tiles;
 * - see-through texels get their solid neighbours' colour first, so no dark
 *   fringes come in from the edges;
 * - alpha is upscaled as its own grey picture; on/off alpha stays on/off;
 * - Real-ESRGAN alone smooths away the drawing's grain (hair strands, eye
 *   pixels): its colours are mixed half and half with a sharp (Lanczos)
 *   enlargement of the original, which keeps the detail.
 * What happened goes to textures/hd-auto.log. */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir_(p) _mkdir(p)
#define SEP "\\"
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define mkdir_(p) mkdir(p, 0755)
#define SEP "/"
#endif

#define PAD 8
#define SCALE 4
static FILE* g_log;

static void logf_(const char* fmt, ...) {
    va_list a; va_start(a, fmt);
    if (g_log) { vfprintf(g_log, fmt, a); fflush(g_log); }
    va_end(a);
}

static int exists(const char* p) { FILE* f = fopen(p, "rb"); if (f) fclose(f); return f != NULL; }

/* the dumped textures' names: 16 hex digits + ".png" */
static int list_dump(const char* dir, char (*names)[32], int max) {
    int n = 0;
#ifdef _WIN32
    char pat[600]; snprintf(pat, sizeof pat, "%s\\*.png", dir);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do { if (strlen(fd.cFileName) == 20 && n < max) snprintf(names[n++], 32, "%s", fd.cFileName); } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(dir); struct dirent* e;
    if (!d) return 0;
    while ((e = readdir(d))) { size_t l = strlen(e->d_name); if (l == 20 && !strcmp(e->d_name + 16, ".png") && n < max) snprintf(names[n++], 32, "%s", e->d_name); }
    closedir(d);
#endif
    return n;
}

/* see-through texels take the average colour of their solid (or already filled) neighbours */
static void bleed(unsigned char* px, int w, int h) {
    int n = w * h, any = 0, all = 1;
    for (int i = 0; i < n; i++) { if (px[i * 4 + 3]) any = 1; else all = 0; }
    if (!any || all) return;
    unsigned char* filled = malloc((size_t)n); unsigned char* nf = malloc((size_t)n);
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
                if (c) { for (int k = 0; k < 3; k++) px[i * 4 + k] = (unsigned char)((s[k] + c / 2) / c); nf[i] = 1; }
                else left = 1;
            }
        memcpy(filled, nf, (size_t)n);
        if (!left) break;
    }
    free(filled); free(nf);
}

/* the picture with p texels of wrap around it, 3 channels (colour, or alpha as grey) */
static unsigned char* padded(const unsigned char* px, int w, int h, int p, int alpha) {
    int W = w + 2 * p, H = h + 2 * p;
    unsigned char* o = malloc((size_t)W * H * 3);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int sx = ((x - p) % w + w) % w, sy = ((y - p) % h + h) % h;
            const unsigned char* s = px + (sy * w + sx) * 4;
            unsigned char* d = o + (y * W + x) * 3;
            if (alpha) d[0] = d[1] = d[2] = s[3]; else { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; }
        }
    return o;
}

/* the colours enlarged SCALE times with a Lanczos-3 filter, wrapping like the texture (3 channels) */
static float lanczos3(float x) {
    if (x < 0) x = -x;
    if (x < 1e-6f) return 1.0f;
    if (x >= 3.0f) return 0.0f;
    const float pi = 3.14159265f;
    return 3.0f * sinf(pi * x) * sinf(pi * x / 3.0f) / (pi * pi * x * x);
}
static unsigned char* lanczos_up(const unsigned char* px, int w, int h) {
    int W = w * SCALE, H = h * SCALE;
    float* row = malloc(sizeof(float) * (size_t)W * h * 3);
    for (int y = 0; y < h; y++)                                  /* across */
        for (int X = 0; X < W; X++) {
            float c = (X + 0.5f) / SCALE - 0.5f, s[3] = { 0, 0, 0 }, ws = 0;
            for (int k = (int)floorf(c) - 2; k <= (int)floorf(c) + 3; k++) {
                float f = lanczos3(c - (float)k); int sx = ((k % w) + w) % w;
                for (int ch = 0; ch < 3; ch++) s[ch] += f * px[(y * w + sx) * 4 + ch];
                ws += f;
            }
            for (int ch = 0; ch < 3; ch++) row[((size_t)y * W + X) * 3 + ch] = s[ch] / ws;
        }
    unsigned char* o = malloc((size_t)W * H * 3);
    for (int Y = 0; Y < H; Y++)                                  /* down */
        for (int X = 0; X < W; X++) {
            float c = (Y + 0.5f) / SCALE - 0.5f, s[3] = { 0, 0, 0 }, ws = 0;
            for (int k = (int)floorf(c) - 2; k <= (int)floorf(c) + 3; k++) {
                float f = lanczos3(c - (float)k); int sy = ((k % h) + h) % h;
                for (int ch = 0; ch < 3; ch++) s[ch] += f * row[((size_t)sy * W + X) * 3 + ch];
                ws += f;
            }
            for (int ch = 0; ch < 3; ch++) { float v = s[ch] / ws + 0.5f; o[((size_t)Y * W + X) * 3 + ch] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v); }
        }
    free(row);
    return o;
}

static int run_upscaler(const char* tool, const char* in, const char* out) {
    char cmd[2400];
    const char* over = getenv("TEXHD_UPSCALER");                 /* tests: another upscaler, "<cmd> <in> <out>" */
    if (over) snprintf(cmd, sizeof cmd, "%s \"%s\" \"%s\"", over, in, out);
    else snprintf(cmd, sizeof cmd, "\"%s" SEP "realesrgan-ncnn-vulkan.exe\" -i \"%s\" -o \"%s\" -n realesrgan-x4plus-anime -s 4 -f png", tool, in, out);
    logf_("  %s\n", cmd);
#ifdef _WIN32
    if (!over) {
        STARTUPINFOA si; PROCESS_INFORMATION pi; memset(&si, 0, sizeof si); si.cb = sizeof si;
        char models[700]; snprintf(models, sizeof models, "%s", tool);
        if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, models, &si, &pi)) { logf_("  can't start Real-ESRGAN (%lu)\n", GetLastError()); return -1; }
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD rc = 1; GetExitCodeProcess(pi.hProcess, &rc); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        return (int)rc;
    }
#endif
    return system(cmd);
}

static void rmtree_files(const char* dir, char (*names)[32], int n) {
    for (int i = 0; i < n; i++) { char p[700]; snprintf(p, sizeof p, "%s" SEP "%s", dir, names[i]); remove(p); }
}

static int upscale_new(const char* game) {
    char dump[600], hd[600], tool[600], tmp[600], in_c[640], in_a[640], out_c[640], out_a[640], logp[600];
    snprintf(dump, sizeof dump, "%s" SEP "textures" SEP "dump", game);
    snprintf(hd, sizeof hd, "%s" SEP "textures" SEP "hd", game);
    snprintf(tool, sizeof tool, "%s" SEP "textures-hd-outil", game);
    snprintf(tmp, sizeof tmp, "%s" SEP "textures" SEP "hd-auto-tmp", game);
    snprintf(logp, sizeof logp, "%s" SEP "textures" SEP "hd-auto.log", game);
    static char names[20000][32];
    int n = list_dump(dump, names, 20000), todo = 0;
    static char work[20000][32]; static int pads[20000];
    for (int i = 0; i < n; i++) { char p[700]; snprintf(p, sizeof p, "%s" SEP "%s", hd, names[i]); if (!exists(p)) snprintf(work[todo++], 32, "%s", names[i]); }
    if (!todo) return 0;
    g_log = fopen(logp, "a");
    time_t t = time(NULL); logf_("\n%s%d new texture(s) in textures/dump\n", ctime(&t), todo);
    mkdir_(hd); mkdir_(tmp);
    snprintf(in_c, sizeof in_c, "%s" SEP "c", tmp); snprintf(in_a, sizeof in_a, "%s" SEP "a", tmp);
    snprintf(out_c, sizeof out_c, "%s" SEP "c4", tmp); snprintf(out_a, sizeof out_a, "%s" SEP "a4", tmp);
    mkdir_(in_c); mkdir_(in_a); mkdir_(out_c); mkdir_(out_a);
    static char alist[20000][32]; int na = 0;
    for (int i = 0; i < todo; i++) {
        char p[700]; snprintf(p, sizeof p, "%s" SEP "%s", dump, work[i]);
        int w, h, c; unsigned char* px = stbi_load(p, &w, &h, &c, 4);
        pads[i] = -1;
        if (!px) { logf_("  %s: can't read\n", work[i]); continue; }
        int pd = PAD < w ? (PAD < h ? PAD : h) : (w < h ? w : h);
        pads[i] = pd;
        int solid = 1; for (int k = 0; k < w * h; k++) if (px[k * 4 + 3] != 255) { solid = 0; break; }
        bleed(px, w, h);
        unsigned char* o = padded(px, w, h, pd, 0);
        snprintf(p, sizeof p, "%s" SEP "%s", in_c, work[i]); stbi_write_png(p, w + 2 * pd, h + 2 * pd, 3, o, (w + 2 * pd) * 3); free(o);
        if (!solid) {
            o = padded(px, w, h, pd, 1);
            snprintf(p, sizeof p, "%s" SEP "%s", in_a, work[i]); stbi_write_png(p, w + 2 * pd, h + 2 * pd, 3, o, (w + 2 * pd) * 3); free(o);
            snprintf(alist[na++], 32, "%s", work[i]);
        }
        stbi_image_free(px);
    }
    int rc = run_upscaler(tool, in_c, out_c);
    if (rc == 0 && na) rc = run_upscaler(tool, in_a, out_a);
    if (rc != 0) logf_("  Real-ESRGAN failed (%d)\n", rc);
    int made = 0;
    for (int i = 0; i < todo && rc == 0; i++) {
        if (pads[i] < 0) continue;
        char p[700]; int w, h, c, W, H, c2;
        snprintf(p, sizeof p, "%s" SEP "%s", dump, work[i]);
        unsigned char* px = stbi_load(p, &w, &h, &c, 4);
        snprintf(p, sizeof p, "%s" SEP "%s", out_c, work[i]);
        unsigned char* big = stbi_load(p, &W, &H, &c2, 3);
        int pd = pads[i];
        if (!px || !big || W != (w + 2 * pd) * SCALE || H != (h + 2 * pd) * SCALE) { logf_("  %s: no upscaled picture\n", work[i]); stbi_image_free(px); stbi_image_free(big); continue; }
        unsigned char* ab = NULL; int binary = 1, solid = 1;
        for (int k = 0; k < w * h; k++) { int a = px[k * 4 + 3]; if (a != 255) solid = 0; if (a != 0 && a != 255) binary = 0; }
        if (!solid) { snprintf(p, sizeof p, "%s" SEP "%s", out_a, work[i]); ab = stbi_load(p, &W, &H, &c2, 3); }
        int ow = w * SCALE, oh = h * SCALE;
        unsigned char* sharp;                                    /* the original's grain, mixed in below */
        { unsigned char* bl = malloc((size_t)w * h * 4); memcpy(bl, px, (size_t)w * h * 4); bleed(bl, w, h); sharp = lanczos_up(bl, w, h); free(bl); }
        unsigned char* o = malloc((size_t)ow * oh * 4);
        for (int y = 0; y < oh; y++)
            for (int x = 0; x < ow; x++) {
                const unsigned char* s = big + ((y + pd * SCALE) * W + x + pd * SCALE) * 3;
                unsigned char* d = o + (y * ow + x) * 4;
                const unsigned char* g = sharp + ((size_t)y * ow + x) * 3;
                for (int ch = 0; ch < 3; ch++) d[ch] = (unsigned char)((s[ch] + g[ch] + 1) / 2);
                if (solid) d[3] = 255;
                else if (ab) { const unsigned char* q = ab + ((y + pd * SCALE) * W + x + pd * SCALE) * 3; int a = (q[0] + q[1] + q[2] + 1) / 3; d[3] = binary ? (a > 127 ? 255 : 0) : (unsigned char)a; }
                else d[3] = px[((y / SCALE) * w + x / SCALE) * 4 + 3];      /* no upscaled alpha: the original's, blocky */
            }
        snprintf(p, sizeof p, "%s" SEP "%s", hd, work[i]);
        if (stbi_write_png(p, ow, oh, 4, o, ow * 4)) made++;
        free(o); free(sharp); stbi_image_free(px); stbi_image_free(big); stbi_image_free(ab);
    }
    rmtree_files(in_c, work, todo); rmtree_files(out_c, work, todo); rmtree_files(in_a, alist, na); rmtree_files(out_a, alist, na);
#ifdef _WIN32
    RemoveDirectoryA(in_c); RemoveDirectoryA(in_a); RemoveDirectoryA(out_c); RemoveDirectoryA(out_a); RemoveDirectoryA(tmp);
#else
    rmdir(in_c); rmdir(in_a); rmdir(out_c); rmdir(out_a); rmdir(tmp);
#endif
    logf_("  %d HD texture(s) written to textures/hd\n", made);
    if (g_log) fclose(g_log);
    return made;
}

int main(int argc, char** argv) {
    char game[600] = ".";
#ifdef _WIN32
    GetModuleFileNameA(NULL, game, sizeof game);
    char* s = strrchr(game, '\\'); if (s) *s = 0;
    SetCurrentDirectoryA(game);
#else
    if (argc > 2 && !strcmp(argv[1], "--game")) snprintf(game, sizeof game, "%s", argv[2]);
#endif
    upscale_new(game);
    return 0;
}
