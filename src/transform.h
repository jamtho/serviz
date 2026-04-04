#ifndef TRANSFORM_H
#define TRANSFORM_H

typedef struct {
    double x_min, x_max;
    double y_min, y_max;
} Viewport;

typedef struct {
    int left, top;
    int width, height;
} ChartArea;

static inline double data_to_pixel_x(double val, const Viewport *vp, const ChartArea *ca) {
    double t = (val - vp->x_min) / (vp->x_max - vp->x_min);
    return ca->left + t * ca->width;
}

static inline double data_to_pixel_y(double val, const Viewport *vp, const ChartArea *ca) {
    double t = (val - vp->y_min) / (vp->y_max - vp->y_min);
    return ca->top + ca->height - t * ca->height;
}

static inline double pixel_to_data_x(double px, const Viewport *vp, const ChartArea *ca) {
    double t = (px - ca->left) / ca->width;
    return vp->x_min + t * (vp->x_max - vp->x_min);
}

static inline double pixel_to_data_y(double py, const Viewport *vp, const ChartArea *ca) {
    double t = (ca->top + ca->height - py) / ca->height;
    return vp->y_min + t * (vp->y_max - vp->y_min);
}

#endif /* TRANSFORM_H */
