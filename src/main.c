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
#include "sysml2/lexer.h"
#include "sysml2/ast_builder.h"
#include "sysml2/json_writer.h"
#include "sysml2/sysml_writer.h"
#include "sysml2/validator.h"
#include "sysml2/import_resolver.h"
#include "sysml_parser.h"  /* packcc-generated parser */

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
                /* Other warnings can be added here */
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
    /* Free dynamically allocated library paths array */
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

/* Read a file into memory */
static char *read_file(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0) {
        fclose(file);
        return NULL;
    }

    char *content = malloc(size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(content, 1, size, file);
    fclose(file);

    content[read] = '\0';
    if (out_size) {
        *out_size = read;
    }

    return content;
}

/* Read from stdin into memory (stdin is not seekable) */
static char *read_stdin(size_t *out_size) {
    size_t capacity = 4096;
    size_t length = 0;
    char *content = malloc(capacity);
    if (!content) return NULL;

    size_t bytes_read;
    while ((bytes_read = fread(content + length, 1, capacity - length, stdin)) > 0) {
        length += bytes_read;
        if (length == capacity) {
            capacity *= 2;
            char *new_content = realloc(content, capacity);
            if (!new_content) {
                free(content);
                return NULL;
            }
            content = new_content;
        }
    }

    content[length] = '\0';
    if (out_size) *out_size = length;
    return content;
}

/* Build line offset table */
static uint32_t *build_line_offsets(const char *content, size_t length, uint32_t *out_count) {
    /* Count lines first */
    uint32_t count = 1;
    for (size_t i = 0; i < length; i++) {
        if (content[i] == '\n') {
            count++;
        }
    }

    uint32_t *offsets = malloc(count * sizeof(uint32_t));
    if (!offsets) {
        return NULL;
    }

    offsets[0] = 0;
    uint32_t line = 1;
    for (size_t i = 0; i < length; i++) {
        if (content[i] == '\n' && line < count) {
            offsets[line++] = i + 1;
        }
    }

    *out_count = count;
    return offsets;
}

/* Process input content (shared by file and stdin processing)
 * If out_model is not NULL, returns the model instead of running validation */
