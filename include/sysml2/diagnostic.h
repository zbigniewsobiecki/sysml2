/*
 * SysML v2 Parser - Diagnostic System
 *
 * Clang-style error reporting with source context,
 * color support, and fix-it hints.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_DIAGNOSTIC_H
#define SYSML2_DIAGNOSTIC_H

#include "common.h"
#include "arena.h"
#include <stdio.h>

/* Error code ranges:
 * E1xxx - Lexical errors
 * E2xxx - Syntax errors
 * E3xxx - Semantic errors
 * W1xxx - Warnings
 */

typedef enum {
    /* Lexical errors (E1xxx) */
    SYSML2_DIAG_E1001_INVALID_CHAR = 1001,
    SYSML2_DIAG_E1002_UNTERMINATED_STRING,
    SYSML2_DIAG_E1003_UNTERMINATED_COMMENT,
    SYSML2_DIAG_E1004_UNTERMINATED_NAME,
    SYSML2_DIAG_E1005_INVALID_NUMBER,
    SYSML2_DIAG_E1006_INVALID_ESCAPE,

    /* Syntax errors (E2xxx) */
    SYSML2_DIAG_E2001_EXPECTED_SEMICOLON = 2001,
    SYSML2_DIAG_E2002_EXPECTED_IDENTIFIER,
    SYSML2_DIAG_E2003_EXPECTED_LBRACE,
    SYSML2_DIAG_E2004_EXPECTED_RBRACE,
    SYSML2_DIAG_E2005_EXPECTED_COLON,
    SYSML2_DIAG_E2006_UNEXPECTED_TOKEN,
    SYSML2_DIAG_E2007_EXPECTED_EXPRESSION,
    SYSML2_DIAG_E2008_EXPECTED_TYPE,
    SYSML2_DIAG_E2009_EXPECTED_LPAREN,
    SYSML2_DIAG_E2010_EXPECTED_RPAREN,
    SYSML2_DIAG_E2011_EXPECTED_LBRACKET,
    SYSML2_DIAG_E2012_EXPECTED_RBRACKET,

    /* Semantic errors (E3xxx) */
    SYSML2_DIAG_E3001_UNDEFINED_TYPE = 3001,
    SYSML2_DIAG_E3002_UNDEFINED_FEATURE,
    SYSML2_DIAG_E3003_UNDEFINED_NAMESPACE,
    SYSML2_DIAG_E3004_DUPLICATE_NAME,
    SYSML2_DIAG_E3005_CIRCULAR_SPECIALIZATION,
    SYSML2_DIAG_E3006_TYPE_MISMATCH,
    SYSML2_DIAG_E3007_MULTIPLICITY_ERROR,
    SYSML2_DIAG_E3008_REDEFINITION_ERROR,
    SYSML2_DIAG_E3009_CIRCULAR_IMPORT,
    SYSML2_DIAG_E3010_IMPORT_NOT_FOUND,

    /* Warnings (W1xxx) */
    SYSML2_DIAG_W1001_UNUSED_IMPORT = 10001,
    SYSML2_DIAG_W1002_SHADOWED_NAME,
    SYSML2_DIAG_W1003_DEPRECATED,
} Sysml2DiagCode;

/* Diagnostic severity levels */
typedef enum {
    SYSML2_SEVERITY_NOTE,
    SYSML2_SEVERITY_WARNING,
    SYSML2_SEVERITY_ERROR,
    SYSML2_SEVERITY_FATAL,
} Sysml2Severity;

/* Fix-it hint */
typedef struct {
    Sysml2SourceRange range;    /* Range to replace */
    const char *replacement;     /* Replacement text */
} Sysml2FixIt;

/* Single diagnostic message */
typedef struct Sysml2Diagnostic {
    struct Sysml2Diagnostic *next;  /* Linked list for notes */

    Sysml2DiagCode code;
    Sysml2Severity severity;
    Sysml2SourceRange range;
    const Sysml2SourceFile *file;

    const char *message;         /* Main diagnostic message */
    const char *help;            /* Optional help text */

    Sysml2FixIt *fixits;         /* Array of fix-its */
    size_t fixit_count;

    struct Sysml2Diagnostic *notes; /* Related notes */
} Sysml2Diagnostic;

