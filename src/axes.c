#include "axes.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Nice number algorithm: round to 1, 2, or 5 * 10^n */
static double nice_step(double raw_step) {
    double exp_val = floor(log10(raw_step));
    double base = pow(10.0, exp_val);
    double frac = raw_step / base;

    if (frac <= 1.5) return base;
    if (frac <= 3.5) return 2.0 * base;
    if (frac <= 7.5) return 5.0 * base;
    return 10.0 * base;
}

void axes_generate_ticks_numeric(double lo, double hi, int target_count, TickSet *out) {
    out->count = 0;
    if (hi <= lo || target_count < 1) return;

    double range = hi - lo;
    double raw_step = range / target_count;
    double step = nice_step(raw_step);

    double start = ceil(lo / step) * step;

    /* Determine label precision from step size */
    int decimals = 0;
    if (step < 1.0) {
        decimals = (int)ceil(-log10(step));
        if (decimals > 10) decimals = 10;
    }

    for (double v = start; v <= hi + step * 0.01 && out->count < MAX_TICKS; v += step) {
        Tick *t = &out->ticks[out->count];
        if (fabs(v) < step * 0.01) v = 0.0; /* Snap near-zero to zero */
        t->value = v;

        if (fabs(v) >= 1e7 || (fabs(v) > 0 && fabs(v) < 1e-3)) {
            snprintf(t->label, sizeof(t->label), "%.3g", v);
        } else if (decimals == 0) {
            snprintf(t->label, sizeof(t->label), "%.0f", v);
        } else {
            snprintf(t->label, sizeof(t->label), "%.*f", decimals, v);
        }
        out->count++;
    }
}

/* Calendar decomposition from microseconds since epoch */
static void micros_to_cal(double us, int *year, int *month, int *day,
                           int *hour, int *min, int *sec) {
    time_t secs = (time_t)(us / 1e6);
    struct tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &secs);
#else
    gmtime_r(&secs, &tm);
#endif
    *year = tm.tm_year + 1900;
    *month = tm.tm_mon + 1;
    *day = tm.tm_mday;
    *hour = tm.tm_hour;
    *min = tm.tm_min;
    *sec = tm.tm_sec;
}

/* Calendar to microseconds since epoch */
static double cal_to_micros(int year, int month, int day, int hour, int min, int sec) {
    struct tm tm = {0};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
#ifdef _WIN32
    time_t t = _mkgmtime(&tm);
#else
    time_t t = timegm(&tm);
#endif
    return (double)t * 1e6;
}

/* Time interval table: microseconds per interval, plus a label format indicator */
typedef struct {
    double micros;
    int type; /* 0=sub-minute, 1=minute, 2=hour, 3=day, 4=month, 5=year */
} TimeInterval;

static const TimeInterval time_intervals[] = {
    { 1e6,           0 }, /* 1s */
    { 5e6,           0 }, /* 5s */
    { 15e6,          0 }, /* 15s */
    { 30e6,          0 }, /* 30s */
    { 60e6,          1 }, /* 1min */
    { 300e6,         1 }, /* 5min */
    { 900e6,         1 }, /* 15min */
    { 1800e6,        1 }, /* 30min */
    { 3600e6,        2 }, /* 1h */
    { 10800e6,       2 }, /* 3h */
    { 21600e6,       2 }, /* 6h */
    { 43200e6,       2 }, /* 12h */
    { 86400e6,       3 }, /* 1d */
    { 604800e6,      3 }, /* 7d */
    { 2592000e6,     4 }, /* ~30d / 1mo */
    { 7776000e6,     4 }, /* ~90d / 3mo */
    { 15552000e6,    4 }, /* ~180d / 6mo */
    { 31536000e6,    5 }, /* ~1y */
    { 157680000e6,   5 }, /* ~5y */
    { 315360000e6,   5 }, /* ~10y */
};
#define NUM_TIME_INTERVALS (sizeof(time_intervals) / sizeof(time_intervals[0]))

