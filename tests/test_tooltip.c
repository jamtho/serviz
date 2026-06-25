#include "tooltip.h"
#include "data.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Build a small DataSet in-memory without going through DuckDB so the
 * hit-test logic can be exercised quickly. */
static DataSet *make_dataset(uint32_t n) {
    static DataSet ds;
    static double xs[64], ys[64];
    static Series s;
    memset(&ds, 0, sizeof(ds));
    ds.x = xs;
    ds.y = ys;
    ds.count = n;
    ds.series = &s;
    ds.series_count = 1;
    s.name = "test";
    s.start = 0;
    s.count = n;
    for (uint32_t i = 0; i < n; i++) {
        xs[i] = (double)i;
        ys[i] = 0.0;
    }
    return &ds;
}

/* Edge: empty dataset must not crash and must report no hit. */
static void test_empty_dataset(void) {
    DataSet *ds = make_dataset(0);
    Viewport vp = {0.0, 10.0, -1.0, 1.0};
    ChartArea ca = {0, 0, 100, 100};
    HitResult hit = tooltip_hit_test(ds, &vp, &ca, 50, 50, 10);
    assert(!hit.active);
    printf("PASS: empty dataset\n");
}

/* Coverage: binary search narrows correctly; nearest point returned. */
static void test_nearest_point(void) {
    DataSet *ds = make_dataset(10);  /* x = 0..9, one per 10px */
    Viewport vp = {0.0, 9.0, -1.0, 1.0};
    ChartArea ca = {0, 0, 90, 100}; /* 10 px per unit x */
    /* mouse at x=35 -> data_x=3.5; nearest points are x=3 (px 30) and
     * x=4 (px 40), both 5px away. y=0 maps to pixel 50 (mid-height).
     * With max_dist 20, y-distance 0, so hit is active. */
    HitResult hit = tooltip_hit_test(ds, &vp, &ca, 35, 50, 20);
    assert(hit.active);
    /* point_idx should be 3 or 4 (both equidistant); accept either */
    assert(hit.point_idx == 3 || hit.point_idx == 4);
    printf("PASS: nearest point\n");
}

/* Coverage: mouse far from any point reports no hit. */
static void test_no_nearby_point(void) {
    DataSet *ds = make_dataset(5);
    Viewport vp = {0.0, 4.0, -1.0, 1.0};
    ChartArea ca = {0, 0, 400, 100}; /* 100 px per unit x */
    /* mouse at (0,0); nearest point x=0 maps to pixel 0 but y=0 maps to
     * pixel 50 (mid-height). Distance = 50 > max_dist 20. */
    HitResult hit = tooltip_hit_test(ds, &vp, &ca, 0, 0, 20);
    assert(!hit.active);
    printf("PASS: no nearby point\n");
}

/* Coverage: single point. */
static void test_single_point(void) {
    DataSet *ds = make_dataset(1);
    Viewport vp = {-1.0, 1.0, -1.0, 1.0};
    ChartArea ca = {0, 0, 100, 100};
    /* x=0 -> pixel 50 (center). y=0 -> pixel 50. */
    HitResult hit = tooltip_hit_test(ds, &vp, &ca, 55, 55, 20);
    assert(hit.active);
    assert(hit.point_idx == 0);
    printf("PASS: single point\n");
}

/* Coverage: points outside x-range are excluded by the search. */
static void test_out_of_range_x(void) {
    DataSet *ds = make_dataset(10);
    Viewport vp = {100.0, 109.0, -1.0, 1.0};  /* viewport far from data */
    ChartArea ca = {0, 0, 100, 100};
    HitResult hit = tooltip_hit_test(ds, &vp, &ca, 50, 50, 5);
    assert(!hit.active);
    printf("PASS: out of range x\n");
}

int main(void) {
    test_empty_dataset();
    test_nearest_point();
    test_no_nearby_point();
    test_single_point();
    test_out_of_range_x();
    printf("All tooltip tests passed.\n");
    return 0;
}
