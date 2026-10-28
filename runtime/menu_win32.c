/* Menu bar for the game window on Windows (see menu.h). */
#include "menu.h"
#include "controls.h"
#include "cheats.h"
#include "uitext.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* ---------------------------------------------------------------- the menus, on every system
 * One tree, built by menu_init: the menu bar shows it (Windows) and so does
 * the in-game menu (gamemenu.c), so both always list the same things. */
#define MAX_NODES 512
static MenuNode g_node[MAX_NODES];
static int g_nnode;
static unsigned char g_checked[MENU_MAX_ID], g_disabled[MENU_MAX_ID];
typedef int Menu;                                 /* a submenu: its node; 0 = the bar */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HMENU g_bar;
static HMENU g_hm[MAX_NODES];                     /* each submenu's popup menu */
#endif

/* L"..." -> UTF-8, as it is ("&Window size\tF3") */
static void to_utf8(const wchar_t* w, char* o, size_t cap) {
    for (; *w; w++) {
        unsigned c = (unsigned)*w;
        char b[4]; int k;
        if (c < 0x80) { b[0] = (char)c; k = 1; }
        else if (c < 0x800) { b[0] = (char)(0xC0 | c >> 6); b[1] = (char)(0x80 | (c & 63)); k = 2; }
        else { b[0] = (char)(0xE0 | c >> 12); b[1] = (char)(0x80 | ((c >> 6) & 63)); b[2] = (char)(0x80 | (c & 63)); k = 3; }
        if (cap <= (size_t)k) break;
        memcpy(o, b, (size_t)k); o += k; cap -= (size_t)k;
    }
    *o = 0;
}
/* "&Window size\tF3" -> text "Window size", key "F3" */
static void split(const char* s, MenuNode* n) {
    char* o = n->text; size_t cap = sizeof n->text;
    for (; *s; s++) {
        if (*s == '&') continue;
        if (*s == '\t') { *o = 0; o = n->key; cap = sizeof n->key; continue; }
        if (cap > 1) { *o++ = *s; cap--; }
    }
    *o = 0;
}

static int add(Menu parent, int kind, int id, const wchar_t* w, const char* utf8) {
    if (g_nnode >= MAX_NODES) return 0;
    int i = g_nnode++;
    MenuNode* n = &g_node[i];
    memset(n, 0, sizeof *n);
    n->parent = (short)parent; n->kind = (short)kind; n->id = id;
    char u[400];
    if (w) { to_utf8(w, u, sizeof u); utf8 = u; }
    if (utf8) { utf8 = T(utf8); split(utf8, n); }           /* in the player's language (uitext.h) */
#ifdef _WIN32
    HMENU pm = parent ? g_hm[parent] : g_bar;
    wchar_t buf[400];
    w = utf8 && MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buf, 400) ? buf : NULL;
    if (pm && i > 0) {                                /* (node 0 is the bar itself, not one of its menus) */
        if (kind == MENU_SUB) { g_hm[i] = CreatePopupMenu(); AppendMenuW(pm, MF_POPUP, (UINT_PTR)g_hm[i], w); }
        else if (kind == MENU_SEP) AppendMenuW(pm, MF_SEPARATOR, 0, NULL);
        else AppendMenuW(pm, kind == MENU_LABEL ? MF_STRING | MF_GRAYED : MF_STRING, (UINT_PTR)id, w);
    }
#endif
    return i;
}
static void item(Menu m, int id, const wchar_t* text) { add(m, MENU_ITEM, id, text, NULL); }
static void item8(Menu m, int id, const char* text) { add(m, MENU_ITEM, id, NULL, text); }
static void label(Menu m, const wchar_t* text) { add(m, MENU_LABEL, 0, text, NULL); }
static void sep(Menu m) { add(m, MENU_SEP, 0, NULL, NULL); }
static Menu sub(Menu parent, const wchar_t* text) { return add(parent, MENU_SUB, 0, text, NULL); }

