/* Modern: farm gold by itself (Modern > Farm gold). The party walks around
 * where it is, so the monsters on the field run into it, fights every battle
 * by pressing A (Fight, then each member's first command: Attack, or what
 * their tactics say), and stops once the party's gold reaches the target.
 * It also stops when someone in the party is low on HP or knocked out (a
 * wipe-out costs half the gold), when the party leaves the area, when no
 * battle has come for a while, when the game seems stuck in a menu, and as
 * soon as the player presses anything. When it stops, the frontend pauses
 * the game and says why.
 *
 * To the game it is just buttons, like a player pressing them. */
#include <math.h>
#include "nds.h"
#include "uitext.h"
#include "farm.h"

int g_farm_on;
int g_farm_target = 10000;
int g_farm_fast = 1;
char g_farm_why[200];
int g_farm_stopped;

/* ---------------------------------------------------------------- what the game shows (main RAM) */
/* 1 while the hero is free to walk, 2 in dialogue, menus and battles, 0 while
 * loading (see input.c) */
#define WALK_FLAG 0x109DA4
/* the field's characters, by number: 0 the hero, 0x70..0x7B the monsters
 * walking around. 0 until an adventure is loaded. Each points at a record
 * whose second word has the map's number (PLACE_NUM: 0x4E21 in Angel
 * Falls' field, 0x4E22 west of Stornway...) in its top half while it is
 * there, with its position (x, height, z, 20.12 fixed point) at +0x44. */
#define FIELD_PTR 0x0F33E0
#define MON_FIRST 0x70
#define MON_COUNT 12
#define POS       0x44
/* the map the party is on (the room, town, field...) */
#define MAP_ID    0x0FB33C
/* the place's number (u16): towns 1..., fields 20001... (mapmarks.c) */
#define PLACE_NUM 0x0FB3F0
/* the party's gold (u32) */
#define GOLD      0x0F6D48
/* the party's characters: up to 5 records of 0x964 bytes, the hero's last.
 * Each starts with the name; HP, MP, max HP and max MP (u16) are at +0x58.
 * An unused record has max HP 0 (or 0xFFFF). */
#define CHARS     0x0F2F24
#define CHAR_SIZE 0x964
#define N_CHARS   5
#define CHAR_HP   0x58
/* the hero's experience (u32, in the hero's record): the game adds it, and
 * the gold, the moment the last monster falls, before the victory messages */
#define HERO_EXP  (CHARS + 4 * CHAR_SIZE + 0x114)

static uint32_t rd32(uint32_t a) { uint32_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 1)], 4); return v; }
static uint16_t rd16(uint32_t a) { uint16_t v; memcpy(&v, &g_main_ram[a & (MAIN_RAM_SIZE - 1)], 2); return v; }

static int adventure_loaded(void) { return rd32(FIELD_PTR) != 0; }
static int free_to_walk(void) { return g_main_ram[WALK_FLAG] == 1 && adventure_loaded(); }

/* The party's gold, or -1 if it can't be read here. */
int farm_gold(void) {
    if (!adventure_loaded()) return -1;
    uint32_t g = rd32(GOLD);
    return g <= 9999999 ? (int)g : -1;
}

/* A battle is on: the 3D is on the top screen (the field has it on the
 * bottom one) and the hero's record has its max HP (0 on the field). */
int party_in_battle(void) {
    int max = rd16(CHARS + (N_CHARS - 1) * CHAR_SIZE + CHAR_HP + 4);
    return (IO16(0x304) & 0x8000) && adventure_loaded() && max > 0 && max <= 999;
}

/* Someone in the party is knocked out or below a third of their HP. */
static int party_in_danger(char* who, size_t n) {
    for (int i = 0; i < N_CHARS; i++) {
        uint32_t c = CHARS + i * CHAR_SIZE;
        int hp = rd16(c + CHAR_HP), max = rd16(c + CHAR_HP + 4);
        if (max == 0 || max > 999 || hp > max) continue;      /* nobody here */
        if (hp * 3 >= max) continue;
        size_t k = 0;                                          /* the name: plain letters, as the save holds it */
        while (k + 1 < n && k < 12) {
            uint8_t ch = g_main_ram[c + k];
            if (ch < 0x20 || ch > 0x7e) break;
            who[k++] = (char)ch;
        }
        who[k] = 0;
        if (!k) snprintf(who, n, "%s", T("Someone"));
        LOG_INFO("farm: %s has %d of %d HP\n", who, hp, max);
        return 1;
    }
    return 0;
}

/* Where a field character is (x, z), or 0 if it isn't there. */
static int field_pos(int id, double* x, double* z) {
    uint32_t p = rd32(FIELD_PTR + 4 * id);
    if (p < 0x02000000 || p >= 0x02400000 || (rd32(p + 4) >> 16) != rd16(PLACE_NUM)) return 0;
    *x = (int32_t)rd32(p + POS) / 4096.0;
    *z = (int32_t)rd32(p + POS + 8) / 4096.0;
    return 1;
}

