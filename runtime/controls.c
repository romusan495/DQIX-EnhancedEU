/* Key and controller bindings (controls.h). */
#include "controls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DS key bits: A B SELECT START RIGHT LEFT UP DOWN R L X Y */
const ControlAction g_ctl[CA_N] = {
    [CA_A] = { "a", "DS: A", 0 },
    [CA_B] = { "b", "DS: B", 1 },
    [CA_X] = { "x", "DS: X", 10 },
    [CA_Y] = { "y", "DS: Y", 11 },
    [CA_L] = { "l", "DS: L (turn the camera)", 9 },
    [CA_R] = { "r", "DS: R (turn the camera)", 8 },
    [CA_START] = { "start", "DS: START", 3 },
    [CA_SELECT] = { "select", "DS: SELECT", 2 },
    [CA_UP] = { "up", "DS: Up", 6 },
    [CA_DOWN] = { "down", "DS: Down", 7 },
    [CA_LEFT] = { "left", "DS: Left", 5 },
    [CA_RIGHT] = { "right", "DS: Right", 4 },
    [CA_FAST] = { "fast_forward", "Fast forward (hold)", -1 },
    [CA_PAUSE] = { "pause", "Pause", -1 },
    [CA_FULL] = { "fullscreen", "Fullscreen", -1 },
    [CA_MINIMAP] = { "minimap", "Show / hide the map (top right)", -1 },
    [CA_WINSIZE] = { "window_size", "Window size", -1 },
    [CA_RES3D] = { "resolution_3d", "3D resolution", -1 },
    [CA_MODE] = { "classic_enhanced", "Classic / Enhanced", -1 },
    [CA_LCD] = { "dslite_screen", "DS Lite screen", -1 },
    [CA_SHELL] = { "dslite_console", "DS Lite console", -1 },
    [CA_ACTUAL] = { "actual_size", "Actual size", -1 },
    [CA_VOLDOWN] = { "volume_down", "Volume down", -1 },
    [CA_VOLUP] = { "volume_up", "Volume up", -1 },
    [CA_RESET] = { "reset", "Reset the game", -1 },
    [CA_SHOT] = { "screenshot", "Screenshot", -1 },
    [CA_REPORT] = { "bug_report", "Bug report", -1 },
    [CA_MENU] = { "menu", "Options menu (in the game; Esc too)", -1 },
};

int g_ctl_key[CA_N][2];
int g_ctl_pad[CA_N];

void controls_defaults(void) {
    memset(g_ctl_key, 0, sizeof g_ctl_key);
    for (int i = 0; i < CA_N; i++) g_ctl_pad[i] = CTL_NONE;
#define K(a, k0, k1) (g_ctl_key[a][0] = (k0), g_ctl_key[a][1] = (k1))
    K(CA_A, SDL_SCANCODE_X, 0); K(CA_B, SDL_SCANCODE_Z, 0); K(CA_X, SDL_SCANCODE_S, 0); K(CA_Y, SDL_SCANCODE_A, 0);
    K(CA_L, SDL_SCANCODE_Q, 0); K(CA_R, SDL_SCANCODE_W, 0);
    K(CA_START, SDL_SCANCODE_RETURN, 0); K(CA_SELECT, SDL_SCANCODE_RSHIFT, SDL_SCANCODE_BACKSPACE);
    K(CA_UP, SDL_SCANCODE_UP, 0); K(CA_DOWN, SDL_SCANCODE_DOWN, 0); K(CA_LEFT, SDL_SCANCODE_LEFT, 0); K(CA_RIGHT, SDL_SCANCODE_RIGHT, 0);
    K(CA_FAST, SDL_SCANCODE_TAB, 0); K(CA_PAUSE, SDL_SCANCODE_P, SDL_SCANCODE_PAUSE); K(CA_FULL, SDL_SCANCODE_F11, 0); K(CA_MINIMAP, SDL_SCANCODE_M, 0);
    K(CA_WINSIZE, SDL_SCANCODE_F3, 0); K(CA_RES3D, SDL_SCANCODE_F4, 0); K(CA_MODE, SDL_SCANCODE_F5, 0);
    K(CA_LCD, SDL_SCANCODE_F6, 0); K(CA_SHELL, SDL_SCANCODE_F7, 0); K(CA_ACTUAL, SDL_SCANCODE_F8, 0);
    K(CA_VOLDOWN, SDL_SCANCODE_MINUS, 0); K(CA_VOLUP, SDL_SCANCODE_EQUALS, 0);
    K(CA_RESET, SDL_SCANCODE_R | CTL_MOD_CTRL, 0); K(CA_SHOT, SDL_SCANCODE_F12, 0); K(CA_REPORT, SDL_SCANCODE_F10, 0);
#undef K
    /* the controller by position (like a DS): right = A, bottom = B, top = X, left = Y */
    g_ctl_pad[CA_A] = SDL_CONTROLLER_BUTTON_B; g_ctl_pad[CA_B] = SDL_CONTROLLER_BUTTON_A;
    g_ctl_pad[CA_X] = SDL_CONTROLLER_BUTTON_Y; g_ctl_pad[CA_Y] = SDL_CONTROLLER_BUTTON_X;
    g_ctl_pad[CA_L] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER; g_ctl_pad[CA_R] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    g_ctl_pad[CA_START] = SDL_CONTROLLER_BUTTON_START; g_ctl_pad[CA_SELECT] = SDL_CONTROLLER_BUTTON_BACK;
    g_ctl_pad[CA_UP] = SDL_CONTROLLER_BUTTON_DPAD_UP; g_ctl_pad[CA_DOWN] = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    g_ctl_pad[CA_LEFT] = SDL_CONTROLLER_BUTTON_DPAD_LEFT; g_ctl_pad[CA_RIGHT] = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    g_ctl_pad[CA_MINIMAP] = SDL_CONTROLLER_BUTTON_RIGHTSTICK;     /* R3 */
    g_ctl_pad[CA_MENU] = SDL_CONTROLLER_BUTTON_GUIDE;             /* the PS / Xbox / Home button */
}

