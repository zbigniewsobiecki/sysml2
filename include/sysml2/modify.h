/*
 * SysML v2 Parser - Model Modification API
 *
 * Provides model manipulation capabilities for programmatic editing:
 *   - DELETE: Remove elements by qualified name pattern
 *   - SET: Insert new elements or replace existing ones (UPSERT semantics)
 *
 * Reuses query pattern syntax (Pkg::Element, Pkg::*, Pkg::**).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_MODIFY_H
#define SYSML2_MODIFY_H

#include "common.h"
#include "arena.h"
#include "intern.h"
#include "ast.h"
#include "query.h"

/*
 * Set operation - elements to insert/replace at a target scope
 */
typedef struct Sysml2SetOp {
    const char *fragment_path;       /* Path to fragment file (NULL for stdin) */
    const char *fragment_content;    /* Pre-loaded content (for stdin) */
    size_t fragment_content_len;
    const char *target_scope;        /* Target scope ID (qualified name) */
    bool create_scope;               /* Create scope if doesn't exist */
    struct Sysml2SetOp *next;        /* Linked list for multiple operations */
} Sysml2SetOp;

/*
 * Modification plan - collects all operations to perform
 */
typedef struct Sysml2ModifyPlan {
    Sysml2QueryPattern *delete_patterns;  /* Patterns of elements to delete */
    Sysml2SetOp *set_ops;                 /* Set operations to perform */
    bool dry_run;                         /* Preview changes without writing */
    Sysml2Arena *arena;                   /* Memory arena for allocations */
} Sysml2ModifyPlan;

/*
 * Per-file modification result
 */
typedef struct Sysml2ModifyFileResult {
    const char *path;                    /* File path */
    SysmlSemanticModel *original;        /* Original model (reference) */
    SysmlSemanticModel *modified;        /* Modified model (new allocation) */
    size_t elements_deleted;             /* Count of deleted elements */
    size_t elements_added;               /* Count of added elements */
    size_t elements_replaced;            /* Count of replaced elements */
    bool needs_write;                    /* True if file needs to be written */
    struct Sysml2ModifyFileResult *next; /* Linked list */
} Sysml2ModifyFileResult;

/*
 * Overall modification result
 */
typedef struct Sysml2ModifyResult {
    Sysml2ModifyFileResult *files;       /* Linked list of file results */
    size_t file_count;                   /* Number of affected files */
    bool success;                        /* Overall success status */
    const char *error_message;           /* Error message if failed */
} Sysml2ModifyResult;

/*
 * Create a new modification plan
 *
 * @param arena Memory arena for allocations
 * @return New modification plan
 */
Sysml2ModifyPlan *sysml2_modify_plan_create(Sysml2Arena *arena);

/*
 * Add a delete pattern to the plan
 *
 * @param plan Modification plan
 * @param pattern Pattern string (e.g., "Pkg::Element", "Pkg::*", "Pkg::**")
 * @return SYSML2_OK on success
 */
Sysml2Result sysml2_modify_plan_add_delete(
    Sysml2ModifyPlan *plan,
    const char *pattern
);

/*
 * Add a set operation to the plan (from file)
 *
 * @param plan Modification plan
 * @param fragment_path Path to fragment file
 * @param target_scope Target scope for insertion
 * @param create_scope Create scope if it doesn't exist
 * @return SYSML2_OK on success
 */
Sysml2Result sysml2_modify_plan_add_set_file(
    Sysml2ModifyPlan *plan,
    const char *fragment_path,
    const char *target_scope,
    bool create_scope
);

/*
 * Add a set operation to the plan (from content string)
 *
 * @param plan Modification plan
 * @param content Fragment content
 * @param content_len Content length
 * @param target_scope Target scope for insertion
 * @param create_scope Create scope if it doesn't exist
 * @return SYSML2_OK on success
 */
Sysml2Result sysml2_modify_plan_add_set_content(
    Sysml2ModifyPlan *plan,
    const char *content,
    size_t content_len,
    const char *target_scope,
    bool create_scope
);

/*
 * Clone a model with elements filtered out by delete patterns
 *
 * This is the core delete algorithm:
 * 1. Match elements against delete patterns
 * 2. Cascade deletion to children
 * 3. Copy non-deleted elements to new model
 * 4. Filter relationships (remove if source or target deleted)
 * 5. Filter imports (remove if owner scope deleted)
 *
 * @param original Original model (unchanged)
 * @param patterns Delete patterns (linked list)
 * @param arena Memory arena for new model
 * @param intern String intern table
 * @param out_deleted_count Output: number of elements deleted
 * @return New model with deletions applied, or NULL on error
 */
SysmlSemanticModel *sysml2_modify_clone_with_deletions(
    const SysmlSemanticModel *original,
    const Sysml2QueryPattern *patterns,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    size_t *out_deleted_count
);

