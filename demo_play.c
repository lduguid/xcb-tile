/* Play a .bank: arrows walk, space jumps (platform maps).
 * Commando-style maps: pass --walk for 4-way, no gravity. */

#include "play.h"

#include <stdio.h>
#include <string.h>

static Bank bank;
static Actor player;
static int started;
static int topdown;

static void tick(float dt)
{
    if (!started) {
        bank_upload(&bank);
        actor_spawn(&player, &bank);
        play_cam_follow(&bank, &player);
        bank_paint_view(&bank);
        started = 1;
    }
    if (topdown)
        actor_topdown(&player, &bank, dt);
    else
        actor_platform(&player, &bank, dt);
    play_cam_follow(&bank, &player);
    actor_draw(&player);
    hw_swap();
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--walk") == 0 || strcmp(argv[i], "-w") == 0)
            topdown = 1;
        else
            path = argv[i];
    }
    if (!path)
        path = "examples/mario.bank";

    bank_init(&bank);
    if (!bank_load(path, &bank)) {
        fprintf(stderr, "cannot load %s\n", path);
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
