/*
 * SysML v2 Parser - Diagnostic System Implementation
 *
 * Clang-style error reporting with source context and color support.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/diagnostic.h"
#include <string.h>
#include <unistd.h>

/* ANSI color codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

void sysml2_diag_context_init(Sysml2DiagContext *ctx, Sysml2Arena *arena) {
    ctx->arena = arena;
    ctx->first = NULL;
    ctx->last = NULL;
    ctx->error_count = 0;
    ctx->warning_count = 0;
    ctx->parse_error_count = 0;
    ctx->semantic_error_count = 0;
    ctx->max_errors = 20;
    ctx->treat_warnings_as_errors = false;
    ctx->has_fatal = false;
}

void sysml2_diag_clear(Sysml2DiagContext *ctx) {
    /* Note: Diagnostics are arena-allocated, so we just reset pointers and counts.
     * The actual memory will be freed when the arena is destroyed. */
    ctx->first = NULL;
    ctx->last = NULL;
    ctx->error_count = 0;
    ctx->warning_count = 0;
    ctx->parse_error_count = 0;
    ctx->semantic_error_count = 0;
    ctx->has_fatal = false;
}

void sysml2_diag_set_max_errors(Sysml2DiagContext *ctx, size_t max) {
    ctx->max_errors = max;
}

bool sysml2_diag_should_stop(const Sysml2DiagContext *ctx) {
    if (ctx->has_fatal) return true;
    if (ctx->max_errors > 0 && ctx->error_count >= ctx->max_errors) return true;
    return false;
}

bool sysml2_diag_has_parse_errors(const Sysml2DiagContext *ctx) {
    return ctx->parse_error_count > 0;
}

bool sysml2_diag_has_semantic_errors(const Sysml2DiagContext *ctx) {
    return ctx->semantic_error_count > 0;
}

Sysml2Diagnostic *sysml2_diag_create(
    Sysml2DiagContext *ctx,
    Sysml2DiagCode code,
    Sysml2Severity severity,
    const Sysml2SourceFile *file,
    Sysml2SourceRange range,
    const char *message
) {
    Sysml2Diagnostic *diag = SYSML2_ARENA_NEW(ctx->arena, Sysml2Diagnostic);
    diag->next = NULL;
    diag->code = code;
    diag->severity = severity;
    diag->range = range;
    diag->file = file;
    diag->message = sysml2_arena_strdup(ctx->arena, message);
    diag->help = NULL;
    diag->fixits = NULL;
    diag->fixit_count = 0;
    diag->notes = NULL;
    return diag;
}

void sysml2_diag_add_help(Sysml2Diagnostic *diag, Sysml2DiagContext *ctx, const char *help) {
    diag->help = sysml2_arena_strdup(ctx->arena, help);
}

void sysml2_diag_add_fixit(
    Sysml2Diagnostic *diag,
    Sysml2DiagContext *ctx,
    Sysml2SourceRange range,
    const char *replacement
) {
    /* Grow fixit array */
    size_t new_count = diag->fixit_count + 1;
    Sysml2FixIt *new_fixits = SYSML2_ARENA_NEW_ARRAY(ctx->arena, Sysml2FixIt, new_count);

    /* Copy existing */
    if (diag->fixits && diag->fixit_count > 0) {
        memcpy(new_fixits, diag->fixits, diag->fixit_count * sizeof(Sysml2FixIt));
    }

    /* Add new */
    new_fixits[diag->fixit_count].range = range;
    new_fixits[diag->fixit_count].replacement = sysml2_arena_strdup(ctx->arena, replacement);

    diag->fixits = new_fixits;
    diag->fixit_count = new_count;
}

