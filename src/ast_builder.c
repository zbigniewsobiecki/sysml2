/*
 * SysML v2 Parser - AST Builder Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/ast_builder.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * Create a new build context
 */
SysmlBuildContext *sysml_build_context_create(
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    const char *source_name
) {
    SysmlBuildContext *ctx = SYSML2_ARENA_NEW(arena, SysmlBuildContext);
    if (!ctx) return NULL;

    ctx->arena = arena;
    ctx->intern = intern;
    ctx->source_name = sysml2_intern(intern, source_name);

    /* Initialize scope stack */
    ctx->scope_capacity = SYSML_BUILD_DEFAULT_SCOPE_CAPACITY;
    ctx->scope_stack = SYSML2_ARENA_NEW_ARRAY(arena, const char *, ctx->scope_capacity);
    ctx->scope_depth = 0;

    /* Initialize counters */
    ctx->anon_counter = 0;
    ctx->rel_counter = 0;

    /* Initialize element array */
    ctx->element_capacity = SYSML_BUILD_DEFAULT_ELEMENT_CAPACITY;
    ctx->elements = SYSML2_ARENA_NEW_ARRAY(arena, SysmlNode *, ctx->element_capacity);
    ctx->element_count = 0;

    /* Initialize relationship array */
    ctx->relationship_capacity = SYSML_BUILD_DEFAULT_REL_CAPACITY;
    ctx->relationships = SYSML2_ARENA_NEW_ARRAY(arena, SysmlRelationship *, ctx->relationship_capacity);
    ctx->relationship_count = 0;

    /* Initialize import array */
    ctx->import_capacity = SYSML_BUILD_DEFAULT_IMPORT_CAPACITY;
    ctx->imports = SYSML2_ARENA_NEW_ARRAY(arena, SysmlImport *, ctx->import_capacity);
    ctx->import_count = 0;

    return ctx;
}

/*
 * Destroy a build context
 */
void sysml_build_context_destroy(SysmlBuildContext *ctx) {
    /* All memory is in the arena, nothing to free */
    (void)ctx;
}

/*
 * Push a new scope onto the scope stack
 */
void sysml_build_push_scope(SysmlBuildContext *ctx, const char *scope_id) {
    if (!ctx || !scope_id) return;

    /* Grow stack if needed */
    if (ctx->scope_depth >= ctx->scope_capacity) {
        size_t new_capacity = ctx->scope_capacity * 2;
        const char **new_stack = SYSML2_ARENA_NEW_ARRAY(ctx->arena, const char *, new_capacity);
        if (!new_stack) return;
        memcpy(new_stack, ctx->scope_stack, ctx->scope_depth * sizeof(const char *));
        ctx->scope_stack = new_stack;
        ctx->scope_capacity = new_capacity;
    }

    ctx->scope_stack[ctx->scope_depth++] = scope_id;
}

/*
 * Pop the current scope from the stack
 */
void sysml_build_pop_scope(SysmlBuildContext *ctx) {
    if (!ctx || ctx->scope_depth == 0) return;
    ctx->scope_depth--;
}

/*
 * Get the current scope ID
 */
const char *sysml_build_current_scope(SysmlBuildContext *ctx) {
    if (!ctx || ctx->scope_depth == 0) return NULL;
    return ctx->scope_stack[ctx->scope_depth - 1];
}

/*
 * Generate a path-based ID for an element
 */
const char *sysml_build_make_id(SysmlBuildContext *ctx, const char *name) {
    if (!ctx) return NULL;

    /* Generate anonymous name if needed */
    char anon_buf[32];
    if (!name) {
        snprintf(anon_buf, sizeof(anon_buf), "_anon_%zu", ++ctx->anon_counter);
        name = anon_buf;
    }

    /* Build path: parent::name */
    const char *parent = sysml_build_current_scope(ctx);
    if (!parent) {
        /* Root level - just the name */
        return sysml2_intern(ctx->intern, name);
    }

    /* Calculate size needed */
    size_t parent_len = strlen(parent);
    size_t name_len = strlen(name);
    size_t total_len = parent_len + 2 + name_len + 1; /* "::" + null */

    /* Allocate and build path */
    char *path = sysml2_arena_alloc(ctx->arena, total_len);
    if (!path) return NULL;

    memcpy(path, parent, parent_len);
    path[parent_len] = ':';
    path[parent_len + 1] = ':';
    memcpy(path + parent_len + 2, name, name_len);
    path[total_len - 1] = '\0';

    return sysml2_intern(ctx->intern, path);
}

