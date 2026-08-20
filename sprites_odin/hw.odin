/* C ABI for hw.h + map.h. Link libtile.a (hw.c, plat.c, map.c). */
package sprites_odin

import "core:c"

HW_TILE :: 8
HW_VIEW_W :: 320
HW_VIEW_H :: 200
HW_MAP_W :: HW_VIEW_W / HW_TILE + 1
HW_MAP_H :: HW_VIEW_H / HW_TILE + 1
HW_COLORS :: 64
HW_TILES :: 256
HW_SP_W :: 32
HW_SP_H :: 32
HW_SPRITES :: 32
HW_SP_PATS :: 32

Pixel :: u32

hw_rgb :: #force_inline proc(r, g, b: u32) -> Pixel {
    return (r << 16) | (g << 8) | b
}

/* Palette index = r + 4*g + 16*b with r,g,b in 0..3. */
hw_pal :: #force_inline proc(r, g, b: i32) -> i32 {
    return r + 4 * g + 16 * b
}

HwCoarse :: struct {
    coarse_x: c.int,
    coarse_y: c.int,
}

KEY_LEFT :: 1
KEY_RIGHT :: 2
KEY_UP :: 3
KEY_DOWN :: 4
KEY_ESC :: 5
KEY_Q :: 6

T_SKY :: 0
T_CLOUD :: 1
T_STAR :: 2
T_GRASS :: 3
T_DIRT :: 4
T_STONE :: 5
T_WATER :: 6
T_BRICK :: 7
T_COUNT :: 8

foreign import plat "../libtile.a"

@(default_calling_convention = "c")
foreign plat {
    hw_view_width :: proc() -> c.int ---
    hw_view_height :: proc() -> c.int ---
    hw_map_width :: proc() -> c.int ---
    hw_map_height :: proc() -> c.int ---

    hw_fine_x :: proc() -> c.int ---
    hw_fine_y :: proc() -> c.int ---
    hw_cam_x :: proc() -> c.int ---
    hw_cam_y :: proc() -> c.int ---

    hw_palette :: proc(index: c.int, color: Pixel) ---
    hw_tile :: proc(id: c.int, pix: [^]u8) ---
    hw_put :: proc(tx, ty, id: c.int) ---
    hw_get :: proc(tx, ty: c.int) -> c.int ---
    hw_fill :: proc(id: c.int) ---

    hw_set_fine :: proc(x, y: c.int) ---
    hw_reset :: proc() ---

    hw_sprite_pat :: proc(id: c.int, pix: [^]u8) ---
    hw_sprite :: proc(i, x, y, pat: c.int) ---
    hw_sprite_off :: proc(i: c.int) ---
    hw_sprite_behind :: proc(i, behind: c.int) ---
    hw_sprite_draw :: proc(i, draw: c.int) ---

    hw_hit :: proc(a, b: c.int) -> c.int ---
    hw_hit_bg :: proc(i: c.int) -> c.int ---

    hw_scroll :: proc(dx, dy: c.int) -> HwCoarse ---
    hw_swap :: proc() ---

    hw_key_down :: proc(key: c.int) -> c.int ---
    hw_main :: proc(tick: proc "c" (dt: f32)) -> c.int ---

    map_tileset :: proc() ---
    map_at :: proc(tx, ty: c.int) -> c.int ---
    map_fill :: proc() ---
    map_fill_col :: proc(col: c.int) ---
    map_fill_row :: proc(row: c.int) ---
    map_apply_coarse :: proc(c: HwCoarse) ---
    map_scroll_keys :: proc(dt: f32) ---
}