/* ---------------------------------------------------------------- the farmer */
#define LEG_FRAMES      80          /* one leg of the walk (~1.3 s) */
#define NO_BATTLE_LIMIT (3 * 60 * 60) /* no battle for 3 minutes: no monsters here */
#define BUSY_LIMIT      (4 * 60 * 60) /* not back on the field for 4 minutes: stuck */
#define SEE        28.0             /* goes for monsters this close to the hero... */
#define LEASH      36.0             /* ...and this close to where the farm started */
#define STEP       8                /* frames between looks at the field */
#define GIVE_UP    (3 * 60)         /* chasing one monster for 3 s without getting closer: leave it */
#define FORGET     (10 * 60)        /* and leave it alone for 10 s */

static const uint16_t k_dirs[8] = {   /* the D-pad's 8 directions, clockwise from up */
    1u << 6, (1u << 6) | (1u << 4), 1u << 4, (1u << 7) | (1u << 4),
    1u << 7, (1u << 7) | (1u << 5), 1u << 5, (1u << 6) | (1u << 5),
};

static struct {
    uint32_t map;                    /* the map it started on */
    int dir, leg, back;              /* walking: direction, frames left in this leg, coming back */
    int since_battle, busy;          /* frames on the field without a battle; frames off the field */
    int tick;                        /* for pressing A now and then */
    int battles;
    int gold_start;
    double x0, z0;                   /* where it started */
    /* which way each button takes the hero: the camera can be turned, so it
     * learns it while walking (least squares: world move = M * button) */
    double kk[2][2], wk[2][2];
    double lx, lz; int lkey, lheld;  /* the last look: position, button held, for how long */
    int prey, prey_for; double prey_best;   /* the monster it chases (-1 none), since when, the closest it got */
    int shy[MON_COUNT];              /* frames left leaving each monster alone */
    uint16_t press;                  /* the button it holds until the next look */
    uint32_t exp0, gold0; int won;   /* this battle: experience and gold when it began, when it was won (0 not yet) */
} F;

static const double k_vec[8][2] = {   /* k_dirs as screen vectors (right, up) */
    { 0, 1 }, { 0.7071, 0.7071 }, { 1, 0 }, { 0.7071, -0.7071 },
    { 0, -1 }, { -0.7071, -0.7071 }, { -1, 0 }, { -0.7071, 0.7071 },
};

/* Learns from the last STEP frames: the hero held button `d` and moved (dx, dz). */
static void learn(int d, double dx, double dz) {
    double n = sqrt(dx * dx + dz * dz);
    if (n < 0.15) return;                                  /* blocked: says nothing */
    dx /= n; dz /= n;
    for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) {
        F.kk[i][j] = F.kk[i][j] * 0.97 + k_vec[d][i] * k_vec[d][j];
        F.wk[i][j] = F.wk[i][j] * 0.97 + (i ? dz : dx) * k_vec[d][j];
    }
}

/* The button that walks the hero toward (dx, dz), or -1 if it hasn't learnt enough yet. */
static int button_toward(double dx, double dz) {
    double a = F.kk[0][0], b = F.kk[0][1], c = F.kk[1][0], d = F.kk[1][1];
    double det = a * d - b * c;
    if (det < 0.5) return -1;                              /* needs two directions at least */
    /* M = wk * kk^-1, then the button k = M^-1 * (dx, dz) */
    double ia = d / det, ib = -b / det, ic = -c / det, id = a / det;
    double m00 = F.wk[0][0] * ia + F.wk[0][1] * ic, m01 = F.wk[0][0] * ib + F.wk[0][1] * id;
    double m10 = F.wk[1][0] * ia + F.wk[1][1] * ic, m11 = F.wk[1][0] * ib + F.wk[1][1] * id;
    double md = m00 * m11 - m01 * m10;
    if (fabs(md) < 0.2) return -1;
    double kx = (m11 * dx - m01 * dz) / md, ky = (-m10 * dx + m00 * dz) / md;
    int best = -1; double bv = -1e9;
    for (int i = 0; i < 8; i++) {
        double v = k_vec[i][0] * kx + k_vec[i][1] * ky;
        if (v > bv) { bv = v; best = i; }
    }
    return best;
}

/* Every STEP frames on the field: picks the button to hold. Goes for the
 * closest monster in sight, else walks a star around the starting point (out
 * and back, turning 135 degrees each time), which also teaches it which way
 * each button goes. */
