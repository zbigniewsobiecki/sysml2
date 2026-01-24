/*
 * SysML v2 Parser - CLI Entry Point
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/common.h"
#include "sysml2/arena.h"
#include "sysml2/intern.h"
#include "sysml2/diagnostic.h"
#include "sysml2/cli.h"
#include "sysml2/pipeline.h"
#include "sysml2/sysml_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

/* Long options for getopt_long */
static const struct option long_options[] = {
    {"output",      required_argument, 0, 'o'},
    {"format",      required_argument, 0, 'f'},
    {"fix",         no_argument,       0, 'F'},
    {"color",       optional_argument, 0, 'c'},
    {"max-errors",  required_argument, 0, 'm'},
    {"dump-tokens", no_argument,       0, 'T'},
    {"dump-ast",    no_argument,       0, 'A'},
    {"verbose",     no_argument,       0, 'v'},
    {"parse-only",  no_argument,       0, 'P'},
    {"no-validate", no_argument,       0, 'P'},  /* alias for --parse-only */
    {"no-resolve",  no_argument,       0, 'R'},
    {"help",        no_argument,       0, 'h'},
    {"version",     no_argument,       0, 'V'},
    {0, 0, 0, 0}
};

static const char *short_options = "o:f:W:I:hVvTAPFR";

/* Parse color mode from string */
static Sysml2ColorMode parse_color_mode(const char *arg) {
    if (!arg || strcmp(arg, "auto") == 0) {
        return SYSML2_COLOR_AUTO;
    } else if (strcmp(arg, "always") == 0) {
        return SYSML2_COLOR_ALWAYS;
    } else if (strcmp(arg, "never") == 0) {
        return SYSML2_COLOR_NEVER;
    }
    return SYSML2_COLOR_AUTO;
}

/* Parse output format from string */
static Sysml2OutputFormat parse_output_format(const char *arg) {
    if (strcmp(arg, "json") == 0) {
        return SYSML2_OUTPUT_JSON;
    } else if (strcmp(arg, "xml") == 0) {
        return SYSML2_OUTPUT_XML;
    } else if (strcmp(arg, "sysml") == 0) {
        return SYSML2_OUTPUT_SYSML;
    }
    return SYSML2_OUTPUT_NONE;
}

Sysml2Result sysml2_cli_parse(Sysml2CliOptions *options, int argc, char **argv) {
    /* Set defaults */
    memset(options, 0, sizeof(*options));
    options->color_mode = SYSML2_COLOR_AUTO;
    options->max_errors = 20;

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (opt) {
            case 'o':
                options->output_file = optarg;
                break;

            case 'f':
                options->output_format = parse_output_format(optarg);
                break;

            case 'c':
                options->color_mode = parse_color_mode(optarg);
                break;

            case 'm':
                options->max_errors = (size_t)atoi(optarg);
                break;

            case 'W':
                if (strcmp(optarg, "error") == 0) {
                    options->treat_warnings_as_errors = true;
                }
                break;

            case 'T':
                options->dump_tokens = true;
                break;

            case 'A':
                options->dump_ast = true;
                break;

            case 'v':
                options->verbose = true;
                break;

            case 'P':
                options->parse_only = true;
                break;

            case 'F':
                options->fix_in_place = true;
                break;

            case 'I':
                /* Add library path */
                if (options->library_path_count >= options->library_path_capacity) {
                    size_t new_cap = options->library_path_capacity == 0 ? 8 : options->library_path_capacity * 2;
                    const char **new_paths = realloc((void *)options->library_paths, new_cap * sizeof(char *));
                    if (!new_paths) {
                        return SYSML2_ERROR_OUT_OF_MEMORY;
                    }
                    options->library_paths = new_paths;
                    options->library_path_capacity = new_cap;
                }
                options->library_paths[options->library_path_count++] = optarg;
                break;

            case 'R':
                options->no_resolve = true;
                break;

            case 'h':
                options->show_help = true;
                return SYSML2_OK;

            case 'V':
                options->show_version = true;
                return SYSML2_OK;

            case '?':
            default:
                return SYSML2_ERROR_SYNTAX;
        }
    }

    /* Collect input files */
    int file_count = argc - optind;
    if (file_count > 0) {
        options->input_files = (const char **)&argv[optind];
        options->input_file_count = file_count;
    }

    return SYSML2_OK;
}