/*
 * Generate a unique relationship ID
 */
const char *sysml_build_make_rel_id(SysmlBuildContext *ctx, const char *kind) {
    if (!ctx) return NULL;

    const char *parent = sysml_build_current_scope(ctx);
    char id_buf[256];

    if (parent) {
        snprintf(id_buf, sizeof(id_buf), "%s::_%s_%zu",
                 parent, kind ? kind : "rel", ++ctx->rel_counter);
    } else {
        snprintf(id_buf, sizeof(id_buf), "_%s_%zu",
                 kind ? kind : "rel", ++ctx->rel_counter);
    }

    return sysml2_intern(ctx->intern, id_buf);
}

/*
 * Create a new AST node
 */
SysmlNode *sysml_build_node(
    SysmlBuildContext *ctx,
    SysmlNodeKind kind,
    const char *name
) {
    if (!ctx) return NULL;

    SysmlNode *node = SYSML2_ARENA_NEW(ctx->arena, SysmlNode);
    if (!node) return NULL;

    /* Intern the name if provided */
    const char *interned_name = name ? sysml2_intern(ctx->intern, name) : NULL;

    node->id = sysml_build_make_id(ctx, name);
    node->name = interned_name;
    node->kind = kind;
    node->parent_id = sysml_build_current_scope(ctx);
    node->typed_by = NULL;
    node->typed_by_count = 0;
    node->loc = SYSML2_LOC_INVALID;

    return node;
}

/*
 * Grow the elements array if needed
 */
static void ensure_element_capacity(SysmlBuildContext *ctx) {
    if (ctx->element_count >= ctx->element_capacity) {
        size_t new_capacity = ctx->element_capacity * 2;
        SysmlNode **new_elements = SYSML2_ARENA_NEW_ARRAY(ctx->arena, SysmlNode *, new_capacity);
        if (!new_elements) return;
        memcpy(new_elements, ctx->elements, ctx->element_count * sizeof(SysmlNode *));
        ctx->elements = new_elements;
        ctx->element_capacity = new_capacity;
    }
}

/*
 * Add an element to the model
 */
void sysml_build_add_element(SysmlBuildContext *ctx, SysmlNode *node) {
    if (!ctx || !node) return;
    ensure_element_capacity(ctx);
    ctx->elements[ctx->element_count++] = node;
}

/*
 * Grow the relationships array if needed
 */
static void ensure_relationship_capacity(SysmlBuildContext *ctx) {
    if (ctx->relationship_count >= ctx->relationship_capacity) {
        size_t new_capacity = ctx->relationship_capacity * 2;
        SysmlRelationship **new_rels = SYSML2_ARENA_NEW_ARRAY(ctx->arena, SysmlRelationship *, new_capacity);
        if (!new_rels) return;
        memcpy(new_rels, ctx->relationships, ctx->relationship_count * sizeof(SysmlRelationship *));
        ctx->relationships = new_rels;
        ctx->relationship_capacity = new_capacity;
    }
}

/*
 * Create a new relationship
 */
SysmlRelationship *sysml_build_relationship(
    SysmlBuildContext *ctx,
    SysmlNodeKind kind,
    const char *source,
    const char *target
) {
    if (!ctx) return NULL;

    SysmlRelationship *rel = SYSML2_ARENA_NEW(ctx->arena, SysmlRelationship);
    if (!rel) return NULL;

    /* Determine kind prefix for ID */
    const char *kind_prefix;
    switch (kind) {
        case SYSML_KIND_REL_CONNECTION: kind_prefix = "conn"; break;
        case SYSML_KIND_REL_FLOW:       kind_prefix = "flow"; break;
        case SYSML_KIND_REL_ALLOCATION: kind_prefix = "alloc"; break;
        case SYSML_KIND_REL_SATISFY:    kind_prefix = "satisfy"; break;
        case SYSML_KIND_REL_VERIFY:     kind_prefix = "verify"; break;
        case SYSML_KIND_REL_TRANSITION: kind_prefix = "trans"; break;
        case SYSML_KIND_REL_SUCCESSION: kind_prefix = "succ"; break;
        case SYSML_KIND_REL_BIND:       kind_prefix = "bind"; break;
        default:                        kind_prefix = "rel"; break;
    }

    rel->id = sysml_build_make_rel_id(ctx, kind_prefix);
    rel->kind = kind;
    rel->source = source ? sysml2_intern(ctx->intern, source) : NULL;
    rel->target = target ? sysml2_intern(ctx->intern, target) : NULL;
    rel->loc = SYSML2_LOC_INVALID;

    return rel;
}

