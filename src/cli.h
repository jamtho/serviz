#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stdio.h>

typedef struct {
    const char *spec_file;       /* positional arg, may be NULL (stdin) */
    const char *screenshot_path; /* --screenshot path, may be NULL */
    bool help;                   /* --help requested */
    bool version;                /* --version requested */
} CliArgs;

/* Parse command-line arguments. Returns 0 on success, non-zero on error
 * (with a message printed to stderr). On success, fields are populated. */
int cli_parse(int argc, char *argv[], CliArgs *out);

/* Print usage to stream. */
void cli_print_usage(FILE *stream);

#endif /* CLI_H */
