/*
 * SysML v2 Parser - AST Builder Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/ast_builder.h"
#include "sysml2/utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * Create a new build context
 */
SysmlBuildContext *sysml2_build_context_create(
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

    /* Initialize alias array */
    ctx->alias_capacity = SYSML_BUILD_DEFAULT_ALIAS_CAPACITY;
    ctx->aliases = SYSML2_ARENA_NEW_ARRAY(arena, SysmlAlias *, ctx->alias_capacity);
    ctx->alias_count = 0;

    /* Initialize trivia list */
    ctx->pending_trivia_head = NULL;
    ctx->pending_trivia_tail = NULL;

    /* Initialize pending modifiers */
    ctx->pending_abstract = false;
    ctx->pending_variation = false;
    ctx->pending_readonly = false;
    ctx->pending_derived = false;
    ctx->pending_ref = false;
    ctx->pending_direction = SYSML_DIR_NONE;
    ctx->pending_visibility = SYSML_VIS_PUBLIC;

    /* Initialize pending multiplicity */
    ctx->pending_multiplicity_lower = NULL;
    ctx->pending_multiplicity_upper = NULL;

    /* Initialize pending default value */
    ctx->pending_default_value = NULL;
    ctx->pending_has_default_keyword = false;

    /* Initialize pending import visibility */
    ctx->pending_import_private = false;
    ctx->pending_import_public = false;

    /* Initialize pending prefix metadata */
    ctx->pending_prefix_metadata_capacity = 8;
    ctx->pending_prefix_metadata = SYSML2_ARENA_NEW_ARRAY(arena, const char *, ctx->pending_prefix_metadata_capacity);
    ctx->pending_prefix_metadata_count = 0;

    /* Initialize pending applied metadata */
    ctx->pending_metadata_capacity = 8;
    ctx->pending_metadata = SYSML2_ARENA_NEW_ARRAY(arena, SysmlMetadataUsage *, ctx->pending_metadata_capacity);
    ctx->pending_metadata_count = 0;

    /* Initialize current metadata */
    ctx->current_metadata = NULL;

    /* Initialize pending body statements */
    ctx->pending_stmt_capacity = 16;
    ctx->pending_stmts = SYSML2_ARENA_NEW_ARRAY(arena, SysmlStatement *, ctx->pending_stmt_capacity);
    ctx->pending_stmt_count = 0;

    /* Initialize pending named comments */
    ctx->pending_comment_capacity = 8;
    ctx->pending_comments = SYSML2_ARENA_NEW_ARRAY(arena, SysmlNamedComment *, ctx->pending_comment_capacity);
    ctx->pending_comment_count = 0;

    /* Initialize pending textual representations */
    ctx->pending_rep_capacity = 8;
    ctx->pending_reps = SYSML2_ARENA_NEW_ARRAY(arena, SysmlTextualRep *, ctx->pending_rep_capacity);
    ctx->pending_rep_count = 0;

    /* Initialize counters */
    ctx->comment_counter = 0;
    ctx->rep_counter = 0;

    return ctx;
}

/*
 * Destroy a build context
 */
void sysml2_build_context_destroy(SysmlBuildContext *ctx) {
    /* All memory is in the arena, nothing to free */
    (void)ctx;
}

/*
 * Push a new scope onto the scope stack
 */
