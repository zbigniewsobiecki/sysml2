/*
 * SysML v2 Parser - Model Modification Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/modify.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Helper: Check if ID is in a set of IDs
 */
static bool id_in_set(const char *id, const char **set, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(set[i], id) == 0) {
            return true;
        }
    }
    return false;
}

/*
 * Helper: Add ID to a dynamic set
 */
static bool add_to_id_set(
    const char *id,
    const char ***set,
    size_t *count,
    size_t *capacity,
    Sysml2Arena *arena
) {
    /* Check if already in set */
    if (id_in_set(id, *set, *count)) {
        return true;
    }

    /* Grow capacity if needed */
    if (*count >= *capacity) {
        size_t new_cap = *capacity == 0 ? 32 : *capacity * 2;
        const char **new_set = sysml2_arena_alloc(arena, new_cap * sizeof(const char *));
        if (!new_set) return false;

        if (*set) {
            memcpy(new_set, *set, *count * sizeof(const char *));
        }
        *set = new_set;
        *capacity = new_cap;
    }

    (*set)[(*count)++] = id;
    return true;
}

/*
 * Create a new modification plan
 */
Sysml2ModifyPlan *sysml2_modify_plan_create(Sysml2Arena *arena) {
    Sysml2ModifyPlan *plan = sysml2_arena_alloc(arena, sizeof(Sysml2ModifyPlan));
    if (!plan) return NULL;

    plan->delete_patterns = NULL;
    plan->set_ops = NULL;
    plan->dry_run = false;
    plan->arena = arena;

    return plan;
}

/*
 * Add a delete pattern to the plan
 */
Sysml2Result sysml2_modify_plan_add_delete(
    Sysml2ModifyPlan *plan,
    const char *pattern
) {
    if (!plan || !pattern) {
        return SYSML2_ERROR_SYNTAX;
    }

    Sysml2QueryPattern *qp = sysml2_query_parse(pattern, plan->arena);
    if (!qp) {
        return SYSML2_ERROR_SYNTAX;
    }

    /* Append to linked list */
    if (!plan->delete_patterns) {
        plan->delete_patterns = qp;
    } else {
        Sysml2QueryPattern *tail = plan->delete_patterns;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = qp;
    }

    return SYSML2_OK;
}

/*
 * Add a set operation to the plan (from file)
 */
Sysml2Result sysml2_modify_plan_add_set_file(
    Sysml2ModifyPlan *plan,
    const char *fragment_path,
    const char *target_scope,
    bool create_scope
) {
    if (!plan || !fragment_path || !target_scope) {
        return SYSML2_ERROR_SYNTAX;
    }

    Sysml2SetOp *op = sysml2_arena_alloc(plan->arena, sizeof(Sysml2SetOp));
    if (!op) return SYSML2_ERROR_OUT_OF_MEMORY;

    op->fragment_path = fragment_path;
    op->fragment_content = NULL;
    op->fragment_content_len = 0;
    op->target_scope = target_scope;
    op->create_scope = create_scope;
    op->next = NULL;

    /* Append to linked list */
    if (!plan->set_ops) {
        plan->set_ops = op;
    } else {
        Sysml2SetOp *tail = plan->set_ops;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = op;
    }

    return SYSML2_OK;
}

/*
 * Add a set operation to the plan (from content string)
 */
Sysml2Result sysml2_modify_plan_add_set_content(
    Sysml2ModifyPlan *plan,
    const char *content,
    size_t content_len,
    const char *target_scope,
    bool create_scope
) {
    if (!plan || !content || !target_scope) {
        return SYSML2_ERROR_SYNTAX;
    }

    Sysml2SetOp *op = sysml2_arena_alloc(plan->arena, sizeof(Sysml2SetOp));
    if (!op) return SYSML2_ERROR_OUT_OF_MEMORY;

    op->fragment_path = NULL;
    op->fragment_content = content;
    op->fragment_content_len = content_len;
    op->target_scope = target_scope;
    op->create_scope = create_scope;
    op->next = NULL;

    /* Append to linked list */
    if (!plan->set_ops) {
        plan->set_ops = op;
    } else {
        Sysml2SetOp *tail = plan->set_ops;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = op;
    }

    return SYSML2_OK;
}

/*
 * Check if an ID starts with a given prefix (as a proper scope prefix)
 */
bool sysml2_modify_id_starts_with(const char *id, const char *prefix) {
    if (!id || !prefix) return false;

    size_t prefix_len = strlen(prefix);
    size_t id_len = strlen(id);

    /* ID must be longer than prefix + "::" */
    if (id_len <= prefix_len + 2) return false;

    /* Check prefix match */
    if (strncmp(id, prefix, prefix_len) != 0) return false;

    /* Check for :: separator */
    return id[prefix_len] == ':' && id[prefix_len + 1] == ':';
}

