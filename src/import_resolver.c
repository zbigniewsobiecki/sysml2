/*
 * SysML v2 Parser - Import Resolver Implementation
 *
 * Automatic resolution of import statements by finding and parsing
 * imported files from configured library paths.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/import_resolver.h"
#include "sysml2/ast_builder.h"
#include "sysml2/utils.h"
#include "sysml_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

/* Initial capacities */
#define RESOLVER_INITIAL_PATH_CAPACITY 8
#define RESOLVER_INITIAL_STACK_CAPACITY 16

/* Environment variable name */
#define SYSML2_LIBRARY_PATH_ENV "SYSML2_LIBRARY_PATH"

/* File I/O and path utilities are now in sysml2/utils.h */

/* Helper: Extract package name from import target
 * "ScalarValues::Real" -> "ScalarValues"
 * "ISQ" -> "ISQ"
 */
static char *extract_package_name(const char *import_target) {
    const char *sep = strstr(import_target, "::");
    size_t len = sep ? (size_t)(sep - import_target) : strlen(import_target);

    char *result = malloc(len + 1);
    if (!result) return NULL;

    memcpy(result, import_target, len);
    result[len] = '\0';
    return result;
}

/* Forward declaration for recursive search */
static char *search_directory_recursive(const char *dir, const char *filename, int max_depth);

/* Search a directory for a file, optionally recursive */
static char *search_directory_recursive(const char *dir, const char *filename, int max_depth) {
    if (max_depth <= 0) return NULL;

    DIR *d = opendir(dir);
    if (!d) return NULL;

    struct dirent *entry;
    char *result = NULL;

    while ((entry = readdir(d)) != NULL && !result) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char *full_path = sysml2_path_join(dir, entry->d_name);
        if (!full_path) continue;

        if (strcmp(entry->d_name, filename) == 0 && sysml2_is_file(full_path)) {
            result = full_path;
        } else if (sysml2_is_directory(full_path)) {
            /* Recurse into subdirectory */
            result = search_directory_recursive(full_path, filename, max_depth - 1);
            free(full_path);
        } else {
            free(full_path);
        }
    }

    closedir(d);
    return result;
}

SysmlImportResolver *sysml_resolver_create(
    Sysml2Arena *arena,
    Sysml2Intern *intern
) {
    SysmlImportResolver *resolver = calloc(1, sizeof(SysmlImportResolver));
    if (!resolver) return NULL;

    resolver->arena = arena;
    resolver->intern = intern;

    resolver->library_paths = calloc(RESOLVER_INITIAL_PATH_CAPACITY, sizeof(char *));
    if (!resolver->library_paths) {
        free(resolver);
        return NULL;
    }
    resolver->path_capacity = RESOLVER_INITIAL_PATH_CAPACITY;

    resolver->resolution_stack = calloc(RESOLVER_INITIAL_STACK_CAPACITY, sizeof(char *));
    if (!resolver->resolution_stack) {
        free(resolver->library_paths);
        free(resolver);
        return NULL;
    }
    resolver->stack_capacity = RESOLVER_INITIAL_STACK_CAPACITY;

    resolver->cache = NULL;
    resolver->verbose = false;
    resolver->disabled = false;
    resolver->strict_imports = false;

    return resolver;
}

void sysml_resolver_destroy(SysmlImportResolver *resolver) {
    if (!resolver) return;

    /* Free library paths */
    for (size_t i = 0; i < resolver->path_count; i++) {
        free(resolver->library_paths[i]);
    }
    free(resolver->library_paths);

    /* Free resolution stack */
    for (size_t i = 0; i < resolver->stack_depth; i++) {
        free(resolver->resolution_stack[i]);
    }
    free(resolver->resolution_stack);

    /* Free file cache (models are owned by arena) */
    SysmlFileCache *cache = resolver->cache;
    while (cache) {
        SysmlFileCache *next = cache->next;
        free(cache->path);
        free(cache);
        cache = next;
    }

    free(resolver);
}

void sysml_resolver_add_path(SysmlImportResolver *resolver, const char *path) {
    if (!resolver || !path) return;

    /* Convert to absolute path if possible */
    char *abs_path = sysml2_get_realpath(path);
    if (!abs_path) {
        /* If realpath fails, use the path as-is */
        abs_path = strdup(path);
        if (!abs_path) return;
    }

    /* Check if already added */
    for (size_t i = 0; i < resolver->path_count; i++) {
        if (strcmp(resolver->library_paths[i], abs_path) == 0) {
            free(abs_path);
            return;
        }
    }

    /* Grow array if needed */
    if (resolver->path_count >= resolver->path_capacity) {
        size_t new_capacity = resolver->path_capacity * 2;
        char **new_paths = realloc(resolver->library_paths,
                                    new_capacity * sizeof(char *));
        if (!new_paths) {
            free(abs_path);
            return;
        }
        resolver->library_paths = new_paths;
        resolver->path_capacity = new_capacity;
    }

    resolver->library_paths[resolver->path_count++] = abs_path;

    if (resolver->verbose) {
        fprintf(stderr, "note: added library path: %s\n", abs_path);
    }
}

