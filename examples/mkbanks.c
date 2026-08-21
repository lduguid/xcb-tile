/* Build example .bank levels for xcb-tile-bank. */

#include "bank.h"

#include <stdio.h>
#include <string.h>

#define C(r, g, b) ((uint8_t)HW_PAL((r), (g), (b)))

static void t4(Bank *bank, int id, const char *p, uint8_t a, uint8_t c1, uint8_t c2, uint8_t c3)
{
    int i;

    if ((unsigned)id >= (unsigned)HW_TILES || !p)
        return;
    for (i = 0; i < BANK_TILE_PIX; i++) {
        char ch = p[i];
        uint8_t v = a;

        if (ch == '#')
            v = c1;
        else if (ch == '+')
            v = c2;
        else if (ch == 'o')
            v = c3;
        bank->tiles[id][i] = v;
    }
}

static void clear_map(Bank *b, int id)
{
    int i, n = b->map_w * b->map_h;

    for (i = 0; i < n; i++)
        b->map[i] = (uint8_t)id;
}

static void rect(Bank *b, int x, int y, int w, int h, int id)
{
    int i, j;

    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            bank_put(b, x + i, y + j, id);
}

static void hrun(Bank *b, int x, int y, int n, int id) { rect(b, x, y, n, 1, id); }
static void vrun(Bank *b, int x, int y, int n, int id) { rect(b, x, y, 1, n, id); }

static void blob(Bank *b, int pat, uint8_t fill, uint8_t rim, int radius)
{
    int x, y, cx = HW_SP_W / 2, cy = HW_SP_H / 2, r2, inner;

    r2 = radius * radius;
    inner = (radius - 3) * (radius - 3);
    if (inner < 1)
        inner = 1;
    memset(b->sprites[pat], 0, BANK_SP_PIX);
    for (y = 0; y < HW_SP_H; y++) {
        for (x = 0; x < HW_SP_W; x++) {
            int d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (d <= r2)
                b->sprites[pat][y * HW_SP_W + x] = d >= inner ? rim : fill;
        }
    }
}

/* ---- Mario ---------------------------------------------------------- */

enum {
    M_SKY = 0, M_CLOUD, M_BUSH, M_GTOP, M_DIRT, M_BRICK, M_Q, M_USED,
    M_PIPE_L, M_PIPE_R, M_PIPE_TL, M_PIPE_TR, M_COIN, M_HARD, M_POLE, M_FLAG,
    M_CASTLE, M_CDOOR, M_BATT, M_HILL, M_WATER, M_WTOP
};

