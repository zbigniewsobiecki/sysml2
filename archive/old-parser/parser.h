/*
 * SysML v2 Parser - Parser Interface
 *
 * Recursive descent parser for KerML and SysML v2.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_PARSER_H
#define SYSML2_PARSER_H

#include "common.h"
#include "lexer.h"
#include "ast.h"
#include "arena.h"
#include "diagnostic.h"

/* Parser state */
typedef struct {
    Sysml2Lexer *lexer;
    Sysml2Arena *arena;
    Sysml2DiagContext *diag;

    Sysml2Token current;        /* Current token */
    Sysml2Token previous;       /* Previous token */

    bool had_error;             /* Error occurred */
    bool panic_mode;            /* In panic mode (skip until sync point) */
} Sysml2Parser;

/* Initialize the parser */
void sysml2_parser_init(
    Sysml2Parser *parser,
    Sysml2Lexer *lexer,
    Sysml2Arena *arena,
    Sysml2DiagContext *diag
);

/* Parse a complete file and return the root namespace */
Sysml2AstNamespace *sysml2_parser_parse(Sysml2Parser *parser);

/* Parse a single namespace body element */
Sysml2AstMember *sysml2_parser_parse_member(Sysml2Parser *parser);

/* Expression parsing (implemented in parser_expr.c) */
Sysml2AstExpr *sysml2_parser_parse_expression(Sysml2Parser *parser);

/* Synchronization points for error recovery */
typedef enum {
    SYSML2_SYNC_NAMESPACE,      /* namespace, package, library */
    SYSML2_SYNC_TYPE,           /* type, classifier, class, etc. */
    SYSML2_SYNC_FEATURE,        /* feature, in, out, etc. */
    SYSML2_SYNC_STATEMENT,      /* ; */
    SYSML2_SYNC_BLOCK,          /* } */
} Sysml2SyncPoint;

/* Synchronize after an error */
void sysml2_parser_synchronize(Sysml2Parser *parser, Sysml2SyncPoint sync);

/* Check if current token is at a sync point */
bool sysml2_parser_at_sync_point(Sysml2Parser *parser);

#endif /* SYSML2_PARSER_H */