void sysml_resolver_add_paths_from_env(SysmlImportResolver *resolver) {
    if (!resolver) return;

    const char *env = getenv(SYSML2_LIBRARY_PATH_ENV);
    if (!env || !*env) return;

    /* Parse colon-separated paths */
    char *env_copy = strdup(env);
    if (!env_copy) return;

    char *saveptr = NULL;
    char *token = strtok_r(env_copy, ":", &saveptr);
    while (token) {
        /* Skip empty tokens */
        if (*token) {
            sysml_resolver_add_path(resolver, token);
        }
        token = strtok_r(NULL, ":", &saveptr);
    }

    free(env_copy);
}

void sysml_resolver_cache_model(
    SysmlImportResolver *resolver,
    const char *path,
    SysmlSemanticModel *model
) {
    if (!resolver || !path || !model) return;

    /* Convert to absolute path */
    char *abs_path = sysml2_get_realpath(path);
    if (!abs_path) {
        abs_path = strdup(path);
        if (!abs_path) return;
    }

    /* Check if already cached */
    SysmlFileCache *cache = resolver->cache;
    while (cache) {
        if (strcmp(cache->path, abs_path) == 0) {
            /* Already cached - update model */
            cache->model = model;
            free(abs_path);
            return;
        }
        cache = cache->next;
    }

    /* Create new cache entry */
    SysmlFileCache *entry = malloc(sizeof(SysmlFileCache));
    if (!entry) {
        free(abs_path);
        return;
    }

    entry->path = abs_path;
    entry->model = model;
    entry->next = resolver->cache;
    resolver->cache = entry;
}

SysmlSemanticModel *sysml_resolver_get_cached(
    SysmlImportResolver *resolver,
    const char *path
) {
    if (!resolver || !path) return NULL;

    /* Convert to absolute path */
    char *abs_path = sysml2_get_realpath(path);
    if (!abs_path) {
        abs_path = strdup(path);
        if (!abs_path) return NULL;
    }

    SysmlFileCache *cache = resolver->cache;
    while (cache) {
        if (strcmp(cache->path, abs_path) == 0) {
            free(abs_path);
            return cache->model;
        }
        cache = cache->next;
    }

    free(abs_path);
    return NULL;
}

char *sysml_resolver_find_file(
    SysmlImportResolver *resolver,
    const char *import_target
) {
    if (!resolver || !import_target) return NULL;

    /* Extract the package name from the import target */
    char *package_name = extract_package_name(import_target);
    if (!package_name) return NULL;

    /* Try each library path */
    for (size_t i = 0; i < resolver->path_count; i++) {
        const char *lib_path = resolver->library_paths[i];

        /* Try direct file: {lib}/{package}.kerml and .sysml */
        char filename_kerml[256];
        char filename_sysml[256];
        snprintf(filename_kerml, sizeof(filename_kerml), "%s.kerml", package_name);
        snprintf(filename_sysml, sizeof(filename_sysml), "%s.sysml", package_name);

        char *full_path;

        /* Try {lib}/{package}.kerml */
        full_path = sysml2_path_join(lib_path, filename_kerml);
        if (full_path && sysml2_is_file(full_path)) {
            free(package_name);
            return full_path;
        }
        free(full_path);

        /* Try {lib}/{package}.sysml */
        full_path = sysml2_path_join(lib_path, filename_sysml);
        if (full_path && sysml2_is_file(full_path)) {
            free(package_name);
            return full_path;
        }
        free(full_path);

        /* Try recursive search in subdirectories (max depth 5) */
        char *found = search_directory_recursive(lib_path, filename_kerml, 5);
        if (found) {
            free(package_name);
            return found;
        }

        found = search_directory_recursive(lib_path, filename_sysml, 5);
        if (found) {
            free(package_name);
            return found;
        }
    }

    free(package_name);
    return NULL;
}

/* Check if a file is in the resolution stack (cycle detection) */
static bool is_in_resolution_stack(SysmlImportResolver *resolver, const char *path) {
    for (size_t i = 0; i < resolver->stack_depth; i++) {
        if (strcmp(resolver->resolution_stack[i], path) == 0) {
            return true;
        }
    }
    return false;
}

