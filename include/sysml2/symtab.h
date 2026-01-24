/*
 * SysML v2 Parser - Symbol Table
 *
 * Two-level hash table for name resolution in semantic validation.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_SYMTAB_H
#define SYSML2_SYMTAB_H

#include "common.h"
#include "arena.h"
#include "intern.h"
#include "ast.h"

/* Default symbol table sizes */
#define SYSML_SYMTAB_DEFAULT_SCOPE_CAPACITY 256
#define SYSML_SYMTAB_DEFAULT_SYMBOL_CAPACITY 32

/*
 * Symbol Entry - represents a named element in a scope
 */
typedef struct Sysml2Symbol {
    const char *name;           /* Local name (interned) */
    const char *qualified_id;   /* Full path ID */
    SysmlNode *node;            /* AST node */
    struct Sysml2Symbol *next;   /* Hash chain */
} Sysml2Symbol;

/*
 * Import Entry - represents an import in a scope
 */
typedef struct Sysml2ImportEntry {
    const char *target;         /* Imported qualified name */
    SysmlNodeKind import_kind;  /* IMPORT, IMPORT_ALL, IMPORT_RECURSIVE */
    struct Sysml2ImportEntry *next; /* Next import in chain */
} Sysml2ImportEntry;

/*
 * Scope Entry - represents a namespace/container
 */
typedef struct Sysml2Scope {
    const char *id;             /* Scope's qualified ID */
    struct Sysml2Scope *parent;  /* Enclosing scope */
    Sysml2Symbol **symbols;      /* Hash table of symbols */
    size_t symbol_count;
    size_t symbol_capacity;
    Sysml2ImportEntry *imports;  /* Linked list of imports */
} Sysml2Scope;

/*
 * Symbol Table - two-level hash table for name resolution
 */
typedef struct {
    Sysml2Arena *arena;         /* Arena for allocations */
    Sysml2Intern *intern;       /* String interning */

    /* Scope index: maps scope IDs to Sysml2Scope* */
    Sysml2Scope **scopes;
    size_t scope_count;
    size_t scope_capacity;

    /* Root scope (global/unnamed namespace) */
    Sysml2Scope *root_scope;
} Sysml2SymbolTable;

/*
 * Initialize a symbol table
 *
 * @param symtab Symbol table to initialize
 * @param arena Memory arena for allocations
 * @param intern String interning table
 */
void sysml2_symtab_init(
    Sysml2SymbolTable *symtab,
    Sysml2Arena *arena,
    Sysml2Intern *intern
);

/*
 * Destroy a symbol table
 *
 * Note: Memory is managed by the arena, so this just resets state.
 */
void sysml2_symtab_destroy(Sysml2SymbolTable *symtab);

/*
 * Get or create a scope by ID
 *
 * @param symtab Symbol table
 * @param scope_id Qualified scope ID (NULL for root scope)
 * @return Scope entry (never NULL)
 */
Sysml2Scope *sysml2_symtab_get_or_create_scope(
    Sysml2SymbolTable *symtab,
    const char *scope_id
);

/*
 * Add a symbol to a scope
 *
 * @param symtab Symbol table
 * @param scope Scope to add to
 * @param name Local name
 * @param qualified_id Full path ID
 * @param node AST node
 * @return The new symbol, or existing symbol if duplicate
 */
Sysml2Symbol *sysml2_symtab_add(
    Sysml2SymbolTable *symtab,
    Sysml2Scope *scope,
    const char *name,
    const char *qualified_id,
    SysmlNode *node
);

/*
 * Look up a symbol by local name in a specific scope (no parent search)
 *
 * @param scope Scope to search
 * @param name Local name
 * @return Symbol if found, NULL otherwise
 */
Sysml2Symbol *sysml2_symtab_lookup(
    const Sysml2Scope *scope,
    const char *name
);

/*
 * Resolve a name reference from a given scope
 *
 * Handles both simple names (walk up scope chain) and
 * qualified names (resolve segments left-to-right).
 *
 * @param symtab Symbol table
 * @param scope Starting scope for resolution
 * @param name Name to resolve (may contain "::")
 * @return Resolved symbol, or NULL if not found
 */
Sysml2Symbol *sysml2_symtab_resolve(
    Sysml2SymbolTable *symtab,
    const Sysml2Scope *scope,
    const char *name
);

/*
 * Find similar names for "did you mean?" suggestions
 *
 * @param symtab Symbol table
 * @param scope Starting scope
 * @param name Name that wasn't found
 * @param suggestions Output array for suggestions
 * @param max_suggestions Maximum suggestions to return
 * @return Number of suggestions found
 */
size_t sysml2_symtab_find_similar(
    Sysml2SymbolTable *symtab,
    const Sysml2Scope *scope,
    const char *name,
    const char **suggestions,
    size_t max_suggestions
);

#endif /* SYSML2_SYMTAB_H */
