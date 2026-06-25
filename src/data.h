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

    /* OHLC data (populated by data_load_ohlc, NULL otherwise) */
    double *open;
    double *high;
    double *low;
    double *close;
    bool is_ohlc;
} DataSet;

int data_load(const char *sql, DataSet *out);
int data_load_ohlc(const char *user_sql, const char *bucket, DataSet *out);
int data_load_histogram(const char *user_sql, double bucket_width, DataSet *out);

/* Load data for a layer, dispatching based on mark type.
 * For MARK_CANDLE/MARK_HISTOGRAM, uses the layer's bucket.
 * sql_override takes precedence over spec_sql. */
int data_load_for_layer(const char *sql_override, const char *spec_sql,
                        int mark, const char *bucket, DataSet *out);

void data_free(DataSet *ds);

#endif /* DATA_H */