void sysml2_build_push_scope(SysmlBuildContext *ctx, const char *scope_id) {
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
void sysml2_build_pop_scope(SysmlBuildContext *ctx) {
    if (!ctx || ctx->scope_depth == 0) return;
    ctx->scope_depth--;
}

/*
 * Get the current scope ID
 */
const char *sysml2_build_current_scope(SysmlBuildContext *ctx) {
    if (!ctx || ctx->scope_depth == 0) return NULL;
    return ctx->scope_stack[ctx->scope_depth - 1];
}

/*
 * Generate a path-based ID for an element
 */
const char *sysml2_build_make_id(SysmlBuildContext *ctx, const char *name) {
    if (!ctx) return NULL;

    /* Generate anonymous name if needed */
    char anon_buf[32];
    if (!name) {
        snprintf(anon_buf, sizeof(anon_buf), "_anon_%zu", ++ctx->anon_counter);
        name = anon_buf;
    }

    /* Build path: parent::name */
    const char *parent = sysml2_build_current_scope(ctx);
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
const char *sysml2_build_make_rel_id(SysmlBuildContext *ctx, const char *kind) {
    if (!ctx) return NULL;

    const char *parent = sysml2_build_current_scope(ctx);
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
SysmlNode *sysml2_build_node(
    SysmlBuildContext *ctx,
    SysmlNodeKind kind,
    const char *name
) {
    if (!ctx) return NULL;

    SysmlNode *node = SYSML2_ARENA_NEW(ctx->arena, SysmlNode);
    if (!node) return NULL;

    /* Intern the name if provided */
    const char *interned_name = name ? sysml2_intern(ctx->intern, name) : NULL;

    node->id = sysml2_build_make_id(ctx, name);
    node->name = interned_name;
    node->kind = kind;
    node->parent_id = sysml2_build_current_scope(ctx);
    node->typed_by = NULL;
    node->typed_by_count = 0;
    node->specializes = NULL;
    node->specializes_count = 0;
    node->redefines = NULL;
    node->redefines_count = 0;
    node->references = NULL;
    node->references_count = 0;

    /* Apply pending multiplicity */
    node->multiplicity_lower = ctx->pending_multiplicity_lower;
    node->multiplicity_upper = ctx->pending_multiplicity_upper;
    ctx->pending_multiplicity_lower = NULL;
    ctx->pending_multiplicity_upper = NULL;

    /* Apply pending default value */
    node->default_value = ctx->pending_default_value;
    node->has_default_keyword = ctx->pending_has_default_keyword;
    ctx->pending_default_value = NULL;
    ctx->pending_has_default_keyword = false;

    /* Apply pending modifiers */
    node->is_abstract = ctx->pending_abstract;
    node->is_variation = ctx->pending_variation;
    node->is_readonly = ctx->pending_readonly;
    node->is_derived = ctx->pending_derived;
    node->is_ref = ctx->pending_ref;
    ctx->pending_abstract = false;
    ctx->pending_variation = false;
    ctx->pending_readonly = false;
    ctx->pending_derived = false;
    ctx->pending_ref = false;

    /* Apply pending direction */
    node->direction = ctx->pending_direction;
    ctx->pending_direction = SYSML_DIR_NONE;

    /* Apply pending visibility */
    node->visibility = ctx->pending_visibility;
    ctx->pending_visibility = SYSML_VIS_PUBLIC;

    node->loc = SYSML2_LOC_INVALID;
    node->documentation = NULL;
    node->metadata = NULL;
    node->metadata_count = 0;
    node->prefix_metadata = NULL;
    node->prefix_metadata_count = 0;
    node->prefix_applied_metadata = NULL;
    node->prefix_applied_metadata_count = 0;
    node->leading_trivia = NULL;
    node->trailing_trivia = NULL;

    /* Initialize body statement arrays */
    node->body_stmts = NULL;
    node->body_stmt_count = 0;
    node->comments = NULL;
    node->comment_count = 0;
    node->textual_reps = NULL;
    node->textual_rep_count = 0;
    node->result_expression = NULL;

    /* Attach any pending trivia as leading trivia */
    sysml2_build_attach_pending_trivia(ctx, node);

    /* Attach any pending prefix metadata */
    if (ctx->pending_prefix_metadata_count > 0) {
        node->prefix_metadata = SYSML2_ARENA_NEW_ARRAY(ctx->arena, const char *, ctx->pending_prefix_metadata_count);
        if (node->prefix_metadata) {
            memcpy(node->prefix_metadata, ctx->pending_prefix_metadata,
                   ctx->pending_prefix_metadata_count * sizeof(const char *));
            node->prefix_metadata_count = ctx->pending_prefix_metadata_count;
        }
        ctx->pending_prefix_metadata_count = 0;
    }

    /* Attach any pending applied metadata (@Type {...}) - goes in prefix position */
    if (ctx->pending_metadata_count > 0) {
        node->prefix_applied_metadata = SYSML2_ARENA_NEW_ARRAY(ctx->arena, SysmlMetadataUsage *, ctx->pending_metadata_count);
        if (node->prefix_applied_metadata) {
            memcpy(node->prefix_applied_metadata, ctx->pending_metadata,
                   ctx->pending_metadata_count * sizeof(SysmlMetadataUsage *));
            node->prefix_applied_metadata_count = ctx->pending_metadata_count;
        }
        ctx->pending_metadata_count = 0;
    }

    return node;
}

/*
 * Grow the elements array if needed
 */
static void ensure_element_capacity(SysmlBuildContext *ctx) {
    SYSML2_ARRAY_GROW(ctx->arena, ctx->elements, ctx->element_count,
                      ctx->element_capacity, SysmlNode *);
}

/*
 * Add an element to the model
 */
void sysml2_build_add_element(SysmlBuildContext *ctx, SysmlNode *node) {
    if (!ctx || !node) return;
    ensure_element_capacity(ctx);
    ctx->elements[ctx->element_count++] = node;
}

/*
 * Grow the relationships array if needed
 */
static void ensure_relationship_capacity(SysmlBuildContext *ctx) {
    SYSML2_ARRAY_GROW(ctx->arena, ctx->relationships, ctx->relationship_count,
                      ctx->relationship_capacity, SysmlRelationship *);
}

/*
 * Create a new relationship
 */
SysmlRelationship *sysml2_build_relationship(
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

    rel->id = sysml2_build_make_rel_id(ctx, kind_prefix);
    rel->kind = kind;
    rel->source = source ? sysml2_intern(ctx->intern, source) : NULL;
    rel->target = target ? sysml2_intern(ctx->intern, target) : NULL;
    rel->loc = SYSML2_LOC_INVALID;

    return rel;
}

/*
 * Add a relationship to the model
 */
void sysml2_build_add_relationship(SysmlBuildContext *ctx, SysmlRelationship *rel) {
    if (!ctx || !rel) return;
    ensure_relationship_capacity(ctx);
    ctx->relationships[ctx->relationship_count++] = rel;
}

/*
 * Add a type reference to a node (: operator)
 */
void sysml2_build_add_typed_by(
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
 * Add a specialization to a node (:> operator)
 */
void sysml2_build_add_specializes(
    SysmlBuildContext *ctx,
    SysmlNode *node,
    const char *type_ref
) {
    if (!ctx || !node || !type_ref) return;

    /* Intern the type reference */
    const char *interned_ref = sysml2_intern(ctx->intern, type_ref);

    /* Allocate or grow specializes array */
    size_t new_count = node->specializes_count + 1;
    const char **new_specializes = SYSML2_ARENA_NEW_ARRAY(ctx->arena, const char *, new_count);
    if (!new_specializes) return;

    /* Copy existing entries */
    if (node->specializes && node->specializes_count > 0) {
        memcpy(new_specializes, node->specializes, node->specializes_count * sizeof(const char *));
    }

    /* Add new entry */
    new_specializes[node->specializes_count] = interned_ref;
    node->specializes = new_specializes;
    node->specializes_count = new_count;
}

/*
 * Add a redefinition to a node (:>> operator)
 */
void sysml2_build_add_redefines(
    SysmlBuildContext *ctx,
    SysmlNode *node,
    const char *type_ref
) {
    if (!ctx || !node || !type_ref) return;

    /* Intern the type reference */
    const char *interned_ref = sysml2_intern(ctx->intern, type_ref);

    /* Allocate or grow redefines array */
    size_t new_count = node->redefines_count + 1;
    const char **new_redefines = SYSML2_ARENA_NEW_ARRAY(ctx->arena, const char *, new_count);
    if (!new_redefines) return;

    /* Copy existing entries */
    if (node->redefines && node->redefines_count > 0) {
        memcpy(new_redefines, node->redefines, node->redefines_count * sizeof(const char *));
    }

    /* Add new entry */
    new_redefines[node->redefines_count] = interned_ref;
    node->redefines = new_redefines;
    node->redefines_count = new_count;
}

/*
 * Add a reference to a node (::> operator)
 */
void sysml2_build_add_references(
    SysmlBuildContext *ctx,
    SysmlNode *node,
    const char *type_ref
) {
    if (!ctx || !node || !type_ref) return;

    /* Intern the type reference */
    const char *interned_ref = sysml2_intern(ctx->intern, type_ref);

    /* Allocate or grow references array */
    size_t new_count = node->references_count + 1;
    const char **new_references = SYSML2_ARENA_NEW_ARRAY(ctx->arena, const char *, new_count);
    if (!new_references) return;

    /* Copy existing entries */
    if (node->references && node->references_count > 0) {
        memcpy(new_references, node->references, node->references_count * sizeof(const char *));
    }

    /* Add new entry */
    new_references[node->references_count] = interned_ref;
    node->references = new_references;
    node->references_count = new_count;
}

/*
 * Grow the imports array if needed
 */
static void ensure_import_capacity(SysmlBuildContext *ctx) {
    SYSML2_ARRAY_GROW(ctx->arena, ctx->imports, ctx->import_count,
                      ctx->import_capacity, SysmlImport *);
}

/*
 * Add an import declaration to the model
 */
void sysml2_build_add_import(
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
    imp->owner_scope = sysml2_build_current_scope(ctx);
    imp->is_private = ctx->pending_import_private;
    imp->is_public_explicit = ctx->pending_import_public;
    imp->loc = SYSML2_LOC_INVALID;

    /* Reset pending import visibility */
    ctx->pending_import_private = false;
    ctx->pending_import_public = false;

    ctx->imports[ctx->import_count++] = imp;
}

/*
 * Finalize the build and return the semantic model
 */
SysmlSemanticModel *sysml2_build_finalize(SysmlBuildContext *ctx) {
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

    model->aliases = ctx->aliases;
    model->alias_count = ctx->alias_count;
    model->alias_capacity = ctx->alias_capacity;

    return model;
}

/*
 * Create a trivia node
 */
SysmlTrivia *sysml2_build_trivia(
    SysmlBuildContext *ctx,
    SysmlTriviaKind kind,
    const char *text,
    Sysml2SourceLoc loc
) {
    if (!ctx) return NULL;

    SysmlTrivia *trivia = SYSML2_ARENA_NEW(ctx->arena, SysmlTrivia);
    if (!trivia) return NULL;

    trivia->kind = kind;
    trivia->text = text ? sysml2_intern(ctx->intern, text) : NULL;
    trivia->loc = loc;
    trivia->next = NULL;

    return trivia;
}

/*
 * Add a trivia node to the pending list
 */
void sysml2_build_add_pending_trivia(SysmlBuildContext *ctx, SysmlTrivia *trivia) {
    if (!ctx || !trivia) return;

    if (ctx->pending_trivia_tail) {
        ctx->pending_trivia_tail->next = trivia;
        ctx->pending_trivia_tail = trivia;
    } else {
        ctx->pending_trivia_head = trivia;
        ctx->pending_trivia_tail = trivia;
    }
}

/*
 * Attach pending trivia to a node
 */
void sysml2_build_attach_pending_trivia(SysmlBuildContext *ctx, SysmlNode *node) {
    if (!ctx || !node) return;

    if (ctx->pending_trivia_head) {
        node->leading_trivia = ctx->pending_trivia_head;
        ctx->pending_trivia_head = NULL;
        ctx->pending_trivia_tail = NULL;
    }
}

/*
 * Create a metadata usage
 */
SysmlMetadataUsage *sysml2_build_metadata_usage(
    SysmlBuildContext *ctx,
    const char *type_ref
) {
    if (!ctx || !type_ref) return NULL;

    SysmlMetadataUsage *meta = SYSML2_ARENA_NEW(ctx->arena, SysmlMetadataUsage);
    if (!meta) return NULL;

    meta->type_ref = sysml2_intern(ctx->intern, type_ref);
    meta->about = NULL;
    meta->about_count = 0;
    meta->features = NULL;
    meta->feature_count = 0;

    return meta;
}

/*
 * Add a feature to metadata usage
 */
void sysml2_build_metadata_add_feature(
    SysmlBuildContext *ctx,
    SysmlMetadataUsage *meta,
    const char *name,
    const char *value
) {
    if (!ctx || !meta || !name) return;

    /* Create feature */
    SysmlMetadataFeature *feature = SYSML2_ARENA_NEW(ctx->arena, SysmlMetadataFeature);
    if (!feature) return;

    feature->name = sysml2_intern(ctx->intern, name);
    feature->value = value ? sysml2_intern(ctx->intern, value) : NULL;

    /* Grow features array */
    size_t new_count = meta->feature_count + 1;
    SysmlMetadataFeature **new_features = SYSML2_ARENA_NEW_ARRAY(ctx->arena, SysmlMetadataFeature *, new_count);
    if (!new_features) return;

    /* Copy existing features */
    if (meta->features && meta->feature_count > 0) {
        memcpy(new_features, meta->features, meta->feature_count * sizeof(SysmlMetadataFeature *));
    }

    /* Add new feature */
    new_features[meta->feature_count] = feature;
    meta->features = new_features;
    meta->feature_count = new_count;
}

/*
 * Add an "about" target to metadata
 */
void sysml2_build_metadata_add_about(
    SysmlBuildContext *ctx,
    SysmlMetadataUsage *meta,
    const char *target_ref
) {
    if (!ctx || !meta || !target_ref) return;

    /* Intern the target reference */
    const char *interned_ref = sysml2_intern(ctx->intern, target_ref);

    /* Grow about array */
    size_t new_count = meta->about_count + 1;
    const char **new_about = SYSML2_ARENA_NEW_ARRAY(ctx->arena, const char *, new_count);
    if (!new_about) return;

    /* Copy existing about targets */
    if (meta->about && meta->about_count > 0) {
        memcpy(new_about, meta->about, meta->about_count * sizeof(const char *));
    }

    /* Add new about target */
    new_about[meta->about_count] = interned_ref;
    meta->about = new_about;
    meta->about_count = new_count;
}

/*
 * Attach metadata to a node
 */
void sysml2_build_add_metadata(
    SysmlBuildContext *ctx,
    SysmlNode *node,
    SysmlMetadataUsage *meta
) {
    if (!ctx || !node || !meta) return;

    /* Grow metadata array */
    size_t new_count = node->metadata_count + 1;
    SysmlMetadataUsage **new_metadata = SYSML2_ARENA_NEW_ARRAY(ctx->arena, SysmlMetadataUsage *, new_count);
    if (!new_metadata) return;

    /* Copy existing metadata */
    if (node->metadata && node->metadata_count > 0) {
        memcpy(new_metadata, node->metadata, node->metadata_count * sizeof(SysmlMetadataUsage *));
    }

    /* Add new metadata */
    new_metadata[node->metadata_count] = meta;
    node->metadata = new_metadata;
    node->metadata_count = new_count;
}

/*
 * Add prefix metadata to a node
 */
void sysml2_build_add_prefix_metadata(
    SysmlBuildContext *ctx,
    SysmlNode *node,
    const char *metadata_ref
) {
    if (!ctx || !node || !metadata_ref) return;

    /* Intern the metadata reference */
    const char *interned_ref = sysml2_intern(ctx->intern, metadata_ref);

    /* Grow prefix_metadata array */
    size_t new_count = node->prefix_metadata_count + 1;
    const char **new_prefix_metadata = SYSML2_ARENA_NEW_ARRAY(ctx->arena, const char *, new_count);
    if (!new_prefix_metadata) return;

    /* Copy existing prefix metadata */
    if (node->prefix_metadata && node->prefix_metadata_count > 0) {
        memcpy(new_prefix_metadata, node->prefix_metadata, node->prefix_metadata_count * sizeof(const char *));
    }

    /* Add new prefix metadata */
    new_prefix_metadata[node->prefix_metadata_count] = interned_ref;
    node->prefix_metadata = new_prefix_metadata;
    node->prefix_metadata_count = new_count;
}

/*
 * Add a pending prefix metadata that will be attached to the next node
 */
void sysml2_build_add_pending_prefix_metadata(
    SysmlBuildContext *ctx,
    const char *metadata_ref
) {
    if (!ctx || !metadata_ref) return;

    /* Grow array if needed */
    if (ctx->pending_prefix_metadata_count >= ctx->pending_prefix_metadata_capacity) {
        size_t new_capacity = ctx->pending_prefix_metadata_capacity * 2;
        const char **new_pending = SYSML2_ARENA_NEW_ARRAY(ctx->arena, const char *, new_capacity);
        if (!new_pending) return;
        if (ctx->pending_prefix_metadata) {
            memcpy(new_pending, ctx->pending_prefix_metadata,
                   ctx->pending_prefix_metadata_count * sizeof(const char *));
        }
        ctx->pending_prefix_metadata = new_pending;
        ctx->pending_prefix_metadata_capacity = new_capacity;
    }

    /* Add the prefix metadata */
    ctx->pending_prefix_metadata[ctx->pending_prefix_metadata_count++] =
        sysml2_intern(ctx->intern, metadata_ref);
}

/*
 * Start building a metadata usage
 */
SysmlMetadataUsage *sysml2_build_start_metadata(
    SysmlBuildContext *ctx,
    const char *type_ref
) {
    if (!ctx || !type_ref) return NULL;

    ctx->current_metadata = sysml2_build_metadata_usage(ctx, type_ref);
    return ctx->current_metadata;
}

/*
 * Add a pending metadata usage that will be attached to the next node
 */
static void add_pending_metadata(SysmlBuildContext *ctx, SysmlMetadataUsage *meta) {
    if (!ctx || !meta) return;

    /* Grow array if needed */
    if (ctx->pending_metadata_count >= ctx->pending_metadata_capacity) {
        size_t new_capacity = ctx->pending_metadata_capacity * 2;
        SysmlMetadataUsage **new_pending = SYSML2_ARENA_NEW_ARRAY(ctx->arena, SysmlMetadataUsage *, new_capacity);
        if (!new_pending) return;
        if (ctx->pending_metadata) {
            memcpy(new_pending, ctx->pending_metadata,
                   ctx->pending_metadata_count * sizeof(SysmlMetadataUsage *));
        }
        ctx->pending_metadata = new_pending;
        ctx->pending_metadata_capacity = new_capacity;
    }

    ctx->pending_metadata[ctx->pending_metadata_count++] = meta;
}

/*
 * Finish building a metadata usage and attach to current scope's node
 * or add to pending if no node exists yet
 */
void sysml2_build_end_metadata(SysmlBuildContext *ctx) {
    if (!ctx || !ctx->current_metadata) return;

    /* Find the current scope's node (most recently added element in current scope) */
    const char *current_scope = sysml2_build_current_scope(ctx);
    bool attached = false;

    if (current_scope) {
        /* Find the node with this ID */
        for (size_t i = ctx->element_count; i > 0; i--) {
            SysmlNode *node = ctx->elements[i - 1];
            if (node->id == current_scope) {
                sysml2_build_add_metadata(ctx, node, ctx->current_metadata);
                attached = true;
                break;
            }
        }
    }

    /* If no node to attach to, add to pending for next node */
    if (!attached) {
        add_pending_metadata(ctx, ctx->current_metadata);
    }

    ctx->current_metadata = NULL;
}

/*
 * Add a feature to the current metadata usage being built
 */
void sysml2_build_current_metadata_add_feature(
    SysmlBuildContext *ctx,
    const char *name,
    const char *value
) {
    if (!ctx || !ctx->current_metadata || !name) return;
    sysml2_build_metadata_add_feature(ctx, ctx->current_metadata, name, value);
}

/*
 * Trivia capture functions called from grammar actions
 * These are the entry points from the packcc-generated parser
 */

/* Forward declaration - defined in sysml_parser.c via grammar */
struct Sysml2ParserContext;

void sysml2_capture_line_comment(struct Sysml2ParserContext *pctx, size_t start_offset, size_t end_offset) {
    if (!pctx) return;

    /* The parser context struct layout - access input and build_ctx */
    typedef struct {
        const char *filename;
        const char *input;
        size_t input_len;
        size_t input_pos;
        int error_count;
        int line;
        int col;
        size_t furthest_pos;
        int furthest_line;
        int furthest_col;
        const char *failed_rules[16];
        int failed_rule_count;
        const char *context_rule;
        const char *last_keyword;
        size_t last_keyword_pos;
        SysmlBuildContext *build_ctx;
    } ParserCtx;

    ParserCtx *ctx = (ParserCtx *)pctx;
    SysmlBuildContext *build_ctx = ctx->build_ctx;
    if (!build_ctx) return;

    /* Convert offsets to pointers */
    const char *start = ctx->input + start_offset;
    const char *end = ctx->input + end_offset;

    /* Extract comment text without the leading // */
    size_t len = end - start;
    if (len < 2) return;  /* Must have at least // */

    const char *text = start + 2;  /* Skip // */
    size_t text_len = len - 2;

    /* Trim leading whitespace */
    while (text_len > 0 && (*text == ' ' || *text == '\t')) {
        text++;
        text_len--;
    }

    /* Intern the comment text */
    const char *interned_text = text_len > 0 ? sysml2_intern_n(build_ctx->intern, text, text_len) : NULL;

    /* Create trivia node */
    SysmlTrivia *trivia = sysml2_build_trivia(build_ctx, SYSML_TRIVIA_LINE_COMMENT, interned_text, SYSML2_LOC_INVALID);
    if (trivia) {
        sysml2_build_add_pending_trivia(build_ctx, trivia);
    }
}

void sysml2_capture_block_comment(struct Sysml2ParserContext *pctx, size_t start_offset, size_t end_offset) {
    if (!pctx) return;

    typedef struct {
        const char *filename;
        const char *input;
        size_t input_len;
        size_t input_pos;
        int error_count;
        int line;
        int col;
        size_t furthest_pos;
        int furthest_line;
        int furthest_col;
        const char *failed_rules[16];
        int failed_rule_count;
        const char *context_rule;
        const char *last_keyword;
        size_t last_keyword_pos;
        SysmlBuildContext *build_ctx;
    } ParserCtx;

    ParserCtx *ctx = (ParserCtx *)pctx;
    SysmlBuildContext *build_ctx = ctx->build_ctx;
    if (!build_ctx) return;

    /* Convert offsets to pointers */
    const char *start = ctx->input + start_offset;
    const char *end = ctx->input + end_offset;

    /* Extract doc comment text (delimiters are 3 chars open + 2 chars close) */
    size_t len = end - start;
    if (len < 5) return;  /* Must have at least 5 chars total */

    const char *text = start + 3;  /* Skip opening 3-char delimiter */
    size_t text_len = len - 5;     /* Remove 5 chars total for delimiters */

    /* Intern the comment text */
    const char *interned_text = text_len > 0 ? sysml2_intern_n(build_ctx->intern, text, text_len) : NULL;

    /* Create trivia node */
    SysmlTrivia *trivia = sysml2_build_trivia(build_ctx, SYSML_TRIVIA_BLOCK_COMMENT, interned_text, SYSML2_LOC_INVALID);
    if (trivia) {
        sysml2_build_add_pending_trivia(build_ctx, trivia);
    }
}

void sysml2_capture_blank_lines(struct Sysml2ParserContext *pctx, size_t start_offset, size_t end_offset) {
    if (!pctx) return;

    typedef struct {
        const char *filename;
        const char *input;
        size_t input_len;
        size_t input_pos;
        int error_count;
        int line;
        int col;
        size_t furthest_pos;
        int furthest_line;
        int furthest_col;
        const char *failed_rules[16];
        int failed_rule_count;
        const char *context_rule;
        const char *last_keyword;
        size_t last_keyword_pos;
        SysmlBuildContext *build_ctx;
    } ParserCtx;

    ParserCtx *ctx = (ParserCtx *)pctx;
    SysmlBuildContext *build_ctx = ctx->build_ctx;
    if (!build_ctx) return;

    /* Count the number of blank lines (consecutive newlines indicate blank lines) */
    size_t len = end_offset - start_offset;
    if (len < 2) return;  /* Need at least 2 newlines for a blank line */

    /* Create trivia node - text is NULL for blank lines */
    SysmlTrivia *trivia = sysml2_build_trivia(build_ctx, SYSML_TRIVIA_BLANK_LINE, NULL, SYSML2_LOC_INVALID);
    if (trivia) {
        sysml2_build_add_pending_trivia(build_ctx, trivia);
    }
}

void sysml2_capture_documentation(struct Sysml2ParserContext *pctx, size_t start_offset, size_t end_offset) {
    if (!pctx) return;

    typedef struct {
        const char *filename;
        const char *input;
        size_t input_len;
        size_t input_pos;
        int error_count;
        int line;
        int col;
        size_t furthest_pos;
        int furthest_line;
        int furthest_col;
        const char *failed_rules[16];
        int failed_rule_count;
        const char *context_rule;
        const char *last_keyword;
        size_t last_keyword_pos;
        SysmlBuildContext *build_ctx;
    } ParserCtx;

    ParserCtx *ctx = (ParserCtx *)pctx;
    SysmlBuildContext *build_ctx = ctx->build_ctx;
    if (!build_ctx) return;

    /* Get the documentation text */
    const char *start = ctx->input + start_offset;
    size_t len = end_offset - start_offset;

    /* Intern the documentation text (includes delimiters) */
    const char *interned_text = len > 0 ? sysml2_intern_n(build_ctx->intern, start, len) : NULL;

    /* Find the current scope's node and attach documentation */
    const char *current_scope = sysml2_build_current_scope(build_ctx);
    if (current_scope) {
        for (size_t i = build_ctx->element_count; i > 0; i--) {
            SysmlNode *node = build_ctx->elements[i - 1];
            if (node->id == current_scope) {
                node->documentation = interned_text;
                break;
            }
        }
    }
}

/*
 * Grow the aliases array if needed
 */
static void ensure_alias_capacity(SysmlBuildContext *ctx) {
    SYSML2_ARRAY_GROW(ctx->arena, ctx->aliases, ctx->alias_count,
                      ctx->alias_capacity, SysmlAlias *);
}

/*
 * Build an alias
 */
void sysml2_build_alias(
    SysmlBuildContext *ctx,
    const char *name,
    size_t name_len,
    const char *target,
    size_t target_len
) {
    if (!ctx || !name || !target) return;

    ensure_alias_capacity(ctx);

    SysmlAlias *alias = SYSML2_ARENA_NEW(ctx->arena, SysmlAlias);
    if (!alias) return;

    /* Trim leading/trailing whitespace from name */
    while (name_len > 0 && (*name == ' ' || *name == '\t' || *name == '\n' || *name == '\r')) { name++; name_len--; }
    while (name_len > 0 && (name[name_len-1] == ' ' || name[name_len-1] == '\t' || name[name_len-1] == '\n' || name[name_len-1] == '\r')) name_len--;

    /* Trim leading/trailing whitespace from target */
    while (target_len > 0 && (*target == ' ' || *target == '\t' || *target == '\n' || *target == '\r')) { target++; target_len--; }
    while (target_len > 0 && (target[target_len-1] == ' ' || target[target_len-1] == '\t' || target[target_len-1] == '\n' || target[target_len-1] == '\r')) target_len--;

    if (name_len == 0 || target_len == 0) return;

    /* Generate a unique ID for the alias */
    char id_buf[256];
    snprintf(id_buf, sizeof(id_buf), "_alias_%zu", ctx->alias_count);
    alias->id = sysml2_intern(ctx->intern, id_buf);
    alias->name = sysml2_intern_n(ctx->intern, name, name_len);
    alias->target = sysml2_intern_n(ctx->intern, target, target_len);
    alias->owner_scope = sysml2_build_current_scope(ctx);
    alias->loc = SYSML2_LOC_INVALID;

    ctx->aliases[ctx->alias_count++] = alias;
}

/*
 * Capture multiplicity bounds
 */
void sysml2_capture_multiplicity(SysmlBuildContext *ctx, const char *text, size_t len) {
    if (!ctx || !text || len == 0) return;

    /* Skip leading/trailing whitespace */
    while (len > 0 && (*text == ' ' || *text == '\t')) { text++; len--; }
    while (len > 0 && (text[len-1] == ' ' || text[len-1] == '\t')) len--;
    if (len == 0) return;

    /* Look for ".." to split lower..upper */
    const char *dotdot = NULL;
    for (size_t i = 0; i + 1 < len; i++) {
        if (text[i] == '.' && text[i + 1] == '.') {
            dotdot = text + i;
            break;
        }
    }

    if (dotdot) {
        /* Has range: lower..upper */
        size_t lower_len = dotdot - text;
        size_t upper_len = len - lower_len - 2;

        /* Trim whitespace around bounds */
        const char *lower = text;
        while (lower_len > 0 && (lower[lower_len-1] == ' ' || lower[lower_len-1] == '\t')) lower_len--;

        const char *upper = dotdot + 2;
        while (upper_len > 0 && (*upper == ' ' || *upper == '\t')) { upper++; upper_len--; }
        while (upper_len > 0 && (upper[upper_len-1] == ' ' || upper[upper_len-1] == '\t')) upper_len--;

        if (lower_len > 0) {
            ctx->pending_multiplicity_lower = sysml2_intern_n(ctx->intern, lower, lower_len);
        }
        if (upper_len > 0) {
            ctx->pending_multiplicity_upper = sysml2_intern_n(ctx->intern, upper, upper_len);
        }
    } else {
        /* Single value: just lower */
        ctx->pending_multiplicity_lower = sysml2_intern_n(ctx->intern, text, len);
        ctx->pending_multiplicity_upper = NULL;
    }
}

/*
 * Capture a default value
 */
void sysml2_capture_default_value(SysmlBuildContext *ctx, const char *text, size_t len, bool has_default_keyword) {
    if (!ctx || !text || len == 0) return;

    /* Skip leading/trailing whitespace */
    while (len > 0 && (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r')) { text++; len--; }
    while (len > 0 && (text[len-1] == ' ' || text[len-1] == '\t' || text[len-1] == '\n' || text[len-1] == '\r')) len--;

    if (len > 0) {
        ctx->pending_default_value = sysml2_intern_n(ctx->intern, text, len);
        ctx->pending_has_default_keyword = has_default_keyword;
    }
}

/*
 * Capture the abstract modifier
 */
void sysml2_capture_abstract(SysmlBuildContext *ctx) {
    if (!ctx) return;
    ctx->pending_abstract = true;
}

/*
 * Capture the variation modifier
 */
void sysml2_capture_variation(SysmlBuildContext *ctx) {
    if (!ctx) return;
    ctx->pending_variation = true;
}

/*
 * Capture the ref modifier
 */
void sysml2_capture_ref(SysmlBuildContext *ctx) {
    if (!ctx) return;
    ctx->pending_ref = true;
}

/*
 * Capture direction (in/out/inout)
 */
void sysml2_capture_direction(SysmlBuildContext *ctx, SysmlDirection dir) {
    if (!ctx) return;
    ctx->pending_direction = dir;
}

/*
 * Capture import visibility (private/public)
 */
void sysml2_capture_import_visibility(SysmlBuildContext *ctx, bool is_private) {
    if (!ctx) return;
    ctx->pending_import_private = is_private;
}

/*
 * Capture parameter kind (item, part, attribute, etc.)
 */
void sysml2_capture_param_kind(SysmlBuildContext *ctx, SysmlNodeKind kind) {
    if (!ctx) return;
    ctx->pending_param_kind = kind;
}

/*
 * Clear all pending modifiers
 */
void sysml2_build_clear_pending_modifiers(SysmlBuildContext *ctx) {
    if (!ctx) return;

    ctx->pending_abstract = false;
    ctx->pending_variation = false;
    ctx->pending_readonly = false;
    ctx->pending_derived = false;
    ctx->pending_ref = false;
    ctx->pending_direction = SYSML_DIR_NONE;
    ctx->pending_visibility = SYSML_VIS_PUBLIC;
    ctx->pending_multiplicity_lower = NULL;
    ctx->pending_multiplicity_upper = NULL;
    ctx->pending_default_value = NULL;
    ctx->pending_has_default_keyword = false;
    ctx->pending_param_kind = SYSML_KIND_UNKNOWN;
}

/*
 * Helper: Trim whitespace from a string
 */
static const char *trim_and_intern(SysmlBuildContext *ctx, const char *text, size_t len) {
    if (!text || len == 0) return NULL;

    /* Trim leading whitespace */
    while (len > 0 && (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r')) {
        text++;
        len--;
    }

    /* Trim trailing whitespace */
    while (len > 0 && (text[len-1] == ' ' || text[len-1] == '\t' || text[len-1] == '\n' || text[len-1] == '\r')) {
        len--;
    }

    if (len == 0) return NULL;
    return sysml2_intern_n(ctx->intern, text, len);
}

/*
 * Helper: Ensure pending statement array has capacity
 */
static void ensure_pending_stmt_capacity(SysmlBuildContext *ctx) {
    if (ctx->pending_stmt_count >= ctx->pending_stmt_capacity) {
        size_t new_capacity = ctx->pending_stmt_capacity * 2;
        SysmlStatement **new_stmts = SYSML2_ARENA_NEW_ARRAY(ctx->arena, SysmlStatement *, new_capacity);
        if (!new_stmts) return;
        if (ctx->pending_stmts) {
            memcpy(new_stmts, ctx->pending_stmts, ctx->pending_stmt_count * sizeof(SysmlStatement *));
        }
        ctx->pending_stmts = new_stmts;
        ctx->pending_stmt_capacity = new_capacity;
    }
}

/*
 * Helper: Create a new statement
 */
static SysmlStatement *create_statement(SysmlBuildContext *ctx, SysmlStatementKind kind) {
    SysmlStatement *stmt = SYSML2_ARENA_NEW(ctx->arena, SysmlStatement);
    if (!stmt) return NULL;

    stmt->kind = kind;
    stmt->loc = SYSML2_LOC_INVALID;
    stmt->raw_text = NULL;
    stmt->source.target = NULL;
    stmt->source.feature_chain = NULL;
    stmt->source.multiplicity = NULL;
    stmt->target.target = NULL;
    stmt->target.feature_chain = NULL;
    stmt->target.multiplicity = NULL;
    stmt->name = NULL;
    stmt->guard = NULL;
    stmt->payload = NULL;
    stmt->nested = NULL;
    stmt->nested_count = 0;

    return stmt;
}

/*
 * Helper: Add statement to pending list
 */
static void add_pending_stmt(SysmlBuildContext *ctx, SysmlStatement *stmt) {
    if (!ctx || !stmt) return;
    ensure_pending_stmt_capacity(ctx);
    ctx->pending_stmts[ctx->pending_stmt_count++] = stmt;
}

/*
 * Capture a bind statement
 */
void sysml2_capture_bind(
    SysmlBuildContext *ctx,
    const char *source, size_t source_len,
    const char *target, size_t target_len
) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_BIND);
    if (!stmt) return;

    stmt->source.target = trim_and_intern(ctx, source, source_len);
    stmt->target.target = trim_and_intern(ctx, target, target_len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture a connect statement
 */
void sysml2_capture_connect(
    SysmlBuildContext *ctx,
    const char *source, size_t source_len,
    const char *target, size_t target_len
) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_CONNECT);
    if (!stmt) return;

    stmt->source.target = trim_and_intern(ctx, source, source_len);
    stmt->target.target = trim_and_intern(ctx, target, target_len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture a flow statement
 */
void sysml2_capture_flow(
    SysmlBuildContext *ctx,
    const char *payload, size_t payload_len,
    const char *source, size_t source_len,
    const char *target, size_t target_len
) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_FLOW);
    if (!stmt) return;

    stmt->payload = trim_and_intern(ctx, payload, payload_len);
    stmt->source.target = trim_and_intern(ctx, source, source_len);
    stmt->target.target = trim_and_intern(ctx, target, target_len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture a succession statement
 */
void sysml2_capture_succession(
    SysmlBuildContext *ctx,
    const char *source, size_t source_len,
    const char *target, size_t target_len,
    const char *guard, size_t guard_len
) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_SUCCESSION);
    if (!stmt) return;

    stmt->source.target = trim_and_intern(ctx, source, source_len);
    stmt->target.target = trim_and_intern(ctx, target, target_len);
    stmt->guard = trim_and_intern(ctx, guard, guard_len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture an entry action
 */
void sysml2_capture_entry(SysmlBuildContext *ctx, const char *text, size_t len) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_ENTRY);
    if (!stmt) return;

    stmt->raw_text = trim_and_intern(ctx, text, len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture an exit action
 */
void sysml2_capture_exit(SysmlBuildContext *ctx, const char *text, size_t len) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_EXIT);
    if (!stmt) return;

    stmt->raw_text = trim_and_intern(ctx, text, len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture a do action
 */
void sysml2_capture_do(SysmlBuildContext *ctx, const char *text, size_t len) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_DO);
    if (!stmt) return;

    stmt->raw_text = trim_and_intern(ctx, text, len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture a transition
 */
void sysml2_capture_transition(SysmlBuildContext *ctx, const char *text, size_t len) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_TRANSITION);
    if (!stmt) return;

    stmt->raw_text = trim_and_intern(ctx, text, len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture an entry transition (then X; without entry keyword)
 * Uses SYSML_STMT_THEN which outputs raw text without prefix
 */
void sysml2_capture_entry_transition(SysmlBuildContext *ctx, const char *text, size_t len) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_THEN);
    if (!stmt) return;

    stmt->raw_text = trim_and_intern(ctx, text, len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture a send action
 */
void sysml2_capture_send(SysmlBuildContext *ctx, const char *text, size_t len) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_SEND);
    if (!stmt) return;

    stmt->raw_text = trim_and_intern(ctx, text, len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture an accept action
 */
void sysml2_capture_accept_action(SysmlBuildContext *ctx, const char *text, size_t len) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_ACCEPT_ACTION);
    if (!stmt) return;

    stmt->raw_text = trim_and_intern(ctx, text, len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture an assignment action
 */
void sysml2_capture_assign(
    SysmlBuildContext *ctx,
    const char *target, size_t target_len,
    const char *expr, size_t expr_len
) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_ASSIGN);
    if (!stmt) return;

    stmt->target.target = trim_and_intern(ctx, target, target_len);
    stmt->raw_text = trim_and_intern(ctx, expr, expr_len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture an if action
 */
void sysml2_capture_if(SysmlBuildContext *ctx, const char *text, size_t len) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_IF);
    if (!stmt) return;

    stmt->raw_text = trim_and_intern(ctx, text, len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture a while/loop
 */
void sysml2_capture_while(SysmlBuildContext *ctx, const char *text, size_t len) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_WHILE);
    if (!stmt) return;

    stmt->raw_text = trim_and_intern(ctx, text, len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture a for loop
 */
void sysml2_capture_for(SysmlBuildContext *ctx, const char *text, size_t len) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_FOR);
    if (!stmt) return;

    stmt->raw_text = trim_and_intern(ctx, text, len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture a control node
 */
void sysml2_capture_control_node(
    SysmlBuildContext *ctx,
    SysmlStatementKind kind,
    const char *text, size_t len
) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, kind);
    if (!stmt) return;

    stmt->raw_text = trim_and_intern(ctx, text, len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture a terminate action
 */
void sysml2_capture_terminate(SysmlBuildContext *ctx) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_TERMINATE);
    if (!stmt) return;

    add_pending_stmt(ctx, stmt);
}

/*
 * Helper: Ensure pending comment array has capacity
 */
static void ensure_pending_comment_capacity(SysmlBuildContext *ctx) {
    if (ctx->pending_comment_count >= ctx->pending_comment_capacity) {
        size_t new_capacity = ctx->pending_comment_capacity * 2;
        SysmlNamedComment **new_comments = SYSML2_ARENA_NEW_ARRAY(ctx->arena, SysmlNamedComment *, new_capacity);
        if (!new_comments) return;
        if (ctx->pending_comments) {
            memcpy(new_comments, ctx->pending_comments, ctx->pending_comment_count * sizeof(SysmlNamedComment *));
        }
        ctx->pending_comments = new_comments;
        ctx->pending_comment_capacity = new_capacity;
    }
}

/*
 * Capture a named comment
 */
void sysml2_capture_named_comment(
    SysmlBuildContext *ctx,
    const char *name, size_t name_len,
    const char *about, size_t about_len,
    const char *text, size_t text_len
) {
    if (!ctx) return;

    ensure_pending_comment_capacity(ctx);

    SysmlNamedComment *comment = SYSML2_ARENA_NEW(ctx->arena, SysmlNamedComment);
    if (!comment) return;

    /* Generate unique ID */
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "_comment_%zu", ++ctx->comment_counter);
    comment->id = sysml2_intern(ctx->intern, id_buf);

    comment->name = trim_and_intern(ctx, name, name_len);
    comment->locale = NULL;
    comment->text = trim_and_intern(ctx, text, text_len);
    comment->loc = SYSML2_LOC_INVALID;

    /* Parse about targets */
    comment->about = NULL;
    comment->about_count = 0;
    if (about && about_len > 0) {
        const char *trimmed_about = trim_and_intern(ctx, about, about_len);
        if (trimmed_about) {
            comment->about = SYSML2_ARENA_NEW_ARRAY(ctx->arena, const char *, 1);
            if (comment->about) {
                comment->about[0] = trimmed_about;
                comment->about_count = 1;
            }
        }
    }

    ctx->pending_comments[ctx->pending_comment_count++] = comment;
}

/*
 * Helper: Ensure pending rep array has capacity
 */
static void ensure_pending_rep_capacity(SysmlBuildContext *ctx) {
    if (ctx->pending_rep_count >= ctx->pending_rep_capacity) {
        size_t new_capacity = ctx->pending_rep_capacity * 2;
        SysmlTextualRep **new_reps = SYSML2_ARENA_NEW_ARRAY(ctx->arena, SysmlTextualRep *, new_capacity);
        if (!new_reps) return;
        if (ctx->pending_reps) {
            memcpy(new_reps, ctx->pending_reps, ctx->pending_rep_count * sizeof(SysmlTextualRep *));
        }
        ctx->pending_reps = new_reps;
        ctx->pending_rep_capacity = new_capacity;
    }
}

/*
 * Capture a textual representation
 */
void sysml2_capture_textual_rep(
    SysmlBuildContext *ctx,
    const char *name, size_t name_len,
    const char *lang, size_t lang_len,
    const char *text, size_t text_len
) {
    if (!ctx) return;

    ensure_pending_rep_capacity(ctx);

    SysmlTextualRep *rep = SYSML2_ARENA_NEW(ctx->arena, SysmlTextualRep);
    if (!rep) return;

    /* Generate unique ID */
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "_rep_%zu", ++ctx->rep_counter);
    rep->id = sysml2_intern(ctx->intern, id_buf);

    rep->name = trim_and_intern(ctx, name, name_len);
    rep->language = trim_and_intern(ctx, lang, lang_len);
    rep->text = trim_and_intern(ctx, text, text_len);
    rep->loc = SYSML2_LOC_INVALID;

    ctx->pending_reps[ctx->pending_rep_count++] = rep;
}

/*
 * Capture a result expression
 */
void sysml2_capture_result_expr(SysmlBuildContext *ctx, const char *expr, size_t len) {
    if (!ctx || !expr || len == 0) return;

    /* Find the current scope's node and attach result expression */
    const char *current_scope = sysml2_build_current_scope(ctx);
    if (current_scope) {
        for (size_t i = ctx->element_count; i > 0; i--) {
            SysmlNode *node = ctx->elements[i - 1];
            if (node->id == current_scope) {
                node->result_expression = trim_and_intern(ctx, expr, len);
                break;
            }
        }
    }
}

/*
 * Capture a standalone metadata usage (metadata X about Y, Z;)
 */
void sysml2_capture_metadata_usage(SysmlBuildContext *ctx, const char *text, size_t len) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_METADATA_USAGE);
    if (!stmt) return;

    stmt->raw_text = trim_and_intern(ctx, text, len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Capture a shorthand feature (:> name : Type; or :>> name = value;)
 */
void sysml2_capture_shorthand_feature(SysmlBuildContext *ctx, const char *text, size_t len) {
    if (!ctx) return;

    SysmlStatement *stmt = create_statement(ctx, SYSML_STMT_SHORTHAND_FEATURE);
    if (!stmt) return;

    stmt->raw_text = trim_and_intern(ctx, text, len);

    add_pending_stmt(ctx, stmt);
}

/*
 * Attach pending body statements to a node (appends to existing)
 */
void sysml2_attach_pending_stmts(SysmlBuildContext *ctx, SysmlNode *node) {
    if (!ctx || !node) return;

    /* Append pending statements to existing */
    if (ctx->pending_stmt_count > 0) {
        size_t new_count = node->body_stmt_count + ctx->pending_stmt_count;
        SysmlStatement **new_stmts = SYSML2_ARENA_NEW_ARRAY(ctx->arena, SysmlStatement *, new_count);
        if (new_stmts) {
            /* Copy existing statements first */
            if (node->body_stmts && node->body_stmt_count > 0) {
                memcpy(new_stmts, node->body_stmts, node->body_stmt_count * sizeof(SysmlStatement *));
            }
            /* Append pending statements */
            memcpy(new_stmts + node->body_stmt_count, ctx->pending_stmts, ctx->pending_stmt_count * sizeof(SysmlStatement *));
            node->body_stmts = new_stmts;
            node->body_stmt_count = new_count;
        }
        ctx->pending_stmt_count = 0;
    }

    /* Append pending comments to existing */
    if (ctx->pending_comment_count > 0) {
        size_t new_count = node->comment_count + ctx->pending_comment_count;
        SysmlNamedComment **new_comments = SYSML2_ARENA_NEW_ARRAY(ctx->arena, SysmlNamedComment *, new_count);
        if (new_comments) {
            if (node->comments && node->comment_count > 0) {
                memcpy(new_comments, node->comments, node->comment_count * sizeof(SysmlNamedComment *));
            }
            memcpy(new_comments + node->comment_count, ctx->pending_comments, ctx->pending_comment_count * sizeof(SysmlNamedComment *));
            node->comments = new_comments;
            node->comment_count = new_count;
        }
        ctx->pending_comment_count = 0;
    }

    /* Append pending textual reps to existing */
    if (ctx->pending_rep_count > 0) {
        size_t new_count = node->textual_rep_count + ctx->pending_rep_count;
        SysmlTextualRep **new_reps = SYSML2_ARENA_NEW_ARRAY(ctx->arena, SysmlTextualRep *, new_count);
        if (new_reps) {
            if (node->textual_reps && node->textual_rep_count > 0) {
                memcpy(new_reps, node->textual_reps, node->textual_rep_count * sizeof(SysmlTextualRep *));
            }
            memcpy(new_reps + node->textual_rep_count, ctx->pending_reps, ctx->pending_rep_count * sizeof(SysmlTextualRep *));
            node->textual_reps = new_reps;
            node->textual_rep_count = new_count;
        }
        ctx->pending_rep_count = 0;
    }
}