void sysml2_cli_cleanup(Sysml2CliOptions *options) {
    if (options->library_paths) {
        free((void *)options->library_paths);
        options->library_paths = NULL;
    }
}

void sysml2_cli_print_help(FILE *output) {
    fprintf(output,
        "sysml2 - SysML v2 Parser and Validator\n"
        "\n"
        "Usage: sysml2 [options] [file]...\n"
        "\n"
        "If no files are specified, reads from standard input.\n"
        "\n"
        "Options:\n"
        "  -o, --output <file>    Write output to file\n"
        "  -f, --format <fmt>     Output format: json, xml, sysml (default: none)\n"
        "  -I <path>              Add library search path for imports\n"
        "      --fix              Format and rewrite files in place\n"
        "  -P, --parse-only       Parse only, skip semantic validation\n"
        "      --no-validate      Same as --parse-only\n"
        "      --no-resolve       Disable automatic import resolution\n"
        "  --color[=when]         Colorize output (auto, always, never)\n"
        "  --max-errors <n>       Stop after n errors (default: 20)\n"
        "  -W<warning>            Enable warning (e.g., -Werror)\n"
        "  --dump-tokens          Dump lexer tokens\n"
        "  --dump-ast             Dump parsed AST\n"
        "  -v, --verbose          Verbose output\n"
        "  -h, --help             Show help\n"
        "  --version              Show version\n"
        "\n"
        "Environment:\n"
        "  SYSML2_LIBRARY_PATH    Colon-separated list of library search paths\n"
        "\n"
        "Examples:\n"
        "  sysml2 model.kerml              Validate a KerML file\n"
        "  sysml2 -f json model.sysml      Parse and output JSON AST\n"
        "  sysml2 -f sysml model.sysml     Pretty print to stdout\n"
        "  sysml2 --fix model.sysml        Format in place\n"
        "  sysml2 -I /path/to/lib model.sysml  Validate with library imports\n"
        "  cat model.sysml | sysml2        Parse from stdin\n"
        "  echo 'package P;' | sysml2      Quick syntax check\n"
        "\n"
        "Exit codes:\n"
        "  0  Success (no errors)\n"
        "  1  Parse/syntax error\n"
        "  2  Semantic/validation error\n"
        "\n"
    );
}

void sysml2_cli_print_version(FILE *output) {
    fprintf(output, "sysml2 version %s\n", SYSML2_VERSION_STRING);
}

/* Result code to string */
const char *sysml2_result_to_string(Sysml2Result result) {
    switch (result) {
        case SYSML2_OK: return "success";
        case SYSML2_ERROR_FILE_NOT_FOUND: return "file not found";
        case SYSML2_ERROR_FILE_READ: return "file read error";
        case SYSML2_ERROR_OUT_OF_MEMORY: return "out of memory";
        case SYSML2_ERROR_INVALID_UTF8: return "invalid UTF-8";
        case SYSML2_ERROR_SYNTAX: return "syntax error";
        case SYSML2_ERROR_SEMANTIC: return "semantic error";
        default: return "unknown error";
    }
}

/* Add directory containing a file to resolver's search paths */
static void add_file_directory_to_resolver(Sysml2ImportResolver *resolver, const char *file_path) {
    char *path_copy = strdup(file_path);
    if (path_copy) {
        char *last_slash = strrchr(path_copy, '/');
        if (last_slash) {
            *last_slash = '\0';
            sysml2_resolver_add_path(resolver, path_copy);
        } else {
            sysml2_resolver_add_path(resolver, ".");
        }
        free(path_copy);
    }
}

