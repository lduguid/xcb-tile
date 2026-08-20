/* Sprites over the tiled world. Positions are screen pixels (hardware
 * does not scroll them). Index 0 is in front. One sprite is marked
 * behind solid tiles. hw_hit bounces pairs; hw_hit_bg flashes. */
package sprites_odin

import "base:runtime"
import "core:c"

N :: 8

x, y, vx, vy: [N]f32
started: bool

make_blob :: proc(pat: int, fill, rim: u8) {
    pix: [HW_SP_W * HW_SP_H]u8
    cx := HW_SP_W / 2
    cy := HW_SP_H / 2
    r := 14
    r2 := r * r
    inner := (r - 3) * (r - 3)

    for sy in 0 ..< HW_SP_H {
        for sx in 0 ..< HW_SP_W {
            d := (sx - cx) * (sx - cx) + (sy - cy) * (sy - cy)
            col: u8 = 0
            if d <= r2 {
                col = d >= inner ? rim : fill
            }
            pix[sy * HW_SP_W + sx] = col
        }
    }
    hw_sprite_pat(c.int(pat), raw_data(pix[:]))
}

tick :: proc "c" (dt: f32) {
    context = runtime.default_context()

    vw := f32(HW_VIEW_W)
    vh := f32(HW_VIEW_H)

    if !started {
        fill := [N]u8 {
            u8(hw_pal(3, 0, 0)),
            u8(hw_pal(3, 2, 0)),
            u8(hw_pal(3, 3, 0)),
            u8(hw_pal(0, 3, 0)),
            u8(hw_pal(0, 3, 3)),
            u8(hw_pal(0, 1, 3)),
            u8(hw_pal(2, 0, 3)),
            u8(hw_pal(3, 0, 2)),
        }
        map_tileset()
        map_fill()
        for i in 0 ..< N {
            make_blob(i, fill[i], u8(hw_pal(3, 3, 3)))
            x[i] = f32(20 + i * 34)
            y[i] = f32(8 + (i % 3) * 20)
            vx[i] = (i & 1) != 0 ? 50.0 + f32(i) * 6.0 : -(46.0 + f32(i) * 5.0)
            vy[i] = (i & 2) != 0 ? 36.0 + f32(i) * 4.0 : -(40.0 + f32(i) * 3.0)
        }
        hw_sprite_behind(N - 1, 1)
        started = true
    }

    map_scroll_keys(dt)

    for i in 0 ..< N {
        x[i] += vx[i] * dt
        y[i] += vy[i] * dt
        if x[i] < 0 {
            x[i] = 0
            vx[i] = -vx[i]
        } else if x[i] > vw - HW_SP_W {
            x[i] = vw - HW_SP_W
            vx[i] = -vx[i]
        }
        if y[i] < 0 {
            y[i] = 0
            vy[i] = -vy[i]
        } else if y[i] > vh - HW_SP_H {
            y[i] = vh - HW_SP_H
            vy[i] = -vy[i]
        }
        hw_sprite(c.int(i), c.int(x[i] + 0.5), c.int(y[i] + 0.5), c.int(i))
    }

    for i in 0 ..< N {
        for j in i + 1 ..< N {
            if hw_hit(c.int(i), c.int(j)) == 0 {
                continue
            }
            dx := x[j] - x[i]
            dy := y[j] - y[i]
            rel := (vx[j] - vx[i]) * dx + (vy[j] - vy[i]) * dy
            if rel < 0.0 {
                vx[i], vx[j] = vx[j], vx[i]
                vy[i], vy[j] = vy[j], vy[i]
            }
        }
        if hw_hit_bg(c.int(i)) != 0 && i != N - 1 {
            if vy[i] > 0 {
                vy[i] = -vy[i]
            }
        }
    }

    hw_swap()
}

main :: proc() {
    hw_main(tick)
}
