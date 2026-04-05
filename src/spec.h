#ifndef SPEC_H
#define SPEC_H

#include "colormap.h"

#define MAX_LAYERS 16

typedef enum {
    MARK_LINE = 0,
    MARK_POINT,
    MARK_BAR,
    MARK_HISTOGRAM,
    MARK_CANDLE
} MarkType;

typedef struct {
    MarkType mark;
    ColormapType scheme;
    int point_size;
    char *name;
    char *sql;
    char *bucket;   /* e.g. "1 day", "4 hour", or "2.5" for numeric */
} Layer;

typedef struct {
    char *sql;
    char *title;
    Layer layers[MAX_LAYERS];
    int layer_count;
} Spec;

int spec_parse_string(const char *json, Spec *out);
int spec_parse(const char *filepath, Spec *out);
void spec_free(Spec *spec);

#endif /* SPEC_H */
