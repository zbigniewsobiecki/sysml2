/*
 * SysML v2 Parser - Semantic Validator Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/validator.h"
#include "sysml2/symtab.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Symbol table implementation is now in symtab.c */

/* ========== Type Compatibility ========== */

bool sysml_is_type_compatible(SysmlNodeKind usage_kind, SysmlNodeKind def_kind) {
    /* Package types can contain anything */
    if (SYSML_KIND_IS_PACKAGE(def_kind)) return true;

    /* Map usage kinds to compatible definition kinds */
    switch (usage_kind) {
        case SYSML_KIND_PART_USAGE:
            return def_kind == SYSML_KIND_PART_DEF ||
                   def_kind == SYSML_KIND_ITEM_DEF ||
                   def_kind == SYSML_KIND_OCCURRENCE_DEF;

        case SYSML_KIND_ACTION_USAGE:
            return def_kind == SYSML_KIND_ACTION_DEF;

        case SYSML_KIND_STATE_USAGE:
            return def_kind == SYSML_KIND_STATE_DEF;

        case SYSML_KIND_PORT_USAGE:
            return def_kind == SYSML_KIND_PORT_DEF;

        case SYSML_KIND_ATTRIBUTE_USAGE:
            return def_kind == SYSML_KIND_ATTRIBUTE_DEF ||
                   def_kind == SYSML_KIND_ENUMERATION_DEF ||
                   def_kind == SYSML_KIND_DATATYPE;

        case SYSML_KIND_REQUIREMENT_USAGE:
            return def_kind == SYSML_KIND_REQUIREMENT_DEF;

        case SYSML_KIND_CONSTRAINT_USAGE:
            return def_kind == SYSML_KIND_CONSTRAINT_DEF;

        case SYSML_KIND_ITEM_USAGE:
            return def_kind == SYSML_KIND_ITEM_DEF ||
                   def_kind == SYSML_KIND_OCCURRENCE_DEF;

        case SYSML_KIND_CONNECTION_USAGE:
            return def_kind == SYSML_KIND_CONNECTION_DEF;

        case SYSML_KIND_FLOW_USAGE:
            return def_kind == SYSML_KIND_FLOW_DEF;

        case SYSML_KIND_INTERFACE_USAGE:
            return def_kind == SYSML_KIND_INTERFACE_DEF;

        case SYSML_KIND_ALLOCATION_USAGE:
            return def_kind == SYSML_KIND_ALLOCATION_DEF;

        case SYSML_KIND_CALC_USAGE:
            return def_kind == SYSML_KIND_CALC_DEF;

        case SYSML_KIND_CASE_USAGE:
            return def_kind == SYSML_KIND_CASE_DEF;

        case SYSML_KIND_ANALYSIS_USAGE:
            return def_kind == SYSML_KIND_ANALYSIS_DEF;

        case SYSML_KIND_VERIFICATION_USAGE:
            return def_kind == SYSML_KIND_VERIFICATION_DEF;

        case SYSML_KIND_USE_CASE_USAGE:
            return def_kind == SYSML_KIND_USE_CASE_DEF;

        case SYSML_KIND_VIEW_USAGE:
            return def_kind == SYSML_KIND_VIEW_DEF;

        case SYSML_KIND_VIEWPOINT_USAGE:
            return def_kind == SYSML_KIND_VIEWPOINT_DEF;

        case SYSML_KIND_RENDERING_USAGE:
            return def_kind == SYSML_KIND_RENDERING_DEF;

        case SYSML_KIND_CONCERN_USAGE:
            return def_kind == SYSML_KIND_CONCERN_DEF;

        default:
            /* For definitions specializing other definitions, check same category */
            if (SYSML_KIND_IS_DEFINITION(usage_kind) && SYSML_KIND_IS_DEFINITION(def_kind)) {
                return true; /* Definitions can specialize other definitions */
            }
            return false;
    }
}

/* ========== Validation Passes ========== */

/* Context for validation passes */
typedef struct {
    SysmlSymbolTable *symtab;
    Sysml2DiagContext *diag_ctx;
    const Sysml2SourceFile *source_file;
    const SysmlValidationOptions *options;
    bool has_errors;
} ValidationContext;