/*
 * Merge a fragment into a model at the specified scope
 *
 * This is the core set algorithm:
 * 1. Validate target scope exists (or create if --create-scope)
 * 1.5. If replace_scope, mark all direct children for removal (clears scope)
 * 2. Remap fragment element IDs to target scope
 * 3. Replace existing elements with same IDs
 * 4. Add new elements
 * 5. Remap relationships and imports
 *
 * @param base Base model to modify (actually creates new model)
 * @param fragment Fragment to merge
 * @param target_scope Target scope for insertion
 * @param create_scope Create scope chain if doesn't exist
 * @param replace_scope Clear target scope before inserting (preserves fragment order)
 * @param arena Memory arena for new model
 * @param intern String intern table
 * @param out_added_count Output: number of elements added
 * @param out_replaced_count Output: number of elements replaced
 * @return New model with merge applied, or NULL on error
 */
SysmlSemanticModel *sysml2_modify_merge_fragment(
    const SysmlSemanticModel *base,
    const SysmlSemanticModel *fragment,
    const char *target_scope,
    bool create_scope,
    bool replace_scope,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    size_t *out_added_count,
    size_t *out_replaced_count
);

/*
 * Remap an element ID to a new scope
 *
 * For top-level elements (NULL parent), returns target_scope.
 * For nested elements, returns target_scope::original_id.
 *
 * @param original_id Original element ID (may be NULL for top-level)
 * @param target_scope Target scope
 * @param arena Memory arena
 * @param intern String intern table
 * @return Remapped ID
 */
const char *sysml2_modify_remap_id(
    const char *original_id,
    const char *target_scope,
    Sysml2Arena *arena,
    Sysml2Intern *intern
);

/*
 * Find which file contains an element
 *
 * @param element_id Element ID to find
 * @param models Array of models
 * @param model_count Number of models
 * @return Index of containing model, or -1 if not found
 */
int sysml2_modify_find_containing_file(
    const char *element_id,
    SysmlSemanticModel **models,
    size_t model_count
);

/*
 * Check if a scope exists in the model
 *
 * @param model Model to search
 * @param scope_id Scope ID to check
 * @return true if scope exists
 */
bool sysml2_modify_scope_exists(
    const SysmlSemanticModel *model,
    const char *scope_id
);

/*
 * Create a scope chain in a model
 *
 * Creates all missing ancestor scopes as packages.
 * For example, creating "A::B::C" will create A, A::B, and A::B::C
 * if they don't exist.
 *
 * @param model Model to modify (creates new model)
 * @param scope_id Full scope path to create
 * @param arena Memory arena
 * @param intern String intern table
 * @return New model with scope chain, or NULL on error
 */
SysmlSemanticModel *sysml2_modify_create_scope_chain(
    const SysmlSemanticModel *model,
    const char *scope_id,
    Sysml2Arena *arena,
    Sysml2Intern *intern
);

/*
 * Check if an ID starts with a given prefix (as a proper scope prefix)
 *
 * "Pkg::A::B" starts with "Pkg::A" but not "Pkg::AB".
 *
 * @param id ID to check
 * @param prefix Prefix to look for
 * @return true if id starts with prefix::
 */
bool sysml2_modify_id_starts_with(const char *id, const char *prefix);

/*
 * Get the local name from a qualified ID
 *
 * For "A::B::C", returns "C".
 * For "A", returns "A".
 *
 * @param qualified_id Qualified ID
 * @return Pointer to local name portion (within the input string)
 */
const char *sysml2_modify_get_local_name(const char *qualified_id);

/*
 * List all scopes in a model
 *
 * Collects all package/namespace IDs that can be used as target scopes.
 * Useful for error messages suggesting available scopes.
 *
 * @param model Model to search
 * @param arena Memory arena for allocations
 * @param out_scopes Output: array of scope IDs (NULL-terminated)
 * @param out_count Output: number of scopes found
 * @return true on success
 */
bool sysml2_modify_list_scopes(
    const SysmlSemanticModel *model,
    Sysml2Arena *arena,
    const char ***out_scopes,
    size_t *out_count
);

/*
 * List all scopes across multiple models
 *
 * Collects all package/namespace IDs from all models.
 * Useful for error messages when target scope is not found.
 *
 * @param models Array of models to search
 * @param model_count Number of models
 * @param arena Memory arena for allocations
 * @param out_scopes Output: array of scope IDs (NULL-terminated)
 * @param out_count Output: number of scopes found
 * @return true on success
 */
bool sysml2_modify_list_scopes_multi(
    SysmlSemanticModel **models,
    size_t model_count,
    Sysml2Arena *arena,
    const char ***out_scopes,
    size_t *out_count
);

/*
 * Find similar scope names (simple prefix matching)
 *
 * Returns scopes that start with the same prefix as the target,
 * or have the same local name (after last ::).
 *
 * @param target Target scope that was not found
 * @param scopes Array of available scopes
 * @param scope_count Number of scopes
 * @param arena Memory arena for allocations
 * @param out_suggestions Output: array of similar scope names (NULL-terminated)
 * @param out_count Output: number of suggestions found
 * @param max_suggestions Maximum number of suggestions to return
 * @return true on success
 */
bool sysml2_modify_find_similar_scopes(
    const char *target,
    const char **scopes,
    size_t scope_count,
    Sysml2Arena *arena,
    const char ***out_suggestions,
    size_t *out_count,
    size_t max_suggestions
);

#endif /* SYSML2_MODIFY_H */
