#ifndef TOOLTIP_H
#define TOOLTIP_H

#include "data.h"
#include "transform.h"
#include "raylib.h"
#include <stdbool.h>

typedef struct {
    bool active;
    uint32_t point_idx;
    int series_idx;
    double data_x;
    double data_y;
    int pixel_x, pixel_y;
} HitResult;

HitResult tooltip_hit_test(const DataSet *ds, const Viewport *vp,
                           const ChartArea *ca, int mouse_x, int mouse_y,
                           int max_dist_px);

void tooltip_draw(const HitResult *hit, const DataSet *ds,
                  bool x_is_time, const Color *palette_colors, int palette_count,
                  int screen_width, int screen_height);

#endif /* TOOLTIP_H */