static void build(int all, const char* const* shell_names, int n_shells) {
    g_nnode = 0;
    add(0, MENU_SUB, 0, L"Menu", NULL);           /* node 0: the bar itself */
    Menu game = sub(0, L"&Game");
    item(game, MI_PAUSE, L"&Pause\tP");
    item(game, MI_TURBO, L"&Fast forward\t(or hold Tab)");
    item(game, MI_RESET, L"&Reset...\tCtrl+R");
    sep(game);
    item(game, MI_VOLUME, L"&Volume...\t- / =");
    item(game, MI_MUTE, L"&Mute");
    item(game, MI_CONTROLS_EDIT, L"&Controls...");
    sep(game);
    item(game, MI_SAVEBAK, L"&Keep a copy of every save (save-backups folder)");
    sep(game);
    item(game, MI_SHOT, L"Save &screenshot\tF12");
    item(game, MI_REPORT, L"Save &bug report\tF10");
    sep(game);
    item(game, MI_EXIT, L"E&xit\tEsc");

    /* all = 0 (the usual): only what the project's own setup uses; the rest
     * keeps working from settings.ini (all_options = 1 shows it again) */
    Menu gfx = sub(0, L"G&raphics");
    if (all) {
        item(gfx, MI_CLASSIC, L"&Classic: exactly like the DS\tF5");
        item(gfx, MI_ENHANCED, L"&Enhanced: sharper 3D\tF5");
        sep(gfx);
        Menu res = sub(gfx, L"3D &resolution (Enhanced)\tF4");
        item(res, MI_RES1, L"1x (DS)");
        item(res, MI_RES2, L"2x");
        item(res, MI_RES3, L"3x");
        item(res, MI_RES4, L"4x");
        item(res, MI_RES6, L"6x");
        item(res, MI_RES8, L"8x");
        sep(res);
        item(res, MI_RES_AUTO, L"&Auto: as sharp as your screen (up to 12x)");
    }
    item(gfx, MI_SMOOTH, L"&Smooth textures (Enhanced)");
    item(gfx, MI_TEXUP, L"&Upscaled textures, xBR (Enhanced)");
    item(gfx, MI_SMOOTH2D, L"Smooth &2D: menus, maps, sprites (Enhanced)");
    if (all) {
        Menu oln = sub(gfx, L"Character &outlines (Enhanced)");
        item(oln, MI_OUTLINE0, L"Like the &DS");
        item(oln, MI_OUTLINE0 + 1, L"&Thin");
        item(oln, MI_OUTLINE0 + 2, L"&None");
        Menu wst = sub(gfx, L"&Window style (Enhanced)");
        item(wst, MI_WINSTYLE0, L"Like the &DS");
        item(wst, MI_WINSTYLE0 + 1, L"&Minimal: no frame, more see-through");
        item(wst, MI_WINSTYLE0 + 2, L"M&odern: square, dark see-through, a thin grey line");
    }
    sep(gfx);
    item(gfx, MI_TEXPACK, L"&HD textures from the textures folder (Enhanced)");
    item(gfx, MI_TEXRELOAD, L"&Reload the HD textures");
    if (all) item(gfx, MI_TEXDUMP, L"&Dump textures to textures\\dump (for making a pack)");
    sep(gfx);
    item(gfx, MI_HDTEXT, L"HD te&xt (Enhanced)");
    Menu fnt = sub(gfx, L"Text &font");
    item(fnt, MI_FONT0, L"The game's own letters, &smoothed");
    if (all) {
        item(fnt, MI_FONT0 + 1, L"&Georgia");
        item(fnt, MI_FONT0 + 2, L"&Palatino Linotype");
        item(fnt, MI_FONT0 + 3, L"&Constantia");
        item(fnt, MI_FONT0 + 4, L"&Times New Roman");
        item(fnt, MI_FONT0 + 5, L"Segoe &UI");
    }
    sep(fnt);                                                   /* these come with the game (fonts/) */
    if (all) {
        item(fnt, MI_FONT0 + 6, L"&Lora");
        item(fnt, MI_FONT0 + 7, L"C&rimson Pro");
        item(fnt, MI_FONT0 + 8, L"&Alegreya");
        item(fnt, MI_FONT0 + 9, L"Spectral");
        item(fnt, MI_FONT0 + 10, L"&EB Garamond");
        item(fnt, MI_FONT0 + 11, L"P&hilosopher");
    }
    item(fnt, MI_FONT0 + 12, L"&Marcellus");
    if (all) {
        item(fnt, MI_FONT0 + 13, L"&Nunito (rounded)");
        item(fnt, MI_FONT0 + 14, L"&Fredoka (rounded, bold)");
    }
    sep(gfx);
    item(gfx, MI_HDMOVIE, L"HD &movies from the movies folder (Enhanced)");

    if (all) {
        Menu scr = sub(0, L"&Screen");
        item(scr, MI_LCD, L"&DS Lite screen\tF6");
        sep(scr);
        item(scr, MI_LCDCOL, L"LCD &colours");
        item(scr, MI_GRID, L"&Pixel grid (at 3x and larger)");
        item(scr, MI_GHOST, L"&Motion blur");
        Menu bl = sub(scr, L"&Backlight");
        item(bl, MI_BL1, L"&Low");
        item(bl, MI_BL2, L"&Medium");
        item(bl, MI_BL3, L"&High");
        item(bl, MI_BL4, L"Ma&x");
    }

    Menu view = sub(0, L"&View");
    if (all) {
        item(view, MI_SHELL, L"Show the DS &Lite\tF7");
        Menu col = sub(view, L"Console &colour");
        for (int i = 0; i < n_shells; i++) item8(col, MI_SHELLC0 + i, shell_names[i]);
        item(view, MI_GAP, L"&Gap between the screens");
        item(view, MI_ACTUAL, L"&Actual size (as big as a real DS Lite)\tF8");
        sep(view);
    }
    Menu size = sub(view, L"&Window size\tF3");
    item(size, MI_SIZE1, L"1x");
    item(size, MI_SIZE2, L"2x");
    item(size, MI_SIZE3, L"3x");
    item(size, MI_SIZE4, L"4x");
    if (all) item(view, MI_INTEGER, L"&Integer scaling");
    item(view, MI_FULL, L"&Fullscreen\tF11");
    Menu rd = sub(view, L"&Readability of the options menu (Esc)");
    item(rd, MI_UICONTRAST, L"Stronger &contrast");
    sep(rd);
    item(rd, MI_UISIZE0, L"&Normal text");
    item(rd, MI_UISIZE0 + 1, L"&Large text");
    item(rd, MI_UISIZE0 + 2, L"La&rger text");

    Menu wl = sub(0, L"&Wireless");
    item(wl, MI_NET, L"&Connect (Nintendo Wi-Fi Connection replacement)");
    Menu srv = sub(wl, L"&Server");
    item(srv, MI_SRV_BUILTIN, L"&Built-in (offline: DQIX's preserved downloads)");
    sep(srv);
    item(srv, MI_SRV_ALTWFC, L"&AltWFC (172.104.88.237)");
    item(srv, MI_SRV_WIIMMFI, L"&Wiimmfi (164.132.44.106): not for DQIX");
    item(srv, MI_SRV_CUSTOM, L"&Custom (wfc_server in settings.ini)");
    sep(wl);
    Menu friends = sub(wl, L"&Play with friends far away (tag mode, visiting)");
    item(friends, MI_SES_HOST, L"&Host: your friends join you...");
    item(friends, MI_SES_JOIN, L"&Join a friend...");
    item(friends, MI_SES_START, L"&Start now (everyone is here)\tEnter");
    item(friends, MI_SES_LEAVE, L"&Leave and play alone");
    sep(friends);
    item(friends, MI_SES_NAME, L"Your &name...");
    item(friends, MI_SES_HELP, L"&How does this work?");
    sep(wl);
    item(wl, MI_DLC, L"&Unlock all downloadable content now (without Sellma)");
    sep(wl);
    item(wl, MI_NET_HELP, L"&How does this work?");

    Menu modern = sub(0, L"&Modern");
    item(modern, MI_ANALOG, L"Walk in any &direction with the left stick");
    if (all) {
        item(modern, MI_HUD, L"Widescreen: battle &HUD over the 3D (party along the top, commands bottom left)");
    }
    sep(modern);
    item(modern, MI_MINIMAP, L"Map in the top right co&rner (one window, gone during battles)\tM");
    Menu ms = sub(modern, L"Map si&ze");
    item(ms, MI_SSIZE0, L"Small");
    item(ms, MI_SSIZE0 + 1, L"Medium");
    item(ms, MI_SSIZE0 + 2, L"Large");
    item(modern, MI_MAPMARKS, L"&Map: mark the chests, bookshelves and quests (the mouse over one: what it holds)");
    item(modern, MI_MAPMARKS_OPENED, L"Map: also mark the ones already &emptied or done (greyed)");
    item(modern, MI_MAPMONSTERS, L"Map: show the m&onsters walking nearby (red dots)");
    item(modern, MI_INTERIOR_MAPS, L"Map: a plan of the &rooms inside buildings (drawn from their walls)");
    sep(modern);
    Menu st = sub(modern, L"&Start (takes effect next launch)");
    item(st, MI_SKIP0, L"&Logos and opening movie, as on a DS");
    item(st, MI_SKIP0 + 1, L"Skip the &logos");
    item(st, MI_SKIP0 + 2, L"Skip the logos and the opening &movie");
    sep(modern);
    Menu bsp = sub(modern, L"&Battle speed");
    item(bsp, MI_BSPEED1, L"&Normal");
    item(bsp, MI_BSPEED1 + 1, L"&2x");
    item(bsp, MI_BSPEED1 + 2, L"&3x");
    item(modern, MI_AUTOSAVE, L"Sa&ve by itself every 10 minutes (on the field, as at a church)");
    sep(modern);
    Menu farm = sub(modern, L"Farm &gold by itself");
    item(farm, MI_FARM, L"&Farm here now\tCtrl+G");
    item(farm, MI_FARM_TARGET, L"Gold &target...");
    item(farm, MI_FARM_FAST, L"Fast &forward while farming");
    sep(farm);
    item(farm, MI_FARM_HELP, L"&How does it work?");
    sep(modern);
    item(modern, MI_MODERN_HELP, L"&What is this?");
    Menu cheat = sub(0, L"&Cheats");                  /* Action Replay codes for this ROM's version (cheats.c) */
    label(cheat, cheats_european() ? L"Codes for the European version" : L"Codes for the USA version");
    sep(cheat);
    for (int i = 0; i < cheats_count(); i++) item8(cheat, MI_CHEAT0 + i, cheats_name(i));
    sep(cheat);
    item(cheat, MI_CHEATS_HELP, L"&About these cheats");
    Menu lang = sub(0, L"&Language");
    item(lang, MI_LANG1, L"&English");
    item(lang, MI_LANG1 + 1, L"&Fran\u00e7ais");
    item(lang, MI_LANG1 + 2, L"&Deutsch");
    item(lang, MI_LANG1 + 3, L"&Italiano");
    item(lang, MI_LANG1 + 4, L"E&spa\u00f1ol");
    sep(lang);
    Menu ul = sub(lang, L"&Menus and messages of this window");
    item(ul, MI_UILANG0, L"The &game's language");
    sep(ul);
    item(ul, MI_UILANG0 + 1, L"&English");
    item(ul, MI_UILANG0 + 2, L"&Fran\u00e7ais");
    item(ul, MI_UILANG0 + 3, L"&Deutsch");
    item(ul, MI_UILANG0 + 4, L"&Italiano");
    item(ul, MI_UILANG0 + 5, L"E&spa\u00f1ol");
    Menu help = sub(0, L"&Help");
    item(help, MI_CONTROLS, L"&Controls");
    item(help, MI_UPDATE, L"Check for &updates...");
    item(help, MI_UPDATE_AUTO, L"Check for updates at &start");
    sep(help);
    item(help, MI_SEND_REPORT, L"&Send a bug report...");
    item(help, MI_ABOUT, L"&About");
}

