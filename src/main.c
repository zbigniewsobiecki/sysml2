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
#include "sysml2/query.h"
#include "sysml2/modify.h"
#include "sysml2/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

/*
 * Helper: Atomic file write using temp file + rename
 *
 * This prevents file truncation if an error occurs during writing.
 * The original file remains intact until the write completes successfully.
 *
 * verbose_msg: NULL to suppress message, or message prefix like "Formatted" or "Modified"
 */
static bool atomic_write_file(
    const char *path,
    SysmlSemanticModel *model,
    const char *verbose_msg
) {
    char tmp_path[PATH_MAX];
    int len = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%d", path, getpid());
    if (len < 0 || (size_t)len >= sizeof(tmp_path)) {
        fprintf(stderr, "error: path too long for temp file: %s\n", path);
        return false;
    }

    /* Write to temp file */
    FILE *out = fopen(tmp_path, "w");
    if (!out) {
        fprintf(stderr, "error: cannot create temp file '%s': %s\n",
                tmp_path, strerror(errno));
        return false;
    }

    sysml2_sysml_write(model, out);

    /* Check for write errors before closing */
    if (ferror(out)) {
        fprintf(stderr, "error: write failed to temp file '%s'\n", tmp_path);
        fclose(out);
        unlink(tmp_path);
        return false;
    }

    if (fclose(out) != 0) {
        fprintf(stderr, "error: failed to close temp file '%s': %s\n",
                tmp_path, strerror(errno));
        unlink(tmp_path);
        return false;
    }

    /* Atomic rename - this preserves original file until this succeeds */
    if (rename(tmp_path, path) != 0) {
        fprintf(stderr, "error: failed to rename temp file '%s' to '%s': %s\n",
                tmp_path, path, strerror(errno));
        unlink(tmp_path);
        return false;
    }

    if (verbose_msg) {
        fprintf(stderr, "%s: %s\n", verbose_msg, path);
    }

    return true;
}

/* Long options for getopt_long */
static const struct option long_options[] = {
    {"output",       required_argument, 0, 'o'},
    {"format",       required_argument, 0, 'f'},
    {"select",       required_argument, 0, 's'},
    {"fix",          no_argument,       0, 'F'},
    {"color",        optional_argument, 0, 'c'},
    {"max-errors",   required_argument, 0, 'm'},
    {"dump-tokens",  no_argument,       0, 'T'},
    {"dump-ast",     no_argument,       0, 'A'},
    {"verbose",      no_argument,       0, 'v'},
    {"parse-only",   no_argument,       0, 'P'},
    {"no-validate",  no_argument,       0, 'P'},  /* alias for --parse-only */
    {"no-resolve",   no_argument,       0, 'R'},
    {"recursive",    no_argument,       0, 'r'},
    {"set",          required_argument, 0, 'S'},
    {"at",           required_argument, 0, 'a'},
    {"delete",       required_argument, 0, 'd'},
    {"create-scope", no_argument,       0, 'C'},
    {"replace-scope", no_argument,      0, 'r' + 256},  /* Long-option only to free -r for --recursive */
    {"force-replace", no_argument,      0, 'R' + 128},  /* Use high value to avoid conflicts */
    {"dry-run",      no_argument,       0, 'D'},
    {"allow-semantic-errors", no_argument, 0, 'e'},
    {"list",         no_argument,       0, 'l'},
    {"help",         no_argument,       0, 'h'},
    {"version",      no_argument,       0, 'V'},
    {0, 0, 0, 0}
};

static const char *short_options = "o:f:s:W:I:S:a:d:hVvTAPFRCDrel";

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

            case 's':
                /* Add select pattern */
                if (options->select_pattern_count >= options->select_pattern_capacity) {
                    size_t new_cap = options->select_pattern_capacity == 0 ? 8 : options->select_pattern_capacity * 2;
                    const char **new_patterns = realloc((void *)options->select_patterns, new_cap * sizeof(char *));
                    if (!new_patterns) {
                        return SYSML2_ERROR_OUT_OF_MEMORY;
                    }
                    options->select_patterns = new_patterns;
                    options->select_pattern_capacity = new_cap;
                }
                options->select_patterns[options->select_pattern_count++] = optarg;
                break;

            case 'R':
                options->no_resolve = true;
                break;

            case 'S':
                /* --set: Add fragment to pending set operation */
                if (options->set_count >= options->set_capacity) {
                    size_t new_cap = options->set_capacity == 0 ? 8 : options->set_capacity * 2;
                    const char **new_fragments = realloc((void *)options->set_fragments, new_cap * sizeof(char *));
                    const char **new_targets = realloc((void *)options->set_targets, new_cap * sizeof(char *));
                    if (!new_fragments || !new_targets) {
                        free(new_fragments);
                        free(new_targets);
                        return SYSML2_ERROR_OUT_OF_MEMORY;
                    }
                    options->set_fragments = new_fragments;
                    options->set_targets = new_targets;
                    options->set_capacity = new_cap;
                }
                options->set_fragments[options->set_count] = optarg;
                options->set_targets[options->set_count] = NULL;  /* Will be set by --at */
                options->set_count++;
                break;

            case 'a':
                /* --at: Set target scope for most recent --set */
                if (options->set_count > 0 && options->set_targets[options->set_count - 1] == NULL) {
                    options->set_targets[options->set_count - 1] = optarg;
                } else {
                    fprintf(stderr, "error: --at must follow --set\n");
                    return SYSML2_ERROR_SYNTAX;
                }
                break;

            case 'd':
                /* --delete: Add delete pattern */
                if (options->delete_pattern_count >= options->delete_pattern_capacity) {
                    size_t new_cap = options->delete_pattern_capacity == 0 ? 8 : options->delete_pattern_capacity * 2;
                    const char **new_patterns = realloc((void *)options->delete_patterns, new_cap * sizeof(char *));
                    if (!new_patterns) {
                        return SYSML2_ERROR_OUT_OF_MEMORY;
                    }
                    options->delete_patterns = new_patterns;
                    options->delete_pattern_capacity = new_cap;
                }
                options->delete_patterns[options->delete_pattern_count++] = optarg;
                break;

            case 'C':
                options->create_scope = true;
                break;

            case 'r':
                options->recursive = true;
                break;

            case 'r' + 256:  /* --replace-scope (long-option only) */
                options->replace_scope = true;
                break;

            case 'R' + 128:  /* --force-replace */
                options->force_replace = true;
                break;

            case 'D':
                options->dry_run = true;
                break;

            case 'e':
                options->allow_semantic_errors = true;
                break;

            case 'l':
                options->list_mode = true;
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
    if (options->select_patterns) {
        free((void *)options->select_patterns);
        options->select_patterns = NULL;
    }
    if (options->set_fragments) {
        free((void *)options->set_fragments);
        options->set_fragments = NULL;
    }
    if (options->set_targets) {
        free((void *)options->set_targets);
        options->set_targets = NULL;
    }
    if (options->delete_patterns) {
        free((void *)options->delete_patterns);
        options->delete_patterns = NULL;
    }
}

