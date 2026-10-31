/* Modern > Journal (J, or L3 on a controller): a PC panel over the game, which
 * waits meanwhile, with a tab per page: the quests, the party's progress
 * (skills, vocations, attributes), and the pages other parts add with
 * journal_add_page(). Everything is read from the game's memory and ROM; the
 * game never sees any of it (journal.c). */
#pragma once
#include <stdint.h>

/* Pages lay themselves out in panel units: the panel is JOURNAL_W x JOURNAL_H
 * units, centred in the window and scaled to it (the picture itself has the
 * window's own pixels, so the text stays sharp at any size). */
#define JOURNAL_W 1280
#define JOURNAL_H 720

/* Where a page draws: the panel below the tabs. */
typedef struct {
    uint8_t* rgba;                 /* the window's picture (premultiplied RGBA, rows top-down), pw x ph pixels */
    int pw, ph;
    float u, ox, oy;               /* a panel unit's pixels, and where the panel's (0, 0) is in the picture */
    int x, y, w, h;                /* the page's area, in panel units */
    int lang;                      /* the game's language: 0 English, 1 French, 2 German, 3 Italian, 4 Spanish */
} JournalCanvas;

/* What the player pressed, given to the page shown (tabs and closing are the journal's). */
enum { JK_UP, JK_DOWN, JK_LEFT, JK_RIGHT, JK_PAGE_UP, JK_PAGE_DOWN, JK_OK, JK_MORE };   /* MORE: H, or Y on a controller */

typedef struct {
    const char* (*title)(int lang);                /* the tab's name */
    void (*draw)(const JournalCanvas* c);          /* the page, each time something changed */
    int (*key)(int k);                             /* a JK_ key: 1 if it changed something (redraw); may be NULL */
    void (*opened)(void);                          /* the journal was just opened (read the game again); may be NULL */
    int (*text)(const char* utf8);                 /* letters typed (a search), "\b" to erase one; 1 if redraw; may be NULL */
} JournalPage;

void journal_add_page(const JournalPage* p);       /* at start, before the journal is first opened */

/* Drawing helpers for pages (panel units; premultiplied, clipped to the picture). */
int journal_text(const JournalCanvas* c, int x, int y, float px, uint32_t rgb, const char* utf8);   /* returns the width */
int journal_text_width(float px, const char* utf8);
int journal_wrap(const JournalCanvas* c, int x, int y, int max_w, float px, uint32_t rgb, const char* utf8, int max_lines);   /* returns the y below */
void journal_box(const JournalCanvas* c, int x, int y, int w, int h, uint32_t rgb, float alpha);   /* rounded rectangle */
void journal_bar(const JournalCanvas* c, int x, int y, int w, int h, float part, uint32_t rgb);     /* a progress bar 0..1 */
/* The game's own texts, with the game's letters (<'e>...) as UTF-8. */
void journal_clean(const char* game_text, char* out, int cap);
/* A text of one of the game's per-language tables (tag 0x67 records: number,
 * text) in the game's language, cleaned, or NULL: ("data/bin/menu/str_sta.gp2",
 * "str_sta", 5) -> "Warrior". The file inside is <stem>_<en|fr|de|it|es>.bin. */
const char* journal_game_string(const char* pack, const char* stem, int id);

/* The frontend's side. */
int journal_open(void);                            /* 1 while shown */
void journal_toggle(void);
int journal_input(int k);                          /* JK_ keys, and these: */
int journal_typed(const char* utf8);               /* letters typed (SDL_TEXTINPUT), "\b" for backspace */
int journal_searching(void);                       /* 1 when the page shown takes letters: they are not shortcuts then */
enum { JK_TAB_PREV = 100, JK_TAB_NEXT, JK_CLOSE };
const uint8_t* journal_picture(int w, int h, int* changed);   /* the window's picture (w x h drawable pixels), *changed 1 when redrawn */