/* Push a file onto the resolution stack */
static bool push_resolution_stack(SysmlImportResolver *resolver, const char *path) {
    if (resolver->stack_depth >= resolver->stack_capacity) {
        size_t new_capacity = resolver->stack_capacity * 2;
        char **new_stack = realloc(resolver->resolution_stack,
                                    new_capacity * sizeof(char *));
        if (!new_stack) return false;
        resolver->resolution_stack = new_stack;
        resolver->stack_capacity = new_capacity;
    }

    char *path_copy = strdup(path);
    if (!path_copy) return false;

    resolver->resolution_stack[resolver->stack_depth++] = path_copy;
    return true;
}

/* Pop a file from the resolution stack */
static void pop_resolution_stack(SysmlImportResolver *resolver) {
    if (resolver->stack_depth > 0) {
        resolver->stack_depth--;
        free(resolver->resolution_stack[resolver->stack_depth]);
        resolver->resolution_stack[resolver->stack_depth] = NULL;
    }
}

/* Parse a single file and return its model */
static SysmlSemanticModel *parse_file(
    SysmlImportResolver *resolver,
    const char *path,
    Sysml2DiagContext *diag
) {
    /* Read file content */
    size_t content_length;
    char *content = sysml2_read_file(path, &content_length);
    if (!content) {
        /* Report error using diagnostic system */
        Sysml2SourceRange range = {{1, 1, 0}, {1, 1, 0}};
        char msg[512];
        snprintf(msg, sizeof(msg), "cannot read file '%s': %s", path, strerror(errno));
        sysml2_diag_emit(diag, sysml2_diag_create(
            diag, SYSML2_DIAG_E3010_IMPORT_NOT_FOUND, SYSML2_SEVERITY_ERROR,
            NULL, range, sysml2_arena_strdup(resolver->arena, msg)
        ));
        return NULL;
    }

    /* Create build context */
    SysmlBuildContext *build_ctx = sysml_build_context_create(
        resolver->arena, resolver->intern, path
    );
    if (!build_ctx) {
        free(content);
        return NULL;
    }

    /* Set up parser context */
    SysmlParserContext ctx = {
        .filename = path,
        .input = content,
        .input_len = content_length,
        .input_pos = 0,
        .error_count = 0,
        .line = 1,
        .col = 1,
        .furthest_pos = 0,
        .furthest_line = 0,
        .furthest_col = 0,
        .failed_rule_count = 0,
        .context_rule = NULL,
        .build_ctx = build_ctx,
    };

    /* Parse */
    sysml_context_t *parser = sysml_create(&ctx);
    if (!parser) {
        free(content);
        return NULL;
    }

    void *result = NULL;
    int parse_ok = sysml_parse(parser, &result);

    if (ctx.error_count > 0) {
        diag->error_count += ctx.error_count;
    }

    SysmlSemanticModel *model = NULL;
    if (parse_ok && ctx.error_count == 0) {
        model = sysml_build_finalize(build_ctx);
    }

    sysml_destroy(parser);
    free(content);

    return model;
}

/* Resolve imports for a single file (recursive) */
static Sysml2Result resolve_file_imports(
    SysmlImportResolver *resolver,
    const char *file_path,
    Sysml2DiagContext *diag
);

/* Resolve a single import target */
static Sysml2Result resolve_single_import(
    SysmlImportResolver *resolver,
    const char *import_target,
    const char *requesting_file,
    Sysml2SourceLoc loc,
    Sysml2DiagContext *diag
) {
    /* Find the file for this import */
    char *found_path = sysml_resolver_find_file(resolver, import_target);
    if (!found_path) {
        /* Import file not found */
        if (resolver->strict_imports) {
            /* In strict mode (--fix), emit E3010 error */
            Sysml2SourceRange range = {loc, loc};
            char msg[512];
            snprintf(msg, sizeof(msg), "import '%s' not found in library paths (from %s)",
                     import_target, requesting_file ? requesting_file : "<unknown>");
            sysml2_diag_emit(diag, sysml2_diag_create(
                diag, SYSML2_DIAG_E3010_IMPORT_NOT_FOUND, SYSML2_SEVERITY_ERROR,
                NULL, range, sysml2_arena_strdup(resolver->arena, msg)
            ));
            return SYSML2_ERROR_SEMANTIC;
        }
        /* Non-strict mode: just print verbose note and continue */
        if (resolver->verbose) {
            fprintf(stderr, "note: import '%s' not found in library paths\n", import_target);
        }
        return SYSML2_OK;  /* Not a hard error - validation will catch undefined refs */
    }

    /* Get canonical path */
    char *abs_path = sysml2_get_realpath(found_path);
    if (!abs_path) {
        abs_path = found_path;
    } else {
        free(found_path);
    }

    /* Check if already cached */
    if (sysml_resolver_get_cached(resolver, abs_path)) {
        free(abs_path);
        return SYSML2_OK;
    }

    /* Check for circular import */
    if (is_in_resolution_stack(resolver, abs_path)) {
        Sysml2SourceRange range = {loc, loc};
        char msg[512];
        snprintf(msg, sizeof(msg), "circular import detected: '%s' is already being processed",
                 abs_path);
        sysml2_diag_emit(diag, sysml2_diag_create(
            diag, SYSML2_DIAG_E3009_CIRCULAR_IMPORT, SYSML2_SEVERITY_ERROR,
            NULL, range, sysml2_arena_strdup(resolver->arena, msg)
        ));
        free(abs_path);
        return SYSML2_ERROR_SEMANTIC;
    }

    if (resolver->verbose) {
        fprintf(stderr, "note: resolving import '%s' -> %s\n", import_target, abs_path);
    }

    /* Push onto resolution stack */
    if (!push_resolution_stack(resolver, abs_path)) {
        free(abs_path);
        return SYSML2_ERROR_OUT_OF_MEMORY;
    }

    /* Parse the file */
    SysmlSemanticModel *model = parse_file(resolver, abs_path, diag);
    if (!model) {
        pop_resolution_stack(resolver);
        free(abs_path);
        return SYSML2_ERROR_SYNTAX;
    }

    /* Cache the model */
    sysml_resolver_cache_model(resolver, abs_path, model);

    /* Recursively resolve its imports */
    Sysml2Result result = resolve_file_imports(resolver, abs_path, diag);

    pop_resolution_stack(resolver);
    free(abs_path);

    return result;
}

