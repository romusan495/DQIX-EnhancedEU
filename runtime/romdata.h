/* Reading the game's own files from the ROM (implemented in mapmarks.c,
 * shared with the journal): NitroFS paths, Level-5's GPC2 packs, their
 * script records and their texts' letter codes. Read only. */
#pragma once
#include <stdint.h>

/* A file of the ROM by its path ("data/prm/itemname.gp2"), or NULL. Points into the ROM. */
const uint8_t* romdata_file(const char* path, uint32_t* size);
/* A file in a GPC2 pack, by name, unpacked (caller frees), or NULL. */
uint8_t* romdata_gpc_file(const uint8_t* d, uint32_t n, const char* name, uint32_t* size);

/* The game's script records: u16 tag, u8 count, 2-bit types (0 a string,
 * 1 a number, 2 a float), then the words. */
#define REC_MAX 64
typedef struct { int tag, n; uint8_t type[REC_MAX]; uint32_t v[REC_MAX]; } Rec;   /* n: the words kept (at most REC_MAX) */
/* The next record from *p (stops at `end` or the 0xFFFF end mark); 0 at the end. */
int romdata_rec_next(const uint8_t* b, uint32_t end, uint32_t* p, Rec* r);

/* A text's letter codes (<'e>...) as UTF-8, grammar notes ([...]) and other codes left out. */
void romdata_clean_name(const char* s, char* out, size_t cap);

/* An item's name (data/prm/itemname.gp2) in the game's language, as UTF-8, or NULL. */
const char* romdata_item_name(uint16_t key);
