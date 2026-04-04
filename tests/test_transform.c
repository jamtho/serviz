#include "transform.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

#define APPROX(a, b) (fabs((a) - (b)) < 1e-9)

static void test_roundtrip_x(void) {
    Viewport vp = {0.0, 100.0, 0.0, 50.0};
    ChartArea ca = {80, 40, 800, 600};

    for (double x = 0.0; x <= 100.0; x += 10.0) {
        double px = data_to_pixel_x(x, &vp, &ca);
        double recovered = pixel_to_data_x(px, &vp, &ca);
        assert(APPROX(recovered, x));
    }
    printf("PASS: roundtrip x\n");
}

static void test_roundtrip_y(void) {
    Viewport vp = {0.0, 100.0, 0.0, 50.0};
    ChartArea ca = {80, 40, 800, 600};

    for (double y = 0.0; y <= 50.0; y += 5.0) {
        double py = data_to_pixel_y(y, &vp, &ca);
        double recovered = pixel_to_data_y(py, &vp, &ca);
        assert(APPROX(recovered, y));
    }
    printf("PASS: roundtrip y\n");
}

static void test_boundaries(void) {
    Viewport vp = {10.0, 20.0, 5.0, 15.0};
    ChartArea ca = {80, 40, 800, 600};

    assert(APPROX(data_to_pixel_x(10.0, &vp, &ca), 80.0));
    assert(APPROX(data_to_pixel_x(20.0, &vp, &ca), 880.0));
    assert(APPROX(data_to_pixel_y(15.0, &vp, &ca), 40.0));
    assert(APPROX(data_to_pixel_y(5.0, &vp, &ca), 640.0));
    printf("PASS: boundaries\n");
}

static void test_y_inversion(void) {
    Viewport vp = {0.0, 100.0, 0.0, 100.0};
    ChartArea ca = {0, 0, 1000, 1000};

    double py_high = data_to_pixel_y(80.0, &vp, &ca);
    double py_low = data_to_pixel_y(20.0, &vp, &ca);
    assert(py_high < py_low); /* Higher data value = lower pixel y (closer to top) */
    printf("PASS: y inversion\n");
}

static void test_midpoint(void) {
    Viewport vp = {0.0, 100.0, 0.0, 100.0};
    ChartArea ca = {0, 0, 1000, 1000};

    assert(APPROX(data_to_pixel_x(50.0, &vp, &ca), 500.0));
    assert(APPROX(data_to_pixel_y(50.0, &vp, &ca), 500.0));
    printf("PASS: midpoint\n");
}

int main(void) {
    test_roundtrip_x();
    test_roundtrip_y();
    test_boundaries();
    test_y_inversion();
    test_midpoint();
    printf("All transform tests passed.\n");
    return 0;
}
