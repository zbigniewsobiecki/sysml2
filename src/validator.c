/*
 * SysML v2 Parser - Semantic Validator Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/validator.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========== Hash Function ========== */

static uint32_t hash_string(const char *str) {
    if (!str) return 0;
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

/* ========== Symbol Table Implementation ========== */

void sysml_symtab_init(
    SysmlSymbolTable *symtab,
    Sysml2Arena *arena,
    Sysml2Intern *intern
) {
    symtab->arena = arena;
    symtab->intern = intern;

    symtab->scope_capacity = SYSML_SYMTAB_DEFAULT_SCOPE_CAPACITY;
    symtab->scope_count = 0;
    symtab->scopes = sysml2_arena_alloc(arena,
        symtab->scope_capacity * sizeof(SysmlScope *));
    memset(symtab->scopes, 0, symtab->scope_capacity * sizeof(SysmlScope *));

    /* Create root scope */
    symtab->root_scope = sysml2_arena_alloc(arena, sizeof(SysmlScope));
    symtab->root_scope->id = NULL;
    symtab->root_scope->parent = NULL;
    symtab->root_scope->symbol_capacity = SYSML_SYMTAB_DEFAULT_SYMBOL_CAPACITY;
    symtab->root_scope->symbol_count = 0;
    symtab->root_scope->symbols = sysml2_arena_alloc(arena,
        symtab->root_scope->symbol_capacity * sizeof(SysmlSymbol *));
    memset(symtab->root_scope->symbols, 0,
        symtab->root_scope->symbol_capacity * sizeof(SysmlSymbol *));
    symtab->root_scope->imports = NULL;
}

void sysml_symtab_destroy(SysmlSymbolTable *symtab) {
    /* Memory is managed by arena, just reset state */
    symtab->scopes = NULL;
    symtab->scope_count = 0;
    symtab->scope_capacity = 0;
    symtab->root_scope = NULL;
}

/* Find scope by ID in the hash table */
static SysmlScope *find_scope(SysmlSymbolTable *symtab, const char *scope_id) {
    if (!scope_id) return symtab->root_scope;

    uint32_t hash = hash_string(scope_id);
    size_t idx = hash % symtab->scope_capacity;

    /* Linear probing */
    for (size_t i = 0; i < symtab->scope_capacity; i++) {
        size_t probe = (idx + i) % symtab->scope_capacity;
        SysmlScope *scope = symtab->scopes[probe];
        if (!scope) return NULL;
        if (scope->id && strcmp(scope->id, scope_id) == 0) {
            return scope;
        }
    }
    return NULL;
}

/* Get parent scope ID by removing last "::" segment */
static const char *get_parent_scope_id(
    SysmlSymbolTable *symtab,
    const char *scope_id
) {
    if (!scope_id) return NULL;

    const char *last_sep = NULL;
    const char *p = scope_id;
    while (*p) {
        if (p[0] == ':' && p[1] == ':') {
            last_sep = p;
        }
        p++;
    }

    if (!last_sep) return NULL; /* No parent, root scope */

    /* Intern the parent ID */
    size_t parent_len = last_sep - scope_id;
    char *buf = sysml2_arena_alloc(symtab->arena, parent_len + 1);
    memcpy(buf, scope_id, parent_len);
    buf[parent_len] = '\0';
    return sysml2_intern(symtab->intern, buf);
}

SysmlScope *sysml_symtab_get_or_create_scope(
    SysmlSymbolTable *symtab,
    const char *scope_id
) {
    if (!scope_id) return symtab->root_scope;

    /* Check if scope already exists */
    SysmlScope *existing = find_scope(symtab, scope_id);
    if (existing) return existing;

    /* Resize if needed (before adding) */
    if (symtab->scope_count >= symtab->scope_capacity * 3 / 4) {
        size_t new_capacity = symtab->scope_capacity * 2;
        SysmlScope **new_scopes = sysml2_arena_alloc(symtab->arena,
            new_capacity * sizeof(SysmlScope *));
        memset(new_scopes, 0, new_capacity * sizeof(SysmlScope *));

        /* Rehash existing scopes */
        for (size_t i = 0; i < symtab->scope_capacity; i++) {
            SysmlScope *scope = symtab->scopes[i];
            if (scope && scope->id) {
                uint32_t hash = hash_string(scope->id);
                size_t idx = hash % new_capacity;
                while (new_scopes[idx]) {
                    idx = (idx + 1) % new_capacity;
                }
                new_scopes[idx] = scope;
            }
        }
        symtab->scopes = new_scopes;
        symtab->scope_capacity = new_capacity;
    }

    /* Create new scope */
    SysmlScope *scope = sysml2_arena_alloc(symtab->arena, sizeof(SysmlScope));
    scope->id = sysml2_intern(symtab->intern, scope_id);
    scope->symbol_capacity = SYSML_SYMTAB_DEFAULT_SYMBOL_CAPACITY;
    scope->symbol_count = 0;
    scope->symbols = sysml2_arena_alloc(symtab->arena,
        scope->symbol_capacity * sizeof(SysmlSymbol *));
    memset(scope->symbols, 0, scope->symbol_capacity * sizeof(SysmlSymbol *));
    scope->imports = NULL;

    /* Link to parent scope */
    const char *parent_id = get_parent_scope_id(symtab, scope_id);
    scope->parent = sysml_symtab_get_or_create_scope(symtab, parent_id);

    /* Insert into hash table */
    uint32_t hash = hash_string(scope->id);
    size_t idx = hash % symtab->scope_capacity;
    while (symtab->scopes[idx]) {
        idx = (idx + 1) % symtab->scope_capacity;
    }
    symtab->scopes[idx] = scope;
    symtab->scope_count++;

    return scope;
}

SysmlSymbol *sysml_symtab_add(
    SysmlSymbolTable *symtab,
    SysmlScope *scope,
    const char *name,
    const char *qualified_id,
    SysmlNode *node
) {
    if (!name || !scope) return NULL;

    /* Check for existing symbol with same name */
    SysmlSymbol *existing = sysml_symtab_lookup(scope, name);
    if (existing) return existing; /* Return existing (duplicate) */

    /* Resize symbols hash table if needed */
    if (scope->symbol_count >= scope->symbol_capacity * 3 / 4) {
        size_t new_capacity = scope->symbol_capacity * 2;
        SysmlSymbol **new_symbols = sysml2_arena_alloc(symtab->arena,
            new_capacity * sizeof(SysmlSymbol *));
        memset(new_symbols, 0, new_capacity * sizeof(SysmlSymbol *));

        /* Rehash */
        for (size_t i = 0; i < scope->symbol_capacity; i++) {
            SysmlSymbol *sym = scope->symbols[i];
            while (sym) {
                SysmlSymbol *next = sym->next;
                uint32_t hash = hash_string(sym->name);
                size_t idx = hash % new_capacity;
                sym->next = new_symbols[idx];
                new_symbols[idx] = sym;
                sym = next;
            }
        }
        scope->symbols = new_symbols;
        scope->symbol_capacity = new_capacity;
    }

    /* Create new symbol */
    SysmlSymbol *sym = sysml2_arena_alloc(symtab->arena, sizeof(SysmlSymbol));
    sym->name = sysml2_intern(symtab->intern, name);
    sym->qualified_id = sysml2_intern(symtab->intern, qualified_id);
    sym->node = node;

    /* Insert into hash chain */
    uint32_t hash = hash_string(name);
    size_t idx = hash % scope->symbol_capacity;
    sym->next = scope->symbols[idx];
    scope->symbols[idx] = sym;
    scope->symbol_count++;

    return sym;
}

SysmlSymbol *sysml_symtab_lookup(
    const SysmlScope *scope,
    const char *name
) {
    if (!scope || !name) return NULL;

    uint32_t hash = hash_string(name);
    size_t idx = hash % scope->symbol_capacity;

    SysmlSymbol *sym = scope->symbols[idx];
    while (sym) {
        if (strcmp(sym->name, name) == 0) {
            return sym;
        }
        sym = sym->next;
    }
    return NULL;
}

/* Forward declaration for resolve_via_imports */
static SysmlSymbol *resolve_via_imports(
    SysmlSymbolTable *symtab,
    const SysmlScope *scope,
    const char *name
);

SysmlSymbol *sysml_symtab_resolve(
    SysmlSymbolTable *symtab,
    const SysmlScope *scope,
    const char *name
) {
    if (!name) return NULL;

    /* Check for qualified name (contains "::") */
    const char *sep = strstr(name, "::");
    if (!sep) {
        /* Simple name - walk up scope chain */
        const SysmlScope *s = scope ? scope : symtab->root_scope;
        while (s) {
            /* 1. Check direct symbols */
            SysmlSymbol *sym = sysml_symtab_lookup(s, name);
            if (sym) return sym;

            /* 2. Check imports in this scope */
            sym = resolve_via_imports(symtab, s, name);
            if (sym) return sym;

            /* 3. Walk up */
            s = s->parent;
        }
        return NULL;
    }

    /* Qualified name - resolve first segment, then descend */
    size_t first_len = sep - name;
    char *first = sysml2_arena_alloc(symtab->arena, first_len + 1);
    memcpy(first, name, first_len);
    first[first_len] = '\0';

    const char *rest = sep + 2; /* Skip "::" */

    /* Resolve first segment */
    SysmlSymbol *sym = sysml_symtab_resolve(symtab, scope, first);
    if (!sym) return NULL;

    /* Get child scope and resolve rest */
    SysmlScope *child_scope = find_scope(symtab, sym->qualified_id);
    if (!child_scope) return NULL;

    return sysml_symtab_resolve(symtab, child_scope, rest);
}

/* ========== Levenshtein Distance for Suggestions ========== */

static size_t levenshtein_distance(const char *s1, const char *s2) {
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);

    if (len1 == 0) return len2;
    if (len2 == 0) return len1;

    /* Use two rows for space efficiency */
    size_t *prev = malloc((len2 + 1) * sizeof(size_t));
    size_t *curr = malloc((len2 + 1) * sizeof(size_t));

    for (size_t j = 0; j <= len2; j++) {
        prev[j] = j;
    }

    for (size_t i = 1; i <= len1; i++) {
        curr[0] = i;
        for (size_t j = 1; j <= len2; j++) {
            size_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            size_t del = prev[j] + 1;
            size_t ins = curr[j - 1] + 1;
            size_t sub = prev[j - 1] + cost;

            curr[j] = del < ins ? del : ins;
            if (sub < curr[j]) curr[j] = sub;
        }
        size_t *tmp = prev;
        prev = curr;
        curr = tmp;
    }

    size_t result = prev[len2];
    free(prev);
    free(curr);
    return result;
}

