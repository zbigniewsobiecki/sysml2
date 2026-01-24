/*
 * SysML v2 Parser - JSON Writer Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/json_writer.h"
#include "sysml2/query.h"
#include <stdlib.h>
#include <string.h>

/* JSON output version */
#define SYSML_JSON_VERSION "1.0"

/*
 * Internal writer state
 */
typedef struct {
    FILE *out;
    const Sysml2JsonOptions *options;
    int indent_level;
} JsonWriter;

/*
 * Write indentation
 */
static void write_indent(JsonWriter *w) {
    if (!w->options->pretty) return;
    for (int i = 0; i < w->indent_level * w->options->indent_size; i++) {
        fputc(' ', w->out);
    }
}

/*
 * Write a newline (if pretty printing)
 */
static void write_newline(JsonWriter *w) {
    if (w->options->pretty) {
        fputc('\n', w->out);
    }
}

/*
 * Escape a string for JSON output
 */
size_t sysml2_json_escape_string(const char *str, char *out, size_t out_size) {
    if (!str) return 0;

    size_t written = 0;
    for (const char *p = str; *p && written < out_size - 1; p++) {
        char c = *p;
        char escape = 0;

        switch (c) {
            case '"':  escape = '"'; break;
            case '\\': escape = '\\'; break;
            case '\n': escape = 'n'; break;
            case '\r': escape = 'r'; break;
            case '\t': escape = 't'; break;
            case '\f': escape = 'f'; break;
            case '\b': escape = 'b'; break;
            default: break;
        }

        if (escape) {
            if (written + 2 >= out_size) break;
            out[written++] = '\\';
            out[written++] = escape;
        } else if ((unsigned char)c < 0x20) {
            /* Control character - use \uXXXX format */
            if (written + 6 >= out_size) break;
            written += snprintf(out + written, out_size - written, "\\u%04x", (unsigned char)c);
        } else {
            out[written++] = c;
        }
    }

    if (written < out_size) {
        out[written] = '\0';
    }
    return written;
}

/*
 * Write an escaped string with quotes
 */
static void write_string(JsonWriter *w, const char *str) {
    fputc('"', w->out);
    if (str) {
        /* Write escaped content */
        for (const char *p = str; *p; p++) {
            char c = *p;
            switch (c) {
                case '"':  fputs("\\\"", w->out); break;
                case '\\': fputs("\\\\", w->out); break;
                case '\n': fputs("\\n", w->out); break;
                case '\r': fputs("\\r", w->out); break;
                case '\t': fputs("\\t", w->out); break;
                case '\f': fputs("\\f", w->out); break;
                case '\b': fputs("\\b", w->out); break;
                default:
                    if ((unsigned char)c < 0x20) {
                        fprintf(w->out, "\\u%04x", (unsigned char)c);
                    } else {
                        fputc(c, w->out);
                    }
                    break;
            }
        }
    }
    fputc('"', w->out);
}

/*
 * Write a key-value pair with string value
 */
static void write_string_field(JsonWriter *w, const char *key, const char *value, bool comma) {
    if (comma) {
        fputc(',', w->out);
        write_newline(w);
    }
    write_indent(w);
    write_string(w, key);
    fputs(": ", w->out);
    if (value) {
        write_string(w, value);
    } else {
        fputs("null", w->out);
    }
}

/*
 * Write a string array field
 */
static void write_string_array_field(JsonWriter *w, const char *key,
                                      const char **values, size_t count, bool comma) {
    if (comma) {
        fputc(',', w->out);
        write_newline(w);
    }
    write_indent(w);
    write_string(w, key);
    fputs(": [", w->out);

    for (size_t i = 0; i < count; i++) {
        if (i > 0) fputs(", ", w->out);
        write_string(w, values[i]);
    }

    fputc(']', w->out);
}

/*
 * Write the meta section
 */
static void write_meta(JsonWriter *w, const SysmlSemanticModel *model) {
    write_indent(w);
    fputs("\"meta\": {", w->out);
    write_newline(w);
    w->indent_level++;

    write_string_field(w, "version", SYSML_JSON_VERSION, false);

    if (w->options->include_source && model->source_name) {
        write_string_field(w, "source", model->source_name, true);
    }

    write_newline(w);
    w->indent_level--;
    write_indent(w);
    fputc('}', w->out);
}

/*
 * Write a single element
 */
