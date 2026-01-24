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
typedef struct SysmlSymbol {
    const char *name;           /* Local name (interned) */
    const char *qualified_id;   /* Full path ID */
    SysmlNode *node;            /* AST node */
    struct SysmlSymbol *next;   /* Hash chain */
} SysmlSymbol;

/*
 * Import Entry - represents an import in a scope
 */
typedef struct SysmlImportEntry {
    const char *target;         /* Imported qualified name */
    SysmlNodeKind import_kind;  /* IMPORT, IMPORT_ALL, IMPORT_RECURSIVE */
    struct SysmlImportEntry *next; /* Next import in chain */
} SysmlImportEntry;

/*
 * Scope Entry - represents a namespace/container
 */
typedef struct SysmlScope {
    const char *id;             /* Scope's qualified ID */
    struct SysmlScope *parent;  /* Enclosing scope */
    SysmlSymbol **symbols;      /* Hash table of symbols */
    size_t symbol_count;
    size_t symbol_capacity;
    SysmlImportEntry *imports;  /* Linked list of imports */
} SysmlScope;

/*
 * Symbol Table - two-level hash table for name resolution
 */
typedef struct {
    Sysml2Arena *arena;         /* Arena for allocations */
    Sysml2Intern *intern;       /* String interning */

    /* Scope index: maps scope IDs to SysmlScope* */
    SysmlScope **scopes;
    size_t scope_count;
    size_t scope_capacity;

    /* Root scope (global/unnamed namespace) */
    SysmlScope *root_scope;
} SysmlSymbolTable;

/*
 * Initialize a symbol table
 *
 * @param symtab Symbol table to initialize
 * @param arena Memory arena for allocations
 * @param intern String interning table
 */
void sysml_symtab_init(
    SysmlSymbolTable *symtab,
    Sysml2Arena *arena,
    Sysml2Intern *intern
);

/*
 * Destroy a symbol table
 *
 * Note: Memory is managed by the arena, so this just resets state.
 */
void sysml_symtab_destroy(SysmlSymbolTable *symtab);

/*
 * Get or create a scope by ID
 *
 * @param symtab Symbol table
 * @param scope_id Qualified scope ID (NULL for root scope)
 * @return Scope entry (never NULL)
 */
SysmlScope *sysml_symtab_get_or_create_scope(
    SysmlSymbolTable *symtab,
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
SysmlSymbol *sysml_symtab_add(
    SysmlSymbolTable *symtab,
    SysmlScope *scope,
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
SysmlSymbol *sysml_symtab_lookup(
    const SysmlScope *scope,
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
SysmlSymbol *sysml_symtab_resolve(
    SysmlSymbolTable *symtab,
    const SysmlScope *scope,
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
size_t sysml_symtab_find_similar(
    SysmlSymbolTable *symtab,
    const SysmlScope *scope,
    const char *name,
    const char **suggestions,
    size_t max_suggestions
);

#endif /* SYSML2_SYMTAB_H */
