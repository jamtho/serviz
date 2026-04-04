#include "axes.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

static void test_numeric_basic(void) {
    TickSet ts;
    axes_generate_ticks_numeric(0, 100, 5, &ts);
    assert(ts.count >= 3 && ts.count <= 12);
    /* Should be monotonically increasing */
    for (int i = 1; i < ts.count; i++) {
        assert(ts.ticks[i].value > ts.ticks[i-1].value);
    }
    /* All ticks within or near range */
    assert(ts.ticks[0].value >= 0.0);
    assert(ts.ticks[ts.count-1].value <= 100.0);
    printf("PASS: numeric basic\n");
}

static void test_numeric_small_range(void) {
    TickSet ts;
    axes_generate_ticks_numeric(0.001, 0.005, 5, &ts);
    assert(ts.count >= 2);
    for (int i = 1; i < ts.count; i++) {
        assert(ts.ticks[i].value > ts.ticks[i-1].value);
    }
    printf("PASS: numeric small range\n");
}

static void test_numeric_negative(void) {
    TickSet ts;
    axes_generate_ticks_numeric(-50, 50, 5, &ts);
    assert(ts.count >= 3);
    /* Should have a tick at or near zero */
    int has_zero = 0;
    for (int i = 0; i < ts.count; i++) {
        if (fabs(ts.ticks[i].value) < 1.0) has_zero = 1;
    }
    assert(has_zero);
    printf("PASS: numeric negative\n");
}

static void test_numeric_empty(void) {
    TickSet ts;
    axes_generate_ticks_numeric(10, 5, 5, &ts);
    assert(ts.count == 0);
    printf("PASS: numeric empty\n");
}

static void test_time_seconds(void) {
    /* 60 second range */
    double lo = 1704067200e6; /* 2024-01-01 00:00:00 UTC */
    double hi = lo + 60e6;
    TickSet ts;
    axes_generate_ticks_time(lo, hi, 6, &ts);
    assert(ts.count >= 2);
    /* Labels should contain colons (time format) */
    assert(strchr(ts.ticks[0].label, ':') != NULL);
    printf("PASS: time seconds\n");
}

static void test_time_hours(void) {
    double lo = 1704067200e6;
    double hi = lo + 86400e6; /* 24 hours */
    TickSet ts;
    axes_generate_ticks_time(lo, hi, 6, &ts);
    assert(ts.count >= 2);
    printf("PASS: time hours\n");
}

static void test_time_years(void) {
    double lo = 946684800e6;  /* 2000-01-01 */
    double hi = 1893456000e6; /* ~2030-01-01 */
    TickSet ts;
    axes_generate_ticks_time(lo, hi, 6, &ts);
    assert(ts.count >= 2);
    printf("PASS: time years\n");
}

int main(void) {
    test_numeric_basic();
    test_numeric_small_range();
    test_numeric_negative();
    test_numeric_empty();
    test_time_seconds();
    test_time_hours();
    test_time_years();
    printf("All axes tests passed.\n");
    return 0;
}
