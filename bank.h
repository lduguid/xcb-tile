#ifndef BANK_H
#define BANK_H

#include "hw.h"

#include <stdint.h>
#include <stdio.h>

/* A tileset + world map + sprite patterns the PPU can consume.
 *
 * Binary .bank (little-endian):
 *   8 bytes  magic "TILBANK\0"
 *   u16      version (1)
 *   u16      map_w, map_h  (1..BANK_MAP_MAX)
 *   u16      reserved (0)
 *   u32[64]  palette 0x00RRGGBB
 *   u8[256][64]   tile pixels, values 0..63
 *   u8[32][1024]  sprite pixels, 0 = transparent
 *   u8[map_h*map_w] nametable ids, row-major
 *   u8[256]  solid flags (version 2; v1 files imply id 0 empty, rest solid)
 *
 * Tile 0 is empty/sky. bank_solid[] says which ids the player collides with.
 * Maps are world-space; the PPU nametable is only 41x26 — paint it
 * from the camera with bank_paint_view / bank_apply_coarse. */

enum {
    BANK_VERSION = 2,
    BANK_MAP_MAX = 256,
    BANK_MAP_MIN = 8,
    BANK_TILE_PIX = HW_TILE * HW_TILE,
    BANK_SP_PIX = HW_SP_W * HW_SP_H
};

typedef struct {
    uint32_t pal[HW_COLORS];
    uint8_t tiles[HW_TILES][BANK_TILE_PIX];
    uint8_t sprites[HW_SP_PATS][BANK_SP_PIX];
    int map_w, map_h;
    uint8_t *map;
    uint8_t solid[HW_TILES];
} Bank;

void bank_init(Bank *b);
void bank_free(Bank *b);
int bank_copy(Bank *dst, const Bank *src);
int bank_resize_map(Bank *b, int w, int h);

/* Default 4x4x4 cube plus the demo charset (sky/cloud/star/grass/...). */
void bank_default_palette(Bank *b);
void bank_seed_charset(Bank *b);
void bank_guess_solid(Bank *b); /* id 0 empty, every other id solid */
int bank_is_solid(const Bank *b, int id);
int bank_blocked(const Bank *b, int tx, int ty);

/* Opaque pixels in pattern `pat` (colour 0 = empty, same as a PPU mask)
 * vs solid tiles. wx, wy are the sprite's world top-left. */
int bank_mask_hit(const Bank *b, int wx, int wy, int pat);
/* Inclusive pixel bounds of opaque cells. Returns 0 if the pattern is empty. */
int bank_mask_bounds(const Bank *b, int pat, int *x0, int *y0, int *x1, int *y1);

void bank_scroll_to(const Bank *b, int cam_x, int cam_y);

int bank_at(const Bank *b, int tx, int ty);
void bank_put(Bank *b, int tx, int ty, int id);

int bank_load(const char *path, Bank *b);
int bank_save(const char *path, const Bank *b);
int bank_export_c(const char *path, const Bank *b);
/* `b` must be zeroed (`Bank b = {0}`) or already bank_init'd / bank_load'd. */
int bank_from_embed(Bank *b, const uint32_t *pal, const uint8_t *tiles, const uint8_t *sprites,
                    int map_w, int map_h, const uint8_t *map);

/* PPU upload / nametable paint — link hw.c. */
void bank_upload(const Bank *b);
void bank_paint_view(const Bank *b);
void bank_paint_col(const Bank *b, int col);
void bank_paint_row(const Bank *b, int row);
void bank_apply_coarse(const Bank *b, HwCoarse c);
void bank_scroll_keys(const Bank *b, float dt);

#endif