int menu_nodes(const MenuNode** nodes) { *nodes = g_node; return g_nnode; }
int menu_checked(int id) { return id > 0 && id < MENU_MAX_ID && g_checked[id]; }
int menu_enabled(int id) { return !(id > 0 && id < MENU_MAX_ID && g_disabled[id]); }
static void note_check(int id, int on) { if (id > 0 && id < MENU_MAX_ID) g_checked[id] = (unsigned char)(on != 0); }
static void note_enable(int id, int on) { if (id > 0 && id < MENU_MAX_ID) g_disabled[id] = (unsigned char)(on == 0); }
static void note_text(int id, const char* text) {
    for (int i = 1; i < g_nnode; i++) if (g_node[i].id == id && g_node[i].kind == MENU_ITEM) snprintf(g_node[i].text, sizeof g_node[i].text, "%s", text);
}

#ifdef _WIN32
#include "SDL_syswm.h"
#include <string.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>

static HWND g_hwnd;
/* a text in the player's language (uitext.h); valid until 15 more calls */
static const wchar_t* TW(const wchar_t* en) {
    static char u8[8192];
    static wchar_t ring[16][4096];
    static int next;
    if (!WideCharToMultiByte(CP_UTF8, 0, en, -1, u8, sizeof u8, NULL, NULL)) return en;
    const char* tr = ui_tr(u8);
    if (tr == u8) return en;
    wchar_t* w = ring[next++ & 15];
    if (!MultiByteToWideChar(CP_UTF8, 0, tr, -1, w, 4096)) return en;
    return w;
}


void menu_init(SDL_Window* win, const char* const* shell_names, int n_shells, int all) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    HMENU old = g_bar;                                /* built again: the language changed */
    g_bar = NULL;
    if (SDL_GetWindowWMInfo(win, &info) && info.subsystem == SDL_SYSWM_WINDOWS) { g_hwnd = info.info.win.window; g_bar = CreateMenu(); }
    build(all, shell_names, n_shells);                /* (the in-game menu needs it even without a bar) */
    if (!g_bar) return;

    SetMenu(g_hwnd, g_bar);
    if (old) DestroyMenu(old);
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
}

void menu_show(int on) {
    if (g_hwnd) SetMenu(g_hwnd, on ? g_bar : NULL);
}

/* The second window (two windows): a bar with one command, fullscreen. */
static HWND g_hwnd2;
static HMENU g_bar2;
void menu_init_second(SDL_Window* win) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    g_hwnd2 = NULL;
    if (!win || !SDL_GetWindowWMInfo(win, &info) || info.subsystem != SDL_SYSWM_WINDOWS) return;
    g_hwnd2 = info.info.win.window;
    if (!g_bar2) { g_bar2 = CreateMenu(); AppendMenuW(g_bar2, MF_STRING, (UINT_PTR)MI_FULL2, TW(L"&Fullscreen (F11)")); }
    SetMenu(g_hwnd2, g_bar2);
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
}
void menu_show_second(int on) {
    if (g_hwnd2 && IsWindow(g_hwnd2)) SetMenu(g_hwnd2, on ? g_bar2 : NULL);
}

void menu_check(int id, int on) {
    note_check(id, on);
    if (g_bar) CheckMenuItem(g_bar, (UINT)id, MF_BYCOMMAND | (on ? MF_CHECKED : MF_UNCHECKED));
}

void menu_enable(int id, int on) {
    note_enable(id, on);
    if (g_bar) EnableMenuItem(g_bar, (UINT)id, MF_BYCOMMAND | (on ? MF_ENABLED : MF_GRAYED));
}

void menu_set_text(int id, const char* text) {
    note_text(id, text);
    wchar_t w[200];
    if (!g_bar || !MultiByteToWideChar(CP_UTF8, 0, text, -1, w, 200)) return;
    MENUITEMINFOW mi = { sizeof mi };
    mi.fMask = MIIM_STRING; mi.dwTypeData = w;
    SetMenuItemInfoW(g_bar, (UINT)id, FALSE, &mi);
}

int menu_command(const SDL_Event* e) {
    if (e->type != SDL_SYSWMEVENT || (!g_bar && !g_bar2)) return 0;
    const SDL_SysWMmsg* m = e->syswm.msg;
    if (m->subsystem != SDL_SYSWM_WINDOWS || m->msg.win.msg != WM_COMMAND || HIWORD(m->msg.win.wParam) != 0) return 0;
    return LOWORD(m->msg.win.wParam);
}

/* The monitor's EDID (in the registry under its device id) holds its
 * physical size: the first detailed timing block in mm, else the basic
 * size in cm. Divided by the monitor's width in pixels. */
