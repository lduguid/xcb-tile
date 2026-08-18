/* Mask sprites: pretty art vs a hidden hitbox.
 *
 * Top pair collides on the full spike graphic (they bounce when the
 * points touch). Bottom pair draws the same spikes but hw_hit uses a
 * small core sprite with hw_sprite_draw(0) — spikes can overlap, only
 * the cores bounce. Masks blink on every few seconds so you can see
 * the hitboxes. */

#include "hw.h"
#include "map.h"

enum { PAT_SPIKE = 0, PAT_CORE = 1 };

static float x[2][2], vx[2][2];
static float show;
static int started;

static void make_spike(int pat, uint8_t fill, uint8_t rim)
{
    uint8_t pix[HW_SP_W * HW_SP_H];
    int sx, sy, cx = HW_SP_W / 2, cy = HW_SP_H / 2;

    for (sy = 0; sy < HW_SP_H; sy++) {
        for (sx = 0; sx < HW_SP_W; sx++) {
            int dx = sx - cx, dy = sy - cy;
            int adx = dx < 0 ? -dx : dx;
            int ady = dy < 0 ? -dy : dy;
            uint8_t c = 0;

            if ((adx <= 2 && ady < 15) || (ady <= 2 && adx < 15) ||
                (adx == ady && adx < 12))
                c = (adx < 2 && ady < 2) ? rim : fill;
            pix[sy * HW_SP_W + sx] = c;
        }
    }
    hw_sprite_pat(pat, pix);
}

static void make_core(int pat, uint8_t fill)
{
    uint8_t pix[HW_SP_W * HW_SP_H];
    int sx, sy, cx = HW_SP_W / 2, cy = HW_SP_H / 2, r2 = 6 * 6;

    for (sy = 0; sy < HW_SP_H; sy++) {
        for (sx = 0; sx < HW_SP_W; sx++) {
            int d = (sx - cx) * (sx - cx) + (sy - cy) * (sy - cy);

            pix[sy * HW_SP_W + sx] = (d <= r2) ? fill : 0;
        }
    }
    hw_sprite_pat(pat, pix);
}

static void bounce_pair(int row, int ia, int ib)
{
    float dx, rel;

    if (!hw_hit(ia, ib))
        return;
    dx = x[row][1] - x[row][0];
    rel = (vx[row][1] - vx[row][0]) * dx;
    if (rel < 0.0f) {
        float t = vx[row][0];

        vx[row][0] = vx[row][1];
        vx[row][1] = t;
    }
}

static void tick(float dt)
{
    int row, k;
    int show_mask;
    float vw = (float)HW_VIEW_W;

    if (!started) {
        map_tileset();
        map_fill();
        make_spike(PAT_SPIKE, HW_PAL(3, 3, 0), HW_PAL(3, 3, 3));
        make_core(PAT_CORE, HW_PAL(3, 0, 0));
        x[0][0] = 16;
        x[0][1] = vw - 48;
        x[1][0] = 16;
        x[1][1] = vw - 48;
        vx[0][0] = 70;
        vx[0][1] = -70;
        vx[1][0] = 70;
        vx[1][1] = -70;
        started = 1;
    }

    map_scroll_keys(dt);
    show += dt;
    show_mask = ((int)show % 4) >= 2;

    for (row = 0; row < 2; row++) {
        int y = row ? 110 : 16;

        for (k = 0; k < 2; k++) {
            x[row][k] += vx[row][k] * dt;
            if (x[row][k] < 0) {
                x[row][k] = 0;
                vx[row][k] = -vx[row][k];
            } else if (x[row][k] > vw - HW_SP_W) {
                x[row][k] = vw - HW_SP_W;
                vx[row][k] = -vx[row][k];
            }
        }
        if (row == 0) {
            hw_sprite(0, (int)(x[0][0] + 0.5f), y, PAT_SPIKE);
            hw_sprite(1, (int)(x[0][1] + 0.5f), y, PAT_SPIKE);
            bounce_pair(0, 0, 1);
        } else {
            hw_sprite(2, (int)(x[1][0] + 0.5f), y, PAT_SPIKE);
            hw_sprite(3, (int)(x[1][1] + 0.5f), y, PAT_SPIKE);
            hw_sprite(4, (int)(x[1][0] + 0.5f), y, PAT_CORE);
            hw_sprite(5, (int)(x[1][1] + 0.5f), y, PAT_CORE);
            hw_sprite_draw(4, show_mask);
            hw_sprite_draw(5, show_mask);
            bounce_pair(1, 4, 5);
        }
    }

    hw_swap();
}

int main(void)
{
    return hw_main(tick);
}
