#include "cli.h"
#include <stdio.h>
#include <string.h>

void cli_print_usage(FILE *stream) {
    fprintf(stream,
        "Usage: serviz [spec.json] [--screenshot path]\n"
        "       Or pipe spec JSON to stdin\n"
        "       serviz --help    Show this message\n"
        "       serviz --version Show version\n");
}

int cli_parse(int argc, char *argv[], CliArgs *out) {
    memset(out, 0, sizeof(*out));

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            out->help = true;
        } else if (strcmp(argv[i], "--version") == 0) {
            out->version = true;
        } else if (strcmp(argv[i], "--screenshot") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --screenshot requires a path argument\n");
                cli_print_usage(stderr);
                return 1;
            }
            out->screenshot_path = argv[++i];
        } else if (!out->spec_file) {
            out->spec_file = argv[i];
        } else {
            cli_print_usage(stderr);
            return 1;
        }
    }

    return 0;
}
