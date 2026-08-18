/* Sprites over the tiled world. Positions are screen pixels (hardware
 * does not scroll them). Index 0 is in front. One sprite is marked
 * behind solid tiles. hw_hit bounces pairs; hw_hit_bg flashes. */

#include "hw.h"
#include "map.h"

enum { N = 8 };

static float x[N], y[N], vx[N], vy[N];
static int started;

static void make_blob(int pat, uint8_t fill, uint8_t rim)
{
    uint8_t pix[HW_SP_W * HW_SP_H];
    int sx, sy, cx = HW_SP_W / 2, cy = HW_SP_H / 2, r = 14, r2, inner;

    r2 = r * r;
    inner = (r - 3) * (r - 3);
    for (sy = 0; sy < HW_SP_H; sy++) {
        for (sx = 0; sx < HW_SP_W; sx++) {
            int d = (sx - cx) * (sx - cx) + (sy - cy) * (sy - cy);
            uint8_t c = 0;

            if (d <= r2)
                c = d >= inner ? rim : fill;
            pix[sy * HW_SP_W + sx] = c;
        }
    }
    hw_sprite_pat(pat, pix);
}

static void tick(float dt)
{
    int i, j;
    float vw = (float)HW_VIEW_W;
    float vh = (float)HW_VIEW_H;

    if (!started) {
        static const uint8_t fill[] = {
            HW_PAL(3, 0, 0), HW_PAL(3, 2, 0), HW_PAL(3, 3, 0), HW_PAL(0, 3, 0),
            HW_PAL(0, 3, 3), HW_PAL(0, 1, 3), HW_PAL(2, 0, 3), HW_PAL(3, 0, 2),
        };
        map_tileset();
        map_fill();
        for (i = 0; i < N; i++) {
            make_blob(i, fill[i], HW_PAL(3, 3, 3));
            x[i] = (float)(20 + i * 34);
            y[i] = (float)(8 + (i % 3) * 20);
            vx[i] = (i & 1) ? 50.0f + i * 6.0f : -(46.0f + i * 5.0f);
            vy[i] = (i & 2) ? 36.0f + i * 4.0f : -(40.0f + i * 3.0f);
        }
        hw_sprite_behind(N - 1, 1);
        started = 1;
    }

    map_scroll_keys(dt);

    for (i = 0; i < N; i++) {
        x[i] += vx[i] * dt;
        y[i] += vy[i] * dt;
        if (x[i] < 0) {
            x[i] = 0;
            vx[i] = -vx[i];
        } else if (x[i] > vw - HW_SP_W) {
            x[i] = vw - HW_SP_W;
            vx[i] = -vx[i];
        }
        if (y[i] < 0) {
            y[i] = 0;
            vy[i] = -vy[i];
        } else if (y[i] > vh - HW_SP_H) {
            y[i] = vh - HW_SP_H;
            vy[i] = -vy[i];
        }
        hw_sprite(i, (int)(x[i] + 0.5f), (int)(y[i] + 0.5f), i);
    }

    for (i = 0; i < N; i++) {
        for (j = i + 1; j < N; j++) {
            float dx, dy, rel;

            if (!hw_hit(i, j))
                continue;
            dx = x[j] - x[i];
            dy = y[j] - y[i];
            rel = (vx[j] - vx[i]) * dx + (vy[j] - vy[i]) * dy;
            if (rel < 0.0f) {
                float tx = vx[i], ty = vy[i];

                vx[i] = vx[j];
                vy[i] = vy[j];
                vx[j] = tx;
                vy[j] = ty;
            }
        }
        if (hw_hit_bg(i) && i != N - 1) {
            if (vy[i] > 0)
                vy[i] = -vy[i];
        }
    }

    hw_swap();
}

int main(void)
{
    return hw_main(tick);
}