Sysml2Diagnostic *sysml2_diag_add_note(
    Sysml2Diagnostic *parent,
    Sysml2DiagContext *ctx,
    const Sysml2SourceFile *file,
    Sysml2SourceRange range,
    const char *message
) {
    Sysml2Diagnostic *note = sysml2_diag_create(
        ctx,
        parent->code,
        SYSML2_SEVERITY_NOTE,
        file,
        range,
        message
    );

    /* Append to notes list */
    if (!parent->notes) {
        parent->notes = note;
    } else {
        Sysml2Diagnostic *last = parent->notes;
        while (last->next) last = last->next;
        last->next = note;
    }

    return note;
}

void sysml2_diag_emit(Sysml2DiagContext *ctx, Sysml2Diagnostic *diag) {
    /* Update counts */
    switch (diag->severity) {
        case SYSML2_SEVERITY_ERROR:
            ctx->error_count++;
            /* Track error type based on diagnostic code */
            if (diag->code >= 1000 && diag->code < 3000) {
                ctx->parse_error_count++;
            } else if (diag->code >= 3000 && diag->code < 10000) {
                ctx->semantic_error_count++;
            }
            break;
        case SYSML2_SEVERITY_WARNING:
            if (ctx->treat_warnings_as_errors) {
                diag->severity = SYSML2_SEVERITY_ERROR;
                ctx->error_count++;
                /* Track error type based on diagnostic code */
                if (diag->code >= 1000 && diag->code < 3000) {
                    ctx->parse_error_count++;
                } else if (diag->code >= 3000 && diag->code < 10000) {
                    ctx->semantic_error_count++;
                }
            } else {
                ctx->warning_count++;
            }
            break;
        case SYSML2_SEVERITY_FATAL:
            ctx->error_count++;
            /* Track error type based on diagnostic code */
            if (diag->code >= 1000 && diag->code < 3000) {
                ctx->parse_error_count++;
            } else if (diag->code >= 3000 && diag->code < 10000) {
                ctx->semantic_error_count++;
            }
            ctx->has_fatal = true;
            break;
        default:
            break;
    }

    /* Append to list */
    if (!ctx->first) {
        ctx->first = diag;
        ctx->last = diag;
    } else {
        ctx->last->next = diag;
        ctx->last = diag;
    }
}

bool sysml2_should_use_color(Sysml2ColorMode mode, FILE *output) {
    switch (mode) {
        case SYSML2_COLOR_ALWAYS:
            return true;
        case SYSML2_COLOR_NEVER:
            return false;
        case SYSML2_COLOR_AUTO:
        default:
            return isatty(fileno(output));
    }
}

const char *sysml2_severity_to_string(Sysml2Severity severity) {
    switch (severity) {
        case SYSML2_SEVERITY_NOTE: return "note";
        case SYSML2_SEVERITY_WARNING: return "warning";
        case SYSML2_SEVERITY_ERROR: return "error";
        case SYSML2_SEVERITY_FATAL: return "fatal error";
        default: return "unknown";
    }
}