static Sysml2Result process_input(
    const char *display_name,  /* e.g., "<stdin>" or file path */
    const char *content,
    size_t content_length,
    const Sysml2CliOptions *options,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    Sysml2DiagContext *diag_ctx,
    SysmlSemanticModel **out_model  /* Output: model (NULL to run validation inline) */
) {
    if (out_model) *out_model = NULL;
    if (options->verbose) {
        fprintf(stderr, "Processing: %s\n", display_name);
    }

    /* Dump tokens if requested (uses the old lexer) */
    if (options->dump_tokens) {
        /* Build line offsets for lexer */
        uint32_t line_count;
        uint32_t *line_offsets = build_line_offsets(content, content_length, &line_count);
        if (!line_offsets) {
            return SYSML2_ERROR_OUT_OF_MEMORY;
        }

        Sysml2SourceFile source_file = {
            .path = sysml2_intern(intern, display_name),
            .content = content,
            .content_length = content_length,
            .line_offsets = line_offsets,
            .line_count = line_count,
        };

        Sysml2Lexer lexer;
        sysml2_lexer_init(&lexer, &source_file, intern, diag_ctx);

        Sysml2Token token;
        printf("Tokens for %s:\n", display_name);
        printf("%-6s %-20s %-10s %s\n", "Line", "Type", "Loc", "Text");
        printf("%-6s %-20s %-10s %s\n", "----", "----", "---", "----");

        while (true) {
            token = sysml2_lexer_next(&lexer);
            printf("%-6u %-20s %u:%-8u " SYSML2_SV_FMT "\n",
                token.range.start.line,
                sysml2_token_type_to_string(token.type),
                token.range.start.line,
                token.range.start.column,
                SYSML2_SV_ARG(token.text));

            if (token.type == SYSML2_TOKEN_EOF) {
                break;
            }
        }
        printf("\n");
        free(line_offsets);
    }

    /* Create build context for AST building and semantic validation */
    SysmlBuildContext *build_ctx = sysml_build_context_create(arena, intern, display_name);
    if (!build_ctx) {
        fprintf(stderr, "error: failed to create build context\n");
        return SYSML2_ERROR_OUT_OF_MEMORY;
    }

    /* Parse input using packcc-generated parser */
    SysmlParserContext ctx = {
        .filename = display_name,
        .input = content,
        .input_len = content_length,
        .input_pos = 0,
        .error_count = 0,
        .line = 1,
        .col = 1,
        /* Furthest failure tracking */
        .furthest_pos = 0,
        .furthest_line = 0,
        .furthest_col = 0,
        .failed_rule_count = 0,
        .context_rule = NULL,
        /* AST building context */
        .build_ctx = build_ctx,
    };

    sysml_context_t *parser = sysml_create(&ctx);
    if (!parser) {
        fprintf(stderr, "error: failed to create parser\n");
        return SYSML2_ERROR_OUT_OF_MEMORY;
    }

    void *result = NULL;
    int parse_ok = sysml_parse(parser, &result);

    /* Track errors in the diagnostic context */
    if (ctx.error_count > 0) {
        diag_ctx->error_count += ctx.error_count;
    }

    /* Dump AST if requested - not yet implemented */
    if (options->dump_ast) {
        if (parse_ok) {
            fprintf(stderr, "note: --dump-ast not yet implemented with new parser\n");
        }
    }

    /* Finalize model and run semantic validation */
    Sysml2Result final_result = (parse_ok && ctx.error_count == 0) ? SYSML2_OK : SYSML2_ERROR_SYNTAX;

    if (parse_ok) {
        SysmlSemanticModel *model = sysml_build_finalize(build_ctx);
        if (model) {
            /* If caller wants the model back, return it for later multi-file validation */
            if (out_model) {
                *out_model = model;
            } else {
                /* Semantic validation (unless --parse-only) */
                if (!options->parse_only) {
                    SysmlValidationOptions val_opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
                    Sysml2Result val_result = sysml_validate(
                        model, diag_ctx, NULL, arena, intern, &val_opts
                    );
                    if (val_result != SYSML2_OK) {
                        final_result = SYSML2_ERROR_SEMANTIC;
                    }
                }

                /* JSON output (only in single-file mode) */
                if (options->output_format == SYSML2_OUTPUT_JSON) {
                    FILE *out = stdout;
                    if (options->output_file) {
                        out = fopen(options->output_file, "w");
                        if (!out) {
                            fprintf(stderr, "error: cannot open output file '%s': %s\n",
                                    options->output_file, strerror(errno));
                            sysml_destroy(parser);
                            return SYSML2_ERROR_FILE_READ;
                        }
                    }

                    SysmlJsonOptions json_opts = SYSML_JSON_OPTIONS_DEFAULT;
                    sysml_json_write(model, out, &json_opts);

                    if (options->output_file && out != stdout) {
                        fclose(out);
                    }
                }

                /* SysML output (pretty printing) */
                if (options->output_format == SYSML2_OUTPUT_SYSML) {
                    FILE *out = stdout;
                    if (options->output_file) {
                        out = fopen(options->output_file, "w");
                        if (!out) {
                            fprintf(stderr, "error: cannot open output file '%s': %s\n",
                                    options->output_file, strerror(errno));
                            sysml_destroy(parser);
                            return SYSML2_ERROR_FILE_READ;
                        }
                    }

                    sysml_sysml_write(model, out);

                    if (options->output_file && out != stdout) {
                        fclose(out);
                    }
                }
            }
        }
    }

    sysml_destroy(parser);

    return final_result;
}