static void walk(void) {
    double x, z;
    if (!field_pos(0, &x, &z)) { F.press = 0; return; }
    if (F.lkey >= 0 && F.lheld >= STEP) learn(F.lkey, x - F.lx, z - F.lz);
    F.lx = x; F.lz = z;
    for (int i = 0; i < MON_COUNT; i++) if (F.shy[i] > 0) F.shy[i] -= STEP;

    int target = -1; double tx = 0, tz = 0, best = SEE * SEE;
    for (int i = 0; i < MON_COUNT; i++) {
        double mx, mz;
        if (F.shy[i] > 0 || !field_pos(MON_FIRST + i, &mx, &mz)) continue;
        double d = (mx - x) * (mx - x) + (mz - z) * (mz - z);
        double home = (mx - F.x0) * (mx - F.x0) + (mz - F.z0) * (mz - F.z0);
        if (d < best && home < LEASH * LEASH) { best = d; target = i; tx = mx; tz = mz; }
    }
    if (target != F.prey) { F.prey = target; F.prey_for = 0; F.prey_best = 1e18; }
    int key = -1;
    if (target >= 0) {
        F.prey_for += STEP;
        if (best < F.prey_best - 1.0) { F.prey_best = best; F.prey_for = 0; }
        if (F.prey_for > GIVE_UP) { F.shy[target] = FORGET; F.prey = -1; }   /* can't reach it: leave it */
        else key = button_toward(tx - x, tz - z);
    } else {
        double hx = F.x0 - x, hz = F.z0 - z;
        if (hx * hx + hz * hz > LEASH * LEASH) key = button_toward(hx, hz);   /* too far: back home */
    }
    if (key < 0) {                                         /* the star */
        F.leg -= STEP;
        if (F.leg <= 0) {
            F.leg = LEG_FRAMES;
            if (F.back) F.dir = (F.dir + 3) % 8;
            F.back = !F.back;
        }
        key = F.back ? (F.dir + 4) % 8 : F.dir;
    }
    F.lheld = key == F.lkey ? F.lheld + STEP : 0;
    F.lkey = key;
    F.press = k_dirs[key];
}

static void stop(const char* why) {
    g_farm_on = 0;
    g_farm_stopped = 1;
    snprintf(g_farm_why, sizeof g_farm_why, "%s", why);
    LOG_INFO("farm: stopped after %d battles: %s\n", F.battles, why);
}

const char* farm_start(void) {
    if (!adventure_loaded()) return "Load your adventure first.";
    if (!free_to_walk()) return "Start it while the hero is free to walk on the field (no menu or conversation open).";
    if (farm_gold() >= 0 && farm_gold() >= g_farm_target) return "The party already has that much gold: raise the target first.";
    memset(&F, 0, sizeof F);
    F.map = rd16(MAP_ID);
    F.dir = (int)(g_frame % 8);
    F.leg = LEG_FRAMES;
    F.gold_start = farm_gold();
    field_pos(0, &F.x0, &F.z0);
    F.lx = F.x0; F.lz = F.z0; F.lkey = -1; F.prey = -1;
    g_farm_on = 1;
    g_farm_stopped = 0;
    g_farm_why[0] = 0;
    LOG_INFO("farm: started on map %u, gold %d, target %d\n", F.map, F.gold_start, g_farm_target);
    return NULL;
}

void farm_cancel(const char* why) { if (g_farm_on) stop(why); }

/* Once per frame, before the game reads the buttons: replaces them while farming. */
void farm_input(uint16_t* keys, int* touch) {
    if (!g_farm_on) return;
    if (*keys || *touch) { stop("You pressed a button: the farm stopped so you can play."); return; }
    char who[64];
    if (party_in_danger(who, sizeof who)) {
        char t[160]; snprintf(t, sizeof t, T("%s is in danger: heal the party before farming more."), who);
        stop(t); return;
    }
    if (!adventure_loaded()) { stop("The adventure isn't loaded any more."); return; }
    int gold = farm_gold();
    F.tick++;
    if (free_to_walk()) {
        F.busy = 0;
        if (gold >= 0 && gold >= g_farm_target) {
            char t[160]; snprintf(t, sizeof t, T("Done: the party has %d gold (target %d), after %d battles."), gold, g_farm_target, F.battles);
            stop(t); return;
        }
        if (rd16(MAP_ID) != F.map) { stop("The party left the area where the farm started (a door or the edge of the map)."); return; }
        if (++F.since_battle > NO_BATTLE_LIMIT) { stop("No battle for 3 minutes: try a place with more monsters."); return; }
        if (F.tick % STEP == 0) walk();
        *keys = F.press;
        return;
    }
    /* a battle, its messages, a conversation: A, a short press every 12 frames */
    if (F.since_battle > 0) {
        F.battles++; F.since_battle = 0; F.prey = -1; F.lkey = -1;
        F.exp0 = rd32(HERO_EXP); F.gold0 = rd32(GOLD); F.won = 0;
    }
    /* Won: the messages after it go by with B, and when a level brings skill
     * points the game opens the Allocate Skill Points screen. B there asks
     * "Cancel skill point allocation?" with the cursor on No, so: B, then Up
     * to Yes and A. The points stay unspent, for the player. On a plain
     * message Up does nothing and A just turns the page. */
    if (!F.won && (rd32(HERO_EXP) > F.exp0 || rd32(GOLD) > F.gold0)) F.won = F.tick;
    if (++F.busy > BUSY_LIMIT) { stop("The game has been off the field for 4 minutes: something needs you."); return; }
    if (F.won) {
        int t = (F.tick - F.won) % 60;
        if (t < 2) *keys = 1u << 1;                       /* B */
        else if (t >= 40 && t < 42) *keys = 1u << 6;      /* Up */
        else if (t >= 48 && t < 50) *keys = 1u << 0;      /* A */
    } else if (F.tick % 12 < 2) *keys = 1u << 0;
}