const char *sysml2_diag_code_to_string(Sysml2DiagCode code) {
    switch (code) {
        case SYSML2_DIAG_E1001_INVALID_CHAR: return "E1001";
        case SYSML2_DIAG_E1002_UNTERMINATED_STRING: return "E1002";
        case SYSML2_DIAG_E1003_UNTERMINATED_COMMENT: return "E1003";
        case SYSML2_DIAG_E1004_UNTERMINATED_NAME: return "E1004";
        case SYSML2_DIAG_E1005_INVALID_NUMBER: return "E1005";
        case SYSML2_DIAG_E1006_INVALID_ESCAPE: return "E1006";
        case SYSML2_DIAG_E2001_EXPECTED_SEMICOLON: return "E2001";
        case SYSML2_DIAG_E2002_EXPECTED_IDENTIFIER: return "E2002";
        case SYSML2_DIAG_E2003_EXPECTED_LBRACE: return "E2003";
        case SYSML2_DIAG_E2004_EXPECTED_RBRACE: return "E2004";
        case SYSML2_DIAG_E2005_EXPECTED_COLON: return "E2005";
        case SYSML2_DIAG_E2006_UNEXPECTED_TOKEN: return "E2006";
        case SYSML2_DIAG_E2007_EXPECTED_EXPRESSION: return "E2007";
        case SYSML2_DIAG_E2008_EXPECTED_TYPE: return "E2008";
        case SYSML2_DIAG_E2009_EXPECTED_LPAREN: return "E2009";
        case SYSML2_DIAG_E2010_EXPECTED_RPAREN: return "E2010";
        case SYSML2_DIAG_E2011_EXPECTED_LBRACKET: return "E2011";
        case SYSML2_DIAG_E2012_EXPECTED_RBRACKET: return "E2012";
        case SYSML2_DIAG_E3001_UNDEFINED_TYPE: return "E3001";
        case SYSML2_DIAG_E3002_UNDEFINED_FEATURE: return "E3002";
        case SYSML2_DIAG_E3003_UNDEFINED_NAMESPACE: return "E3003";
        case SYSML2_DIAG_E3004_DUPLICATE_NAME: return "E3004";
        case SYSML2_DIAG_E3005_CIRCULAR_SPECIALIZATION: return "E3005";
        case SYSML2_DIAG_E3006_TYPE_MISMATCH: return "E3006";
        case SYSML2_DIAG_E3007_MULTIPLICITY_ERROR: return "E3007";
        case SYSML2_DIAG_E3008_REDEFINITION_ERROR: return "E3008";
        case SYSML2_DIAG_E3009_CIRCULAR_IMPORT: return "E3009";
        case SYSML2_DIAG_E3010_IMPORT_NOT_FOUND: return "E3010";
        case SYSML2_DIAG_W1001_UNUSED_IMPORT: return "W1001";
        case SYSML2_DIAG_W1002_SHADOWED_NAME: return "W1002";
        case SYSML2_DIAG_W1003_DEPRECATED: return "W1003";
        default: return "E0000";
    }
}

/* Get source line at a given line number */
static const char *get_source_line(const Sysml2SourceFile *file, uint32_t line, size_t *out_length) {
    if (!file || !file->content || line == 0 || line > file->line_count) {
        *out_length = 0;
        return NULL;
    }

    uint32_t start = file->line_offsets[line - 1];
    uint32_t end;

    if (line < file->line_count) {
        end = file->line_offsets[line];
        /* Exclude newline */
        if (end > start && file->content[end - 1] == '\n') {
            end--;
        }
        if (end > start && file->content[end - 1] == '\r') {
            end--;
        }
    } else {
        end = file->content_length;
    }

    *out_length = end - start;
    return file->content + start;
}

