#include "hw.h"

#include <stdlib.h>
#include <string.h>

enum { PIX = HW_TILE * HW_TILE, SP_PIX = HW_SP_W * HW_SP_H };

static Pixel pal[HW_COLORS];
static uint8_t tiles[HW_TILES][PIX];
static uint8_t map[HW_MAP_H][HW_MAP_W];
static uint8_t spat[HW_SP_PATS][SP_PIX];
static Pixel *vis;
static int fine_x, fine_y;
static int origin_tx, origin_ty;
static int keys[8];

typedef struct {
    int x, y, pat, on, behind, draw;
} Sprite;

static Sprite sp[HW_SPRITES];

int hw_view_width(void) { return HW_VIEW_W; }
int hw_view_height(void) { return HW_VIEW_H; }
int hw_map_width(void) { return HW_MAP_W; }
int hw_map_height(void) { return HW_MAP_H; }
int hw_fine_x(void) { return fine_x; }
int hw_fine_y(void) { return fine_y; }
int hw_cam_x(void) { return origin_tx * HW_TILE + fine_x; }
int hw_cam_y(void) { return origin_ty * HW_TILE + fine_y; }

void hw_key_set(int key, int down)
{
    if (key <= 0 || key >= 8)
        return;
    keys[key] = down;
    if (down) {
        if (key == KEY_LEFT)
            keys[KEY_RIGHT] = 0;
        else if (key == KEY_RIGHT)
            keys[KEY_LEFT] = 0;
        else if (key == KEY_UP)
            keys[KEY_DOWN] = 0;
        else if (key == KEY_DOWN)
            keys[KEY_UP] = 0;
    }
}

int hw_key_down(int key)
{
    if (key <= 0 || key >= 8)
        return 0;
    return keys[key];
}

static void default_palette(void)
{
    int i;

    for (i = 0; i < HW_COLORS; i++) {
        int r = (i % 4) * 85;
        int g = ((i / 4) % 4) * 85;
        int b = ((i / 16) % 4) * 85;

        pal[i] = HW_RGB(r, g, b);
    }
}

int hw_init(void)
{
    vis = calloc((size_t)HW_VIEW_W * (size_t)HW_VIEW_H, sizeof(Pixel));
    if (!vis)
        return 0;
    memset(tiles, 0, sizeof(tiles));
    memset(map, 0, sizeof(map));
    memset(spat, 0, sizeof(spat));
    memset(sp, 0, sizeof(sp));
    default_palette();
    fine_x = fine_y = 0;
    origin_tx = origin_ty = 0;
    return 1;
}

void hw_shutdown(void)
{
    free(vis);
    vis = NULL;
}

const Pixel *hw_visible(void)
{
    return vis;
}

void hw_palette(int index, Pixel color)
{
    if ((unsigned)index >= (unsigned)HW_COLORS)
        return;
    pal[index] = color;
}

void hw_tile(int id, const uint8_t pix[HW_TILE * HW_TILE])
{
    int i;

    if ((unsigned)id >= (unsigned)HW_TILES || !pix)
        return;
    for (i = 0; i < PIX; i++)
        tiles[id][i] = (uint8_t)(pix[i] & (HW_COLORS - 1));
}

void hw_put(int tx, int ty, int id)
{
    if ((unsigned)tx >= (unsigned)HW_MAP_W || (unsigned)ty >= (unsigned)HW_MAP_H)
        return;
    if ((unsigned)id >= (unsigned)HW_TILES)
        id = 0;
    map[ty][tx] = (uint8_t)id;
}

int hw_get(int tx, int ty)
{
    if ((unsigned)tx >= (unsigned)HW_MAP_W || (unsigned)ty >= (unsigned)HW_MAP_H)
        return 0;
    return map[ty][tx];
}