/*
 * Add a relationship to the model
 */
void sysml_build_add_relationship(SysmlBuildContext *ctx, SysmlRelationship *rel) {
    if (!ctx || !rel) return;
    ensure_relationship_capacity(ctx);
    ctx->relationships[ctx->relationship_count++] = rel;
}

/*
 * Add a type specialization to a node
 */
void sysml_build_add_typed_by(
    SysmlBuildContext *ctx,
    SysmlNode *node,
    const char *type_ref
) {
    if (!ctx || !node || !type_ref) return;

    /* Intern the type reference */
    const char *interned_ref = sysml2_intern(ctx->intern, type_ref);

    /* Allocate or grow typed_by array */
    size_t new_count = node->typed_by_count + 1;
    const char **new_typed_by = SYSML2_ARENA_NEW_ARRAY(ctx->arena, const char *, new_count);
    if (!new_typed_by) return;

    /* Copy existing entries */
    if (node->typed_by && node->typed_by_count > 0) {
        memcpy(new_typed_by, node->typed_by, node->typed_by_count * sizeof(const char *));
    }

    /* Add new entry */
    new_typed_by[node->typed_by_count] = interned_ref;
    node->typed_by = new_typed_by;
    node->typed_by_count = new_count;
}

/*
 * Grow the imports array if needed
 */
static void ensure_import_capacity(SysmlBuildContext *ctx) {
    if (ctx->import_count >= ctx->import_capacity) {
        size_t new_capacity = ctx->import_capacity * 2;
        SysmlImport **new_imports = SYSML2_ARENA_NEW_ARRAY(ctx->arena, SysmlImport *, new_capacity);
        if (!new_imports) return;
        memcpy(new_imports, ctx->imports, ctx->import_count * sizeof(SysmlImport *));
        ctx->imports = new_imports;
        ctx->import_capacity = new_capacity;
    }
}

/*
 * Add an import declaration to the model
 */
void sysml_build_add_import(
    SysmlBuildContext *ctx,
    SysmlNodeKind kind,
    const char *target
) {
    if (!ctx || !target) return;

    ensure_import_capacity(ctx);

    SysmlImport *imp = SYSML2_ARENA_NEW(ctx->arena, SysmlImport);
    if (!imp) return;

    /* Generate a unique ID for the import */
    char id_buf[256];
    snprintf(id_buf, sizeof(id_buf), "_import_%zu", ctx->import_count);
    imp->id = sysml2_intern(ctx->intern, id_buf);
    imp->kind = kind;
    imp->target = sysml2_intern(ctx->intern, target);
    imp->owner_scope = sysml_build_current_scope(ctx);
    imp->loc = SYSML2_LOC_INVALID;

    ctx->imports[ctx->import_count++] = imp;
}

/*
 * Finalize the build and return the semantic model
 */
SysmlSemanticModel *sysml_build_finalize(SysmlBuildContext *ctx) {
    if (!ctx) return NULL;

    SysmlSemanticModel *model = SYSML2_ARENA_NEW(ctx->arena, SysmlSemanticModel);
    if (!model) return NULL;

    model->source_name = ctx->source_name;

    model->elements = ctx->elements;
    model->element_count = ctx->element_count;
    model->element_capacity = ctx->element_capacity;

    model->relationships = ctx->relationships;
    model->relationship_count = ctx->relationship_count;
    model->relationship_capacity = ctx->relationship_capacity;

    model->imports = ctx->imports;
    model->import_count = ctx->import_count;
    model->import_capacity = ctx->import_capacity;

    return model;
}
