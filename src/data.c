#include "data.h"
#include "../third_party/duckdb/duckdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

/* Column name priority lists for inference */
static const char *x_names[] = {"x", "time", "timestamp", "ts", "date", "datetime", "t", NULL};
static const char *y_names[] = {"y", "value", "val", "v", "output", "result", "price", NULL};
static const char *series_names[] = {"series", "group", "category", "name", "label", "id", NULL};
static const char *color_names[] = {"color", "colour", NULL};

/* Case-insensitive string compare */
static int strcasecmp_portable(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/* Find column matching any name in the list (case-insensitive). Returns col index or -1. */
static int find_column_from_list(duckdb_result *result, const char **names, idx_t col_count) {
    for (int n = 0; names[n]; n++) {
        for (idx_t i = 0; i < col_count; i++) {
            const char *col_name = duckdb_column_name(result, i);
            if (col_name && strcasecmp_portable(col_name, names[n]) == 0) {
                return (int)i;
            }
        }
    }
    return -1;
}

static int is_numeric_type(duckdb_type t) {
    switch (t) {
        case DUCKDB_TYPE_FLOAT:
        case DUCKDB_TYPE_DOUBLE:
        case DUCKDB_TYPE_TINYINT:
        case DUCKDB_TYPE_SMALLINT:
        case DUCKDB_TYPE_INTEGER:
        case DUCKDB_TYPE_BIGINT:
        case DUCKDB_TYPE_UTINYINT:
        case DUCKDB_TYPE_USMALLINT:
        case DUCKDB_TYPE_UINTEGER:
        case DUCKDB_TYPE_UBIGINT:
        case DUCKDB_TYPE_HUGEINT:
            return 1;
        default:
            return 0;
    }
}

static int is_time_type(duckdb_type t) {
    switch (t) {
        case DUCKDB_TYPE_TIMESTAMP:
        case DUCKDB_TYPE_TIMESTAMP_S:
        case DUCKDB_TYPE_TIMESTAMP_MS:
        case DUCKDB_TYPE_TIMESTAMP_NS:
        case DUCKDB_TYPE_TIMESTAMP_TZ:
        case DUCKDB_TYPE_DATE:
        case DUCKDB_TYPE_TIME:
            return 1;
        default:
            return 0;
    }
}

static double get_double_from_vector(duckdb_vector vec, duckdb_type col_type, uint64_t row) {
    switch (col_type) {
        case DUCKDB_TYPE_FLOAT: {
            float *data = (float *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_DOUBLE: {
            double *data = (double *)duckdb_vector_get_data(vec);
            return data[row];
        }
        case DUCKDB_TYPE_TINYINT: {
            int8_t *data = (int8_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_SMALLINT: {
            int16_t *data = (int16_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_INTEGER: {
            int32_t *data = (int32_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_BIGINT: {
            int64_t *data = (int64_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_UTINYINT: {
            uint8_t *data = (uint8_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_USMALLINT: {
            uint16_t *data = (uint16_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_UINTEGER: {
            uint32_t *data = (uint32_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_UBIGINT: {
            uint64_t *data = (uint64_t *)duckdb_vector_get_data(vec);
            return (double)data[row];
        }
        case DUCKDB_TYPE_HUGEINT: {
            duckdb_hugeint *data = (duckdb_hugeint *)duckdb_vector_get_data(vec);
            return (double)data[row].lower + (double)data[row].upper * 18446744073709551616.0;
        }
        default:
            return 0.0;
    }
}

/* Extract timestamp/date/time as microseconds since epoch */
static double get_time_as_micros(duckdb_vector vec, duckdb_type col_type, uint64_t row) {
    switch (col_type) {
        case DUCKDB_TYPE_TIMESTAMP:
        case DUCKDB_TYPE_TIMESTAMP_TZ: {
            duckdb_timestamp *data = (duckdb_timestamp *)duckdb_vector_get_data(vec);
            return (double)data[row].micros;
        }
        case DUCKDB_TYPE_TIMESTAMP_S: {
            int64_t *data = (int64_t *)duckdb_vector_get_data(vec);
            return (double)data[row] * 1e6;
        }
        case DUCKDB_TYPE_TIMESTAMP_MS: {
            int64_t *data = (int64_t *)duckdb_vector_get_data(vec);
            return (double)data[row] * 1e3;
        }
        case DUCKDB_TYPE_TIMESTAMP_NS: {
            int64_t *data = (int64_t *)duckdb_vector_get_data(vec);
            return (double)data[row] / 1e3;
        }
        case DUCKDB_TYPE_DATE: {
            duckdb_date *data = (duckdb_date *)duckdb_vector_get_data(vec);
            return (double)data[row].days * 86400.0 * 1e6;
        }
        case DUCKDB_TYPE_TIME: {
            duckdb_time *data = (duckdb_time *)duckdb_vector_get_data(vec);
            return (double)data[row].micros;
        }
        default:
            return 0.0;
    }
}

static char *str_dup(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

/* Get a string from a VARCHAR vector */
static char *get_string_from_vector(duckdb_vector vec, uint64_t row) {
    duckdb_string_t *data = (duckdb_string_t *)duckdb_vector_get_data(vec);
    duckdb_string_t val = data[row];
    const char *str = val.value.inlined.length <= 12
        ? val.value.inlined.inlined
        : val.value.pointer.ptr;
    uint32_t len = val.value.inlined.length;
    char *copy = (char *)malloc(len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

/* Format any column value as a string for tooltip display */
static char *format_value_as_string(duckdb_vector vec, duckdb_type col_type, uint64_t row) {
    char buf[128];
    if (col_type == DUCKDB_TYPE_VARCHAR) {
        return get_string_from_vector(vec, row);
    } else if (is_numeric_type(col_type)) {
        double v = get_double_from_vector(vec, col_type, row);
        snprintf(buf, sizeof(buf), "%g", v);
        return str_dup(buf);
    } else if (is_time_type(col_type)) {
        double us = get_time_as_micros(vec, col_type, row);
        int64_t micros = (int64_t)us;
        int64_t secs = micros / 1000000;
        int64_t rem = micros % 1000000;
        if (rem < 0) { secs--; rem += 1000000; }
        /* Simple ISO-ish format */
        snprintf(buf, sizeof(buf), "%lld.%06lld",
                 (long long)secs, (long long)rem);
        return str_dup(buf);
    } else {
        return str_dup("?");
    }
}

/* For DECIMAL columns, wrap the user query to cast to DOUBLE */
static char *wrap_decimal_casts(const char *sql, duckdb_result *result) {
    idx_t col_count = duckdb_column_count(result);
    int has_decimal = 0;
    for (idx_t i = 0; i < col_count; i++) {
        if (duckdb_column_type(result, i) == DUCKDB_TYPE_DECIMAL) {
            has_decimal = 1;
            break;
        }
    }
    if (!has_decimal) return NULL;

    size_t buf_size = strlen(sql) + col_count * 64 + 128;
    char *buf = (char *)malloc(buf_size);
    if (!buf) return NULL;

    size_t pos = 0;
    pos += snprintf(buf + pos, buf_size - pos, "SELECT ");
    for (idx_t i = 0; i < col_count; i++) {
        const char *name = duckdb_column_name(result, i);
        if (i > 0) pos += snprintf(buf + pos, buf_size - pos, ", ");
        if (duckdb_column_type(result, i) == DUCKDB_TYPE_DECIMAL) {
            pos += snprintf(buf + pos, buf_size - pos,
                            "CAST(\"%s\" AS DOUBLE) AS \"%s\"", name, name);
        } else {
            pos += snprintf(buf + pos, buf_size - pos, "\"%s\"", name);
        }
    }
    pos += snprintf(buf + pos, buf_size - pos, " FROM (%s) AS _t", sql);
    return buf;
}

/* Comparison for sorting by (series_index, x) */
typedef struct {
    uint32_t original_index;
    int series_index;
    double x;
} SortEntry;

static int sort_entry_cmp(const void *a, const void *b) {
    const SortEntry *sa = (const SortEntry *)a;
    const SortEntry *sb = (const SortEntry *)b;
    if (sa->series_index != sb->series_index)
        return sa->series_index - sb->series_index;
    if (sa->x < sb->x) return -1;
    if (sa->x > sb->x) return 1;
    return 0;
}

int data_load(const char *sql, DataSet *out) {
    memset(out, 0, sizeof(DataSet));

    duckdb_database db;
    duckdb_connection con;

    if (duckdb_open(NULL, &db) != DuckDBSuccess) {
        fprintf(stderr, "Error: failed to open DuckDB in-memory database\n");
        return -1;
    }
    if (duckdb_connect(db, &con) != DuckDBSuccess) {
        fprintf(stderr, "Error: failed to connect to DuckDB\n");
        duckdb_close(&db);
        return -1;
    }

    {
        duckdb_result res;
        duckdb_query(con, "SET autoinstall_known_extensions=1; SET autoload_known_extensions=1;", &res);
        duckdb_destroy_result(&res);
    }

    if (strstr(sql, "s3://") || strstr(sql, "S3://")) {
        duckdb_result res;
        if (duckdb_query(con, "INSTALL httpfs; LOAD httpfs;", &res) != DuckDBSuccess) {
            fprintf(stderr, "Warning: failed to load httpfs extension\n");
        }
        duckdb_destroy_result(&res);
    }

    fprintf(stderr, "Executing query: %s\n", sql);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    duckdb_result result;
    if (duckdb_query(con, sql, &result) != DuckDBSuccess) {
        const char *err = duckdb_result_error(&result);
        fprintf(stderr, "Error: DuckDB query failed: %s\n", err ? err : "unknown error");
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return -1;
    }

    /* Handle DECIMAL columns */
    char *wrapped = wrap_decimal_casts(sql, &result);
    if (wrapped) {
        duckdb_destroy_result(&result);
        fprintf(stderr, "Re-executing with DECIMAL casts: %s\n", wrapped);
        if (duckdb_query(con, wrapped, &result) != DuckDBSuccess) {
            const char *err = duckdb_result_error(&result);
            fprintf(stderr, "Error: DuckDB query failed: %s\n", err ? err : "unknown error");
            free(wrapped);
            duckdb_destroy_result(&result);
            duckdb_disconnect(&con);
            duckdb_close(&db);
            return -1;
        }
        free(wrapped);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double query_secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    fprintf(stderr, "Query executed in %.3f s\n", query_secs);

    idx_t col_count = duckdb_column_count(&result);

    /* Infer column roles */
    int x_col = find_column_from_list(&result, x_names, col_count);
    int y_col = find_column_from_list(&result, y_names, col_count);
    int series_col = find_column_from_list(&result, series_names, col_count);
    int color_col = find_column_from_list(&result, color_names, col_count);

    /* Positional fallback */
    if (x_col < 0 && col_count >= 1) x_col = 0;
    if (y_col < 0 && col_count >= 2) {
        y_col = (x_col == 0) ? 1 : 0;
    }

    if (x_col < 0 || y_col < 0) {
        fprintf(stderr, "Error: could not identify x and y columns\n");
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return -1;
    }

    /* Detect x type */
    duckdb_type x_type = duckdb_column_type(&result, x_col);
    duckdb_type y_type = duckdb_column_type(&result, y_col);
    out->x_is_time = is_time_type(x_type) ? true : false;

    /* Validate types */
    if (!is_numeric_type(x_type) && !is_time_type(x_type)) {
        fprintf(stderr, "Error: x column '%s' is not numeric or time\n",
                duckdb_column_name(&result, x_col));
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return -1;
    }
    if (!is_numeric_type(y_type)) {
        fprintf(stderr, "Error: y column '%s' is not numeric\n",
                duckdb_column_name(&result, y_col));
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return -1;
    }

    if (color_col >= 0 && !is_numeric_type(duckdb_column_type(&result, color_col))) {
        fprintf(stderr, "Warning: color column is not numeric, ignoring\n");
        color_col = -1;
    }

    out->has_color = (color_col >= 0);

    /* Identify extra columns */
    int extra_cols[MAX_EXTRA_COLS];
    int extra_count = 0;
    for (idx_t i = 0; i < col_count && extra_count < MAX_EXTRA_COLS; i++) {
        if ((int)i == x_col || (int)i == y_col ||
            (int)i == series_col || (int)i == color_col)
            continue;
        extra_cols[extra_count++] = (int)i;
    }

    /* Count total rows */
    uint64_t total_rows = 0;
    idx_t chunk_count = duckdb_result_chunk_count(result);
    for (idx_t ci = 0; ci < chunk_count; ci++) {
        duckdb_data_chunk chunk = duckdb_result_get_chunk(result, ci);
        total_rows += duckdb_data_chunk_get_size(chunk);
        duckdb_destroy_data_chunk(&chunk);
    }

    if (total_rows == 0) {
        fprintf(stderr, "Warning: query returned 0 rows\n");
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return 0;
    }

    fprintf(stderr, "Loading %llu rows...\n", (unsigned long long)total_rows);

    /* Allocate arrays */
    out->x = (double *)malloc(total_rows * sizeof(double));
    out->y = (double *)malloc(total_rows * sizeof(double));
    if (out->has_color)
        out->color_values = (double *)malloc(total_rows * sizeof(double));

    /* Temp arrays for series names and extras */
    char **series_strs = NULL;
    if (series_col >= 0)
        series_strs = (char **)calloc(total_rows, sizeof(char *));

    out->extra_count = extra_count;
    for (int e = 0; e < extra_count; e++) {
        out->extras[e].name = str_dup(duckdb_column_name(&result, extra_cols[e]));
        out->extras[e].values = (char **)calloc(total_rows, sizeof(char *));
    }

    if (!out->x || !out->y) {
        fprintf(stderr, "Error: failed to allocate memory\n");
        free(series_strs);
        data_free(out);
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return -1;
    }

    /* Extract data chunk by chunk */
    duckdb_type c_type = out->has_color ? duckdb_column_type(&result, color_col) : DUCKDB_TYPE_FLOAT;
    uint64_t row_offset = 0;

    for (idx_t ci = 0; ci < chunk_count; ci++) {
        duckdb_data_chunk chunk = duckdb_result_get_chunk(result, ci);
        idx_t chunk_size = duckdb_data_chunk_get_size(chunk);

        duckdb_vector x_vec = duckdb_data_chunk_get_vector(chunk, x_col);
        duckdb_vector y_vec = duckdb_data_chunk_get_vector(chunk, y_col);
        uint64_t *x_validity = duckdb_vector_get_validity(x_vec);
        uint64_t *y_validity = duckdb_vector_get_validity(y_vec);

        duckdb_vector c_vec = out->has_color ? duckdb_data_chunk_get_vector(chunk, color_col) : NULL;
        uint64_t *c_validity = c_vec ? duckdb_vector_get_validity(c_vec) : NULL;

        duckdb_vector s_vec = series_col >= 0 ? duckdb_data_chunk_get_vector(chunk, series_col) : NULL;
        uint64_t *s_validity = s_vec ? duckdb_vector_get_validity(s_vec) : NULL;

        /* Extra column vectors */
        duckdb_vector e_vecs[MAX_EXTRA_COLS];
        duckdb_type e_types[MAX_EXTRA_COLS];
        uint64_t *e_validity[MAX_EXTRA_COLS];
        for (int e = 0; e < extra_count; e++) {
            e_vecs[e] = duckdb_data_chunk_get_vector(chunk, extra_cols[e]);
            e_types[e] = duckdb_column_type(&result, extra_cols[e]);
            e_validity[e] = duckdb_vector_get_validity(e_vecs[e]);
        }

        for (idx_t r = 0; r < chunk_size; r++) {
            uint64_t idx = row_offset + r;

            int x_valid = !x_validity || duckdb_validity_row_is_valid(x_validity, r);
            int y_valid = !y_validity || duckdb_validity_row_is_valid(y_validity, r);

            if (out->x_is_time) {
                out->x[idx] = x_valid ? get_time_as_micros(x_vec, x_type, r) : 0.0;
            } else {
                out->x[idx] = x_valid ? get_double_from_vector(x_vec, x_type, r) : 0.0;
            }
            out->y[idx] = y_valid ? get_double_from_vector(y_vec, y_type, r) : 0.0;

            if (out->has_color && c_vec) {
                int c_valid = !c_validity || duckdb_validity_row_is_valid(c_validity, r);
                out->color_values[idx] = c_valid ? get_double_from_vector(c_vec, c_type, r) : 0.0;
            }

            if (s_vec) {
                int s_valid = !s_validity || duckdb_validity_row_is_valid(s_validity, r);
                series_strs[idx] = s_valid ? get_string_from_vector(s_vec, r) : str_dup("(null)");
            }

            for (int e = 0; e < extra_count; e++) {
                int e_valid = !e_validity[e] || duckdb_validity_row_is_valid(e_validity[e], r);
                out->extras[e].values[idx] = e_valid
                    ? format_value_as_string(e_vecs[e], e_types[e], r)
                    : str_dup("NULL");
            }
        }

        row_offset += chunk_size;
        duckdb_destroy_data_chunk(&chunk);
    }

    out->count = (uint32_t)total_rows;

    duckdb_destroy_result(&result);
    duckdb_disconnect(&con);
    duckdb_close(&db);

    /* Build series index and sort */
    if (series_strs) {
        /* Count distinct series */
        char **unique_names = NULL;
        int unique_count = 0;
        int *point_series_idx = (int *)malloc(total_rows * sizeof(int));

        for (uint64_t i = 0; i < total_rows; i++) {
            int found = -1;
            for (int s = 0; s < unique_count; s++) {
                if (strcmp(series_strs[i], unique_names[s]) == 0) {
                    found = s;
                    break;
                }
            }
            if (found < 0) {
                unique_names = (char **)realloc(unique_names, (unique_count + 1) * sizeof(char *));
                unique_names[unique_count] = str_dup(series_strs[i]);
                found = unique_count;
                unique_count++;
            }
            point_series_idx[i] = found;
        }

        /* Sort by (series_index, x) */
        SortEntry *entries = (SortEntry *)malloc(total_rows * sizeof(SortEntry));
        for (uint64_t i = 0; i < total_rows; i++) {
            entries[i].original_index = (uint32_t)i;
            entries[i].series_index = point_series_idx[i];
            entries[i].x = out->x[i];
        }
        qsort(entries, total_rows, sizeof(SortEntry), sort_entry_cmp);

        /* Reorder all arrays */
        double *new_x = (double *)malloc(total_rows * sizeof(double));
        double *new_y = (double *)malloc(total_rows * sizeof(double));
        double *new_c = out->has_color ? (double *)malloc(total_rows * sizeof(double)) : NULL;
        char ***new_extras = (char ***)malloc(extra_count * sizeof(char **));
        for (int e = 0; e < extra_count; e++)
            new_extras[e] = (char **)malloc(total_rows * sizeof(char *));

        for (uint64_t i = 0; i < total_rows; i++) {
            uint32_t oi = entries[i].original_index;
            new_x[i] = out->x[oi];
            new_y[i] = out->y[oi];
            if (new_c) new_c[i] = out->color_values[oi];
            for (int e = 0; e < extra_count; e++)
                new_extras[e][i] = out->extras[e].values[oi];
        }

        free(out->x); out->x = new_x;
        free(out->y); out->y = new_y;
        if (out->has_color) { free(out->color_values); out->color_values = new_c; }
        for (int e = 0; e < extra_count; e++) {
            free(out->extras[e].values);
            out->extras[e].values = new_extras[e];
        }
        free(new_extras);

        /* Build Series array from sorted entries */
        out->series = (Series *)malloc(unique_count * sizeof(Series));
        out->series_count = unique_count;

        int cur_series = entries[0].series_index;
        uint32_t cur_start = 0;
        for (uint64_t i = 1; i <= total_rows; i++) {
            int si = (i < total_rows) ? entries[i].series_index : -1;
            if (si != cur_series) {
                int s = cur_series;
                out->series[s].name = unique_names[s];
                out->series[s].start = cur_start;
                out->series[s].count = (uint32_t)(i - cur_start);
                cur_start = (uint32_t)i;
                cur_series = si;
            }
        }

        free(entries);
        free(point_series_idx);
        free(unique_names); /* names are now owned by Series */

        for (uint64_t i = 0; i < total_rows; i++)
            free(series_strs[i]);
        free(series_strs);
    } else {
        /* Single series */
        out->series = (Series *)malloc(sizeof(Series));
        out->series_count = 1;
        out->series[0].name = str_dup("data");
        out->series[0].start = 0;
        out->series[0].count = out->count;

        /* Sort by x within the single series */
        SortEntry *entries = (SortEntry *)malloc(total_rows * sizeof(SortEntry));
        for (uint64_t i = 0; i < total_rows; i++) {
            entries[i].original_index = (uint32_t)i;
            entries[i].series_index = 0;
            entries[i].x = out->x[i];
        }
        qsort(entries, total_rows, sizeof(SortEntry), sort_entry_cmp);

        double *new_x = (double *)malloc(total_rows * sizeof(double));
        double *new_y = (double *)malloc(total_rows * sizeof(double));
        double *new_c = out->has_color ? (double *)malloc(total_rows * sizeof(double)) : NULL;
        char ***new_extras = NULL;
        if (extra_count > 0) {
            new_extras = (char ***)malloc(extra_count * sizeof(char **));
            for (int e = 0; e < extra_count; e++)
                new_extras[e] = (char **)malloc(total_rows * sizeof(char *));
        }

        for (uint64_t i = 0; i < total_rows; i++) {
            uint32_t oi = entries[i].original_index;
            new_x[i] = out->x[oi];
            new_y[i] = out->y[oi];
            if (new_c) new_c[i] = out->color_values[oi];
            for (int e = 0; e < extra_count; e++)
                new_extras[e][i] = out->extras[e].values[oi];
        }

        free(out->x); out->x = new_x;
        free(out->y); out->y = new_y;
        if (out->has_color) { free(out->color_values); out->color_values = new_c; }
        for (int e = 0; e < extra_count; e++) {
            free(out->extras[e].values);
            out->extras[e].values = new_extras[e];
        }
        free(new_extras);
        free(entries);
    }

    /* Compute bounds */
    double xmin = DBL_MAX, xmax = -DBL_MAX;
    double ymin = DBL_MAX, ymax = -DBL_MAX;
    double cmin = DBL_MAX, cmax = -DBL_MAX;

    for (uint32_t i = 0; i < out->count; i++) {
        if (out->x[i] < xmin) xmin = out->x[i];
        if (out->x[i] > xmax) xmax = out->x[i];
        if (out->y[i] < ymin) ymin = out->y[i];
        if (out->y[i] > ymax) ymax = out->y[i];
        if (out->has_color) {
            if (out->color_values[i] < cmin) cmin = out->color_values[i];
            if (out->color_values[i] > cmax) cmax = out->color_values[i];
        }
    }

    out->x_min = xmin; out->x_max = xmax;
    out->y_min = ymin; out->y_max = ymax;
    out->color_min = cmin; out->color_max = cmax;

    fprintf(stderr, "Loaded %u rows, %d series. X: [%.4f, %.4f] Y: [%.4f, %.4f]\n",
            out->count, out->series_count,
            out->x_min, out->x_max, out->y_min, out->y_max);

    return 0;
}

/* Probe a SQL query to discover column names and types without loading data.
 * Returns 0 on success, fills x_col_name, y_col_name, and x_is_time. */
static int probe_columns(const char *user_sql,
                          char *x_col_name, int x_col_name_size,
                          char *y_col_name, int y_col_name_size,
                          bool *x_is_time) {
    duckdb_database db;
    duckdb_connection con;

    if (duckdb_open(NULL, &db) != DuckDBSuccess) return -1;
    if (duckdb_connect(db, &con) != DuckDBSuccess) {
        duckdb_close(&db);
        return -1;
    }

    {
        duckdb_result res;
        duckdb_query(con, "SET autoinstall_known_extensions=1; SET autoload_known_extensions=1;", &res);
        duckdb_destroy_result(&res);
    }

    if (strstr(user_sql, "s3://") || strstr(user_sql, "S3://")) {
        duckdb_result res;
        if (duckdb_query(con, "INSTALL httpfs; LOAD httpfs;", &res) != DuckDBSuccess) {
            fprintf(stderr, "Warning: failed to load httpfs extension\n");
        }
        duckdb_destroy_result(&res);
    }

    char *probe_sql = (char *)malloc(strlen(user_sql) + 64);
    sprintf(probe_sql, "SELECT * FROM (%s) AS _t LIMIT 0", user_sql);

    duckdb_result result;
    if (duckdb_query(con, probe_sql, &result) != DuckDBSuccess) {
        free(probe_sql);
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return -1;
    }
    free(probe_sql);

    idx_t col_count = duckdb_column_count(&result);
    int x_col = find_column_from_list(&result, x_names, col_count);
    int y_col = find_column_from_list(&result, y_names, col_count);

    if (x_col < 0 && col_count >= 1) x_col = 0;
    if (y_col < 0 && col_count >= 2) y_col = (x_col == 0) ? 1 : 0;

    if (x_col < 0 || y_col < 0) {
        duckdb_destroy_result(&result);
        duckdb_disconnect(&con);
        duckdb_close(&db);
        return -1;
    }

    snprintf(x_col_name, x_col_name_size, "%s", duckdb_column_name(&result, x_col));
    snprintf(y_col_name, y_col_name_size, "%s", duckdb_column_name(&result, y_col));
    *x_is_time = is_time_type(duckdb_column_type(&result, x_col)) ? true : false;

    duckdb_destroy_result(&result);
    duckdb_disconnect(&con);
    duckdb_close(&db);
    return 0;
}

int data_load_ohlc(const char *user_sql, const char *bucket, DataSet *out) {
    char x_col[128], y_col[128];
    bool x_is_time;

    if (probe_columns(user_sql, x_col, sizeof(x_col), y_col, sizeof(y_col), &x_is_time) != 0) {
        fprintf(stderr, "Error: failed to probe columns for OHLC aggregation\n");
        return -1;
    }

    /* Build aggregation SQL */
    size_t sql_size = strlen(user_sql) + 512;
    char *agg_sql = (char *)malloc(sql_size);

    if (x_is_time) {
        snprintf(agg_sql, sql_size,
            "SELECT date_trunc('%s', \"%s\") AS x, "
            "first(\"%s\" ORDER BY \"%s\") AS y, "
            "first(\"%s\" ORDER BY \"%s\") AS open, "
            "max(\"%s\") AS high, "
            "min(\"%s\") AS low, "
            "last(\"%s\" ORDER BY \"%s\") AS close "
            "FROM (%s) AS _raw GROUP BY 1 ORDER BY 1",
            bucket, x_col,
            y_col, x_col,
            y_col, x_col,
            y_col,
            y_col,
            y_col, x_col,
            user_sql);
    } else {
        snprintf(agg_sql, sql_size,
            "SELECT floor(\"%s\" / %s) * %s AS x, "
            "first(\"%s\" ORDER BY \"%s\") AS y, "
            "first(\"%s\" ORDER BY \"%s\") AS open, "
            "max(\"%s\") AS high, "
            "min(\"%s\") AS low, "
            "last(\"%s\" ORDER BY \"%s\") AS close "
            "FROM (%s) AS _raw GROUP BY 1 ORDER BY 1",
            x_col, bucket, bucket,
            y_col, x_col,
            y_col, x_col,
            y_col,
            y_col,
            y_col, x_col,
            user_sql);
    }

    fprintf(stderr, "OHLC aggregation SQL: %s\n", agg_sql);

    int rc = data_load(agg_sql, out);
    free(agg_sql);

    if (rc != 0) return rc;

    /* The open/high/low/close columns are in extras. Move them to dedicated arrays. */
    out->is_ohlc = true;
    out->open = (double *)malloc(out->count * sizeof(double));
    out->high = (double *)malloc(out->count * sizeof(double));
    out->low = (double *)malloc(out->count * sizeof(double));
    out->close = (double *)malloc(out->count * sizeof(double));

    if (!out->open || !out->high || !out->low || !out->close) {
        fprintf(stderr, "Error: failed to allocate OHLC arrays\n");
        data_free(out);
        return -1;
    }

    /* Find the OHLC extra columns and parse their string values */
    int open_idx = -1, high_idx = -1, low_idx = -1, close_idx = -1;
    for (int e = 0; e < out->extra_count; e++) {
        if (strcmp(out->extras[e].name, "open") == 0) open_idx = e;
        else if (strcmp(out->extras[e].name, "high") == 0) high_idx = e;
        else if (strcmp(out->extras[e].name, "low") == 0) low_idx = e;
        else if (strcmp(out->extras[e].name, "close") == 0) close_idx = e;
    }

    for (uint32_t i = 0; i < out->count; i++) {
        out->open[i] = (open_idx >= 0) ? atof(out->extras[open_idx].values[i]) : out->y[i];
        out->high[i] = (high_idx >= 0) ? atof(out->extras[high_idx].values[i]) : out->y[i];
        out->low[i] = (low_idx >= 0) ? atof(out->extras[low_idx].values[i]) : out->y[i];
        out->close[i] = (close_idx >= 0) ? atof(out->extras[close_idx].values[i]) : out->y[i];
    }

    /* Update y bounds to cover full OHLC range */
    out->y_min = out->low[0];
    out->y_max = out->high[0];
    for (uint32_t i = 1; i < out->count; i++) {
        if (out->low[i] < out->y_min) out->y_min = out->low[i];
        if (out->high[i] > out->y_max) out->y_max = out->high[i];
    }

    return 0;
}

int data_load_histogram(const char *user_sql, double bucket_width, DataSet *out) {
    if (bucket_width <= 0.0) {
        fprintf(stderr, "Error: histogram bucket width must be positive (got %g)\n",
                bucket_width);
        return -1;
    }

    char x_col[128], y_col[128];
    bool x_is_time;

    if (probe_columns(user_sql, x_col, sizeof(x_col), y_col, sizeof(y_col), &x_is_time) != 0) {
        fprintf(stderr, "Error: failed to probe columns for histogram\n");
        return -1;
    }

    size_t sql_size = strlen(user_sql) + 256;
    char *hist_sql = (char *)malloc(sql_size);
    snprintf(hist_sql, sql_size,
        "SELECT floor(\"%s\" / %g) * %g AS x, "
        "count(*) AS y "
        "FROM (%s) AS _raw GROUP BY 1 ORDER BY 1",
        y_col, bucket_width, bucket_width, user_sql);

    fprintf(stderr, "Histogram SQL: %s\n", hist_sql);

    int rc = data_load(hist_sql, out);
    free(hist_sql);

    /* Histogram x is never time */
    if (rc == 0) out->x_is_time = false;

    return rc;
}

void data_free(DataSet *ds) {
    free(ds->x);
    free(ds->y);
    free(ds->color_values);
    free(ds->open);
    free(ds->high);
    free(ds->low);
    free(ds->close);
    if (ds->series) {
        for (int i = 0; i < ds->series_count; i++)
            free(ds->series[i].name);
        free(ds->series);
    }
    for (int e = 0; e < ds->extra_count; e++) {
        free(ds->extras[e].name);
        if (ds->extras[e].values) {
            for (uint32_t i = 0; i < ds->count; i++)
                free(ds->extras[e].values[i]);
            free(ds->extras[e].values);
        }
    }
    memset(ds, 0, sizeof(DataSet));
}
