/*
 * SysML v2 Parser - CLI Options
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_CLI_H
#define SYSML2_CLI_H

#include "common.h"
#include "diagnostic.h"

/* Output formats */
typedef enum {
    SYSML2_OUTPUT_NONE,      /* Validation only */
    SYSML2_OUTPUT_JSON,      /* JSON AST output */
    SYSML2_OUTPUT_XML,       /* XML AST output (future) */
    SYSML2_OUTPUT_SYSML,     /* Formatted SysML/KerML source */
} Sysml2OutputFormat;

/* CLI options */
typedef struct {
    /* Input/output */
    const char **input_files;       /* Array of input file paths */
    size_t input_file_count;
    const char *output_file;        /* Output file (NULL for stdout) */
    Sysml2OutputFormat output_format;

    /* Library paths for import resolution */
    const char **library_paths;     /* Array of library search paths (-I) */
    size_t library_path_count;
    size_t library_path_capacity;

    /* Query options */
    const char **select_patterns;   /* Array of --select patterns */
    size_t select_pattern_count;
    size_t select_pattern_capacity;

    /* Modification options */
    const char **set_fragments;     /* Fragment file paths for --set */
    const char **set_targets;       /* Target scopes for --set (parallel array) */
    size_t set_count;
    size_t set_capacity;

    const char **delete_patterns;   /* Delete patterns for --delete */
    size_t delete_pattern_count;
    size_t delete_pattern_capacity;

    bool create_scope;              /* --create-scope flag */
    bool replace_scope;             /* --replace-scope flag: clear scope before inserting */
    bool force_replace;             /* --force-replace flag: suppress data loss warning */
    bool dry_run;                   /* --dry-run flag */

    /* Diagnostics */
    Sysml2ColorMode color_mode;
    size_t max_errors;
    bool treat_warnings_as_errors;

    /* Debug options */
    bool dump_tokens;           /* Print lexer tokens */
    bool dump_ast;              /* Print parsed AST */
    bool verbose;               /* Verbose output */

    /* Mode options */
    bool parse_only;            /* Skip semantic validation */
    bool fix_in_place;          /* --fix: rewrite files with formatting */
    bool no_resolve;            /* --no-resolve: disable import resolution */
    bool allow_semantic_errors; /* --allow-semantic-errors: write files despite E3xxx errors */
    bool recursive;             /* --recursive: load all .sysml files from directory */
    bool list_mode;             /* --list: output element summary (name + kind) */

    /* Meta */
    bool show_help;
    bool show_version;
} Sysml2CliOptions;

/* Parse command line arguments */
Sysml2Result sysml2_cli_parse(Sysml2CliOptions *options, int argc, char **argv);

/* Free allocated memory in options */
void sysml2_cli_cleanup(Sysml2CliOptions *options);

/* Print help message */
void sysml2_cli_print_help(FILE *output);

/* Print version information */
void sysml2_cli_print_version(FILE *output);

#endif /* SYSML2_CLI_H */