/* Run --fix mode: parse, resolve, validate, then rewrite files */
static int run_fix_mode(
    Sysml2PipelineContext *ctx,
    const Sysml2CliOptions *options
) {
    Sysml2ImportResolver *resolver = sysml2_pipeline_get_resolver(ctx);
    Sysml2DiagContext *diag = sysml2_pipeline_get_diag(ctx);
    resolver->strict_imports = true;  /* Fail on missing imports in --fix mode */

    /* Add directories containing input files to search paths */
    for (size_t i = 0; i < options->input_file_count; i++) {
        add_file_directory_to_resolver(resolver, options->input_files[i]);
    }

    /* Allocate model array */
    SysmlSemanticModel **models = malloc(options->input_file_count * sizeof(SysmlSemanticModel *));
    if (!models) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    bool has_errors = false;
    size_t error_count_before = diag->error_count;

    /* Pass 1: Parse ALL input files */
    for (size_t i = 0; i < options->input_file_count; i++) {
        models[i] = NULL;
        Sysml2Result result = sysml2_pipeline_process_file(ctx, options->input_files[i], &models[i]);
        if (result != SYSML2_OK || models[i] == NULL) {
            has_errors = true;
        }
        if (models[i]) {
            sysml2_resolver_cache_model(resolver, options->input_files[i], models[i]);
        }
    }

    if (has_errors) {
        sysml2_pipeline_print_diagnostics(ctx, stderr);
        fprintf(stderr, "error: --fix aborted due to parse errors (no files modified)\n");
        free(models);
        return 1;
    }

    /* Pass 2: Resolve imports (unless --no-resolve) */
    if (!options->no_resolve) {
        for (size_t i = 0; i < options->input_file_count; i++) {
            if (models[i]) {
                sysml2_resolver_resolve_imports(resolver, models[i], diag);
            }
            if (sysml2_diag_should_stop(diag)) {
                break;
            }
        }

        if (diag->error_count > error_count_before) {
            sysml2_pipeline_print_diagnostics(ctx, stderr);
            fprintf(stderr, "error: --fix aborted due to import errors (no files modified)\n");
            free(models);
            return 1;
        }
    }

    /* Pass 3: Unified validation (unless --parse-only) */
    if (!options->parse_only) {
        Sysml2Result val_result = sysml2_pipeline_validate_all(ctx);
        if (val_result != SYSML2_OK || diag->error_count > error_count_before) {
            sysml2_pipeline_print_diagnostics(ctx, stderr);
            fprintf(stderr, "error: --fix aborted due to validation errors (no files modified)\n");
            free(models);
            return 1;
        }
    }

    /* Pass 4: All checks passed, now rewrite input files */
    for (size_t i = 0; i < options->input_file_count; i++) {
        if (models[i]) {
            FILE *out = fopen(options->input_files[i], "w");
            if (!out) {
                fprintf(stderr, "error: cannot open file '%s' for writing: %s\n",
                        options->input_files[i], strerror(errno));
                has_errors = true;
                continue;
            }

            sysml2_sysml_write(models[i], out);
            fclose(out);

            if (options->verbose) {
                fprintf(stderr, "Formatted: %s\n", options->input_files[i]);
            }
        }
    }

    free(models);
    return has_errors ? 1 : 0;
}

