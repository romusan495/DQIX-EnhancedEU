/* The PC interface's texts (menu bar, dialogs, messages, help) in the
 * player's language. The English text is the key: lang/<language>.txt (in
 * the game folder) gives each one its translation, and a text it doesn't
 * have stays in English. Nothing to translate is written in the code. */
#pragma once

/* languages, numbered like the game's (settings.ini language): 1 English,
 * 2 French, 3 German, 4 Italian, 5 Spanish */
#define UI_LANGS 5
extern const char* const UI_LANG_KEY[UI_LANGS + 1];   /* "", "english", "french"... (settings.ini, lang/<key>.txt) */

void ui_lang_load(int lang);          /* reads lang/<key>.txt (English: nothing to read) */
int ui_lang(void);                    /* the language loaded, 1-5 */

/* the text in the language loaded (UTF-8), or `en` itself. A translation
 * keeps the original's printf conversions (%d, %s...) in the same order, or
 * it isn't used: a format can always go through T(). */
const char* ui_tr(const char* en);
#define T(s) ui_tr(s)