float display_mm_per_pixel(SDL_Window* win) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(win, &info) || info.subsystem != SDL_SYSWM_WINDOWS) return 0;
    HMONITOR mon = MonitorFromWindow(info.info.win.window, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW mi; mi.cbSize = sizeof mi;
    if (!GetMonitorInfoW(mon, (MONITORINFO*)&mi)) return 0;
    DEVMODEW dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    if (!EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm) || !dm.dmPelsWidth) return 0;
    int px_w = (int)dm.dmPelsWidth, px_h = (int)dm.dmPelsHeight;
    DISPLAY_DEVICEW dd; dd.cb = sizeof dd;
    for (DWORD k = 0; EnumDisplayDevicesW(mi.szDevice, k, &dd, EDD_GET_DEVICE_INTERFACE_NAME); k++) {
        if (!(dd.StateFlags & DISPLAY_DEVICE_ACTIVE)) continue;
        /* \\?\DISPLAY#<model>#<instance>#{guid} -> SYSTEM\CurrentControlSet\Enum\DISPLAY\<model>\<instance> */
        wchar_t id[256]; wcsncpy(id, dd.DeviceID, 255); id[255] = 0;
        wchar_t* parts[4] = { 0 }; int n = 0;
        for (wchar_t *ctx = NULL, *t = wcstok(id, L"#", &ctx); t && n < 4; t = wcstok(NULL, L"#", &ctx)) parts[n++] = t;
        if (n < 3) continue;
        wchar_t key[512];
        _snwprintf(key, 511, L"SYSTEM\\CurrentControlSet\\Enum\\DISPLAY\\%ls\\%ls\\Device Parameters", parts[1], parts[2]);
        key[511] = 0;
        BYTE edid[256]; DWORD len = sizeof edid;
        if (RegGetValueW(HKEY_LOCAL_MACHINE, key, L"EDID", RRF_RT_REG_BINARY, NULL, edid, &len) != ERROR_SUCCESS || len < 128) continue;
        if (edid[0] != 0 || edid[1] != 0xFF || edid[7] != 0) continue;
        int w_mm = 0, h_mm = 0;
        if (edid[54] | edid[55]) { w_mm = edid[66] | ((edid[68] & 0xF0) << 4); h_mm = edid[67] | ((edid[68] & 0x0F) << 8); }
        if (w_mm < 100 || h_mm < 50) { w_mm = edid[21] * 10; h_mm = edid[22] * 10; }    /* cm */
        if (w_mm < 100 || h_mm < 50) continue;
        if ((px_w < px_h) != (w_mm < h_mm)) { int t = w_mm; w_mm = h_mm; h_mm = t; } /* rotated monitor */
        float mm = (float)w_mm / (float)px_w;
        if (mm > 0.05f && mm < 1.5f) return mm;
    }
    return 0;
}

/* Buttons sized to their words (a translation can be longer than the English):
 * laid out from the first one's left edge, or (right) so that the last one
 * keeps its right edge, with the gaps they had. */
static void fit_buttons(HWND h, const int* ids, int n, int right) {
    if (n < 1 || n > 8) return;
    HDC dc = GetDC(h);
    HGDIOBJ old = SelectObject(dc, (HFONT)SendMessageW(h, WM_GETFONT, 0, 0));
    RECT pad = { 6, 0, 0, 0 }; MapDialogRect(h, &pad);
    RECT r[8]; int w[8], total = 0;
    for (int i = 0; i < n; i++) {
        HWND b = GetDlgItem(h, ids[i]);
        GetWindowRect(b, &r[i]); MapWindowPoints(NULL, h, (POINT*)&r[i], 2);
        wchar_t t[200]; SIZE sz = { 0, 0 };
        int len = GetWindowTextW(b, t, 200);
        GetTextExtentPoint32W(dc, t, len, &sz);
        w[i] = r[i].right - r[i].left;
        if (w[i] < sz.cx + 2 * pad.left) w[i] = sz.cx + 2 * pad.left;
        total += w[i];
    }
    SelectObject(dc, old);
    ReleaseDC(h, dc);
    int gap = n > 1 ? r[1].left - r[0].right : 0;
    total += gap * (n - 1);
    int x = right ? r[n - 1].right - total : r[0].left;
    for (int i = 0; i < n; i++) {
        SetWindowPos(GetDlgItem(h, ids[i]), NULL, x, r[i].top, w[i], r[i].bottom - r[i].top, SWP_NOZORDER);
        x += w[i] + gap;
    }
}

/* ---------------------------------------------------------------- text box (bug descriptions)
 * A dialog built in memory: a prompt, a large multi-line edit box, and
 * Save / Skip buttons. */
#define ID_TEXT 1001
static wchar_t* g_text_out;

static INT_PTR CALLBACK text_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG: {
        static const int b[] = { IDOK, IDCANCEL };
        fit_buttons(h, b, 2, 1);
        SendDlgItemMessageW(h, ID_TEXT, EM_SETLIMITTEXT, 1 << 20, 0);
        SetFocus(GetDlgItem(h, ID_TEXT));
        return FALSE;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            int n = GetWindowTextLengthW(GetDlgItem(h, ID_TEXT));
            g_text_out = calloc((size_t)n + 1, sizeof(wchar_t));
            if (g_text_out) GetDlgItemTextW(h, ID_TEXT, g_text_out, n + 1);
            EndDialog(h, 1);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) { EndDialog(h, 0); return TRUE; }
        break;
    }
    return FALSE;
}

static WORD* align4(WORD* p) { return (WORD*)(((ULONG_PTR)p + 3) & ~(ULONG_PTR)3); }
static WORD* put_str(WORD* p, const wchar_t* s) { do { *p++ = (WORD)*s; } while (*s++); return p; }
static WORD* put_item(WORD* p, DWORD style, short x, short y, short cx, short cy, WORD id, WORD cls, const wchar_t* text) {
    p = align4(p);
    DLGITEMTEMPLATE* it = (DLGITEMTEMPLATE*)p;
    it->style = style | WS_CHILD | WS_VISIBLE; it->dwExtendedStyle = 0;
    it->x = x; it->y = y; it->cx = cx; it->cy = cy; it->id = id;
    p = (WORD*)(it + 1);
    *p++ = 0xFFFF; *p++ = cls;                                   /* 0x80 button, 0x81 edit, 0x82 static */
    p = put_str(p, text);
    *p++ = 0;                                                    /* no creation data */
    return p;
}


/* ---------------------------------------------------------------- a message with buttons that fit */
#define CB_TEXT 2000
#define CB_ICON 2001
#define CB_CHECK 2002
#define CB_BUTTON 3000
typedef struct { int n, def, cancel, has_check, checked; } ChoiceInfo;

