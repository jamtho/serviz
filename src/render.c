#include "render.h"
#include "colormap.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static Color *pixel_buffer = NULL;
static Texture2D overlay_texture;
static Image overlay_image;
static int buf_width = 0;
static int buf_height = 0;
static bool texture_valid = false;

void render_init(int width, int height) {
    buf_width = width;
    buf_height = height;
    pixel_buffer = (Color *)calloc(width * height, sizeof(Color));
    overlay_image = GenImageColor(width, height, BLANK);
    overlay_texture = LoadTextureFromImage(overlay_image);
    texture_valid = true;
}

void render_resize(int width, int height) {
    if (width == buf_width && height == buf_height) return;

    free(pixel_buffer);
    buf_width = width;
    buf_height = height;
    pixel_buffer = (Color *)calloc(width * height, sizeof(Color));

    if (texture_valid) {
        UnloadTexture(overlay_texture);
        UnloadImage(overlay_image);
    }
    overlay_image = GenImageColor(width, height, BLANK);
    overlay_texture = LoadTextureFromImage(overlay_image);
    texture_valid = true;
}

static inline void plot_pixel(int px, int py, Color c) {
    if (px >= 0 && px < buf_width && py >= 0 && py < buf_height) {
        pixel_buffer[py * buf_width + px] = c;
    }
}

static inline void plot_point(int px, int py, int size, Color c) {
    int half = size / 2;
    for (int dy = -half; dy < size - half; dy++) {
        for (int dx = -half; dx < size - half; dx++) {
            plot_pixel(px + dx, py + dy, c);
        }
    }
}

static void draw_line_bresenham(int x0, int y0, int x1, int y1, Color c) {
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        plot_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static Color color_from_rgb(ColorRGB rgb) {
    Color c;
    c.r = rgb.r;
    c.g = rgb.g;
    c.b = rgb.b;
    c.a = 255;
    return c;
}

static void fill_rect(int x0, int y0, int x1, int y1, Color c) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int py = y0; py <= y1; py++) {
        for (int px = x0; px <= x1; px++) {
            plot_pixel(px, py, c);
        }
    }
}

/* Compute bar width in pixels from the data spacing */
static int compute_bar_width(const DataSet *ds, const Series *s,
                              const Viewport *vp, const ChartArea *ca) {
    if (s->count < 2) return 20;
    /* Find minimum spacing between consecutive x points */
    double min_gap = 1e300;
    for (uint32_t i = s->start + 1; i < s->start + s->count; i++) {
        double gap = ds->x[i] - ds->x[i - 1];
        if (gap > 0 && gap < min_gap) min_gap = gap;
    }
    double px_gap = min_gap / (vp->x_max - vp->x_min) * ca->width;
    int w = (int)(px_gap * 0.8);
    if (w < 1) w = 1;
    if (w > 60) w = 60;
    return w;
}

