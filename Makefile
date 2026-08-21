CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 $(shell pkg-config --cflags x11 xcb)
MACHINE = $(shell gcc -dumpmachine)
X11XCB = $(shell test -e /usr/lib/$(MACHINE)/libX11-xcb.so && echo -lX11-xcb || echo /usr/lib/$(MACHINE)/libX11-xcb.so.1)
LIBS = $(shell pkg-config --libs x11 xcb) $(X11XCB)

HARNESS = hw.c plat.c map.c hw.h hw_internal.h map.h
TILELIB_OBJ = hw.o plat.o map.o
.PHONY: all clean odin examples

all: xcb-tile xcb-tile-anim xcb-tile-sprites xcb-tile-mask xcb-tile-edit xcb-tile-bank xcb-tile-play examples

xcb-tile: demo.c $(HARNESS)
	$(CC) $(CFLAGS) -o $@ demo.c hw.c plat.c map.c $(LIBS)

xcb-tile-anim: demo_anim.c $(HARNESS)
	$(CC) $(CFLAGS) -o $@ demo_anim.c hw.c plat.c map.c $(LIBS)

xcb-tile-sprites: demo_sprites.c $(HARNESS)
	$(CC) $(CFLAGS) -o $@ demo_sprites.c hw.c plat.c map.c $(LIBS)

xcb-tile-mask: demo_mask.c $(HARNESS)
	$(CC) $(CFLAGS) -o $@ demo_mask.c hw.c plat.c map.c $(LIBS)

xcb-tile-edit: editor.c bank.c bank.h hw.h
	$(CC) $(CFLAGS) -o $@ editor.c bank.c $(LIBS)

xcb-tile-bank: demo_bank.c bank.c bank_hw.c $(HARNESS)
	$(CC) $(CFLAGS) -o $@ demo_bank.c bank.c bank_hw.c hw.c plat.c $(LIBS)

xcb-tile-play: demo_play.c play.c play.h bank.c bank_hw.c $(HARNESS)
	$(CC) $(CFLAGS) -o $@ demo_play.c play.c bank.c bank_hw.c hw.c plat.c $(LIBS)

mkbanks: examples/mkbanks.c bank.c bank.h hw.h
	$(CC) $(CFLAGS) -I. -o $@ examples/mkbanks.c bank.c

examples: mkbanks
	./mkbanks

hw.o: hw.c hw.h hw_internal.h
	$(CC) $(CFLAGS) -c -o $@ hw.c

plat.o: plat.c hw.h hw_internal.h
	$(CC) $(CFLAGS) -c -o $@ plat.c

map.o: map.c map.h hw.h
	$(CC) $(CFLAGS) -c -o $@ map.c

libtile.a: $(TILELIB_OBJ)
	ar rcs $@ $(TILELIB_OBJ)

xcb-tile-sprites-odin: sprites_odin/main.odin sprites_odin/hw.odin libtile.a
	odin build sprites_odin -out:$@ -extra-linker-flags:"$(LIBS)"

odin: xcb-tile-sprites-odin

clean:
	rm -f xcb-tile xcb-tile-anim xcb-tile-sprites xcb-tile-mask
	rm -f xcb-tile-edit xcb-tile-bank xcb-tile-play xcb-tile-sprites-odin mkbanks libtile.a $(TILELIB_OBJ)
