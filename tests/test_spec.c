#include "spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

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

int main(void) {
    test_minimal();
    test_full();
    test_missing_sql();
    test_missing_layers();
    test_missing_mark();
    test_invalid_json();
    test_default_scheme();
    test_point_mark();
    printf("All spec tests passed.\n");
    return 0;
}
