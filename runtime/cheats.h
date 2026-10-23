/* Cheats: Action Replay DS codes, each one turned on or off in the Cheats
 * menu (cheats.c). All off unless the player turns them on. */
#pragma once
#include <stdint.h>

/* The cheats this ROM has (the USA or the European version's codes):
 * cheat i = 0..cheats_count()-1. */
int cheats_count(void);
int cheats_european(void);            /* 1: the European version's (else the USA's) */
const char* cheats_name(int i);        /* the menu's text (with the chosen number) */
int cheats_max(int i);                 /* > 0: the player chooses a number, 0..this */
int cheats_value(int i);
void cheats_set_value(int i, int v);
const char* cheats_key(int i);         /* its name in settings.ini */
int cheats_on(int i);
void cheats_set(int i, int on);
/* settings.ini: "cheats = gold=50000,hp" (the keys of those on, with their numbers) */
void cheats_from_list(const char* list);
void cheats_to_list(char* out, int size);
/* Once a frame, after the buttons are read (input.c): run the codes that are on. */
void cheats_frame(void);
