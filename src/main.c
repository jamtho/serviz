#include "spec.h"
#include "data.h"
#include "axes.h"
#include "render.h"
#include "tooltip.h"
#include "palette.h"
#include "transform.h"
#include "cli.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 800
#define MARGIN_LEFT   80
#define MARGIN_RIGHT  20
#define MARGIN_TOP    40
#define MARGIN_BOTTOM 50
#define SCREENSHOT_DELAY_FRAMES 10

static char *read_stdin(void) {
    size_t cap = 4096;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;

    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, stdin)) > 0) {
        len += n;
        if (len + 1 >= cap) {
            cap *= 2;
            char *tmp = (char *)realloc(buf, cap);
            if (!tmp) { free(buf); return NULL; }
            buf = tmp;
        }
    }
    buf[len] = '\0';
    return buf;
}

static void auto_fit_y(Viewport *vp, const DataSet *datasets, int count) {
    double y_lo = DBL_MAX, y_hi = -DBL_MAX;
    for (int di = 0; di < count; di++) {
        const DataSet *ds = &datasets[di];
        if (ds->count == 0) continue;
        for (int si = 0; si < ds->series_count; si++) {
            const Series *s = &ds->series[si];
            for (uint32_t i = s->start; i < s->start + s->count; i++) {
                if (ds->x[i] < vp->x_min || ds->x[i] > vp->x_max) continue;
                if (ds->is_ohlc && ds->low && ds->high) {
                    if (ds->low[i] < y_lo) y_lo = ds->low[i];
                    if (ds->high[i] > y_hi) y_hi = ds->high[i];
                } else {
                    if (ds->y[i] < y_lo) y_lo = ds->y[i];
                    if (ds->y[i] > y_hi) y_hi = ds->y[i];
                }
            }
        }
    }
    if (y_lo < y_hi) {
        double margin = (y_hi - y_lo) * 0.05;
        if (margin == 0.0) margin = 1.0;
        vp->y_min = y_lo - margin;
        vp->y_max = y_hi + margin;
    }
}

static ChartArea compute_chart_area(int w, int h) {
    ChartArea ca;
    ca.left = MARGIN_LEFT;
    ca.top = MARGIN_TOP;
    ca.width = w - MARGIN_LEFT - MARGIN_RIGHT;
    ca.height = h - MARGIN_TOP - MARGIN_BOTTOM;
    if (ca.width < 1) ca.width = 1;
    if (ca.height < 1) ca.height = 1;
    return ca;
}

