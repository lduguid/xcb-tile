/* Load a .bank from xcb-tile-edit and scroll it like the procedural map. */

#include "bank.h"

#include <stdio.h>

static Bank bank;
static int started;

static void tick(float dt)
{
    if (!started) {
        bank_upload(&bank);
        bank_paint_view(&bank);
        started = 1;
    }
    bank_scroll_keys(&bank, dt);
    hw_swap();
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "game.bank";

    bank_init(&bank);
    if (!bank_load(path, &bank)) {
        fprintf(stderr, "cannot load %s (save one from xcb-tile-edit)\n", path);
        bank_free(&bank);
        return 1;
    }
    if (hw_main(tick) != 0) {
        bank_free(&bank);
        return 1;
    }
    bank_free(&bank);
    return 0;
}