/*
 * Get the local name from a qualified ID
 */
const char *sysml2_modify_get_local_name(const char *qualified_id) {
    if (!qualified_id) return NULL;

    /* Find last :: separator */
    const char *last_sep = NULL;
    const char *p = qualified_id;
    while (*p) {
        if (p[0] == ':' && p[1] == ':') {
            last_sep = p;
        }
        p++;
    }

    if (!last_sep) {
        return qualified_id;  /* No separator, entire string is local name */
    }

    return last_sep + 2;  /* Skip past :: */
}

/*
 * Clone a model with elements filtered out by delete patterns
 */
SysmlSemanticModel *sysml2_modify_clone_with_deletions(
    const SysmlSemanticModel *original,
    const Sysml2QueryPattern *patterns,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    size_t *out_deleted_count
) {
    if (!original || !patterns || !arena) {
        if (out_deleted_count) *out_deleted_count = 0;
        return NULL;
    }

    /* Collect IDs to delete */
    const char **deleted_ids = NULL;
    size_t deleted_count = 0;
    size_t deleted_capacity = 0;

    /* Pass 1: Direct matches against patterns */
    for (size_t i = 0; i < original->element_count; i++) {
        SysmlNode *node = original->elements[i];
        if (!node || !node->id) continue;

        if (sysml2_query_matches_any(patterns, node->id)) {
            add_to_id_set(node->id, &deleted_ids, &deleted_count, &deleted_capacity, arena);
        }
    }

    /* Pass 2: Cascade to children (repeat until no change) */
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < original->element_count; i++) {
            SysmlNode *node = original->elements[i];
            if (!node || !node->id) continue;

            /* Skip if already deleted */
            if (id_in_set(node->id, deleted_ids, deleted_count)) continue;

            /* Check if parent is deleted */
            if (node->parent_id && id_in_set(node->parent_id, deleted_ids, deleted_count)) {
                add_to_id_set(node->id, &deleted_ids, &deleted_count, &deleted_capacity, arena);
                changed = true;
            }
        }
    }

    if (out_deleted_count) *out_deleted_count = deleted_count;

    /* If nothing to delete, return a shallow clone */
    if (deleted_count == 0) {
        /* Create a copy of the model structure */
        SysmlSemanticModel *clone = sysml2_arena_alloc(arena, sizeof(SysmlSemanticModel));
        if (!clone) return NULL;

        clone->source_name = original->source_name;
        clone->element_count = original->element_count;
        clone->element_capacity = original->element_count;
        clone->relationship_count = original->relationship_count;
        clone->relationship_capacity = original->relationship_count;
        clone->import_count = original->import_count;
        clone->import_capacity = original->import_count;

        /* Copy element pointers (they're immutable) */
        if (original->element_count > 0) {
            clone->elements = sysml2_arena_alloc(arena, original->element_count * sizeof(SysmlNode *));
            if (!clone->elements) return NULL;
            memcpy(clone->elements, original->elements, original->element_count * sizeof(SysmlNode *));
        } else {
            clone->elements = NULL;
        }

        /* Copy relationship pointers */
        if (original->relationship_count > 0) {
            clone->relationships = sysml2_arena_alloc(arena, original->relationship_count * sizeof(SysmlRelationship *));
            if (!clone->relationships) return NULL;
            memcpy(clone->relationships, original->relationships, original->relationship_count * sizeof(SysmlRelationship *));
        } else {
            clone->relationships = NULL;
        }

        /* Copy import pointers */
        if (original->import_count > 0) {
            clone->imports = sysml2_arena_alloc(arena, original->import_count * sizeof(SysmlImport *));
            if (!clone->imports) return NULL;
            memcpy(clone->imports, original->imports, original->import_count * sizeof(SysmlImport *));
        } else {
            clone->imports = NULL;
        }

        return clone;
    }

    /* Create new model */
    SysmlSemanticModel *result = sysml2_arena_alloc(arena, sizeof(SysmlSemanticModel));
    if (!result) return NULL;

    result->source_name = original->source_name;
    result->element_count = 0;
    result->element_capacity = original->element_count;
    result->relationship_count = 0;
    result->relationship_capacity = original->relationship_count;
    result->import_count = 0;
    result->import_capacity = original->import_count;

    /* Allocate arrays */
    if (original->element_count > 0) {
        result->elements = sysml2_arena_alloc(arena, original->element_count * sizeof(SysmlNode *));
        if (!result->elements) return NULL;
    } else {
        result->elements = NULL;
    }

    if (original->relationship_count > 0) {
        result->relationships = sysml2_arena_alloc(arena, original->relationship_count * sizeof(SysmlRelationship *));
        if (!result->relationships) return NULL;
    } else {
        result->relationships = NULL;
    }

    if (original->import_count > 0) {
        result->imports = sysml2_arena_alloc(arena, original->import_count * sizeof(SysmlImport *));
        if (!result->imports) return NULL;
    } else {
        result->imports = NULL;
    }

    /* Pass 3: Copy non-deleted elements */
    for (size_t i = 0; i < original->element_count; i++) {
        SysmlNode *node = original->elements[i];
        if (!node || !node->id) continue;

        if (!id_in_set(node->id, deleted_ids, deleted_count)) {
            result->elements[result->element_count++] = node;
        }
    }

    /* Pass 4: Filter relationships (remove if source OR target deleted) */
    for (size_t i = 0; i < original->relationship_count; i++) {
        SysmlRelationship *rel = original->relationships[i];
        if (!rel) continue;

        bool source_deleted = rel->source && id_in_set(rel->source, deleted_ids, deleted_count);
        bool target_deleted = rel->target && id_in_set(rel->target, deleted_ids, deleted_count);

        if (!source_deleted && !target_deleted) {
            result->relationships[result->relationship_count++] = rel;
        }
    }

    /* Pass 5: Filter imports (remove if owner scope deleted) */
    for (size_t i = 0; i < original->import_count; i++) {
        SysmlImport *imp = original->imports[i];
        if (!imp) continue;

        bool owner_deleted = imp->owner_scope && id_in_set(imp->owner_scope, deleted_ids, deleted_count);

        if (!owner_deleted) {
            result->imports[result->import_count++] = imp;
        }
    }

    return result;
}

