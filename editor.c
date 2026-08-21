/* Tile / map / sprite editor for the xcb-tile PPU.
 *
 * 8x8 tiles, 32x32 sprites, 64-color 4x4x4 cube. Save a .bank the
 * hardware can load; E exports a C header (BANK_LOAD_EMBED).
 *
 * 1 2 3     tiles / map / sprites
 * LMB       paint (or pan the map with Space)
 * RMB       pick color / tile
 * F / P     fill / pencil
 * [ ]       prev / next tile or sprite
 * Arrows    pan map (or Shift+arrows to scroll pixels)
 * C / V     copy / paste current tile or sprite
 * Z         undo
 * S L E     save / load / export .h
 * D         restore demo charset
 * X         toggle solid (player collides)
 * G         grid
 * - =       zoom
 * Click the path to type a filename. Esc quits. */

#define _POSIX_C_SOURCE 200809L

#include "bank.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <xcb/xcb.h>

extern xcb_connection_t *XGetXCBConnection(Display *dpy);
extern void XSetEventQueueOwner(Display *dpy, int owner);
#ifndef XCBOwnsEventQueue
#define XCBOwnsEventQueue 1
#endif

enum { WIN_W = 1280, WIN_H = 800, TOP_H = 40, STATUS_H = 24, LEFT_W = 188 };
enum { TAB_TILE = 0, TAB_MAP = 1, TAB_SPRITE = 2 };
enum { TOOL_PENCIL = 0, TOOL_FILL = 1 };
enum { UNDO_MAX = 24 };

typedef struct {
    int x, y, w, h;
} Rect;

static Display *dpy;
static xcb_connection_t *conn;
static xcb_window_t win;
static xcb_gcontext_t gc;
static xcb_pixmap_t pixmap;
static xcb_intern_atom_reply_t *wm_delete;
static xcb_screen_t *screen;
static uint32_t *fb;
static int fb_w, fb_h, win_w = WIN_W, win_h = WIN_H;
static uint32_t put_max;
static int running = 1;
static int dirty = 1;

static Bank bank;
static Bank undo_s[UNDO_MAX];
static int undo_n;
static int stroke;

static int tab = TAB_TILE;
static int tool = TOOL_PENCIL;
static int color;
static int tile_id = 3;
static int sprite_id;
static int grid_on = 1;
static int tile_zoom = 28;
static int sp_zoom = 14;
static int map_zoom = 2;
static int map_ox, map_oy;
static int space_down;
static int pan_x, pan_y, panning;
static int dirty_doc;
static int path_focus;
static char path[256] = "game.bank";
static char status[192] = "ready";
static char msg[192];
static double msg_t;

static uint8_t clip[BANK_SP_PIX];
static int clip_kind; /* 0 none, 1 tile, 2 sprite */

static int mx, my, mbtn;

