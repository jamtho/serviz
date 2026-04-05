#include "tooltip.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Binary search: find first index in sorted x array where x >= target */
static uint32_t lower_bound(const double *x, uint32_t start, uint32_t count, double target) {
    uint32_t lo = start, hi = start + count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (x[mid] < target) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

HitResult tooltip_hit_test(const DataSet *ds, const Viewport *vp,
                           const ChartArea *ca, int mouse_x, int mouse_y,
                           int max_dist_px) {
    HitResult result = {0};
    if (!ds || ds->count == 0) return result;

    double best_dist_sq = (double)max_dist_px * max_dist_px;

    /* Convert mouse to data space for x-range narrowing */
    double margin_data = (double)max_dist_px / ca->width * (vp->x_max - vp->x_min);
    double x_lo = pixel_to_data_x(mouse_x - max_dist_px, vp, ca);
    double x_hi = pixel_to_data_x(mouse_x + max_dist_px, vp, ca);
    (void)margin_data;

    for (int si = 0; si < ds->series_count; si++) {
        const Series *s = &ds->series[si];

        /* Binary search to narrow x range */
        uint32_t from = lower_bound(ds->x, s->start, s->count, x_lo);
        uint32_t to_bound = lower_bound(ds->x, s->start, s->count, x_hi);
        if (to_bound < s->start + s->count) to_bound++;
        /* Check a few extra neighbors */
        if (from > s->start) from--;
        if (to_bound < s->start + s->count) to_bound++;

        for (uint32_t i = from; i < to_bound; i++) {
            double px = data_to_pixel_x(ds->x[i], vp, ca);
            double py = data_to_pixel_y(ds->y[i], vp, ca);
            double dx = px - mouse_x;
            double dy = py - mouse_y;
            double dist_sq = dx * dx + dy * dy;

            if (dist_sq < best_dist_sq) {
                best_dist_sq = dist_sq;
                result.active = true;
                result.point_idx = i;
                result.series_idx = si;
                result.data_x = ds->x[i];
                result.data_y = ds->y[i];
                result.pixel_x = (int)px;
                result.pixel_y = (int)py;
            }
        }
    }

    return result;
}

static void format_time(double us, char *buf, int buf_size) {
    time_t secs = (time_t)(us / 1e6);
    int64_t micros = (int64_t)us;
    int64_t frac = micros % 1000000;
    if (frac < 0) frac += 1000000;

    struct tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &secs);
#else
    gmtime_r(&secs, &tm);
#endif

    if (frac != 0) {
        int ms = (int)(frac / 1000);
        snprintf(buf, buf_size, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
    } else if (tm.tm_hour || tm.tm_min || tm.tm_sec) {
        snprintf(buf, buf_size, "%04d-%02d-%02d %02d:%02d:%02d",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec);
    } else {
        snprintf(buf, buf_size, "%04d-%02d-%02d",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    }
}

void tooltip_draw(const HitResult *hit, const DataSet *ds,
                  bool x_is_time, const Color *palette_colors, int palette_count,
                  int screen_width, int screen_height) {
    if (!hit->active) return;

    Color series_color = palette_colors[hit->series_idx % palette_count];

    /* Draw indicator dot */
    DrawCircle(hit->pixel_x, hit->pixel_y, 5, series_color);
    DrawCircleLines(hit->pixel_x, hit->pixel_y, 5, WHITE);

    /* Build tooltip lines */
    char lines[MAX_EXTRA_COLS + 4][128];
    int line_count = 0;
    int max_width = 0;
    int font_size = 14;

    /* Series name (if multiple) */
    if (ds->series_count > 1) {
        snprintf(lines[line_count], sizeof(lines[0]), "%s",
                 ds->series[hit->series_idx].name);
        int w = MeasureText(lines[line_count], font_size);
        if (w > max_width) max_width = w;
        line_count++;
    }

    /* X value */
    if (x_is_time) {
        char time_str[64];
        format_time(hit->data_x, time_str, sizeof(time_str));
        snprintf(lines[line_count], sizeof(lines[0]), "x: %s", time_str);
    } else {
        snprintf(lines[line_count], sizeof(lines[0]), "x: %g", hit->data_x);
    }
    int w = MeasureText(lines[line_count], font_size);
    if (w > max_width) max_width = w;
    line_count++;

    /* Y value / OHLC values */
    if (ds->is_ohlc && ds->open && ds->close) {
        snprintf(lines[line_count], sizeof(lines[0]), "O: %g  H: %g",
                 ds->open[hit->point_idx], ds->high[hit->point_idx]);
        w = MeasureText(lines[line_count], font_size);
        if (w > max_width) max_width = w;
        line_count++;
        snprintf(lines[line_count], sizeof(lines[0]), "L: %g  C: %g",
                 ds->low[hit->point_idx], ds->close[hit->point_idx]);
        w = MeasureText(lines[line_count], font_size);
        if (w > max_width) max_width = w;
        line_count++;
    } else {
        snprintf(lines[line_count], sizeof(lines[0]), "y: %g", hit->data_y);
        w = MeasureText(lines[line_count], font_size);
        if (w > max_width) max_width = w;
        line_count++;
    }

    /* Extra columns */
    for (int e = 0; e < ds->extra_count && line_count < MAX_EXTRA_COLS + 4; e++) {
        /* Skip OHLC columns in extras since we already displayed them */
        if (ds->is_ohlc && (strcmp(ds->extras[e].name, "open") == 0 ||
            strcmp(ds->extras[e].name, "high") == 0 ||
            strcmp(ds->extras[e].name, "low") == 0 ||
            strcmp(ds->extras[e].name, "close") == 0)) continue;
        snprintf(lines[line_count], sizeof(lines[0]), "%s: %s",
                 ds->extras[e].name, ds->extras[e].values[hit->point_idx]);
        w = MeasureText(lines[line_count], font_size);
        if (w > max_width) max_width = w;
        line_count++;
    }

    /* Position tooltip */
    int pad = 8;
    int box_w = max_width + pad * 2;
    int box_h = line_count * (font_size + 4) + pad * 2;
    int tx = hit->pixel_x + 15;
    int ty = hit->pixel_y - box_h / 2;

    /* Keep on screen */
    if (tx + box_w > screen_width) tx = hit->pixel_x - 15 - box_w;
    if (ty < 0) ty = 0;
    if (ty + box_h > screen_height) ty = screen_height - box_h;

    /* Draw box */
    DrawRectangle(tx, ty, box_w, box_h, (Color){30, 30, 30, 230});
    DrawRectangleLines(tx, ty, box_w, box_h, (Color){100, 100, 100, 255});

    /* Draw text */
    for (int i = 0; i < line_count; i++) {
        Color text_color = (i == 0 && ds->series_count > 1) ? series_color : (Color){220, 220, 220, 255};
        DrawText(lines[i], tx + pad, ty + pad + i * (font_size + 4), font_size, text_color);
    }
}