void hw_fill(int id)
{
    int x, y;

    if ((unsigned)id >= (unsigned)HW_TILES)
        id = 0;
    for (y = 0; y < HW_MAP_H; y++)
        for (x = 0; x < HW_MAP_W; x++)
            map[y][x] = (uint8_t)id;
}

void hw_set_fine(int x, int y)
{
    if (x < 0)
        x = 0;
    if (x >= HW_TILE)
        x = HW_TILE - 1;
    if (y < 0)
        y = 0;
    if (y >= HW_TILE)
        y = HW_TILE - 1;
    fine_x = x;
    fine_y = y;
}

void hw_reset(void)
{
    fine_x = fine_y = 0;
    origin_tx = origin_ty = 0;
    memset(map, 0, sizeof(map));
    memset(sp, 0, sizeof(sp));
}

static void coarse_x(int dir)
{
    int y;

    if (dir > 0) {
        for (y = 0; y < HW_MAP_H; y++) {
            memmove(&map[y][0], &map[y][1], (size_t)(HW_MAP_W - 1));
            map[y][HW_MAP_W - 1] = 0;
        }
        origin_tx++;
        fine_x -= HW_TILE;
    } else {
        for (y = 0; y < HW_MAP_H; y++) {
            memmove(&map[y][1], &map[y][0], (size_t)(HW_MAP_W - 1));
            map[y][0] = 0;
        }
        origin_tx--;
        fine_x += HW_TILE;
    }
}

static void coarse_y(int dir)
{
    if (dir > 0) {
        memmove(&map[0][0], &map[1][0], (size_t)(HW_MAP_H - 1) * HW_MAP_W);
        memset(&map[HW_MAP_H - 1][0], 0, HW_MAP_W);
        origin_ty++;
        fine_y -= HW_TILE;
    } else {
        memmove(&map[1][0], &map[0][0], (size_t)(HW_MAP_H - 1) * HW_MAP_W);
        memset(&map[0][0], 0, HW_MAP_W);
        origin_ty--;
        fine_y += HW_TILE;
    }
}

HwCoarse hw_scroll(int dx, int dy)
{
    HwCoarse c = {0, 0};

    while (dx > 0) {
        fine_x++;
        dx--;
        if (fine_x >= HW_TILE) {
            coarse_x(1);
            c.coarse_x++;
        }
    }
    while (dx < 0) {
        if (fine_x == 0) {
            coarse_x(-1);
            c.coarse_x--;
        }
        fine_x--;
        dx++;
    }
    while (dy > 0) {
        fine_y++;
        dy--;
        if (fine_y >= HW_TILE) {
            coarse_y(1);
            c.coarse_y++;
        }
    }
    while (dy < 0) {
        if (fine_y == 0) {
            coarse_y(-1);
            c.coarse_y--;
        }
        fine_y--;
        dy++;
    }
    return c;
}

void hw_sprite_pat(int id, const uint8_t pix[HW_SP_W * HW_SP_H])
{
    int i;

    if ((unsigned)id >= (unsigned)HW_SP_PATS || !pix)
        return;
    for (i = 0; i < SP_PIX; i++)
        spat[id][i] = (uint8_t)(pix[i] & (HW_COLORS - 1));
}

void hw_sprite(int i, int x, int y, int pat)
{
    if ((unsigned)i >= (unsigned)HW_SPRITES)
        return;
    if ((unsigned)pat >= (unsigned)HW_SP_PATS)
        pat = 0;
    sp[i].x = x;
    sp[i].y = y;
    sp[i].pat = pat;
    if (!sp[i].on)
        sp[i].draw = 1;
    sp[i].on = 1;
}

void hw_sprite_off(int i)
{
    if ((unsigned)i >= (unsigned)HW_SPRITES)
        return;
    sp[i].on = 0;
}

void hw_sprite_behind(int i, int behind)
{
    if ((unsigned)i >= (unsigned)HW_SPRITES)
        return;
    sp[i].behind = behind ? 1 : 0;
}