/* Get max allowed edit distance based on name length */
static size_t max_edit_distance(size_t name_len) {
    if (name_len < 4) return 1;
    if (name_len <= 8) return 2;
    return 3;
}

/* Candidate for suggestion */
typedef struct {
    const char *name;
    size_t distance;
} SuggestionCandidate;

/* Collect suggestions from a scope */
static void collect_suggestions_from_scope(
    const SysmlScope *scope,
    const char *name,
    size_t max_dist,
    SuggestionCandidate *candidates,
    size_t *candidate_count,
    size_t max_candidates
) {
    for (size_t i = 0; i < scope->symbol_capacity; i++) {
        SysmlSymbol *sym = scope->symbols[i];
        while (sym) {
            size_t dist = levenshtein_distance(name, sym->name);
            if (dist <= max_dist && dist > 0) { /* Exclude exact match */
                /* Insert into sorted candidates */
                size_t insert_pos = *candidate_count;
                for (size_t j = 0; j < *candidate_count; j++) {
                    if (dist < candidates[j].distance) {
                        insert_pos = j;
                        break;
                    }
                }

                if (insert_pos < max_candidates) {
                    /* Shift elements */
                    if (*candidate_count < max_candidates) {
                        (*candidate_count)++;
                    }
                    for (size_t j = *candidate_count - 1; j > insert_pos; j--) {
                        candidates[j] = candidates[j - 1];
                    }
                    candidates[insert_pos].name = sym->name;
                    candidates[insert_pos].distance = dist;
                }
            }
            sym = sym->next;
        }
    }
}