static void mario_tiles(Bank *b)
{
    uint8_t sky = C(1, 2, 3), wh = C(3, 3, 3), gr = C(0, 2, 0), gr2 = C(1, 3, 0);
    uint8_t brn = C(2, 1, 0), brn2 = C(1, 0, 0), red = C(3, 1, 0), yel = C(3, 3, 0);
    uint8_t blk = C(0, 0, 0), pgr = C(0, 3, 0), pdk = C(0, 1, 0), gold = C(3, 2, 0);
    uint8_t gry = C(2, 2, 2), dgry = C(1, 1, 1), blu = C(0, 1, 3);

    t4(b, M_SKY, "                                                                ", sky, sky, sky, sky);
    t4(b, M_CLOUD,
       "        "
       "  ####  "
       " ###### "
       "########"
       " ###### "
       "  ####  "
       "        "
       "        ", sky, wh, sky, sky);
    t4(b, M_BUSH,
       "        "
       "        "
       "   ##   "
       "  ####  "
       " ###### "
       "########"
       "## #### "
       "        ", sky, gr2, gr, sky);
    t4(b, M_GTOP,
       "#+##+#+#"
       "########"
       "########"
       "########"
       "oo######"
       "########"
       "########"
       "########", brn, gr2, gr, brn2);
    t4(b, M_DIRT,
       "########"
       "##o###o#"
       "########"
       "o###o###"
       "########"
       "##o#####"
       "########"
       "#####o##", brn, brn, brn2, brn2);
    t4(b, M_BRICK,
       "########"
       "#      #"
       "########"
       "   ##   "
       "########"
       "#      #"
       "########"
       "########", red, blk, red, red);
    t4(b, M_Q,
       "########"
       "#++++++#"
       "#++oo++#"
       "#++oo++#"
       "#++++++#"
       "#++oo++#"
       "#++++++#"
       "########", blk, gold, yel, wh);
    t4(b, M_USED,
       "########"
       "#oooooo#"
       "#oooooo#"
       "#oooooo#"
       "#oooooo#"
       "#oooooo#"
       "#oooooo#"
       "########", blk, dgry, gry, gry);
    t4(b, M_PIPE_TL,
       "        "
       "########"
       "########"
       "##oooooo"
       "##oooooo"
       "##oooooo"
       "##oooooo"
       "##oooooo", sky, pgr, pdk, pdk);
    t4(b, M_PIPE_TR,
       "        "
       "########"
       "########"
       "oooooo##"
       "oooooo##"
       "oooooo##"
       "oooooo##"
       "oooooo##", sky, pgr, pdk, pdk);
    t4(b, M_PIPE_L,
       "##oooooo"
       "##oooooo"
       "##oooooo"
       "##oooooo"
       "##oooooo"
       "##oooooo"
       "##oooooo"
       "##oooooo", pdk, pgr, pdk, pdk);
    t4(b, M_PIPE_R,
       "oooooo##"
       "oooooo##"
       "oooooo##"
       "oooooo##"
       "oooooo##"
       "oooooo##"
       "oooooo##"
       "oooooo##", pdk, pgr, pdk, pdk);
    t4(b, M_COIN,
       "        "
       "  ++++  "
       " +oooo+ "
       " +oooo+ "
       " +oooo+ "
       "  ++++  "
       "        "
       "        ", sky, gold, yel, wh);
    t4(b, M_HARD,
       "########"
       "#++++++#"
       "#++++++#"
       "#++++++#"
       "#++++++#"
       "#++++++#"
       "#++++++#"
       "########", blk, gry, dgry, dgry);
    t4(b, M_POLE,
       "   ##   "
       "   ##   "
       "   ##   "
       "   ##   "
       "   ##   "
       "   ##   "
       "   ##   "
       "   ##   ", sky, wh, sky, sky);
    t4(b, M_FLAG,
       "##++++++"
       "##++++++"
       "##++++++"
       "##++++  "
       "##++    "
       "##      "
       "##      "
       "##      ", sky, pgr, gr2, sky);
    t4(b, M_CASTLE,
       "########"
       "#  ##  #"
       "#  ##  #"
       "########"
       "########"
       "#  ##  #"
       "#  ##  #"
       "########", dgry, gry, blk, blk);
    t4(b, M_CDOOR,
       "########"
       "##oooo##"
       "##oooo##"
       "##oooo##"
       "##oooo##"
       "##oooo##"
       "##oooo##"
       "##oooo##", dgry, gry, blk, blk);
    t4(b, M_BATT,
       "##  ##  "
       "##  ##  "
       "########"
       "########"
       "########"
       "########"
       "########"
       "########", dgry, gry, blk, blk);
    t4(b, M_HILL,
       "        "
       "        "
       "   ##   "
       "  ####  "
       " ###### "
       "########"
       "########"
       "########", sky, gr, gr2, sky);
    t4(b, M_WATER,
       "  #  #  "
       "#  #  # "
       "  #  #  "
       "#  #  # "
       "  #  #  "
       "#  #  # "
       "  #  #  "
       "#  #  # ", blu, C(1, 2, 3), blu, blu);
    t4(b, M_WTOP,
       " o o o o"
       "  #  #  "
       "#  #  # "
       "  #  #  "
       "#  #  # "
       "  #  #  "
       "#  #  # "
       "  #  #  ", blu, C(0, 2, 3), wh, blu);
    blob(b, 0, C(3, 0, 0), C(3, 3, 3), 12);
    blob(b, 1, C(2, 1, 0), C(0, 0, 0), 10);
    blob(b, 2, C(3, 0, 0), C(3, 3, 3), 8);
    memset(b->solid, 0, sizeof(b->solid));
    b->solid[M_GTOP] = b->solid[M_DIRT] = b->solid[M_BRICK] = 1;
    b->solid[M_Q] = b->solid[M_USED] = b->solid[M_HARD] = 1;
    b->solid[M_PIPE_L] = b->solid[M_PIPE_R] = b->solid[M_PIPE_TL] = b->solid[M_PIPE_TR] = 1;
    b->solid[M_CASTLE] = b->solid[M_CDOOR] = b->solid[M_BATT] = 1;
}

