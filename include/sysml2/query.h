/*
 * SysML v2 Parser - Query API
 *
 * Query mechanism using SysML/KerML qualified name syntax to filter
 * and extract specific parts of a model.
 *
 * Pattern syntax (matches import semantics):
 *   - "Pkg::Element"    - specific element (EXACT)
 *   - "Pkg::*"          - direct members (DIRECT)
 *   - "Pkg::**"         - all descendants recursive (RECURSIVE)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_QUERY_H
#define SYSML2_QUERY_H

#include "common.h"
#include "ast.h"
#include "arena.h"

/*
 * Query pattern kinds (matching import semantics)
 */
typedef enum {
    SYSML2_QUERY_EXACT,      /* Pkg::Element - specific element */
    SYSML2_QUERY_DIRECT,     /* Pkg::* - direct members only */
    SYSML2_QUERY_RECURSIVE,  /* Pkg::** - all descendants */
} Sysml2QueryKind;

/*
 * Query pattern - parsed from a string like "Pkg::Element" or "Pkg::*"
 */
typedef struct Sysml2QueryPattern {
    Sysml2QueryKind kind;
    const char *base_path;              /* e.g., "DataModel" or "DataModel::Entities" */
    struct Sysml2QueryPattern *next;    /* Linked list for multiple patterns */
} Sysml2QueryPattern;

/*
 * Query result - contains filtered elements and relationships
 */
typedef struct {
    SysmlNode **elements;
    size_t element_count;
    size_t element_capacity;

    SysmlRelationship **relationships;
    size_t relationship_count;
    size_t relationship_capacity;

    SysmlImport **imports;
    size_t import_count;
    size_t import_capacity;

    /* Set of element IDs in result (for relationship filtering) */
    const char **element_ids;
    size_t element_id_count;
    size_t element_id_capacity;
} Sysml2QueryResult;

/*
 * Parse a query pattern string
 *
 * Supports patterns like:
 *   - "Pkg::Element"    -> EXACT match
 *   - "Pkg::*"          -> DIRECT children
 *   - "Pkg::**"         -> RECURSIVE descendants
 *
 * @param pattern Pattern string to parse
 * @param arena Memory arena for allocation
 * @return Parsed pattern, or NULL on error
 */
Sysml2QueryPattern *sysml2_query_parse(const char *pattern, Sysml2Arena *arena);

/*
 * Parse multiple query patterns and chain them
 *
 * @param patterns Array of pattern strings
 * @param pattern_count Number of patterns
 * @param arena Memory arena for allocation
 * @return Linked list of parsed patterns, or NULL on error
 */
Sysml2QueryPattern *sysml2_query_parse_multi(
    const char **patterns,
    size_t pattern_count,
    Sysml2Arena *arena
);

/*
 * Check if an element ID matches a query pattern
 *
 * @param pattern Query pattern to match against
 * @param element_id Element ID (qualified path like "Pkg::Element")
 * @return true if the element matches the pattern
 */
bool sysml2_query_matches(const Sysml2QueryPattern *pattern, const char *element_id);

/*
 * Check if an element ID matches any pattern in a linked list
 *
 * @param patterns Linked list of query patterns
 * @param element_id Element ID to check
 * @return true if the element matches any pattern
 */
bool sysml2_query_matches_any(const Sysml2QueryPattern *patterns, const char *element_id);

/*
 * Execute a query against one or more semantic models
 *
 * Returns elements that match any of the patterns, plus their relationships
 * (where both endpoints are in the result set).
 *
 * @param patterns Linked list of query patterns
 * @param models Array of semantic models to query
 * @param model_count Number of models
 * @param arena Memory arena for result allocation
 * @return Query result with filtered elements and relationships
 */
Sysml2QueryResult *sysml2_query_execute(
    const Sysml2QueryPattern *patterns,
    SysmlSemanticModel **models,
    size_t model_count,
    Sysml2Arena *arena
);

/*
 * Check if an element ID is in the query result
 *
 * @param result Query result to check
 * @param element_id Element ID to look for
 * @return true if the element is in the result
 */
bool sysml2_query_result_contains(const Sysml2QueryResult *result, const char *element_id);

/*
 * Free a query result (if not using arena allocation)
 *
 * Note: If arena allocation was used, this is a no-op as the arena
 * manages the memory.
 *
 * @param result Query result to free
 */
void sysml2_query_result_free(Sysml2QueryResult *result);

/*
 * Get the parent path from an element ID
 *
 * For "A::B::C", returns "A::B"
 * For "A", returns NULL
 *
 * @param element_id Element ID
 * @param arena Memory arena for allocation
 * @return Parent path, or NULL if no parent
 */
const char *sysml2_query_parent_path(const char *element_id, Sysml2Arena *arena);

/*
 * Get ancestors needed to form valid SysML output (parent stubs)
 *
 * When outputting a filtered result as SysML, we need parent package stubs
 * to maintain valid syntax. This returns all ancestor IDs for elements in
 * the result that are not themselves in the result.
 *
 * @param result Query result
 * @param models Source models (to look up ancestor nodes)
 * @param model_count Number of models
 * @param arena Memory arena for allocation
 * @param out_ancestors Output array of ancestor element IDs
 * @param out_count Output count of ancestors
 */
void sysml2_query_get_ancestors(
    const Sysml2QueryResult *result,
    SysmlSemanticModel **models,
    size_t model_count,
    Sysml2Arena *arena,
    const char ***out_ancestors,
    size_t *out_count
);

#endif /* SYSML2_QUERY_H */