/*
 * Remap an element ID to a new scope
 */
const char *sysml2_modify_remap_id(
    const char *original_id,
    const char *target_scope,
    Sysml2Arena *arena,
    Sysml2Intern *intern
) {
    if (!target_scope) return original_id;

    /* Top-level element (NULL or empty ID) → target scope */
    if (!original_id || *original_id == '\0') {
        return sysml2_intern(intern, target_scope);
    }

    /* Build new ID: target_scope::original_id */
    size_t target_len = strlen(target_scope);
    size_t orig_len = strlen(original_id);
    size_t new_len = target_len + 2 + orig_len;  /* :: separator */

    char *new_id = sysml2_arena_alloc(arena, new_len + 1);
    if (!new_id) return NULL;

    memcpy(new_id, target_scope, target_len);
    new_id[target_len] = ':';
    new_id[target_len + 1] = ':';
    memcpy(new_id + target_len + 2, original_id, orig_len);
    new_id[new_len] = '\0';

    return sysml2_intern(intern, new_id);
}

/*
 * Check if a scope exists in the model
 */
bool sysml2_modify_scope_exists(
    const SysmlSemanticModel *model,
    const char *scope_id
) {
    if (!model || !scope_id) return false;

    for (size_t i = 0; i < model->element_count; i++) {
        SysmlNode *node = model->elements[i];
        if (node && node->id && strcmp(node->id, scope_id) == 0) {
            return true;
        }
    }

    return false;
}

/*
 * Create a scope chain in a model
 */
