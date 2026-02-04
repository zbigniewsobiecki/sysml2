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
#include <limits.h>
#include <errno.h>
#include <stdint.h>

/* Symbol table implementation is now in symtab.c */

/* ========== Multiplicity Parsing ========== */

/*
 * Parsed multiplicity bounds
 */
typedef struct {
    int64_t lower;      /* -1 for invalid */
    int64_t upper;      /* INT64_MAX for "*" */
    bool valid;
} ParsedMultiplicity;

/*
 * Parse a single multiplicity bound string
 * Returns -1 for invalid, INT64_MAX for "*"
 */
static int64_t parse_mult_bound(const char *s) {
    if (!s || !*s) return -1;

    /* Handle "*" as unbounded */
    if (strcmp(s, "*") == 0) return INT64_MAX;

    /* Parse as integer */
    char *endptr;
    errno = 0;
    long long val = strtoll(s, &endptr, 10);

    /* Check for parse errors */
    if (errno != 0 || endptr == s || *endptr != '\0') {
        return -1;
    }

    /* Check for negative values */
    if (val < 0) {
        return -1;
    }

    return (int64_t)val;
}

/*
 * Parse multiplicity from lower and upper bound strings
 */
static ParsedMultiplicity parse_multiplicity(const char *lower, const char *upper) {
    ParsedMultiplicity result = { .lower = -1, .upper = -1, .valid = false };

    if (!lower) return result;

    result.lower = parse_mult_bound(lower);
    if (result.lower < 0) return result;

    /* If no upper bound, it's the same as lower (e.g., [5] means [5..5]) */
    if (!upper || !*upper) {
        result.upper = result.lower;
    } else {
        result.upper = parse_mult_bound(upper);
        if (result.upper < 0) return result;
    }

    result.valid = true;
    return result;
}

/* ========== Type Compatibility ========== */