void render_rasterise(const Spec *spec, const DataSet *ds,
                      const Viewport *vp, const ChartArea *ca,
                      const Color *palette_colors, int palette_count) {
    if (!pixel_buffer || !ds || ds->count == 0) return;

    memset(pixel_buffer, 0, buf_width * buf_height * sizeof(Color));

    double color_range = ds->color_max - ds->color_min;
    if (color_range == 0.0) color_range = 1.0;

    for (int li = 0; li < spec->layer_count; li++) {
        const Layer *layer = &spec->layers[li];
        ColormapType cmap = layer->scheme;
        bool has_color = ds->has_color && ds->color_values != NULL;

        for (int si = 0; si < ds->series_count; si++) {
            const Series *series = &ds->series[si];
            Color series_color = palette_colors[si % palette_count];

            if (layer->mark == MARK_POINT) {
                int pt_size = layer->point_size;
                for (uint32_t i = series->start; i < series->start + series->count; i++) {
                    int px = (int)data_to_pixel_x(ds->x[i], vp, ca);
                    int py = (int)data_to_pixel_y(ds->y[i], vp, ca);

                    Color c;
                    if (has_color) {
                        float t = (float)((ds->color_values[i] - ds->color_min) / color_range);
                        c = color_from_rgb(colormap_sample(cmap, t));
                    } else {
                        c = series_color;
                    }
                    plot_point(px, py, pt_size, c);
                }
            } else if (layer->mark == MARK_LINE) {
                int prev_px = 0, prev_py = 0;
                for (uint32_t i = series->start; i < series->start + series->count; i++) {
                    int px = (int)data_to_pixel_x(ds->x[i], vp, ca);
                    int py = (int)data_to_pixel_y(ds->y[i], vp, ca);

                    if (i > series->start) {
                        Color c;
                        if (has_color) {
                            float t = (float)((ds->color_values[i] - ds->color_min) / color_range);
                            c = color_from_rgb(colormap_sample(cmap, t));
                        } else {
                            c = series_color;
                        }
                        draw_line_bresenham(prev_px, prev_py, px, py, c);
                    }
                    prev_px = px;
                    prev_py = py;
                }
            } else if (layer->mark == MARK_BAR || layer->mark == MARK_HISTOGRAM) {
                int bar_w = compute_bar_width(ds, series, vp, ca);
                int half_w = bar_w / 2;
                /* Baseline at y=0 if visible, otherwise clamp to viewport edge */
                double baseline_val = 0.0;
                if (0.0 < vp->y_min) baseline_val = vp->y_min;
                else if (0.0 > vp->y_max) baseline_val = vp->y_max;
                int baseline_py = (int)data_to_pixel_y(baseline_val, vp, ca);

                for (uint32_t i = series->start; i < series->start + series->count; i++) {
                    int px = (int)data_to_pixel_x(ds->x[i], vp, ca);
                    int py = (int)data_to_pixel_y(ds->y[i], vp, ca);

                    Color c;
                    if (has_color) {
                        float t = (float)((ds->color_values[i] - ds->color_min) / color_range);
                        c = color_from_rgb(colormap_sample(cmap, t));
                    } else {
                        c = series_color;
                    }
                    fill_rect(px - half_w, py, px + half_w, baseline_py, c);
                }
            } else if (layer->mark == MARK_CANDLE && ds->is_ohlc) {
                int bar_w = compute_bar_width(ds, series, vp, ca);
                int half_w = bar_w / 2;
                Color green = {50, 200, 50, 255};
                Color red = {220, 50, 50, 255};

                for (uint32_t i = series->start; i < series->start + series->count; i++) {
                    int px = (int)data_to_pixel_x(ds->x[i], vp, ca);
                    int py_high = (int)data_to_pixel_y(ds->high[i], vp, ca);
                    int py_low = (int)data_to_pixel_y(ds->low[i], vp, ca);
                    int py_open = (int)data_to_pixel_y(ds->open[i], vp, ca);
                    int py_close = (int)data_to_pixel_y(ds->close[i], vp, ca);

                    Color c = (ds->close[i] >= ds->open[i]) ? green : red;

                    /* Wick: thin vertical line from high to low */
                    draw_line_bresenham(px, py_high, px, py_low, c);

                    /* Body: filled rectangle from open to close */
                    fill_rect(px - half_w, py_open, px + half_w, py_close, c);
                }
            }
        }
    }

    UpdateTexture(overlay_texture, pixel_buffer);
}

void render_draw_overlay(const ChartArea *ca) {
    if (texture_valid) {
        /* Draw only the chart area portion with a scissor to clip */
        BeginScissorMode(ca->left, ca->top, ca->width, ca->height);
        DrawTexture(overlay_texture, 0, 0, WHITE);
        EndScissorMode();
    }
}

void render_shutdown(void) {
    if (texture_valid) {
        UnloadTexture(overlay_texture);
        UnloadImage(overlay_image);
        texture_valid = false;
    }
    free(pixel_buffer);
    pixel_buffer = NULL;
}
