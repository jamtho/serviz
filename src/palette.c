#include "palette.h"

/* Tableau 10 */
static const Color default_palette[PALETTE_SIZE] = {
    {  31, 119, 180, 255 },
    { 255, 127,  14, 255 },
    {  44, 160,  44, 255 },
    { 214,  39,  40, 255 },
    { 148, 103, 189, 255 },
    { 140,  86,  75, 255 },
    { 227, 119, 194, 255 },
    { 127, 127, 127, 255 },
    { 188, 189,  34, 255 },
    {  23, 190, 207, 255 },
};

const Color *palette_get_default(void) {
    return default_palette;
}

Color palette_color(int index) {
    if (index < 0) index = 0;
    return default_palette[index % PALETTE_SIZE];
}