/* Resolve imports for a model */
static Sysml2Result resolve_file_imports(
    SysmlImportResolver *resolver,
    const char *file_path,
    Sysml2DiagContext *diag
) {
    /* Get the model from cache */
    SysmlSemanticModel *model = sysml_resolver_get_cached(resolver, file_path);
    if (!model) {
        return SYSML2_OK;  /* Nothing to do */
    }

    /* Process each import */
    Sysml2Result overall_result = SYSML2_OK;
    for (size_t i = 0; i < model->import_count; i++) {
        SysmlImport *import = model->imports[i];
        if (!import || !import->target) continue;

        Sysml2Result result = resolve_single_import(
            resolver, import->target, file_path, import->loc, diag
        );
        if (result != SYSML2_OK && overall_result == SYSML2_OK) {
            overall_result = result;
        }

        if (sysml2_diag_should_stop(diag)) {
            break;
        }
    }

    return overall_result;
}

Sysml2Result sysml_resolver_resolve_imports(
    SysmlImportResolver *resolver,
    SysmlSemanticModel *model,
    Sysml2DiagContext *diag
) {
    if (!resolver || !model || resolver->disabled) {
        return SYSML2_OK;
    }

    /* Make sure the model's source is in the cache */
    if (model->source_name) {
        sysml_resolver_cache_model(resolver, model->source_name, model);
    }

    /* Push the source file onto the resolution stack */
    const char *source = model->source_name ? model->source_name : "<input>";
    char *abs_source = sysml2_get_realpath(source);
    if (!abs_source) {
        abs_source = strdup(source);
    }

    if (abs_source && !push_resolution_stack(resolver, abs_source)) {
        free(abs_source);
        return SYSML2_ERROR_OUT_OF_MEMORY;
    }

    /* Process imports */
    Sysml2Result overall_result = SYSML2_OK;
    for (size_t i = 0; i < model->import_count; i++) {
        SysmlImport *import = model->imports[i];
        if (!import || !import->target) continue;

        Sysml2Result result = resolve_single_import(
            resolver, import->target, source, import->loc, diag
        );
        if (result != SYSML2_OK && overall_result == SYSML2_OK) {
            overall_result = result;
        }

        if (sysml2_diag_should_stop(diag)) {
            break;
        }
    }

    if (abs_source) {
        pop_resolution_stack(resolver);
        free(abs_source);
    }

    return overall_result;
}

SysmlSemanticModel **sysml_resolver_get_all_models(
    SysmlImportResolver *resolver,
    size_t *count
) {
    if (!resolver || !count) {
        if (count) *count = 0;
        return NULL;
    }

    /* Count cached models */
    size_t n = 0;
    SysmlFileCache *cache = resolver->cache;
    while (cache) {
        n++;
        cache = cache->next;
    }

    if (n == 0) {
        *count = 0;
        return NULL;
    }

    /* Allocate array */
    SysmlSemanticModel **models = malloc(n * sizeof(SysmlSemanticModel *));
    if (!models) {
        *count = 0;
        return NULL;
    }

    /* Fill array (reverse order so first added is first in array) */
    size_t i = n;
    cache = resolver->cache;
    while (cache && i > 0) {
        models[--i] = cache->model;
        cache = cache->next;
    }

    *count = n;
    return models;
}
