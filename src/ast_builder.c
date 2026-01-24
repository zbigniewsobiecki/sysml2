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

    /* Initialize trivia list */
    ctx->pending_trivia_head = NULL;
    ctx->pending_trivia_tail = NULL;

    /* Initialize pending prefix metadata */
    ctx->pending_prefix_metadata_capacity = 8;
    ctx->pending_prefix_metadata = SYSML2_ARENA_NEW_ARRAY(arena, const char *, ctx->pending_prefix_metadata_capacity);
    ctx->pending_prefix_metadata_count = 0;

    /* Initialize current metadata */
    ctx->current_metadata = NULL;

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
    node->loc = SYSML2_LOC_INVALID;
    node->metadata = NULL;
    node->metadata_count = 0;
    node->prefix_metadata = NULL;
    node->prefix_metadata_count = 0;
    node->leading_trivia = NULL;
    node->trailing_trivia = NULL;

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
    imp->loc = SYSML2_LOC_INVALID;

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
 * Finish building a metadata usage and attach to current scope's node
 */
void sysml2_build_end_metadata(SysmlBuildContext *ctx) {
    if (!ctx || !ctx->current_metadata) return;

    /* Find the current scope's node (most recently added element in current scope) */
    const char *current_scope = sysml2_build_current_scope(ctx);
    if (current_scope) {
        /* Find the node with this ID */
        for (size_t i = ctx->element_count; i > 0; i--) {
            SysmlNode *node = ctx->elements[i - 1];
            if (node->id == current_scope) {
                sysml2_build_add_metadata(ctx, node, ctx->current_metadata);
                break;
            }
        }
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