/* Process a single file */
static Sysml2Result process_file(
    const char *path,
    const Sysml2CliOptions *options,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    Sysml2DiagContext *diag_ctx,
    SysmlSemanticModel **out_model
) {
    /* Read file */
    size_t content_length;
    char *content = read_file(path, &content_length);
    if (!content) {
        fprintf(stderr, "error: cannot read file '%s': %s\n", path, strerror(errno));
        return SYSML2_ERROR_FILE_READ;
    }

    Sysml2Result result = process_input(path, content, content_length, options, arena, intern, diag_ctx, out_model);
    free(content);
    return result;
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
    if (options.fix_in_place) {
        if (options.input_file_count == 0) {
            fprintf(stderr, "error: --fix requires file arguments (cannot read from stdin)\n");
            return 1;
        }
    }

    /* Initialize memory arena and string interning */
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Initialize diagnostics */
    Sysml2DiagContext diag_ctx;
    sysml2_diag_context_init(&diag_ctx, &arena);
    sysml2_diag_set_max_errors(&diag_ctx, options.max_errors);
    diag_ctx.treat_warnings_as_errors = options.treat_warnings_as_errors;

    Sysml2Result final_result = SYSML2_OK;

    /* --fix mode: Parse all files first, then rewrite if all succeed */
    if (options.fix_in_place) {
        /* Allocate model array */
        SysmlSemanticModel **models = malloc(options.input_file_count * sizeof(SysmlSemanticModel *));
        if (!models) {
            fprintf(stderr, "error: out of memory\n");
            sysml2_intern_destroy(&intern);
            sysml2_arena_destroy(&arena);
            return 1;
        }

        bool has_errors = false;

        /* Phase 1: Parse ALL files first */
        for (size_t i = 0; i < options.input_file_count; i++) {
            models[i] = NULL;
            result = process_file(options.input_files[i], &options, &arena, &intern, &diag_ctx, &models[i]);
            if (result != SYSML2_OK || models[i] == NULL) {
                has_errors = true;
            }
        }

        if (has_errors) {
            /* Print diagnostics and abort */
            Sysml2DiagOptions diag_options = {
                .output = stderr,
                .color_mode = options.color_mode,
                .show_source_context = true,
                .show_column_numbers = true,
                .show_error_codes = true,
            };
            sysml2_diag_print_all(&diag_ctx, &diag_options);
            sysml2_diag_print_summary(&diag_ctx, stderr);
            fprintf(stderr, "error: --fix aborted due to parse errors (no files modified)\n");
            free(models);
            sysml2_intern_destroy(&intern);
            sysml2_arena_destroy(&arena);
            return 1;
        }

        /* Phase 2: All parsed successfully, now rewrite */
        for (size_t i = 0; i < options.input_file_count; i++) {
            if (models[i]) {
                FILE *out = fopen(options.input_files[i], "w");
                if (!out) {
                    fprintf(stderr, "error: cannot open file '%s' for writing: %s\n",
                            options.input_files[i], strerror(errno));
                    has_errors = true;
                    continue;
                }

                sysml_sysml_write(models[i], out);
                fclose(out);

                if (options.verbose) {
                    fprintf(stderr, "Formatted: %s\n", options.input_files[i]);
                }
            }
        }

        free(models);

        /* Cleanup and exit */
        sysml2_intern_destroy(&intern);
        sysml2_arena_destroy(&arena);
        sysml2_cli_cleanup(&options);
        return has_errors ? 1 : 0;
    }

    /* Create import resolver */
    SysmlImportResolver *resolver = sysml_resolver_create(&arena, &intern);
    if (!resolver) {
        fprintf(stderr, "error: failed to create import resolver\n");
        sysml2_intern_destroy(&intern);
        sysml2_arena_destroy(&arena);
        return 1;
    }

    resolver->verbose = options.verbose;
    resolver->disabled = options.no_resolve;

    /* Add library paths from environment and CLI */
    sysml_resolver_add_paths_from_env(resolver);
    for (size_t i = 0; i < options.library_path_count; i++) {
        sysml_resolver_add_path(resolver, options.library_paths[i]);
    }

    if (options.input_file_count == 0) {
        /* Read from stdin - single file mode (no import resolution for stdin) */
        size_t content_length;
        char *content = read_stdin(&content_length);
        if (!content) {
            fprintf(stderr, "error: failed to read from stdin\n");
            sysml_resolver_destroy(resolver);
            sysml2_intern_destroy(&intern);
            sysml2_arena_destroy(&arena);
            return 1;
        }
        result = process_input("<stdin>", content, content_length, &options, &arena, &intern, &diag_ctx, NULL);
        free(content);
        if (result != SYSML2_OK) {
            final_result = result;
        }
    } else {
        /* Single or multiple files - with import resolution */
        SysmlSemanticModel **input_models = malloc(options.input_file_count * sizeof(SysmlSemanticModel *));
        if (!input_models) {
            fprintf(stderr, "error: out of memory\n");
            sysml_resolver_destroy(resolver);
            sysml2_intern_destroy(&intern);
            sysml2_arena_destroy(&arena);
            return 1;
        }

        bool has_parse_errors = false;

        /* Pass 1: Parse all input files */
        for (size_t i = 0; i < options.input_file_count; i++) {
            input_models[i] = NULL;
            result = process_file(options.input_files[i], &options, &arena, &intern, &diag_ctx, &input_models[i]);
            if (result != SYSML2_OK) {
                has_parse_errors = true;
            }
            if (input_models[i]) {
                /* Cache the model for import resolution */
                sysml_resolver_cache_model(resolver, options.input_files[i], input_models[i]);
            }
            if (sysml2_diag_should_stop(&diag_ctx)) {
                break;
            }
        }

        /* Pass 2: Resolve imports (unless --no-resolve or parse errors) */
        if (!options.no_resolve && !has_parse_errors) {
            for (size_t i = 0; i < options.input_file_count; i++) {
                if (input_models[i]) {
                    result = sysml_resolver_resolve_imports(resolver, input_models[i], &diag_ctx);
                    if (result != SYSML2_OK && final_result == SYSML2_OK) {
                        /* Import resolution failure - not a hard error, validation will catch missing refs */
                    }
                }
                if (sysml2_diag_should_stop(&diag_ctx)) {
                    break;
                }
            }
        }

        /* Pass 3: Unified validation (unless --parse-only) */
        if (!options.parse_only && !has_parse_errors) {
            /* Get all cached models (input files + resolved imports) */
            size_t all_model_count;
            SysmlSemanticModel **all_models = sysml_resolver_get_all_models(resolver, &all_model_count);

            if (all_models && all_model_count > 0) {
                SysmlValidationOptions val_opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
                Sysml2Result val_result = sysml_validate_multi(
                    all_models, all_model_count, &diag_ctx, &arena, &intern, &val_opts
                );
                if (val_result != SYSML2_OK) {
                    final_result = SYSML2_ERROR_SEMANTIC;
                }
                free(all_models);
            }
        } else if (has_parse_errors) {
            final_result = SYSML2_ERROR_SYNTAX;
        }

        /* JSON output (only for first input file, for compatibility) */
        if (options.output_format == SYSML2_OUTPUT_JSON && !has_parse_errors && input_models[0]) {
            FILE *out = stdout;
            if (options.output_file) {
                out = fopen(options.output_file, "w");
                if (!out) {
                    fprintf(stderr, "error: cannot open output file '%s': %s\n",
                            options.output_file, strerror(errno));
                } else {
                    SysmlJsonOptions json_opts = SYSML_JSON_OPTIONS_DEFAULT;
                    sysml_json_write(input_models[0], out, &json_opts);
                    fclose(out);
                }
            } else {
                SysmlJsonOptions json_opts = SYSML_JSON_OPTIONS_DEFAULT;
                sysml_json_write(input_models[0], out, &json_opts);
            }
        }

        /* SysML output (pretty printing, only for first input file) */
        if (options.output_format == SYSML2_OUTPUT_SYSML && !has_parse_errors && input_models[0]) {
            FILE *out = stdout;
            if (options.output_file) {
                out = fopen(options.output_file, "w");
                if (!out) {
                    fprintf(stderr, "error: cannot open output file '%s': %s\n",
                            options.output_file, strerror(errno));
                } else {
                    sysml_sysml_write(input_models[0], out);
                    fclose(out);
                }
            } else {
                sysml_sysml_write(input_models[0], out);
            }
        }

        free(input_models);
    }

    /* Destroy resolver */
    sysml_resolver_destroy(resolver);

    /* Print diagnostics */
    Sysml2DiagOptions diag_options = {
        .output = stderr,
        .color_mode = options.color_mode,
        .show_source_context = true,
        .show_column_numbers = true,
        .show_error_codes = true,
    };
    sysml2_diag_print_all(&diag_ctx, &diag_options);
    sysml2_diag_print_summary(&diag_ctx, stderr);

    /* Cleanup */
    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
    sysml2_cli_cleanup(&options);

    /* Exit codes:
     * 0 - Success (no errors)
     * 1 - Parse/syntax error
     * 2 - Semantic/validation error
     */
    if (final_result == SYSML2_OK && diag_ctx.error_count == 0) {
        return 0;
    } else if (final_result == SYSML2_ERROR_SYNTAX) {
        return 1;
    } else if (final_result == SYSML2_ERROR_SEMANTIC || diag_ctx.error_count > 0) {
        return 2;
    } else {
        return 1;  /* Other errors (file not found, etc.) */
    }
}