static INT_PTR CALLBACK choice_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    ChoiceInfo* ci = (ChoiceInfo*)GetWindowLongPtrW(h, DWLP_USER);
    switch (msg) {
    case WM_INITDIALOG: {
        ci = (ChoiceInfo*)lp;
        SetWindowLongPtrW(h, DWLP_USER, (LONG_PTR)ci);
        SendDlgItemMessageW(h, CB_ICON, STM_SETICON, (WPARAM)LoadIconW(NULL, (LPCWSTR)IDI_INFORMATION), 0);
        if (ci->has_check) CheckDlgButton(h, CB_CHECK, ci->checked ? BST_CHECKED : BST_UNCHECKED);
        /* sizes in dialog units -> pixels for this font and monitor */
        RECT u = { 7, 7, 280, 14 }; MapDialogRect(h, &u);
        int margin = u.left, textw = u.right, btnh = u.bottom;
        RECT g = { 4, 50, 0, 0 }; MapDialogRect(h, &g);
        int gap = g.left, minbtn = g.top, icon = GetSystemMetrics(SM_CXICON);
        HDC dc = GetDC(h);
        HFONT f = (HFONT)SendMessageW(h, WM_GETFONT, 0, 0);
        HGDIOBJ old = SelectObject(dc, f);
        wchar_t t[4096];
        GetDlgItemTextW(h, CB_TEXT, t, 4096);
        RECT tr = { 0, 0, textw, 0 };
        DrawTextW(dc, t, -1, &tr, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        int bw[8], total = 0;
        for (int i = 0; i < ci->n; i++) {
            wchar_t b[200]; SIZE sz;
            GetDlgItemTextW(h, CB_BUTTON + i, b, 200);
            GetTextExtentPoint32W(dc, b, (int)wcslen(b), &sz);
            bw[i] = sz.cx + 2 * margin + gap; if (bw[i] < minbtn) bw[i] = minbtn;
            total += bw[i] + (i ? gap : 0);
        }
        int checkh = 0, checkw = 0;
        if (ci->has_check) {
            wchar_t c[300]; SIZE sz; GetDlgItemTextW(h, CB_CHECK, c, 300);
            GetTextExtentPoint32W(dc, c, (int)wcslen(c), &sz);
            checkw = sz.cx + GetSystemMetrics(SM_CXMENUCHECK) + 3 * gap; checkh = btnh;
        }
        SelectObject(dc, old);
        ReleaseDC(h, dc);
        int tx = margin + icon + 2 * gap, th = tr.bottom;
        int cw = tx + tr.right + margin;
        if (cw < total + 2 * margin) cw = total + 2 * margin;
        if (ci->has_check && cw < tx + checkw + margin) cw = tx + checkw + margin;
        int y = margin;
        SetWindowPos(GetDlgItem(h, CB_ICON), NULL, margin, y, icon, icon, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(h, CB_TEXT), NULL, tx, y, cw - tx - margin, th, SWP_NOZORDER);
        y += (th > icon ? th : icon) + margin;
        if (ci->has_check) { SetWindowPos(GetDlgItem(h, CB_CHECK), NULL, tx, y, cw - tx - margin, checkh, SWP_NOZORDER); y += checkh + margin; }
        int bx = cw - margin - total;
        for (int i = 0; i < ci->n; i++) { SetWindowPos(GetDlgItem(h, CB_BUTTON + i), NULL, bx, y, bw[i], btnh, SWP_NOZORDER); bx += bw[i] + gap; }
        int ch = y + btnh + margin;
        RECT wr = { 0, 0, cw, ch };
        AdjustWindowRectEx(&wr, (DWORD)GetWindowLongW(h, GWL_STYLE), FALSE, (DWORD)GetWindowLongW(h, GWL_EXSTYLE));
        HWND parent = GetParent(h); RECT pr;
        if (!parent || !GetWindowRect(parent, &pr)) SystemParametersInfoW(SPI_GETWORKAREA, 0, &pr, 0);
        int w = wr.right - wr.left, hh = wr.bottom - wr.top;
        SetWindowPos(h, NULL, pr.left + (pr.right - pr.left - w) / 2, pr.top + (pr.bottom - pr.top - hh) / 2, w, hh, SWP_NOZORDER);
        SetFocus(GetDlgItem(h, CB_BUTTON + ci->def));
        return FALSE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id >= CB_BUTTON && id < CB_BUTTON + ci->n) {
            if (ci->has_check) ci->checked = IsDlgButtonChecked(h, CB_CHECK) == BST_CHECKED;
            EndDialog(h, id - CB_BUTTON); return TRUE;
        }
        if (id == IDCANCEL) { if (ci->has_check) ci->checked = IsDlgButtonChecked(h, CB_CHECK) == BST_CHECKED; EndDialog(h, ci->cancel); return TRUE; }
        if (id == IDOK) { EndDialog(h, ci->def); return TRUE; }
        break;
    }
    }
    return FALSE;
}

int choice_box(SDL_Window* win, const char* title, const char* text, const char* const* buttons, int n, int def, int cancel,
               const char* check, int* checked) {
    if (n < 1 || n > 8) return -1;
    title = T(title); text = T(text); if (check) check = T(check);     /* in the player's language */
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    HWND parent = NULL;
    if (win && SDL_GetWindowWMInfo(win, &info) && info.subsystem == SDL_SYSWM_WINDOWS) parent = info.info.win.window;
    static WORD buf[16384];
    memset(buf, 0, sizeof buf);
    DLGTEMPLATE* d = (DLGTEMPLATE*)align4(buf);
    d->style = DS_MODALFRAME | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    d->cdit = (WORD)(2 + n + (check ? 1 : 0)); d->cx = 200; d->cy = 100;
    WORD* p = (WORD*)(d + 1);
    *p++ = 0; *p++ = 0;
    static wchar_t w[8192];
    MultiByteToWideChar(CP_UTF8, 0, title, -1, w, 8192);
    p = put_str(p, w);
    *p++ = 9; p = put_str(p, L"Segoe UI");
    p = put_item(p, SS_ICON, 0, 0, 0, 0, CB_ICON, 0x82, L"");
    /* the text: \n -> \r\n for the static control */
    static wchar_t t[8192]; int k = 0;
    MultiByteToWideChar(CP_UTF8, 0, text, -1, w, 8192);
    for (int i = 0; w[i] && k < 8190; i++) { if (w[i] == L'\n') t[k++] = L'\r'; t[k++] = w[i]; }
    t[k] = 0;
    p = put_item(p, SS_NOPREFIX, 0, 0, 10, 10, CB_TEXT, 0x82, t);
    if (check) { MultiByteToWideChar(CP_UTF8, 0, check, -1, w, 8192); p = put_item(p, WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 10, 10, CB_CHECK, 0x80, w); }
    for (int i = 0; i < n; i++) {
        MultiByteToWideChar(CP_UTF8, 0, T(buttons[i]), -1, w, 8192);
        p = put_item(p, WS_TABSTOP | (i == def ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON), 0, 0, 10, 10, (WORD)(CB_BUTTON + i), 0x80, w);
    }
    ChoiceInfo ci = { n, def, cancel, check != NULL, checked ? *checked : 0 };
    INT_PTR r = DialogBoxIndirectParamW(GetModuleHandleW(NULL), d, parent, choice_proc, (LPARAM)&ci);
    if (r < 0) return -1;
    if (checked) *checked = ci.checked;
    return (int)r;
}

char* ask_text(SDL_Window* win, const char* title, const char* prompt) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(win, &info) || info.subsystem != SDL_SYSWM_WINDOWS) return NULL;
    wchar_t wtitle[200], wprompt[1000];
    MultiByteToWideChar(CP_UTF8, 0, T(title), -1, wtitle, 200);
    MultiByteToWideChar(CP_UTF8, 0, T(prompt), -1, wprompt, 1000);
    static WORD buf[4096];
    memset(buf, 0, sizeof buf);
    DLGTEMPLATE* d = (DLGTEMPLATE*)align4(buf);
    d->style = DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    d->cdit = 4; d->x = 0; d->y = 0; d->cx = 340; d->cy = 230;
    WORD* p = (WORD*)(d + 1);
    *p++ = 0; *p++ = 0;                                          /* no menu, default class */
    p = put_str(p, wtitle);
    *p++ = 9; p = put_str(p, L"Segoe UI");                       /* font */
    p = put_item(p, 0, 7, 7, 326, 34, 0xFFFF, 0x82, wprompt);
    p = put_item(p, WS_BORDER | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 7, 44, 326, 158, ID_TEXT, 0x81, L"");
    p = put_item(p, WS_TABSTOP | BS_DEFPUSHBUTTON, 213, 208, 60, 15, IDOK, 0x80, TW(L"Save"));
    p = put_item(p, WS_TABSTOP | BS_PUSHBUTTON, 277, 208, 56, 15, IDCANCEL, 0x80, TW(L"Skip"));
    g_text_out = NULL;
    INT_PTR r = DialogBoxIndirectParamW(GetModuleHandleW(NULL), d, info.info.win.window, text_proc, 0);
    if (r != 1 || !g_text_out) { free(g_text_out); g_text_out = NULL; return NULL; }
    int n = WideCharToMultiByte(CP_UTF8, 0, g_text_out, -1, NULL, 0, NULL, NULL);
    char* out = malloc((size_t)(n > 0 ? n : 1));
    if (out) { out[0] = 0; if (n > 0) WideCharToMultiByte(CP_UTF8, 0, g_text_out, -1, out, n, NULL, NULL); }
    free(g_text_out); g_text_out = NULL;
    return out;
}