static void mario_ground(Bank *b, int x0, int x1, int gy)
{
    int x, y;

    for (x = x0; x <= x1 && x < b->map_w; x++) {
        if (x < 0)
            continue;
        bank_put(b, x, gy, M_GTOP);
        for (y = gy + 1; y < b->map_h; y++)
            bank_put(b, x, y, M_DIRT);
    }
}

static void mario_pipe(Bank *b, int x, int top, int gy)
{
    int y;

    bank_put(b, x, top, M_PIPE_TL);
    bank_put(b, x + 1, top, M_PIPE_TR);
    for (y = top + 1; y < gy; y++) {
        bank_put(b, x, y, M_PIPE_L);
        bank_put(b, x + 1, y, M_PIPE_R);
    }
}

static void mario_stairs(Bank *b, int x, int gy, int n, int dir)
{
    int i, j;

    for (i = 0; i < n; i++)
        for (j = 0; j <= i; j++)
            bank_put(b, x + i * dir, gy - j, M_HARD);
}

static void build_mario(Bank *b)
{
    int gy = 24, x, i;

    bank_resize_map(b, 176, 32);
    clear_map(b, M_SKY);
    mario_tiles(b);

    for (i = 0; i < 12; i++)
        bank_put(b, 6 + i * 14, 3 + (i % 3), M_CLOUD);
    bank_put(b, 8, gy - 1, M_BUSH);
    bank_put(b, 9, gy - 1, M_BUSH);
    bank_put(b, 22, gy - 1, M_HILL);
    bank_put(b, 50, gy - 1, M_BUSH);
    bank_put(b, 88, gy - 1, M_HILL);
    bank_put(b, 89, gy - 1, M_BUSH);

    mario_ground(b, 0, 18, gy);
    mario_pipe(b, 14, gy - 3, gy);

    for (x = 26; x <= 30; x++)
        bank_put(b, x, gy - 6, M_COIN);
    mario_ground(b, 31, 58, gy);
    hrun(b, 34, gy - 5, 4, M_BRICK);
    bank_put(b, 36, gy - 5, M_Q);
    hrun(b, 42, gy - 9, 3, M_Q);
    bank_put(b, 43, gy - 9, M_BRICK);
    mario_pipe(b, 52, gy - 4, gy);

    mario_ground(b, 59, 62, gy);
    for (x = 63; x <= 72; x++) {
        bank_put(b, x, gy + 2, M_WTOP);
        rect(b, x, gy + 3, 1, b->map_h - (gy + 3), M_WATER);
    }
    hrun(b, 64, gy - 1, 8, M_BRICK);
    bank_put(b, 67, gy - 5, M_COIN);
    bank_put(b, 68, gy - 5, M_COIN);
    mario_ground(b, 73, 110, gy);
    mario_pipe(b, 78, gy - 2, gy);
    mario_pipe(b, 84, gy - 5, gy);
    hrun(b, 90, gy - 8, 5, M_BRICK);
    bank_put(b, 92, gy - 8, M_Q);
    bank_put(b, 93, gy - 12, M_COIN);
    mario_stairs(b, 100, gy, 5, 1);
    mario_stairs(b, 110, gy, 4, -1);

    mario_ground(b, 111, 118, gy);
    mario_ground(b, 126, 175, gy);
    hrun(b, 120, gy - 4, 5, M_HARD);
    bank_put(b, 122, gy - 8, M_Q);
    mario_stairs(b, 148, gy, 8, 1);
    vrun(b, 157, 8, gy - 8, M_POLE);
    bank_put(b, 157, 8, M_FLAG);
    bank_put(b, 156, 8, M_FLAG);
    rect(b, 164, gy - 8, 8, 8, M_CASTLE);
    hrun(b, 164, gy - 9, 8, M_BATT);
    bank_put(b, 167, gy - 2, M_CDOOR);
    bank_put(b, 168, gy - 2, M_CDOOR);
    bank_put(b, 167, gy - 1, M_CDOOR);
    bank_put(b, 168, gy - 1, M_CDOOR);
}

