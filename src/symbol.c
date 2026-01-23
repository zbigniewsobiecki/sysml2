/*
 * SysML v2 Parser - Symbol Table Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/symbol.h"
#include <string.h>

#define DEFAULT_SCOPE_CAPACITY 64

/* FNV-1a hash for symbol lookup */
static uint32_t hash_name(const char *name) {
    uint32_t hash = 2166136261u;
    while (*name) {
        hash ^= (uint8_t)*name++;
        hash *= 16777619u;
    }
    return hash;
}

/* Create a new scope */
static Sysml2Scope *create_scope(
    Sysml2Arena *arena,
    Sysml2Scope *parent,
    const char *name,
    Sysml2SymbolKind kind
) {
    Sysml2Scope *scope = SYSML2_ARENA_NEW(arena, Sysml2Scope);
    scope->parent = parent;
    scope->name = name;
    scope->kind = kind;
    scope->symbol_capacity = DEFAULT_SCOPE_CAPACITY;
    scope->symbol_count = 0;
    scope->symbols = SYSML2_ARENA_NEW_ARRAY(arena, Sysml2Symbol *, DEFAULT_SCOPE_CAPACITY);
    return scope;
}

void sysml2_symtab_init(Sysml2SymbolTable *table, Sysml2Arena *arena) {
    table->arena = arena;
    table->global_scope = create_scope(arena, NULL, NULL, SYSML2_SYM_NAMESPACE);
    table->current_scope = table->global_scope;
}

void sysml2_symtab_destroy(Sysml2SymbolTable *table) {
    /* Everything is in the arena, nothing to free */
    table->global_scope = NULL;
    table->current_scope = NULL;
}

Sysml2Scope *sysml2_symtab_push_scope(
    Sysml2SymbolTable *table,
    const char *name,
    Sysml2SymbolKind kind
) {
    Sysml2Scope *scope = create_scope(table->arena, table->current_scope, name, kind);
    table->current_scope = scope;
    return scope;
}

void sysml2_symtab_pop_scope(Sysml2SymbolTable *table) {
    if (table->current_scope && table->current_scope->parent) {
        table->current_scope = table->current_scope->parent;
    }
}

Sysml2Symbol *sysml2_symtab_define(
    Sysml2SymbolTable *table,
    const char *name,
    Sysml2SymbolKind kind,
    Sysml2Visibility visibility,
    void *ast_node,
    Sysml2SourceRange definition
) {
    if (!name || !table->current_scope) return NULL;

    Sysml2Scope *scope = table->current_scope;
    uint32_t hash = hash_name(name);
    size_t index = hash % scope->symbol_capacity;

    /* Check for existing symbol */
    Sysml2Symbol *existing = scope->symbols[index];
    while (existing) {
        if (existing->name == name) { /* Pointer comparison (interned) */
            return NULL; /* Duplicate */
        }
        existing = existing->next;
    }

    /* Create new symbol */
    Sysml2Symbol *sym = SYSML2_ARENA_NEW(table->arena, Sysml2Symbol);
    sym->name = name;
    sym->kind = kind;
    sym->visibility = visibility;
    sym->ast_node = ast_node;
    sym->scope = scope;
    sym->definition = definition;

    /* Insert at head of chain */
    sym->next = scope->symbols[index];
    scope->symbols[index] = sym;
    scope->symbol_count++;

    return sym;
}

Sysml2Symbol *sysml2_symtab_lookup(Sysml2SymbolTable *table, const char *name) {
    if (!name) return NULL;

    uint32_t hash = hash_name(name);

    /* Search up the scope chain */
    for (Sysml2Scope *scope = table->current_scope; scope; scope = scope->parent) {
        size_t index = hash % scope->symbol_capacity;
        for (Sysml2Symbol *sym = scope->symbols[index]; sym; sym = sym->next) {
            if (sym->name == name) { /* Pointer comparison (interned) */
                return sym;
            }
        }
    }

    return NULL;
}

Sysml2Symbol *sysml2_symtab_lookup_local(Sysml2SymbolTable *table, const char *name) {
    if (!name || !table->current_scope) return NULL;

    Sysml2Scope *scope = table->current_scope;
    uint32_t hash = hash_name(name);
    size_t index = hash % scope->symbol_capacity;

    for (Sysml2Symbol *sym = scope->symbols[index]; sym; sym = sym->next) {
        if (sym->name == name) {
            return sym;
        }
    }

    return NULL;
}

