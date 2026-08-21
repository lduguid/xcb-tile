#define _POSIX_C_SOURCE 200809L

#include "hw_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
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

enum { SCALE = 3, TICK_US = 16667 };

static Display *dpy;
static xcb_connection_t *conn;
static xcb_window_t win;
static xcb_gcontext_t gc;
static xcb_pixmap_t pixmap;
static xcb_intern_atom_reply_t *wm_delete;
static xcb_generic_event_t *pending;
static int running = 1;
static uint32_t *stage;
static int stage_w, stage_h;
static uint32_t put_max;

static int map_key(KeySym ks)
{
    if (ks == XK_Left || ks == XK_a || ks == XK_A)
        return KEY_LEFT;
    if (ks == XK_Right || ks == XK_d || ks == XK_D)
        return KEY_RIGHT;
    if (ks == XK_Up || ks == XK_w || ks == XK_W)
        return KEY_UP;
    if (ks == XK_Down || ks == XK_s || ks == XK_S)
        return KEY_DOWN;
    if (ks == XK_Escape)
        return KEY_ESC;
    if (ks == XK_q || ks == XK_Q)
        return KEY_Q;
    if (ks == XK_space)
        return KEY_SPACE;
    return 0;
}

static xcb_generic_event_t *next_event(void)
{
    xcb_generic_event_t *ev;

    if (pending) {
        ev = pending;
        pending = NULL;
        return ev;
    }
    return xcb_poll_for_event(conn);
}

/* X auto-repeat is a release+press pair with the same timestamp. Drop it so
 * held arrows stay down instead of stuttering. */
static int is_auto_repeat(const xcb_key_release_event_t *rel)
{
    xcb_generic_event_t *ev = next_event();
    xcb_key_press_event_t *pr;

    if (!ev)
        return 0;
    if ((ev->response_type & ~0x80) == XCB_KEY_PRESS) {
        pr = (xcb_key_press_event_t *)ev;
        if (pr->detail == rel->detail && pr->time == rel->time) {
            free(ev);
            return 1;
        }
    }
    pending = ev;
    return 0;
}

static void blit_visible(void)
{
    const Pixel *src = hw_visible();
    int x, y, sy, chunk, y0;
    int vw = HW_VIEW_W, vh = HW_VIEW_H;
    int row_bytes = stage_w * 4;
    int max_rows;

    for (y = 0; y < vh; y++) {
        const Pixel *row = src + (size_t)y * vw;
        uint32_t *d0 = stage + (size_t)(y * SCALE) * stage_w;

        for (x = 0; x < vw; x++) {
            uint32_t p = row[x] | 0xff000000u;
            uint32_t *d = d0 + x * SCALE;
            for (sy = 0; sy < SCALE; sy++)
                d[sy] = p;
        }
        for (sy = 1; sy < SCALE; sy++)
            memcpy(d0 + (size_t)sy * stage_w, d0, (size_t)row_bytes);
    }

    max_rows = (int)(put_max / (uint32_t)row_bytes);
    if (max_rows < 1)
        max_rows = 1;
    for (y0 = 0; y0 < stage_h; y0 += chunk) {
        chunk = stage_h - y0;
        if (chunk > max_rows)
            chunk = max_rows;
        xcb_put_image(conn, XCB_IMAGE_FORMAT_Z_PIXMAP, pixmap, gc, (uint16_t)stage_w,
                      (uint16_t)chunk, 0, (int16_t)y0, 0, 24,
                      (uint32_t)chunk * (uint32_t)row_bytes,
                      (const uint8_t *)(stage + (size_t)y0 * stage_w));
    }
    xcb_copy_area(conn, pixmap, win, gc, 0, 0, 0, 0, (uint16_t)stage_w, (uint16_t)stage_h);
    xcb_flush(conn);
}

void hw_swap(void)
{
    hw_compose();
    blit_visible();
}

static void on_event(xcb_generic_event_t *ev)
{
    switch (ev->response_type & ~0x80) {
    case XCB_EXPOSE:
        blit_visible();
        break;
    case XCB_KEY_PRESS: {
        xcb_key_press_event_t *kp = (xcb_key_press_event_t *)ev;
        int k = map_key(XkbKeycodeToKeysym(dpy, kp->detail, 0, 0));
        hw_key_set(k, 1);
        if (k == KEY_ESC || k == KEY_Q)
            running = 0;
        break;
    }
    case XCB_KEY_RELEASE: {
        xcb_key_release_event_t *kr = (xcb_key_release_event_t *)ev;
        if (is_auto_repeat(kr))
            break;
        hw_key_set(map_key(XkbKeycodeToKeysym(dpy, kr->detail, 0, 0)), 0);
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
}

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

int hw_main(void (*tick)(float dt))
{
    xcb_screen_t *screen;
    xcb_intern_atom_reply_t *wm_proto = NULL;
    int fd;
    double prev;
    const char *title = "xcb-tile";

    if (!hw_init()) {
        fprintf(stderr, "cannot allocate framebuffer\n");
        return 1;
    }

    stage_w = HW_VIEW_W * SCALE;
    stage_h = HW_VIEW_H * SCALE;
    stage = calloc((size_t)stage_w * (size_t)stage_h, sizeof(*stage));
    if (!stage) {
        hw_shutdown();
        return 1;
    }

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "cannot open X display\n");
        hw_shutdown();
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
    pixmap = xcb_generate_id(conn);

    {
        uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        uint32_t values[] = {
            screen->black_pixel,
            XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY,
        };
        xcb_create_window(conn, XCB_COPY_FROM_PARENT, win, screen->root, 80, 60,
                          (uint16_t)stage_w, (uint16_t)stage_h, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          screen->root_visual, mask, values);
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
        uint32_t gv[] = {screen->white_pixel};
        xcb_create_gc(conn, gc, win, XCB_GC_FOREGROUND, gv);
    }
    xcb_create_pixmap(conn, screen->root_depth, pixmap, win, (uint16_t)stage_w, (uint16_t)stage_h);
    xcb_map_window(conn, win);
    xcb_flush(conn);

    fd = xcb_get_file_descriptor(conn);
    prev = now_sec();
    while (running) {
        xcb_generic_event_t *ev;
        fd_set fds;
        struct timeval tv;
        double t, dt, left;

        while ((ev = next_event())) {
            on_event(ev);
            free(ev);
        }
        if (!running)
            break;

        t = now_sec();
        dt = t - prev;
        prev = t;
        if (dt < 0.0)
            dt = 0.0;
        if (dt > 0.05)
            dt = 0.05;
        tick((float)dt);
        hw_keys_end_frame();

        left = TICK_US / 1000000.0 - (now_sec() - t);
        if (left < 0.0)
            left = 0.0;
        tv.tv_sec = 0;
        tv.tv_usec = (suseconds_t)(left * 1000000.0);
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        select(fd + 1, &fds, NULL, NULL, &tv);
    }
    free(pending);

    xcb_free_pixmap(conn, pixmap);
    xcb_free_gc(conn, gc);
    free(wm_proto);
    free(wm_delete);
    free(stage);
    hw_shutdown();
    XCloseDisplay(dpy);
    return 0;
}
