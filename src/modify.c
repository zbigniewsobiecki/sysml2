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
 * Extract the feature name from a shorthand statement's raw_text.
 *
 * Shorthand statements have formats like:
 *   :> name : Type;
 *   :>> name = value;
 *   :>> name : Type;
 *   :>> name;
 *
 * Returns the name part (interned), or NULL if not parseable.
 */
static const char *sysml2_extract_shorthand_stmt_name(
    const char *raw_text,
    Sysml2Arena *arena,
    Sysml2Intern *intern
) {
    if (!raw_text || !arena || !intern) return NULL;

    const char *p = raw_text;

    /* Skip leading whitespace */
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

    /* Must start with :> or :>> */
    if (*p != ':') return NULL;
    p++;
    if (*p != '>') return NULL;
    p++;
    /* Optional second > for :>> */
    if (*p == '>') p++;

    /* Skip whitespace after :> or :>> */
    while (*p && (*p == ' ' || *p == '\t')) p++;

    /* Read the name (up to whitespace, =, :, ;, or end) */
    const char *name_start = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '=' && *p != ':' && *p != ';' && *p != '\n') {
        p++;
    }

    if (p == name_start) return NULL;  /* Empty name */

    /* Intern the name */
    size_t name_len = (size_t)(p - name_start);
    char *name_buf = sysml2_arena_alloc(arena, name_len + 1);
    if (!name_buf) return NULL;
    memcpy(name_buf, name_start, name_len);
    name_buf[name_len] = '\0';

    return sysml2_intern(intern, name_buf);
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

    /* Deep copy typed_by_conjugated array */
    if (src->typed_by_count > 0 && src->typed_by_conjugated) {
        dst->typed_by_conjugated = sysml2_arena_alloc(arena, src->typed_by_count * sizeof(bool));
        if (!dst->typed_by_conjugated) return NULL;
        memcpy(dst->typed_by_conjugated, src->typed_by_conjugated, src->typed_by_count * sizeof(bool));
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

        /* Fix comment duplication: if trailing_trivia text appears in any body_stmt's
         * raw_text, the parser has double-captured it. Clear the duplicate trivia.
         * This commonly happens with trailing line comments on shorthand features. */
        if (dst->trailing_trivia && dst->trailing_trivia->text) {
            const char *trivia_text = dst->trailing_trivia->text;
            for (size_t i = 0; i < dst->body_stmt_count; i++) {
                if (dst->body_stmts[i] && dst->body_stmts[i]->raw_text &&
                    strstr(dst->body_stmts[i]->raw_text, trivia_text)) {
                    /* Trivia is already in a body statement - clear the duplicate */
                    dst->trailing_trivia = NULL;
                    break;
                }
            }
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
    bool replace_scope,
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

    /* Step 0: Auto-unwrap scope-matching package wraps (common LLM mistake)
     *
     * If the fragment contains a single top-level package with the same name as
     * the target scope, strip the wrapper prefix from all element IDs and remove
     * the wrapper. This handles the common LLM pattern:
     *   --set "package Foo { item def X; }" --at Foo
     *
     * Without unwrapping: X gets ID "Foo::Foo::X" (wrong - nested)
     * With unwrapping:    X gets ID "Foo::X" (correct)
     */
    SysmlNode *wrapper_to_unwrap = NULL;
    size_t top_level_count = 0;

    for (size_t i = 0; i < fragment->element_count; i++) {
        SysmlNode *node = fragment->elements[i];
        if (!node) continue;

        /* Check if this is a top-level element (no parent) */
        if (!node->parent_id || *node->parent_id == '\0') {
            top_level_count++;

            /* Check if it's a package matching target scope */
            if (node->kind == SYSML_KIND_PACKAGE && node->name) {
                const char *target_local = sysml2_modify_get_local_name(target_scope);
                if (target_local && strcmp(node->name, target_local) == 0) {
                    wrapper_to_unwrap = node;
                }
            }
        }
    }

    /* If there's exactly one top-level element and it's a scope-matching package,
     * strip its prefix from all IDs and remove it from the fragment */
    if (wrapper_to_unwrap && top_level_count == 1) {
        const char *wrapper_id = wrapper_to_unwrap->id;
        size_t wrapper_id_len = wrapper_id ? strlen(wrapper_id) : 0;

        if (getenv("SYSML2_DEBUG_MODIFY")) {
            fprintf(stderr, "DEBUG: Auto-unwrapping package '%s' (id='%s')\n",
                    wrapper_to_unwrap->name, wrapper_id ? wrapper_id : "(null)");
        }

        /* Strip wrapper prefix from all element IDs and parent_ids.
         * E.g., "Foo::X" becomes "X", "Foo::X::y" becomes "X::y" */
        for (size_t i = 0; i < fragment->element_count; i++) {
            SysmlNode *node = fragment->elements[i];
            if (!node || node == wrapper_to_unwrap) continue;

            /* Strip prefix from ID */
            if (node->id && wrapper_id && sysml2_modify_id_starts_with(node->id, wrapper_id)) {
                /* Skip "Foo::" prefix (wrapper_id_len + 2 for "::") */
                node->id = sysml2_intern(intern, node->id + wrapper_id_len + 2);
            }

            /* Strip prefix from parent_id, or set to NULL if parent is wrapper */
            if (node->parent_id) {
                if (strcmp(node->parent_id, wrapper_id) == 0) {
                    /* Direct child of wrapper - becomes top-level */
                    node->parent_id = NULL;
                } else if (sysml2_modify_id_starts_with(node->parent_id, wrapper_id)) {
                    /* Grandchild - strip prefix */
                    node->parent_id = sysml2_intern(intern, node->parent_id + wrapper_id_len + 2);
                }
            }
        }

        /* Strip wrapper prefix from import owner_scopes too */
        for (size_t i = 0; i < fragment->import_count; i++) {
            SysmlImport *imp = fragment->imports[i];
            if (!imp || !imp->owner_scope) continue;

            if (strcmp(imp->owner_scope, wrapper_id) == 0) {
                /* Import owned by wrapper - becomes top-level (owner_scope = NULL) */
                imp->owner_scope = NULL;
            } else if (sysml2_modify_id_starts_with(imp->owner_scope, wrapper_id)) {
                /* Import owned by nested element - strip prefix */
                imp->owner_scope = sysml2_intern(intern, imp->owner_scope + wrapper_id_len + 2);
            }
        }

        /* Remove the wrapper package from fragment (cast away const for this) */
        SysmlSemanticModel *mutable_fragment = (SysmlSemanticModel *)fragment;
        for (size_t i = 0; i < mutable_fragment->element_count; i++) {
            if (mutable_fragment->elements[i] == wrapper_to_unwrap) {
                mutable_fragment->elements[i] = NULL;
                break;
            }
        }
    }

    /* Capture wrapper documentation and metadata before unwrapping.
     * These will be applied to the target scope if fragment has scope metadata. */
    const char *wrapper_doc = NULL;
    SysmlMetadataUsage **wrapper_metadata = NULL;
    size_t wrapper_metadata_count = 0;
    SysmlMetadataUsage **wrapper_prefix_metadata = NULL;
    size_t wrapper_prefix_metadata_count = 0;

    if (wrapper_to_unwrap) {
        wrapper_doc = wrapper_to_unwrap->documentation;
        wrapper_metadata = wrapper_to_unwrap->metadata;
        wrapper_metadata_count = wrapper_to_unwrap->metadata_count;
        wrapper_prefix_metadata = wrapper_to_unwrap->prefix_applied_metadata;
        wrapper_prefix_metadata_count = wrapper_to_unwrap->prefix_applied_metadata_count;
    }

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

    /* HYBRID APPROACH: Replace matching elements, ADD new ones, keep others.
     *
     * When merging a fragment at a target scope, we use "hybrid" semantics:
     *   1. Named elements: Replace if same ID exists in fragment, ADD if new
     *   2. Keep existing elements that are NOT being replaced
     *   3. For replaced elements: also remove their direct children (one level)
     *
     * This approach:
     *   - Supports incremental writes (add one element at a time)
     *   - Prevents duplicate accumulation (replaced elements are removed first)
     *   - Preserves content not being modified
     *
     * @SourceFile deduplication is handled in Step 5 when adding fragment elements.
     */
    const char **ids_to_remove = NULL;
    size_t remove_count = 0;
    size_t remove_capacity = 0;

    /* Step 1.5: If replace_scope is set, mark ALL direct children for removal.
     * This clears the scope so fragment elements preserve their order.
     * The target scope itself is NOT removed, only its direct children.
     * Also cascade to grandchildren by adding to cascade tracking.
     */
    const char **replace_scope_removed = NULL;
    size_t replace_scope_count = 0;
    size_t replace_scope_capacity = 0;

    if (replace_scope) {
        for (size_t i = 0; i < working_base->element_count; i++) {
            SysmlNode *node = working_base->elements[i];
            if (!node || !node->id || !node->parent_id) continue;

            /* Mark direct children of target scope for removal */
            if (strcmp(node->parent_id, target_scope) == 0) {
                add_to_id_set(node->id, &ids_to_remove, &remove_count, &remove_capacity, arena);
                add_to_id_set(node->id, &replace_scope_removed, &replace_scope_count, &replace_scope_capacity, arena);
            }
        }

        if (getenv("SYSML2_DEBUG_MODIFY")) {
            fprintf(stderr, "DEBUG: replace_scope=true, marked %zu direct children for removal\n",
                    replace_scope_count);
        }
    }

    /* Step 2a: Only remove elements that are being REPLACED (matched by ID).
     *
     * Children of replaced elements are preserved unless they are also being
     * replaced (i.e., the fragment has an element with the same remapped ID).
     * This enables incremental updates: replacing a parent while keeping
     * existing children that aren't in the fragment.
     *
     * We track replaced IDs separately from deleted IDs because replaced
     * elements should NOT cascade deletion to their children.
     */
    for (size_t i = 0; i < replaced_count; i++) {
        add_to_id_set(replaced_ids[i], &ids_to_remove, &remove_count, &remove_capacity, arena);
    }

    /* Step 2b: Only remove children that are also being replaced by fragment.
     *
     * Check each child of a replaced element: if the fragment has a matching
     * element (same local name under the same parent), remove the base child.
     * This prevents duplicates while preserving children not in the fragment.
     *
     * We track these separately for cascade purposes.
     */
    const char **children_to_remove = NULL;
    size_t children_remove_count = 0;
    size_t children_remove_capacity = 0;

    for (size_t i = 0; i < replaced_count; i++) {
        for (size_t j = 0; j < working_base->element_count; j++) {
            SysmlNode *node = working_base->elements[j];
            if (!node || !node->id || !node->parent_id) continue;
            if (strcmp(node->parent_id, replaced_ids[i]) != 0) continue;

            /* Check if fragment has a child with the same name under the
             * corresponding parent. The fragment parent has a shorter ID
             * (without target_scope prefix). */
            const char *child_name = node->name;
            if (!child_name) continue;

            /* Find the fragment element corresponding to replaced_ids[i] */
            for (size_t k = 0; k < fragment->element_count; k++) {
                SysmlNode *frag_elem = fragment->elements[k];
                if (!frag_elem) continue;

                /* Check if this fragment element's remapped ID matches replaced_ids[i] */
                const char *frag_remapped_id = sysml2_modify_remap_id(
                    frag_elem->id, target_scope, arena, intern);
                if (!frag_remapped_id || strcmp(frag_remapped_id, replaced_ids[i]) != 0)
                    continue;

                /* Found the fragment parent. Now check its children in fragment. */
                for (size_t m = 0; m < fragment->element_count; m++) {
                    SysmlNode *frag_child = fragment->elements[m];
                    if (!frag_child || !frag_child->parent_id) continue;
                    if (strcmp(frag_child->parent_id, frag_elem->id) != 0) continue;

                    /* Fragment has a child of the replaced element */
                    if (frag_child->name && strcmp(frag_child->name, child_name) == 0) {
                        /* This base child will be replaced by fragment child */
                        add_to_id_set(node->id, &ids_to_remove, &remove_count, &remove_capacity, arena);
                        add_to_id_set(node->id, &children_to_remove, &children_remove_count, &children_remove_capacity, arena);
                        break;
                    }
                }
                break;
            }
        }
    }

    /* Step 2c: Cascade deletion from explicitly matched children and replace_scope elements.
     *
     * Replaced parents preserve their children (unless also matched by name).
     * Only children that are being replaced should cascade to their descendants.
     * When replace_scope is used, ALL descendants of removed elements are also removed.
     */
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t j = 0; j < working_base->element_count; j++) {
            SysmlNode *node = working_base->elements[j];
            if (!node || !node->id) continue;
            if (id_in_set(node->id, ids_to_remove, remove_count)) continue;

            /* Cascade from children_to_remove (matched by name) */
            if (node->parent_id && id_in_set(node->parent_id, children_to_remove, children_remove_count)) {
                add_to_id_set(node->id, &ids_to_remove, &remove_count, &remove_capacity, arena);
                add_to_id_set(node->id, &children_to_remove, &children_remove_count, &children_remove_capacity, arena);
                changed = true;
            }
            /* Also cascade from replace_scope removed elements */
            else if (node->parent_id && id_in_set(node->parent_id, replace_scope_removed, replace_scope_count)) {
                add_to_id_set(node->id, &ids_to_remove, &remove_count, &remove_capacity, arena);
                add_to_id_set(node->id, &replace_scope_removed, &replace_scope_count, &replace_scope_capacity, arena);
                changed = true;
            }
        }
    }

    /* Debug logging */
    if (getenv("SYSML2_DEBUG_MODIFY")) {
        fprintf(stderr, "DEBUG: Hybrid mode at scope '%s': removing %zu elements (replaced: %zu)\n",
                target_scope, remove_count, replaced_count);
    }

    /* Step 3: Allocate new model */
    size_t max_elements = working_base->element_count + fragment->element_count;
    size_t max_relationships = working_base->relationship_count + fragment->relationship_count;
    size_t max_imports = working_base->import_count + fragment->import_count;

    SysmlSemanticModel *result = sysml2_arena_alloc(arena, sizeof(SysmlSemanticModel));
    if (!result) return NULL;
    memset(result, 0, sizeof(SysmlSemanticModel));  /* Zero all fields */

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

    /* Check if fragment provides top-level scope metadata.
     * Top-level fragment elements have NULL parent_id (they are direct children of the scope).
     * Only clear target scope metadata if the fragment explicitly provides replacement metadata.
     */
    bool fragment_has_scope_metadata = false;
    for (size_t j = 0; j < fragment->element_count; j++) {
        SysmlNode *fnode = fragment->elements[j];
        if (fnode && fnode->parent_id == NULL) {
            if (fnode->prefix_applied_metadata_count > 0 ||
                fnode->metadata_count > 0) {
                fragment_has_scope_metadata = true;
                break;
            }
        }
    }

    /* Track which fragment elements have been processed (for in-place replacement) */
    bool *fragment_processed = sysml2_arena_alloc(arena,
        fragment->element_count * sizeof(bool));
    if (!fragment_processed && fragment->element_count > 0) return NULL;
    for (size_t i = 0; i < fragment->element_count; i++) {
        fragment_processed[i] = false;
    }

    /* Step 4: Copy base elements, replacing updated elements in-place
     *
     * HYBRID MODE: Preserve all elements that weren't marked for removal.
     * For replaced elements, insert the fragment's version at the same position
     * to preserve element ordering. This is critical because the writer sorts
     * body elements by offset, and when all offsets are 0, it falls back to
     * insertion order (i.e., array position).
     */
    for (size_t i = 0; i < working_base->element_count; i++) {
        SysmlNode *node = working_base->elements[i];
        if (!node || !node->id) continue;

        /* Check if this is a replaced element (not just removed) */
        if (id_in_set(node->id, replaced_ids, replaced_count)) {
            /* When replace_scope is true and this is a direct child of the target scope,
             * skip in-place replacement. Let Step 5 add fragment elements in fragment order
             * to preserve the order specified in the fragment. */
            if (replace_scope && node->parent_id && strcmp(node->parent_id, target_scope) == 0) {
                continue;  /* Skip to next base element, Step 5 will add in fragment order */
            }

            /* Find and process the corresponding fragment element in-place */
            for (size_t fi = 0; fi < fragment->element_count; fi++) {
                if (fragment_processed[fi]) continue;
                SysmlNode *frag_node = fragment->elements[fi];
                if (!frag_node) continue;

                /* Check if this fragment element replaces the current base element */
                const char *remapped_id = sysml2_modify_remap_id(
                    frag_node->id, target_scope, arena, intern);
                if (!remapped_id || strcmp(remapped_id, node->id) != 0) continue;

                /* Found the replacement - process it in-place */
                fragment_processed[fi] = true;
                replaced++;

                SysmlNode *new_node = sysml2_modify_deep_copy_node(
                    frag_node, target_scope, arena, intern);
                if (!new_node) return NULL;

                /* Inherit original location to preserve element ordering */
                new_node->loc = node->loc;

                /* Preserve documentation if fragment has none */
                if (!new_node->documentation && node->documentation) {
                    new_node->documentation = node->documentation;
                }

                /* Preserve prefix_applied_metadata if fragment has none */
                if (new_node->prefix_applied_metadata_count == 0 &&
                    node->prefix_applied_metadata_count > 0) {
                    new_node->prefix_applied_metadata = sysml2_arena_alloc(
                        arena, node->prefix_applied_metadata_count * sizeof(SysmlMetadataUsage *));
                    if (new_node->prefix_applied_metadata) {
                        for (size_t k = 0; k < node->prefix_applied_metadata_count; k++) {
                            new_node->prefix_applied_metadata[k] =
                                sysml2_modify_copy_metadata_usage(node->prefix_applied_metadata[k], arena);
                        }
                        new_node->prefix_applied_metadata_count = node->prefix_applied_metadata_count;
                    }
                }

                /* Preserve body metadata if fragment has none */
                if (new_node->metadata_count == 0 && node->metadata_count > 0) {
                    new_node->metadata = sysml2_arena_alloc(
                        arena, node->metadata_count * sizeof(SysmlMetadataUsage *));
                    if (new_node->metadata) {
                        for (size_t k = 0; k < node->metadata_count; k++) {
                            new_node->metadata[k] =
                                sysml2_modify_copy_metadata_usage(node->metadata[k], arena);
                        }
                        new_node->metadata_count = node->metadata_count;
                    }
                }

                /* Union merge body_stmts: fragment statements take precedence,
                 * preserve original statements not in fragment.
                 * Only handles shorthand features (SYSML_STMT_SHORTHAND_FEATURE). */
                if (node->body_stmt_count > 0) {
                    /* Build set of statement names from fragment */
                    const char **frag_names = sysml2_arena_alloc(
                        arena, (new_node->body_stmt_count + 1) * sizeof(const char *));
                    size_t frag_name_count = 0;

                    if (frag_names) {
                        for (size_t k = 0; k < new_node->body_stmt_count; k++) {
                            SysmlStatement *stmt = new_node->body_stmts[k];
                            if (stmt && stmt->kind == SYSML_STMT_SHORTHAND_FEATURE && stmt->raw_text) {
                                const char *name = sysml2_extract_shorthand_stmt_name(
                                    stmt->raw_text, arena, intern);
                                if (name) {
                                    frag_names[frag_name_count++] = name;
                                }
                            }
                        }
                    }

                    /* Count original statements not in fragment */
                    size_t preserve_count = 0;
                    for (size_t k = 0; k < node->body_stmt_count; k++) {
                        SysmlStatement *stmt = node->body_stmts[k];
                        if (!stmt) continue;

                        if (stmt->kind == SYSML_STMT_SHORTHAND_FEATURE && stmt->raw_text) {
                            const char *name = sysml2_extract_shorthand_stmt_name(
                                stmt->raw_text, arena, intern);
                            if (name && !id_in_set(name, frag_names, frag_name_count)) {
                                preserve_count++;
                            }
                        }
                    }

                    /* Allocate merged array if we have statements to preserve */
                    if (preserve_count > 0) {
                        size_t new_total = new_node->body_stmt_count + preserve_count;
                        SysmlStatement **merged = sysml2_arena_alloc(
                            arena, new_total * sizeof(SysmlStatement *));

                        if (merged) {
                            size_t idx = 0;
                            for (size_t k = 0; k < new_node->body_stmt_count; k++) {
                                merged[idx++] = new_node->body_stmts[k];
                            }

                            for (size_t k = 0; k < node->body_stmt_count; k++) {
                                SysmlStatement *stmt = node->body_stmts[k];
                                if (!stmt) continue;

                                if (stmt->kind == SYSML_STMT_SHORTHAND_FEATURE && stmt->raw_text) {
                                    const char *name = sysml2_extract_shorthand_stmt_name(
                                        stmt->raw_text, arena, intern);
                                    if (name && !id_in_set(name, frag_names, frag_name_count)) {
                                        merged[idx++] = sysml2_modify_copy_statement(stmt, arena);
                                    }
                                }
                            }

                            new_node->body_stmts = merged;
                            new_node->body_stmt_count = idx;
                        }
                    }
                }

                result->elements[result->element_count++] = new_node;
                break;  /* Found and processed the replacement */
            }
            continue;  /* Skip to next base element */
        }

        if (!id_in_set(node->id, ids_to_remove, remove_count)) {
            /* For the target scope element itself, conditionally clear metadata.
             * Only clear if the fragment explicitly provides replacement metadata;
             * otherwise preserve existing metadata to prevent data loss. */
            if (target_scope && strcmp(node->id, target_scope) == 0) {
                if (fragment_has_scope_metadata) {
                    node->prefix_applied_metadata = NULL;
                    node->prefix_applied_metadata_count = 0;
                    node->metadata = NULL;
                    node->metadata_count = 0;
                }

                /* Apply wrapper metadata/documentation to target scope if wrapper was unwrapped.
                 * This preserves scope-level annotations when a fragment uses a wrapper package. */
                if (wrapper_doc && !node->documentation) {
                    node->documentation = wrapper_doc;
                }
                if (wrapper_metadata_count > 0 && node->metadata_count == 0) {
                    node->metadata = wrapper_metadata;
                    node->metadata_count = wrapper_metadata_count;
                }
                if (wrapper_prefix_metadata_count > 0 && node->prefix_applied_metadata_count == 0) {
                    node->prefix_applied_metadata = wrapper_prefix_metadata;
                    node->prefix_applied_metadata_count = wrapper_prefix_metadata_count;
                }

                /* Only clear trivia when scope metadata was actually replaced,
                 * to preserve blank lines and formatting when just adding elements */
                if (fragment_has_scope_metadata) {
                    node->leading_trivia = NULL;
                    node->trailing_trivia = NULL;
                }
            }
            result->elements[result->element_count++] = node;
        }
    }

    /* Step 5: Add remaining fragment elements (new elements, not replacements)
     *
     * Replacement elements were already processed in Step 4 to preserve order.
     * This step handles new elements that don't replace existing ones.
     */
    for (size_t i = 0; i < fragment->element_count; i++) {
        /* Skip elements already processed in Step 4 (replacements) */
        if (fragment_processed[i]) continue;

        SysmlNode *frag_node = fragment->elements[i];
        if (!frag_node) continue;

        /* Deep copy node with all pointer arrays and remapped IDs */
        SysmlNode *new_node = sysml2_modify_deep_copy_node(frag_node, target_scope, arena, intern);
        if (!new_node) return NULL;

        /* Check if this was a replacement (shouldn't happen, but handle for safety) */
        if (id_in_set(new_node->id, replaced_ids, replaced_count)) {
            /* This is a fallback - normally replacements are handled in Step 4 */

            /* Find original element once for all preservation logic */
            SysmlNode *orig = NULL;
            for (size_t j = 0; j < working_base->element_count; j++) {
                if (working_base->elements[j] && working_base->elements[j]->id &&
                    strcmp(working_base->elements[j]->id, new_node->id) == 0) {
                    orig = working_base->elements[j];
                    break;
                }
            }

            if (orig) {
                /* Inherit original location to preserve element ordering.
                 * The writer sorts by loc.offset, so fragment elements with different
                 * source file offsets would otherwise get reordered. */
                new_node->loc = orig->loc;

                /* Preserve documentation if fragment has none */
                if (!new_node->documentation && orig->documentation) {
                    new_node->documentation = orig->documentation;  /* Interned string */
                }

                /* Preserve prefix_applied_metadata if fragment has none */
                if (new_node->prefix_applied_metadata_count == 0 && orig->prefix_applied_metadata_count > 0) {
                    new_node->prefix_applied_metadata = sysml2_arena_alloc(
                        arena, orig->prefix_applied_metadata_count * sizeof(SysmlMetadataUsage *));
                    if (new_node->prefix_applied_metadata) {
                        for (size_t k = 0; k < orig->prefix_applied_metadata_count; k++) {
                            new_node->prefix_applied_metadata[k] =
                                sysml2_modify_copy_metadata_usage(orig->prefix_applied_metadata[k], arena);
                        }
                        new_node->prefix_applied_metadata_count = orig->prefix_applied_metadata_count;
                    }
                }

                /* Preserve body metadata if fragment has none */
                if (new_node->metadata_count == 0 && orig->metadata_count > 0) {
                    new_node->metadata = sysml2_arena_alloc(
                        arena, orig->metadata_count * sizeof(SysmlMetadataUsage *));
                    if (new_node->metadata) {
                        for (size_t k = 0; k < orig->metadata_count; k++) {
                            new_node->metadata[k] =
                                sysml2_modify_copy_metadata_usage(orig->metadata[k], arena);
                        }
                        new_node->metadata_count = orig->metadata_count;
                    }
                }

                /* Union merge body_stmts: fragment statements take precedence,
                 * preserve original statements not in fragment.
                 * Only handles shorthand features (SYSML_STMT_SHORTHAND_FEATURE). */
                if (orig->body_stmt_count > 0) {
                    /* Build set of statement names from fragment */
                    const char **frag_names = sysml2_arena_alloc(
                        arena, (new_node->body_stmt_count + 1) * sizeof(const char *));
                    size_t frag_name_count = 0;

                    if (frag_names) {
                        for (size_t k = 0; k < new_node->body_stmt_count; k++) {
                            SysmlStatement *stmt = new_node->body_stmts[k];
                            if (stmt && stmt->kind == SYSML_STMT_SHORTHAND_FEATURE && stmt->raw_text) {
                                const char *name = sysml2_extract_shorthand_stmt_name(
                                    stmt->raw_text, arena, intern);
                                if (name) {
                                    frag_names[frag_name_count++] = name;
                                }
                            }
                        }
                    }

                    /* Count original statements not in fragment */
                    size_t preserve_count = 0;
                    for (size_t k = 0; k < orig->body_stmt_count; k++) {
                        SysmlStatement *stmt = orig->body_stmts[k];
                        if (!stmt) continue;

                        /* Only merge shorthand features by name */
                        if (stmt->kind == SYSML_STMT_SHORTHAND_FEATURE && stmt->raw_text) {
                            const char *name = sysml2_extract_shorthand_stmt_name(
                                stmt->raw_text, arena, intern);
                            if (name && !id_in_set(name, frag_names, frag_name_count)) {
                                preserve_count++;
                            }
                        }
                        /* Non-shorthand statements from original are not preserved
                         * (they would be structural changes that fragment should handle) */
                    }

                    /* Allocate merged array if we have statements to preserve */
                    if (preserve_count > 0) {
                        size_t new_total = new_node->body_stmt_count + preserve_count;
                        SysmlStatement **merged = sysml2_arena_alloc(
                            arena, new_total * sizeof(SysmlStatement *));

                        if (merged) {
                            /* Copy fragment statements first (they take precedence) */
                            size_t idx = 0;
                            for (size_t k = 0; k < new_node->body_stmt_count; k++) {
                                merged[idx++] = new_node->body_stmts[k];
                            }

                            /* Append preserved original statements */
                            for (size_t k = 0; k < orig->body_stmt_count; k++) {
                                SysmlStatement *stmt = orig->body_stmts[k];
                                if (!stmt) continue;

                                if (stmt->kind == SYSML_STMT_SHORTHAND_FEATURE && stmt->raw_text) {
                                    const char *name = sysml2_extract_shorthand_stmt_name(
                                        stmt->raw_text, arena, intern);
                                    if (name && !id_in_set(name, frag_names, frag_name_count)) {
                                        /* Deep copy the preserved statement */
                                        merged[idx++] = sysml2_modify_copy_statement(stmt, arena);
                                    }
                                }
                            }

                            new_node->body_stmts = merged;
                            new_node->body_stmt_count = idx;
                        }
                    }
                }
            }
        } else {
            added++;

            /* For new elements, we want them to appear after existing siblings
             * while preserving their relative ordering from the fragment.
             *
             * The writer sorts by offset: non-zero first (ascending), then zero (by insertion_order).
             *
             * Strategy:
             * - No existing siblings: keep original offsets (preserve fragment's internal order)
             * - Siblings have non-zero offsets: shift new element's offset to be after them
             * - Siblings have offset=0: set new element's offset to 0 (insertion order applies)
             */
            if (new_node->parent_id) {
                uint32_t max_sibling_offset = 0;
                bool has_sibling = false;
                bool has_nonzero_sibling = false;
                for (size_t j = 0; j < result->element_count; j++) {
                    if (result->elements[j] && result->elements[j]->parent_id &&
                        strcmp(result->elements[j]->parent_id, new_node->parent_id) == 0) {
                        has_sibling = true;
                        if (result->elements[j]->loc.offset > max_sibling_offset) {
                            max_sibling_offset = result->elements[j]->loc.offset;
                        }
                        if (result->elements[j]->loc.offset > 0) {
                            has_nonzero_sibling = true;
                        }
                    }
                }

                if (!has_sibling) {
                    /* No existing siblings - keep original offset to preserve
                     * fragment's internal ordering (children vs body_stmts) */
                } else if (has_nonzero_sibling) {
                    /* Siblings have known offsets; shift new element after them */
                    if (new_node->loc.offset <= max_sibling_offset) {
                        new_node->loc.offset = max_sibling_offset + 1000 + new_node->loc.offset;
                    }
                } else {
                    /* Siblings exist but use offset=0 (insertion order).
                     * New element should also use offset=0 to appear after them.
                     * Also zero out body_stmt offsets for consistent sorting. */
                    new_node->loc.offset = 0;
                    for (size_t k = 0; k < new_node->body_stmt_count; k++) {
                        if (new_node->body_stmts[k]) {
                            new_node->body_stmts[k]->loc.offset = 0;
                        }
                    }
                }
            }
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

    /* Step 8: Copy non-affected imports from base
     *
     * HYBRID MODE: Keep imports unless their owner scope was removed.
     * We do NOT remove all imports owned by the target scope - only those
     * whose owner was explicitly removed.
     */
    for (size_t i = 0; i < working_base->import_count; i++) {
        SysmlImport *imp = working_base->imports[i];
        if (!imp) continue;

        bool owner_removed = imp->owner_scope && id_in_set(imp->owner_scope, ids_to_remove, remove_count);

        if (!owner_removed) {
            result->imports[result->import_count++] = imp;
        }
    }

    /* Step 9: Add remapped fragment imports, with deduplication
     *
     * Fragment imports often duplicate imports already in the base model
     * (e.g., "import SysMLPrimitives::*;"). We skip duplicates to prevent
     * accumulation when the same fragment is applied multiple times.
     *
     * Two imports are considered duplicates if they have the same:
     *   - owner_scope (after remapping)
     *   - target
     *   - kind (IMPORT, IMPORT_ALL, IMPORT_RECURSIVE)
     */
    for (size_t i = 0; i < fragment->import_count; i++) {
        SysmlImport *frag_imp = fragment->imports[i];
        if (!frag_imp) continue;

        /* Compute remapped owner scope for duplicate check.
         * When owner_scope is NULL (e.g., from unwrapped wrapper), map to target_scope
         * so the import becomes scoped to the target rather than staying top-level. */
        const char *new_owner;
        if (frag_imp->owner_scope) {
            new_owner = sysml2_modify_remap_id(frag_imp->owner_scope, target_scope, arena, intern);
        } else if (target_scope) {
            /* Unwrapped imports belong to target scope, not top-level */
            new_owner = sysml2_intern(intern, target_scope);
        } else {
            new_owner = NULL;
        }

        /* Check for duplicate in result */
        bool is_duplicate = false;
        for (size_t j = 0; j < result->import_count; j++) {
            SysmlImport *existing = result->imports[j];
            if (!existing) continue;

            /* Compare owner_scope */
            bool same_owner = (new_owner == existing->owner_scope) ||
                              (new_owner && existing->owner_scope &&
                               strcmp(new_owner, existing->owner_scope) == 0);
            if (!same_owner) continue;

            /* Compare target */
            bool same_target = (frag_imp->target == existing->target) ||
                               (frag_imp->target && existing->target &&
                                strcmp(frag_imp->target, existing->target) == 0);
            if (!same_target) continue;

            /* Compare kind */
            if (frag_imp->kind == existing->kind) {
                is_duplicate = true;
                break;
            }
        }

        if (is_duplicate) {
            continue;  /* Skip this import */
        }

        /* Create new import with remapped owner scope */
        SysmlImport *new_imp = sysml2_arena_alloc(arena, sizeof(SysmlImport));
        if (!new_imp) return NULL;

        memcpy(new_imp, frag_imp, sizeof(SysmlImport));

        if (frag_imp->id) {
            new_imp->id = sysml2_modify_remap_id(frag_imp->id, target_scope, arena, intern);
        }
        new_imp->owner_scope = new_owner;
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

/*
 * List all scopes in a model
 */
bool sysml2_modify_list_scopes(
    const SysmlSemanticModel *model,
    Sysml2Arena *arena,
    const char ***out_scopes,
    size_t *out_count
) {
    if (!model || !arena || !out_scopes || !out_count) {
        if (out_scopes) *out_scopes = NULL;
        if (out_count) *out_count = 0;
        return false;
    }

    /* Count packages/namespaces first */
    size_t scope_count = 0;
    for (size_t i = 0; i < model->element_count; i++) {
        SysmlNode *node = model->elements[i];
        if (!node || !node->id) continue;

        /* Include packages and namespaces as scopes */
        if (node->kind == SYSML_KIND_PACKAGE ||
            node->kind == SYSML_KIND_NAMESPACE) {
            scope_count++;
        }
    }

    if (scope_count == 0) {
        *out_scopes = NULL;
        *out_count = 0;
        return true;
    }

    /* Allocate array (+1 for NULL terminator) */
    const char **scopes = sysml2_arena_alloc(arena, (scope_count + 1) * sizeof(const char *));
    if (!scopes) {
        *out_scopes = NULL;
        *out_count = 0;
        return false;
    }

    /* Collect scope IDs */
    size_t idx = 0;
    for (size_t i = 0; i < model->element_count; i++) {
        SysmlNode *node = model->elements[i];
        if (!node || !node->id) continue;

        if (node->kind == SYSML_KIND_PACKAGE ||
            node->kind == SYSML_KIND_NAMESPACE) {
            scopes[idx++] = node->id;
        }
    }
    scopes[idx] = NULL;  /* NULL terminator */

    *out_scopes = scopes;
    *out_count = scope_count;
    return true;
}

/*
 * List all scopes across multiple models
 */
bool sysml2_modify_list_scopes_multi(
    SysmlSemanticModel **models,
    size_t model_count,
    Sysml2Arena *arena,
    const char ***out_scopes,
    size_t *out_count
) {
    if (!models || !arena || !out_scopes || !out_count) {
        if (out_scopes) *out_scopes = NULL;
        if (out_count) *out_count = 0;
        return false;
    }

    /* Count total scopes across all models */
    size_t total_count = 0;
    for (size_t m = 0; m < model_count; m++) {
        if (!models[m]) continue;

        for (size_t i = 0; i < models[m]->element_count; i++) {
            SysmlNode *node = models[m]->elements[i];
            if (!node || !node->id) continue;

            if (node->kind == SYSML_KIND_PACKAGE ||
                node->kind == SYSML_KIND_NAMESPACE) {
                total_count++;
            }
        }
    }

    if (total_count == 0) {
        *out_scopes = NULL;
        *out_count = 0;
        return true;
    }

    /* Allocate array (+1 for NULL terminator) */
    const char **scopes = sysml2_arena_alloc(arena, (total_count + 1) * sizeof(const char *));
    if (!scopes) {
        *out_scopes = NULL;
        *out_count = 0;
        return false;
    }

    /* Collect scope IDs from all models */
    size_t idx = 0;
    for (size_t m = 0; m < model_count; m++) {
        if (!models[m]) continue;

        for (size_t i = 0; i < models[m]->element_count; i++) {
            SysmlNode *node = models[m]->elements[i];
            if (!node || !node->id) continue;

            if (node->kind == SYSML_KIND_PACKAGE ||
                node->kind == SYSML_KIND_NAMESPACE) {
                /* Check for duplicates (same scope in multiple files) */
                bool duplicate = false;
                for (size_t j = 0; j < idx; j++) {
                    if (strcmp(scopes[j], node->id) == 0) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    scopes[idx++] = node->id;
                }
            }
        }
    }
    scopes[idx] = NULL;  /* NULL terminator */

    *out_scopes = scopes;
    *out_count = idx;
    return true;
}

/*
 * Calculate simple edit distance between two strings (for fuzzy matching)
 * Limited to max_dist to avoid expensive comparisons
 */
static size_t simple_edit_distance(const char *s1, const char *s2, size_t max_dist) {
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);

    /* Quick length check */
    if (len1 > len2 + max_dist || len2 > len1 + max_dist) {
        return max_dist + 1;
    }

    /* Simple Levenshtein using two rows */
    size_t *prev = malloc((len2 + 1) * sizeof(size_t));
    size_t *curr = malloc((len2 + 1) * sizeof(size_t));
    if (!prev || !curr) {
        free(prev);
        free(curr);
        return max_dist + 1;
    }

    for (size_t j = 0; j <= len2; j++) {
        prev[j] = j;
    }

    for (size_t i = 1; i <= len1; i++) {
        curr[0] = i;
        size_t min_in_row = i;

        for (size_t j = 1; j <= len2; j++) {
            size_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            curr[j] = prev[j] + 1;  /* deletion */
            if (curr[j - 1] + 1 < curr[j]) {
                curr[j] = curr[j - 1] + 1;  /* insertion */
            }
            if (prev[j - 1] + cost < curr[j]) {
                curr[j] = prev[j - 1] + cost;  /* substitution */
            }
            if (curr[j] < min_in_row) {
                min_in_row = curr[j];
            }
        }

        /* Early termination if all values in row exceed max_dist */
        if (min_in_row > max_dist) {
            free(prev);
            free(curr);
            return max_dist + 1;
        }

        /* Swap rows */
        size_t *tmp = prev;
        prev = curr;
        curr = tmp;
    }

    size_t result = prev[len2];
    free(prev);
    free(curr);
    return result;
}

/*
 * Find similar scope names
 */
bool sysml2_modify_find_similar_scopes(
    const char *target,
    const char **scopes,
    size_t scope_count,
    Sysml2Arena *arena,
    const char ***out_suggestions,
    size_t *out_count,
    size_t max_suggestions
) {
    if (!target || !scopes || !arena || !out_suggestions || !out_count) {
        if (out_suggestions) *out_suggestions = NULL;
        if (out_count) *out_count = 0;
        return false;
    }

    if (scope_count == 0 || max_suggestions == 0) {
        *out_suggestions = NULL;
        *out_count = 0;
        return true;
    }

    /* Get local name of target for comparison */
    const char *target_local = sysml2_modify_get_local_name(target);

    /* Allocate space for suggestions with scores */
    typedef struct {
        const char *scope;
        size_t score;  /* lower is better */
    } ScoredScope;

    ScoredScope *scored = sysml2_arena_alloc(arena, scope_count * sizeof(ScoredScope));
    if (!scored) {
        *out_suggestions = NULL;
        *out_count = 0;
        return false;
    }

    size_t scored_count = 0;
    size_t max_edit_dist = 5;  /* Maximum edit distance to consider */

    for (size_t i = 0; i < scope_count; i++) {
        if (!scopes[i]) continue;

        /* Calculate similarity score */
        const char *scope_local = sysml2_modify_get_local_name(scopes[i]);
        size_t score = SIZE_MAX;

        /* Exact local name match (best) */
        if (strcmp(scope_local, target_local) == 0) {
            score = 0;
        }
        /* Case-insensitive local name match */
        else if (strcasecmp(scope_local, target_local) == 0) {
            score = 1;
        }
        /* Prefix match (scope starts with target or vice versa) */
        else if (strncmp(scopes[i], target, strlen(target)) == 0) {
            score = 2;
        }
        else if (strncmp(target, scopes[i], strlen(scopes[i])) == 0) {
            score = 2;
        }
        /* Edit distance on local name */
        else {
            size_t dist = simple_edit_distance(scope_local, target_local, max_edit_dist);
            if (dist <= max_edit_dist) {
                score = 10 + dist;
            }
        }

        /* Only include if score is reasonable */
        if (score < SIZE_MAX) {
            scored[scored_count].scope = scopes[i];
            scored[scored_count].score = score;
            scored_count++;
        }
    }

    /* Sort by score (simple bubble sort - small arrays) */
    for (size_t i = 0; i < scored_count; i++) {
        for (size_t j = i + 1; j < scored_count; j++) {
            if (scored[j].score < scored[i].score) {
                ScoredScope tmp = scored[i];
                scored[i] = scored[j];
                scored[j] = tmp;
            }
        }
    }

    /* Take top N suggestions */
    size_t result_count = scored_count < max_suggestions ? scored_count : max_suggestions;

    if (result_count == 0) {
        *out_suggestions = NULL;
        *out_count = 0;
        return true;
    }

    const char **suggestions = sysml2_arena_alloc(arena, (result_count + 1) * sizeof(const char *));
    if (!suggestions) {
        *out_suggestions = NULL;
        *out_count = 0;
        return false;
    }

    for (size_t i = 0; i < result_count; i++) {
        suggestions[i] = scored[i].scope;
    }
    suggestions[result_count] = NULL;

    *out_suggestions = suggestions;
    *out_count = result_count;
    return true;
}