void axes_generate_ticks_time(double lo_us, double hi_us, int target_count, TickSet *out) {
    out->count = 0;
    if (hi_us <= lo_us || target_count < 1) return;

    double range = hi_us - lo_us;
    double raw_interval = range / target_count;

    /* Find best matching interval */
    int best = 0;
    double best_diff = fabs(time_intervals[0].micros - raw_interval);
    for (int i = 1; i < (int)NUM_TIME_INTERVALS; i++) {
        double diff = fabs(time_intervals[i].micros - raw_interval);
        if (diff < best_diff) {
            best_diff = diff;
            best = i;
        }
    }

    double interval = time_intervals[best].micros;
    int type = time_intervals[best].type;

    /* Generate ticks at interval boundaries */
    if (type <= 3) {
        /* Sub-month: snap to multiples of interval */
        double start = ceil(lo_us / interval) * interval;
        for (double v = start; v <= hi_us + interval * 0.01 && out->count < MAX_TICKS; v += interval) {
            Tick *t = &out->ticks[out->count];
            t->value = v;

            int y, mo, d, h, mi, s;
            micros_to_cal(v, &y, &mo, &d, &h, &mi, &s);

            switch (type) {
                case 0: /* seconds */
                    snprintf(t->label, sizeof(t->label), "%02d:%02d:%02d", h, mi, s);
                    break;
                case 1: /* minutes */
                    snprintf(t->label, sizeof(t->label), "%02d:%02d", h, mi);
                    break;
                case 2: /* hours */
                    snprintf(t->label, sizeof(t->label), "%02d:%02d", h, mi);
                    break;
                case 3: /* days */
                    snprintf(t->label, sizeof(t->label), "%d-%02d-%02d", y, mo, d);
                    break;
            }
            out->count++;
        }
    } else if (type == 4) {
        /* Month-level: step by months */
        int months_step = (int)(interval / 2592000e6 + 0.5);
        if (months_step < 1) months_step = 1;

        int y, mo, d, h, mi, s;
        micros_to_cal(lo_us, &y, &mo, &d, &h, &mi, &s);
        /* Snap to start of month */
        mo = ((mo - 1) / months_step) * months_step + 1;

        for (;;) {
            double v = cal_to_micros(y, mo, 1, 0, 0, 0);
            if (v > hi_us || out->count >= MAX_TICKS) break;
            if (v >= lo_us) {
                Tick *t = &out->ticks[out->count];
                t->value = v;
                static const char *mon_names[] = {
                    "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
                };
                snprintf(t->label, sizeof(t->label), "%s %d", mon_names[mo], y);
                out->count++;
            }
            mo += months_step;
            while (mo > 12) { mo -= 12; y++; }
        }
    } else {
        /* Year-level */
        int years_step = (int)(interval / 31536000e6 + 0.5);
        if (years_step < 1) years_step = 1;

        int y, mo, d, h, mi, s;
        micros_to_cal(lo_us, &y, &mo, &d, &h, &mi, &s);
        y = (y / years_step) * years_step;

        for (;;) {
            double v = cal_to_micros(y, 1, 1, 0, 0, 0);
            if (v > hi_us || out->count >= MAX_TICKS) break;
            if (v >= lo_us) {
                Tick *t = &out->ticks[out->count];
                t->value = v;
                snprintf(t->label, sizeof(t->label), "%d", y);
                out->count++;
            }
            y += years_step;
        }
    }
}

void axes_draw_grid(const TickSet *x_ticks, const TickSet *y_ticks,
                    const Viewport *vp, const ChartArea *ca) {
    Color grid_color = {60, 60, 60, 255};

    for (int i = 0; i < x_ticks->count; i++) {
        int px = (int)data_to_pixel_x(x_ticks->ticks[i].value, vp, ca);
        if (px > ca->left && px < ca->left + ca->width) {
            DrawLine(px, ca->top, px, ca->top + ca->height, grid_color);
        }
    }
    for (int i = 0; i < y_ticks->count; i++) {
        int py = (int)data_to_pixel_y(y_ticks->ticks[i].value, vp, ca);
        if (py > ca->top && py < ca->top + ca->height) {
            DrawLine(ca->left, py, ca->left + ca->width, py, grid_color);
        }
    }
}

void axes_draw_chrome(const TickSet *x_ticks, const TickSet *y_ticks,
                      const Viewport *vp, const ChartArea *ca,
                      bool x_is_time, const char *title) {
    (void)x_is_time;
    Color axis_color = {180, 180, 180, 255};
    Color label_color = {160, 160, 160, 255};
    int font_size = 14;

    /* Axis frame */
    DrawLine(ca->left, ca->top, ca->left, ca->top + ca->height, axis_color);
    DrawLine(ca->left, ca->top + ca->height,
             ca->left + ca->width, ca->top + ca->height, axis_color);

    /* X tick labels */
    for (int i = 0; i < x_ticks->count; i++) {
        int px = (int)data_to_pixel_x(x_ticks->ticks[i].value, vp, ca);
        if (px >= ca->left && px <= ca->left + ca->width) {
            DrawLine(px, ca->top + ca->height, px, ca->top + ca->height + 5, axis_color);
            int tw = MeasureText(x_ticks->ticks[i].label, font_size);
            DrawText(x_ticks->ticks[i].label, px - tw / 2,
                     ca->top + ca->height + 8, font_size, label_color);
        }
    }

    /* Y tick labels */
    for (int i = 0; i < y_ticks->count; i++) {
        int py = (int)data_to_pixel_y(y_ticks->ticks[i].value, vp, ca);
        if (py >= ca->top && py <= ca->top + ca->height) {
            DrawLine(ca->left - 5, py, ca->left, py, axis_color);
            int tw = MeasureText(y_ticks->ticks[i].label, font_size);
            DrawText(y_ticks->ticks[i].label, ca->left - tw - 8,
                     py - font_size / 2, font_size, label_color);
        }
    }

    /* Title */
    if (title && title[0]) {
        int tw = MeasureText(title, 18);
        DrawText(title, ca->left + (ca->width - tw) / 2, ca->top - 28, 18, axis_color);
    }
}