static void write_element(JsonWriter *w, const SysmlNode *node) {
    write_indent(w);
    fputc('{', w->out);
    write_newline(w);
    w->indent_level++;

    /* id */
    write_string_field(w, "id", node->id, false);

    /* name */
    write_string_field(w, "name", node->name, true);

    /* type */
    write_string_field(w, "type", sysml2_kind_to_json_type(node->kind), true);

    /* parent */
    write_string_field(w, "parent", node->parent_id, true);

    /* specializes (if present) */
    if (node->specializes && node->specializes_count > 0) {
        write_string_array_field(w, "specializes", node->specializes, node->specializes_count, true);
    }

    /* redefines (if present) */
    if (node->redefines && node->redefines_count > 0) {
        write_string_array_field(w, "redefines", node->redefines, node->redefines_count, true);
    }

    /* references (if present) */
    if (node->references && node->references_count > 0) {
        write_string_array_field(w, "references", node->references, node->references_count, true);
    }

    /* typedBy (if present) */
    if (node->typed_by && node->typed_by_count > 0) {
        write_string_array_field(w, "typedBy", node->typed_by, node->typed_by_count, true);
    }

    /* prefixMetadata (if present) */
    if (node->prefix_metadata && node->prefix_metadata_count > 0) {
        write_string_array_field(w, "prefixMetadata", node->prefix_metadata, node->prefix_metadata_count, true);
    }

    /* metadata (if present) */
    if (node->metadata && node->metadata_count > 0) {
        fputc(',', w->out);
        write_newline(w);
        write_indent(w);
        fputs("\"metadata\": [", w->out);
        for (size_t i = 0; i < node->metadata_count; i++) {
            SysmlMetadataUsage *m = node->metadata[i];
            if (!m) continue;
            if (i > 0) fputc(',', w->out);
            write_newline(w);
            write_indent(w);
            fputs("  { \"type\": ", w->out);
            write_string(w, m->type_ref);
            if (m->feature_count > 0) {
                fputs(", \"features\": {", w->out);
                for (size_t j = 0; j < m->feature_count; j++) {
                    SysmlMetadataFeature *f = m->features[j];
                    if (!f) continue;
                    if (j > 0) fputc(',', w->out);
                    fputc(' ', w->out);
                    write_string(w, f->name);
                    fputs(": ", w->out);
                    if (f->value) {
                        write_string(w, f->value);
                    } else {
                        fputs("null", w->out);
                    }
                }
                fputs(" }", w->out);
            }
            fputs(" }", w->out);
        }
        write_newline(w);
        write_indent(w);
        fputc(']', w->out);
    }

    write_newline(w);
    w->indent_level--;
    write_indent(w);
    fputc('}', w->out);
}

/*
 * Write the elements array
 */
static void write_elements(JsonWriter *w, const SysmlSemanticModel *model) {
    write_indent(w);
    fputs("\"elements\": [", w->out);
    write_newline(w);
    w->indent_level++;

    for (size_t i = 0; i < model->element_count; i++) {
        if (i > 0) {
            fputc(',', w->out);
            write_newline(w);
        }
        write_element(w, model->elements[i]);
    }

    write_newline(w);
    w->indent_level--;
    write_indent(w);
    fputc(']', w->out);
}

/*
 * Write a single relationship
 */
static void write_relationship(JsonWriter *w, const SysmlRelationship *rel) {
    write_indent(w);
    fputc('{', w->out);
    write_newline(w);
    w->indent_level++;

    /* id */
    write_string_field(w, "id", rel->id, false);

    /* type */
    write_string_field(w, "type", sysml2_kind_to_json_type(rel->kind), true);

    /* source */
    write_string_field(w, "source", rel->source, true);

    /* target */
    write_string_field(w, "target", rel->target, true);

    write_newline(w);
    w->indent_level--;
    write_indent(w);
    fputc('}', w->out);
}

/*
 * Write the relationships array
 */
static void write_relationships(JsonWriter *w, const SysmlSemanticModel *model) {
    write_indent(w);
    fputs("\"relationships\": [", w->out);
    write_newline(w);
    w->indent_level++;

    for (size_t i = 0; i < model->relationship_count; i++) {
        if (i > 0) {
            fputc(',', w->out);
            write_newline(w);
        }
        write_relationship(w, model->relationships[i]);
    }

    write_newline(w);
    w->indent_level--;
    write_indent(w);
    fputc(']', w->out);
}

/*
 * Write the semantic model as JSON to a file
 */