/* ---------------------------------------------------------------- the session window (menu.h) */
enum { ID_ADDR = 1101, ID_COPY, ID_HINT, ID_LIST, ID_STATUS, ID_START, ID_HEAD };
static HWND g_ses;
static int g_ses_action;
static SessionWindow g_ses_shown;

static void set_text(HWND h, int id, const char* utf8) {
    wchar_t w[1024];
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, 1024);
    w[1023] = 0;
    SetDlgItemTextW(h, id, w);
}

static void copy_address(HWND h) {
    int n = GetWindowTextLengthW(GetDlgItem(h, ID_ADDR));
    if (n <= 0 || !OpenClipboard(h)) return;
    EmptyClipboard();
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, ((size_t)n + 1) * sizeof(wchar_t));
    if (mem) {
        wchar_t* p = GlobalLock(mem);
        GetDlgItemTextW(h, ID_ADDR, p, n + 1);
        GlobalUnlock(mem);
        if (!SetClipboardData(CF_UNICODETEXT, mem)) GlobalFree(mem);
    }
    CloseClipboard();
    SetDlgItemTextW(h, ID_COPY, L"Copied!");
    SetTimer(h, 1, 2000, NULL);
}

static INT_PTR CALLBACK ses_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG: return TRUE;
    case WM_TIMER: KillTimer(h, 1); SetDlgItemTextW(h, ID_COPY, L"Copy"); return TRUE;
    case WM_CLOSE: g_ses_action = 2; return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_COPY: copy_address(h); return TRUE;
        case ID_START: if (IsWindowEnabled(GetDlgItem(h, ID_START))) g_ses_action = 1; return TRUE;
        case IDCANCEL: g_ses_action = 2; return TRUE;
        }
        break;
    }
    return FALSE;
}

int session_window_open(SDL_Window* win) {
    if (g_ses) return 1;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(win, &info) || info.subsystem != SDL_SYSWM_WINDOWS) return 0;
    static WORD buf[4096];
    memset(buf, 0, sizeof buf);
    DLGTEMPLATE* d = (DLGTEMPLATE*)align4(buf);
    d->style = DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    d->cdit = 9; d->x = 0; d->y = 0; d->cx = 270; d->cy = 278;
    WORD* p = (WORD*)(d + 1);
    *p++ = 0; *p++ = 0;
    p = put_str(p, L"Play with friends far away");
    *p++ = 10; p = put_str(p, L"Segoe UI");
    p = put_item(p, 0, 8, 8, 254, 10, ID_HEAD, 0x82, L"");
    p = put_item(p, WS_BORDER | ES_READONLY | ES_AUTOHSCROLL | ES_CENTER, 8, 21, 196, 16, ID_ADDR, 0x81, L"");
    p = put_item(p, WS_TABSTOP | BS_PUSHBUTTON, 210, 21, 52, 16, ID_COPY, 0x80, L"Copy");
    p = put_item(p, 0, 8, 42, 254, 92, ID_HINT, 0x82, L"");
    p = put_item(p, 0, 8, 138, 254, 10, 0xFFFF, 0x82, L"Players:");
    p = put_item(p, WS_BORDER | WS_VSCROLL | LBS_NOSEL, 8, 150, 254, 54, ID_LIST, 0x83, L"");
    p = put_item(p, 0, 8, 210, 254, 34, ID_STATUS, 0x82, L"");
    p = put_item(p, WS_TABSTOP | BS_DEFPUSHBUTTON, 118, 254, 84, 17, ID_START, 0x80, L"Start the game");
    p = put_item(p, WS_TABSTOP | BS_PUSHBUTTON, 208, 254, 54, 17, IDCANCEL, 0x80, L"Cancel");
    g_ses = CreateDialogIndirectParamW(GetModuleHandleW(NULL), d, info.info.win.window, ses_proc, 0);
    if (!g_ses) return 0;
    /* a bigger, bolder address */
    HFONT f = CreateFontW(-22, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    if (f) SendDlgItemMessageW(g_ses, ID_ADDR, WM_SETFONT, (WPARAM)f, TRUE);
    memset(&g_ses_shown, 0, sizeof g_ses_shown);
    g_ses_shown.can_start = -1;
    g_ses_action = 0;
    return 1;
}

void session_window_update(const SessionWindow* w) {
    if (!g_ses) return;
    SessionWindow* o = &g_ses_shown;
    if (strcmp(o->heading, w->heading)) set_text(g_ses, ID_HEAD, w->heading);
    if (strcmp(o->address, w->address)) set_text(g_ses, ID_ADDR, w->address);
    if (strcmp(o->hint, w->hint)) set_text(g_ses, ID_HINT, w->hint);
    if (strcmp(o->status, w->status)) set_text(g_ses, ID_STATUS, w->status);
    int list_changed = o->n_players != w->n_players;
    for (int i = 0; i < w->n_players && !list_changed; i++) list_changed = strcmp(o->players[i], w->players[i]) != 0;
    if (list_changed) {
        HWND l = GetDlgItem(g_ses, ID_LIST);
        SendMessageW(l, LB_RESETCONTENT, 0, 0);
        for (int i = 0; i < w->n_players; i++) {
            wchar_t t[128]; MultiByteToWideChar(CP_UTF8, 0, w->players[i], -1, t, 128); t[127] = 0;
            SendMessageW(l, LB_ADDSTRING, 0, (LPARAM)t);
        }
    }
    int start = w->host && w->can_start && !w->failed;
    if (o->can_start != start || o->host != w->host || o->failed != w->failed) {
        EnableWindow(GetDlgItem(g_ses, ID_START), start);
        ShowWindow(GetDlgItem(g_ses, ID_START), w->host && !w->failed ? SW_SHOW : SW_HIDE);
        SetDlgItemTextW(g_ses, IDCANCEL, w->failed ? L"Back" : L"Cancel");
        EnableWindow(GetDlgItem(g_ses, ID_COPY), w->address[0] != 0);
    }
    *o = *w;
    o->can_start = start;
}