SysmlSemanticModel *sysml2_modify_create_scope_chain(
    const SysmlSemanticModel *model,
    const char *scope_id,
    Sysml2Arena *arena,
    Sysml2Intern *intern
) {
    if (!model || !scope_id || !arena || !intern) return NULL;

    /* Collect scopes that need to be created */
    const char **scopes_to_create = NULL;
    size_t scope_count = 0;
    size_t scope_capacity = 0;

    /* Walk up the scope chain */
    const char *current = scope_id;
    while (current && *current) {
        if (!sysml2_modify_scope_exists(model, current)) {
            add_to_id_set(current, &scopes_to_create, &scope_count, &scope_capacity, arena);
        }
        current = sysml2_query_parent_path(current, arena);
    }

    /* If nothing to create, return a shallow clone */
    if (scope_count == 0) {
        SysmlSemanticModel *clone = sysml2_arena_alloc(arena, sizeof(SysmlSemanticModel));
        if (!clone) return NULL;

        clone->source_name = model->source_name;
        clone->element_count = model->element_count;
        clone->element_capacity = model->element_count;

        if (model->element_count > 0) {
            clone->elements = sysml2_arena_alloc(arena, model->element_count * sizeof(SysmlNode *));
            if (!clone->elements) return NULL;
            memcpy(clone->elements, model->elements, model->element_count * sizeof(SysmlNode *));
        } else {
            clone->elements = NULL;
        }

        clone->relationship_count = model->relationship_count;
        clone->relationship_capacity = model->relationship_count;
        if (model->relationship_count > 0) {
            clone->relationships = sysml2_arena_alloc(arena, model->relationship_count * sizeof(SysmlRelationship *));
            if (!clone->relationships) return NULL;
            memcpy(clone->relationships, model->relationships, model->relationship_count * sizeof(SysmlRelationship *));
        } else {
            clone->relationships = NULL;
        }

        clone->import_count = model->import_count;
        clone->import_capacity = model->import_count;
        if (model->import_count > 0) {
            clone->imports = sysml2_arena_alloc(arena, model->import_count * sizeof(SysmlImport *));
            if (!clone->imports) return NULL;
            memcpy(clone->imports, model->imports, model->import_count * sizeof(SysmlImport *));
        } else {
            clone->imports = NULL;
        }

        return clone;
    }

    /* Create new model with space for new scopes */
    size_t new_element_count = model->element_count + scope_count;
    SysmlSemanticModel *result = sysml2_arena_alloc(arena, sizeof(SysmlSemanticModel));
    if (!result) return NULL;

    result->source_name = model->source_name;
    result->element_count = 0;
    result->element_capacity = new_element_count;
    result->elements = sysml2_arena_alloc(arena, new_element_count * sizeof(SysmlNode *));
    if (!result->elements) return NULL;

    /* Copy existing elements */
    for (size_t i = 0; i < model->element_count; i++) {
        result->elements[result->element_count++] = model->elements[i];
    }

    /* Create new scope packages (in reverse order so parents come first) */
    for (size_t i = scope_count; i > 0; i--) {
        const char *scope = scopes_to_create[i - 1];

        SysmlNode *node = sysml2_arena_alloc(arena, sizeof(SysmlNode));
        if (!node) return NULL;

        memset(node, 0, sizeof(SysmlNode));
        node->id = sysml2_intern(intern, scope);
        node->name = sysml2_intern(intern, sysml2_modify_get_local_name(scope));
        node->kind = SYSML_KIND_PACKAGE;
        node->parent_id = sysml2_query_parent_path(scope, arena);
        if (node->parent_id) {
            node->parent_id = sysml2_intern(intern, node->parent_id);
        }

        result->elements[result->element_count++] = node;
    }

    /* Copy relationships */
    result->relationship_count = model->relationship_count;
    result->relationship_capacity = model->relationship_count;
    if (model->relationship_count > 0) {
        result->relationships = sysml2_arena_alloc(arena, model->relationship_count * sizeof(SysmlRelationship *));
        if (!result->relationships) return NULL;
        memcpy(result->relationships, model->relationships, model->relationship_count * sizeof(SysmlRelationship *));
    } else {
        result->relationships = NULL;
    }

    /* Copy imports */
    result->import_count = model->import_count;
    result->import_capacity = model->import_count;
    if (model->import_count > 0) {
        result->imports = sysml2_arena_alloc(arena, model->import_count * sizeof(SysmlImport *));
        if (!result->imports) return NULL;
        memcpy(result->imports, model->imports, model->import_count * sizeof(SysmlImport *));
    } else {
        result->imports = NULL;
    }

    return result;
}

/*
 * Deep copy a metadata feature
 */
static SysmlMetadataFeature *sysml2_modify_copy_metadata_feature(
    SysmlMetadataFeature *src,
    Sysml2Arena *arena
) {
    if (!src) return NULL;

    SysmlMetadataFeature *dst = sysml2_arena_alloc(arena, sizeof(SysmlMetadataFeature));
    if (!dst) return NULL;

    /* Strings are interned - just copy pointers */
    dst->name = src->name;
    dst->value = src->value;

    return dst;
}

/*
 * Deep copy a metadata usage
 */
static SysmlMetadataUsage *sysml2_modify_copy_metadata_usage(
    SysmlMetadataUsage *src,
    Sysml2Arena *arena
) {
    if (!src) return NULL;

    SysmlMetadataUsage *dst = sysml2_arena_alloc(arena, sizeof(SysmlMetadataUsage));
    if (!dst) return NULL;

    /* Copy scalars and interned strings */
    dst->type_ref = src->type_ref;
    dst->about_count = src->about_count;
    dst->feature_count = src->feature_count;

    /* Deep copy about array */
    if (src->about_count > 0 && src->about) {
        dst->about = sysml2_arena_alloc(arena, src->about_count * sizeof(const char *));
        if (!dst->about) return NULL;
        memcpy(dst->about, src->about, src->about_count * sizeof(const char *));
    } else {
        dst->about = NULL;
    }

    /* Deep copy features array */
    if (src->feature_count > 0 && src->features) {
        dst->features = sysml2_arena_alloc(arena, src->feature_count * sizeof(SysmlMetadataFeature *));
        if (!dst->features) return NULL;
        for (size_t i = 0; i < src->feature_count; i++) {
            dst->features[i] = sysml2_modify_copy_metadata_feature(src->features[i], arena);
        }
    } else {
        dst->features = NULL;
    }

    return dst;
}

