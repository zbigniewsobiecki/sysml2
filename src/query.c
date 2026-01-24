/*
 * SysML v2 Parser - Query Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/query.h"
#include <stdlib.h>
#include <string.h>

/*
 * Parse a query pattern string
 *
 * Pattern syntax:
 *   - "Pkg::Element"  -> EXACT
 *   - "Pkg::*"        -> DIRECT (direct children only)
 *   - "Pkg::**"       -> RECURSIVE (all descendants)
 */
Sysml2QueryPattern *sysml2_query_parse(const char *pattern, Sysml2Arena *arena) {
    if (!pattern || !*pattern) {
        return NULL;
    }

    Sysml2QueryPattern *qp = sysml2_arena_alloc(arena, sizeof(Sysml2QueryPattern));
    if (!qp) return NULL;

    qp->next = NULL;

    size_t len = strlen(pattern);

    /* Check for recursive wildcard suffix ::** */
    if (len >= 4 && strcmp(pattern + len - 4, "::**") == 0) {
        qp->kind = SYSML2_QUERY_RECURSIVE;
        /* Copy base path without ::** */
        char *base = sysml2_arena_alloc(arena, len - 3);  /* -4 for suffix, +1 for null */
        if (!base) return NULL;
        memcpy(base, pattern, len - 4);
        base[len - 4] = '\0';
        qp->base_path = base;
    }
    /* Check for direct wildcard suffix ::* */
    else if (len >= 3 && strcmp(pattern + len - 3, "::*") == 0) {
        qp->kind = SYSML2_QUERY_DIRECT;
        /* Copy base path without ::* */
        char *base = sysml2_arena_alloc(arena, len - 2);  /* -3 for suffix, +1 for null */
        if (!base) return NULL;
        memcpy(base, pattern, len - 3);
        base[len - 3] = '\0';
        qp->base_path = base;
    }
    /* Exact match */
    else {
        qp->kind = SYSML2_QUERY_EXACT;
        /* Copy the full pattern as base path */
        char *base = sysml2_arena_alloc(arena, len + 1);
        if (!base) return NULL;
        memcpy(base, pattern, len + 1);
        qp->base_path = base;
    }

    return qp;
}

/*
 * Parse multiple query patterns and chain them
 */
Sysml2QueryPattern *sysml2_query_parse_multi(
    const char **patterns,
    size_t pattern_count,
    Sysml2Arena *arena
) {
    if (!patterns || pattern_count == 0) {
        return NULL;
    }

    Sysml2QueryPattern *head = NULL;
    Sysml2QueryPattern *tail = NULL;

    for (size_t i = 0; i < pattern_count; i++) {
        Sysml2QueryPattern *qp = sysml2_query_parse(patterns[i], arena);
        if (!qp) continue;

        if (!head) {
            head = qp;
            tail = qp;
        } else {
            tail->next = qp;
            tail = qp;
        }
    }

    return head;
}

/*
 * Check if an element ID matches a query pattern
 */
bool sysml2_query_matches(const Sysml2QueryPattern *pattern, const char *element_id) {
    if (!pattern || !element_id) {
        return false;
    }

    size_t base_len = strlen(pattern->base_path);
    size_t id_len = strlen(element_id);

    switch (pattern->kind) {
        case SYSML2_QUERY_EXACT:
            /* "Pkg::X" matches only "Pkg::X" exactly */
            return strcmp(element_id, pattern->base_path) == 0;

        case SYSML2_QUERY_DIRECT:
            /* "Pkg::*" matches "Pkg::X" but not "Pkg::X::Y" */
            /* Must start with base_path:: */
            if (id_len <= base_len + 2) return false;
            if (strncmp(element_id, pattern->base_path, base_len) != 0) return false;
            if (element_id[base_len] != ':' || element_id[base_len + 1] != ':') return false;
            /* Must not have any more :: after the prefix */
            const char *remainder = element_id + base_len + 2;
            return strchr(remainder, ':') == NULL;

        case SYSML2_QUERY_RECURSIVE:
            /* "Pkg::**" matches "Pkg::X", "Pkg::X::Y", etc. */
            /* Also matches "Pkg" itself for convenience */
            if (strcmp(element_id, pattern->base_path) == 0) {
                return true;  /* Include the base element itself */
            }
            if (id_len <= base_len + 2) return false;
            if (strncmp(element_id, pattern->base_path, base_len) != 0) return false;
            return element_id[base_len] == ':' && element_id[base_len + 1] == ':';
    }

    return false;
}

/*
 * Check if an element ID matches any pattern in a linked list
 */