/* ---------------------------------------------------------------- volume (Game > Volume...) */
#define ID_VOL_BAR 4000
#define ID_VOL_TEXT 4001
#define ID_VOL_MUTE 4002
static HWND g_vol;
static int* g_vol_val; static int* g_vol_mute;
static void vol_show(HWND h) {
    wchar_t t[32]; swprintf(t, 32, L"%d%%", *g_vol_val); SetDlgItemTextW(h, ID_VOL_TEXT, t);
    CheckDlgButton(h, ID_VOL_MUTE, *g_vol_mute ? BST_CHECKED : BST_UNCHECKED);
}
static INT_PTR CALLBACK vol_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        SCROLLINFO si = { sizeof si, SIF_RANGE | SIF_POS | SIF_PAGE, 0, 100 + 9, 10, *g_vol_val, 0 };
        SetScrollInfo(GetDlgItem(h, ID_VOL_BAR), SB_CTL, &si, TRUE);
        vol_show(h);
        return TRUE;
    }
    case WM_HSCROLL: {
        HWND bar = GetDlgItem(h, ID_VOL_BAR);
        int v = *g_vol_val;
        switch (LOWORD(wp)) {
        case SB_LINELEFT: v -= 1; break; case SB_LINERIGHT: v += 1; break;
        case SB_PAGELEFT: v -= 10; break; case SB_PAGERIGHT: v += 10; break;
        case SB_LEFT: v = 0; break; case SB_RIGHT: v = 100; break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: v = HIWORD(wp); break;
        default: return TRUE;
        }
        v = v < 0 ? 0 : v > 100 ? 100 : v;
        *g_vol_val = v; if (v > 0) *g_vol_mute = 0;
        SetScrollPos(bar, SB_CTL, v, TRUE);
        vol_show(h);
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == ID_VOL_MUTE) { *g_vol_mute = IsDlgButtonChecked(h, ID_VOL_MUTE) == BST_CHECKED; return TRUE; }
        if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) { DestroyWindow(h); g_vol = NULL; return TRUE; }
        break;
    case WM_CLOSE: DestroyWindow(h); g_vol = NULL; return TRUE;
    }
    (void)lp;
    return FALSE;
}
void volume_window_open(SDL_Window* win, int* volume, int* mute) {
    g_vol_val = volume; g_vol_mute = mute;
    if (g_vol) { vol_show(g_vol); SetForegroundWindow(g_vol); return; }
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(win, &info) || info.subsystem != SDL_SYSWM_WINDOWS) return;
    static WORD buf[1024];
    memset(buf, 0, sizeof buf);
    DLGTEMPLATE* d = (DLGTEMPLATE*)align4(buf);
    d->style = DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    d->cdit = 4; d->cx = 220; d->cy = 70;
    WORD* p = (WORD*)(d + 1);
    *p++ = 0; *p++ = 0;
    p = put_str(p, TW(L"Volume"));
    *p++ = 10; p = put_str(p, L"Segoe UI");
    p = put_item(p, WS_TABSTOP | SBS_HORZ, 8, 10, 168, 12, ID_VOL_BAR, 0x84, L"");
    p = put_item(p, 0, 182, 11, 32, 10, ID_VOL_TEXT, 0x82, L"");
    p = put_item(p, WS_TABSTOP | BS_AUTOCHECKBOX, 8, 30, 120, 12, ID_VOL_MUTE, 0x80, TW(L"Mute"));
    p = put_item(p, WS_TABSTOP | BS_DEFPUSHBUTTON, 160, 48, 54, 15, IDOK, 0x80, TW(L"Close"));
    g_vol = CreateDialogIndirectParamW(GetModuleHandleW(NULL), d, info.info.win.window, vol_proc, 0);
}
void volume_window_refresh(void) { if (g_vol) { SetScrollPos(GetDlgItem(g_vol, ID_VOL_BAR), SB_CTL, *g_vol_val, TRUE); vol_show(g_vol); } }

