# xcb-tile

You never plot pixels. You are programming a **tile PPU** (VIC-II / NES nametable, 8-pixel fine scroll).

Load a 64-color palette, stamp 8×8 tiles into a charset, poke tile ids into a nametable, poke the scroll. Sprites are 32×32, 32 of them, color 0 transparent. Positions are **screen pixels** — subtract the camera yourself, same as a C64.

The nametable is one tile larger than the 320×200 view (41×26). `hw_scroll` moves one pixel at a time. After 8 pixels the nametable `memmove`s one cell and fine wraps. Newly exposed cells become **tile 0**; fill them with `hw_put`. That “paint the new column” step is the constraint that shaped C64/NES scrollers.

Include `hw.h`. Linux and Windows.

## Build

```bash
make
./xcb-tile
./xcb-tile-play examples/mario.bank
```

Windows (no editor — that is Linux/X11): `make -f Makefile.win32 TAG=-mingw`.

## Your loop

```c
#include "hw.h"
#include "map.h"

static void tick(float dt)
{
    map_tileset();          /* once: palette + charset */
    map_scroll_keys(dt);    /* hw_scroll + map_apply_coarse */
    hw_swap();
}

int main(void)
{
    return hw_main(tick);
}
```

Upload tiles once (`hw_palette`, `hw_tile`). Each frame: move, `hw_put` any new column/row, place sprites, `hw_swap`.

## PPU poke

```c
hw_palette(HW_PAL(3, 0, 0), HW_RGB(255, 40, 40));  /* 4×4×4 cube index */
hw_tile(1, pixels_8x8);     /* values 0..63 */
hw_put(tx, ty, 1);          /* nametable cell */
hw_fill(0);
HwCoarse c = hw_scroll(dx, dy);
if (c.coarse_x)
    /* stamp the new column only */;
```

Sprites:

```c
hw_sprite_pat(0, pix_32x32);
hw_sprite(0, sx, sy, 0);    /* screen x,y; enables */
hw_sprite_behind(0, 1);     /* behind non-zero tiles */
hw_sprite_draw(0, 0);       /* invisible, still collides (C64 mask sprite) */
hw_hit(0, 1);               /* opaque overlap */
hw_hit_bg(0);               /* opaque sprite over a non-zero tile */
```

Keys: `hw_key_down` / `hw_key_pressed`. `KEY_JUMP` is space.

`hw_reset` zeros fine, origin, nametable, and sprites. Palette and charsets stay.

## Banks and play

A `.bank` is a tileset + world map + sprite patterns the PPU can consume (`bank.h`). Maps are world-sized; the PPU nametable is only 41×26 — paint it from the camera with `bank_paint_view` / `bank_apply_coarse`.

`play.h` is a 32×32 actor: `actor_platform` (gravity + jump) or `actor_topdown`, collision via the sprite **mask** (color 0 empty). `play_cam_follow` keeps the PPU pointed at the player.

```bash
./xcb-tile-play examples/mario.bank
./xcb-tile-play --walk examples/commando.bank
```

## Editor (Linux)

`./xcb-tile-edit` paints tiles, map, and sprites. Save `.bank`; **E** exports a C header. `1 2 3` switch panes; LMB paint, RMB pick; **X** toggles solid; **S/L** save/load. See the comment at the top of `editor.c`.

`examples/mkbanks.c` rebuilds the sample `.bank` files.

## What to steal

| Demo | Ideas |
|------|--------|
| `demo.c` | Fine scroll, paint only the new column/row (`map.h`) |
| `demo_anim.c` | Animated charset |
| `demo_sprites.c` | 32 sprites, hit tests |
| `demo_mask.c` | Invisible collision sprites |
| `demo_play.c` | Load a `.bank`, walk/jump |
| `sprites_odin/` | Same PPU from Odin |
