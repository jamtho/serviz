#include "cli.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_no_args(void) {
    CliArgs args;
    char *argv[] = {"serviz"};
    int rc = cli_parse(1, argv, &args);
    assert(rc == 0);
    assert(args.spec_file == NULL);
    assert(args.screenshot_path == NULL);
    assert(!args.help);
    assert(!args.version);
    printf("PASS: no args\n");
}

static void test_spec_file_only(void) {
    CliArgs args;
    char *argv[] = {"serviz", "spec.json"};
    int rc = cli_parse(2, argv, &args);
    assert(rc == 0);
    assert(strcmp(args.spec_file, "spec.json") == 0);
    assert(args.screenshot_path == NULL);
    printf("PASS: spec file only\n");
}

static void test_screenshot_after_spec(void) {
    CliArgs args;
    char *argv[] = {"serviz", "spec.json", "--screenshot", "out.png"};
    int rc = cli_parse(4, argv, &args);
    assert(rc == 0);
    assert(strcmp(args.spec_file, "spec.json") == 0);
    assert(strcmp(args.screenshot_path, "out.png") == 0);
    printf("PASS: screenshot after spec\n");
}

static void test_screenshot_before_spec(void) {
    CliArgs args;
    char *argv[] = {"serviz", "--screenshot", "out.png", "spec.json"};
    int rc = cli_parse(4, argv, &args);
    assert(rc == 0);
    assert(strcmp(args.spec_file, "spec.json") == 0);
    assert(strcmp(args.screenshot_path, "out.png") == 0);
    printf("PASS: screenshot before spec\n");
}

static void test_screenshot_missing_value(void) {
    CliArgs args;
    char *argv[] = {"serviz", "spec.json", "--screenshot"};
    int rc = cli_parse(3, argv, &args);
    assert(rc != 0);
    printf("PASS: screenshot missing value\n");
}

/* BUG #5 regression: --screenshot as the only arg should be an error,
 * not treated as a spec filename. */
static void test_screenshot_alone_is_error(void) {
    CliArgs args;
    char *argv[] = {"serviz", "--screenshot"};
    int rc = cli_parse(2, argv, &args);
    assert(rc != 0);
    printf("PASS: --screenshot alone is error\n");
}

/* BUG #5 regression: --help should be recognised, not treated as a spec
 * filename. */
static void test_help_flag(void) {
    CliArgs args;
    char *argv[] = {"serviz", "--help"};
    int rc = cli_parse(2, argv, &args);
    assert(rc == 0);
    assert(args.help);
    printf("PASS: --help flag\n");
}

/* BUG #5 regression: --version should be recognised, not treated as a spec
 * filename. */
static void test_version_flag(void) {
    CliArgs args;
    char *argv[] = {"serviz", "--version"};
    int rc = cli_parse(2, argv, &args);
    assert(rc == 0);
    assert(args.version);
    printf("PASS: --version flag\n");
}

static void test_too_many_positional(void) {
    CliArgs args;
    char *argv[] = {"serviz", "a.json", "b.json"};
    int rc = cli_parse(3, argv, &args);
    assert(rc != 0);
    printf("PASS: too many positional\n");
}

int main(void) {
    test_no_args();
    test_spec_file_only();
    test_screenshot_after_spec();
    test_screenshot_before_spec();
    test_screenshot_missing_value();
    test_screenshot_alone_is_error();
    test_help_flag();
    test_version_flag();
    test_too_many_positional();
    printf("All cli tests passed.\n");
    return 0;
}