Sysml2Result sysml2_json_write(
    const SysmlSemanticModel *model,
    FILE *out,
    const Sysml2JsonOptions *options
) {
    if (!model || !out) {
        return SYSML2_ERROR_SYNTAX;
    }

    /* Use default options if not provided */
    Sysml2JsonOptions default_opts = SYSML_JSON_OPTIONS_DEFAULT;
    if (!options) {
        options = &default_opts;
    }

    JsonWriter w = {
        .out = out,
        .options = options,
        .indent_level = 0
    };

    /* Write root object */
    fputc('{', w.out);
    write_newline(&w);
    w.indent_level++;

    /* Meta section */
    write_meta(&w, model);
    fputc(',', w.out);
    write_newline(&w);

    /* Elements array */
    write_elements(&w, model);
    fputc(',', w.out);
    write_newline(&w);

    /* Relationships array */
    write_relationships(&w, model);
    write_newline(&w);

    /* Close root object */
    w.indent_level--;
    write_indent(&w);
    fputc('}', w.out);
    write_newline(&w);

    return SYSML2_OK;
}

/*
 * Write the semantic model as JSON to a string
 */
Sysml2Result sysml2_json_write_string(
    const SysmlSemanticModel *model,
    const Sysml2JsonOptions *options,
    char **out_str
) {
    if (!model || !out_str) {
        return SYSML2_ERROR_SYNTAX;
    }

    /* Use memory stream */
    char *buffer = NULL;
    size_t size = 0;
    FILE *memstream = open_memstream(&buffer, &size);
    if (!memstream) {
        return SYSML2_ERROR_OUT_OF_MEMORY;
    }

    Sysml2Result result = sysml2_json_write(model, memstream, options);
    fclose(memstream);

    if (result == SYSML2_OK) {
        *out_str = buffer;
    } else {
        free(buffer);
        *out_str = NULL;
    }

    return result;
}

/*
 * Write query meta section
 */
static void write_query_meta(JsonWriter *w) {
    write_indent(w);
    fputs("\"meta\": {", w->out);
    write_newline(w);
    w->indent_level++;

    write_string_field(w, "version", SYSML_JSON_VERSION, false);
    write_string_field(w, "type", "query_result", true);

    write_newline(w);
    w->indent_level--;
    write_indent(w);
    fputc('}', w->out);
}

/*
 * Write query elements array
 */
static void write_query_elements(JsonWriter *w, const Sysml2QueryResult *result) {
    write_indent(w);
    fputs("\"elements\": [", w->out);
    write_newline(w);
    w->indent_level++;

    for (size_t i = 0; i < result->element_count; i++) {
        if (i > 0) {
            fputc(',', w->out);
            write_newline(w);
        }
        write_element(w, result->elements[i]);
    }

    write_newline(w);
    w->indent_level--;
    write_indent(w);
    fputc(']', w->out);
}

/*
 * Write query relationships array
 */
static void write_query_relationships(JsonWriter *w, const Sysml2QueryResult *result) {
    write_indent(w);
    fputs("\"relationships\": [", w->out);
    write_newline(w);
    w->indent_level++;

    for (size_t i = 0; i < result->relationship_count; i++) {
        if (i > 0) {
            fputc(',', w->out);
            write_newline(w);
        }
        write_relationship(w, result->relationships[i]);
    }

    write_newline(w);
    w->indent_level--;
    write_indent(w);
    fputc(']', w->out);
}

/*
 * Write a query result as JSON to a file
 */
Sysml2Result sysml2_json_write_query(
    const Sysml2QueryResult *result,
    FILE *out,
    const Sysml2JsonOptions *options
) {
    if (!result || !out) {
        return SYSML2_ERROR_SYNTAX;
    }

    /* Use default options if not provided */
    Sysml2JsonOptions default_opts = SYSML_JSON_OPTIONS_DEFAULT;
    if (!options) {
        options = &default_opts;
    }

    JsonWriter w = {
        .out = out,
        .options = options,
        .indent_level = 0
    };

    /* Write root object */
    fputc('{', w.out);
    write_newline(&w);
    w.indent_level++;

    /* Meta section */
    write_query_meta(&w);
    fputc(',', w.out);
    write_newline(&w);

    /* Elements array */
    write_query_elements(&w, result);
    fputc(',', w.out);
    write_newline(&w);

    /* Relationships array */
    write_query_relationships(&w, result);
    write_newline(&w);

    /* Close root object */
    w.indent_level--;
    write_indent(&w);
    fputc('}', w.out);
    write_newline(&w);

    return SYSML2_OK;
}
