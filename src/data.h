#ifndef DATA_H
#define DATA_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_EXTRA_COLS 16

typedef struct {
    char *name;
    uint32_t start;
    uint32_t count;
} Series;

typedef struct {
    char *name;
    char **values;
} ExtraColumn;

typedef struct {
    double *x;
    double *y;
    double *color_values;
    uint32_t count;

    double x_min, x_max;
    double y_min, y_max;
    double color_min, color_max;

    bool x_is_time;
    bool has_color;

    Series *series;
    int series_count;

    ExtraColumn extras[MAX_EXTRA_COLS];
    int extra_count;
} DataSet;

int data_load(const char *sql, DataSet *out);
void data_free(DataSet *ds);

#endif /* DATA_H */