/* Pass 1: Build symbol table and detect duplicates */
static void pass1_build_symtab(
    ValidationContext *vctx,
    const SysmlSemanticModel *model
) {
    for (size_t i = 0; i < model->element_count; i++) {
        SysmlNode *node = model->elements[i];
        if (!node->name) continue; /* Skip anonymous elements */

        /* Get or create scope from parent_id */
        SysmlScope *scope = sysml_symtab_get_or_create_scope(
            vctx->symtab, node->parent_id);

        /* Try to add symbol */
        SysmlSymbol *existing = sysml_symtab_lookup(scope, node->name);
        if (existing) {
            /* Duplicate! */
            if (vctx->options->check_duplicate_names) {
                /* Create error message */
                char msg[256];
                snprintf(msg, sizeof(msg),
                    "duplicate definition of '%s'", node->name);

                Sysml2SourceRange range = SYSML2_RANGE_INVALID;
                range.start = node->loc;
                range.end = node->loc;

                Sysml2Diagnostic *diag = sysml2_diag_create(
                    vctx->diag_ctx,
                    SYSML2_DIAG_E3004_DUPLICATE_NAME,
                    SYSML2_SEVERITY_ERROR,
                    vctx->source_file,
                    range,
                    sysml2_intern(vctx->symtab->intern, msg)
                );

                /* Add note about previous definition */
                if (existing->node) {
                    char note_msg[256];
                    snprintf(note_msg, sizeof(note_msg),
                        "previous definition at line %u",
                        existing->node->loc.line);

                    Sysml2SourceRange prev_range = SYSML2_RANGE_INVALID;
                    prev_range.start = existing->node->loc;
                    prev_range.end = existing->node->loc;

                    sysml2_diag_add_note(diag, vctx->diag_ctx,
                        vctx->source_file, prev_range,
                        sysml2_intern(vctx->symtab->intern, note_msg));
                }

                sysml2_diag_emit(vctx->diag_ctx, diag);
                vctx->has_errors = true;
            }
        } else {
            /* Add new symbol */
            sysml_symtab_add(vctx->symtab, scope, node->name, node->id, node);
        }

        /* Also create scope for this element if it can contain children */
        if (SYSML_KIND_IS_PACKAGE(node->kind) || SYSML_KIND_IS_DEFINITION(node->kind)) {
            sysml_symtab_get_or_create_scope(vctx->symtab, node->id);
        }
    }

    /* Process imports and add them to their owner scopes */
    for (size_t i = 0; i < model->import_count; i++) {
        SysmlImport *imp = model->imports[i];
        SysmlScope *scope = sysml_symtab_get_or_create_scope(
            vctx->symtab, imp->owner_scope);

        /* Create import entry and add to scope's import list */
        SysmlImportEntry *entry = sysml2_arena_alloc(vctx->symtab->arena, sizeof(SysmlImportEntry));
        if (entry) {
            entry->target = imp->target;
            entry->import_kind = imp->kind;
            entry->next = scope->imports;
            scope->imports = entry;
        }
    }
}

/* Pass 2: Resolve types and check compatibility */
static void pass2_resolve_types(
    ValidationContext *vctx,
    const SysmlSemanticModel *model
) {
    for (size_t i = 0; i < model->element_count; i++) {
        SysmlNode *node = model->elements[i];
        if (node->typed_by_count == 0) continue;

        /* Get scope for this element */
        SysmlScope *scope = sysml_symtab_get_or_create_scope(
            vctx->symtab, node->parent_id);

        for (size_t j = 0; j < node->typed_by_count; j++) {
            const char *type_ref = node->typed_by[j];

            /* Try to resolve the type */
            SysmlSymbol *type_sym = sysml_symtab_resolve(
                vctx->symtab, scope, type_ref);

            if (!type_sym) {
                /* E3001: Undefined type */
                if (vctx->options->check_undefined_types) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                        "undefined type '%s'", type_ref);

                    Sysml2SourceRange range = SYSML2_RANGE_INVALID;
                    range.start = node->loc;
                    range.end = node->loc;

                    Sysml2Diagnostic *diag = sysml2_diag_create(
                        vctx->diag_ctx,
                        SYSML2_DIAG_E3001_UNDEFINED_TYPE,
                        SYSML2_SEVERITY_ERROR,
                        vctx->source_file,
                        range,
                        sysml2_intern(vctx->symtab->intern, msg)
                    );

                    /* Add suggestions if enabled */
                    if (vctx->options->suggest_corrections) {
                        const char *suggestions[8];
                        size_t count = sysml_symtab_find_similar(
                            vctx->symtab, scope, type_ref,
                            suggestions, vctx->options->max_suggestions);

                        if (count > 0) {
                            char help[512];
                            snprintf(help, sizeof(help), "did you mean '%s'?", suggestions[0]);
                            sysml2_diag_add_help(diag, vctx->diag_ctx,
                                sysml2_intern(vctx->symtab->intern, help));
                        }
                    }

                    sysml2_diag_emit(vctx->diag_ctx, diag);
                    vctx->has_errors = true;
                }
            } else if (vctx->options->check_type_compatibility && type_sym->node) {
                /* E3006: Type compatibility check */
                if (!sysml_is_type_compatible(node->kind, type_sym->node->kind)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                        "'%s' cannot be typed by '%s' (%s)",
                        node->name ? node->name : "<anonymous>",
                        type_ref,
                        sysml_kind_to_string(type_sym->node->kind));

                    Sysml2SourceRange range = SYSML2_RANGE_INVALID;
                    range.start = node->loc;
                    range.end = node->loc;

                    Sysml2Diagnostic *diag = sysml2_diag_create(
                        vctx->diag_ctx,
                        SYSML2_DIAG_E3006_TYPE_MISMATCH,
                        SYSML2_SEVERITY_ERROR,
                        vctx->source_file,
                        range,
                        sysml2_intern(vctx->symtab->intern, msg)
                    );

                    sysml2_diag_emit(vctx->diag_ctx, diag);
                    vctx->has_errors = true;
                }
            }
        }
    }
}

