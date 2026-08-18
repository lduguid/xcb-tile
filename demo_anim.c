/* Same tiled world, but the water charset is rewritten every few frames —
 * the C64 trick of animating tiles by changing chargen, not the nametable. */

#include "hw.h"
#include "map.h"

static int started;
static float anim;

static void water_frame(int frame)
{
    uint8_t pix[HW_TILE * HW_TILE];
    int x, y;
    uint8_t hi = (uint8_t)HW_PAL(1, 2, 3);
    uint8_t lo = (uint8_t)HW_PAL(0, 1, 3);

    for (y = 0; y < HW_TILE; y++) {
        int phase = (y + frame) & 3;

        for (x = 0; x < HW_TILE; x++)
            pix[y * HW_TILE + x] = ((x + phase) & 3) ? lo : hi;
    }
    hw_tile(T_WATER, pix);
}

static void tick(float dt)
{
    if (!started) {
        map_tileset();
        map_fill();
        started = 1;
    }
    anim += dt * 8.0f;
    water_frame((int)anim);
    map_scroll_keys(dt);
    hw_swap();
}

int main(void)
{
    return hw_main(tick);
}