/* ---------------------------------------------------------------- names */
void controls_key_name(int key, char* out, size_t n) {
    if (!key) { snprintf(out, n, "-"); return; }
    const char* nm = SDL_GetScancodeName((SDL_Scancode)(key & 0xFFFF));
    snprintf(out, n, "%s%s%s%s", key & CTL_MOD_CTRL ? "Ctrl+" : "", key & CTL_MOD_ALT ? "Alt+" : "", key & CTL_MOD_SHIFT ? "Shift+" : "",
             nm && nm[0] ? nm : "?");
}

/* names: generic, then as printed on PlayStation, Xbox and Nintendo controllers
 * (the buttons are by position, so on a Nintendo one the right button is A) */
static const struct { int pad; const char* ini; const char* name; const char* ps; const char* xb; const char* nin; } PADS[] = {
    { SDL_CONTROLLER_BUTTON_B, "b", "Right face button", "Circle", "B", "A" },
    { SDL_CONTROLLER_BUTTON_A, "a", "Bottom face button", "Cross", "A", "B" },
    { SDL_CONTROLLER_BUTTON_Y, "y", "Top face button", "Triangle", "Y", "X" },
    { SDL_CONTROLLER_BUTTON_X, "x", "Left face button", "Square", "X", "Y" },
    { SDL_CONTROLLER_BUTTON_BACK, "back", "Back / Select", "Create / Share", "View", "-" },
    { SDL_CONTROLLER_BUTTON_GUIDE, "guide", "Home / Guide", "PS button", "Xbox button", "Home" },
    { SDL_CONTROLLER_BUTTON_START, "start", "Start", "Options", "Menu", "+" },
    { SDL_CONTROLLER_BUTTON_LEFTSTICK, "leftstick", "Left stick click", "L3", "Left stick click", "Left stick click" },
    { SDL_CONTROLLER_BUTTON_RIGHTSTICK, "rightstick", "Right stick click", "R3", "Right stick click", "Right stick click" },
    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, "leftshoulder", "Left shoulder", "L1", "LB", "L" },
    { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, "rightshoulder", "Right shoulder", "R1", "RB", "R" },
    { SDL_CONTROLLER_BUTTON_DPAD_UP, "dpup", "D-pad up", 0, 0, 0 },
    { SDL_CONTROLLER_BUTTON_DPAD_DOWN, "dpdown", "D-pad down", 0, 0, 0 },
    { SDL_CONTROLLER_BUTTON_DPAD_LEFT, "dpleft", "D-pad left", 0, 0, 0 },
    { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, "dpright", "D-pad right", 0, 0, 0 },
    { SDL_CONTROLLER_BUTTON_MISC1, "misc1", "Share / Capture", "Mute button", "Share", "Capture" },
    { SDL_CONTROLLER_BUTTON_PADDLE1, "paddle1", "Back paddle 1", "Back button 1", 0, 0 },
    { SDL_CONTROLLER_BUTTON_PADDLE2, "paddle2", "Back paddle 2", "Back button 2", 0, 0 },
    { SDL_CONTROLLER_BUTTON_PADDLE3, "paddle3", "Back paddle 3", "Back button 3", 0, 0 },
    { SDL_CONTROLLER_BUTTON_PADDLE4, "paddle4", "Back paddle 4", "Back button 4", 0, 0 },
    { SDL_CONTROLLER_BUTTON_TOUCHPAD, "touchpad", "Touchpad click", "Touchpad click", 0, 0 },
    { CTL_PAD_LT, "lefttrigger", "Left trigger", "L2", "LT", "ZL" },
    { CTL_PAD_RT, "righttrigger", "Right trigger", "R2", "RT", "ZR" },
};
#define N_PADS ((int)(sizeof PADS / sizeof PADS[0]))