/* ---- Commando ------------------------------------------------------- */

enum {
    K_GRASS = 0, K_TREE, K_TRUNK, K_ROAD, K_ROAD2, K_WATER, K_SHORE, K_BUSH,
    K_CRATE, K_BUNKER, K_BAG, K_ROCK, K_BRIDGE, K_JUNGLE, K_BARREL, K_ROOF,
    K_WALL, K_FENCE, K_PAD
};

static void commando_tiles(Bank *b)
{
    uint8_t g0 = C(0, 2, 0), g1 = C(1, 3, 0), g2 = C(0, 1, 0);
    uint8_t rd = C(2, 1, 0), yel = C(3, 2, 0), blu = C(0, 1, 3), blu2 = C(0, 2, 3);
    uint8_t brn = C(2, 1, 0), gry = C(2, 2, 2), dgry = C(1, 1, 1), red = C(3, 0, 0);
    uint8_t tan = C(3, 2, 1), blk = C(0, 0, 0), dk = C(0, 1, 0);

    t4(b, K_GRASS,
       ".#.#.#.#"
       "#.#.#.#."
       ".#.#.#.#"
       "#.#.#.#."
       ".#.#.#.#"
       "#.#.#.#."
       ".#.#.#.#"
       "#.#.#.#.", g0, g1, g0, g0);
    t4(b, K_TREE,
       "  ####  "
       " ###### "
       "########"
       "########"
       " ###### "
       "  ####  "
       "   ++   "
       "   ++   ", g0, g2, brn, g0);
    t4(b, K_TRUNK,
       "        "
       "        "
       "   ++   "
       "   ++   "
       "   ++   "
       "   ++   "
       "   ++   "
       "   ++   ", g0, brn, C(1, 0, 0), g0);
    t4(b, K_ROAD,
       "########"
       "########"
       "########"
       "########"
       "########"
       "########"
       "########"
       "########", rd, rd, rd, rd);
    t4(b, K_ROAD2,
       "########"
       "###++###"
       "########"
       "########"
       "###++###"
       "########"
       "########"
       "########", rd, rd, yel, rd);
    t4(b, K_WATER,
       "  #  #  "
       "#  #  # "
       "  #  #  "
       "#  #  # "
       "  #  #  "
       "#  #  # "
       "  #  #  "
       "#  #  # ", blu, blu2, blu, blu);
    t4(b, K_SHORE,
       "########"
       "###oo###"
       "##oooo##"
       "#oooooo#"
       "oooooooo"
       "  #  #  "
       "#  #  # "
       "  #  #  ", blu, tan, rd, blu2);
    t4(b, K_BUSH,
       "        "
       "  ####  "
       " ###### "
       "########"
       " ###### "
       "  ####  "
       "        "
       "        ", g0, g2, g1, g0);
    t4(b, K_CRATE,
       "########"
       "#++++++#"
       "#+####+#"
       "#++++++#"
       "#+####+#"
       "#++++++#"
       "#++++++#"
       "########", dgry, brn, yel, brn);
    t4(b, K_BUNKER,
       "########"
       "#      #"
       "# #### #"
       "# #  # #"
       "# #### #"
       "#      #"
       "#      #"
       "########", dgry, gry, blk, dgry);
    t4(b, K_BAG,
       "        "
       " ###### "
       "########"
       "########"
       "########"
       " ###### "
       "        "
       "        ", g0, tan, brn, g0);
    t4(b, K_ROCK,
       "        "
       "  ####  "
       " ###### "
       "########"
       " ###### "
       "  ####  "
       "        "
       "        ", g0, gry, dgry, g0);
    t4(b, K_BRIDGE,
       "++  ++  "
       "++++++++"
       "++  ++  "
       "++++++++"
       "++  ++  "
       "++++++++"
       "++  ++  "
       "++++++++", blu, brn, yel, blu);
    t4(b, K_JUNGLE,
       "########"
       "##o##o##"
       "########"
       "o######o"
       "########"
       "##o#####"
       "########"
       "#####o##", dk, g2, C(0, 3, 0), dk);
    t4(b, K_BARREL,
       "  ####  "
       " #++++# "
       " #++++# "
       " ###### "
       " #++++# "
       " #++++# "
       " #++++# "
       "  ####  ", g0, dgry, red, g0);
    t4(b, K_ROOF,
       "########"
       "#++++++#"
       "#++++++#"
       "#++++++#"
       "#++++++#"
       "#++++++#"
       "#++++++#"
       "########", dgry, red, C(2, 0, 0), dgry);
    t4(b, K_WALL,
       "########"
       "#oooooo#"
       "########"
       "#oooooo#"
       "########"
       "#oooooo#"
       "########"
       "#oooooo#", dgry, gry, tan, dgry);
    t4(b, K_FENCE,
       "# # # # "
       "# # # # "
       "########"
       "# # # # "
       "# # # # "
       "########"
       "# # # # "
       "# # # # ", g0, brn, g0, g0);
    t4(b, K_PAD,
       "########"
       "#++++++#"
       "#+    +#"
       "#+    +#"
       "#+    +#"
       "#+    +#"
       "#++++++#"
       "########", g0, gry, yel, g0);
    blob(b, 0, C(0, 2, 0), C(2, 1, 0), 11);
    blob(b, 1, C(3, 1, 0), C(0, 0, 0), 10);
    blob(b, 2, C(2, 2, 0), C(1, 1, 1), 9);
    memset(b->solid, 0, sizeof(b->solid));
    b->solid[K_TREE] = b->solid[K_TRUNK] = b->solid[K_WATER] = 1;
    b->solid[K_BUSH] = b->solid[K_CRATE] = b->solid[K_BUNKER] = 1;
    b->solid[K_BAG] = b->solid[K_ROCK] = b->solid[K_JUNGLE] = 1;
    b->solid[K_BARREL] = b->solid[K_ROOF] = b->solid[K_WALL] = 1;
    b->solid[K_FENCE] = 1;
}

