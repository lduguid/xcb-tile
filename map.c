#include "map.h"

enum { SCROLL_PX_S = 140 };

static unsigned uhash(int x, int y)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;

    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static void stamp(int id, const char *rows, uint8_t fg, uint8_t bg)
{
    uint8_t pix[HW_TILE * HW_TILE];
    int i;

    for (i = 0; i < HW_TILE * HW_TILE; i++)
        pix[i] = (rows[i] == '#') ? fg : bg;
    hw_tile(id, pix);
}

void map_tileset(void)
{
    stamp(T_SKY,
          "        "
          "        "
          "        "
          "        "
          "        "
          "        "
          "        "
          "        ",
          HW_PAL(1, 2, 3), HW_PAL(1, 2, 3));
    stamp(T_CLOUD,
          "        "
          "  ####  "
          " ###### "
          "########"
          " ###### "
          "        "
          "        "
          "        ",
          HW_PAL(3, 3, 3), HW_PAL(1, 2, 3));
    stamp(T_STAR,
          "        "
          "   #    "
          "  ###   "
          " #####  "
          "  ###   "
          "   #    "
          "        "
          "        ",
          HW_PAL(3, 3, 1), HW_PAL(0, 0, 1));
    stamp(T_GRASS,
          "# # # # "
          "########"
          "########"
          "########"
          "########"
          "########"
          "########"
          "########",
          HW_PAL(1, 3, 0), HW_PAL(0, 2, 0));
    stamp(T_DIRT,
          "########"
          "## # ###"
          "########"
          "### ## #"
          "########"
          "# ## ###"
          "########"
          "########",
          HW_PAL(2, 1, 0), HW_PAL(1, 0, 0));
    stamp(T_STONE,
          "########"
          "#  ##  #"
          "#  ##  #"
          "########"
          "########"
          "#  ##  #"
          "#  ##  #"
          "########",
          HW_PAL(2, 2, 2), HW_PAL(1, 1, 1));
    stamp(T_WATER,
          "  #  #  "
          "#  #  # "
          "  #  #  "
          "#  #  # "
          "  #  #  "
          "#  #  # "
          "  #  #  "
          "#  #  # ",
          HW_PAL(1, 2, 3), HW_PAL(0, 1, 3));
    stamp(T_BRICK,
          "########"
          "#      #"
          "########"
          "   ##   "
          "########"
          "#      #"
          "########"
          "########",
          HW_PAL(3, 1, 0), HW_PAL(2, 0, 0));
}

int map_at(int tx, int ty)
{
    unsigned h = uhash(tx, ty);
    int hill = 8 + (int)(uhash(tx, 0) % 9);
    int sea = 22;

    if (ty >= sea)
        return T_WATER;
    if (ty > hill + 4)
        return T_STONE;
    if (ty > hill + 1)
        return T_DIRT;
    if (ty == hill + 1)
        return T_GRASS;
    if (ty == hill && (h & 7) == 0)
        return T_BRICK;
    if (ty < 3 && (h & 0x3f) == 1)
        return T_STAR;
    if (ty < hill - 2 && (h & 0x1f) == 3)
        return T_CLOUD;
    return T_SKY;
}

static void stamp_cell(int col, int row)
{
    int ox = (hw_cam_x() - hw_fine_x()) / HW_TILE;
    int oy = (hw_cam_y() - hw_fine_y()) / HW_TILE;

    hw_put(col, row, map_at(ox + col, oy + row));
}

void map_fill(void)
{
    int x, y;

    for (y = 0; y < HW_MAP_H; y++)
        for (x = 0; x < HW_MAP_W; x++)
            stamp_cell(x, y);
}

void map_fill_col(int col)
{
    int y;

    for (y = 0; y < HW_MAP_H; y++)
        stamp_cell(col, y);
}

void map_fill_row(int row)
{
    int x;

    for (x = 0; x < HW_MAP_W; x++)
        stamp_cell(x, row);
}

void map_apply_coarse(HwCoarse c)
{
    if (c.coarse_x > 1 || c.coarse_x < -1 || c.coarse_y > 1 || c.coarse_y < -1) {
        map_fill();
        return;
    }
    if (c.coarse_x > 0)
        map_fill_col(HW_MAP_W - 1);
    else if (c.coarse_x < 0)
        map_fill_col(0);
    if (c.coarse_y > 0)
        map_fill_row(HW_MAP_H - 1);
    else if (c.coarse_y < 0)
        map_fill_row(0);
}

void map_scroll_keys(float dt)
{
    static float acc, acc_x, acc_y;
    static int prev_x, prev_y;
    int dir_x = hw_key_down(KEY_RIGHT) - hw_key_down(KEY_LEFT);
    int dir_y = hw_key_down(KEY_DOWN) - hw_key_down(KEY_UP);
    int step, step_x, step_y;
    HwCoarse c = {0, 0};

    if (dir_x != prev_x) {
        acc_x = 0;
        acc = 0;
        prev_x = dir_x;
    }
    if (dir_y != prev_y) {
        acc_y = 0;
        acc = 0;
        prev_y = dir_y;
    }
    if (!dir_x)
        acc_x = 0;
    if (!dir_y)
        acc_y = 0;
    if (!dir_x || !dir_y)
        acc = 0;

    if (dir_x && dir_y) {
        acc += SCROLL_PX_S * dt;
        step = (int)acc;
        acc -= (float)step;
        if (step)
            c = hw_scroll(dir_x * step, dir_y * step);
    } else {
        acc_x += (float)dir_x * SCROLL_PX_S * dt;
        acc_y += (float)dir_y * SCROLL_PX_S * dt;
        step_x = (int)acc_x;
        step_y = (int)acc_y;
        acc_x -= (float)step_x;
        acc_y -= (float)step_y;
        if (step_x || step_y)
            c = hw_scroll(step_x, step_y);
    }
    map_apply_coarse(c);
}
