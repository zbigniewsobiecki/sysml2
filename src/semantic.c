/*
 * SysML v2 Parser - Semantic Analysis Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/semantic.h"
#include <string.h>

/* Forward declarations */
static void analyze_namespace(Sysml2SemanticContext *ctx, Sysml2AstNamespace *ns);
static void analyze_package(Sysml2SemanticContext *ctx, Sysml2AstPackage *pkg);
static void analyze_classifier(Sysml2SemanticContext *ctx, Sysml2AstClassifier *cls);
static void analyze_feature(Sysml2SemanticContext *ctx, Sysml2AstFeature *feat);
static void analyze_member(Sysml2SemanticContext *ctx, Sysml2AstMember *member);
static void analyze_members(Sysml2SemanticContext *ctx, Sysml2AstMember *members);
static void check_relationship(
    Sysml2SemanticContext *ctx,
    Sysml2AstRelationship *rel,
    const char *context_name
);

/* Built-in type names */
static const char *builtin_types[] = {
    "Base",
    "Anything",
    "Boolean",
    "String",
    "Integer",
    "Real",
    "Natural",
    "Positive",
    "UnlimitedNatural",
    "Occurrence",
    "Object",
    "Link",
    "Classifier",
    "Type",
    "Feature",
    "Class",
    "DataType",
    "Struct",
    "Association",
    "Behavior",
    "Function",
    "Predicate",
    NULL
};

void sysml2_semantic_init(
    Sysml2SemanticContext *ctx,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    Sysml2DiagContext *diag
) {
    ctx->arena = arena;
    ctx->intern = intern;
    ctx->diag = diag;
    ctx->in_type_body = false;
    ctx->checking_cycles = false;

    sysml2_symtab_init(&ctx->symbols, arena);
}

void sysml2_semantic_destroy(Sysml2SemanticContext *ctx) {
    sysml2_symtab_destroy(&ctx->symbols);
}

/* Register built-in types in the symbol table */
void sysml2_semantic_register_builtins(Sysml2SemanticContext *ctx) {
    for (int i = 0; builtin_types[i]; i++) {
        const char *name = sysml2_intern(ctx->intern, builtin_types[i]);
        sysml2_symtab_define(
            &ctx->symbols,
            name,
            SYSML2_SYM_TYPE,
            SYSML2_VIS_PUBLIC,
            NULL, /* No AST node for builtins */
            SYSML2_RANGE_INVALID
        );
    }
}

/* Check if a relationship target exists */
static void check_relationship(
    Sysml2SemanticContext *ctx,
    Sysml2AstRelationship *rel,
    const char *context_name
) {
    if (!rel || !rel->target) return;

    Sysml2Symbol *sym = sysml2_symtab_lookup_qualified(&ctx->symbols, rel->target);

    if (!sym) {
        /* Try simple lookup for first segment */
        if (rel->target->segment_count > 0) {
            sym = sysml2_symtab_lookup(&ctx->symbols, rel->target->segments[0]);
        }
    }

    if (!sym) {
        /* Undefined type reference */
        Sysml2DiagCode code;
        const char *rel_kind;

        switch (rel->kind) {
            case SYSML2_REL_TYPED_BY:
                code = SYSML2_DIAG_E3001_UNDEFINED_TYPE;
                rel_kind = "type";
                break;
            case SYSML2_REL_SPECIALIZES:
                code = SYSML2_DIAG_E3001_UNDEFINED_TYPE;
                rel_kind = "supertype";
                break;
            case SYSML2_REL_SUBSETS:
                code = SYSML2_DIAG_E3002_UNDEFINED_FEATURE;
                rel_kind = "subsetted feature";
                break;
            case SYSML2_REL_REDEFINES:
                code = SYSML2_DIAG_E3002_UNDEFINED_FEATURE;
                rel_kind = "redefined feature";
                break;
            default:
                code = SYSML2_DIAG_E3001_UNDEFINED_TYPE;
                rel_kind = "reference";
                break;
        }

        char *target_str = sysml2_ast_qname_to_string(ctx->arena, rel->target);
        char *message = sysml2_arena_sprintf(ctx->arena,
            "undefined %s '%s'", rel_kind, target_str);

        Sysml2Diagnostic *diag = sysml2_diag_create(
            ctx->diag,
            code,
            SYSML2_SEVERITY_ERROR,
            NULL, /* Source file would need to be passed through */
            rel->target->range,
            message
        );

        /* Try to find similar name for suggestion */
        if (rel->target->segment_count > 0) {
            const char *similar = sysml2_symtab_find_similar(
                &ctx->symbols,
                rel->target->segments[rel->target->segment_count - 1],
                3 /* max distance */
            );
            if (similar) {
                char *help = sysml2_arena_sprintf(ctx->arena,
                    "did you mean '%s'?", similar);
                sysml2_diag_add_help(diag, ctx->diag, help);
            }
        }

        sysml2_diag_emit(ctx->diag, diag);
    }
}

