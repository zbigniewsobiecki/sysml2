/*
 * SysML v2 Parser - Symbol Table
 *
 * Scoped symbol table for semantic analysis.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_SYMBOL_H
#define SYSML2_SYMBOL_H

#include "common.h"
#include "arena.h"
#include "ast.h"

/* Symbol kinds */
typedef enum {
    SYSML2_SYM_NAMESPACE,
    SYSML2_SYM_PACKAGE,
    SYSML2_SYM_TYPE,
    SYSML2_SYM_CLASSIFIER,
    SYSML2_SYM_FEATURE,
    SYSML2_SYM_ALIAS,
    SYSML2_SYM_IMPORT,
} Sysml2SymbolKind;

/* Symbol entry */
typedef struct Sysml2Symbol {
    const char *name;               /* Interned name */
    Sysml2SymbolKind kind;
    Sysml2Visibility visibility;
    void *ast_node;                 /* Pointer to AST node */
    struct Sysml2Symbol *next;      /* Hash collision chain */
    struct Sysml2Scope *scope;      /* Enclosing scope */
    Sysml2SourceRange definition;   /* Where symbol was defined */
} Sysml2Symbol;

/* Scope (namespace, package, type body, etc.) */
typedef struct Sysml2Scope {
    struct Sysml2Scope *parent;     /* Enclosing scope */
    const char *name;               /* Scope name (for qualified names) */
    Sysml2SymbolKind kind;          /* What kind of construct this scope is for */
    Sysml2Symbol **symbols;         /* Hash table of symbols */
    size_t symbol_capacity;
    size_t symbol_count;
} Sysml2Scope;

/* Symbol table */
typedef struct {
    Sysml2Arena *arena;
    Sysml2Scope *global_scope;      /* Root scope */
    Sysml2Scope *current_scope;     /* Current scope during analysis */
} Sysml2SymbolTable;

/* Initialize symbol table */
void sysml2_symtab_init(Sysml2SymbolTable *table, Sysml2Arena *arena);

/* Destroy symbol table */
void sysml2_symtab_destroy(Sysml2SymbolTable *table);

/* Create and enter a new scope */
Sysml2Scope *sysml2_symtab_push_scope(
    Sysml2SymbolTable *table,
    const char *name,
    Sysml2SymbolKind kind
);

/* Exit current scope, return to parent */
void sysml2_symtab_pop_scope(Sysml2SymbolTable *table);

/* Define a symbol in the current scope */
Sysml2Symbol *sysml2_symtab_define(
    Sysml2SymbolTable *table,
    const char *name,
    Sysml2SymbolKind kind,
    Sysml2Visibility visibility,
    void *ast_node,
    Sysml2SourceRange definition
);

/* Look up a symbol in current scope chain */
Sysml2Symbol *sysml2_symtab_lookup(Sysml2SymbolTable *table, const char *name);

/* Look up a symbol only in the current scope (no parent search) */
Sysml2Symbol *sysml2_symtab_lookup_local(Sysml2SymbolTable *table, const char *name);

/* Look up a qualified name */
Sysml2Symbol *sysml2_symtab_lookup_qualified(
    Sysml2SymbolTable *table,
    Sysml2AstQualifiedName *qname
);

/* Get the qualified name of a symbol */
char *sysml2_symtab_get_qualified_name(
    Sysml2SymbolTable *table,
    Sysml2Symbol *symbol
);

/* Check if a name exists in the current scope (for duplicate detection) */
bool sysml2_symtab_has_local(Sysml2SymbolTable *table, const char *name);

/* Get the current scope */
Sysml2Scope *sysml2_symtab_current_scope(Sysml2SymbolTable *table);

/* Find similar names (for "did you mean?" suggestions) */
const char *sysml2_symtab_find_similar(
    Sysml2SymbolTable *table,
    const char *name,
    size_t max_distance
);

#endif /* SYSML2_SYMBOL_H */
