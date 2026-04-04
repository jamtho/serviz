#ifndef AXES_H
#define AXES_H

#include "transform.h"
#include <stdbool.h>

#define MAX_TICKS 32

typedef struct {
    double value;
    char label[64];
} Tick;

typedef struct {
    Tick ticks[MAX_TICKS];
    int count;
} TickSet;

void axes_generate_ticks_numeric(double lo, double hi, int target_count, TickSet *out);
void axes_generate_ticks_time(double lo_us, double hi_us, int target_count, TickSet *out);
void axes_draw_grid(const TickSet *x_ticks, const TickSet *y_ticks,
                    const Viewport *vp, const ChartArea *ca);
void axes_draw_chrome(const TickSet *x_ticks, const TickSet *y_ticks,
                      const Viewport *vp, const ChartArea *ca,
                      bool x_is_time, const char *title);

#endif /* AXES_H */