int g_ctl_pad_style = CTL_STYLE_GENERIC;
char g_ctl_pad_label[96];

void controls_pad_name(int pad, char* out, size_t n) {
    for (int i = 0; i < N_PADS; i++) if (PADS[i].pad == pad) {
        const char* s = g_ctl_pad_style == CTL_STYLE_PLAYSTATION ? PADS[i].ps : g_ctl_pad_style == CTL_STYLE_XBOX ? PADS[i].xb :
                        g_ctl_pad_style == CTL_STYLE_NINTENDO ? PADS[i].nin : 0;
        snprintf(out, n, "%s", s ? s : PADS[i].name);
        return;
    }
    snprintf(out, n, "-");
}

void controls_pad_detect(SDL_GameController* p) {
    g_ctl_pad_style = CTL_STYLE_GENERIC; g_ctl_pad_label[0] = 0;
    if (!p) return;
    const char* nm = SDL_GameControllerName(p);
    snprintf(g_ctl_pad_label, sizeof g_ctl_pad_label, "%s", nm ? nm : "Controller");
    switch (SDL_GameControllerGetType(p)) {
    case SDL_CONTROLLER_TYPE_PS3: case SDL_CONTROLLER_TYPE_PS4: case SDL_CONTROLLER_TYPE_PS5: g_ctl_pad_style = CTL_STYLE_PLAYSTATION; break;
    case SDL_CONTROLLER_TYPE_XBOX360: case SDL_CONTROLLER_TYPE_XBOXONE: g_ctl_pad_style = CTL_STYLE_XBOX; break;
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: g_ctl_pad_style = CTL_STYLE_NINTENDO; break;
    default: break;
    }
}

/* ---------------------------------------------------------------- state */
int controls_pad_down(SDL_GameController* p, int pad) {
    if (!p || pad == CTL_NONE) return 0;
    if (pad == CTL_PAD_LT) return SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 12000;
    if (pad == CTL_PAD_RT) return SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000;
    return SDL_GameControllerGetButton(p, (SDL_GameControllerButton)pad) != 0;
}
int controls_pad_held(int action, SDL_GameController* p) { return controls_pad_down(p, g_ctl_pad[action]); }

static int mods_of(Uint16 mod) {
    return (mod & KMOD_CTRL ? CTL_MOD_CTRL : 0) | (mod & KMOD_ALT ? CTL_MOD_ALT : 0) | (mod & KMOD_SHIFT ? CTL_MOD_SHIFT : 0);
}

int controls_key_held(int action, const Uint8* ks) {
    int m = mods_of((Uint16)SDL_GetModState());
    for (int s = 0; s < 2; s++) {
        int k = g_ctl_key[action][s];
        if (!k || !ks[k & 0xFFFF]) continue;
        int need = k & ~0xFFFF;
        if (need) { if ((m & need) == need) return 1; }
        else if (!(m & CTL_MOD_ALT) || (k & 0xFFFF) != SDL_SCANCODE_RETURN) return 1;   /* Alt+Enter is fullscreen, not START */
    }
    return 0;
}

uint16_t controls_ds_keys(const Uint8* ks) {
    uint16_t b = 0;
    for (int a = 0; a < CA_N_DS; a++) if (controls_key_held(a, ks)) b |= (uint16_t)(1u << g_ctl[a].ds_bit);
    return b;
}
uint16_t controls_ds_pad(SDL_GameController* p) {
    uint16_t b = 0;
    if (!p) return 0;
    for (int a = 0; a < CA_N_DS; a++) if (controls_pad_held(a, p)) b |= (uint16_t)(1u << g_ctl[a].ds_bit);
    return b;
}

int controls_key_action(SDL_Scancode sc, Uint16 mod) {
    int m = mods_of(mod), best = -1, best_mods = -1;
    for (int a = CA_N_DS; a < CA_N; a++)
        for (int s = 0; s < 2; s++) {
            int k = g_ctl_key[a][s];
            if (!k || (k & 0xFFFF) != (int)sc) continue;
            int need = k & ~0xFFFF;
            if ((m & need) != need) continue;
            if (!need && (m & (CTL_MOD_CTRL | CTL_MOD_ALT))) continue;   /* a plain key doesn't fire with Ctrl or Alt held */
            int nm = __builtin_popcount((unsigned)need);
            if (nm > best_mods) { best = a; best_mods = nm; }      /* Ctrl+R wins over R */
        }
    return best;
}