/* DFS state for cycle detection */
typedef struct {
    const char **stack;
    size_t stack_count;
    size_t stack_capacity;
    const char **visited;
    size_t visited_count;
    size_t visited_capacity;
} CycleDetector;

static void cycle_detector_init(CycleDetector *cd, Sysml2Arena *arena) {
    cd->stack_capacity = 64;
    cd->stack_count = 0;
    cd->stack = sysml2_arena_alloc(arena, cd->stack_capacity * sizeof(const char *));

    cd->visited_capacity = 256;
    cd->visited_count = 0;
    cd->visited = sysml2_arena_alloc(arena, cd->visited_capacity * sizeof(const char *));
}

static bool cycle_in_stack(CycleDetector *cd, const char *id) {
    for (size_t i = 0; i < cd->stack_count; i++) {
        if (strcmp(cd->stack[i], id) == 0) return true;
    }
    return false;
}

static bool cycle_is_visited(CycleDetector *cd, const char *id) {
    for (size_t i = 0; i < cd->visited_count; i++) {
        if (strcmp(cd->visited[i], id) == 0) return true;
    }
    return false;
}

static void cycle_push(CycleDetector *cd, const char *id) {
    if (cd->stack_count < cd->stack_capacity) {
        cd->stack[cd->stack_count++] = id;
    }
}

static void cycle_pop(CycleDetector *cd) {
    if (cd->stack_count > 0) {
        cd->stack_count--;
    }
}

static void cycle_mark_visited(CycleDetector *cd, const char *id) {
    if (cd->visited_count < cd->visited_capacity) {
        cd->visited[cd->visited_count++] = id;
    }
}

/* Build cycle path string */
static const char *build_cycle_path(
    CycleDetector *cd,
    const char *cycle_start,
    Sysml2Arena *arena
) {
    /* Find start of cycle in stack */
    size_t start_idx = 0;
    for (size_t i = 0; i < cd->stack_count; i++) {
        if (strcmp(cd->stack[i], cycle_start) == 0) {
            start_idx = i;
            break;
        }
    }

    /* Calculate required buffer size */
    size_t buf_size = 0;
    for (size_t i = start_idx; i < cd->stack_count; i++) {
        buf_size += strlen(cd->stack[i]) + 4; /* " -> " */
    }
    buf_size += strlen(cycle_start) + 1;

    char *buf = sysml2_arena_alloc(arena, buf_size);
    buf[0] = '\0';

    for (size_t i = start_idx; i < cd->stack_count; i++) {
        strcat(buf, cd->stack[i]);
        strcat(buf, " -> ");
    }
    strcat(buf, cycle_start);

    return buf;
}

/* Check for cycles starting from a node */
static bool check_cycles_from_node(
    ValidationContext *vctx,
    CycleDetector *cd,
    SysmlNode *node
) {
    if (!node->id) return false;
    if (cycle_is_visited(cd, node->id)) return false;

    if (cycle_in_stack(cd, node->id)) {
        /* Cycle detected! */
        if (vctx->options->check_circular_specs) {
            const char *cycle_path = build_cycle_path(cd, node->id, vctx->symtab->arena);

            char msg[128];
            snprintf(msg, sizeof(msg), "circular specialization detected");

            Sysml2SourceRange range = SYSML2_RANGE_INVALID;
            range.start = node->loc;
            range.end = node->loc;

            Sysml2Diagnostic *diag = sysml2_diag_create(
                vctx->diag_ctx,
                SYSML2_DIAG_E3005_CIRCULAR_SPECIALIZATION,
                SYSML2_SEVERITY_ERROR,
                vctx->source_file,
                range,
                sysml2_intern(vctx->symtab->intern, msg)
            );

            char note_msg[512];
            snprintf(note_msg, sizeof(note_msg), "cycle: %s", cycle_path);
            sysml2_diag_add_note(diag, vctx->diag_ctx,
                vctx->source_file, SYSML2_RANGE_INVALID,
                sysml2_intern(vctx->symtab->intern, note_msg));

            sysml2_diag_emit(vctx->diag_ctx, diag);
            vctx->has_errors = true;
        }
        return true;
    }

    cycle_push(cd, node->id);

    /* Follow typed_by references */
    SysmlScope *scope = sysml_symtab_get_or_create_scope(
        vctx->symtab, node->parent_id);

    for (size_t i = 0; i < node->typed_by_count; i++) {
        SysmlSymbol *type_sym = sysml_symtab_resolve(
            vctx->symtab, scope, node->typed_by[i]);
        if (type_sym && type_sym->node) {
            if (check_cycles_from_node(vctx, cd, type_sym->node)) {
                cycle_pop(cd);
                return true;
            }
        }
    }

    cycle_pop(cd);
    cycle_mark_visited(cd, node->id);
    return false;
}

