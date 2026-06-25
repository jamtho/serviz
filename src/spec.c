#include "spec.h"
#include "../third_party/cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *str_dup(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

static int read_file(const char *path, char **out_buf) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open spec file '%s'\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) {
        fprintf(stderr, "Error: cannot determine size of '%s'\n", path);
        fclose(f);
        return -1;
    }
    *out_buf = (char *)malloc(len + 1);
    if (!*out_buf) {
        fclose(f);
        return -1;
    }
    size_t nread = fread(*out_buf, 1, (size_t)len, f);
    (*out_buf)[nread] = '\0';
    fclose(f);
    return 0;
}

static int parse_mark(const char *s) {
    if (strcmp(s, "line") == 0) return MARK_LINE;
    if (strcmp(s, "point") == 0) return MARK_POINT;
    if (strcmp(s, "bar") == 0) return MARK_BAR;
    if (strcmp(s, "histogram") == 0) return MARK_HISTOGRAM;
    if (strcmp(s, "candle") == 0) return MARK_CANDLE;
    return -1;
}

int spec_parse_string(const char *json, Spec *out) {
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        fprintf(stderr, "Error: invalid JSON in spec\n");
        return -1;
    }

    memset(out, 0, sizeof(Spec));

    cJSON *sql = cJSON_GetObjectItemCaseSensitive(root, "sql");
    if (!sql || !cJSON_IsString(sql)) {
        fprintf(stderr, "Error: missing 'sql' string in spec\n");
        cJSON_Delete(root);
        return -1;
    }
    out->sql = str_dup(sql->valuestring);

    cJSON *title = cJSON_GetObjectItemCaseSensitive(root, "title");
    if (title && cJSON_IsString(title)) {
        out->title = str_dup(title->valuestring);
    }

    cJSON *layers = cJSON_GetObjectItemCaseSensitive(root, "layers");
    if (!layers || !cJSON_IsArray(layers)) {
        fprintf(stderr, "Error: missing 'layers' array in spec\n");
        spec_free(out);
        cJSON_Delete(root);
        return -1;
    }

    int count = cJSON_GetArraySize(layers);
    if (count > MAX_LAYERS) {
        fprintf(stderr, "Warning: too many layers (%d), clamping to %d\n", count, MAX_LAYERS);
        count = MAX_LAYERS;
    }
    out->layer_count = count;

    for (int i = 0; i < count; i++) {
        cJSON *layer = cJSON_GetArrayItem(layers, i);
        Layer *l = &out->layers[i];

        cJSON *mark = cJSON_GetObjectItemCaseSensitive(layer, "mark");
        if (!mark || !cJSON_IsString(mark)) {
            fprintf(stderr, "Error: layer %d missing 'mark'\n", i);
            spec_free(out);
            cJSON_Delete(root);
            return -1;
        }
        int mark_val = parse_mark(mark->valuestring);
        if (mark_val < 0) {
            fprintf(stderr, "Error: layer %d has unknown mark '%s' "
                    "(expected: line, point, bar, histogram, or candle)\n",
                    i, mark->valuestring);
            spec_free(out);
            cJSON_Delete(root);
            return -1;
        }
        l->mark = (MarkType)mark_val;

        cJSON *scheme = cJSON_GetObjectItemCaseSensitive(layer, "scheme");
        l->scheme = colormap_from_name(
            scheme && cJSON_IsString(scheme) ? scheme->valuestring : NULL);

        cJSON *ps = cJSON_GetObjectItemCaseSensitive(layer, "point_size");
        l->point_size = (ps && cJSON_IsNumber(ps) && ps->valueint > 0) ? ps->valueint : 6;

        cJSON *name = cJSON_GetObjectItemCaseSensitive(layer, "name");
        if (name && cJSON_IsString(name)) {
            l->name = str_dup(name->valuestring);
        }

        cJSON *layer_sql = cJSON_GetObjectItemCaseSensitive(layer, "sql");
        if (layer_sql && cJSON_IsString(layer_sql)) {
            l->sql = str_dup(layer_sql->valuestring);
        }

        /* bucket (optional for bar, required for histogram/candle) */
        cJSON *bucket = cJSON_GetObjectItemCaseSensitive(layer, "bucket");
        if (bucket) {
            if (cJSON_IsString(bucket)) {
                l->bucket = str_dup(bucket->valuestring);
            } else if (cJSON_IsNumber(bucket)) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%g", bucket->valuedouble);
                l->bucket = str_dup(buf);
            }
        }

        if ((l->mark == MARK_HISTOGRAM || l->mark == MARK_CANDLE) && !l->bucket) {
            fprintf(stderr, "Error: layer %d '%s' mark requires 'bucket'\n",
                    i, mark->valuestring);
            spec_free(out);
            cJSON_Delete(root);
            return -1;
        }

        /* Reject non-positive numeric buckets: they produce inf/garbage. */
        if (l->bucket && (l->mark == MARK_HISTOGRAM || l->mark == MARK_CANDLE)) {
            char *end;
            double bv = strtod(l->bucket, &end);
            if (end != l->bucket && *end == '\0' && bv <= 0.0) {
                fprintf(stderr, "Error: layer %d bucket must be positive "
                        "(got '%s')\n", i, l->bucket);
                spec_free(out);
                cJSON_Delete(root);
                return -1;
            }
        }
    }

    cJSON_Delete(root);
    return 0;
}

int spec_parse(const char *filepath, Spec *out) {
    char *buf = NULL;
    if (read_file(filepath, &buf) != 0) return -1;
    int result = spec_parse_string(buf, out);
    free(buf);
    return result;
}

void spec_free(Spec *spec) {
    free(spec->sql);
    free(spec->title);
    for (int i = 0; i < spec->layer_count; i++) {
        free(spec->layers[i].name);
        free(spec->layers[i].sql);
        free(spec->layers[i].bucket);
    }
    memset(spec, 0, sizeof(Spec));
}