size_t sysml_symtab_find_similar(
    SysmlSymbolTable *symtab,
    const SysmlScope *scope,
    const char *name,
    const char **suggestions,
    size_t max_suggestions
) {
    if (!name || max_suggestions == 0) return 0;

    size_t name_len = strlen(name);
    size_t max_dist = max_edit_distance(name_len);

    /* Allocate candidate array */
    SuggestionCandidate *candidates = malloc(max_suggestions * sizeof(SuggestionCandidate));
    size_t candidate_count = 0;

    /* Search current scope and ancestors */
    const SysmlScope *s = scope ? scope : symtab->root_scope;
    while (s) {
        collect_suggestions_from_scope(s, name, max_dist, candidates,
            &candidate_count, max_suggestions);
        s = s->parent;
    }

    /* Copy results */
    for (size_t i = 0; i < candidate_count; i++) {
        suggestions[i] = candidates[i].name;
    }

    free(candidates);
    return candidate_count;
}

/* ========== Import Resolution ========== */

/* Forward declaration of find_scope */
static SysmlScope *find_scope(SysmlSymbolTable *symtab, const char *scope_id);

/* Extract the final segment from a qualified name (after the last "::") */
static const char *get_final_segment(const char *qname) {
    if (!qname) return NULL;
    const char *last = qname;
    const char *p = qname;
    while (*p) {
        if (p[0] == ':' && p[1] == ':') {
            last = p + 2;
            p += 2;
        } else {
            p++;
        }
    }
    return last;
}