/* First pass: collect all definitions */
static void collect_definitions_member(Sysml2SemanticContext *ctx, Sysml2AstMember *member) {
    if (!member || !member->node) return;

    const char *name = NULL;
    Sysml2SymbolKind kind = SYSML2_SYM_NAMESPACE;
    Sysml2SourceRange range = SYSML2_RANGE_INVALID;

    switch (member->kind) {
        case SYSML2_AST_NAMESPACE: {
            Sysml2AstNamespace *ns = (Sysml2AstNamespace *)member->node;
            name = ns->name;
            kind = SYSML2_SYM_NAMESPACE;
            range = ns->range;
            break;
        }
        case SYSML2_AST_PACKAGE: {
            Sysml2AstPackage *pkg = (Sysml2AstPackage *)member->node;
            name = pkg->name;
            kind = SYSML2_SYM_PACKAGE;
            range = pkg->range;
            break;
        }
        case SYSML2_AST_TYPE:
        case SYSML2_AST_CLASSIFIER:
        case SYSML2_AST_CLASS:
        case SYSML2_AST_DATATYPE:
        case SYSML2_AST_STRUCT:
        case SYSML2_AST_ASSOC:
        case SYSML2_AST_BEHAVIOR:
        case SYSML2_AST_FUNCTION:
        case SYSML2_AST_PREDICATE:
        case SYSML2_AST_PART_DEF:
        case SYSML2_AST_ACTION_DEF:
        case SYSML2_AST_STATE_DEF:
        case SYSML2_AST_REQUIREMENT_DEF:
        case SYSML2_AST_CONSTRAINT_DEF:
        case SYSML2_AST_PORT_DEF:
        case SYSML2_AST_ATTRIBUTE_DEF:
        case SYSML2_AST_ITEM_DEF: {
            Sysml2AstClassifier *cls = (Sysml2AstClassifier *)member->node;
            name = cls->name;
            kind = SYSML2_SYM_CLASSIFIER;
            range = cls->range;
            break;
        }
        case SYSML2_AST_FEATURE:
        case SYSML2_AST_CONNECTOR:
        case SYSML2_AST_BINDING:
        case SYSML2_AST_SUCCESSION:
        case SYSML2_AST_PART_USAGE:
        case SYSML2_AST_ACTION_USAGE:
        case SYSML2_AST_STATE_USAGE:
        case SYSML2_AST_REQUIREMENT_USAGE:
        case SYSML2_AST_CONSTRAINT_USAGE:
        case SYSML2_AST_PORT_USAGE:
        case SYSML2_AST_ATTRIBUTE_USAGE:
        case SYSML2_AST_ITEM_USAGE: {
            Sysml2AstFeature *feat = (Sysml2AstFeature *)member->node;
            name = feat->name;
            kind = SYSML2_SYM_FEATURE;
            range = feat->range;
            break;
        }
        case SYSML2_AST_ALIAS: {
            Sysml2AstAlias *alias = (Sysml2AstAlias *)member->node;
            name = alias->name;
            kind = SYSML2_SYM_ALIAS;
            range = alias->range;
            break;
        }
        default:
            return;
    }

    if (name) {
        /* Check for duplicate */
        if (sysml2_symtab_has_local(&ctx->symbols, name)) {
            char *message = sysml2_arena_sprintf(ctx->arena,
                "duplicate definition of '%s'", name);

            Sysml2Diagnostic *diag = sysml2_diag_create(
                ctx->diag,
                SYSML2_DIAG_E3004_DUPLICATE_NAME,
                SYSML2_SEVERITY_ERROR,
                NULL,
                range,
                message
            );

            /* Add note pointing to previous definition */
            Sysml2Symbol *existing = sysml2_symtab_lookup_local(&ctx->symbols, name);
            if (existing && existing->definition.start.line > 0) {
                sysml2_diag_add_note(
                    diag,
                    ctx->diag,
                    NULL,
                    existing->definition,
                    "previous definition was here"
                );
            }

            sysml2_diag_emit(ctx->diag, diag);
        } else {
            sysml2_symtab_define(
                &ctx->symbols,
                name,
                kind,
                member->visibility,
                member->node,
                range
            );
        }
    }
}