/* 8x8, bit 7 left. ASCII 32..95; a-z fold to A-Z. */
static const uint8_t font8[64][8] = {
    [' ' - 32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['!' - 32] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00},
    ['#' - 32] = {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00},
    ['*' - 32] = {0x00, 0x24, 0x18, 0x7E, 0x18, 0x24, 0x00, 0x00},
    ['+' - 32] = {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00},
    ['-' - 32] = {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
    ['.' - 32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    ['/' - 32] = {0x02, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00},
    ['0' - 32] = {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
    ['1' - 32] = {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    ['2' - 32] = {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00},
    ['3' - 32] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
    ['4' - 32] = {0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00},
    ['5' - 32] = {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
    ['6' - 32] = {0x1C, 0x30, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
    ['7' - 32] = {0x7E, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['8' - 32] = {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
    ['9' - 32] = {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x38, 0x00},
    [':' - 32] = {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00},
    ['=' - 32] = {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00},
    ['?' - 32] = {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00},
    ['A' - 32] = {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00},
    ['B' - 32] = {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00},
    ['C' - 32] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
    ['D' - 32] = {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00},
    ['E' - 32] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00},
    ['F' - 32] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00},
    ['G' - 32] = {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00},
    ['H' - 32] = {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['I' - 32] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    ['J' - 32] = {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00},
    ['K' - 32] = {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00},
    ['L' - 32] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
    ['M' - 32] = {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00},
    ['N' - 32] = {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00},
    ['O' - 32] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['P' - 32] = {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00},
    ['Q' - 32] = {0x3C, 0x66, 0x66, 0x66, 0x6A, 0x6C, 0x36, 0x00},
    ['R' - 32] = {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00},
    ['S' - 32] = {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00},
    ['T' - 32] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['U' - 32] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['V' - 32] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    ['W' - 32] = {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
    ['X' - 32] = {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    ['Y' - 32] = {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00},
    ['Z' - 32] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
    ['[' - 32] = {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00},
    [']' - 32] = {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00},
    ['_' - 32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00},
};

static uint32_t rgb(int r, int g, int b)
{
    return 0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

static int in_rect(Rect r, int x, int y)
{
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static Rect R(int x, int y, int w, int h)
{
    Rect r = {x, y, w, h};
    return r;
}

static void set_msg(const char *s)
{
    snprintf(msg, sizeof(msg), "%s", s);
    msg_t = now_sec() + 2.5;
}

static void mark(void) { dirty = 1; }

static void checkpoint(void)
{
    if (stroke)
        return;
    stroke = 1;
    if (undo_n == UNDO_MAX) {
        bank_free(&undo_s[0]);
        memmove(&undo_s[0], &undo_s[1], sizeof(Bank) * (UNDO_MAX - 1));
        memset(&undo_s[UNDO_MAX - 1], 0, sizeof(Bank));
        undo_n = UNDO_MAX - 1;
    }
    if (!bank_copy(&undo_s[undo_n], &bank))
        return;
    undo_n++;
}

static void do_undo(void)
{
    Bank tmp;

    if (undo_n < 1)
        return;
    undo_n--;
    tmp = bank;
    bank = undo_s[undo_n];
    memset(&undo_s[undo_n], 0, sizeof(Bank));
    bank_free(&tmp);
    dirty_doc = 1;
    set_msg("undo");
    mark();
}

static int clampi(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static void put(int x, int y, uint32_t p)
{
    if ((unsigned)x >= (unsigned)fb_w || (unsigned)y >= (unsigned)fb_h)
        return;
    fb[(size_t)y * fb_w + x] = p;
}

static void fill(int x, int y, int w, int h, uint32_t p)
{
    int ix, iy, x1, y1;

    if (w < 1 || h < 1)
        return;
    x1 = x + w;
    y1 = y + h;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x1 > fb_w)
        x1 = fb_w;
    if (y1 > fb_h)
        y1 = fb_h;
    for (iy = y; iy < y1; iy++)
        for (ix = x; ix < x1; ix++)
            fb[(size_t)iy * fb_w + ix] = p;
}

static void frame(Rect r, uint32_t p)
{
    fill(r.x, r.y, r.w, 1, p);
    fill(r.x, r.y + r.h - 1, r.w, 1, p);
    fill(r.x, r.y, 1, r.h, p);
    fill(r.x + r.w - 1, r.y, 1, r.h, p);
}

static const uint8_t *glyph(int c)
{
    if (c >= 'a' && c <= 'z')
        c -= 32;
    if (c < 32 || c > 95)
        c = '?';
    return font8[c - 32];
}

static void text(int x, int y, const char *s, uint32_t p)
{
    int gx, gy;

    for (; *s; s++, x += 8) {
        const uint8_t *g = glyph((unsigned char)*s);

        for (gy = 0; gy < 8; gy++)
            for (gx = 0; gx < 8; gx++)
                if (g[gy] & (0x80 >> gx))
                    put(x + gx, y + gy, p);
    }
}

static uint32_t pal_rgb(int i)
{
    return bank.pal[i & (HW_COLORS - 1)] | 0xff000000u;
}

static void blit_pix(int ox, int oy, int zoom, const uint8_t *pix, int tw, int th, int checker)
{
    int x, y;

    for (y = 0; y < th; y++) {
        for (x = 0; x < tw; x++) {
            uint8_t ci = pix[y * tw + x] & (HW_COLORS - 1);
            uint32_t col;

            if (checker && ci == 0)
                col = ((x + y) & 1) ? rgb(52, 52, 58) : rgb(34, 34, 40);
            else
                col = pal_rgb(ci);
            fill(ox + x * zoom, oy + y * zoom, zoom, zoom, col);
            if (grid_on && zoom >= 6) {
                uint32_t g = rgb(16, 16, 20);
                fill(ox + x * zoom, oy + y * zoom, zoom, 1, g);
                fill(ox + x * zoom, oy + y * zoom, 1, zoom, g);
            }
        }
    }
}

static Rect pal_rect(void) { return R(10, TOP_H + 8, 22 * 8, 22 * 8); }
static Rect path_rect(void) { return R(420, 8, win_w - 700, 24); }
static Rect canvas_rect(void)
{
    int x = LEFT_W + 8, y = TOP_H + 8;
    int w, h;

    if (tab == TAB_TILE)
        return R(x, y, HW_TILE * tile_zoom, HW_TILE * tile_zoom);
    if (tab == TAB_SPRITE)
        return R(x, y, HW_SP_W * sp_zoom, HW_SP_H * sp_zoom);
    w = win_w - x - 12;
    h = win_h - y - STATUS_H - 12;
    return R(x, y, w > 40 ? w : 40, h > 40 ? h : 40);
}

static int tile_sheet_zoom(void) { return tab == TAB_MAP ? 1 : 3; }

static int tile_sheet_cell(void) { return HW_TILE * tile_sheet_zoom() + 1; }

static Rect sheet_rect(void)
{
    int x, y, cell;

    if (tab == TAB_MAP) {
        cell = tile_sheet_cell();
        return R(10, TOP_H + 8 + 22 * 8 + 120, 16 * cell, 16 * cell);
    }
    if (tab == TAB_TILE) {
        cell = tile_sheet_cell();
        x = LEFT_W + 8 + HW_TILE * tile_zoom + 16;
        y = TOP_H + 8;
        return R(x, y, 16 * cell, 16 * cell);
    }
    x = LEFT_W + 8 + HW_SP_W * sp_zoom + 16;
    y = TOP_H + 8;
    return R(x, y, 8 * (HW_SP_W + 2), 4 * (HW_SP_H + 2));
}

static void draw_btn(Rect r, const char *lab, int on)
{
    fill(r.x, r.y, r.w, r.h, on ? rgb(70, 90, 140) : rgb(40, 42, 50));
    frame(r, on ? rgb(180, 200, 255) : rgb(80, 82, 92));
    text(r.x + 8, r.y + (r.h - 8) / 2, lab, rgb(230, 230, 235));
}

static void flood_pix(uint8_t *pix, int w, int h, int x, int y, uint8_t from, uint8_t to)
{
    if (from == to || x < 0 || y < 0 || x >= w || y >= h)
        return;
    if (pix[y * w + x] != from)
        return;
    pix[y * w + x] = to;
    flood_pix(pix, w, h, x + 1, y, from, to);
    flood_pix(pix, w, h, x - 1, y, from, to);
    flood_pix(pix, w, h, x, y + 1, from, to);
    flood_pix(pix, w, h, x, y - 1, from, to);
}

static void flood_map(int sx, int sy, uint8_t from, uint8_t to)
{
    int *st, n = 0, cap, x, y;

    if (from == to)
        return;
    cap = bank.map_w * bank.map_h;
    st = malloc((size_t)cap * 2 * sizeof(int));
    if (!st)
        return;
    st[n++] = sx;
    st[n++] = sy;
    while (n > 0) {
        y = st[--n];
        x = st[--n];
        if (x < 0 || y < 0 || x >= bank.map_w || y >= bank.map_h)
            continue;
        if (bank_at(&bank, x, y) != from)
            continue;
        bank_put(&bank, x, y, to);
        st[n++] = x + 1;
        st[n++] = y;
        st[n++] = x - 1;
        st[n++] = y;
        st[n++] = x;
        st[n++] = y + 1;
        st[n++] = x;
        st[n++] = y - 1;
    }
    free(st);
}

static void shift_pix(uint8_t *pix, int w, int h, int dx, int dy)
{
    uint8_t tmp[BANK_SP_PIX];
    int x, y, nx, ny;

    memcpy(tmp, pix, (size_t)w * (size_t)h);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            nx = (x + dx + w) % w;
            ny = (y + dy + h) % h;
            pix[ny * w + nx] = tmp[y * w + x];
        }
    }
}

static void paint_canvas(int x, int y, int btn)
{
    Rect c = canvas_rect();
    int lx, ly, zoom, tw, th;
    uint8_t *pix;

    if (!in_rect(c, x, y))
        return;
    if (tab == TAB_MAP) {
        int cell = HW_TILE * map_zoom;
        int tx = map_ox + (x - c.x) / cell;
        int ty = map_oy + (y - c.y) / cell;

        if (tx < 0 || ty < 0 || tx >= bank.map_w || ty >= bank.map_h)
            return;
        if (btn == 3) {
            tile_id = bank_at(&bank, tx, ty);
            return;
        }
        checkpoint();
        dirty_doc = 1;
        if (tool == TOOL_FILL)
            flood_map(tx, ty, (uint8_t)bank_at(&bank, tx, ty), (uint8_t)tile_id);
        else
            bank_put(&bank, tx, ty, tile_id);
        return;
    }
    if (tab == TAB_TILE) {
        zoom = tile_zoom;
        tw = HW_TILE;
        th = HW_TILE;
        pix = bank.tiles[tile_id];
    } else {
        zoom = sp_zoom;
        tw = HW_SP_W;
        th = HW_SP_H;
        pix = bank.sprites[sprite_id];
    }
    lx = (x - c.x) / zoom;
    ly = (y - c.y) / zoom;
    if (lx < 0 || ly < 0 || lx >= tw || ly >= th)
        return;
    if (btn == 3) {
        color = pix[ly * tw + lx] & (HW_COLORS - 1);
        return;
    }
    checkpoint();
    dirty_doc = 1;
    if (tool == TOOL_FILL)
        flood_pix(pix, tw, th, lx, ly, pix[ly * tw + lx], (uint8_t)color);
    else
        pix[ly * tw + lx] = (uint8_t)color;
}

static void click_sheet(int x, int y, int btn)
{
    Rect s = sheet_rect();
    int col, row, id, cell;

    if (!in_rect(s, x, y))
        return;
    if (tab == TAB_SPRITE) {
        col = (x - s.x) / (HW_SP_W + 2);
        row = (y - s.y) / (HW_SP_H + 2);
        id = row * 8 + col;
        if (id >= 0 && id < HW_SP_PATS)
            sprite_id = id;
        return;
    }
    cell = tile_sheet_cell();
    col = (x - s.x) / cell;
    row = (y - s.y) / cell;
    id = row * 16 + col;
    if (id >= 0 && id < HW_TILES) {
        if (btn == 3)
            color = bank.tiles[id][HW_TILE * 4 + 4] & (HW_COLORS - 1);
        else
            tile_id = id;
    }
}

static void click_pal(int x, int y)
{
    Rect p = pal_rect();
    int col, row, id;

    if (!in_rect(p, x, y))
        return;
    col = (x - p.x) / 22;
    row = (y - p.y) / 22;
    id = row * 8 + col;
    if (id >= 0 && id < HW_COLORS)
        color = id;
}

static char *export_name(char *out, size_t n)
{
    size_t len = strlen(path);

    snprintf(out, n, "%s", path);
    if (len >= 5 && strcmp(path + len - 5, ".bank") == 0) {
        snprintf(out, n, "%.*s.h", (int)(len - 5), path);
    } else {
        snprintf(out, n, "%s.h", path);
    }
    return out;
}

static void do_save(void)
{
    if (bank_save(path, &bank)) {
        dirty_doc = 0;
        set_msg("saved");
    } else {
        set_msg("save failed");
    }
}

static void do_load(void)
{
    checkpoint();
    stroke = 0;
    if (bank_load(path, &bank)) {
        dirty_doc = 0;
        map_ox = map_oy = 0;
        set_msg("loaded");
    } else {
        set_msg("load failed");
    }
}

static void do_export(void)
{
    char out[280];

    export_name(out, sizeof(out));
    if (bank_export_c(out, &bank))
        set_msg("exported .h");
    else
        set_msg("export failed");
}

static void on_press(int x, int y, int btn, int state)
{
    Rect tabs[3] = {R(8, 8, 72, 24), R(84, 8, 64, 24), R(152, 8, 88, 24)};
    Rect bsave = R(win_w - 268, 8, 56, 24);
    Rect bload = R(win_w - 204, 8, 56, 24);
    Rect bexp = R(win_w - 140, 8, 72, 24);
    Rect bpenc = R(10, TOP_H + 8 + 22 * 8 + 12, 80, 22);
    Rect bfill = R(96, TOP_H + 8 + 22 * 8 + 12, 72, 22);
    Rect bw0 = R(10, TOP_H + 8 + 22 * 8 + 40, 28, 20);
    Rect bw1 = R(42, TOP_H + 8 + 22 * 8 + 40, 28, 20);
    Rect bh0 = R(90, TOP_H + 8 + 22 * 8 + 40, 28, 20);
    Rect bh1 = R(122, TOP_H + 8 + 22 * 8 + 40, 28, 20);
    int i;

    (void)state;
    mx = x;
    my = y;
    if (btn == 4 || btn == 5) {
        int d = btn == 4 ? 1 : -1;
        if (tab == TAB_TILE)
            tile_zoom = clampi(tile_zoom + d * 2, 8, 48);
        else if (tab == TAB_SPRITE)
            sp_zoom = clampi(sp_zoom + d, 4, 24);
        else
            map_zoom = clampi(map_zoom + d, 1, 4);
        mark();
        return;
    }
    if (btn == 2 || (space_down && btn == 1 && tab == TAB_MAP)) {
        panning = 1;
        pan_x = x;
        pan_y = y;
        return;
    }
    path_focus = in_rect(path_rect(), x, y);
    for (i = 0; i < 3; i++) {
        if (in_rect(tabs[i], x, y)) {
            tab = i;
            mark();
            return;
        }
    }
    if (in_rect(bsave, x, y)) {
        do_save();
        mark();
        return;
    }
    if (in_rect(bload, x, y)) {
        do_load();
        mark();
        return;
    }
    if (in_rect(bexp, x, y)) {
        do_export();
        mark();
        return;
    }
    if (in_rect(bpenc, x, y)) {
        tool = TOOL_PENCIL;
        mark();
        return;
    }
    if (in_rect(bfill, x, y)) {
        tool = TOOL_FILL;
        mark();
        return;
    }
    if (tab == TAB_MAP) {
        if (in_rect(bw0, x, y)) {
            checkpoint();
            stroke = 0;
            bank_resize_map(&bank, bank.map_w - 8, bank.map_h);
            dirty_doc = 1;
            mark();
            return;
        }
        if (in_rect(bw1, x, y)) {
            checkpoint();
            stroke = 0;
            bank_resize_map(&bank, bank.map_w + 8, bank.map_h);
            dirty_doc = 1;
            mark();
            return;
        }
        if (in_rect(bh0, x, y)) {
            checkpoint();
            stroke = 0;
            bank_resize_map(&bank, bank.map_w, bank.map_h - 8);
            dirty_doc = 1;
            mark();
            return;
        }
        if (in_rect(bh1, x, y)) {
            checkpoint();
            stroke = 0;
            bank_resize_map(&bank, bank.map_w, bank.map_h + 8);
            dirty_doc = 1;
            mark();
            return;
        }
    }
    mbtn = btn;
    click_pal(x, y);
    click_sheet(x, y, btn);
    paint_canvas(x, y, btn);
    mark();
}

static void on_release(void)
{
    mbtn = 0;
    panning = 0;
    stroke = 0;
}

static void on_motion(int x, int y)
{
    mx = x;
    my = y;
    if (panning && tab == TAB_MAP) {
        int cell = HW_TILE * map_zoom;
        int dx = x - pan_x, dy = y - pan_y;

        if (dx <= -cell || dx >= cell || dy <= -cell || dy >= cell) {
            map_ox -= dx / cell;
            map_oy -= dy / cell;
            map_ox = clampi(map_ox, 0, bank.map_w - 1);
            map_oy = clampi(map_oy, 0, bank.map_h - 1);
            pan_x = x;
            pan_y = y;
            mark();
        }
        return;
    }
    if (mbtn == 1 || mbtn == 3) {
        paint_canvas(x, y, mbtn);
        mark();
    }
}

static void copy_cur(void)
{
    if (tab == TAB_SPRITE) {
        memcpy(clip, bank.sprites[sprite_id], BANK_SP_PIX);
        clip_kind = 2;
        set_msg("copied sprite");
    } else {
        memcpy(clip, bank.tiles[tile_id], BANK_TILE_PIX);
        clip_kind = 1;
        set_msg("copied tile");
    }
}

static void paste_cur(void)
{
    checkpoint();
    stroke = 0;
    if (clip_kind == 2 && tab == TAB_SPRITE) {
        memcpy(bank.sprites[sprite_id], clip, BANK_SP_PIX);
        dirty_doc = 1;
        set_msg("pasted sprite");
    } else if (clip_kind == 1 && tab != TAB_SPRITE) {
        memcpy(bank.tiles[tile_id], clip, BANK_TILE_PIX);
        dirty_doc = 1;
        set_msg("pasted tile");
    } else {
        set_msg("clipboard empty");
    }
}

static void on_key(KeySym ks, unsigned state)
{
    int shift = (state & ShiftMask) != 0;

    if (ks == XK_Escape) {
        if (path_focus)
            path_focus = 0;
        else
            running = 0;
        mark();
        return;
    }
    if (path_focus) {
        size_t n = strlen(path);
        if (ks == XK_Return) {
            path_focus = 0;
        } else if (ks == XK_BackSpace && n > 0) {
            path[n - 1] = 0;
        } else if (ks >= 32 && ks < 127 && n + 1 < sizeof(path)) {
            path[n] = (char)ks;
            path[n + 1] = 0;
        }
        mark();
        return;
    }
    if (ks == XK_space) {
        space_down = 1;
        return;
    }
    if (ks == XK_s || ks == XK_S) {
        do_save();
        mark();
        return;
    }
    if (ks == XK_l || ks == XK_L) {
        do_load();
        mark();
        return;
    }
    if (ks == XK_e || ks == XK_E) {
        do_export();
        mark();
        return;
    }
    if (ks == XK_z || ks == XK_Z) {
        do_undo();
        return;
    }
    if (ks == XK_c || ks == XK_C) {
        copy_cur();
        mark();
        return;
    }
    if (ks == XK_v || ks == XK_V) {
        paste_cur();
        mark();
        return;
    }
    if (ks == XK_1) {
        tab = TAB_TILE;
        mark();
        return;
    }
    if (ks == XK_2) {
        tab = TAB_MAP;
        mark();
        return;
    }
    if (ks == XK_3) {
        tab = TAB_SPRITE;
        mark();
        return;
    }
    if (ks == XK_p || ks == XK_P) {
        tool = TOOL_PENCIL;
        mark();
        return;
    }
    if (ks == XK_f || ks == XK_F) {
        tool = TOOL_FILL;
        mark();
        return;
    }
    if (ks == XK_g || ks == XK_G) {
        grid_on = !grid_on;
        mark();
        return;
    }
    if (ks == XK_d || ks == XK_D) {
        checkpoint();
        stroke = 0;
        bank_seed_charset(&bank);
        dirty_doc = 1;
        set_msg("demo charset");
        mark();
        return;
    }
    if (ks == XK_x || ks == XK_X) {
        bank.solid[tile_id] = !bank.solid[tile_id];
        dirty_doc = 1;
        set_msg(bank.solid[tile_id] ? "solid" : "passable");
        mark();
        return;
    }
    if (ks == XK_n || ks == XK_N) {
        checkpoint();
        stroke = 0;
        if (tab == TAB_SPRITE)
            memset(bank.sprites[sprite_id], 0, BANK_SP_PIX);
        else if (tab == TAB_TILE)
            memset(bank.tiles[tile_id], 0, BANK_TILE_PIX);
        dirty_doc = 1;
        mark();
        return;
    }
    if (ks == XK_bracketleft) {
        if (tab == TAB_SPRITE)
            sprite_id = (sprite_id + HW_SP_PATS - 1) % HW_SP_PATS;
        else
            tile_id = (tile_id + HW_TILES - 1) % HW_TILES;
        mark();
        return;
    }
    if (ks == XK_bracketright) {
        if (tab == TAB_SPRITE)
            sprite_id = (sprite_id + 1) % HW_SP_PATS;
        else
            tile_id = (tile_id + 1) % HW_TILES;
        mark();
        return;
    }
    if (ks == XK_minus) {
        if (tab == TAB_TILE)
            tile_zoom = clampi(tile_zoom - 2, 8, 48);
        else if (tab == TAB_SPRITE)
            sp_zoom = clampi(sp_zoom - 1, 4, 24);
        else
            map_zoom = clampi(map_zoom - 1, 1, 4);
        mark();
        return;
    }
    if (ks == XK_equal || ks == XK_plus) {
        if (tab == TAB_TILE)
            tile_zoom = clampi(tile_zoom + 2, 8, 48);
        else if (tab == TAB_SPRITE)
            sp_zoom = clampi(sp_zoom + 1, 4, 24);
        else
            map_zoom = clampi(map_zoom + 1, 1, 4);
        mark();
        return;
    }
    if (shift && (ks == XK_Left || ks == XK_Right || ks == XK_Up || ks == XK_Down)) {
        int dx = (ks == XK_Right) - (ks == XK_Left);
        int dy = (ks == XK_Down) - (ks == XK_Up);
        checkpoint();
        stroke = 0;
        if (tab == TAB_SPRITE)
            shift_pix(bank.sprites[sprite_id], HW_SP_W, HW_SP_H, dx, dy);
        else
            shift_pix(bank.tiles[tile_id], HW_TILE, HW_TILE, dx, dy);
        dirty_doc = 1;
        mark();
        return;
    }
    if (tab == TAB_MAP && (ks == XK_Left || ks == XK_Right || ks == XK_Up || ks == XK_Down)) {
        if (ks == XK_Left)
            map_ox--;
        if (ks == XK_Right)
            map_ox++;
        if (ks == XK_Up)
            map_oy--;
        if (ks == XK_Down)
            map_oy++;
        map_ox = clampi(map_ox, 0, bank.map_w - 1);
        map_oy = clampi(map_oy, 0, bank.map_h - 1);
        mark();
        return;
    }
}

static void on_key_up(KeySym ks)
{
    if (ks == XK_space)
        space_down = 0;
}

static void draw_sheet_tiles(Rect s)
{
    int id, col, row, z = tile_sheet_zoom(), cw, x, y;

    cw = HW_TILE * z + 1;
    for (id = 0; id < HW_TILES; id++) {
        col = id % 16;
        row = id / 16;
        x = s.x + col * cw;
        y = s.y + row * cw;
        blit_pix(x, y, z, bank.tiles[id], HW_TILE, HW_TILE, 0);
        if (id == tile_id)
            frame(R(x - 1, y - 1, HW_TILE * z + 2, HW_TILE * z + 2), rgb(255, 220, 80));
        if (bank.solid[id])
            fill(x + 1, y + 1, 3, 3, rgb(220, 40, 40));
    }
}

static void draw_sheet_sprites(Rect s)
{
    int id, col, row, z = 1, cw, ch, x, y;

    cw = HW_SP_W * z + 2;
    ch = HW_SP_H * z + 2;
    for (id = 0; id < HW_SP_PATS; id++) {
        col = id % 8;
        row = id / 8;
        x = s.x + col * cw;
        y = s.y + row * ch;
        blit_pix(x, y, z, bank.sprites[id], HW_SP_W, HW_SP_H, 1);
        if (id == sprite_id)
            frame(R(x - 1, y - 1, HW_SP_W * z + 2, HW_SP_H * z + 2), rgb(255, 220, 80));
    }
}

static void draw_map(Rect c)
{
    int tx, ty, x, y, id, vw, vh, cell = HW_TILE * map_zoom;

    vw = c.w / cell;
    vh = c.h / cell;
    fill(c.x, c.y, c.w, c.h, rgb(12, 12, 16));
    for (ty = 0; ty < vh; ty++) {
        for (tx = 0; tx < vw; tx++) {
            int mx0 = map_ox + tx, my0 = map_oy + ty;

            if (mx0 >= bank.map_w || my0 >= bank.map_h)
                continue;
            id = bank_at(&bank, mx0, my0);
            x = c.x + tx * cell;
            y = c.y + ty * cell;
            blit_pix(x, y, map_zoom, bank.tiles[id], HW_TILE, HW_TILE, 0);
        }
    }
    {
        int vwpx = (HW_VIEW_W / HW_TILE) * cell;
        int vhpx = (HW_VIEW_H / HW_TILE) * cell;
        frame(R(c.x, c.y, vwpx, vhpx), rgb(255, 80, 80));
    }
    frame(c, rgb(70, 72, 80));
}

static void redraw(void)
{
    Rect pal = pal_rect(), can, sh;
    int i, col, row;
    char line[256];
    uint32_t bg = rgb(22, 24, 28), fg = rgb(220, 222, 230), dim = rgb(140, 144, 154);

    if (!fb)
        return;
    fill(0, 0, fb_w, fb_h, bg);
    fill(0, 0, fb_w, TOP_H, rgb(28, 30, 36));
    fill(0, win_h - STATUS_H, fb_w, STATUS_H, rgb(18, 18, 22));

    draw_btn(R(8, 8, 72, 24), "TILES", tab == TAB_TILE);
    draw_btn(R(84, 8, 64, 24), "MAP", tab == TAB_MAP);
    draw_btn(R(152, 8, 88, 24), "SPRITES", tab == TAB_SPRITE);
    draw_btn(R(win_w - 268, 8, 56, 24), "SAVE", 0);
    draw_btn(R(win_w - 204, 8, 56, 24), "LOAD", 0);
    draw_btn(R(win_w - 140, 8, 72, 24), "EXPORT", 0);

    {
        Rect pr = path_rect();
        fill(pr.x, pr.y, pr.w, pr.h, path_focus ? rgb(40, 44, 58) : rgb(32, 34, 40));
        frame(pr, path_focus ? rgb(200, 210, 255) : rgb(70, 72, 80));
        snprintf(line, sizeof(line), "%.40s%s", path, dirty_doc ? "*" : "");
        text(pr.x + 6, pr.y + 8, line, fg);
    }

    for (i = 0; i < HW_COLORS; i++) {
        col = i % 8;
        row = i / 8;
        fill(pal.x + col * 22, pal.y + row * 22, 21, 21, pal_rgb(i));
        if (i == color)
            frame(R(pal.x + col * 22, pal.y + row * 22, 21, 21), rgb(255, 255, 255));
    }
    if (tab == TAB_SPRITE)
        text(pal.x, pal.y + 22 * 8 + 2, "0 = CLEAR", dim);

    draw_btn(R(10, TOP_H + 8 + 22 * 8 + 12, 80, 22), "PENCIL", tool == TOOL_PENCIL);
    draw_btn(R(96, TOP_H + 8 + 22 * 8 + 12, 72, 22), "FILL", tool == TOOL_FILL);

    fill(10, TOP_H + 8 + 22 * 8 + 64, 36, 36, pal_rgb(color));
    frame(R(10, TOP_H + 8 + 22 * 8 + 64, 36, 36), rgb(255, 255, 255));
    snprintf(line, sizeof(line), "COL %d", color);
    text(52, TOP_H + 8 + 22 * 8 + 76, line, fg);

    can = canvas_rect();
    sh = sheet_rect();

    if (tab == TAB_TILE) {
        blit_pix(can.x, can.y, tile_zoom, bank.tiles[tile_id], HW_TILE, HW_TILE, 0);
        frame(can, rgb(200, 200, 210));
        draw_sheet_tiles(sh);
        /* repeat preview */
        for (row = 0; row < 4; row++)
            for (col = 0; col < 4; col++)
                blit_pix(can.x + col * HW_TILE, can.y + HW_TILE * tile_zoom + 12 + row * HW_TILE, 1,
                         bank.tiles[tile_id], HW_TILE, HW_TILE, 0);
        snprintf(line, sizeof(line), "TILE %d / %d  %s", tile_id, HW_TILES - 1,
                 bank.solid[tile_id] ? "SOLID" : "PASS");
        text(10, win_h - STATUS_H - 18, line, dim);
    } else if (tab == TAB_SPRITE) {
        blit_pix(can.x, can.y, sp_zoom, bank.sprites[sprite_id], HW_SP_W, HW_SP_H, 1);
        frame(can, rgb(200, 200, 210));
        draw_sheet_sprites(sh);
        snprintf(line, sizeof(line), "SPRITE %d / %d", sprite_id, HW_SP_PATS - 1);
        text(10, win_h - STATUS_H - 18, line, dim);
    } else {
        draw_map(can);
        draw_sheet_tiles(sh);
        draw_btn(R(10, TOP_H + 8 + 22 * 8 + 40, 28, 20), "W-", 0);
        draw_btn(R(42, TOP_H + 8 + 22 * 8 + 40, 28, 20), "W+", 0);
        draw_btn(R(90, TOP_H + 8 + 22 * 8 + 40, 28, 20), "H-", 0);
        draw_btn(R(122, TOP_H + 8 + 22 * 8 + 40, 28, 20), "H+", 0);
        snprintf(line, sizeof(line), "MAP %dx%d  AT %d,%d  TILE %d", bank.map_w, bank.map_h, map_ox, map_oy,
                 tile_id);
        text(10, win_h - STATUS_H - 18, line, dim);
    }

    if (now_sec() < msg_t)
        snprintf(status, sizeof(status), "%s", msg);
    else
        snprintf(status, sizeof(status),
                 "1-3 tabs  LMB paint  X solid  F fill  S save  L load  E export");
    text(8, win_h - STATUS_H + 8, status, dim);
}

static int ensure_fb(void)
{
    size_t n;

    if (fb && fb_w == win_w && fb_h == win_h)
        return 1;
    free(fb);
    fb_w = win_w;
    fb_h = win_h;
    n = (size_t)fb_w * (size_t)fb_h;
    fb = calloc(n, 4);
    if (!fb)
        return 0;
    if (pixmap)
        xcb_free_pixmap(conn, pixmap);
    pixmap = xcb_generate_id(conn);
    xcb_create_pixmap(conn, screen->root_depth, pixmap, win, (uint16_t)fb_w, (uint16_t)fb_h);
    return 1;
}

static void present(void)
{
    int y0, chunk, max_rows, row_bytes;

    if (!fb || !pixmap)
        return;
    row_bytes = fb_w * 4;
    max_rows = (int)(put_max / (uint32_t)row_bytes);
    if (max_rows < 1)
        max_rows = 1;
    for (y0 = 0; y0 < fb_h; y0 += chunk) {
        chunk = fb_h - y0;
        if (chunk > max_rows)
            chunk = max_rows;
        xcb_put_image(conn, XCB_IMAGE_FORMAT_Z_PIXMAP, pixmap, gc, (uint16_t)fb_w, (uint16_t)chunk, 0,
                      (int16_t)y0, 0, 24, (uint32_t)chunk * (uint32_t)row_bytes,
                      (const uint8_t *)(fb + (size_t)y0 * fb_w));
    }
    xcb_copy_area(conn, pixmap, win, gc, 0, 0, 0, 0, (uint16_t)fb_w, (uint16_t)fb_h);
    xcb_flush(conn);
}

int main(int argc, char **argv)
{
    xcb_intern_atom_reply_t *wm_proto = NULL;
    int fd;
    const char *title = "xcb-tile-edit";

    bank_init(&bank);
    if (argc > 1) {
        snprintf(path, sizeof(path), "%s", argv[1]);
        if (bank_load(path, &bank))
            dirty_doc = 0;
    }

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "cannot open X display\n");
        return 1;
    }
    conn = XGetXCBConnection(dpy);
    if (!conn || xcb_connection_has_error(conn)) {
        fprintf(stderr, "cannot get XCB connection\n");
        return 1;
    }
    XSetEventQueueOwner(dpy, XCBOwnsEventQueue);
    XkbSetDetectableAutoRepeat(dpy, True, NULL);
    screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    put_max = xcb_get_maximum_request_length(conn) * 4u / 2u;
    if (put_max < 4096)
        put_max = 4096;

    win = xcb_generate_id(conn);
    gc = xcb_generate_id(conn);
    {
        uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        uint32_t values[] = {
            screen->black_pixel,
            XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_STRUCTURE_NOTIFY,
        };
        xcb_create_window(conn, XCB_COPY_FROM_PARENT, win, screen->root, 40, 30, WIN_W, WIN_H, 0,
                          XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, mask, values);
    }
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                        (uint32_t)strlen(title), title);
    {
        xcb_intern_atom_cookie_t pc = xcb_intern_atom(conn, 1, 12, "WM_PROTOCOLS");
        xcb_intern_atom_cookie_t dc = xcb_intern_atom(conn, 0, 16, "WM_DELETE_WINDOW");
        wm_proto = xcb_intern_atom_reply(conn, pc, NULL);
        wm_delete = xcb_intern_atom_reply(conn, dc, NULL);
        if (wm_proto && wm_delete)
            xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, wm_proto->atom, XCB_ATOM_ATOM, 32, 1,
                                &wm_delete->atom);
    }
    {
        uint32_t gv[] = {screen->white_pixel, 0};
        xcb_create_gc(conn, gc, win, XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES, gv);
    }
    if (!ensure_fb()) {
        fprintf(stderr, "cannot allocate framebuffer\n");
        return 1;
    }
    xcb_map_window(conn, win);
    xcb_flush(conn);

    fd = xcb_get_file_descriptor(conn);
    while (running) {
        fd_set fds;
        struct timeval tv = {0, 16000};
        xcb_generic_event_t *ev;

        if (dirty) {
            redraw();
            present();
            dirty = 0;
        }
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        select(fd + 1, &fds, NULL, NULL, &tv);
        while ((ev = xcb_poll_for_event(conn))) {
            switch (ev->response_type & ~0x80) {
            case XCB_EXPOSE: {
                xcb_expose_event_t *ex = (xcb_expose_event_t *)ev;
                if (ex->count == 0)
                    present();
                break;
            }
            case XCB_CONFIGURE_NOTIFY: {
                xcb_configure_notify_event_t *cfg = (xcb_configure_notify_event_t *)ev;
                if (cfg->width > 0 && cfg->height > 0 &&
                    (cfg->width != win_w || cfg->height != win_h)) {
                    win_w = cfg->width;
                    win_h = cfg->height;
                    ensure_fb();
                    mark();
                }
                break;
            }
            case XCB_BUTTON_PRESS: {
                xcb_button_press_event_t *bp = (xcb_button_press_event_t *)ev;
                on_press(bp->event_x, bp->event_y, bp->detail, bp->state);
                break;
            }
            case XCB_BUTTON_RELEASE:
                on_release();
                break;
            case XCB_MOTION_NOTIFY: {
                xcb_motion_notify_event_t *mv = (xcb_motion_notify_event_t *)ev;
                on_motion(mv->event_x, mv->event_y);
                break;
            }
            case XCB_KEY_PRESS: {
                xcb_key_press_event_t *kp = (xcb_key_press_event_t *)ev;
                on_key(XkbKeycodeToKeysym(dpy, kp->detail, 0, 0), kp->state);
                break;
            }
            case XCB_KEY_RELEASE: {
                xcb_key_release_event_t *kr = (xcb_key_release_event_t *)ev;
                on_key_up(XkbKeycodeToKeysym(dpy, kr->detail, 0, 0));
                break;
            }
            case XCB_CLIENT_MESSAGE: {
                xcb_client_message_event_t *cm = (xcb_client_message_event_t *)ev;
                if (wm_delete && cm->data.data32[0] == wm_delete->atom)
                    running = 0;
                break;
            }
            default:
                break;
            }
            free(ev);
        }
    }

    {
        int i;
        for (i = 0; i < UNDO_MAX; i++)
            bank_free(&undo_s[i]);
    }
    bank_free(&bank);
    free(fb);
    if (pixmap)
        xcb_free_pixmap(conn, pixmap);
    xcb_free_gc(conn, gc);
    free(wm_proto);
    free(wm_delete);
    XCloseDisplay(dpy);
    return 0;
}