/* Resolve a simple name via imports in a scope */
static SysmlSymbol *resolve_via_imports(
    SysmlSymbolTable *symtab,
    const SysmlScope *scope,
    const char *name
) {
    if (!scope || !name) return NULL;

    for (SysmlImportEntry *imp = scope->imports; imp; imp = imp->next) {
        switch (imp->import_kind) {
            case SYSML_KIND_IMPORT: {
                /* Direct import: import A::B::Engine → check if name matches "Engine" */
                const char *imported_name = get_final_segment(imp->target);
                if (imported_name && strcmp(imported_name, name) == 0) {
                    /* Resolve the full qualified name */
                    SysmlScope *target_parent = find_scope(symtab, NULL);
                    return sysml_symtab_resolve(symtab, target_parent, imp->target);
                }
                break;
            }

            case SYSML_KIND_IMPORT_ALL: {
                /* Namespace import: import A::B::* → look in A::B scope */
                SysmlScope *target_scope = find_scope(symtab, imp->target);
                if (target_scope) {
                    SysmlSymbol *sym = sysml_symtab_lookup(target_scope, name);
                    if (sym) return sym;
                }
                break;
            }

            case SYSML_KIND_IMPORT_RECURSIVE: {
                /* Recursive import: import A::B::** → search A::B and all nested */
                SysmlScope *target_scope = find_scope(symtab, imp->target);
                if (target_scope) {
                    SysmlSymbol *sym = sysml_symtab_lookup(target_scope, name);
                    if (sym) return sym;
                    /* For recursive, we'd need to search nested scopes too */
                    /* This is a simplified implementation - just check direct scope */
                }
                break;
            }

            default:
                break;
        }
    }

    return NULL;
}

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