static void collect_definitions(Sysml2SemanticContext *ctx, Sysml2AstMember *members) {
    for (Sysml2AstMember *m = members; m; m = m->next) {
        collect_definitions_member(ctx, m);
    }
}

/* Analyze a feature */
static void analyze_feature(Sysml2SemanticContext *ctx, Sysml2AstFeature *feat) {
    if (!feat) return;

    /* Check relationship targets */
    for (Sysml2AstRelationship *rel = feat->relationships; rel; rel = rel->next) {
        check_relationship(ctx, rel, feat->name);
    }

    /* Analyze nested members */
    if (feat->members) {
        sysml2_symtab_push_scope(&ctx->symbols, feat->name, SYSML2_SYM_FEATURE);
        collect_definitions(ctx, feat->members);
        analyze_members(ctx, feat->members);
        sysml2_symtab_pop_scope(&ctx->symbols);
    }
}

/* Analyze a classifier */
static void analyze_classifier(Sysml2SemanticContext *ctx, Sysml2AstClassifier *cls) {
    if (!cls) return;

    /* Check relationship targets */
    for (Sysml2AstRelationship *rel = cls->relationships; rel; rel = rel->next) {
        check_relationship(ctx, rel, cls->name);
    }

    /* Analyze members */
    if (cls->members) {
        sysml2_symtab_push_scope(&ctx->symbols, cls->name, SYSML2_SYM_CLASSIFIER);
        ctx->in_type_body = true;
        collect_definitions(ctx, cls->members);
        analyze_members(ctx, cls->members);
        ctx->in_type_body = false;
        sysml2_symtab_pop_scope(&ctx->symbols);
    }
}

/* Analyze a type */
static void analyze_type(Sysml2SemanticContext *ctx, Sysml2AstType *type) {
    if (!type) return;

    /* Check relationship targets */
    for (Sysml2AstRelationship *rel = type->relationships; rel; rel = rel->next) {
        check_relationship(ctx, rel, type->name);
    }

    /* Analyze members */
    if (type->members) {
        sysml2_symtab_push_scope(&ctx->symbols, type->name, SYSML2_SYM_TYPE);
        ctx->in_type_body = true;
        collect_definitions(ctx, type->members);
        analyze_members(ctx, type->members);
        ctx->in_type_body = false;
        sysml2_symtab_pop_scope(&ctx->symbols);
    }
}

/* Analyze a single member */
static void analyze_member(Sysml2SemanticContext *ctx, Sysml2AstMember *member) {
    if (!member || !member->node) return;

    switch (member->kind) {
        case SYSML2_AST_NAMESPACE:
            analyze_namespace(ctx, (Sysml2AstNamespace *)member->node);
            break;
        case SYSML2_AST_PACKAGE:
            analyze_package(ctx, (Sysml2AstPackage *)member->node);
            break;
        case SYSML2_AST_TYPE:
            analyze_type(ctx, (Sysml2AstType *)member->node);
            break;
        case SYSML2_AST_CLASSIFIER:
        case SYSML2_AST_CLASS:
        case SYSML2_AST_DATATYPE:
        case SYSML2_AST_STRUCT:
        case SYSML2_AST_ASSOC:
        case SYSML2_AST_BEHAVIOR:
        case SYSML2_AST_FUNCTION:
        case SYSML2_AST_PREDICATE:
        case SYSML2_AST_PART_DEF:
        case SYSML2_AST_ACTION_DEF:
        case SYSML2_AST_STATE_DEF:
        case SYSML2_AST_REQUIREMENT_DEF:
        case SYSML2_AST_CONSTRAINT_DEF:
        case SYSML2_AST_PORT_DEF:
        case SYSML2_AST_ATTRIBUTE_DEF:
        case SYSML2_AST_ITEM_DEF:
            analyze_classifier(ctx, (Sysml2AstClassifier *)member->node);
            break;
        case SYSML2_AST_FEATURE:
        case SYSML2_AST_CONNECTOR:
        case SYSML2_AST_BINDING:
        case SYSML2_AST_SUCCESSION:
        case SYSML2_AST_PART_USAGE:
        case SYSML2_AST_ACTION_USAGE:
        case SYSML2_AST_STATE_USAGE:
        case SYSML2_AST_REQUIREMENT_USAGE:
        case SYSML2_AST_CONSTRAINT_USAGE:
        case SYSML2_AST_PORT_USAGE:
        case SYSML2_AST_ATTRIBUTE_USAGE:
        case SYSML2_AST_ITEM_USAGE:
            analyze_feature(ctx, (Sysml2AstFeature *)member->node);
            break;
        default:
            break;
    }
}