/*
 * Deep copy a connector end (struct, not pointer)
 */
static void sysml2_modify_copy_connector_end(
    SysmlConnectorEnd *dst,
    const SysmlConnectorEnd *src
) {
    /* Interned strings - just copy pointers */
    dst->target = src->target;
    dst->feature_chain = src->feature_chain;
    dst->multiplicity = src->multiplicity;
}

/*
 * Deep copy a statement (forward declaration for recursion)
 */
static SysmlStatement *sysml2_modify_copy_statement(
    SysmlStatement *src,
    Sysml2Arena *arena
);

/*
 * Deep copy a statement
 */
static SysmlStatement *sysml2_modify_copy_statement(
    SysmlStatement *src,
    Sysml2Arena *arena
) {
    if (!src) return NULL;

    SysmlStatement *dst = sysml2_arena_alloc(arena, sizeof(SysmlStatement));
    if (!dst) return NULL;

    /* Copy scalars */
    dst->kind = src->kind;
    dst->loc = src->loc;
    dst->nested_count = src->nested_count;

    /* Copy interned strings */
    dst->raw_text = src->raw_text;
    dst->name = src->name;
    dst->guard = src->guard;
    dst->payload = src->payload;

    /* Copy connector ends (embedded structs) */
    sysml2_modify_copy_connector_end(&dst->source, &src->source);
    sysml2_modify_copy_connector_end(&dst->target, &src->target);

    /* Deep copy nested statements */
    if (src->nested_count > 0 && src->nested) {
        dst->nested = sysml2_arena_alloc(arena, src->nested_count * sizeof(SysmlStatement *));
        if (!dst->nested) return NULL;
        for (size_t i = 0; i < src->nested_count; i++) {
            dst->nested[i] = sysml2_modify_copy_statement(src->nested[i], arena);
        }
    } else {
        dst->nested = NULL;
    }

    return dst;
}

/*
 * Deep copy a trivia linked list
 */
static SysmlTrivia *sysml2_modify_copy_trivia(
    SysmlTrivia *src,
    Sysml2Arena *arena
) {
    if (!src) return NULL;

    SysmlTrivia *dst = sysml2_arena_alloc(arena, sizeof(SysmlTrivia));
    if (!dst) return NULL;

    dst->kind = src->kind;
    dst->text = src->text;  /* Interned */
    dst->loc = src->loc;

    /* Recursively copy next in linked list */
    dst->next = sysml2_modify_copy_trivia(src->next, arena);

    return dst;
}

/*
 * Deep copy a named comment
 */
static SysmlNamedComment *sysml2_modify_copy_named_comment(
    SysmlNamedComment *src,
    Sysml2Arena *arena
) {
    if (!src) return NULL;

    SysmlNamedComment *dst = sysml2_arena_alloc(arena, sizeof(SysmlNamedComment));
    if (!dst) return NULL;

    /* Copy scalars and interned strings */
    dst->id = src->id;
    dst->name = src->name;
    dst->locale = src->locale;
    dst->text = src->text;
    dst->loc = src->loc;
    dst->about_count = src->about_count;

    /* Deep copy about array */
    if (src->about_count > 0 && src->about) {
        dst->about = sysml2_arena_alloc(arena, src->about_count * sizeof(const char *));
        if (!dst->about) return NULL;
        memcpy(dst->about, src->about, src->about_count * sizeof(const char *));
    } else {
        dst->about = NULL;
    }

    return dst;
}

/*
 * Deep copy a textual representation
 */
static SysmlTextualRep *sysml2_modify_copy_textual_rep(
    SysmlTextualRep *src,
    Sysml2Arena *arena
) {
    if (!src) return NULL;

    SysmlTextualRep *dst = sysml2_arena_alloc(arena, sizeof(SysmlTextualRep));
    if (!dst) return NULL;

    /* All fields are interned strings or scalars */
    dst->id = src->id;
    dst->name = src->name;
    dst->language = src->language;
    dst->text = src->text;
    dst->loc = src->loc;

    return dst;
}

/*
 * Deep copy a node with all its pointer arrays
 */