static void grove(Bank *b, int x, int y, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        bank_put(b, x + (i * 3) % 7, y + (i * 2) % 5, K_TREE);
        bank_put(b, x + (i * 5) % 6, y + 1 + (i % 4), K_BUSH);
    }
}

static void build_commando(Bank *b)
{
    int x, y;

    bank_resize_map(b, 56, 80);
    clear_map(b, K_GRASS);
    commando_tiles(b);

    /* LZ at the top of the valley */
    rect(b, 18, 2, 8, 6, K_PAD);
    hrun(b, 16, 8, 12, K_FENCE);
    bank_put(b, 20, 9, K_CRATE);
    bank_put(b, 22, 9, K_BARREL);
    bank_put(b, 24, 9, K_CRATE);

    /* dirt road snaking south */
    for (y = 10; y < 28; y++)
        hrun(b, 22 + (y % 5) - 2, y, 4, (y & 2) ? K_ROAD2 : K_ROAD);

    grove(b, 4, 4, 8);
    grove(b, 40, 6, 10);
    grove(b, 2, 16, 6);

    /* river + bridge */
    for (y = 28; y <= 32; y++)
        hrun(b, 0, y, b->map_w, K_WATER);
    hrun(b, 0, 27, b->map_w, K_SHORE);
    hrun(b, 0, 33, b->map_w, K_SHORE);
    for (x = 24; x <= 29; x++)
        vrun(b, x, 27, 7, K_BRIDGE);

    /* road continues */
    for (y = 34; y < 48; y++)
        hrun(b, 20, y, 5, (y & 2) ? K_ROAD2 : K_ROAD);

    grove(b, 2, 36, 12);
    grove(b, 36, 38, 14);
    rect(b, 0, 34, 8, 20, K_JUNGLE);
    rect(b, 48, 34, 8, 18, K_JUNGLE);

    /* bunker camp */
    rect(b, 16, 50, 10, 6, K_WALL);
    rect(b, 17, 51, 8, 4, K_ROOF);
    bank_put(b, 20, 54, K_BUNKER);
    bank_put(b, 21, 54, K_BUNKER);
    hrun(b, 14, 49, 14, K_BAG);
    bank_put(b, 15, 48, K_BARREL);
    bank_put(b, 28, 51, K_CRATE);
    bank_put(b, 30, 52, K_CRATE);
    bank_put(b, 29, 53, K_BARREL);

    for (y = 56; y < 68; y++)
        hrun(b, 18 + ((y / 2) & 3), y, 4, K_ROAD);

    grove(b, 4, 58, 10);
    grove(b, 38, 60, 8);
    bank_put(b, 10, 62, K_ROCK);
    bank_put(b, 44, 64, K_ROCK);
    bank_put(b, 12, 70, K_ROCK);

    /* south compound */
    rect(b, 20, 70, 12, 8, K_WALL);
    rect(b, 21, 71, 10, 6, K_ROOF);
    hrun(b, 18, 69, 16, K_FENCE);
    bank_put(b, 25, 74, K_BUNKER);
    bank_put(b, 26, 74, K_PAD);
}