/* Pass 3: Detect circular specializations */
static void pass3_detect_cycles(
    ValidationContext *vctx,
    const SysmlSemanticModel *model
) {
    CycleDetector cd;
    cycle_detector_init(&cd, vctx->symtab->arena);

    for (size_t i = 0; i < model->element_count; i++) {
        SysmlNode *node = model->elements[i];
        if (node->typed_by_count > 0) {
            check_cycles_from_node(vctx, &cd, node);
        }
    }
}

/* ========== Main Validation Entry Point ========== */

Sysml2Result sysml_validate(
    const SysmlSemanticModel *model,
    Sysml2DiagContext *diag_ctx,
    const Sysml2SourceFile *source_file,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    const SysmlValidationOptions *options
) {
    if (!model || !diag_ctx || !arena || !intern) {
        return SYSML2_ERROR_SEMANTIC;
    }

    /* Use default options if none provided */
    SysmlValidationOptions default_opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    if (!options) {
        options = &default_opts;
    }

    /* Initialize symbol table */
    SysmlSymbolTable symtab;
    sysml_symtab_init(&symtab, arena, intern);

    /* Set up validation context */
    ValidationContext vctx = {
        .symtab = &symtab,
        .diag_ctx = diag_ctx,
        .source_file = source_file,
        .options = options,
        .has_errors = false
    };

    /* Pass 1: Build symbol table + detect duplicates (E3004) */
    if (options->check_duplicate_names) {
        pass1_build_symtab(&vctx, model);
    } else {
        /* Still need to build symbol table for other passes */
        SysmlValidationOptions temp_opts = *options;
        temp_opts.check_duplicate_names = false;
        vctx.options = &temp_opts;
        pass1_build_symtab(&vctx, model);
        vctx.options = options;
    }

    /* Pass 2: Resolve types + check compatibility (E3001, E3006) */
    if (options->check_undefined_types || options->check_type_compatibility) {
        pass2_resolve_types(&vctx, model);
    }

    /* Pass 3: Detect circular specializations (E3005) */
    if (options->check_circular_specs) {
        pass3_detect_cycles(&vctx, model);
    }

    /* Cleanup */
    sysml_symtab_destroy(&symtab);

    return vctx.has_errors ? SYSML2_ERROR_SEMANTIC : SYSML2_OK;
}

/* ========== Multi-Model Validation ========== */

Sysml2Result sysml_validate_multi(
    SysmlSemanticModel **models,
    size_t model_count,
    Sysml2DiagContext *diag_ctx,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    const SysmlValidationOptions *options
) {
    if (!models || model_count == 0 || !diag_ctx || !arena || !intern) {
        return SYSML2_ERROR_SEMANTIC;
    }

    /* Use default options if none provided */
    SysmlValidationOptions default_opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    if (!options) {
        options = &default_opts;
    }

    /* Initialize unified symbol table */
    SysmlSymbolTable symtab;
    sysml_symtab_init(&symtab, arena, intern);

    /* Set up validation context (source_file is NULL for multi-model) */
    ValidationContext vctx = {
        .symtab = &symtab,
        .diag_ctx = diag_ctx,
        .source_file = NULL,
        .options = options,
        .has_errors = false
    };

    /* Pass 1: Build unified symbol table from ALL models */
    for (size_t i = 0; i < model_count; i++) {
        if (models[i]) {
            pass1_build_symtab(&vctx, models[i]);
        }
    }

    /* Pass 2: Resolve types across all models */
    for (size_t i = 0; i < model_count; i++) {
        if (models[i]) {
            pass2_resolve_types(&vctx, models[i]);
        }
    }

    /* Pass 3: Detect cycles across all models */
    if (options->check_circular_specs) {
        for (size_t i = 0; i < model_count; i++) {
            if (models[i]) {
                pass3_detect_cycles(&vctx, models[i]);
            }
        }
    }

    /* Cleanup */
    sysml_symtab_destroy(&symtab);

    return vctx.has_errors ? SYSML2_ERROR_SEMANTIC : SYSML2_OK;
}