static SysmlNode *sysml2_modify_deep_copy_node(
    SysmlNode *src,
    const char *target_scope,
    Sysml2Arena *arena,
    Sysml2Intern *intern
) {
    if (!src) return NULL;

    SysmlNode *dst = sysml2_arena_alloc(arena, sizeof(SysmlNode));
    if (!dst) return NULL;

    /* Start with shallow copy for scalar fields */
    memcpy(dst, src, sizeof(SysmlNode));

    /* Remap IDs */
    dst->id = sysml2_modify_remap_id(src->id, target_scope, arena, intern);
    if (src->parent_id) {
        dst->parent_id = sysml2_modify_remap_id(src->parent_id, target_scope, arena, intern);
    } else {
        /* Top-level fragment element → parent is target scope */
        dst->parent_id = sysml2_intern(intern, target_scope);
    }

    /* Deep copy typed_by array */
    if (src->typed_by_count > 0 && src->typed_by) {
        dst->typed_by = sysml2_arena_alloc(arena, src->typed_by_count * sizeof(const char *));
        if (!dst->typed_by) return NULL;
        memcpy(dst->typed_by, src->typed_by, src->typed_by_count * sizeof(const char *));
    }

    /* Deep copy specializes array */
    if (src->specializes_count > 0 && src->specializes) {
        dst->specializes = sysml2_arena_alloc(arena, src->specializes_count * sizeof(const char *));
        if (!dst->specializes) return NULL;
        memcpy(dst->specializes, src->specializes, src->specializes_count * sizeof(const char *));
    }

    /* Deep copy redefines array */
    if (src->redefines_count > 0 && src->redefines) {
        dst->redefines = sysml2_arena_alloc(arena, src->redefines_count * sizeof(const char *));
        if (!dst->redefines) return NULL;
        memcpy(dst->redefines, src->redefines, src->redefines_count * sizeof(const char *));
    }

    /* Deep copy references array */
    if (src->references_count > 0 && src->references) {
        dst->references = sysml2_arena_alloc(arena, src->references_count * sizeof(const char *));
        if (!dst->references) return NULL;
        memcpy(dst->references, src->references, src->references_count * sizeof(const char *));
    }

    /* Deep copy metadata array */
    if (src->metadata_count > 0 && src->metadata) {
        dst->metadata = sysml2_arena_alloc(arena, src->metadata_count * sizeof(SysmlMetadataUsage *));
        if (!dst->metadata) return NULL;
        for (size_t i = 0; i < src->metadata_count; i++) {
            dst->metadata[i] = sysml2_modify_copy_metadata_usage(src->metadata[i], arena);
        }
    }

    /* Deep copy prefix_metadata array (interned strings) */
    if (src->prefix_metadata_count > 0 && src->prefix_metadata) {
        dst->prefix_metadata = sysml2_arena_alloc(arena, src->prefix_metadata_count * sizeof(const char *));
        if (!dst->prefix_metadata) return NULL;
        memcpy(dst->prefix_metadata, src->prefix_metadata, src->prefix_metadata_count * sizeof(const char *));
    }

    /* Deep copy prefix_applied_metadata array */
    if (src->prefix_applied_metadata_count > 0 && src->prefix_applied_metadata) {
        dst->prefix_applied_metadata = sysml2_arena_alloc(arena, src->prefix_applied_metadata_count * sizeof(SysmlMetadataUsage *));
        if (!dst->prefix_applied_metadata) return NULL;
        for (size_t i = 0; i < src->prefix_applied_metadata_count; i++) {
            dst->prefix_applied_metadata[i] = sysml2_modify_copy_metadata_usage(src->prefix_applied_metadata[i], arena);
        }
    }

    /* Deep copy trivia linked lists */
    dst->leading_trivia = sysml2_modify_copy_trivia(src->leading_trivia, arena);
    dst->trailing_trivia = sysml2_modify_copy_trivia(src->trailing_trivia, arena);

    /* Deep copy body_stmts array */
    if (src->body_stmt_count > 0 && src->body_stmts) {
        dst->body_stmts = sysml2_arena_alloc(arena, src->body_stmt_count * sizeof(SysmlStatement *));
        if (!dst->body_stmts) return NULL;
        for (size_t i = 0; i < src->body_stmt_count; i++) {
            dst->body_stmts[i] = sysml2_modify_copy_statement(src->body_stmts[i], arena);
        }
    }

    /* Deep copy comments array */
    if (src->comment_count > 0 && src->comments) {
        dst->comments = sysml2_arena_alloc(arena, src->comment_count * sizeof(SysmlNamedComment *));
        if (!dst->comments) return NULL;
        for (size_t i = 0; i < src->comment_count; i++) {
            dst->comments[i] = sysml2_modify_copy_named_comment(src->comments[i], arena);
        }
    }

    /* Deep copy textual_reps array */
    if (src->textual_rep_count > 0 && src->textual_reps) {
        dst->textual_reps = sysml2_arena_alloc(arena, src->textual_rep_count * sizeof(SysmlTextualRep *));
        if (!dst->textual_reps) return NULL;
        for (size_t i = 0; i < src->textual_rep_count; i++) {
            dst->textual_reps[i] = sysml2_modify_copy_textual_rep(src->textual_reps[i], arena);
        }
    }

    return dst;
}