int controls_key_owner(int key, int* slot) {
    for (int a = 0; a < CA_N; a++) for (int s = 0; s < 2; s++) if (key && g_ctl_key[a][s] == key) { if (slot) *slot = s; return a; }
    return -1;
}
int controls_pad_owner(int pad) {
    for (int a = 0; a < CA_N; a++) if (pad != CTL_NONE && g_ctl_pad[a] == pad) return a;
    return -1;
}
void controls_set_key(int action, int slot, int key) {
    int os, o = controls_key_owner(key, &os);
    if (o == action && os == slot) return;
    int old = g_ctl_key[action][slot];
    if (o >= 0) g_ctl_key[o][os] = old;                        /* the other action gets this one's old key */
    g_ctl_key[action][slot] = key;
    if (o == action) g_ctl_key[action][os] = old;
}
void controls_set_pad(int action, int pad) {
    int o = controls_pad_owner(pad);
    if (o == action) return;
    if (o >= 0) g_ctl_pad[o] = g_ctl_pad[action];
    g_ctl_pad[action] = pad;
}

/* ---------------------------------------------------------------- controls.ini */
static const char* INI = "controls.ini";

static int parse_key(const char* s) {
    while (*s == ' ') s++;
    if (!*s || !strcmp(s, "none")) return 0;
    int mods = 0;
    for (;;) {
        if (!strncmp(s, "Ctrl+", 5)) { mods |= CTL_MOD_CTRL; s += 5; }
        else if (!strncmp(s, "Alt+", 4)) { mods |= CTL_MOD_ALT; s += 4; }
        else if (!strncmp(s, "Shift+", 6) && s[6]) { mods |= CTL_MOD_SHIFT; s += 6; }
        else break;
    }
    SDL_Scancode sc = SDL_GetScancodeFromName(s);
    return sc == SDL_SCANCODE_UNKNOWN ? 0 : (int)sc | mods;
}
static int parse_pad(const char* s) {
    while (*s == ' ') s++;
    for (int i = 0; i < N_PADS; i++) if (!strcmp(s, PADS[i].ini)) return PADS[i].pad;
    return CTL_NONE;
}
static void trim(char* s) { size_t n = strlen(s); while (n && (s[n - 1] == ' ' || s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == '\t')) s[--n] = 0; }

void controls_load(void) {
    controls_defaults();
    FILE* f = fopen(INI, "r");
    if (!f) return;
    char line[300];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        char* eq = strchr(line, '='); if (!eq) continue;
        *eq = 0; char* id = line; trim(id); while (*id == ' ') id++;
        int a = -1; for (int i = 0; i < CA_N; i++) if (!strcmp(id, g_ctl[i].id)) a = i;
        if (a < 0) continue;
        /* "key1 | key2 | button" */
        char* f1 = eq + 1; char* f2 = strstr(f1, " | "); if (!f2) continue; *f2 = 0; f2 += 3;
        char* f3 = strstr(f2, " | "); if (!f3) continue; *f3 = 0; f3 += 3;
        trim(f1); trim(f2); trim(f3);
        g_ctl_key[a][0] = parse_key(f1); g_ctl_key[a][1] = parse_key(f2); g_ctl_pad[a] = parse_pad(f3);
    }
    fclose(f);
}

void controls_save(void) {
    if (getenv("DQIX_NO_SETTINGS")) return;
    FILE* f = fopen(INI, "w");
    if (!f) return;
    fprintf(f, "# Dragon Quest IX controls (Game > Controls on Windows). The game writes this file.\n");
    fprintf(f, "# action = key | second key | controller button\n");
    fprintf(f, "# Keys by name (X, Return, Left Shift, F9, -, Ctrl+R), or none.\n");
    fprintf(f, "# Controller (by position): a (bottom) b (right) x (left) y (top) back guide start leftstick rightstick\n");
    fprintf(f, "# leftshoulder rightshoulder lefttrigger righttrigger dpup dpdown dpleft dpright misc1 paddle1-4 touchpad, or none.\n");
    for (int a = 0; a < CA_N; a++) {
        char k0[64], k1[64]; const char* p = "none";
        controls_key_name(g_ctl_key[a][0], k0, sizeof k0); controls_key_name(g_ctl_key[a][1], k1, sizeof k1);
        if (!g_ctl_key[a][0]) strcpy(k0, "none");
        if (!g_ctl_key[a][1]) strcpy(k1, "none");
        for (int i = 0; i < N_PADS; i++) if (PADS[i].pad == g_ctl_pad[a]) p = PADS[i].ini;
        fprintf(f, "%s = %s | %s | %s\n", g_ctl[a].id, k0, k1, p);
    }
    fclose(f);
}
