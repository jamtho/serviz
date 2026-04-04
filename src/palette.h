#ifndef PALETTE_H
#define PALETTE_H

#include "raylib.h"

#define PALETTE_SIZE 10

const Color *palette_get_default(void);
Color palette_color(int index);

#endif /* PALETTE_H */