void sysml2_diag_print(
    const Sysml2Diagnostic *diag,
    const Sysml2DiagOptions *options
) {
    FILE *out = options->output;
    bool use_color = sysml2_should_use_color(options->color_mode, out);

    /* Location */
    if (diag->file && diag->file->path) {
        if (use_color) fprintf(out, "%s", COLOR_BOLD);
        fprintf(out, "%s", diag->file->path);
        if (use_color) fprintf(out, "%s", COLOR_RESET);
        fprintf(out, ":");
    }

    if (diag->range.start.line > 0) {
        fprintf(out, "%u", diag->range.start.line);
        if (options->show_column_numbers && diag->range.start.column > 0) {
            fprintf(out, ":%u", diag->range.start.column);
        }
        fprintf(out, ": ");
    }

    /* Severity with color */
    if (use_color) {
        switch (diag->severity) {
            case SYSML2_SEVERITY_ERROR:
            case SYSML2_SEVERITY_FATAL:
                fprintf(out, "%s%s", COLOR_BOLD, COLOR_RED);
                break;
            case SYSML2_SEVERITY_WARNING:
                fprintf(out, "%s%s", COLOR_BOLD, COLOR_YELLOW);
                break;
            case SYSML2_SEVERITY_NOTE:
                fprintf(out, "%s%s", COLOR_BOLD, COLOR_CYAN);
                break;
        }
    }

    fprintf(out, "%s", sysml2_severity_to_string(diag->severity));

    /* Error code */
    if (options->show_error_codes) {
        fprintf(out, "[%s]", sysml2_diag_code_to_string(diag->code));
    }

    if (use_color) fprintf(out, "%s", COLOR_RESET);

    fprintf(out, ": ");

    /* Message */
    if (use_color) fprintf(out, "%s", COLOR_BOLD);
    fprintf(out, "%s", diag->message);
    if (use_color) fprintf(out, "%s", COLOR_RESET);
    fprintf(out, "\n");

    /* Source context */
    if (options->show_source_context && diag->file && diag->range.start.line > 0) {
        size_t line_len;
        const char *line = get_source_line(diag->file, diag->range.start.line, &line_len);

        if (line && line_len > 0) {
            /* Line number gutter */
            fprintf(out, "   |\n");
            fprintf(out, "%3u| ", diag->range.start.line);

            /* Print line content */
            fwrite(line, 1, line_len, out);
            fprintf(out, "\n");

            /* Caret line */
            fprintf(out, "   | ");

            /* Spaces up to error position */
            uint32_t col = diag->range.start.column;
            if (col > 0) col--;
            for (uint32_t i = 0; i < col && i < line_len; i++) {
                if (line[i] == '\t') {
                    fprintf(out, "\t");
                } else {
                    fprintf(out, " ");
                }
            }

            /* Caret/underline */
            if (use_color) fprintf(out, "%s", COLOR_GREEN);

            uint32_t end_col = diag->range.end.column;
            if (diag->range.end.line > diag->range.start.line) {
                end_col = line_len + 1;
            }
            if (end_col > 0) end_col--;

            if (end_col > col) {
                for (uint32_t i = col; i < end_col && i < line_len; i++) {
                    fprintf(out, "^");
                }
            } else {
                fprintf(out, "^");
            }

            if (use_color) fprintf(out, "%s", COLOR_RESET);
            fprintf(out, "\n");

            fprintf(out, "   |\n");
        }
    }

    /* Help text */
    if (diag->help) {
        fprintf(out, "   ");
        if (use_color) fprintf(out, "%s", COLOR_CYAN);
        fprintf(out, "= help: ");
        if (use_color) fprintf(out, "%s", COLOR_RESET);
        fprintf(out, "%s\n", diag->help);
    }

    /* Fix-it hints */
    for (size_t i = 0; i < diag->fixit_count; i++) {
        fprintf(out, "   ");
        if (use_color) fprintf(out, "%s", COLOR_GREEN);
        fprintf(out, "= suggestion: ");
        if (use_color) fprintf(out, "%s", COLOR_RESET);
        fprintf(out, "replace with '%s'\n", diag->fixits[i].replacement);
    }

    /* Notes */
    for (Sysml2Diagnostic *note = diag->notes; note; note = note->next) {
        sysml2_diag_print(note, options);
    }

    fprintf(out, "\n");
}

void sysml2_diag_print_all(const Sysml2DiagContext *ctx, const Sysml2DiagOptions *options) {
    for (Sysml2Diagnostic *diag = ctx->first; diag; diag = diag->next) {
        sysml2_diag_print(diag, options);
    }
}

void sysml2_diag_print_summary(const Sysml2DiagContext *ctx, FILE *output) {
    if (ctx->error_count == 0 && ctx->warning_count == 0) {
        return;
    }

    if (ctx->error_count > 0) {
        fprintf(output, "%zu error%s",
            ctx->error_count,
            ctx->error_count == 1 ? "" : "s");
    }

    if (ctx->error_count > 0 && ctx->warning_count > 0) {
        fprintf(output, " and ");
    }

    if (ctx->warning_count > 0) {
        fprintf(output, "%zu warning%s",
            ctx->warning_count,
            ctx->warning_count == 1 ? "" : "s");
    }

    fprintf(output, " generated.\n");
}
