CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 $(shell pkg-config --cflags x11 xcb)
MACHINE = $(shell gcc -dumpmachine)
X11XCB = $(shell test -e /usr/lib/$(MACHINE)/libX11-xcb.so && echo -lX11-xcb || echo /usr/lib/$(MACHINE)/libX11-xcb.so.1)
LIBS = $(shell pkg-config --libs x11 xcb) $(X11XCB)

HARNESS = hw.c plat.c map.c hw.h hw_internal.h map.h
.PHONY: all clean

all: xcb-tile xcb-tile-anim xcb-tile-sprites xcb-tile-mask

xcb-tile: demo.c $(HARNESS)
	$(CC) $(CFLAGS) -o $@ demo.c hw.c plat.c map.c $(LIBS)

xcb-tile-anim: demo_anim.c $(HARNESS)
	$(CC) $(CFLAGS) -o $@ demo_anim.c hw.c plat.c map.c $(LIBS)

xcb-tile-sprites: demo_sprites.c $(HARNESS)
	$(CC) $(CFLAGS) -o $@ demo_sprites.c hw.c plat.c map.c $(LIBS)

xcb-tile-mask: demo_mask.c $(HARNESS)
	$(CC) $(CFLAGS) -o $@ demo_mask.c hw.c plat.c map.c $(LIBS)

clean:
	rm -f xcb-tile xcb-tile-anim xcb-tile-sprites xcb-tile-mask