/*
 * Merge a fragment into a model at the specified scope
 */
SysmlSemanticModel *sysml2_modify_merge_fragment(
    const SysmlSemanticModel *base,
    const SysmlSemanticModel *fragment,
    const char *target_scope,
    bool create_scope,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    size_t *out_added_count,
    size_t *out_replaced_count
) {
    if (!base || !fragment || !target_scope || !arena || !intern) {
        if (out_added_count) *out_added_count = 0;
        if (out_replaced_count) *out_replaced_count = 0;
        return NULL;
    }

    size_t added = 0;
    size_t replaced = 0;

    /* Step 1: Check if target scope exists (or create it) */
    SysmlSemanticModel *working_base = (SysmlSemanticModel *)base;
    if (!sysml2_modify_scope_exists(base, target_scope)) {
        if (!create_scope) {
            /* Target scope doesn't exist and we can't create it */
            if (out_added_count) *out_added_count = 0;
            if (out_replaced_count) *out_replaced_count = 0;
            return NULL;
        }
        working_base = sysml2_modify_create_scope_chain(base, target_scope, arena, intern);
        if (!working_base) return NULL;
    }

    /* Step 2: Build mapping of remapped IDs */
    /* Collect IDs that will be replaced */
    const char **replaced_ids = NULL;
    size_t replaced_count = 0;
    size_t replaced_capacity = 0;

    for (size_t i = 0; i < fragment->element_count; i++) {
        SysmlNode *frag_node = fragment->elements[i];
        if (!frag_node) continue;

        /* Compute the remapped ID */
        const char *new_id = sysml2_modify_remap_id(frag_node->id, target_scope, arena, intern);
        if (!new_id) continue;

        /* Debug logging to trace element matching */
        if (getenv("SYSML2_DEBUG_MODIFY")) {
            bool exists = sysml2_modify_scope_exists(working_base, new_id);
            fprintf(stderr, "DEBUG: Fragment '%s' -> remapped '%s', exists=%d\n",
                    frag_node->id ? frag_node->id : "(null)",
                    new_id,
                    exists);
        }

        /* Check if this ID exists in the base */
        if (sysml2_modify_scope_exists(working_base, new_id)) {
            add_to_id_set(new_id, &replaced_ids, &replaced_count, &replaced_capacity, arena);
        }
    }

    /* Collect IDs to remove (only the replaced elements themselves, NOT children)
     *
     * IMPORTANT: We intentionally do NOT cascade deletion to children here.
     * The replacement element will have the same ID as the replaced element,
     * so existing children will naturally become children of the new element.
     * This preserves accumulated content from previous upsert operations.
     *
     * Old behavior: Cascade deletion wiped all children, causing data loss
     * when a replacement fragment didn't include all previously-added children.
     */
    const char **ids_to_remove = NULL;
    size_t remove_count = 0;
    size_t remove_capacity = 0;

    for (size_t i = 0; i < replaced_count; i++) {
        add_to_id_set(replaced_ids[i], &ids_to_remove, &remove_count, &remove_capacity, arena);
    }

    /* NOTE: Cascade deletion to children removed to prevent data loss.
     * Children are preserved and inherit the replacement element as parent. */

    /* Step 3: Allocate new model */
    size_t max_elements = working_base->element_count + fragment->element_count;
    size_t max_relationships = working_base->relationship_count + fragment->relationship_count;
    size_t max_imports = working_base->import_count + fragment->import_count;

    SysmlSemanticModel *result = sysml2_arena_alloc(arena, sizeof(SysmlSemanticModel));
    if (!result) return NULL;

    result->source_name = working_base->source_name;
    result->element_count = 0;
    result->element_capacity = max_elements;
    result->relationship_count = 0;
    result->relationship_capacity = max_relationships;
    result->import_count = 0;
    result->import_capacity = max_imports;

    result->elements = sysml2_arena_alloc(arena, max_elements * sizeof(SysmlNode *));
    if (!result->elements) return NULL;

    result->relationships = sysml2_arena_alloc(arena, max_relationships * sizeof(SysmlRelationship *));
    if (!result->relationships) return NULL;

    result->imports = sysml2_arena_alloc(arena, max_imports * sizeof(SysmlImport *));
    if (!result->imports) return NULL;

    /* Step 4: Copy non-removed base elements */
    for (size_t i = 0; i < working_base->element_count; i++) {
        SysmlNode *node = working_base->elements[i];
        if (!node || !node->id) continue;

        if (!id_in_set(node->id, ids_to_remove, remove_count)) {
            result->elements[result->element_count++] = node;
        }
    }

    /* Step 5: Add remapped fragment elements (deep copy to avoid pointer aliasing) */
    for (size_t i = 0; i < fragment->element_count; i++) {
        SysmlNode *frag_node = fragment->elements[i];
        if (!frag_node) continue;

        /* Deep copy node with all pointer arrays and remapped IDs */
        SysmlNode *new_node = sysml2_modify_deep_copy_node(frag_node, target_scope, arena, intern);
        if (!new_node) return NULL;

        /* Check if this was a replacement */
        if (id_in_set(new_node->id, replaced_ids, replaced_count)) {
            replaced++;
        } else {
            added++;
        }

        result->elements[result->element_count++] = new_node;
    }

    /* Step 6: Copy non-affected relationships from base */
    for (size_t i = 0; i < working_base->relationship_count; i++) {
        SysmlRelationship *rel = working_base->relationships[i];
        if (!rel) continue;

        bool source_removed = rel->source && id_in_set(rel->source, ids_to_remove, remove_count);
        bool target_removed = rel->target && id_in_set(rel->target, ids_to_remove, remove_count);

        if (!source_removed && !target_removed) {
            result->relationships[result->relationship_count++] = rel;
        }
    }

    /* Step 7: Add remapped fragment relationships */
    for (size_t i = 0; i < fragment->relationship_count; i++) {
        SysmlRelationship *frag_rel = fragment->relationships[i];
        if (!frag_rel) continue;

        /* Create new relationship with remapped IDs */
        SysmlRelationship *new_rel = sysml2_arena_alloc(arena, sizeof(SysmlRelationship));
        if (!new_rel) return NULL;

        memcpy(new_rel, frag_rel, sizeof(SysmlRelationship));

        if (frag_rel->id) {
            new_rel->id = sysml2_modify_remap_id(frag_rel->id, target_scope, arena, intern);
        }
        if (frag_rel->source) {
            new_rel->source = sysml2_modify_remap_id(frag_rel->source, target_scope, arena, intern);
        }
        if (frag_rel->target) {
            new_rel->target = sysml2_modify_remap_id(frag_rel->target, target_scope, arena, intern);
        }

        result->relationships[result->relationship_count++] = new_rel;
    }

    /* Step 8: Copy non-affected imports from base */
    for (size_t i = 0; i < working_base->import_count; i++) {
        SysmlImport *imp = working_base->imports[i];
        if (!imp) continue;

        bool owner_removed = imp->owner_scope && id_in_set(imp->owner_scope, ids_to_remove, remove_count);

        if (!owner_removed) {
            result->imports[result->import_count++] = imp;
        }
    }

    /* Step 9: Add remapped fragment imports */
    for (size_t i = 0; i < fragment->import_count; i++) {
        SysmlImport *frag_imp = fragment->imports[i];
        if (!frag_imp) continue;

        /* Create new import with remapped owner scope */
        SysmlImport *new_imp = sysml2_arena_alloc(arena, sizeof(SysmlImport));
        if (!new_imp) return NULL;

        memcpy(new_imp, frag_imp, sizeof(SysmlImport));

        if (frag_imp->id) {
            new_imp->id = sysml2_modify_remap_id(frag_imp->id, target_scope, arena, intern);
        }
        if (frag_imp->owner_scope) {
            new_imp->owner_scope = sysml2_modify_remap_id(frag_imp->owner_scope, target_scope, arena, intern);
        }
        /* Note: target is NOT remapped - it refers to external elements */

        result->imports[result->import_count++] = new_imp;
    }

    if (out_added_count) *out_added_count = added;
    if (out_replaced_count) *out_replaced_count = replaced;

    return result;
}

/*
 * Find which file contains an element
 */
int sysml2_modify_find_containing_file(
    const char *element_id,
    SysmlSemanticModel **models,
    size_t model_count
) {
    if (!element_id || !models) return -1;

    for (size_t m = 0; m < model_count; m++) {
        SysmlSemanticModel *model = models[m];
        if (!model) continue;

        for (size_t i = 0; i < model->element_count; i++) {
            SysmlNode *node = model->elements[i];
            if (node && node->id && strcmp(node->id, element_id) == 0) {
                return (int)m;
            }
        }
    }

    return -1;
}