void sysml2_cli_print_help(FILE *output) {
    fprintf(output,
        "sysml2 - SysML v2 CLI\n"
        "\n"
        "Usage: sysml2 [options] [file]...\n"
        "\n"
        "If no files are specified, reads from standard input.\n"
        "\n"
        "Options:\n"
        "  -o, --output <file>    Write output to file\n"
        "  -f, --format <fmt>     Output format: json, xml, sysml (default: none)\n"
        "  -s, --select <pattern> Filter output to matching elements (repeatable)\n"
        "  -l, --list             List element names and kinds (discovery mode)\n"
        "  -I <path>              Add library search path for imports\n"
        "  -r, --recursive        Recursively load all .sysml files from directory\n"
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
        "Modification options:\n"
        "  --set <file> --at <scope>  Insert elements from file into scope\n"
        "  --delete <pattern>         Delete elements matching pattern (repeatable)\n"
        "  --create-scope             Create target scope if it doesn't exist\n"
        "  --replace-scope            Clear target scope before inserting (preserves order)\n"
        "  --force-replace            Suppress data loss warning for --replace-scope\n"
        "  --dry-run                  Preview changes without writing files\n"
        "  --allow-semantic-errors    Write files even with semantic errors (E3xxx)\n"
        "                             Parse errors still abort. Exit code 2 signals errors.\n"
        "\n"
        "Query/Delete patterns:\n"
        "  Pkg::Element           Specific element (and children for delete)\n"
        "  Pkg::*                 Direct members only\n"
        "  Pkg::**                All descendants recursively\n"
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
        "  sysml2 --select 'DataModel::*' -f json model.sysml\n"
        "\n"
        "Discovery workflow:\n"
        "  sysml2 --list -r ~/model/           List root elements\n"
        "  sysml2 --list -s 'Pkg::*' model.sysml  List children of Pkg\n"
        "  sysml2 --list -f json model.sysml   JSON summary output\n"
        "\n"
        "Modification examples:\n"
        "  sysml2 --delete 'Pkg::OldElement' model.sysml\n"
        "  sysml2 --set fragment.sysml --at 'Pkg' model.sysml\n"
        "  echo 'part def Car;' | sysml2 --set - --at 'Vehicles' model.sysml\n"
        "  sysml2 --delete 'Legacy::**' --dry-run model.sysml\n"
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

/*
 * Expand input files for --recursive mode
 *
 * When --recursive is set, expands directories to all .sysml files within them.
 * Regular files are passed through unchanged.
 *
 * Returns dynamically allocated array (caller must free with sysml2_free_file_list).
 * Returns NULL on error.
 */
static const char **expand_input_files(
    const Sysml2CliOptions *options,
    size_t *out_count
) {
    if (!options->recursive) {
        /* No expansion needed - return copy of original array */
        const char **copy = malloc(options->input_file_count * sizeof(char *));
        if (!copy) {
            return NULL;
        }
        for (size_t i = 0; i < options->input_file_count; i++) {
            copy[i] = strdup(options->input_files[i]);
            if (!copy[i]) {
                /* Free already allocated strings */
                for (size_t j = 0; j < i; j++) {
                    free((void *)copy[j]);
                }
                free(copy);
                return NULL;
            }
            /* Normalize to canonical path once */
            char *resolved = sysml2_get_realpath(copy[i]);
            if (resolved) {
                free((void *)copy[i]);
                copy[i] = resolved;
            }
        }
        *out_count = options->input_file_count;
        return copy;
    }

    /* --recursive mode: expand directories */
    size_t capacity = 16;
    size_t count = 0;
    const char **result = malloc(capacity * sizeof(char *));
    if (!result) {
        return NULL;
    }

    for (size_t i = 0; i < options->input_file_count; i++) {
        const char *path = options->input_files[i];

        if (sysml2_is_directory(path)) {
            /* Expand directory recursively */
            size_t found_count = 0;
            char **found_files = sysml2_find_files_recursive(path, ".sysml", &found_count);

            if (!found_files) {
                /* Directory not accessible - warn and continue */
                fprintf(stderr, "warning: cannot access directory '%s'\n", path);
                continue;
            }

            if (found_count == 0) {
                sysml2_free_file_list(found_files, found_count);
                continue;
            }

            /* Add found files to result */
            for (size_t j = 0; j < found_count; j++) {
                if (count >= capacity) {
                    capacity *= 2;
                    const char **new_result = realloc(result, capacity * sizeof(char *));
                    if (!new_result) {
                        sysml2_free_file_list(found_files, found_count);
                        sysml2_free_file_list((char **)result, count);
                        return NULL;
                    }
                    result = new_result;
                }
                result[count] = found_files[j];
                found_files[j] = NULL;  /* Transfer ownership */
                /* Normalize to canonical path once */
                char *resolved = sysml2_get_realpath(result[count]);
                if (resolved) {
                    free((void *)result[count]);
                    result[count] = resolved;
                }
                count++;
            }
            free(found_files);  /* Free array only, not strings */
        } else if (sysml2_is_file(path)) {
            /* Regular file - add as-is */
            if (count >= capacity) {
                capacity *= 2;
                const char **new_result = realloc(result, capacity * sizeof(char *));
                if (!new_result) {
                    sysml2_free_file_list((char **)result, count);
                    return NULL;
                }
                result = new_result;
            }
            result[count] = strdup(path);
            if (!result[count]) {
                sysml2_free_file_list((char **)result, count);
                return NULL;
            }
            /* Normalize to canonical path once */
            char *resolved = sysml2_get_realpath(result[count]);
            if (resolved) {
                free((void *)result[count]);
                result[count] = resolved;
            }
            count++;
        } else {
            fprintf(stderr, "warning: '%s' is not a file or directory\n", path);
        }
    }

    *out_count = count;
    return result;
}

/*
 * Write element list summary for --list mode.
 *
 * JSON format: [{"id": "...", "name": "...", "kind": "..."}]
 * Text format: id\tkind\n (tab-separated, one per line)
 */
static void write_element_list(
    SysmlNode **elements, size_t count,
    Sysml2OutputFormat format, FILE *out
) {
    if (format == SYSML2_OUTPUT_JSON) {
        fprintf(out, "[");
        bool first = true;
        for (size_t i = 0; i < count; i++) {
            SysmlNode *node = elements[i];
            if (!node) continue;
            if (!first) fprintf(out, ",");
            first = false;
            const char *id = node->id ? node->id : "";
            const char *name = node->name ? node->name : "";
            const char *kind = sysml2_kind_to_keyword(node->kind);
            /* Minimal JSON escaping - names/ids don't contain special chars */
            fprintf(out, "\n  {\"id\": \"%s\", \"name\": \"%s\", \"kind\": \"%s\"}",
                    id, name, kind);
        }
        if (!first) fprintf(out, "\n");
        fprintf(out, "]\n");
    } else {
        for (size_t i = 0; i < count; i++) {
            SysmlNode *node = elements[i];
            if (!node) continue;
            const char *id = node->id ? node->id : "";
            const char *kind = sysml2_kind_to_keyword(node->kind);
            fprintf(out, "%s\t%s\n", id, kind);
        }
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

    /* Expand input files (handles --recursive) */
    size_t input_count = 0;
    const char **input_files = expand_input_files(options, &input_count);
    if (!input_files && options->input_file_count > 0) {
        fprintf(stderr, "error: out of memory expanding input files\n");
        return 1;
    }

    if (input_count == 0) {
        fprintf(stderr, "error: no .sysml files found\n");
        sysml2_free_file_list((char **)input_files, input_count);
        return 1;
    }

    /* Preload from configured library paths */
    if (!options->no_resolve) {
        sysml2_resolver_preload_libraries(resolver, diag);
    }

    /* Add directories containing input files to search paths */
    if (options->recursive) {
        for (size_t i = 0; i < options->input_file_count; i++) {
            if (sysml2_is_directory(options->input_files[i])) {
                sysml2_resolver_add_path(resolver, options->input_files[i]);
            } else {
                add_file_directory_to_resolver(resolver, options->input_files[i]);
            }
        }
    } else {
        for (size_t i = 0; i < input_count; i++) {
            add_file_directory_to_resolver(resolver, input_files[i]);
        }
    }

    /* Discover packages in input file directories.
     * Skip in recursive mode: Pass 1 parses every file and
     * sysml2_resolver_cache_model already calls register_package_file. */
    if (!options->no_resolve && !options->recursive) {
        for (size_t i = 0; i < input_count; i++) {
            char *path_copy = strdup(input_files[i]);
            if (path_copy) {
                char *last_slash = strrchr(path_copy, '/');
                if (last_slash) {
                    *last_slash = '\0';
                    sysml2_resolver_discover_packages(resolver, path_copy, diag);
                } else {
                    sysml2_resolver_discover_packages(resolver, ".", diag);
                }
                free(path_copy);
            }
        }
        /* Clear any parse errors from discovery - they shouldn't affect exit code */
        sysml2_diag_clear(diag);
    }

    /* Allocate model array */
    SysmlSemanticModel **models = malloc(input_count * sizeof(SysmlSemanticModel *));
    if (!models) {
        fprintf(stderr, "error: out of memory\n");
        sysml2_free_file_list((char **)input_files, input_count);
        return 1;
    }

    bool has_errors = false;
    size_t error_count_before = diag->error_count;

    /* Pass 1: Parse ALL input files */
    for (size_t i = 0; i < input_count; i++) {
        models[i] = NULL;
        Sysml2Result result = sysml2_pipeline_process_file(ctx, input_files[i], &models[i]);
        if (result != SYSML2_OK || models[i] == NULL) {
            has_errors = true;
        }
        if (models[i]) {
            sysml2_resolver_cache_model(resolver, input_files[i], models[i]);
        }
    }

    if (has_errors) {
        sysml2_pipeline_print_diagnostics(ctx, stderr);
        fprintf(stderr, "error: --fix aborted due to parse errors (no files modified)\n");
        free(models);
        sysml2_free_file_list((char **)input_files, input_count);
        return 1;
    }

    /* Pass 2: Resolve imports (unless --no-resolve) */
    if (!options->no_resolve) {
        for (size_t i = 0; i < input_count; i++) {
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
            sysml2_free_file_list((char **)input_files, input_count);
            return sysml2_diag_has_parse_errors(diag) ? 1 : 2;
        }
    }

    /* Pass 3: Unified validation (unless --parse-only) */
    if (!options->parse_only) {
        Sysml2Result val_result = sysml2_pipeline_validate_all(ctx);
        if (val_result != SYSML2_OK || diag->error_count > error_count_before) {
            sysml2_pipeline_print_diagnostics(ctx, stderr);
            fprintf(stderr, "error: --fix aborted due to validation errors (no files modified)\n");
            free(models);
            sysml2_free_file_list((char **)input_files, input_count);
            return sysml2_diag_has_parse_errors(diag) ? 1 : 2;
        }
    }

    /* Pass 4: All checks passed, now rewrite input files using atomic writes */
    for (size_t i = 0; i < input_count; i++) {
        if (models[i]) {
            if (!atomic_write_file(input_files[i], models[i],
                                   options->verbose ? "Formatted" : NULL)) {
                has_errors = true;
            }
        }
    }

    free(models);
    sysml2_free_file_list((char **)input_files, input_count);

    /* Exit codes: 0 = success, 1 = parse error, 2 = semantic error */
    if (diag->error_count == 0 && !has_errors) {
        return 0;
    } else if (sysml2_diag_has_parse_errors(diag)) {
        return 1;
    } else {
        return 2;
    }
}

/* Run normal mode: parse, resolve, validate, output */
static int run_normal_mode(
    Sysml2PipelineContext *ctx,
    const Sysml2CliOptions *options
) {
    Sysml2ImportResolver *resolver = sysml2_pipeline_get_resolver(ctx);
    Sysml2DiagContext *diag = sysml2_pipeline_get_diag(ctx);

    /* Expand input files (handles --recursive) */
    size_t input_count = 0;
    const char **input_files = NULL;
    if (options->input_file_count > 0) {
        input_files = expand_input_files(options, &input_count);
        if (!input_files) {
            fprintf(stderr, "error: out of memory expanding input files\n");
            return 1;
        }
        if (input_count == 0 && options->recursive) {
            fprintf(stderr, "error: no .sysml files found\n");
            sysml2_free_file_list((char **)input_files, input_count);
            return 1;
        }
    }

    /* Preload from configured library paths (-I and SYSML2_LIBRARY_PATH).
     * These files are fully cached for validation. */
    if (!options->no_resolve) {
        sysml2_resolver_preload_libraries(resolver, diag);
    }

    /* Add directories containing input files to search paths */
    if (options->recursive) {
        for (size_t i = 0; i < options->input_file_count; i++) {
            if (sysml2_is_directory(options->input_files[i])) {
                sysml2_resolver_add_path(resolver, options->input_files[i]);
            } else {
                add_file_directory_to_resolver(resolver, options->input_files[i]);
            }
        }
    } else {
        for (size_t i = 0; i < input_count; i++) {
            add_file_directory_to_resolver(resolver, input_files[i]);
        }
    }

    /* Discover packages in input file directories.
     * This builds the package map without caching files for validation,
     * allowing imports to find packages even when filename != package name.
     *
     * Skip in recursive mode: Pass 1 parses every file and
     * sysml2_resolver_cache_model already calls register_package_file.
     *
     * We clear diagnostics after discovery because errors in sibling files
     * shouldn't affect the exit code - only errors in the actual input files
     * and their imports matter. */
    if (!options->no_resolve && !options->recursive) {
        for (size_t i = 0; i < input_count; i++) {
            char *path_copy = strdup(input_files[i]);
            if (path_copy) {
                char *last_slash = strrchr(path_copy, '/');
                if (last_slash) {
                    *last_slash = '\0';
                    sysml2_resolver_discover_packages(resolver, path_copy, diag);
                } else {
                    sysml2_resolver_discover_packages(resolver, ".", diag);
                }
                free(path_copy);
            }
        }
        /* Clear any parse errors from discovery - they shouldn't affect exit code */
        sysml2_diag_clear(diag);
    }

    Sysml2Result final_result = SYSML2_OK;

    if (input_count == 0) {
        /* Read from stdin - single file mode (with import resolution) */
        SysmlSemanticModel *model = NULL;
        Sysml2Result result = sysml2_pipeline_process_stdin(ctx, &model);
        if (result != SYSML2_OK) {
            final_result = result;
        }

        /* Resolve imports for stdin input (uses -I library paths) */
        if (model && !options->no_resolve) {
            sysml2_resolver_resolve_imports(resolver, model, diag);
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
            if (options->list_mode) {
                /* --list mode: collect root elements and output summary */
                size_t root_count = 0;
                for (size_t i = 0; i < model->element_count; i++) {
                    if (model->elements[i] && model->elements[i]->parent_id == NULL)
                        root_count++;
                }
                SysmlNode **roots = malloc(root_count * sizeof(SysmlNode *));
                if (roots) {
                    size_t idx = 0;
                    for (size_t i = 0; i < model->element_count; i++) {
                        if (model->elements[i] && model->elements[i]->parent_id == NULL)
                            roots[idx++] = model->elements[i];
                    }
                    FILE *out = options->output_file ? fopen(options->output_file, "w") : stdout;
                    if (out) {
                        write_element_list(roots, root_count, options->output_format, out);
                        if (options->output_file) fclose(out);
                    }
                    free(roots);
                }
            } else if (options->output_format == SYSML2_OUTPUT_JSON) {
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
        SysmlSemanticModel **input_models = malloc(input_count * sizeof(SysmlSemanticModel *));
        if (!input_models) {
            fprintf(stderr, "error: out of memory\n");
            sysml2_free_file_list((char **)input_files, input_count);
            return 1;
        }

        bool has_parse_errors = false;

        /* Pass 1: Parse all input files */
        for (size_t i = 0; i < input_count; i++) {
            input_models[i] = NULL;
            Sysml2Result result = sysml2_pipeline_process_file(ctx, input_files[i], &input_models[i]);
            if (result != SYSML2_OK) {
                has_parse_errors = true;
            }
            if (input_models[i]) {
                sysml2_resolver_cache_model(resolver, input_files[i], input_models[i]);
            }
            if (sysml2_diag_should_stop(diag)) {
                break;
            }
        }

        /* Pass 2: Resolve imports */
        if (!options->no_resolve && !has_parse_errors) {
            for (size_t i = 0; i < input_count; i++) {
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

        /* Output */
        if (!has_parse_errors && input_models[0]) {
            if (options->list_mode && options->select_pattern_count > 0) {
                /* --list with --select: query then output summary */
                Sysml2Arena *arena = sysml2_pipeline_get_arena(ctx);
                Sysml2QueryPattern *patterns = sysml2_query_parse_multi(
                    options->select_patterns,
                    options->select_pattern_count,
                    arena
                );
                if (patterns) {
                    Sysml2QueryResult *query_result = sysml2_query_execute(
                        patterns, input_models, input_count, arena
                    );
                    if (query_result) {
                        FILE *out = options->output_file ? fopen(options->output_file, "w") : stdout;
                        if (out) {
                            write_element_list(query_result->elements, query_result->element_count,
                                               options->output_format, out);
                            if (options->output_file) fclose(out);
                        }
                    }
                }
            } else if (options->list_mode) {
                /* --list without --select: collect root elements across all input models */
                size_t root_count = 0;
                for (size_t m = 0; m < input_count; m++) {
                    if (!input_models[m]) continue;
                    for (size_t i = 0; i < input_models[m]->element_count; i++) {
                        if (input_models[m]->elements[i] &&
                            input_models[m]->elements[i]->parent_id == NULL)
                            root_count++;
                    }
                }
                SysmlNode **roots = malloc(root_count * sizeof(SysmlNode *));
                if (roots) {
                    size_t idx = 0;
                    for (size_t m = 0; m < input_count; m++) {
                        if (!input_models[m]) continue;
                        for (size_t i = 0; i < input_models[m]->element_count; i++) {
                            if (input_models[m]->elements[i] &&
                                input_models[m]->elements[i]->parent_id == NULL)
                                roots[idx++] = input_models[m]->elements[i];
                        }
                    }
                    FILE *out = options->output_file ? fopen(options->output_file, "w") : stdout;
                    if (out) {
                        write_element_list(roots, root_count, options->output_format, out);
                        if (options->output_file) fclose(out);
                    }
                    free(roots);
                }
            } else if (options->select_pattern_count > 0) {
                /* Query mode: filter output using patterns */
                Sysml2Arena *arena = sysml2_pipeline_get_arena(ctx);
                Sysml2QueryPattern *patterns = sysml2_query_parse_multi(
                    options->select_patterns,
                    options->select_pattern_count,
                    arena
                );

                if (patterns) {
                    Sysml2QueryResult *query_result = sysml2_query_execute(
                        patterns,
                        input_models,
                        input_count,
                        arena
                    );

                    if (query_result) {
                        FILE *out = options->output_file ? fopen(options->output_file, "w") : stdout;
                        if (out) {
                            if (options->output_format == SYSML2_OUTPUT_JSON) {
                                sysml2_pipeline_write_query_json(ctx, query_result, out);
                            } else if (options->output_format == SYSML2_OUTPUT_SYSML) {
                                sysml2_pipeline_write_query_sysml(ctx, query_result, input_models, input_count, out);
                            }
                            if (options->output_file) fclose(out);
                        }
                    }
                }
            } else {
                /* Normal mode: output all resolved models */
                size_t all_model_count;
                SysmlSemanticModel **all_models = sysml2_resolver_get_all_models(ctx->resolver, &all_model_count);
                if (all_models && all_model_count > 0) {
                    FILE *out = options->output_file ? fopen(options->output_file, "w") : stdout;
                    if (out) {
                        for (size_t i = 0; i < all_model_count; i++) {
                            if (all_models[i]) {
                                if (options->output_format == SYSML2_OUTPUT_JSON) {
                                    sysml2_pipeline_write_json(ctx, all_models[i], out);
                                } else if (options->output_format == SYSML2_OUTPUT_SYSML) {
                                    sysml2_pipeline_write_sysml(ctx, all_models[i], out);
                                }
                            }
                        }
                        if (options->output_file) fclose(out);
                    }
                    free(all_models);
                }
            }
        }

        free(input_models);
    }

    /* Cleanup expanded file list */
    if (input_files) {
        sysml2_free_file_list((char **)input_files, input_count);
    }

    /* Print diagnostics */
    sysml2_pipeline_print_diagnostics(ctx, stderr);

    /* Exit codes: 0 = success, 1 = parse error, 2 = semantic error */
    if (diag->error_count == 0) {
        return 0;
    } else if (sysml2_diag_has_parse_errors(diag)) {
        return 1;
    } else {
        return 2;
    }
}

/* Check if modification mode is requested */
static bool has_modify_options(const Sysml2CliOptions *options) {
    return options->delete_pattern_count > 0 || options->set_count > 0;
}

/* Run modification mode: parse, delete, set, validate, write */
static int run_modify_mode(
    Sysml2PipelineContext *ctx,
    const Sysml2CliOptions *options
) {
    Sysml2ImportResolver *resolver = sysml2_pipeline_get_resolver(ctx);
    Sysml2DiagContext *diag = sysml2_pipeline_get_diag(ctx);
    Sysml2Arena *arena = sysml2_pipeline_get_arena(ctx);
    Sysml2Intern *intern = ctx->intern;

    /* Expand input files (handles --recursive) */
    size_t input_count = 0;
    const char **input_files = expand_input_files(options, &input_count);
    if (!input_files && options->input_file_count > 0) {
        fprintf(stderr, "error: out of memory expanding input files\n");
        return 1;
    }

    if (input_count == 0) {
        fprintf(stderr, "error: no .sysml files found\n");
        sysml2_free_file_list((char **)input_files, input_count);
        return 1;
    }

    /* Preload from configured library paths */
    if (!options->no_resolve) {
        sysml2_resolver_preload_libraries(resolver, diag);
    }

    /* Add directories containing input files to search paths */
    if (options->recursive) {
        for (size_t i = 0; i < options->input_file_count; i++) {
            if (sysml2_is_directory(options->input_files[i])) {
                sysml2_resolver_add_path(resolver, options->input_files[i]);
            } else {
                add_file_directory_to_resolver(resolver, options->input_files[i]);
            }
        }
    } else {
        for (size_t i = 0; i < input_count; i++) {
            add_file_directory_to_resolver(resolver, input_files[i]);
        }
    }

    /* Discover packages in input file directories.
     * Skip in recursive mode: Pass 1 parses every file and
     * sysml2_resolver_cache_model already calls register_package_file. */
    if (!options->no_resolve && !options->recursive) {
        for (size_t i = 0; i < input_count; i++) {
            char *path_copy = strdup(input_files[i]);
            if (path_copy) {
                char *last_slash = strrchr(path_copy, '/');
                if (last_slash) {
                    *last_slash = '\0';
                    sysml2_resolver_discover_packages(resolver, path_copy, diag);
                } else {
                    sysml2_resolver_discover_packages(resolver, ".", diag);
                }
                free(path_copy);
            }
        }
        /* Clear any parse errors from discovery - they shouldn't affect exit code */
        sysml2_diag_clear(diag);
    }

    /* Allocate model array */
    SysmlSemanticModel **models = malloc(input_count * sizeof(SysmlSemanticModel *));
    SysmlSemanticModel **modified_models = malloc(input_count * sizeof(SysmlSemanticModel *));
    if (!models || !modified_models) {
        fprintf(stderr, "error: out of memory\n");
        free(models);
        free(modified_models);
        sysml2_free_file_list((char **)input_files, input_count);
        return 1;
    }

    bool has_errors = false;
    size_t error_count_before = diag->error_count;

    /* Pass 1: Parse ALL input files */
    for (size_t i = 0; i < input_count; i++) {
        models[i] = NULL;
        modified_models[i] = NULL;
        Sysml2Result result = sysml2_pipeline_process_file(ctx, input_files[i], &models[i]);
        if (result != SYSML2_OK || models[i] == NULL) {
            has_errors = true;
        }
        if (models[i]) {
            sysml2_resolver_cache_model(resolver, input_files[i], models[i]);
        }
    }

    if (has_errors) {
        sysml2_pipeline_print_diagnostics(ctx, stderr);
        fprintf(stderr, "error: modification aborted due to parse errors (no files modified)\n");
        free(models);
        free(modified_models);
        sysml2_free_file_list((char **)input_files, input_count);
        return 1;
    }

    /* Resolve imports for all input files (uses -I library paths) */
    if (!options->no_resolve) {
        for (size_t i = 0; i < input_count; i++) {
            if (models[i]) {
                sysml2_resolver_resolve_imports(resolver, models[i], diag);
            }
        }
    }

    /* Pass 2: Build modification plan and apply deletions */
    Sysml2ModifyPlan *plan = sysml2_modify_plan_create(arena);
    if (!plan) {
        fprintf(stderr, "error: out of memory creating modification plan\n");
        free(models);
        free(modified_models);
        sysml2_free_file_list((char **)input_files, input_count);
        return 1;
    }
    plan->dry_run = options->dry_run;

    /* Add delete patterns */
    for (size_t i = 0; i < options->delete_pattern_count; i++) {
        Sysml2Result add_result = sysml2_modify_plan_add_delete(plan, options->delete_patterns[i]);
        if (add_result != SYSML2_OK) {
            fprintf(stderr, "error: invalid delete pattern '%s'\n", options->delete_patterns[i]);
            free(models);
            free(modified_models);
            sysml2_free_file_list((char **)input_files, input_count);
            return 1;
        }
    }

    /* Apply deletions to all models */
    size_t total_deleted = 0;
    for (size_t i = 0; i < input_count; i++) {
        if (!models[i]) continue;

        if (plan->delete_patterns) {
            size_t deleted_count = 0;
            modified_models[i] = sysml2_modify_clone_with_deletions(
                models[i],
                plan->delete_patterns,
                arena,
                intern,
                &deleted_count
            );
            total_deleted += deleted_count;

            if (!modified_models[i]) {
                fprintf(stderr, "error: failed to apply deletions to '%s'\n", input_files[i]);
                free(models);
                free(modified_models);
                sysml2_free_file_list((char **)input_files, input_count);
                return 1;
            }
        } else {
            /* No deletions, just copy reference */
            modified_models[i] = models[i];
        }
    }

    /* Pass 3: Apply set operations */
    size_t total_added = 0;
    size_t total_replaced = 0;

    for (size_t op_idx = 0; op_idx < options->set_count; op_idx++) {
        const char *fragment_path = options->set_fragments[op_idx];
        const char *target_scope = options->set_targets[op_idx];

        if (!target_scope) {
            fprintf(stderr, "error: --set '%s' missing --at target scope\n", fragment_path);
            free(models);
            free(modified_models);
            sysml2_free_file_list((char **)input_files, input_count);
            return 1;
        }

        /* Parse the fragment */
        SysmlSemanticModel *fragment = NULL;

        if (strcmp(fragment_path, "-") == 0) {
            /* Read from stdin */
            Sysml2Result frag_result = sysml2_pipeline_process_stdin(ctx, &fragment);
            if (frag_result != SYSML2_OK || !fragment) {
                fprintf(stderr, "error: failed to parse fragment from stdin\n");
                free(models);
                free(modified_models);
                sysml2_free_file_list((char **)input_files, input_count);
                return 1;
            }
        } else {
            /* Read from file */
            Sysml2Result frag_result = sysml2_pipeline_process_file(ctx, fragment_path, &fragment);
            if (frag_result != SYSML2_OK || !fragment) {
                fprintf(stderr, "error: failed to parse fragment file '%s'\n", fragment_path);
                free(models);
                free(modified_models);
                sysml2_free_file_list((char **)input_files, input_count);
                return 1;
            }
        }

        /* Find which model contains the target scope (or first model if creating) */
        int target_model_idx = -1;
        for (size_t i = 0; i < input_count; i++) {
            if (modified_models[i] && sysml2_modify_scope_exists(modified_models[i], target_scope)) {
                target_model_idx = (int)i;
                break;
            }
        }

        /* If not found and create_scope is set, use first model */
        if (target_model_idx < 0) {
            if (options->create_scope && input_count > 0) {
                target_model_idx = 0;
            } else {
                fprintf(stderr, "error: target scope '%s' not found\n", target_scope);

                /* List available scopes and suggest similar names */
                const char **all_scopes = NULL;
                size_t scope_count = 0;
                if (sysml2_modify_list_scopes_multi(modified_models, input_count, arena, &all_scopes, &scope_count)) {
                    /* Find similar scope names */
                    const char **suggestions = NULL;
                    size_t suggestion_count = 0;
                    if (scope_count > 0 && sysml2_modify_find_similar_scopes(
                            target_scope, all_scopes, scope_count, arena,
                            &suggestions, &suggestion_count, 3)) {
                        if (suggestion_count > 0) {
                            fprintf(stderr, "  did you mean: %s", suggestions[0]);
                            for (size_t i = 1; i < suggestion_count; i++) {
                                fprintf(stderr, ", %s", suggestions[i]);
                            }
                            fprintf(stderr, "?\n");
                        }
                    }

                    /* Show available scopes (limited to first 10) */
                    if (scope_count > 0) {
                        fprintf(stderr, "  available scopes: ");
                        size_t show_count = scope_count < 10 ? scope_count : 10;
                        for (size_t i = 0; i < show_count; i++) {
                            if (i > 0) fprintf(stderr, ", ");
                            fprintf(stderr, "%s", all_scopes[i]);
                        }
                        if (scope_count > 10) {
                            fprintf(stderr, ", ... (%zu more)", scope_count - 10);
                        }
                        fprintf(stderr, "\n");
                    }
                }

                fprintf(stderr, "  hint: use --create-scope to create it\n");
                free(models);
                free(modified_models);
                sysml2_free_file_list((char **)input_files, input_count);
                return 1;
            }
        }

        /* Data loss safeguard for --replace-scope */
        if (options->replace_scope && !options->force_replace && target_model_idx >= 0) {
            /* Count direct children of target scope in base model */
            size_t scope_child_count = 0;
            SysmlSemanticModel *base_model = modified_models[target_model_idx];
            for (size_t i = 0; i < base_model->element_count; i++) {
                SysmlNode *node = base_model->elements[i];
                if (node && node->parent_id && strcmp(node->parent_id, target_scope) == 0) {
                    scope_child_count++;
                }
            }

            /* Count elements in fragment (excluding NULL entries from unwrapping) */
            size_t fragment_elem_count = 0;
            for (size_t i = 0; i < fragment->element_count; i++) {
                if (fragment->elements[i]) fragment_elem_count++;
            }

            /* Warn if fragment has significantly fewer elements than scope */
            if (scope_child_count > 0 && fragment_elem_count < scope_child_count / 2) {
                fprintf(stderr, "warning: --replace-scope will delete %zu elements but fragment only has %zu.\n",
                        scope_child_count, fragment_elem_count);
                fprintf(stderr, "  This may cause DATA LOSS. Use --force-replace to suppress this warning.\n");
                fprintf(stderr, "  Aborting modification (no files modified).\n");
                free(models);
                free(modified_models);
                sysml2_free_file_list((char **)input_files, input_count);
                return 1;
            }
        }

        /* Merge fragment into target model */
        size_t added_count = 0;
        size_t replaced_count = 0;
        SysmlSemanticModel *merged = sysml2_modify_merge_fragment(
            modified_models[target_model_idx],
            fragment,
            target_scope,
            options->create_scope,
            options->replace_scope,
            arena,
            intern,
            &added_count,
            &replaced_count
        );

        if (!merged) {
            fprintf(stderr, "error: failed to merge fragment into scope '%s'\n", target_scope);
            free(models);
            free(modified_models);
            sysml2_free_file_list((char **)input_files, input_count);
            return 1;
        }

        modified_models[target_model_idx] = merged;
        total_added += added_count;
        total_replaced += replaced_count;
    }

    /* Pass 4: Validation (unless --parse-only) */
    if (!options->parse_only) {
        /* Update resolver cache with modified models */
        for (size_t i = 0; i < input_count; i++) {
            if (modified_models[i]) {
                sysml2_resolver_cache_model(resolver, input_files[i], modified_models[i]);
            }
        }

        Sysml2Result val_result = sysml2_pipeline_validate_all(ctx);
        if (val_result != SYSML2_OK || diag->error_count > error_count_before) {
            sysml2_pipeline_print_diagnostics(ctx, stderr);

            /* Check if we have parse errors (always fatal) or only semantic errors */
            bool has_parse_errors = sysml2_diag_has_parse_errors(diag);

            /* Abort on parse errors, or semantic errors unless --allow-semantic-errors */
            if (has_parse_errors || !options->allow_semantic_errors) {
                fprintf(stderr, "error: modification aborted due to validation errors (no files modified)\n");
                free(models);
                free(modified_models);
                sysml2_free_file_list((char **)input_files, input_count);
                return has_parse_errors ? 1 : 2;
            }

            /* --allow-semantic-errors: continue despite semantic errors */
            fprintf(stderr, "warning: continuing with %zu semantic error(s) (--allow-semantic-errors)\n",
                    diag->error_count - error_count_before);
        }
    }

    /* Summary */
    if (options->verbose || options->dry_run) {
        fprintf(stderr, "Modification summary:\n");
        fprintf(stderr, "  Elements deleted:  %zu\n", total_deleted);
        fprintf(stderr, "  Elements added:    %zu\n", total_added);
        fprintf(stderr, "  Elements replaced: %zu\n", total_replaced);
    }

    /* JSON output for modification results when -f json */
    if (options->output_format == SYSML2_OUTPUT_JSON) {
        printf("{\"added\":%zu,\"replaced\":%zu,\"deleted\":%zu}\n",
               total_added, total_replaced, total_deleted);
    }

    /* Pass 5: Write modified files (unless dry-run) */
    if (!options->dry_run) {
        for (size_t i = 0; i < input_count; i++) {
            if (!modified_models[i]) continue;

            /* Check if model was actually modified */
            bool was_modified = (modified_models[i] != models[i]);
            if (!was_modified && plan->delete_patterns) {
                /* Check if any deletions affected this model */
                size_t check_deleted = 0;
                sysml2_modify_clone_with_deletions(models[i], plan->delete_patterns, arena, intern, &check_deleted);
                was_modified = (check_deleted > 0);
            }

            if (was_modified || options->set_count > 0) {
                if (!atomic_write_file(input_files[i], modified_models[i],
                                       options->verbose ? "Modified" : NULL)) {
                    has_errors = true;
                }
            }
        }
    } else {
        fprintf(stderr, "Dry run: no files modified\n");
    }

    free(models);
    free(modified_models);
    sysml2_free_file_list((char **)input_files, input_count);

    /* Exit codes: 0 = success, 1 = parse/operational error, 2 = semantic error */
    if (diag->error_count == 0 && !has_errors) {
        return 0;
    } else if (has_errors || sysml2_diag_has_parse_errors(diag)) {
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

    /* Modification modes require file arguments */
    if (has_modify_options(&options) && options.input_file_count == 0) {
        fprintf(stderr, "error: --set/--delete require file arguments\n");
        return 1;
    }

    /* Validate --list is not combined with modification flags */
    if (options.list_mode) {
        if (options.fix_in_place) {
            fprintf(stderr, "error: --list cannot be combined with --fix\n");
            return 1;
        }
        if (options.set_count > 0) {
            fprintf(stderr, "error: --list cannot be combined with --set\n");
            return 1;
        }
        if (options.delete_pattern_count > 0) {
            fprintf(stderr, "error: --list cannot be combined with --delete\n");
            return 1;
        }
    }

    /* Validate --set has corresponding --at */
    for (size_t i = 0; i < options.set_count; i++) {
        if (options.set_targets[i] == NULL) {
            fprintf(stderr, "error: --set '%s' missing --at target scope\n", options.set_fragments[i]);
            return 1;
        }
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
    if (has_modify_options(&options)) {
        exit_code = run_modify_mode(ctx, &options);
    } else if (options.fix_in_place) {
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
