/* Modern: farm gold by itself (farm.c). */
#pragma once
#include <stdint.h>

extern int g_farm_on;              /* farming now */
extern int g_farm_target;          /* stop once the party has this much gold */
extern int g_farm_fast;            /* fast forward while farming */
extern char g_farm_why[200];       /* why it stopped */
extern int g_farm_stopped;         /* it just stopped: the frontend pauses, says why and clears this */

const char* farm_start(void);      /* NULL if it started, else why it can't */
void farm_cancel(const char* why);
int farm_gold(void);               /* the party's gold, -1 if unknown */
int party_in_battle(void);         /* a battle is on (Modern > Battle speed) */
void farm_input(uint16_t* keys, int* touch);   /* input.c, every frame */

extern int g_battle_auto_victory;  /* Modern > Battle speed: the victory's messages go by themselves */
void battle_victory_input(uint16_t* keys);   /* input.c, every frame */