/* Run normal mode: parse, resolve, validate, output */
static int run_normal_mode(
    Sysml2PipelineContext *ctx,
    const Sysml2CliOptions *options
) {
    Sysml2ImportResolver *resolver = sysml2_pipeline_get_resolver(ctx);
    Sysml2DiagContext *diag = sysml2_pipeline_get_diag(ctx);

    /* Add directories containing input files to search paths */
    for (size_t i = 0; i < options->input_file_count; i++) {
        add_file_directory_to_resolver(resolver, options->input_files[i]);
    }

    Sysml2Result final_result = SYSML2_OK;

    if (options->input_file_count == 0) {
        /* Read from stdin - single file mode (no import resolution for stdin) */
        SysmlSemanticModel *model = NULL;
        Sysml2Result result = sysml2_pipeline_process_stdin(ctx, &model);
        if (result != SYSML2_OK) {
            final_result = result;
        }

        /* Validation for single stdin input */
        if (model && !options->parse_only) {
            sysml2_resolver_cache_model(resolver, "<stdin>", model);
            Sysml2Result val_result = sysml2_pipeline_validate_all(ctx);
            if (val_result != SYSML2_OK) {
                final_result = SYSML2_ERROR_SEMANTIC;
            }
        }

        /* Output for stdin */
        if (model && final_result == SYSML2_OK) {
            if (options->output_format == SYSML2_OUTPUT_JSON) {
                FILE *out = options->output_file ? fopen(options->output_file, "w") : stdout;
                if (out) {
                    sysml2_pipeline_write_json(ctx, model, out);
                    if (options->output_file) fclose(out);
                }
            } else if (options->output_format == SYSML2_OUTPUT_SYSML) {
                FILE *out = options->output_file ? fopen(options->output_file, "w") : stdout;
                if (out) {
                    sysml2_pipeline_write_sysml(ctx, model, out);
                    if (options->output_file) fclose(out);
                }
            }
        }
    } else {
        /* Multiple files - with import resolution */
        SysmlSemanticModel **input_models = malloc(options->input_file_count * sizeof(SysmlSemanticModel *));
        if (!input_models) {
            fprintf(stderr, "error: out of memory\n");
            return 1;
        }

        bool has_parse_errors = false;

        /* Pass 1: Parse all input files */
        for (size_t i = 0; i < options->input_file_count; i++) {
            input_models[i] = NULL;
            Sysml2Result result = sysml2_pipeline_process_file(ctx, options->input_files[i], &input_models[i]);
            if (result != SYSML2_OK) {
                has_parse_errors = true;
            }
            if (input_models[i]) {
                sysml2_resolver_cache_model(resolver, options->input_files[i], input_models[i]);
            }
            if (sysml2_diag_should_stop(diag)) {
                break;
            }
        }

        /* Pass 2: Resolve imports */
        if (!options->no_resolve && !has_parse_errors) {
            for (size_t i = 0; i < options->input_file_count; i++) {
                if (input_models[i]) {
                    sysml2_resolver_resolve_imports(resolver, input_models[i], diag);
                }
                if (sysml2_diag_should_stop(diag)) {
                    break;
                }
            }
        }

        /* Pass 3: Unified validation */
        if (!options->parse_only && !has_parse_errors) {
            Sysml2Result val_result = sysml2_pipeline_validate_all(ctx);
            if (val_result != SYSML2_OK) {
                final_result = SYSML2_ERROR_SEMANTIC;
            }
        } else if (has_parse_errors) {
            final_result = SYSML2_ERROR_SYNTAX;
        }

        /* Output (only for first input file) */
        if (!has_parse_errors && input_models[0]) {
            if (options->output_format == SYSML2_OUTPUT_JSON) {
                FILE *out = options->output_file ? fopen(options->output_file, "w") : stdout;
                if (out) {
                    sysml2_pipeline_write_json(ctx, input_models[0], out);
                    if (options->output_file) fclose(out);
                }
            } else if (options->output_format == SYSML2_OUTPUT_SYSML) {
                FILE *out = options->output_file ? fopen(options->output_file, "w") : stdout;
                if (out) {
                    sysml2_pipeline_write_sysml(ctx, input_models[0], out);
                    if (options->output_file) fclose(out);
                }
            }
        }

        free(input_models);
    }

    /* Print diagnostics */
    sysml2_pipeline_print_diagnostics(ctx, stderr);

    /* Exit codes */
    if (final_result == SYSML2_OK && !sysml2_pipeline_has_errors(ctx)) {
        return 0;
    } else if (final_result == SYSML2_ERROR_SYNTAX) {
        return 1;
    } else {
        return 2;
    }
}

int main(int argc, char **argv) {
    Sysml2CliOptions options;
    Sysml2Result result = sysml2_cli_parse(&options, argc, argv);

    if (result != SYSML2_OK) {
        sysml2_cli_print_help(stderr);
        return 1;
    }

    if (options.show_help) {
        sysml2_cli_print_help(stdout);
        return 0;
    }

    if (options.show_version) {
        sysml2_cli_print_version(stdout);
        return 0;
    }

    /* --fix mode requires file arguments */
    if (options.fix_in_place && options.input_file_count == 0) {
        fprintf(stderr, "error: --fix requires file arguments (cannot read from stdin)\n");
        return 1;
    }

    /* Initialize memory arena and string interning */
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Create pipeline context */
    Sysml2PipelineContext *ctx = sysml2_pipeline_create(&arena, &intern, &options);
    if (!ctx) {
        fprintf(stderr, "error: failed to create pipeline context\n");
        sysml2_intern_destroy(&intern);
        sysml2_arena_destroy(&arena);
        return 1;
    }

    /* Run appropriate mode */
    int exit_code;
    if (options.fix_in_place) {
        exit_code = run_fix_mode(ctx, &options);
    } else {
        exit_code = run_normal_mode(ctx, &options);
    }

    /* Cleanup */
    sysml2_pipeline_destroy(ctx);
    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
    sysml2_cli_cleanup(&options);

    return exit_code;
}
