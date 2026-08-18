#ifndef MAP_H
#define MAP_H

#include "hw.h"

enum {
    T_SKY = 0,
    T_CLOUD,
    T_STAR,
    T_GRASS,
    T_DIRT,
    T_STONE,
    T_WATER,
    T_BRICK,
    T_COUNT
};

void map_tileset(void);
int map_at(int tx, int ty);
void map_fill(void);
void map_fill_col(int col);
void map_fill_row(int row);
void map_apply_coarse(HwCoarse c);
void map_scroll_keys(float dt);

#endif
