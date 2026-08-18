#ifndef HW_INTERNAL_H
#define HW_INTERNAL_H

#include "hw.h"

int hw_init(void);
void hw_shutdown(void);
const Pixel *hw_visible(void);
void hw_compose(void);
void hw_key_set(int key, int down);

#endif
