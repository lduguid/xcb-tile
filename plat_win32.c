#define WIN32_LEAN_AND_MEAN

#include "hw_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <mmsystem.h>

enum { SCALE = 3 };

static HWND hwnd;
static int running = 1;
static uint32_t *dib;
static int dib_w, dib_h;
static HRESULT(WINAPI *dwm_flush)(void);

static int map_vk(WPARAM vk)
{
    if (vk == VK_LEFT || vk == 'A')
        return KEY_LEFT;
    if (vk == VK_RIGHT || vk == 'D')
        return KEY_RIGHT;
    if (vk == VK_UP || vk == 'W')
        return KEY_UP;
    if (vk == VK_DOWN || vk == 'S')
        return KEY_DOWN;
    if (vk == VK_ESCAPE)
        return KEY_ESC;
    if (vk == 'Q')
        return KEY_Q;
    if (vk == VK_SPACE)
        return KEY_SPACE;
    return 0;
}

static void blit_visible(HDC hdc)
{
    BITMAPINFO bmi;
    const Pixel *src = hw_visible();
    int x, y, vw = HW_VIEW_W, vh = HW_VIEW_H;

    for (y = 0; y < vh; y++) {
        const Pixel *row = src + (size_t)y * vw;
        uint32_t *dst = dib + (size_t)y * vw;
        for (x = 0; x < vw; x++)
            dst[x] = row[x] | 0xff000000u;
    }

    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = vw;
    bmi.bmiHeader.biHeight = -vh;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(hdc, 0, 0, dib_w, dib_h, 0, 0, vw, vh, dib, &bmi, DIB_RGB_COLORS, SRCCOPY);
}

void hw_swap(void)
{
    HDC hdc = GetDC(hwnd);

    hw_compose();
    if (hdc) {
        blit_visible(hdc);
        ReleaseDC(hwnd, hdc);
    }
    if (dwm_flush)
        dwm_flush();
}

static LRESULT CALLBACK wnd_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(w, &ps);
        blit_visible(hdc);
        EndPaint(w, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (lp & (1u << 30))
            return 0;
        {
            int k = map_vk(wp);
            hw_key_set(k, 1);
            if (k == KEY_ESC || k == KEY_Q)
                running = 0;
        }
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        hw_key_set(map_vk(wp), 0);
        return 0;
    case WM_CLOSE:
        running = 0;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(w, msg, wp, lp);
    }
}

static double now_sec(void)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER t;

    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)freq.QuadPart;
}

int hw_main(void (*tick)(float dt))
{
    WNDCLASSEXA wc;
    RECT rc;
    HINSTANCE inst = GetModuleHandleA(NULL);
    HMODULE dwm;
    double prev;
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    if (!hw_init()) {
        fprintf(stderr, "cannot allocate framebuffer\n");
        return 1;
    }
    dib_w = HW_VIEW_W * SCALE;
    dib_h = HW_VIEW_H * SCALE;
    dib = calloc((size_t)HW_VIEW_W * (size_t)HW_VIEW_H, sizeof(*dib));
    if (!dib) {
        hw_shutdown();
        return 1;
    }

    dwm = LoadLibraryA("dwmapi.dll");
    if (dwm) {
        FARPROC p = GetProcAddress(dwm, "DwmFlush");
        memcpy(&dwm_flush, &p, sizeof(p));
    }

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "XcbTile";
    RegisterClassExA(&wc);

    rc.left = 0;
    rc.top = 0;
    rc.right = dib_w;
    rc.bottom = dib_h;
    AdjustWindowRect(&rc, style, FALSE);
    hwnd = CreateWindowExA(0, "XcbTile", "xcb-tile", style, CW_USEDEFAULT, CW_USEDEFAULT,
                           rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, inst, NULL);
    if (!hwnd) {
        fprintf(stderr, "cannot create window\n");
        return 1;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    timeBeginPeriod(1);
    prev = now_sec();
    while (running) {
        MSG msg;
        double t, dt, left;

        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                running = 0;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
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

        if (!dwm_flush) {
            left = 1.0 / 60.0 - (now_sec() - t);
            if (left > 0.0)
                Sleep((DWORD)(left * 1000.0 + 0.5));
        }
    }
    timeEndPeriod(1);
    free(dib);
    hw_shutdown();
    if (dwm)
        FreeLibrary(dwm);
    return 0;
}
