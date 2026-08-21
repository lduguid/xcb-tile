#include "play.h"

#include <string.h>

enum {
    WALK = 95,
    GRAV = 900,
    JUMP = 280,
    TERM = 280,
    CUT = 3
};

static int mask_pat(const Actor *a)
{
    if (a->mask >= 0)
        return a->mask;
    return a->pat;
}

static int hits(const Actor *a, const Bank *b, int x, int y)
{
    return bank_mask_hit(b, x, y, mask_pat(a));
}

static void move_axis(Actor *a, const Bank *b, float nx, float ny, int horiz)
{
    int ix = (int)nx, iy = (int)ny;

    if (!hits(a, b, ix, iy)) {
        a->x = nx;
        a->y = ny;
        return;
    }
    if (horiz) {
        int dir = nx > a->x ? 1 : -1;
        int x = (int)a->x;

        while (x != ix) {
            int n = x + dir;
            if (hits(a, b, n, (int)a->y))
                break;
            x = n;
        }
        a->x = (float)x;
        a->vx = 0;
    } else {
        int dir = ny > a->y ? 1 : -1;
        int y = (int)a->y;

        while (y != iy) {
            int n = y + dir;
            if (hits(a, b, (int)a->x, n))
                break;
            y = n;
        }
        a->y = (float)y;
        if (dir > 0)
            a->grounded = 1;
        a->vy = 0;
    }
}

static void default_blob(Bank *b)
{
    uint8_t pix[BANK_SP_PIX];
    int x, y, cx = 16, cy = 20;

    memset(pix, 0, sizeof(pix));
    for (y = 0; y < HW_SP_H; y++) {
        for (x = 0; x < HW_SP_W; x++) {
            int d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (d <= 10 * 10)
                pix[y * HW_SP_W + x] = (uint8_t)(d >= 8 * 8 ? HW_PAL(3, 3, 3) : HW_PAL(3, 0, 0));
        }
    }
    memcpy(b->sprites[0], pix, BANK_SP_PIX);
    hw_sprite_pat(0, pix);
}

static void ensure_sprite(Bank *b)
{
    int i, n = 0;

    for (i = 0; i < BANK_SP_PIX; i++)
        n += b->sprites[0][i];
    if (n)
        return;
    default_blob(b);
}

static int actor_find_spawn(const Bank *b, const Actor *a, float *ox, float *oy)
{
    int tx, ty, mx0, my0, mx1, my1, foot;

    if (!bank_mask_bounds(b, mask_pat(a), &mx0, &my0, &mx1, &my1)) {
        mx0 = 8;
        my0 = 8;
        mx1 = 23;
        my1 = 31;
    }
    foot = my1 + 1;
    for (tx = 1; tx < b->map_w - 1; tx++) {
        for (ty = 2; ty < b->map_h; ty++) {
            int wx, wy;

            if (bank_blocked(b, tx, ty - 1) || !bank_blocked(b, tx, ty))
                continue;
            wx = tx * HW_TILE + HW_TILE / 2 - (mx0 + mx1) / 2;
            wy = ty * HW_TILE - foot;
            if (wx < 0)
                wx = 0;
            if (bank_mask_hit(b, wx, wy, mask_pat(a)))
                continue;
            *ox = (float)wx;
            *oy = (float)wy;
            return 1;
        }
    }
    *ox = 16.0f;
    *oy = 16.0f;
    return 0;
}

void actor_spawn(Actor *a, Bank *b)
{
    memset(a, 0, sizeof(*a));
    a->pat = 0;
    a->mask = -1;
    ensure_sprite(b);
    actor_find_spawn(b, a, &a->spawn_x, &a->spawn_y);
    a->x = a->spawn_x;
    a->y = a->spawn_y;
}

static void respawn(Actor *a)
{
    a->x = a->spawn_x;
    a->y = a->spawn_y;
    a->vx = a->vy = 0;
    a->grounded = 0;
}

void actor_platform(Actor *a, const Bank *b, float dt)
{
    float nx, ny;

    a->vx = 0;
    if (hw_key_down(KEY_LEFT))
        a->vx = -WALK;
    if (hw_key_down(KEY_RIGHT))
        a->vx = WALK;

    if (a->grounded && hw_key_pressed(KEY_JUMP)) {
        a->vy = -(float)JUMP;
        a->grounded = 0;
    }
    if (hw_key_released(KEY_JUMP) && a->vy < 0)
        a->vy /= (float)CUT;

    a->vy += (float)GRAV * dt;
    if (a->vy > TERM)
        a->vy = TERM;

    nx = a->x + a->vx * dt;
    move_axis(a, b, nx, a->y, 1);

    a->grounded = 0;
    ny = a->y + a->vy * dt;
    move_axis(a, b, a->x, ny, 0);

    if (hits(a, b, (int)a->x, (int)a->y + 1))
        a->grounded = 1;

    if (a->y > (float)(b->map_h * HW_TILE + HW_SP_H))
        respawn(a);
}

void actor_topdown(Actor *a, const Bank *b, float dt)
{
    float nx, ny;

    a->vx = (float)(hw_key_down(KEY_RIGHT) - hw_key_down(KEY_LEFT)) * WALK;
    a->vy = (float)(hw_key_down(KEY_DOWN) - hw_key_down(KEY_UP)) * WALK;
    nx = a->x + a->vx * dt;
    ny = a->y + a->vy * dt;
    move_axis(a, b, nx, a->y, 1);
    move_axis(a, b, a->x, ny, 0);
    a->grounded = 1;
}

void actor_draw(const Actor *a)
{
    int sx = (int)(a->x + 0.5f) - hw_cam_x();
    int sy = (int)(a->y + 0.5f) - hw_cam_y();

    hw_sprite(ACTOR_SPRITE, sx, sy, a->pat);
}

void play_cam_follow(const Bank *b, const Actor *a)
{
    int fx = (int)(a->x + HW_SP_W / 2);
    int fy = (int)(a->y + HW_SP_H / 2);
    int cx = fx - HW_VIEW_W / 3;
    int cy = fy - HW_VIEW_H / 2;
    int max_x = b->map_w * HW_TILE - HW_VIEW_W;
    int max_y = b->map_h * HW_TILE - HW_VIEW_H;

    if (max_x < 0)
        max_x = 0;
    if (max_y < 0)
        max_y = 0;
    if (cx < 0)
        cx = 0;
    if (cy < 0)
        cy = 0;
    if (cx > max_x)
        cx = max_x;
    if (cy > max_y)
        cy = max_y;
    bank_scroll_to(b, cx, cy);
}
