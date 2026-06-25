#ifndef RENDER_H
#define RENDER_H

#include "data.h"
#include "spec.h"
#include "transform.h"
#include "raylib.h"

void render_init(int width, int height);
void render_resize(int width, int height);
void render_rasterise(const Spec *spec, const DataSet *datasets,
                      const Viewport *vp, const ChartArea *ca,
                      const Color *palette_colors, int palette_count);
void render_draw_overlay(const ChartArea *ca);
void render_shutdown(void);

#endif /* RENDER_H */