bool sysml2_query_matches_any(const Sysml2QueryPattern *patterns, const char *element_id) {
    for (const Sysml2QueryPattern *p = patterns; p; p = p->next) {
        if (sysml2_query_matches(p, element_id)) {
            return true;
        }
    }
    return false;
}

/*
 * Add an element ID to the result's ID set
 */
static bool add_element_id(Sysml2QueryResult *result, const char *id, Sysml2Arena *arena) {
    /* Check if already present */
    for (size_t i = 0; i < result->element_id_count; i++) {
        if (strcmp(result->element_ids[i], id) == 0) {
            return true;  /* Already in set */
        }
    }

    /* Grow capacity if needed */
    if (result->element_id_count >= result->element_id_capacity) {
        size_t new_cap = result->element_id_capacity == 0 ? 32 : result->element_id_capacity * 2;
        const char **new_ids = sysml2_arena_alloc(arena, new_cap * sizeof(const char *));
        if (!new_ids) return false;

        if (result->element_ids) {
            memcpy(new_ids, result->element_ids, result->element_id_count * sizeof(const char *));
        }
        result->element_ids = new_ids;
        result->element_id_capacity = new_cap;
    }

    result->element_ids[result->element_id_count++] = id;
    return true;
}

/*
 * Add an element to the result
 */
static bool add_element(Sysml2QueryResult *result, SysmlNode *element, Sysml2Arena *arena) {
    /* Grow capacity if needed */
    if (result->element_count >= result->element_capacity) {
        size_t new_cap = result->element_capacity == 0 ? 32 : result->element_capacity * 2;
        SysmlNode **new_elements = sysml2_arena_alloc(arena, new_cap * sizeof(SysmlNode *));
        if (!new_elements) return false;

        if (result->elements) {
            memcpy(new_elements, result->elements, result->element_count * sizeof(SysmlNode *));
        }
        result->elements = new_elements;
        result->element_capacity = new_cap;
    }

    result->elements[result->element_count++] = element;
    add_element_id(result, element->id, arena);
    return true;
}

/*
 * Add a relationship to the result
 */
static bool add_relationship(Sysml2QueryResult *result, SysmlRelationship *rel, Sysml2Arena *arena) {
    /* Grow capacity if needed */
    if (result->relationship_count >= result->relationship_capacity) {
        size_t new_cap = result->relationship_capacity == 0 ? 16 : result->relationship_capacity * 2;
        SysmlRelationship **new_rels = sysml2_arena_alloc(arena, new_cap * sizeof(SysmlRelationship *));
        if (!new_rels) return false;

        if (result->relationships) {
            memcpy(new_rels, result->relationships, result->relationship_count * sizeof(SysmlRelationship *));
        }
        result->relationships = new_rels;
        result->relationship_capacity = new_cap;
    }

    result->relationships[result->relationship_count++] = rel;
    return true;
}

/*
 * Add an import to the result
 */
static bool add_import(Sysml2QueryResult *result, SysmlImport *imp, Sysml2Arena *arena) {
    /* Grow capacity if needed */
    if (result->import_count >= result->import_capacity) {
        size_t new_cap = result->import_capacity == 0 ? 16 : result->import_capacity * 2;
        SysmlImport **new_imports = sysml2_arena_alloc(arena, new_cap * sizeof(SysmlImport *));
        if (!new_imports) return false;

        if (result->imports) {
            memcpy(new_imports, result->imports, result->import_count * sizeof(SysmlImport *));
        }
        result->imports = new_imports;
        result->import_capacity = new_cap;
    }

    result->imports[result->import_count++] = imp;
    return true;
}

/*
 * Execute a query against semantic models
 */
