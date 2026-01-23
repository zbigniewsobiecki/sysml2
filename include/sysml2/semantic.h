/*
 * SysML v2 Parser - Semantic Analysis
 *
 * Semantic validation after parsing.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_SEMANTIC_H
#define SYSML2_SEMANTIC_H

#include "common.h"
#include "arena.h"
#include "ast.h"
#include "symbol.h"
#include "diagnostic.h"
#include "intern.h"

/* Semantic analysis context */
typedef struct {
    Sysml2Arena *arena;
    Sysml2Intern *intern;
    Sysml2DiagContext *diag;
    Sysml2SymbolTable symbols;

    /* Analysis state */
    bool in_type_body;          /* Currently in a type body */
    bool checking_cycles;       /* Currently checking for cycles */
} Sysml2SemanticContext;

/* Initialize semantic analysis context */
void sysml2_semantic_init(
    Sysml2SemanticContext *ctx,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    Sysml2DiagContext *diag
);

/* Destroy semantic analysis context */
void sysml2_semantic_destroy(Sysml2SemanticContext *ctx);

/* Run semantic analysis on the AST */
void sysml2_semantic_analyze(Sysml2SemanticContext *ctx, Sysml2AstNamespace *ast);

/* Individual semantic checks */

/* Check for undefined type references */
void sysml2_semantic_check_references(
    Sysml2SemanticContext *ctx,
    Sysml2AstNamespace *ast
);

/* Check for duplicate names in scopes */
void sysml2_semantic_check_duplicates(
    Sysml2SemanticContext *ctx,
    Sysml2AstNamespace *ast
);

/* Check for circular specializations */
void sysml2_semantic_check_cycles(
    Sysml2SemanticContext *ctx,
    Sysml2AstNamespace *ast
);

/* Check type compatibility for typed-by relationships */
void sysml2_semantic_check_typing(
    Sysml2SemanticContext *ctx,
    Sysml2AstNamespace *ast
);

/* Check multiplicity constraints */
void sysml2_semantic_check_multiplicity(
    Sysml2SemanticContext *ctx,
    Sysml2AstNamespace *ast
);

/* Resolve imports and populate symbol table */
void sysml2_semantic_resolve_imports(
    Sysml2SemanticContext *ctx,
    Sysml2AstNamespace *ast
);

/* Register built-in types */
void sysml2_semantic_register_builtins(Sysml2SemanticContext *ctx);

#endif /* SYSML2_SEMANTIC_H */
