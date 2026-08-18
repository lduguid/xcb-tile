#ifndef HW_H
#define HW_H

#include <stdint.h>

/* Tile PPU — VIC-II / NES nametable, 8-pixel fine scroll.
 *
 * You never plot pixels. You load a 64-color palette, stamp 8x8 tiles into
 * a charset, poke tile ids into a nametable, and poke the scroll.
 *
 * The nametable is one tile larger than the view on each axis so fine
 * scroll 0..7 has a full extra column and row to reveal. hw_scroll moves
 * one pixel at a time. After 8 pixels the nametable is memmove'd one cell
 * and fine wraps — same coarse/fine split as $D016/$D011 or PPUSCROLL.
 * Newly exposed cells are tile 0; fill them with hw_put. That "paint the
 * new column" step is the constraint that shaped C64/NES scrollers.
 *
 * hw_reset zeros fine, origin, nametable, and sprites. Palette and
 * charsets stay.
 *
 * Sprites are 32x32, 32 of them, 32 patterns, same 64-color palette.
 * Color 0 in a pattern is transparent. Positions are screen pixels
 * (the PPU does not know about world space — subtract the camera
 * yourself, same as a C64 or NES). Index 0 is in front of 31.
 * hw_sprite_behind(i, 1) hides the sprite where the nametable cell
 * is not tile 0 (VIC-II / NES "behind background").
 * hw_sprite_draw(i, 0) leaves the sprite in the collision set but does
 * not plot it — a C64 "mask sprite" (invisible, still solid).
 *
 * hw_hit / hw_hit_bg are pixel tests on the current poke state
 * (call them after moving, before or after hw_swap). */

typedef uint32_t Pixel; /* 0x00RRGGBB */

enum {
    HW_TILE = 8,
    HW_VIEW_W = 320,
    HW_VIEW_H = 200,
    HW_MAP_W = HW_VIEW_W / HW_TILE + 1, /* 41 */
    HW_MAP_H = HW_VIEW_H / HW_TILE + 1, /* 26 */
    HW_COLORS = 64,
    HW_TILES = 256,
    HW_SP_W = 32,
    HW_SP_H = 32,
    HW_SPRITES = 32,
    HW_SP_PATS = 32
};

#define HW_RGB(r, g, b) ((Pixel)(((r) << 16) | ((g) << 8) | (b)))

/* Palette index = r + 4*g + 16*b with r,g,b in 0..3 (4x4x4 cube). */
#define HW_PAL(r, g, b) ((int)((r) + 4 * (g) + 16 * (b)))

int hw_view_width(void);
int hw_view_height(void);
int hw_map_width(void);
int hw_map_height(void);

int hw_fine_x(void); /* 0 .. HW_TILE-1 */
int hw_fine_y(void);
int hw_cam_x(void);  /* origin_tx * 8 + fine_x */
int hw_cam_y(void);

void hw_palette(int index, Pixel color);                 /* 0 .. 63 */
void hw_tile(int id, const uint8_t pix[HW_TILE * HW_TILE]); /* 0 .. 255, values 0..63 */
void hw_put(int tx, int ty, int id);
int hw_get(int tx, int ty);
void hw_fill(int id);

void hw_set_fine(int x, int y); /* clamped to 0..7 */
void hw_reset(void);

void hw_sprite_pat(int id, const uint8_t pix[HW_SP_W * HW_SP_H]);
void hw_sprite(int i, int x, int y, int pat); /* screen pixels; enables */
void hw_sprite_off(int i);
void hw_sprite_behind(int i, int behind); /* 0 = front (default), 1 = behind solid tiles */
void hw_sprite_draw(int i, int draw);     /* 0 = collide only, 1 = draw (default) */

int hw_hit(int a, int b);  /* opaque pixel overlap */
int hw_hit_bg(int i);      /* opaque sprite pixel over a non-zero tile id */

typedef struct {
    int coarse_x; /* tiles shifted this call, -N..+N */
    int coarse_y;
} HwCoarse;

/* Positive dx looks right, positive dy looks down. */
HwCoarse hw_scroll(int dx, int dy);

void hw_swap(void);

enum {
    KEY_LEFT = 1,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_ESC,
    KEY_Q
};

int hw_key_down(int key);
int hw_main(void (*tick)(float dt));

#endif
