#include "data.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static void test_basic_xy(void) {
    DataSet ds;
    int rc = data_load("SELECT i AS x, i*2.0 AS y FROM range(10) t(i)", &ds);
    assert(rc == 0);
    assert(ds.count == 10);
    assert(ds.series_count == 1);
    assert(!ds.x_is_time);
    assert(!ds.has_color);
    assert(ds.x_min == 0.0);
    assert(ds.x_max == 9.0);
    /* sorted by x */
    assert(ds.x[0] == 0.0);
    assert(ds.x[9] == 9.0);
    data_free(&ds);
    printf("PASS: basic xy\n");
}

static void test_column_inference_value(void) {
    DataSet ds;
    int rc = data_load("SELECT i AS timestamp, i*3.0 AS value FROM range(5) t(i)", &ds);
    assert(rc == 0);
    assert(ds.count == 5);
    assert(!ds.x_is_time); /* 'timestamp' name but integer type = not time */
    data_free(&ds);
    printf("PASS: column inference value\n");
}

static void test_positional_fallback(void) {
    DataSet ds;
    int rc = data_load("SELECT i AS a, i*2.0 AS b FROM range(5) t(i)", &ds);
    assert(rc == 0);
    assert(ds.count == 5);
    data_free(&ds);
    printf("PASS: positional fallback\n");
}

static void test_series_column(void) {
    DataSet ds;
    int rc = data_load(
        "SELECT * FROM (VALUES "
        "(1, 10.0, 'A'), (2, 20.0, 'A'), (3, 30.0, 'A'),"
        "(1, 15.0, 'B'), (2, 25.0, 'B'), (3, 35.0, 'B')"
        ") AS t(x, y, series)", &ds);
    assert(rc == 0);
    assert(ds.count == 6);
    assert(ds.series_count == 2);
    /* Series should be grouped */
    assert(ds.series[0].count == 3);
    assert(ds.series[1].count == 3);
    data_free(&ds);
    printf("PASS: series column\n");
}

static void test_color_column(void) {
    DataSet ds;
    int rc = data_load("SELECT i AS x, i*2.0 AS y, i*0.1 AS color FROM range(10) t(i)", &ds);
    assert(rc == 0);
    assert(ds.has_color);
    assert(ds.count == 10);
    data_free(&ds);
    printf("PASS: color column\n");
}

static void test_extra_columns(void) {
    DataSet ds;
    int rc = data_load("SELECT i AS x, i*2.0 AS y, i*3.0 AS extra1 FROM range(5) t(i)", &ds);
    assert(rc == 0);
    assert(ds.extra_count == 1);
    assert(strcmp(ds.extras[0].name, "extra1") == 0);
    assert(ds.extras[0].values[0] != NULL);
    data_free(&ds);
    printf("PASS: extra columns\n");
}

static void test_empty_result(void) {
    DataSet ds;
    int rc = data_load("SELECT 1 AS x, 2.0 AS y WHERE false", &ds);
    assert(rc == 0);
    assert(ds.count == 0);
    data_free(&ds);
    printf("PASS: empty result\n");
}

static void test_bad_sql(void) {
    DataSet ds;
    int rc = data_load("THIS IS NOT SQL", &ds);
    assert(rc != 0);
    printf("PASS: bad sql\n");
}

static void test_sorted_by_x(void) {
    DataSet ds;
    /* Values inserted out of order */
    int rc = data_load(
        "SELECT * FROM (VALUES (3, 30.0), (1, 10.0), (2, 20.0)) AS t(x, y)", &ds);
    assert(rc == 0);
    assert(ds.x[0] == 1.0);
    assert(ds.x[1] == 2.0);
    assert(ds.x[2] == 3.0);
    assert(ds.y[0] == 10.0);
    assert(ds.y[1] == 20.0);
    assert(ds.y[2] == 30.0);
    data_free(&ds);
    printf("PASS: sorted by x\n");
}

static void test_case_insensitive_cols(void) {
    DataSet ds;
    int rc = data_load("SELECT i AS Time, i*2.0 AS Value FROM range(5) t(i)", &ds);
    assert(rc == 0);
    assert(ds.count == 5);
    data_free(&ds);
    printf("PASS: case insensitive cols\n");
}

static void test_histogram(void) {
    DataSet ds;
    /* 10 values: 0,1,2,...,9. Bucket width 5 -> two bins: [0,5) and [5,10) */
    int rc = data_load_histogram("SELECT i AS x, i AS y FROM range(10) t(i)", 5.0, &ds);
    assert(rc == 0);
    assert(ds.count == 2);
    assert(!ds.x_is_time);
    /* Bins at 0 and 5 */
    assert(ds.x[0] == 0.0);
    assert(ds.x[1] == 5.0);
    /* 5 values in each bin */
    assert(ds.y[0] == 5.0);
    assert(ds.y[1] == 5.0);
    data_free(&ds);
    printf("PASS: histogram\n");
}

static void test_ohlc_numeric(void) {
    DataSet ds;
    /* 20 points, bucket width 10. Should produce 2 candles. */
    int rc = data_load_ohlc(
        "SELECT i AS x, (10 + i % 5) AS y FROM range(20) t(i)",
        "10", &ds);
    assert(rc == 0);
    assert(ds.is_ohlc);
    assert(ds.open != NULL);
    assert(ds.high != NULL);
    assert(ds.low != NULL);
    assert(ds.close != NULL);
    assert(ds.count == 2);
    data_free(&ds);
    printf("PASS: ohlc numeric\n");
}

/* BUG #6 regression: zero bucket width must be rejected at the data layer
 * (defence in depth — spec_parse also rejects, but data_load_histogram
 * must not produce inf/garbage if called directly). */
static void test_histogram_zero_bucket_rejected(void) {
    DataSet ds;
    int rc = data_load_histogram("SELECT i AS x, i AS y FROM range(10) t(i)", 0.0, &ds);
    assert(rc != 0);
    printf("PASS: histogram zero bucket rejected\n");
}

/* Edge case: single point. */
static void test_single_point(void) {
    DataSet ds;
    int rc = data_load("SELECT 1 AS x, 2.0 AS y", &ds);
    assert(rc == 0);
    assert(ds.count == 1);
    assert(ds.series_count == 1);
    assert(ds.x[0] == 1.0);
    assert(ds.y[0] == 2.0);
    data_free(&ds);
    printf("PASS: single point\n");
}

int main(void) {
    test_basic_xy();
    test_column_inference_value();
    test_positional_fallback();
    test_series_column();
    test_color_column();
    test_extra_columns();
    test_empty_result();
    test_bad_sql();
    test_sorted_by_x();
    test_case_insensitive_cols();
    test_histogram();
    test_ohlc_numeric();
    test_histogram_zero_bucket_rejected();
    test_single_point();
    printf("All data tests passed.\n");
    return 0;
}
