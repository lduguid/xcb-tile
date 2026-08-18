/* Tiled world. Arrows/WASD fine-scroll; after 8 pixels the nametable
 * shifts one cell and only the new column/row is stamped. */

#include "hw.h"
#include "map.h"

static int started;

static void tick(float dt)
{
    if (!started) {
        map_tileset();
        map_fill();
        started = 1;
    }
    map_scroll_keys(dt);
    hw_swap();
}

int main(void)
{
    return hw_main(tick);
}
