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
#define RESOLVER_INITIAL_PACKAGE_MAP_CAPACITY 64

/* Environment variable name */
#define SYSML2_LIBRARY_PATH_ENV "SYSML2_LIBRARY_PATH"

/* Hash function for package names (djb2) */
static size_t hash_string(const char *str) {
    size_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + (size_t)c;
    }
    return hash;
}

/* Extract top-level package name from a model
 * Returns the name of the first package with no parent (top-level).
 * Returns NULL if no top-level package found.
 */
static const char *extract_top_level_package(const SysmlSemanticModel *model) {
    if (!model) return NULL;

    for (size_t i = 0; i < model->element_count; i++) {
        SysmlNode *node = model->elements[i];
        if (!node) continue;

        /* Check if it's a package (SYSML_KIND_PACKAGE or SYSML_KIND_LIBRARY_PACKAGE) */
        if (SYSML_KIND_IS_PACKAGE(node->kind)) {
            /* Top-level package has no parent */
            if (node->parent_id == NULL || node->parent_id[0] == '\0') {
                return node->name;
            }
        }
    }
    return NULL;
}

/* Register a package->file mapping in the resolver
 * If the package already exists, first-wins (don't overwrite).
 */
static void register_package_file(
    Sysml2ImportResolver *resolver,
    const char *pkg_name,
    const char *file_path
) {
    if (!resolver || !pkg_name || !file_path) return;
    if (!resolver->package_map) return;

    size_t bucket = hash_string(pkg_name) % resolver->package_map_capacity;

    /* Check if already registered */
    Sysml2PackageEntry *entry = resolver->package_map[bucket];
    while (entry) {
        if (strcmp(entry->package_name, pkg_name) == 0) {
            /* Already registered - first-wins, just warn in verbose mode */
            if (resolver->verbose) {
                fprintf(stderr, "note: package '%s' already mapped to %s, ignoring %s\n",
                        pkg_name, entry->file_path, file_path);
            }
            return;
        }
        entry = entry->next;
    }

    /* Create new entry */
    Sysml2PackageEntry *new_entry = malloc(sizeof(Sysml2PackageEntry));
    if (!new_entry) return;

    new_entry->package_name = pkg_name;  /* Interned string, not owned */
    new_entry->file_path = strdup(file_path);
    if (!new_entry->file_path) {
        free(new_entry);
        return;
    }

    /* Insert at head of bucket chain */
    new_entry->next = resolver->package_map[bucket];
    resolver->package_map[bucket] = new_entry;
    resolver->package_map_count++;

    if (resolver->verbose) {
        fprintf(stderr, "note: registered package '%s' -> %s\n", pkg_name, file_path);
    }
}

/* Lookup a package in the package map
 * Returns the file path if found, NULL otherwise.
 * The returned path is owned by the resolver - caller should strdup if needed.
 */
static const char *lookup_package_file(
    Sysml2ImportResolver *resolver,
    const char *pkg_name
) {
    if (!resolver || !pkg_name) return NULL;
    if (!resolver->package_map) return NULL;

    size_t bucket = hash_string(pkg_name) % resolver->package_map_capacity;

    Sysml2PackageEntry *entry = resolver->package_map[bucket];
    while (entry) {
        if (strcmp(entry->package_name, pkg_name) == 0) {
            return entry->file_path;
        }
        entry = entry->next;
    }
    return NULL;
}

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

Sysml2ImportResolver *sysml2_resolver_create(
    Sysml2Arena *arena,
    Sysml2Intern *intern
) {
    Sysml2ImportResolver *resolver = calloc(1, sizeof(Sysml2ImportResolver));
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

    /* Initialize package map */
    resolver->package_map = calloc(RESOLVER_INITIAL_PACKAGE_MAP_CAPACITY, sizeof(Sysml2PackageEntry *));
    if (!resolver->package_map) {
        free(resolver->resolution_stack);
        free(resolver->library_paths);
        free(resolver);
        return NULL;
    }
    resolver->package_map_capacity = RESOLVER_INITIAL_PACKAGE_MAP_CAPACITY;
    resolver->package_map_count = 0;

    resolver->cache = NULL;
    resolver->verbose = false;
    resolver->disabled = false;
    resolver->strict_imports = false;

    return resolver;
}