/* Diagnostic collection/context */
typedef struct {
    Sysml2Arena *arena;         /* Arena for allocations */
    Sysml2Diagnostic *first;    /* First diagnostic */
    Sysml2Diagnostic *last;     /* Last diagnostic (for appending) */

    size_t error_count;         /* Number of errors */
    size_t warning_count;       /* Number of warnings */
    size_t parse_error_count;   /* E1xxx and E2xxx errors */
    size_t semantic_error_count; /* E3xxx errors */
    size_t max_errors;          /* Stop after this many errors (0 = unlimited) */

    bool treat_warnings_as_errors;
    bool has_fatal;             /* Has a fatal error occurred? */
} Sysml2DiagContext;

/* Color mode for output */
typedef enum {
    SYSML2_COLOR_AUTO,   /* Detect from terminal */
    SYSML2_COLOR_ALWAYS,
    SYSML2_COLOR_NEVER,
} Sysml2ColorMode;

/* Diagnostic output options */
typedef struct {
    FILE *output;               /* Output stream */
    Sysml2ColorMode color_mode;
    bool show_source_context;   /* Show source lines */
    bool show_column_numbers;   /* Show column in location */
    bool show_error_codes;      /* Show [E1001] etc */
} Sysml2DiagOptions;

/* Initialize diagnostic context */
void sysml2_diag_context_init(Sysml2DiagContext *ctx, Sysml2Arena *arena);

/* Clear all diagnostics and reset counts */
void sysml2_diag_clear(Sysml2DiagContext *ctx);

/* Set maximum number of errors before stopping */
void sysml2_diag_set_max_errors(Sysml2DiagContext *ctx, size_t max);

/* Check if we should stop due to error limit */
bool sysml2_diag_should_stop(const Sysml2DiagContext *ctx);

/* Check if there are parse errors (E1xxx or E2xxx) */
bool sysml2_diag_has_parse_errors(const Sysml2DiagContext *ctx);

/* Check if there are semantic errors (E3xxx) */
bool sysml2_diag_has_semantic_errors(const Sysml2DiagContext *ctx);

/* Create a new diagnostic */
Sysml2Diagnostic *sysml2_diag_create(
    Sysml2DiagContext *ctx,
    Sysml2DiagCode code,
    Sysml2Severity severity,
    const Sysml2SourceFile *file,
    Sysml2SourceRange range,
    const char *message
);

/* Add help text to a diagnostic */
void sysml2_diag_add_help(Sysml2Diagnostic *diag, Sysml2DiagContext *ctx, const char *help);

/* Add a fix-it hint to a diagnostic */
void sysml2_diag_add_fixit(
    Sysml2Diagnostic *diag,
    Sysml2DiagContext *ctx,
    Sysml2SourceRange range,
    const char *replacement
);

/* Add a note to a diagnostic */
Sysml2Diagnostic *sysml2_diag_add_note(
    Sysml2Diagnostic *parent,
    Sysml2DiagContext *ctx,
    const Sysml2SourceFile *file,
    Sysml2SourceRange range,
    const char *message
);

/* Emit a diagnostic to the context */
void sysml2_diag_emit(Sysml2DiagContext *ctx, Sysml2Diagnostic *diag);

/* Print all diagnostics */
void sysml2_diag_print_all(const Sysml2DiagContext *ctx, const Sysml2DiagOptions *options);

/* Print a single diagnostic */
void sysml2_diag_print(
    const Sysml2Diagnostic *diag,
    const Sysml2DiagOptions *options
);

/* Print diagnostic summary (e.g., "3 errors and 2 warnings generated.") */
void sysml2_diag_print_summary(const Sysml2DiagContext *ctx, FILE *output);

/* Get string representation of severity */
const char *sysml2_severity_to_string(Sysml2Severity severity);

/* Get string representation of diagnostic code */
const char *sysml2_diag_code_to_string(Sysml2DiagCode code);

/* Check if colors should be used */
bool sysml2_should_use_color(Sysml2ColorMode mode, FILE *output);

/* Convenience macros for creating diagnostics */
#define SYSML2_DIAG_ERROR(ctx, code, file, range, msg) \
    sysml2_diag_emit((ctx), sysml2_diag_create((ctx), (code), SYSML2_SEVERITY_ERROR, (file), (range), (msg)))

#define SYSML2_DIAG_WARNING(ctx, code, file, range, msg) \
    sysml2_diag_emit((ctx), sysml2_diag_create((ctx), (code), SYSML2_SEVERITY_WARNING, (file), (range), (msg)))

#endif /* SYSML2_DIAGNOSTIC_H */