bool sysml2_is_type_compatible(SysmlNodeKind usage_kind, SysmlNodeKind def_kind) {
    /* Package types can contain anything */
    if (SYSML_KIND_IS_PACKAGE(def_kind)) return true;

    /* KerML classifiers can type any feature-like usage per KerML spec.
     * Class, Structure, Behavior, etc. can type features. */
    if (SYSML_KIND_IS_KERML_CLASSIFIER(def_kind)) {
        return true;
    }

    /* Metadata definitions can type metadata usages and references */
    if (def_kind == SYSML_KIND_METADATA_DEF) {
        return true;
    }

    /* KerML features can be typed by any definition per KerML spec. */
    if (SYSML_KIND_IS_KERML_FEATURE(usage_kind) && SYSML_KIND_IS_DEFINITION(def_kind)) {
        return true;
    }

    /* Parameters can be typed by any definition - they represent part params,
     * item params, attribute params, etc. depending on their type. */
    if (usage_kind == SYSML_KIND_PARAMETER && SYSML_KIND_IS_DEFINITION(def_kind)) {
        return true;
    }

    /* Reference usages can reference any definition */
    if (usage_kind == SYSML_KIND_REFERENCE_USAGE && SYSML_KIND_IS_DEFINITION(def_kind)) {
        return true;
    }

    /* Usages can also be typed by KerML features (redefining/subsetting) */
    if (SYSML_KIND_IS_USAGE(usage_kind) && SYSML_KIND_IS_KERML_FEATURE(def_kind)) {
        return true;
    }

    /* Map usage kinds to compatible definition kinds */
    switch (usage_kind) {
        case SYSML_KIND_PART_USAGE:
            return def_kind == SYSML_KIND_PART_DEF ||
                   def_kind == SYSML_KIND_ITEM_DEF ||
                   def_kind == SYSML_KIND_OCCURRENCE_DEF;

        case SYSML_KIND_ACTION_USAGE:
        case SYSML_KIND_PERFORM_ACTION_USAGE:
            return def_kind == SYSML_KIND_ACTION_DEF ||
                   def_kind == SYSML_KIND_CALC_DEF;

        case SYSML_KIND_STATE_USAGE:
            return def_kind == SYSML_KIND_STATE_DEF ||
                   def_kind == SYSML_KIND_ACTION_DEF;

        case SYSML_KIND_PORT_USAGE:
            return def_kind == SYSML_KIND_PORT_DEF;

        case SYSML_KIND_ATTRIBUTE_USAGE:
            return def_kind == SYSML_KIND_ATTRIBUTE_DEF ||
                   def_kind == SYSML_KIND_ENUMERATION_DEF ||
                   def_kind == SYSML_KIND_DATATYPE;

        case SYSML_KIND_REQUIREMENT_USAGE:
            return def_kind == SYSML_KIND_REQUIREMENT_DEF ||
                   def_kind == SYSML_KIND_CONCERN_DEF;

        case SYSML_KIND_CONSTRAINT_USAGE:
            return def_kind == SYSML_KIND_CONSTRAINT_DEF;

        case SYSML_KIND_ITEM_USAGE:
            return def_kind == SYSML_KIND_ITEM_DEF ||
                   def_kind == SYSML_KIND_PART_DEF ||
                   def_kind == SYSML_KIND_OCCURRENCE_DEF;

        case SYSML_KIND_OCCURRENCE_USAGE:
            return def_kind == SYSML_KIND_OCCURRENCE_DEF ||
                   def_kind == SYSML_KIND_ITEM_DEF ||
                   def_kind == SYSML_KIND_PART_DEF;

        case SYSML_KIND_CONNECTION_USAGE:
            return def_kind == SYSML_KIND_CONNECTION_DEF ||
                   def_kind == SYSML_KIND_INTERFACE_DEF;

        case SYSML_KIND_FLOW_USAGE:
            return def_kind == SYSML_KIND_FLOW_DEF;

        case SYSML_KIND_INTERFACE_USAGE:
            return def_kind == SYSML_KIND_INTERFACE_DEF;

        case SYSML_KIND_ALLOCATION_USAGE:
            return def_kind == SYSML_KIND_ALLOCATION_DEF;

        case SYSML_KIND_CALC_USAGE:
            return def_kind == SYSML_KIND_CALC_DEF ||
                   def_kind == SYSML_KIND_ACTION_DEF;

        case SYSML_KIND_CASE_USAGE:
            return def_kind == SYSML_KIND_CASE_DEF ||
                   def_kind == SYSML_KIND_CALC_DEF;

        case SYSML_KIND_ANALYSIS_USAGE:
            return def_kind == SYSML_KIND_ANALYSIS_DEF ||
                   def_kind == SYSML_KIND_CASE_DEF;

        case SYSML_KIND_VERIFICATION_USAGE:
            return def_kind == SYSML_KIND_VERIFICATION_DEF ||
                   def_kind == SYSML_KIND_CASE_DEF;

        case SYSML_KIND_USE_CASE_USAGE:
            return def_kind == SYSML_KIND_USE_CASE_DEF ||
                   def_kind == SYSML_KIND_CASE_DEF;

        case SYSML_KIND_VIEW_USAGE:
            return def_kind == SYSML_KIND_VIEW_DEF;

        case SYSML_KIND_VIEWPOINT_USAGE:
            return def_kind == SYSML_KIND_VIEWPOINT_DEF;

        case SYSML_KIND_RENDERING_USAGE:
            return def_kind == SYSML_KIND_RENDERING_DEF;

        case SYSML_KIND_CONCERN_USAGE:
            return def_kind == SYSML_KIND_CONCERN_DEF ||
                   def_kind == SYSML_KIND_REQUIREMENT_DEF;

        case SYSML_KIND_EVENT_USAGE:
            return def_kind == SYSML_KIND_OCCURRENCE_DEF ||
                   def_kind == SYSML_KIND_ITEM_DEF ||
                   def_kind == SYSML_KIND_PART_DEF;

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
    Sysml2SymbolTable *symtab;
    Sysml2DiagContext *diag_ctx;
    const Sysml2SourceFile *source_file;
    const Sysml2ValidationOptions *options;
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
        Sysml2Scope *scope = sysml2_symtab_get_or_create_scope(
            vctx->symtab, node->parent_id);

        /* Try to add symbol */
        Sysml2Symbol *existing = sysml2_symtab_lookup(scope, node->name);
        if (existing) {
            /*
             * Package contribution (SysML v2 §7.5): multiple files may
             * contribute members to the same package.  When the existing
             * symbol and the new node are both packages, we silently
             * merge — the shared scope already exists and children from
             * each file will be added to it.
             */
            bool is_package_merge = SYSML_KIND_IS_PACKAGE(node->kind)
                && existing->node
                && SYSML_KIND_IS_PACKAGE(existing->node->kind);

            if (!is_package_merge) {
                /* Genuine duplicate — not a package merge */
                if (vctx->options->check_duplicate_names) {
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
            }
            /* For package merges, fall through to scope creation below. */
        } else {
            /* Add new symbol */
            sysml2_symtab_add(vctx->symtab, scope, node->name, node->id, node);
        }

        /* Also create scope for this element if it can contain children */
        if (SYSML_KIND_IS_PACKAGE(node->kind) || SYSML_KIND_IS_DEFINITION(node->kind)) {
            sysml2_symtab_get_or_create_scope(vctx->symtab, node->id);
        }
    }

    /* Add implicit imports from standard library packages to root scope */
    for (size_t i = 0; i < model->element_count; i++) {
        SysmlNode *node = model->elements[i];
        if (node->kind == SYSML_KIND_LIBRARY_PACKAGE && node->id) {
            /* Add implicit import entry to root scope */
            Sysml2Scope *root = vctx->symtab->root_scope;
            Sysml2ImportEntry *entry = sysml2_arena_alloc(vctx->symtab->arena, sizeof(Sysml2ImportEntry));
            if (entry) {
                entry->target = node->id;
                entry->import_kind = SYSML_KIND_IMPORT_ALL;
                entry->next = root->imports;
                root->imports = entry;
            }
        }
    }

    /* Process imports and add them to their owner scopes */
    for (size_t i = 0; i < model->import_count; i++) {
        SysmlImport *imp = model->imports[i];
        Sysml2Scope *scope = sysml2_symtab_get_or_create_scope(
            vctx->symtab, imp->owner_scope);

        /* Create import entry and add to scope's import list */
        Sysml2ImportEntry *entry = sysml2_arena_alloc(vctx->symtab->arena, sizeof(Sysml2ImportEntry));
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
        Sysml2Scope *scope = sysml2_symtab_get_or_create_scope(
            vctx->symtab, node->parent_id);

        for (size_t j = 0; j < node->typed_by_count; j++) {
            const char *type_ref = node->typed_by[j];

            /* Try to resolve the type */
            Sysml2Symbol *type_sym = sysml2_symtab_resolve(
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
                        size_t count = sysml2_symtab_find_similar(
                            vctx->symtab, scope, type_ref,
                            suggestions, vctx->options->max_suggestions);

                        if (count > 0) {
                            char help[512];
                            snprintf(help, sizeof(help), "did you mean '%s'?", suggestions[0]);
                            sysml2_diag_add_help(diag, vctx->diag_ctx,
                                sysml2_intern(vctx->symtab->intern, help));
                        } else {
                            sysml2_diag_add_help(diag, vctx->diag_ctx,
                                sysml2_intern(vctx->symtab->intern,
                                    "define this type before use, or add an import for the package that defines it"));
                        }
                    }

                    sysml2_diag_emit(vctx->diag_ctx, diag);
                    vctx->has_errors = true;
                }
            } else if (vctx->options->check_type_compatibility && type_sym->node) {
                /* E3006: Type compatibility check */
                if (!sysml2_is_type_compatible(node->kind, type_sym->node->kind)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                        "'%s' cannot be typed by '%s' (%s)",
                        node->name ? node->name : "<anonymous>",
                        type_ref,
                        sysml2_kind_to_string(type_sym->node->kind));

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

    /* Follow all type relationship references */
    Sysml2Scope *scope = sysml2_symtab_get_or_create_scope(
        vctx->symtab, node->parent_id);

    /* Check typed_by references */
    for (size_t i = 0; i < node->typed_by_count; i++) {
        Sysml2Symbol *type_sym = sysml2_symtab_resolve(
            vctx->symtab, scope, node->typed_by[i]);
        if (type_sym && type_sym->node) {
            if (check_cycles_from_node(vctx, cd, type_sym->node)) {
                cycle_pop(cd);
                return true;
            }
        }
    }

    /* Check specializes references */
    for (size_t i = 0; i < node->specializes_count; i++) {
        Sysml2Symbol *type_sym = sysml2_symtab_resolve(
            vctx->symtab, scope, node->specializes[i]);
        if (type_sym && type_sym->node) {
            if (check_cycles_from_node(vctx, cd, type_sym->node)) {
                cycle_pop(cd);
                return true;
            }
        }
    }

    /* NOTE: We intentionally SKIP redefines references (:>>) for cycle detection.
     * Redefinitions are OVERRIDES that narrow inherited features, not type cycles.
     * Example: attribute :>> unitConversion redefines parent's unitConversion -
     * this is valid inheritance, not a circular dependency.
     * See SysML v2 spec: redefinitions allow child types to specialize/narrow
     * features inherited from parent types without creating cycles. */

    /* Check references references */
    for (size_t i = 0; i < node->references_count; i++) {
        Sysml2Symbol *type_sym = sysml2_symtab_resolve(
            vctx->symtab, scope, node->references[i]);
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
        /* Check if node has any type relationships */
        if (node->typed_by_count > 0 ||
            node->specializes_count > 0 ||
            node->redefines_count > 0 ||
            node->references_count > 0) {
            check_cycles_from_node(vctx, &cd, node);
        }
    }
}

/* ========== Pass 4: Validate Multiplicities (E3007) ========== */

static void pass4_validate_multiplicities(
    ValidationContext *vctx,
    const SysmlSemanticModel *model
) {
    for (size_t i = 0; i < model->element_count; i++) {
        SysmlNode *node = model->elements[i];
        if (!node->multiplicity_lower) continue;

        ParsedMultiplicity m = parse_multiplicity(
            node->multiplicity_lower,
            node->multiplicity_upper
        );

        Sysml2SourceRange range = SYSML2_RANGE_INVALID;
        range.start = node->loc;
        range.end = node->loc;

        if (!m.valid) {
            /* E3007: invalid multiplicity format */
            char msg[256];
            if (node->multiplicity_upper) {
                snprintf(msg, sizeof(msg),
                    "invalid multiplicity bounds [%s..%s]",
                    node->multiplicity_lower, node->multiplicity_upper);
            } else {
                snprintf(msg, sizeof(msg),
                    "invalid multiplicity bound [%s]",
                    node->multiplicity_lower);
            }

            Sysml2Diagnostic *diag = sysml2_diag_create(
                vctx->diag_ctx,
                SYSML2_DIAG_E3007_MULTIPLICITY_ERROR,
                SYSML2_SEVERITY_ERROR,
                vctx->source_file,
                range,
                sysml2_intern(vctx->symtab->intern, msg)
            );
            sysml2_diag_emit(vctx->diag_ctx, diag);
            vctx->has_errors = true;
        } else if (m.upper != INT64_MAX && m.lower > m.upper) {
            /* E3007: lower bound exceeds upper bound */
            char msg[256];
            snprintf(msg, sizeof(msg),
                "multiplicity lower bound (%s) exceeds upper bound (%s)",
                node->multiplicity_lower, node->multiplicity_upper);

            Sysml2Diagnostic *diag = sysml2_diag_create(
                vctx->diag_ctx,
                SYSML2_DIAG_E3007_MULTIPLICITY_ERROR,
                SYSML2_SEVERITY_ERROR,
                vctx->source_file,
                range,
                sysml2_intern(vctx->symtab->intern, msg)
            );

            char help_msg[128];
            snprintf(help_msg, sizeof(help_msg),
                "swap the bounds: [%s..%s]",
                node->multiplicity_upper, node->multiplicity_lower);
            sysml2_diag_add_help(diag, vctx->diag_ctx,
                sysml2_intern(vctx->symtab->intern, help_msg));

            sysml2_diag_emit(vctx->diag_ctx, diag);
            vctx->has_errors = true;
        }
    }
}

/* ========== Pass 5: Validate Redefines (E3002, E3008) ========== */

/*
 * Find a feature in a type's direct members
 */
static SysmlNode *find_feature_in_scope(
    ValidationContext *vctx,
    const char *scope_id,
    const char *feature_name
) {
    if (!scope_id || !feature_name) return NULL;

    Sysml2Scope *scope = sysml2_symtab_get_or_create_scope(vctx->symtab, scope_id);
    Sysml2Symbol *sym = sysml2_symtab_lookup(scope, feature_name);
    if (sym && sym->node) {
        return sym->node;
    }
    return NULL;
}

/*
 * Find a feature in a type's inheritance chain (typed_by + specializes)
 * Returns the feature node if found, NULL otherwise.
 *
 * @param skip_self If true, don't check direct features of type_node (for redefines)
 */
static SysmlNode *find_inherited_feature(
    ValidationContext *vctx,
    SysmlNode *type_node,
    const char *feature_name,
    int depth,
    bool skip_self
) {
    if (!type_node || !feature_name || depth > 20) return NULL;

    /* Check direct features of this type (unless skip_self) */
    if (!skip_self) {
        SysmlNode *found = find_feature_in_scope(vctx, type_node->id, feature_name);
        if (found) return found;
    }

    /* Check base types (typed_by) */
    Sysml2Scope *type_scope = sysml2_symtab_get_or_create_scope(
        vctx->symtab, type_node->parent_id);

    for (size_t i = 0; i < type_node->typed_by_count; i++) {
        Sysml2Symbol *base_sym = sysml2_symtab_resolve(
            vctx->symtab, type_scope, type_node->typed_by[i]);
        if (base_sym && base_sym->node) {
            SysmlNode *found = find_inherited_feature(vctx, base_sym->node, feature_name, depth + 1, false);
            if (found) return found;
        }
    }

    /* Check specialized types */
    for (size_t i = 0; i < type_node->specializes_count; i++) {
        Sysml2Symbol *base_sym = sysml2_symtab_resolve(
            vctx->symtab, type_scope, type_node->specializes[i]);
        if (base_sym && base_sym->node) {
            SysmlNode *found = find_inherited_feature(vctx, base_sym->node, feature_name, depth + 1, false);
            if (found) return found;
        }
    }

    return NULL;
}

/*
 * Get the parent type node for an element (for redefines context)
 */
static SysmlNode *get_parent_type_node(
    ValidationContext *vctx,
    SysmlNode *node
) {
    if (!node || !node->parent_id) return NULL;

    /* Look up the parent in the symbol table */
    Sysml2Scope *parent_scope = sysml2_symtab_get_or_create_scope(
        vctx->symtab, node->parent_id);

    /* The parent scope ID is the parent's qualified ID */
    /* Find the symbol for that ID in the parent's parent scope */
    const char *last_sep = strrchr(node->parent_id, ':');
    const char *parent_name = last_sep ? (last_sep + 1) : node->parent_id;

    /* Skip second colon if present (::) */
    while (*parent_name == ':') parent_name++;

    Sysml2Symbol *parent_sym = sysml2_symtab_lookup(parent_scope->parent, parent_name);
    if (parent_sym && parent_sym->node) {
        return parent_sym->node;
    }

    /* Alternative: look up by qualified ID directly */
    Sysml2Symbol *direct_sym = sysml2_symtab_resolve(
        vctx->symtab, vctx->symtab->root_scope, node->parent_id);
    if (direct_sym && direct_sym->node) {
        return direct_sym->node;
    }

    return NULL;
}

/*
 * Check if new_type specializes or is the same as orig_type
 */
static bool is_subtype_of(
    ValidationContext *vctx,
    const char *new_type,
    const char *orig_type,
    Sysml2Scope *scope,
    int depth
) {
    if (!new_type || !orig_type || depth > 20) return false;

    /* Same type is valid */
    if (strcmp(new_type, orig_type) == 0) return true;

    /* Resolve new_type and check its base types */
    Sysml2Symbol *new_sym = sysml2_symtab_resolve(vctx->symtab, scope, new_type);
    if (!new_sym || !new_sym->node) return false;

    SysmlNode *new_node = new_sym->node;
    Sysml2Scope *new_scope = sysml2_symtab_get_or_create_scope(
        vctx->symtab, new_node->parent_id);

    /* Check typed_by */
    for (size_t i = 0; i < new_node->typed_by_count; i++) {
        if (is_subtype_of(vctx, new_node->typed_by[i], orig_type, new_scope, depth + 1)) {
            return true;
        }
    }

    /* Check specializes */
    for (size_t i = 0; i < new_node->specializes_count; i++) {
        if (is_subtype_of(vctx, new_node->specializes[i], orig_type, new_scope, depth + 1)) {
            return true;
        }
    }

    return false;
}

/*
 * Check if new multiplicity is a valid narrowing of original
 * (new_lower >= orig_lower AND new_upper <= orig_upper)
 */
static bool is_valid_multiplicity_narrowing(
    ParsedMultiplicity orig,
    ParsedMultiplicity new_mult
) {
    if (!orig.valid || !new_mult.valid) return true; /* Skip if invalid */

    /* new_lower must be >= orig_lower */
    if (new_mult.lower < orig.lower) return false;

    /* new_upper must be <= orig_upper (unless orig is unbounded) */
    if (orig.upper != INT64_MAX && new_mult.upper > orig.upper) return false;

    return true;
}

static void pass5_validate_redefines(
    ValidationContext *vctx,
    const SysmlSemanticModel *model
) {
    for (size_t i = 0; i < model->element_count; i++) {
        SysmlNode *node = model->elements[i];
        if (node->redefines_count == 0) continue;

        /* Get the parent type for context */
        SysmlNode *parent_type = get_parent_type_node(vctx, node);

        Sysml2Scope *scope = sysml2_symtab_get_or_create_scope(
            vctx->symtab, node->parent_id);

        for (size_t j = 0; j < node->redefines_count; j++) {
            const char *ref = node->redefines[j];
            SysmlNode *orig_feature = NULL;

            /* If simple name, must exist in parent type hierarchy */
            if (!strchr(ref, ':')) {
                if (parent_type) {
                    /* Skip self (B) and look only in inherited types (A, etc.) */
                    orig_feature = find_inherited_feature(vctx, parent_type, ref, 0, true);
                }

                if (!orig_feature && vctx->options->check_undefined_features) {
                    /* E3002: feature not found in parent type */
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                        "feature '%s' not found in parent type%s%s",
                        ref,
                        parent_type && parent_type->name ? " '" : "",
                        parent_type && parent_type->name ? parent_type->name : "");
                    if (parent_type && parent_type->name) {
                        strcat(msg, "'");
                    }

                    Sysml2SourceRange range = SYSML2_RANGE_INVALID;
                    range.start = node->loc;
                    range.end = node->loc;

                    Sysml2Diagnostic *diag = sysml2_diag_create(
                        vctx->diag_ctx,
                        SYSML2_DIAG_E3002_UNDEFINED_FEATURE,
                        SYSML2_SEVERITY_ERROR,
                        vctx->source_file,
                        range,
                        sysml2_intern(vctx->symtab->intern, msg)
                    );
                    sysml2_diag_emit(vctx->diag_ctx, diag);
                    vctx->has_errors = true;
                    continue;
                }
            } else {
                /* Qualified name - resolve it */
                Sysml2Symbol *ref_sym = sysml2_symtab_resolve(vctx->symtab, scope, ref);
                if (ref_sym && ref_sym->node) {
                    orig_feature = ref_sym->node;
                } else if (vctx->options->check_undefined_features) {
                    /* E3002: qualified feature not found */
                    char msg[256];
                    snprintf(msg, sizeof(msg), "undefined feature '%s'", ref);

                    Sysml2SourceRange range = SYSML2_RANGE_INVALID;
                    range.start = node->loc;
                    range.end = node->loc;

                    Sysml2Diagnostic *diag = sysml2_diag_create(
                        vctx->diag_ctx,
                        SYSML2_DIAG_E3002_UNDEFINED_FEATURE,
                        SYSML2_SEVERITY_ERROR,
                        vctx->source_file,
                        range,
                        sysml2_intern(vctx->symtab->intern, msg)
                    );
                    sysml2_diag_emit(vctx->diag_ctx, diag);
                    vctx->has_errors = true;
                    continue;
                }
            }

            /* E3008: Check redefinition compatibility */
            if (orig_feature && vctx->options->check_redefinition_compat) {
                /* Check type narrowing if redefining node has a type */
                if (node->typed_by_count > 0 && orig_feature->typed_by_count > 0) {
                    const char *new_type = node->typed_by[0];
                    const char *orig_type = orig_feature->typed_by[0];

                    if (!is_subtype_of(vctx, new_type, orig_type, scope, 0)) {
                        char msg[256];
                        snprintf(msg, sizeof(msg),
                            "redefinition type '%s' is not a subtype of '%s'",
                            new_type, orig_type);

                        Sysml2SourceRange range = SYSML2_RANGE_INVALID;
                        range.start = node->loc;
                        range.end = node->loc;

                        Sysml2Diagnostic *diag = sysml2_diag_create(
                            vctx->diag_ctx,
                            SYSML2_DIAG_E3008_REDEFINITION_ERROR,
                            SYSML2_SEVERITY_ERROR,
                            vctx->source_file,
                            range,
                            sysml2_intern(vctx->symtab->intern, msg)
                        );

                        char help_msg[256];
                        snprintf(help_msg, sizeof(help_msg),
                            "redefinition must use same type or a subtype of '%s'",
                            orig_type);
                        sysml2_diag_add_help(diag, vctx->diag_ctx,
                            sysml2_intern(vctx->symtab->intern, help_msg));

                        sysml2_diag_emit(vctx->diag_ctx, diag);
                        vctx->has_errors = true;
                    }
                }

                /* Check multiplicity narrowing */
                if (node->multiplicity_lower && orig_feature->multiplicity_lower) {
                    ParsedMultiplicity new_mult = parse_multiplicity(
                        node->multiplicity_lower, node->multiplicity_upper);
                    ParsedMultiplicity orig_mult = parse_multiplicity(
                        orig_feature->multiplicity_lower, orig_feature->multiplicity_upper);

                    if (!is_valid_multiplicity_narrowing(orig_mult, new_mult)) {
                        char msg[256];
                        if (orig_feature->multiplicity_upper) {
                            snprintf(msg, sizeof(msg),
                                "redefinition multiplicity [%s..%s] widens original [%s..%s]",
                                node->multiplicity_lower,
                                node->multiplicity_upper ? node->multiplicity_upper : node->multiplicity_lower,
                                orig_feature->multiplicity_lower,
                                orig_feature->multiplicity_upper);
                        } else {
                            snprintf(msg, sizeof(msg),
                                "redefinition multiplicity [%s..%s] widens original [%s]",
                                node->multiplicity_lower,
                                node->multiplicity_upper ? node->multiplicity_upper : node->multiplicity_lower,
                                orig_feature->multiplicity_lower);
                        }

                        Sysml2SourceRange range = SYSML2_RANGE_INVALID;
                        range.start = node->loc;
                        range.end = node->loc;

                        Sysml2Diagnostic *diag = sysml2_diag_create(
                            vctx->diag_ctx,
                            SYSML2_DIAG_E3008_REDEFINITION_ERROR,
                            SYSML2_SEVERITY_ERROR,
                            vctx->source_file,
                            range,
                            sysml2_intern(vctx->symtab->intern, msg)
                        );

                        sysml2_diag_add_help(diag, vctx->diag_ctx,
                            sysml2_intern(vctx->symtab->intern,
                                "redefinition can only narrow (not widen) the multiplicity"));

                        sysml2_diag_emit(vctx->diag_ctx, diag);
                        vctx->has_errors = true;
                    }
                }
            }
        }
    }
}

/* ========== Pass 6: Validate Imports (E3003) ========== */

/*
 * Extract namespace from import target (without ::* or ::** suffix)
 */
static const char *extract_import_namespace(const char *target, Sysml2Arena *arena) {
    if (!target) return NULL;

    size_t len = strlen(target);

    /* Remove ::** suffix */
    if (len >= 4 && strcmp(target + len - 4, "::**") == 0) {
        char *ns = sysml2_arena_alloc(arena, len - 3);
        strncpy(ns, target, len - 4);
        ns[len - 4] = '\0';
        return ns;
    }

    /* Remove ::* suffix */
    if (len >= 3 && strcmp(target + len - 3, "::*") == 0) {
        char *ns = sysml2_arena_alloc(arena, len - 2);
        strncpy(ns, target, len - 3);
        ns[len - 3] = '\0';
        return ns;
    }

    return target;
}

static void pass6_validate_imports(
    ValidationContext *vctx,
    const SysmlSemanticModel *model
) {
    for (size_t i = 0; i < model->import_count; i++) {
        SysmlImport *imp = model->imports[i];
        if (!imp->target) continue;

        const char *ns = extract_import_namespace(imp->target, vctx->symtab->arena);
        if (!ns) continue;

        /* Try to resolve the namespace */
        Sysml2Symbol *sym = sysml2_symtab_resolve(
            vctx->symtab, vctx->symtab->root_scope, ns);

        if (!sym) {
            /* E3003: namespace not found */
            char msg[256];
            snprintf(msg, sizeof(msg), "undefined namespace '%s'", ns);

            Sysml2SourceRange range = SYSML2_RANGE_INVALID;
            range.start = imp->loc;
            range.end = imp->loc;

            Sysml2Diagnostic *diag = sysml2_diag_create(
                vctx->diag_ctx,
                SYSML2_DIAG_E3003_UNDEFINED_NAMESPACE,
                SYSML2_SEVERITY_ERROR,
                vctx->source_file,
                range,
                sysml2_intern(vctx->symtab->intern, msg)
            );

            /* Try to find similar namespaces */
            if (vctx->options->suggest_corrections) {
                const char *suggestions[8];
                size_t count = sysml2_symtab_find_similar(
                    vctx->symtab, vctx->symtab->root_scope, ns,
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
    }
}

/* ========== Pass 7: Abstract Instantiation Warnings ========== */

static void pass7_check_abstract_instantiation(
    ValidationContext *vctx,
    const SysmlSemanticModel *model
) {
    for (size_t i = 0; i < model->element_count; i++) {
        SysmlNode *node = model->elements[i];

        /* Only check usages (not definitions) */
        if (!SYSML_KIND_IS_USAGE(node->kind)) continue;

        /* Skip abstract usages - they're not concrete instantiations */
        if (node->is_abstract) continue;

        /* Get scope for type resolution */
        Sysml2Scope *scope = sysml2_symtab_get_or_create_scope(
            vctx->symtab, node->parent_id);

        for (size_t j = 0; j < node->typed_by_count; j++) {
            const char *type_ref = node->typed_by[j];

            Sysml2Symbol *type_sym = sysml2_symtab_resolve(
                vctx->symtab, scope, type_ref);

            if (type_sym && type_sym->node && type_sym->node->is_abstract) {
                /* Warning: instantiating abstract type */
                char msg[256];
                snprintf(msg, sizeof(msg),
                    "instantiation of abstract type '%s'",
                    type_ref);

                Sysml2SourceRange range = SYSML2_RANGE_INVALID;
                range.start = node->loc;
                range.end = node->loc;

                Sysml2Diagnostic *diag = sysml2_diag_create(
                    vctx->diag_ctx,
                    SYSML2_DIAG_W1003_DEPRECATED, /* Reuse deprecated code for warning */
                    SYSML2_SEVERITY_WARNING,
                    vctx->source_file,
                    range,
                    sysml2_intern(vctx->symtab->intern, msg)
                );

                sysml2_diag_add_help(diag, vctx->diag_ctx,
                    sysml2_intern(vctx->symtab->intern,
                        "abstract types should not be directly instantiated; use a concrete subtype"));

                sysml2_diag_emit(vctx->diag_ctx, diag);
                /* Note: this is a warning, not an error */
            }
        }
    }
}

/* ========== Main Validation Entry Point ========== */

Sysml2Result sysml2_validate(
    const SysmlSemanticModel *model,
    Sysml2DiagContext *diag_ctx,
    const Sysml2SourceFile *source_file,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    const Sysml2ValidationOptions *options
) {
    if (!model || !diag_ctx || !arena || !intern) {
        return SYSML2_ERROR_SEMANTIC;
    }

    /* Use default options if none provided */
    Sysml2ValidationOptions default_opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    if (!options) {
        options = &default_opts;
    }

    /* Initialize symbol table */
    Sysml2SymbolTable symtab;
    sysml2_symtab_init(&symtab, arena, intern);

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
        Sysml2ValidationOptions temp_opts = *options;
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

    /* Pass 4: Validate multiplicities (E3007) */
    if (options->check_multiplicity) {
        pass4_validate_multiplicities(&vctx, model);
    }

    /* Pass 5: Validate redefines (E3002, E3008) */
    if (options->check_undefined_features || options->check_redefinition_compat) {
        pass5_validate_redefines(&vctx, model);
    }

    /* Pass 6: Validate imports (E3003) */
    if (options->check_undefined_namespaces) {
        pass6_validate_imports(&vctx, model);
    }

    /* Pass 7: Abstract instantiation warnings */
    if (options->warn_abstract_instantiation) {
        pass7_check_abstract_instantiation(&vctx, model);
    }

    /* Cleanup */
    sysml2_symtab_destroy(&symtab);

    return vctx.has_errors ? SYSML2_ERROR_SEMANTIC : SYSML2_OK;
}

/* ========== Multi-Model Validation ========== */

Sysml2Result sysml2_validate_multi(
    SysmlSemanticModel **models,
    size_t model_count,
    Sysml2DiagContext *diag_ctx,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    const Sysml2ValidationOptions *options
) {
    if (!models || model_count == 0 || !diag_ctx || !arena || !intern) {
        return SYSML2_ERROR_SEMANTIC;
    }

    /* Use default options if none provided */
    Sysml2ValidationOptions default_opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    if (!options) {
        options = &default_opts;
    }

    /* Initialize unified symbol table */
    Sysml2SymbolTable symtab;
    sysml2_symtab_init(&symtab, arena, intern);

    /* Set up validation context (source_file set per-model in each pass) */
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
            vctx.source_file = models[i]->source_file;
            pass1_build_symtab(&vctx, models[i]);
        }
    }

    /* Pass 2: Resolve types across all models */
    for (size_t i = 0; i < model_count; i++) {
        if (models[i]) {
            vctx.source_file = models[i]->source_file;
            pass2_resolve_types(&vctx, models[i]);
        }
    }

    /* Pass 3: Detect cycles across all models */
    if (options->check_circular_specs) {
        for (size_t i = 0; i < model_count; i++) {
            if (models[i]) {
                vctx.source_file = models[i]->source_file;
                pass3_detect_cycles(&vctx, models[i]);
            }
        }
    }

    /* Pass 4: Validate multiplicities (E3007) */
    if (options->check_multiplicity) {
        for (size_t i = 0; i < model_count; i++) {
            if (models[i]) {
                vctx.source_file = models[i]->source_file;
                pass4_validate_multiplicities(&vctx, models[i]);
            }
        }
    }

    /* Pass 5: Validate redefines (E3002, E3008) */
    if (options->check_undefined_features || options->check_redefinition_compat) {
        for (size_t i = 0; i < model_count; i++) {
            if (models[i]) {
                vctx.source_file = models[i]->source_file;
                pass5_validate_redefines(&vctx, models[i]);
            }
        }
    }

    /* Pass 6: Validate imports (E3003) */
    if (options->check_undefined_namespaces) {
        for (size_t i = 0; i < model_count; i++) {
            if (models[i]) {
                vctx.source_file = models[i]->source_file;
                pass6_validate_imports(&vctx, models[i]);
            }
        }
    }

    /* Pass 7: Abstract instantiation warnings */
    if (options->warn_abstract_instantiation) {
        for (size_t i = 0; i < model_count; i++) {
            if (models[i]) {
                vctx.source_file = models[i]->source_file;
                pass7_check_abstract_instantiation(&vctx, models[i]);
            }
        }
    }

    /* Cleanup */
    sysml2_symtab_destroy(&symtab);

    return vctx.has_errors ? SYSML2_ERROR_SEMANTIC : SYSML2_OK;
}
