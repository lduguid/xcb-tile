#include "bank.h"

#include <string.h>

enum { SCROLL_PX_S = 140 };

void bank_upload(const Bank *b)
{
    int i;

    if (!b)
        return;
    for (i = 0; i < HW_COLORS; i++)
        hw_palette(i, b->pal[i]);
    for (i = 0; i < HW_TILES; i++)
        hw_tile(i, b->tiles[i]);
    for (i = 0; i < HW_SP_PATS; i++)
        hw_sprite_pat(i, b->sprites[i]);
}

static void stamp_cell(const Bank *b, int col, int row)
{
    int ox = (hw_cam_x() - hw_fine_x()) / HW_TILE;
    int oy = (hw_cam_y() - hw_fine_y()) / HW_TILE;

    hw_put(col, row, bank_at(b, ox + col, oy + row));
}

void bank_paint_view(const Bank *b)
{
    int x, y;

    for (y = 0; y < HW_MAP_H; y++)
        for (x = 0; x < HW_MAP_W; x++)
            stamp_cell(b, x, y);
}

void bank_paint_col(const Bank *b, int col)
{
    int y;

    for (y = 0; y < HW_MAP_H; y++)
        stamp_cell(b, col, y);
}

void bank_paint_row(const Bank *b, int row)
{
    int x;

    for (x = 0; x < HW_MAP_W; x++)
        stamp_cell(b, x, row);
}

void bank_apply_coarse(const Bank *b, HwCoarse c)
{
    if (c.coarse_x > 1 || c.coarse_x < -1 || c.coarse_y > 1 || c.coarse_y < -1) {
        bank_paint_view(b);
        return;
    }
    if (c.coarse_x > 0)
        bank_paint_col(b, HW_MAP_W - 1);
    else if (c.coarse_x < 0)
        bank_paint_col(b, 0);
    if (c.coarse_y > 0)
        bank_paint_row(b, HW_MAP_H - 1);
    else if (c.coarse_y < 0)
        bank_paint_row(b, 0);
}

void bank_scroll_keys(const Bank *b, float dt)
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
    bank_apply_coarse(b, c);
}

void bank_scroll_to(const Bank *b, int cam_x, int cam_y)
{
    int dx = cam_x - hw_cam_x();
    int dy = cam_y - hw_cam_y();

    if (dx || dy)
        bank_apply_coarse(b, hw_scroll(dx, dy));
}
