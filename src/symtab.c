/*
 * SysML v2 Parser - Symbol Table Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/symtab.h"
#include <stdlib.h>
#include <string.h>

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
                /* Direct import: import A::B::Engine -> check if name matches "Engine" */
                const char *imported_name = get_final_segment(imp->target);
                if (imported_name && strcmp(imported_name, name) == 0) {
                    /* Resolve the full qualified name */
                    SysmlScope *target_parent = find_scope(symtab, NULL);
                    return sysml_symtab_resolve(symtab, target_parent, imp->target);
                }
                break;
            }

            case SYSML_KIND_IMPORT_ALL: {
                /* Namespace import: import A::B::* -> look in A::B scope */
                SysmlScope *target_scope = find_scope(symtab, imp->target);
                if (target_scope) {
                    SysmlSymbol *sym = sysml_symtab_lookup(target_scope, name);
                    if (sym) return sym;
                }
                break;
            }

            case SYSML_KIND_IMPORT_RECURSIVE: {
                /* Recursive import: import A::B::** -> search A::B and all nested */
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