/* ---- Surprise: lunar wreck ------------------------------------------ */

enum {
    L_SPACE = 0, L_STAR, L_STAR2, L_DUST, L_ROCK, L_ROCK2, L_METAL, L_GRATE,
    L_HULL, L_STRIPE, L_PAD, L_LIGHT, L_CRATER, L_PANEL, L_PORT, L_GLOW,
    L_SOLAR, L_LADDER, L_CARGO, L_EARTH
};

static void lunar_tiles(Bank *b)
{
    uint8_t sp = C(0, 0, 0), gry = C(2, 2, 2), dgry = C(1, 1, 1), wh = C(3, 3, 3);
    uint8_t cyn = C(0, 3, 3), blu = C(0, 1, 3), yel = C(3, 3, 0), red = C(3, 0, 0);
    uint8_t org = C(3, 1, 0);

    t4(b, L_SPACE, "                                                                ", sp, sp, sp, sp);
    t4(b, L_STAR,
       "        "
       "   #    "
       "  ###   "
       " #####  "
       "  ###   "
       "   #    "
       "        "
       "        ", sp, wh, sp, sp);
    t4(b, L_STAR2,
       "        "
       "        "
       "   #    "
       "  ###   "
       "   #    "
       "        "
       "        "
       "        ", sp, gry, sp, sp);
    t4(b, L_DUST,
       "        "
       "  #     "
       "     #  "
       " #      "
       "    #   "
       "  #     "
       "      # "
       "        ", sp, dgry, sp, sp);
    t4(b, L_ROCK,
       "        "
       "  ####  "
       " ###### "
       "########"
       "########"
       " ###### "
       "  ####  "
       "        ", sp, gry, dgry, sp);
    t4(b, L_ROCK2,
       "        "
       "   ##   "
       "  ####  "
       " ###### "
       "  ####  "
       "   ##   "
       "        "
       "        ", sp, dgry, gry, sp);
    t4(b, L_METAL,
       "########"
       "#++++++#"
       "#++++++#"
       "#++++++#"
       "#++++++#"
       "#++++++#"
       "#++++++#"
       "########", dgry, gry, dgry, dgry);
    t4(b, L_GRATE,
       "# # # # "
       " # # # #"
       "# # # # "
       " # # # #"
       "# # # # "
       " # # # #"
       "# # # # "
       " # # # #", dgry, gry, dgry, dgry);
    t4(b, L_HULL,
       "########"
       "##oo####"
       "########"
       "####oo##"
       "########"
       "##oo####"
       "########"
       "########", dgry, gry, C(1, 1, 2), dgry);
    t4(b, L_STRIPE,
       "++oooo++"
       "++oooo++"
       "++oooo++"
       "++oooo++"
       "++oooo++"
       "++oooo++"
       "++oooo++"
       "++oooo++", dgry, yel, red, dgry);
    t4(b, L_PAD,
       "########"
       "#++++++#"
       "#+    +#"
       "#+ ++ +#"
       "#+ ++ +#"
       "#+    +#"
       "#++++++#"
       "########", dgry, gry, cyn, dgry);
    t4(b, L_LIGHT,
       "        "
       "  oooo  "
       " oooooo "
       "oooooooo"
       " oooooo "
       "  oooo  "
       "        "
       "        ", sp, cyn, wh, cyn);
    t4(b, L_CRATER,
       "        "
       "  ####  "
       " #oooo# "
       "#oooooo#"
       "#oooooo#"
       " #oooo# "
       "  ####  "
       "        ", sp, gry, dgry, sp);
    t4(b, L_PANEL,
       "########"
       "#oooooo#"
       "#o####o#"
       "#o#++#o#"
       "#o####o#"
       "#oooooo#"
       "#oooooo#"
       "########", dgry, gry, cyn, dgry);
    t4(b, L_PORT,
       "########"
       "#      #"
       "# ++++ #"
       "# +oo+ #"
       "# +oo+ #"
       "# ++++ #"
       "#      #"
       "########", dgry, blu, C(1, 2, 3), wh);
    t4(b, L_GLOW,
       "        "
       "   ++   "
       "  ++++  "
       " ++++++ "
       "  ++++  "
       "   ++   "
       "        "
       "        ", sp, cyn, wh, sp);
    t4(b, L_SOLAR,
       "########"
       "#++++++#"
       "########"
       "#++++++#"
       "########"
       "#++++++#"
       "########"
       "#++++++#", dgry, blu, cyn, dgry);
    t4(b, L_LADDER,
       "#      #"
       "# #### #"
       "#      #"
       "# #### #"
       "#      #"
       "# #### #"
       "#      #"
       "# #### #", sp, gry, dgry, sp);
    t4(b, L_CARGO,
       "########"
       "#++++++#"
       "#+oooo+#"
       "#++++++#"
       "#+oooo+#"
       "#++++++#"
       "#++++++#"
       "########", dgry, org, yel, dgry);
    t4(b, L_EARTH,
       "  ####  "
       " ###### "
       "########"
       "###++###"
       "##++++##"
       "###++###"
       " ###### "
       "  ####  ", sp, blu, C(0, 3, 0), C(1, 2, 3));
    blob(b, 0, C(3, 3, 3), C(0, 3, 3), 11); /* suit */
    blob(b, 1, C(0, 3, 0), C(3, 0, 3), 14); /* alien */
    blob(b, 2, C(2, 2, 2), C(3, 3, 0), 8);  /* probe */
    memset(b->solid, 0, sizeof(b->solid));
    b->solid[L_ROCK] = b->solid[L_ROCK2] = b->solid[L_METAL] = 1;
    b->solid[L_GRATE] = b->solid[L_HULL] = b->solid[L_STRIPE] = 1;
    b->solid[L_PAD] = b->solid[L_PANEL] = b->solid[L_PORT] = 1;
    b->solid[L_SOLAR] = b->solid[L_CARGO] = 1;
}