int main(int argc, char *argv[]) {
    CliArgs args;
    if (cli_parse(argc, argv, &args) != 0) return 1;

    if (args.help) {
        cli_print_usage(stdout);
        return 0;
    }
    if (args.version) {
        printf("serviz 0.1.0\n");
        return 0;
    }

    const char *spec_file = args.spec_file;
    const char *screenshot_path = args.screenshot_path;

    Spec spec;
    if (spec_file) {
        if (spec_parse(spec_file, &spec) != 0) return 1;
    } else {
        char *json = read_stdin();
        if (!json || json[0] == '\0') {
            cli_print_usage(stderr);
            free(json);
            return 1;
        }
        if (spec_parse_string(json, &spec) != 0) {
            free(json);
            return 1;
        }
        free(json);
    }

    fprintf(stderr, "Spec parsed: %d layers\n", spec.layer_count);

    /* Load one dataset per layer */
    DataSet datasets[MAX_LAYERS];
    int loaded_count = 0;

    for (int i = 0; i < spec.layer_count; i++) {
        const Layer *layer = &spec.layers[i];
        int rc = data_load_for_layer(layer->sql, spec.sql,
                                     (int)layer->mark, layer->bucket,
                                     &datasets[i]);
        if (rc != 0) {
            fprintf(stderr, "Error: failed to load data for layer %d\n", i);
            for (int j = 0; j < i; j++) data_free(&datasets[j]);
            spec_free(&spec);
            return 1;
        }
        if (datasets[i].count == 0) {
            fprintf(stderr, "Warning: layer %d returned 0 rows\n", i);
        }
        loaded_count++;
    }

    if (loaded_count == 0) {
        fprintf(stderr, "Error: no layers loaded\n");
        spec_free(&spec);
        return 1;
    }

    /* Check at least one layer has data */
    int has_data = 0;
    for (int i = 0; i < loaded_count; i++) {
        if (datasets[i].count > 0) { has_data = 1; break; }
    }
    if (!has_data) {
        fprintf(stderr, "Error: no data rows loaded\n");
        for (int i = 0; i < loaded_count; i++) data_free(&datasets[i]);
        spec_free(&spec);
        return 1;
    }

    /* Compute viewport across all datasets */
    Viewport vp;
    double x_min = datasets[0].x_min, x_max = datasets[0].x_max;
    double y_min = datasets[0].y_min, y_max = datasets[0].y_max;
    for (int i = 1; i < loaded_count; i++) {
        if (datasets[i].count == 0) continue;
        if (datasets[i].x_min < x_min) x_min = datasets[i].x_min;
        if (datasets[i].x_max > x_max) x_max = datasets[i].x_max;
        if (datasets[i].y_min < y_min) y_min = datasets[i].y_min;
        if (datasets[i].y_max > y_max) y_max = datasets[i].y_max;
    }
    double x_margin = (x_max - x_min) * 0.05;
    double y_margin = (y_max - y_min) * 0.05;
    if (x_margin == 0.0) x_margin = 1.0;
    if (y_margin == 0.0) y_margin = 1.0;
    vp.x_min = x_min - x_margin;
    vp.x_max = x_max + x_margin;
    vp.y_min = y_min - y_margin;
    vp.y_max = y_max + y_margin;

    /* Use first dataset's x_is_time as representative */
    bool x_is_time = datasets[0].x_is_time;

    /* Palette */
    const Color *palette = palette_get_default();

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "serviz");
    SetTargetFPS(60);

    int w = GetScreenWidth();
    int h = GetScreenHeight();
    ChartArea ca = compute_chart_area(w, h);
    render_init(w, h);

    int frame_count = 0;
    int viewport_dirty = 1;

    TickSet x_ticks = {0}, y_ticks = {0};

    while (!WindowShouldClose()) {
        int new_w = GetScreenWidth();
        int new_h = GetScreenHeight();

        if (new_w != w || new_h != h) {
            w = new_w;
            h = new_h;
            ca = compute_chart_area(w, h);
            render_resize(w, h);
            viewport_dirty = 1;
        }

        /* Pan (left mouse drag) */
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 delta = GetMouseDelta();
            if (delta.x != 0 || delta.y != 0) {
                double dx = -(delta.x / ca.width) * (vp.x_max - vp.x_min);
                double dy = (delta.y / ca.height) * (vp.y_max - vp.y_min);
                vp.x_min += dx;
                vp.x_max += dx;
                vp.y_min += dy;
                vp.y_max += dy;
                viewport_dirty = 1;
            }
        }

        /* Zoom (scroll wheel, x-axis only, centered on cursor) */
        float scroll = GetMouseWheelMove();
        if (scroll != 0) {
            Vector2 mouse = GetMousePosition();
            double data_x = pixel_to_data_x(mouse.x, &vp, &ca);

            double factor = (scroll > 0) ? 0.8 : 1.25;
            double x_range = vp.x_max - vp.x_min;
            double new_range = x_range * factor;

            double t = (data_x - vp.x_min) / x_range;
            vp.x_min = data_x - t * new_range;
            vp.x_max = data_x + (1.0 - t) * new_range;

            auto_fit_y(&vp, datasets, loaded_count);
            viewport_dirty = 1;
        }

        /* Reset viewport on 'R' */
        if (IsKeyPressed(KEY_R)) {
            vp.x_min = x_min - x_margin;
            vp.x_max = x_max + x_margin;
            vp.y_min = y_min - y_margin;
            vp.y_max = y_max + y_margin;
            viewport_dirty = 1;
        }

        /* Regenerate on dirty */
        if (viewport_dirty) {
            if (x_is_time) {
                axes_generate_ticks_time(vp.x_min, vp.x_max, 8, &x_ticks);
            } else {
                axes_generate_ticks_numeric(vp.x_min, vp.x_max, 8, &x_ticks);
            }
            axes_generate_ticks_numeric(vp.y_min, vp.y_max, 6, &y_ticks);
            render_rasterise(&spec, datasets, &vp, &ca, palette, PALETTE_SIZE);
            viewport_dirty = 0;
        }

        /* Draw */
        BeginDrawing();
        ClearBackground((Color){30, 30, 30, 255});

        axes_draw_grid(&x_ticks, &y_ticks, &vp, &ca);
        render_draw_overlay(&ca);
        axes_draw_chrome(&x_ticks, &y_ticks, &vp, &ca, x_is_time, spec.title);

        /* Tooltip: search across all datasets for the nearest point */
        Vector2 mouse = GetMousePosition();
        if (mouse.x >= ca.left && mouse.x <= ca.left + ca.width &&
            mouse.y >= ca.top && mouse.y <= ca.top + ca.height) {
            HitResult best = {0};
            double best_dist = 1e18;
            for (int i = 0; i < loaded_count; i++) {
                if (datasets[i].count == 0) continue;
                HitResult hit = tooltip_hit_test(&datasets[i], &vp, &ca,
                                                  (int)mouse.x, (int)mouse.y, 20);
                if (hit.active) {
                    double dx = hit.pixel_x - mouse.x;
                    double dy = hit.pixel_y - mouse.y;
                    double dist = dx * dx + dy * dy;
                    if (dist < best_dist) {
                        best_dist = dist;
                        best = hit;
                        best.series_idx = i; /* layer index for color */
                    }
                }
            }
            if (best.active) {
                tooltip_draw(&best, &datasets[best.series_idx], x_is_time,
                             palette, PALETTE_SIZE, w, h);
            }
        }

        /* HUD */
        DrawFPS(10, 10);
        uint32_t total_points = 0;
        for (int i = 0; i < loaded_count; i++) total_points += datasets[i].count;
        char info[128];
        snprintf(info, sizeof(info), "Points: %u  Layers: %d", total_points, loaded_count);
        DrawText(info, 10, 30, 14, GREEN);

        /* Legend: show layer names (if set) or series names */
        int ly = 50;
        int legend_entries = 0;
        for (int i = 0; i < loaded_count && legend_entries < PALETTE_SIZE; i++) {
            const DataSet *ds = &datasets[i];
            if (ds->count == 0) continue;
            const char *name = spec.layers[i].name;
            if (!name && ds->series_count == 1) name = ds->series[0].name;
            if (!name) continue;

            Color c = palette_color(legend_entries);
            DrawRectangle(10, ly, 12, 12, c);
            DrawText(name, 26, ly, 14, (Color){200, 200, 200, 255});
            ly += 18;
            legend_entries++;
        }

        EndDrawing();

        frame_count++;
        if (screenshot_path && frame_count == SCREENSHOT_DELAY_FRAMES) {
            TakeScreenshot(screenshot_path);
            fprintf(stderr, "Screenshot saved to %s\n", screenshot_path);
            break;
        }
    }

    render_shutdown();
    for (int i = 0; i < loaded_count; i++) data_free(&datasets[i]);
    spec_free(&spec);
    CloseWindow();

    return 0;
}
