#include "spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Coverage: spec_parse from a file path (not just string). */
static void test_parse_from_file(void) {
    /* Write a temp spec file. */
    FILE *f = fopen("/tmp/opencode/serviz_test_spec.json", "wb");
    assert(f);
    fputs("{\"sql\":\"SELECT 1\",\"layers\":[{\"mark\":\"line\"}]}", f);
    fclose(f);

    Spec spec;
    int rc = spec_parse("/tmp/opencode/serviz_test_spec.json", &spec);
    assert(rc == 0);
    assert(strcmp(spec.sql, "SELECT 1") == 0);
    assert(spec.layer_count == 1);
    spec_free(&spec);
    printf("PASS: parse from file\n");
}

/* Coverage: spec_parse on a missing file returns error. */
static void test_parse_missing_file(void) {
    Spec spec;
    int rc = spec_parse("/tmp/opencode/serviz_does_not_exist.json", &spec);
    assert(rc != 0);
    printf("PASS: parse missing file\n");
}

static void test_minimal(void) {
    const char *json = "{\"sql\":\"SELECT 1\",\"layers\":[{\"mark\":\"line\"}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc == 0);
    assert(strcmp(spec.sql, "SELECT 1") == 0);
    assert(spec.layer_count == 1);
    assert(spec.layers[0].mark == MARK_LINE);
    assert(spec.layers[0].point_size == 6);
    assert(spec.title == NULL);
    spec_free(&spec);
    printf("PASS: minimal\n");
}

static void test_full(void) {
    const char *json =
        "{\"sql\":\"SELECT x, y FROM t\","
        "\"title\":\"My Chart\","
        "\"layers\":["
        "  {\"mark\":\"point\",\"scheme\":\"inferno\",\"point_size\":10,"
        "   \"name\":\"Sensor A\",\"sql\":\"SELECT a, b FROM s\"},"
        "  {\"mark\":\"line\",\"name\":\"Sensor B\"}"
        "]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc == 0);
    assert(strcmp(spec.title, "My Chart") == 0);
    assert(spec.layer_count == 2);
    assert(spec.layers[0].mark == MARK_POINT);
    assert(spec.layers[0].scheme == COLORMAP_INFERNO);
    assert(spec.layers[0].point_size == 10);
    assert(strcmp(spec.layers[0].name, "Sensor A") == 0);
    assert(strcmp(spec.layers[0].sql, "SELECT a, b FROM s") == 0);
    assert(spec.layers[1].mark == MARK_LINE);
    assert(strcmp(spec.layers[1].name, "Sensor B") == 0);
    assert(spec.layers[1].sql == NULL);
    spec_free(&spec);
    printf("PASS: full\n");
}

static void test_missing_sql(void) {
    const char *json = "{\"layers\":[{\"mark\":\"line\"}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc != 0);
    printf("PASS: missing sql\n");
}

static void test_missing_layers(void) {
    const char *json = "{\"sql\":\"SELECT 1\"}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc != 0);
    printf("PASS: missing layers\n");
}

static void test_missing_mark(void) {
    const char *json = "{\"sql\":\"SELECT 1\",\"layers\":[{}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc != 0);
    printf("PASS: missing mark\n");
}

static void test_invalid_json(void) {
    Spec spec;
    int rc = spec_parse_string("not json", &spec);
    assert(rc != 0);
    printf("PASS: invalid json\n");
}

static void test_default_scheme(void) {
    const char *json = "{\"sql\":\"SELECT 1\",\"layers\":[{\"mark\":\"line\"}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc == 0);
    assert(spec.layers[0].scheme == COLORMAP_VIRIDIS);
    spec_free(&spec);
    printf("PASS: default scheme\n");
}

static void test_point_mark(void) {
    const char *json = "{\"sql\":\"SELECT 1\",\"layers\":[{\"mark\":\"point\"}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc == 0);
    assert(spec.layers[0].mark == MARK_POINT);
    spec_free(&spec);
    printf("PASS: point mark\n");
}

