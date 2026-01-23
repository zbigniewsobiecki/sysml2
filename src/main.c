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
#include "sysml2/parser.h"
#include "sysml2/semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

/* Long options for getopt_long */
static const struct option long_options[] = {
    {"output",      required_argument, 0, 'o'},
    {"format",      required_argument, 0, 'f'},
    {"color",       optional_argument, 0, 'c'},
    {"max-errors",  required_argument, 0, 'm'},
    {"dump-tokens", no_argument,       0, 'T'},
    {"dump-ast",    no_argument,       0, 'A'},
    {"verbose",     no_argument,       0, 'v'},
    {"help",        no_argument,       0, 'h'},
    {"version",     no_argument,       0, 'V'},
    {0, 0, 0, 0}
};

static const char *short_options = "o:f:W:hVvTA";

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
    /* No dynamically allocated memory in current implementation */
    (void)options;
}

void sysml2_cli_print_help(FILE *output) {
    fprintf(output,
        "sysml2 - SysML v2 Parser and Validator\n"
        "\n"
        "Usage: sysml2 [options] <file>...\n"
        "\n"
        "Options:\n"
        "  -o, --output <file>    Write output to file\n"
        "  -f, --format <fmt>     Output format: json, xml (default: none)\n"
        "  --color[=when]         Colorize output (auto, always, never)\n"
        "  --max-errors <n>       Stop after n errors (default: 20)\n"
        "  -W<warning>            Enable warning (e.g., -Werror)\n"
        "  --dump-tokens          Dump lexer tokens\n"
        "  --dump-ast             Dump parsed AST\n"
        "  -v, --verbose          Verbose output\n"
        "  -h, --help             Show help\n"
        "  --version              Show version\n"
        "\n"
        "Examples:\n"
        "  sysml2 model.kerml              Validate a KerML file\n"
        "  sysml2 -f json model.sysml      Parse and output JSON AST\n"
        "  sysml2 --dump-tokens file.kerml Show lexer tokens\n"
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

/* Process a single file */
static Sysml2Result process_file(
    const char *path,
    const Sysml2CliOptions *options,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    Sysml2DiagContext *diag_ctx
) {
    if (options->verbose) {
        fprintf(stderr, "Processing: %s\n", path);
    }

    /* Read file */
    size_t content_length;
    char *content = read_file(path, &content_length);
    if (!content) {
        fprintf(stderr, "error: cannot read file '%s': %s\n", path, strerror(errno));
        return SYSML2_ERROR_FILE_READ;
    }

    /* Build line offsets */
    uint32_t line_count;
    uint32_t *line_offsets = build_line_offsets(content, content_length, &line_count);
    if (!line_offsets) {
        free(content);
        return SYSML2_ERROR_OUT_OF_MEMORY;
    }

    /* Create source file */
    Sysml2SourceFile source_file = {
        .path = sysml2_intern(intern, path),
        .content = content,
        .content_length = content_length,
        .line_offsets = line_offsets,
        .line_count = line_count,
    };

    /* Create lexer */
    Sysml2Lexer lexer;
    sysml2_lexer_init(&lexer, &source_file, intern, diag_ctx);

    /* Dump tokens if requested */
    if (options->dump_tokens) {
        Sysml2Token token;
        printf("Tokens for %s:\n", path);
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

        /* Reset lexer for parsing */
        sysml2_lexer_init(&lexer, &source_file, intern, diag_ctx);
    }

    /* Parse file */
    Sysml2Parser parser;
    sysml2_parser_init(&parser, &lexer, arena, diag_ctx);

    Sysml2AstNamespace *ast = sysml2_parser_parse(&parser);

    /* Dump AST if requested */
    if (options->dump_ast && ast) {
        printf("AST for %s:\n", path);
        sysml2_ast_print(ast, 0);
        printf("\n");
    }

    /* Run semantic analysis if no syntax errors */
    if (ast && diag_ctx->error_count == 0) {
        Sysml2SemanticContext sem_ctx;
        sysml2_semantic_init(&sem_ctx, arena, intern, diag_ctx);
        sysml2_semantic_analyze(&sem_ctx, ast);
        sysml2_semantic_destroy(&sem_ctx);
    }

    /* Output JSON if requested */
    if (options->output_format == SYSML2_OUTPUT_JSON && ast) {
        FILE *out = stdout;
        if (options->output_file) {
            out = fopen(options->output_file, "w");
            if (!out) {
                fprintf(stderr, "error: cannot open output file '%s': %s\n",
                    options->output_file, strerror(errno));
                free(content);
                free(line_offsets);
                return SYSML2_ERROR_FILE_NOT_FOUND;
            }
        }

        sysml2_ast_to_json(ast, out);

        if (options->output_file && out != stdout) {
            fclose(out);
        }
    }

    /* Cleanup */
    free(content);
    free(line_offsets);

    return diag_ctx->error_count > 0 ? SYSML2_ERROR_SYNTAX : SYSML2_OK;
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

    if (options.input_file_count == 0) {
        fprintf(stderr, "error: no input files\n\n");
        sysml2_cli_print_help(stderr);
        return 1;
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

    /* Process each file */
    Sysml2Result final_result = SYSML2_OK;
    for (size_t i = 0; i < options.input_file_count; i++) {
        result = process_file(options.input_files[i], &options, &arena, &intern, &diag_ctx);
        if (result != SYSML2_OK) {
            final_result = result;
        }

        if (sysml2_diag_should_stop(&diag_ctx)) {
            break;
        }
    }

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

    return (final_result == SYSML2_OK && diag_ctx.error_count == 0) ? 0 : 1;
}