static void build_lunar(Bank *b)
{
    int x, y, i;

    bank_resize_map(b, 96, 40);
    clear_map(b, L_SPACE);
    lunar_tiles(b);

    for (i = 0; i < 40; i++) {
        bank_put(b, (i * 17 + 3) % b->map_w, (i * 5 + 1) % 12, (i & 1) ? L_STAR : L_STAR2);
        bank_put(b, (i * 13 + 8) % b->map_w, 1 + (i * 3) % 8, L_DUST);
    }
    bank_put(b, 8, 2, L_EARTH);
    bank_put(b, 9, 2, L_EARTH);

    /* moon surface */
    for (x = 0; x < b->map_w; x++) {
        int h = 22 + (x / 7) % 3 - (x / 11) % 2;

        bank_put(b, x, h, L_ROCK);
        for (y = h + 1; y < b->map_h; y++)
            bank_put(b, x, y, (y + x) & 1 ? L_ROCK2 : L_DUST);
        if (x % 11 == 3)
            bank_put(b, x, h - 1, L_CRATER);
    }

    /* crashed hull on the left */
    rect(b, 6, 14, 18, 8, L_HULL);
    hrun(b, 6, 14, 18, L_STRIPE);
    hrun(b, 6, 21, 18, L_STRIPE);
    vrun(b, 6, 14, 8, L_STRIPE);
    rect(b, 10, 16, 4, 3, L_GRATE);
    bank_put(b, 16, 17, L_PORT);
    bank_put(b, 17, 17, L_PORT);
    bank_put(b, 20, 16, L_PANEL);
    bank_put(b, 12, 13, L_LIGHT);
    bank_put(b, 22, 13, L_GLOW);
    vrun(b, 14, 22, 4, L_LADDER);

    /* landing pad + cargo */
    rect(b, 32, 18, 8, 4, L_PAD);
    bank_put(b, 31, 17, L_CARGO);
    bank_put(b, 40, 17, L_CARGO);
    bank_put(b, 41, 18, L_CARGO);
    hrun(b, 32, 17, 8, L_STRIPE);

    /* solar farm */
    for (x = 50; x < 70; x += 3)
        for (y = 16; y < 20; y++)
            bank_put(b, x, y, L_SOLAR);
    bank_put(b, 58, 15, L_LIGHT);
    bank_put(b, 64, 15, L_LIGHT);

    /* outpost */
    rect(b, 74, 12, 14, 10, L_METAL);
    hrun(b, 74, 12, 14, L_STRIPE);
    rect(b, 76, 14, 4, 3, L_GRATE);
    bank_put(b, 82, 15, L_PORT);
    bank_put(b, 83, 15, L_PANEL);
    bank_put(b, 78, 13, L_LIGHT);
    vrun(b, 80, 22, 3, L_LADDER);

    /* asteroid field on the right sky */
    for (i = 0; i < 10; i++)
        bank_put(b, 78 + (i % 6), 3 + i % 5, (i & 1) ? L_ROCK : L_ROCK2);
}

static int write_bank(const char *path, void (*build)(Bank *))
{
    Bank b;

    bank_init(&b);
    build(&b);
    if (!bank_save(path, &b)) {
        fprintf(stderr, "save failed: %s\n", path);
        bank_free(&b);
        return 0;
    }
    printf("wrote %s (%dx%d)\n", path, b.map_w, b.map_h);
    bank_free(&b);
    return 1;
}

int main(void)
{
    int ok = 1;

    ok &= write_bank("examples/mario.bank", build_mario);
    ok &= write_bank("examples/commando.bank", build_commando);
    ok &= write_bank("examples/surprise.bank", build_lunar);
    return ok ? 0 : 1;
}