static void test_bar_mark(void) {
    const char *json = "{\"sql\":\"SELECT 1\",\"layers\":[{\"mark\":\"bar\"}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc == 0);
    assert(spec.layers[0].mark == MARK_BAR);
    assert(spec.layers[0].bucket == NULL);
    spec_free(&spec);
    printf("PASS: bar mark\n");
}

static void test_histogram_with_bucket(void) {
    const char *json = "{\"sql\":\"SELECT 1\",\"layers\":[{\"mark\":\"histogram\",\"bucket\":5}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc == 0);
    assert(spec.layers[0].mark == MARK_HISTOGRAM);
    assert(strcmp(spec.layers[0].bucket, "5") == 0);
    spec_free(&spec);
    printf("PASS: histogram with bucket\n");
}

static void test_candle_with_bucket(void) {
    const char *json = "{\"sql\":\"SELECT 1\",\"layers\":[{\"mark\":\"candle\",\"bucket\":\"1 day\"}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc == 0);
    assert(spec.layers[0].mark == MARK_CANDLE);
    assert(strcmp(spec.layers[0].bucket, "1 day") == 0);
    spec_free(&spec);
    printf("PASS: candle with bucket\n");
}

static void test_histogram_missing_bucket(void) {
    const char *json = "{\"sql\":\"SELECT 1\",\"layers\":[{\"mark\":\"histogram\"}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc != 0);
    printf("PASS: histogram missing bucket\n");
}

static void test_candle_missing_bucket(void) {
    const char *json = "{\"sql\":\"SELECT 1\",\"layers\":[{\"mark\":\"candle\"}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc != 0);
    printf("PASS: candle missing bucket\n");
}

/* BUG #7 regression: unknown mark names should be rejected, not silently
 * fall back to "line". */
static void test_unknown_mark_rejected(void) {
    const char *json = "{\"sql\":\"SELECT 1\",\"layers\":[{\"mark\":\"lines\"}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc != 0);
    printf("PASS: unknown mark rejected\n");
}

static void test_typo_mark_rejected(void) {
    const char *json = "{\"sql\":\"SELECT 1\",\"layers\":[{\"mark\":\"lne\"}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc != 0);
    printf("PASS: typo mark rejected\n");
}

/* BUG #6 regression: histogram bucket <= 0 should be rejected at spec
 * parse time, not produce inf/garbage downstream. */
static void test_histogram_zero_bucket_rejected(void) {
    const char *json = "{\"sql\":\"SELECT 1\",\"layers\":[{\"mark\":\"histogram\",\"bucket\":0}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc != 0);
    printf("PASS: histogram zero bucket rejected\n");
}

static void test_histogram_negative_bucket_rejected(void) {
    const char *json = "{\"sql\":\"SELECT 1\",\"layers\":[{\"mark\":\"histogram\",\"bucket\":-5}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc != 0);
    printf("PASS: histogram negative bucket rejected\n");
}

/* BUG #6 regression: candle bucket <= 0 should be rejected. */
static void test_candle_zero_bucket_rejected(void) {
    const char *json = "{\"sql\":\"SELECT 1\",\"layers\":[{\"mark\":\"candle\",\"bucket\":0}]}";
    Spec spec;
    int rc = spec_parse_string(json, &spec);
    assert(rc != 0);
    printf("PASS: candle zero bucket rejected\n");
}

int main(void) {
    test_parse_from_file();
    test_parse_missing_file();
    test_minimal();
    test_full();
    test_missing_sql();
    test_missing_layers();
    test_missing_mark();
    test_invalid_json();
    test_default_scheme();
    test_point_mark();
    test_bar_mark();
    test_histogram_with_bucket();
    test_candle_with_bucket();
    test_histogram_missing_bucket();
    test_candle_missing_bucket();
    test_unknown_mark_rejected();
    test_typo_mark_rejected();
    test_histogram_zero_bucket_rejected();
    test_histogram_negative_bucket_rejected();
    test_candle_zero_bucket_rejected();
    printf("All spec tests passed.\n");
    return 0;
}