void hw_sprite_draw(int i, int draw)
{
    if ((unsigned)i >= (unsigned)HW_SPRITES)
        return;
    sp[i].draw = draw ? 1 : 0;
}

static int bg_id(int vx, int vy)
{
    int mx, my;

    if ((unsigned)vx >= (unsigned)HW_VIEW_W || (unsigned)vy >= (unsigned)HW_VIEW_H)
        return 0;
    mx = fine_x + vx;
    my = fine_y + vy;
    return map[my >> 3][mx >> 3];
}

static uint8_t sp_pix(const Sprite *s, int sx, int sy)
{
    return spat[s->pat][sy * HW_SP_W + sx];
}

int hw_hit(int a, int b)
{
    int x0, y0, x1, y1, x, y;
    const Sprite *sa, *sb;

    if ((unsigned)a >= (unsigned)HW_SPRITES || (unsigned)b >= (unsigned)HW_SPRITES)
        return 0;
    if (a == b)
        return 0;
    sa = &sp[a];
    sb = &sp[b];
    if (!sa->on || !sb->on)
        return 0;
    x0 = sa->x > sb->x ? sa->x : sb->x;
    y0 = sa->y > sb->y ? sa->y : sb->y;
    x1 = sa->x + HW_SP_W < sb->x + HW_SP_W ? sa->x + HW_SP_W : sb->x + HW_SP_W;
    y1 = sa->y + HW_SP_H < sb->y + HW_SP_H ? sa->y + HW_SP_H : sb->y + HW_SP_H;
    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            if (sp_pix(sa, x - sa->x, y - sa->y) && sp_pix(sb, x - sb->x, y - sb->y))
                return 1;
        }
    }
    return 0;
}

int hw_hit_bg(int i)
{
    int sx, sy, vx, vy;
    const Sprite *s;

    if ((unsigned)i >= (unsigned)HW_SPRITES)
        return 0;
    s = &sp[i];
    if (!s->on)
        return 0;
    for (sy = 0; sy < HW_SP_H; sy++) {
        vy = s->y + sy;
        for (sx = 0; sx < HW_SP_W; sx++) {
            if (!sp_pix(s, sx, sy))
                continue;
            vx = s->x + sx;
            if (bg_id(vx, vy))
                return 1;
        }
    }
    return 0;
}

static void draw_sprite(int i)
{
    int sx, sy, vx, vy;
    const Sprite *s = &sp[i];

    if (!s->on || !s->draw)
        return;
    for (sy = 0; sy < HW_SP_H; sy++) {
        vy = s->y + sy;
        if ((unsigned)vy >= (unsigned)HW_VIEW_H)
            continue;
        for (sx = 0; sx < HW_SP_W; sx++) {
            uint8_t pi = sp_pix(s, sx, sy);

            if (!pi)
                continue;
            vx = s->x + sx;
            if ((unsigned)vx >= (unsigned)HW_VIEW_W)
                continue;
            if (s->behind && bg_id(vx, vy))
                continue;
            vis[(size_t)vy * HW_VIEW_W + (size_t)vx] = pal[pi];
        }
    }
}

void hw_compose(void)
{
    int x, y, i;

    for (y = 0; y < HW_VIEW_H; y++) {
        Pixel *dst = vis + (size_t)y * HW_VIEW_W;
        int my = fine_y + y;
        int ty = my >> 3;
        int py = my & 7;

        for (x = 0; x < HW_VIEW_W; x++) {
            int mx = fine_x + x;
            int id = map[ty][mx >> 3];
            uint8_t pi = tiles[id][py * HW_TILE + (mx & 7)];

            dst[x] = pal[pi];
        }
    }
    for (i = HW_SPRITES - 1; i >= 0; i--)
        if (sp[i].behind)
            draw_sprite(i);
    for (i = HW_SPRITES - 1; i >= 0; i--)
        if (!sp[i].behind)
            draw_sprite(i);
}