void sysml2_resolver_destroy(Sysml2ImportResolver *resolver) {
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
    Sysml2FileCache *cache = resolver->cache;
    while (cache) {
        Sysml2FileCache *next = cache->next;
        free(cache->path);
        free(cache);
        cache = next;
    }

    /* Free package map */
    if (resolver->package_map) {
        for (size_t i = 0; i < resolver->package_map_capacity; i++) {
            Sysml2PackageEntry *entry = resolver->package_map[i];
            while (entry) {
                Sysml2PackageEntry *next = entry->next;
                free(entry->file_path);
                free(entry);
                entry = next;
            }
        }
        free(resolver->package_map);
    }

    free(resolver);
}

void sysml2_resolver_add_path(Sysml2ImportResolver *resolver, const char *path) {
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

void sysml2_resolver_add_paths_from_env(Sysml2ImportResolver *resolver) {
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
            sysml2_resolver_add_path(resolver, token);
        }
        token = strtok_r(NULL, ":", &saveptr);
    }

    free(env_copy);
}

void sysml2_resolver_cache_model(
    Sysml2ImportResolver *resolver,
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
    Sysml2FileCache *cache = resolver->cache;
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
    Sysml2FileCache *entry = malloc(sizeof(Sysml2FileCache));
    if (!entry) {
        free(abs_path);
        return;
    }

    entry->path = abs_path;
    entry->model = model;
    entry->next = resolver->cache;
    resolver->cache = entry;

    /* Register the top-level package for discovery.
     * This allows imports to find packages even when the filename
     * doesn't match the package name (e.g., SystemBehavior in _index.sysml). */
    const char *pkg_name = extract_top_level_package(model);
    if (pkg_name) {
        register_package_file(resolver, pkg_name, abs_path);
    }
}