/* ---------------------------------------------------------------- controls (Game > Controls...) */
#define ID_CTL_LIST 4100
#define ID_CTL_K1 4101
#define ID_CTL_K2 4102
#define ID_CTL_PAD 4103
#define ID_CTL_CLEAR 4104
#define ID_CTL_DEFAULTS 4105
#define ID_CTL_STATUS 4106
static HWND g_ctlw;
static int g_ctl_req = -1, g_ctl_req_what;
static void ctl_status(HWND h) {                            /* which controller is connected (checked every second) */
    char st[160]; wchar_t ws[160], cur[160];
    if (g_ctl_pad_label[0]) snprintf(st, sizeof st, T("Controller: %s"), g_ctl_pad_label);
    else snprintf(st, sizeof st, "%s", T("No controller connected"));
    MultiByteToWideChar(CP_UTF8, 0, st, -1, ws, 160); ws[159] = 0;
    GetDlgItemTextW(h, ID_CTL_STATUS, cur, 160);
    if (wcscmp(cur, ws)) SetDlgItemTextW(h, ID_CTL_STATUS, ws);
}
static int g_ctl_style_shown = -1;
static void ctl_fill(HWND h) {
    HWND l = GetDlgItem(h, ID_CTL_LIST);
    int sel = (int)SendMessageW(l, LB_GETCURSEL, 0, 0), top = (int)SendMessageW(l, LB_GETTOPINDEX, 0, 0);
    SendMessageW(l, LB_RESETCONTENT, 0, 0);
    g_ctl_style_shown = g_ctl_pad_style;
    for (int a = 0; a < CA_N; a++) {
        char k0[64], k1[64], pd[64], row[300];
        controls_key_name(g_ctl_key[a][0], k0, sizeof k0); controls_key_name(g_ctl_key[a][1], k1, sizeof k1);
        controls_pad_name(g_ctl_pad[a], pd, sizeof pd);
        snprintf(row, sizeof row, "%s\t%s\t%s\t%s", T(g_ctl[a].name), T(k0), T(k1), T(pd));
        wchar_t w[300]; MultiByteToWideChar(CP_UTF8, 0, row, -1, w, 300); w[299] = 0;
        SendMessageW(l, LB_ADDSTRING, 0, (LPARAM)w);
    }
    SendMessageW(l, LB_SETTOPINDEX, (WPARAM)top, 0);
    if (sel >= 0) SendMessageW(l, LB_SETCURSEL, (WPARAM)sel, 0);
    ctl_status(h);
}
static INT_PTR CALLBACK ctl_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG: {
        int tabs[3] = { 172, 228, 284 };                          /* dialog units: action | key | key | controller */
        SendDlgItemMessageW(h, ID_CTL_LIST, LB_SETTABSTOPS, 3, (LPARAM)tabs);
        static const int row1[] = { ID_CTL_K1, ID_CTL_K2, ID_CTL_PAD, ID_CTL_CLEAR }, row2[] = { ID_CTL_DEFAULTS };
        fit_buttons(h, row1, 4, 0);
        fit_buttons(h, row2, 1, 0);
        { RECT d, s; GetWindowRect(GetDlgItem(h, ID_CTL_DEFAULTS), &d); GetWindowRect(GetDlgItem(h, ID_CTL_STATUS), &s);   /* the status after it */
          MapWindowPoints(NULL, h, (POINT*)&d, 2); MapWindowPoints(NULL, h, (POINT*)&s, 2);
          RECT g = { 6, 0, 0, 0 }; MapDialogRect(h, &g);
          if (s.left < d.right + g.left) SetWindowPos(GetDlgItem(h, ID_CTL_STATUS), NULL, d.right + g.left, s.top, s.right - d.right - g.left, s.bottom - s.top, SWP_NOZORDER); }
        ctl_fill(h);
        SendDlgItemMessageW(h, ID_CTL_LIST, LB_SETCURSEL, 0, 0);
        SetTimer(h, 1, 1000, NULL);
        return TRUE;
    }
    case WM_TIMER:                                               /* a controller plugged in or out: its button names */
        if (g_ctl_style_shown != g_ctl_pad_style) ctl_fill(h); else ctl_status(h);
        return TRUE;
    case WM_COMMAND: {
        int sel = (int)SendDlgItemMessageW(h, ID_CTL_LIST, LB_GETCURSEL, 0, 0);
        switch (LOWORD(wp)) {
        case ID_CTL_LIST: if (HIWORD(wp) == LBN_DBLCLK && sel >= 0) { g_ctl_req = sel; g_ctl_req_what = 0; } return TRUE;
        case ID_CTL_K1: if (sel >= 0) { g_ctl_req = sel; g_ctl_req_what = 0; } return TRUE;
        case ID_CTL_K2: if (sel >= 0) { g_ctl_req = sel; g_ctl_req_what = 1; } return TRUE;
        case ID_CTL_PAD: if (sel >= 0) { g_ctl_req = sel; g_ctl_req_what = 2; } return TRUE;
        case ID_CTL_CLEAR:
            if (sel >= 0) {
                if (g_ctl_key[sel][1]) g_ctl_key[sel][1] = 0;
                else if (g_ctl_key[sel][0]) g_ctl_key[sel][0] = 0;
                else g_ctl_pad[sel] = CTL_NONE;
                controls_save(); ctl_fill(h);
            }
            return TRUE;
        case ID_CTL_DEFAULTS:
            if (MessageBoxW(h, TW(L"Put every key and button back to how the game came?"), TW(L"Controls"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
                controls_defaults(); controls_save(); ctl_fill(h);
            }
            return TRUE;
        case IDOK: case IDCANCEL: DestroyWindow(h); g_ctlw = NULL; return TRUE;
        }
        break;
    }
    case WM_CLOSE: DestroyWindow(h); g_ctlw = NULL; return TRUE;
    }
    return FALSE;
}
void controls_window_open(SDL_Window* win) {
    if (g_ctlw) { SetForegroundWindow(g_ctlw); return; }
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(win, &info) || info.subsystem != SDL_SYSWM_WINDOWS) return;
    static WORD buf[4096];
    memset(buf, 0, sizeof buf);
    DLGTEMPLATE* d = (DLGTEMPLATE*)align4(buf);
    d->style = DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    d->cdit = 13; d->cx = 400; d->cy = 290;
    WORD* p = (WORD*)(d + 1);
    *p++ = 0; *p++ = 0;
    p = put_str(p, TW(L"Controls"));
    *p++ = 9; p = put_str(p, L"Segoe UI");
    p = put_item(p, 0, 8, 6, 384, 18, 0xFFFF, 0x82, TW(L"Pick an action, then change one of its keys or its controller button. A key or button that "
                 "another action has moves over to it (they swap). Esc and Alt+Enter, and the sticks, stay as they are."));
    p = put_item(p, 0, 8, 28, 170, 9, 0xFFFF, 0x82, TW(L"Action"));
    p = put_item(p, 0, 180, 28, 56, 9, 0xFFFF, 0x82, TW(L"Key"));                 /* over the list's tab stops */
    p = put_item(p, 0, 236, 28, 56, 9, 0xFFFF, 0x82, TW(L"Second key"));
    p = put_item(p, 0, 292, 28, 100, 9, 0xFFFF, 0x82, TW(L"Controller"));
    p = put_item(p, WS_BORDER | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_USETABSTOPS | LBS_NOINTEGRALHEIGHT, 8, 39, 384, 196, ID_CTL_LIST, 0x83, L"");
    p = put_item(p, WS_TABSTOP | BS_PUSHBUTTON, 8, 241, 72, 15, ID_CTL_K1, 0x80, TW(L"Change key..."));
    p = put_item(p, WS_TABSTOP | BS_PUSHBUTTON, 84, 241, 82, 15, ID_CTL_K2, 0x80, TW(L"Second key..."));
    p = put_item(p, WS_TABSTOP | BS_PUSHBUTTON, 170, 241, 92, 15, ID_CTL_PAD, 0x80, TW(L"Controller button..."));
    p = put_item(p, WS_TABSTOP | BS_PUSHBUTTON, 266, 241, 54, 15, ID_CTL_CLEAR, 0x80, TW(L"Clear"));
    p = put_item(p, WS_TABSTOP | BS_PUSHBUTTON, 8, 266, 100, 15, ID_CTL_DEFAULTS, 0x80, TW(L"Restore defaults"));
    p = put_item(p, SS_LEFTNOWORDWRAP, 114, 269, 218, 9, ID_CTL_STATUS, 0x82, L"");
    p = put_item(p, WS_TABSTOP | BS_DEFPUSHBUTTON, 338, 266, 54, 15, IDOK, 0x80, TW(L"Close"));
    g_ctlw = CreateDialogIndirectParamW(GetModuleHandleW(NULL), d, info.info.win.window, ctl_proc, 0);
}
int controls_window_request(int* action, int* what) {
    if (g_ctl_req < 0) return 0;
    *action = g_ctl_req; *what = g_ctl_req_what; g_ctl_req = -1;
    return 1;
}
void controls_window_refresh(void) { if (g_ctlw) { ctl_fill(g_ctlw); SetForegroundWindow(g_ctlw); } }

int session_window_action(void) { int a = g_ses_action; g_ses_action = 0; return a; }

void session_window_close(void) {
    if (!g_ses) return;
    DestroyWindow(g_ses);
    g_ses = NULL;
}
#else
int choice_box(SDL_Window* win, const char* title, const char* text, const char* const* buttons, int n, int def, int cancel,
               const char* check, int* checked) { (void)win; (void)title; (void)text; (void)buttons; (void)n; (void)def; (void)cancel; (void)check; (void)checked; return -1; }
char* ask_text(SDL_Window* win, const char* title, const char* prompt) { (void)win; (void)title; (void)prompt; return NULL; }
float display_mm_per_pixel(SDL_Window* win) { (void)win; return 0; }
void menu_init(SDL_Window* win, const char* const* shell_names, int n_shells, int all) { (void)win; build(all, shell_names, n_shells); }
void menu_show(int on) { (void)on; }
void menu_init_second(SDL_Window* win) { (void)win; }
void menu_show_second(int on) { (void)on; }
void menu_check(int id, int on) { note_check(id, on); }
void menu_enable(int id, int on) { note_enable(id, on); }
void menu_set_text(int id, const char* text) { note_text(id, text); }
int menu_command(const SDL_Event* e) { (void)e; return 0; }
int session_window_open(SDL_Window* win) { (void)win; return 0; }
void volume_window_open(SDL_Window* win, int* volume, int* mute) { (void)win; (void)volume; (void)mute; }
void volume_window_refresh(void) {}
void controls_window_open(SDL_Window* win) { (void)win; }
int controls_window_request(int* action, int* what) { (void)action; (void)what; return 0; }
void controls_window_refresh(void) {}
void session_window_update(const SessionWindow* w) { (void)w; }
int session_window_action(void) { return 0; }
void session_window_close(void) {}
#endif