Sysml2QueryResult *sysml2_query_execute(
    const Sysml2QueryPattern *patterns,
    SysmlSemanticModel **models,
    size_t model_count,
    Sysml2Arena *arena
) {
    if (!patterns || !models || model_count == 0) {
        return NULL;
    }

    Sysml2QueryResult *result = sysml2_arena_alloc(arena, sizeof(Sysml2QueryResult));
    if (!result) return NULL;

    memset(result, 0, sizeof(Sysml2QueryResult));

    /* Pass 1: Collect matching elements */
    for (size_t m = 0; m < model_count; m++) {
        SysmlSemanticModel *model = models[m];
        if (!model) continue;

        for (size_t i = 0; i < model->element_count; i++) {
            SysmlNode *node = model->elements[i];
            if (node && node->id && sysml2_query_matches_any(patterns, node->id)) {
                add_element(result, node, arena);
            }
        }
    }

    /* Pass 2: Collect relationships where both endpoints are in result */
    for (size_t m = 0; m < model_count; m++) {
        SysmlSemanticModel *model = models[m];
        if (!model) continue;

        for (size_t i = 0; i < model->relationship_count; i++) {
            SysmlRelationship *rel = model->relationships[i];
            if (!rel) continue;

            /* Include relationship only if both source and target are in result */
            bool source_in = sysml2_query_result_contains(result, rel->source);
            bool target_in = sysml2_query_result_contains(result, rel->target);

            if (source_in && target_in) {
                add_relationship(result, rel, arena);
            }
        }
    }

    /* Pass 3: Collect imports where owner scope is in result */
    for (size_t m = 0; m < model_count; m++) {
        SysmlSemanticModel *model = models[m];
        if (!model) continue;

        for (size_t i = 0; i < model->import_count; i++) {
            SysmlImport *imp = model->imports[i];
            if (!imp) continue;

            /* Include import if its owner scope is in result */
            if (imp->owner_scope && sysml2_query_result_contains(result, imp->owner_scope)) {
                add_import(result, imp, arena);
            }
        }
    }

    return result;
}

/*
 * Check if an element ID is in the query result
 */
bool sysml2_query_result_contains(const Sysml2QueryResult *result, const char *element_id) {
    if (!result || !element_id) {
        return false;
    }

    for (size_t i = 0; i < result->element_id_count; i++) {
        if (strcmp(result->element_ids[i], element_id) == 0) {
            return true;
        }
    }

    return false;
}

/*
 * Free a query result (no-op for arena allocation)
 */
void sysml2_query_result_free(Sysml2QueryResult *result) {
    /* Arena-allocated results don't need manual freeing */
    (void)result;
}

/*
 * Get the parent path from an element ID
 */
const char *sysml2_query_parent_path(const char *element_id, Sysml2Arena *arena) {
    if (!element_id) return NULL;

    /* Find last :: separator */
    const char *last_sep = NULL;
    const char *p = element_id;
    while (*p) {
        if (p[0] == ':' && p[1] == ':') {
            last_sep = p;
        }
        p++;
    }

    if (!last_sep) {
        return NULL;  /* No parent (top-level element) */
    }

    size_t parent_len = last_sep - element_id;
    char *parent = sysml2_arena_alloc(arena, parent_len + 1);
    if (!parent) return NULL;

    memcpy(parent, element_id, parent_len);
    parent[parent_len] = '\0';

    return parent;
}

/*
 * Get ancestors needed for valid SysML output (parent stubs)
 */
void sysml2_query_get_ancestors(
    const Sysml2QueryResult *result,
    SysmlSemanticModel **models,
    size_t model_count,
    Sysml2Arena *arena,
    const char ***out_ancestors,
    size_t *out_count
) {
    if (!result || !out_ancestors || !out_count) {
        if (out_ancestors) *out_ancestors = NULL;
        if (out_count) *out_count = 0;
        return;
    }

    /* Temporary storage for ancestors */
    size_t capacity = 32;
    size_t count = 0;
    const char **ancestors = sysml2_arena_alloc(arena, capacity * sizeof(const char *));
    if (!ancestors) {
        *out_ancestors = NULL;
        *out_count = 0;
        return;
    }

    /* Helper to check if ID is in ancestors array */
    #define IN_ANCESTORS(id) ({ \
        bool found = false; \
        for (size_t i = 0; i < count; i++) { \
            if (strcmp(ancestors[i], id) == 0) { found = true; break; } \
        } \
        found; \
    })

    /* For each element in result, trace up to root */
    for (size_t i = 0; i < result->element_count; i++) {
        const char *elem_id = result->elements[i]->id;

        /* Walk up the parent chain */
        const char *parent_id = sysml2_query_parent_path(elem_id, arena);
        while (parent_id) {
            /* Skip if this ancestor is already in result or ancestors list */
            if (!sysml2_query_result_contains(result, parent_id) && !IN_ANCESTORS(parent_id)) {
                /* Grow capacity if needed */
                if (count >= capacity) {
                    size_t new_cap = capacity * 2;
                    const char **new_arr = sysml2_arena_alloc(arena, new_cap * sizeof(const char *));
                    if (!new_arr) break;
                    memcpy(new_arr, ancestors, count * sizeof(const char *));
                    ancestors = new_arr;
                    capacity = new_cap;
                }
                ancestors[count++] = parent_id;
            }

            parent_id = sysml2_query_parent_path(parent_id, arena);
        }
    }

    #undef IN_ANCESTORS

    *out_ancestors = ancestors;
    *out_count = count;
}