/* Analyze all members */
static void analyze_members(Sysml2SemanticContext *ctx, Sysml2AstMember *members) {
    for (Sysml2AstMember *m = members; m; m = m->next) {
        analyze_member(ctx, m);
    }
}

/* Analyze a namespace */
static void analyze_namespace(Sysml2SemanticContext *ctx, Sysml2AstNamespace *ns) {
    if (!ns) return;

    if (ns->name) {
        sysml2_symtab_push_scope(&ctx->symbols, ns->name, SYSML2_SYM_NAMESPACE);
    }

    /* First pass: collect all definitions */
    collect_definitions(ctx, ns->members);

    /* Second pass: check references and analyze nested scopes */
    analyze_members(ctx, ns->members);

    if (ns->name) {
        sysml2_symtab_pop_scope(&ctx->symbols);
    }
}

/* Analyze a package */
static void analyze_package(Sysml2SemanticContext *ctx, Sysml2AstPackage *pkg) {
    if (!pkg) return;

    sysml2_symtab_push_scope(&ctx->symbols, pkg->name, SYSML2_SYM_PACKAGE);

    /* First pass: collect all definitions */
    collect_definitions(ctx, pkg->members);

    /* Second pass: analyze */
    analyze_members(ctx, pkg->members);

    sysml2_symtab_pop_scope(&ctx->symbols);
}

/* Main analysis entry point */
void sysml2_semantic_analyze(Sysml2SemanticContext *ctx, Sysml2AstNamespace *ast) {
    if (!ast) return;

    /* Register built-in types */
    sysml2_semantic_register_builtins(ctx);

    /* First pass: collect all top-level definitions */
    collect_definitions(ctx, ast->members);

    /* Second pass: check references and analyze */
    analyze_members(ctx, ast->members);
}

/* Individual check implementations */
void sysml2_semantic_check_references(
    Sysml2SemanticContext *ctx,
    Sysml2AstNamespace *ast
) {
    /* Already done in main analyze pass */
    (void)ctx;
    (void)ast;
}

void sysml2_semantic_check_duplicates(
    Sysml2SemanticContext *ctx,
    Sysml2AstNamespace *ast
) {
    /* Already done during definition collection */
    (void)ctx;
    (void)ast;
}

void sysml2_semantic_check_cycles(
    Sysml2SemanticContext *ctx,
    Sysml2AstNamespace *ast
) {
    /* TODO: implement cycle detection in specialization hierarchy */
    (void)ctx;
    (void)ast;
}

void sysml2_semantic_check_typing(
    Sysml2SemanticContext *ctx,
    Sysml2AstNamespace *ast
) {
    /* TODO: implement type compatibility checking */
    (void)ctx;
    (void)ast;
}

void sysml2_semantic_check_multiplicity(
    Sysml2SemanticContext *ctx,
    Sysml2AstNamespace *ast
) {
    /* TODO: implement multiplicity constraint validation */
    (void)ctx;
    (void)ast;
}

void sysml2_semantic_resolve_imports(
    Sysml2SemanticContext *ctx,
    Sysml2AstNamespace *ast
) {
    /* TODO: implement import resolution */
    (void)ctx;
    (void)ast;
}
