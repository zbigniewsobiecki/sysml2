/*
 * SysML v2 Parser - Import Resolver
 *
 * Automatic resolution of import statements by finding and parsing
 * imported files from configured library paths.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_IMPORT_RESOLVER_H
#define SYSML2_IMPORT_RESOLVER_H

#include "common.h"
#include "arena.h"
#include "intern.h"
#include "ast.h"
#include "diagnostic.h"

/* Forward declaration */
typedef struct SysmlImportResolver SysmlImportResolver;

/*
 * File cache entry - stores a parsed file to avoid re-parsing
 */
typedef struct SysmlFileCache {
    char *path;                      /* Full path to file (owned) */
    SysmlSemanticModel *model;       /* Parsed model (owned by arena) */
    struct SysmlFileCache *next;     /* Next entry in linked list */
} SysmlFileCache;

/*
 * Import Resolver - manages library paths, file caching, and cycle detection
 */
struct SysmlImportResolver {
    /* Library search paths */
    char **library_paths;            /* Array of library search paths (owned) */
    size_t path_count;
    size_t path_capacity;

    /* File cache */
    SysmlFileCache *cache;           /* Linked list of parsed files */

    /* Cycle detection */
    char **resolution_stack;         /* Stack of files being resolved (owned) */
    size_t stack_depth;
    size_t stack_capacity;

    /* Memory management */
    Sysml2Arena *arena;              /* Arena for AST allocations */
    Sysml2Intern *intern;            /* String interning */

    /* Options */
    bool verbose;                    /* Print verbose messages */
    bool disabled;                   /* --no-resolve flag */
    bool strict_imports;             /* Emit errors for missing imports (for --fix mode) */
};

/*
 * Create a new import resolver
 *
 * @param arena Memory arena for AST allocations
 * @param intern String interning table
 * @return New resolver, or NULL on allocation failure
 */
SysmlImportResolver *sysml_resolver_create(
    Sysml2Arena *arena,
    Sysml2Intern *intern
);

/*
 * Destroy an import resolver and free its resources
 *
 * Note: AST nodes are managed by the arena, not freed here.
 *
 * @param resolver Resolver to destroy (may be NULL)
 */
void sysml_resolver_destroy(SysmlImportResolver *resolver);

/*
 * Add a library search path
 *
 * Paths are searched in the order they are added.
 *
 * @param resolver Import resolver
 * @param path Directory path to add
 */
void sysml_resolver_add_path(SysmlImportResolver *resolver, const char *path);

/*
 * Add library paths from the SYSML2_LIBRARY_PATH environment variable
 *
 * The variable should contain colon-separated paths (like PATH).
 *
 * @param resolver Import resolver
 */
void sysml_resolver_add_paths_from_env(SysmlImportResolver *resolver);

/*
 * Cache a parsed model for a file
 *
 * This is used to add the initial input file(s) to the cache
 * before resolving their imports.
 *
 * @param resolver Import resolver
 * @param path Full path to the file
 * @param model Parsed semantic model
 */
void sysml_resolver_cache_model(
    SysmlImportResolver *resolver,
    const char *path,
    SysmlSemanticModel *model
);

/*
 * Check if a file is already cached
 *
 * @param resolver Import resolver
 * @param path Full path to check
 * @return Cached model, or NULL if not cached
 */
SysmlSemanticModel *sysml_resolver_get_cached(
    SysmlImportResolver *resolver,
    const char *path
);

/*
 * Resolve all imports in a model
 *
 * Recursively finds and parses imported files, adding them to the cache.
 * Detects and reports circular imports.
 *
 * @param resolver Import resolver
 * @param model Model with imports to resolve
 * @param diag Diagnostic context for errors
 * @return SYSML2_OK on success, error code on failure
 */
Sysml2Result sysml_resolver_resolve_imports(
    SysmlImportResolver *resolver,
    SysmlSemanticModel *model,
    Sysml2DiagContext *diag
);

/*
 * Get all cached models for multi-file validation
 *
 * @param resolver Import resolver
 * @param count Output: number of models
 * @return Array of model pointers (owned by resolver)
 */
SysmlSemanticModel **sysml_resolver_get_all_models(
    SysmlImportResolver *resolver,
    size_t *count
);

/*
 * Find the file path for an import target
 *
 * Searches library paths for a file matching the package name.
 *
 * @param resolver Import resolver
 * @param import_target Import target (e.g., "ScalarValues", "ISQ::Length")
 * @return Full path to file (must be freed by caller), or NULL if not found
 */
char *sysml_resolver_find_file(
    SysmlImportResolver *resolver,
    const char *import_target
);

#endif /* SYSML2_IMPORT_RESOLVER_H */