SysmlSemanticModel *sysml2_resolver_get_cached(
    Sysml2ImportResolver *resolver,
    const char *path
) {
    if (!resolver || !path) return NULL;

    /* Convert to absolute path */
    char *abs_path = sysml2_get_realpath(path);
    if (!abs_path) {
        abs_path = strdup(path);
        if (!abs_path) return NULL;
    }

    Sysml2FileCache *cache = resolver->cache;
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

char *sysml2_resolver_find_file(
    Sysml2ImportResolver *resolver,
    const char *import_target
) {
    if (!resolver || !import_target) return NULL;

    /* Extract the package name from the import target */
    char *package_name = extract_package_name(import_target);
    if (!package_name) return NULL;

    /* NEW: Check package map first (populated by preload) */
    const char *mapped_path = lookup_package_file(resolver, package_name);
    if (mapped_path) {
        if (resolver->verbose) {
            fprintf(stderr, "note: found '%s' via package map -> %s\n",
                    package_name, mapped_path);
        }
        free(package_name);
        return strdup(mapped_path);
    }

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
static bool is_in_resolution_stack(Sysml2ImportResolver *resolver, const char *path) {
    for (size_t i = 0; i < resolver->stack_depth; i++) {
        if (strcmp(resolver->resolution_stack[i], path) == 0) {
            return true;
        }
    }
    return false;
}

/* Push a file onto the resolution stack */
static bool push_resolution_stack(Sysml2ImportResolver *resolver, const char *path) {
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
static void pop_resolution_stack(Sysml2ImportResolver *resolver) {
    if (resolver->stack_depth > 0) {
        resolver->stack_depth--;
        free(resolver->resolution_stack[resolver->stack_depth]);
        resolver->resolution_stack[resolver->stack_depth] = NULL;
    }
}

/* Parse a single file and return its model */
static SysmlSemanticModel *parse_file(
    Sysml2ImportResolver *resolver,
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
    SysmlBuildContext *build_ctx = sysml2_build_context_create(
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
    sysml2_context_t *parser = sysml2_create(&ctx);
    if (!parser) {
        free(content);
        return NULL;
    }

    void *result = NULL;
    int parse_ok = sysml2_parse(parser, &result);

    if (ctx.error_count > 0) {
        diag->error_count += ctx.error_count;
        diag->parse_error_count += ctx.error_count;
    }

    SysmlSemanticModel *model = NULL;
    if (parse_ok && ctx.error_count == 0) {
        model = sysml2_build_finalize(build_ctx);
    }

    sysml2_destroy(parser);
    free(content);

    return model;
}

/* Resolve imports for a single file (recursive) */
static Sysml2Result resolve_file_imports(
    Sysml2ImportResolver *resolver,
    const char *file_path,
    Sysml2DiagContext *diag
);

/* Resolve a single import target */
static Sysml2Result resolve_single_import(
    Sysml2ImportResolver *resolver,
    const char *import_target,
    const char *requesting_file,
    Sysml2SourceLoc loc,
    Sysml2DiagContext *diag
) {
    /* Find the file for this import */
    char *found_path = sysml2_resolver_find_file(resolver, import_target);
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
    if (sysml2_resolver_get_cached(resolver, abs_path)) {
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
    sysml2_resolver_cache_model(resolver, abs_path, model);

    /* Recursively resolve its imports */
    Sysml2Result result = resolve_file_imports(resolver, abs_path, diag);

    pop_resolution_stack(resolver);
    free(abs_path);

    return result;
}

/* Resolve imports for a model */
static Sysml2Result resolve_file_imports(
    Sysml2ImportResolver *resolver,
    const char *file_path,
    Sysml2DiagContext *diag
) {
    /* Get the model from cache */
    SysmlSemanticModel *model = sysml2_resolver_get_cached(resolver, file_path);
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

Sysml2Result sysml2_resolver_resolve_imports(
    Sysml2ImportResolver *resolver,
    SysmlSemanticModel *model,
    Sysml2DiagContext *diag
) {
    if (!resolver || !model || resolver->disabled) {
        return SYSML2_OK;
    }

    /* Make sure the model's source is in the cache */
    if (model->source_name) {
        sysml2_resolver_cache_model(resolver, model->source_name, model);
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

SysmlSemanticModel **sysml2_resolver_get_all_models(
    Sysml2ImportResolver *resolver,
    size_t *count
) {
    if (!resolver || !count) {
        if (count) *count = 0;
        return NULL;
    }

    /* Count cached models */
    size_t n = 0;
    Sysml2FileCache *cache = resolver->cache;
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

/* Recursively load all SysML/KerML files from a directory */
static void preload_directory(
    Sysml2ImportResolver *resolver,
    const char *dir_path,
    Sysml2DiagContext *diag,
    int max_depth
) {
    if (max_depth <= 0) return;

    DIR *d = opendir(dir_path);
    if (!d) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        /* Skip hidden files/directories (starting with .) */
        if (entry->d_name[0] == '.') {
            continue;
        }

        char *full_path = sysml2_path_join(dir_path, entry->d_name);
        if (!full_path) continue;

        if (sysml2_is_directory(full_path)) {
            /* Recurse into subdirectory */
            preload_directory(resolver, full_path, diag, max_depth - 1);
        } else if (sysml2_is_file(full_path)) {
            /* Check if it's a SysML or KerML file */
            size_t len = strlen(entry->d_name);
            bool is_sysml = (len > 6 && strcmp(entry->d_name + len - 6, ".sysml") == 0);
            bool is_kerml = (len > 6 && strcmp(entry->d_name + len - 6, ".kerml") == 0);

            if (is_sysml || is_kerml) {
                /* Check if already cached */
                char *abs_path = sysml2_get_realpath(full_path);
                if (!abs_path) abs_path = strdup(full_path);

                if (abs_path && !sysml2_resolver_get_cached(resolver, abs_path)) {
                    /* Parse and cache the file */
                    SysmlSemanticModel *model = parse_file(resolver, abs_path, diag);
                    if (model) {
                        sysml2_resolver_cache_model(resolver, abs_path, model);

                        /* NEW: Register top-level package for discovery */
                        const char *pkg_name = extract_top_level_package(model);
                        if (pkg_name) {
                            register_package_file(resolver, pkg_name, abs_path);
                        }
                    }
                }
                free(abs_path);
            }
        }
        free(full_path);
    }

    closedir(d);
}

/* Discover packages in a directory for package map (without full caching).
 * This parses files only to extract package names, doesn't add to validation set. */
static void discover_packages_in_directory(
    Sysml2ImportResolver *resolver,
    const char *dir_path,
    Sysml2DiagContext *diag,
    int max_depth
) {
    if (max_depth <= 0) return;

    DIR *d = opendir(dir_path);
    if (!d) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        /* Skip hidden files/directories (starting with .) */
        if (entry->d_name[0] == '.') {
            continue;
        }

        char *full_path = sysml2_path_join(dir_path, entry->d_name);
        if (!full_path) continue;

        if (sysml2_is_directory(full_path)) {
            /* Recurse into subdirectory */
            discover_packages_in_directory(resolver, full_path, diag, max_depth - 1);
        } else if (sysml2_is_file(full_path)) {
            /* Check if it's a SysML or KerML file */
            size_t len = strlen(entry->d_name);
            bool is_sysml = (len > 6 && strcmp(entry->d_name + len - 6, ".sysml") == 0);
            bool is_kerml = (len > 6 && strcmp(entry->d_name + len - 6, ".kerml") == 0);

            if (is_sysml || is_kerml) {
                char *abs_path = sysml2_get_realpath(full_path);
                if (!abs_path) abs_path = strdup(full_path);

                /* Check if package already registered for this file */
                if (abs_path && !sysml2_resolver_get_cached(resolver, abs_path)) {
                    /* Parse file just for package discovery */
                    SysmlSemanticModel *model = parse_file(resolver, abs_path, diag);
                    if (model) {
                        /* Only register the package, don't cache the model.
                         * The file will be fully cached when actually imported. */
                        const char *pkg_name = extract_top_level_package(model);
                        if (pkg_name) {
                            register_package_file(resolver, pkg_name, abs_path);
                        }
                    }
                }
                free(abs_path);
            }
        }
        free(full_path);
    }

    closedir(d);
}

Sysml2Result sysml2_resolver_preload_libraries(
    Sysml2ImportResolver *resolver,
    Sysml2DiagContext *diag
) {
    if (!resolver) return SYSML2_ERROR_SEMANTIC;

    /* Preload all SysML/KerML files from each library path */
    for (size_t i = 0; i < resolver->path_count; i++) {
        const char *lib_path = resolver->library_paths[i];
        if (resolver->verbose) {
            fprintf(stderr, "note: preloading library files from %s\n", lib_path);
        }
        preload_directory(resolver, lib_path, diag, 10);
    }

    return SYSML2_OK;
}

Sysml2Result sysml2_resolver_discover_packages(
    Sysml2ImportResolver *resolver,
    const char *dir_path,
    Sysml2DiagContext *diag
) {
    if (!resolver || !dir_path) return SYSML2_ERROR_SEMANTIC;

    if (resolver->verbose) {
        fprintf(stderr, "note: discovering packages in %s\n", dir_path);
    }
    discover_packages_in_directory(resolver, dir_path, diag, 10);

    return SYSML2_OK;
}