Sysml2Symbol *sysml2_symtab_lookup_qualified(
    Sysml2SymbolTable *table,
    Sysml2AstQualifiedName *qname
) {
    if (!qname || qname->segment_count == 0) return NULL;

    /* Start from global scope if global reference */
    Sysml2Scope *scope = qname->is_global ? table->global_scope : table->current_scope;

    Sysml2Symbol *sym = NULL;

    for (size_t i = 0; i < qname->segment_count; i++) {
        const char *segment = qname->segments[i];
        uint32_t hash = hash_name(segment);

        /* Search in current scope */
        sym = NULL;
        for (Sysml2Scope *s = scope; s && !sym; s = (i == 0 && !qname->is_global) ? s->parent : NULL) {
            size_t index = hash % s->symbol_capacity;
            for (Sysml2Symbol *candidate = s->symbols[index]; candidate; candidate = candidate->next) {
                if (candidate->name == segment) {
                    sym = candidate;
                    break;
                }
            }
        }

        if (!sym) return NULL;

        /* For next segment, we need to look inside this symbol's scope */
        if (i < qname->segment_count - 1) {
            /* This symbol should have an associated scope */
            /* For now, we'd need to track scopes per symbol */
            /* This is a simplification - in full implementation we'd traverse the scope tree */
            return NULL; /* TODO: implement nested scope lookup */
        }
    }

    return sym;
}

char *sysml2_symtab_get_qualified_name(
    Sysml2SymbolTable *table,
    Sysml2Symbol *symbol
) {
    if (!symbol) return NULL;

    /* Collect scope names */
    const char *names[64];
    size_t count = 0;

    names[count++] = symbol->name;

    for (Sysml2Scope *scope = symbol->scope; scope && scope->name && count < 64; scope = scope->parent) {
        names[count++] = scope->name;
    }

    /* Calculate total length */
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        total += strlen(names[i]);
        if (i < count - 1) total += 2; /* :: */
    }

    /* Build string in reverse order */
    char *result = sysml2_arena_alloc(table->arena, total + 1);
    char *p = result;

    for (size_t i = count; i > 0; i--) {
        size_t len = strlen(names[i - 1]);
        memcpy(p, names[i - 1], len);
        p += len;
        if (i > 1) {
            *p++ = ':';
            *p++ = ':';
        }
    }
    *p = '\0';

    return result;
}

bool sysml2_symtab_has_local(Sysml2SymbolTable *table, const char *name) {
    return sysml2_symtab_lookup_local(table, name) != NULL;
}

Sysml2Scope *sysml2_symtab_current_scope(Sysml2SymbolTable *table) {
    return table->current_scope;
}

/* Levenshtein distance for "did you mean?" suggestions */
static size_t levenshtein_distance(const char *s1, const char *s2) {
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);

    if (len1 == 0) return len2;
    if (len2 == 0) return len1;

    /* Use simple algorithm for short strings */
    if (len1 > 20 || len2 > 20) return SIZE_MAX;

    size_t matrix[21][21];

    for (size_t i = 0; i <= len1; i++) matrix[i][0] = i;
    for (size_t j = 0; j <= len2; j++) matrix[0][j] = j;

    for (size_t i = 1; i <= len1; i++) {
        for (size_t j = 1; j <= len2; j++) {
            size_t cost = (s1[i-1] == s2[j-1]) ? 0 : 1;
            size_t del = matrix[i-1][j] + 1;
            size_t ins = matrix[i][j-1] + 1;
            size_t sub = matrix[i-1][j-1] + cost;

            matrix[i][j] = del;
            if (ins < matrix[i][j]) matrix[i][j] = ins;
            if (sub < matrix[i][j]) matrix[i][j] = sub;
        }
    }

    return matrix[len1][len2];
}

const char *sysml2_symtab_find_similar(
    Sysml2SymbolTable *table,
    const char *name,
    size_t max_distance
) {
    if (!name) return NULL;

    const char *best_match = NULL;
    size_t best_distance = max_distance + 1;

    /* Search all scopes */
    for (Sysml2Scope *scope = table->current_scope; scope; scope = scope->parent) {
        for (size_t i = 0; i < scope->symbol_capacity; i++) {
            for (Sysml2Symbol *sym = scope->symbols[i]; sym; sym = sym->next) {
                size_t dist = levenshtein_distance(name, sym->name);
                if (dist <= max_distance && dist < best_distance) {
                    best_distance = dist;
                    best_match = sym->name;
                }
            }
        }
    }

    return best_match;
}
