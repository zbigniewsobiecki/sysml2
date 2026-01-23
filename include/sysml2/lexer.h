/*
 * SysML v2 Parser - Lexer Interface
 *
 * DFA-based lexer with line/column tracking and error recovery.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_LEXER_H
#define SYSML2_LEXER_H

#include "common.h"
#include "token.h"
#include "intern.h"
#include "diagnostic.h"

/* Lexer state */
typedef struct {
    const Sysml2SourceFile *source;  /* Source file being lexed */
    Sysml2Intern *intern;            /* String interning table */
    Sysml2DiagContext *diag;         /* Diagnostic context */

    const char *start;               /* Start of current token */
    const char *current;             /* Current position */
    const char *end;                 /* End of source */

    uint32_t line;                   /* Current line (1-based) */
    uint32_t column;                 /* Current column (1-based) */
    uint32_t token_line;             /* Line where current token started */
    uint32_t token_column;           /* Column where current token started */

    bool had_error;                  /* Error occurred in current token */
} Sysml2Lexer;

/* Initialize the lexer for a source file */
void sysml2_lexer_init(
    Sysml2Lexer *lexer,
    const Sysml2SourceFile *source,
    Sysml2Intern *intern,
    Sysml2DiagContext *diag
);

/* Get the next token */
Sysml2Token sysml2_lexer_next(Sysml2Lexer *lexer);

/* Peek at the next token without consuming it */
Sysml2Token sysml2_lexer_peek(Sysml2Lexer *lexer);

/* Check if at end of file */
bool sysml2_lexer_is_eof(const Sysml2Lexer *lexer);

/* Get current source location */
Sysml2SourceLoc sysml2_lexer_current_loc(const Sysml2Lexer *lexer);

/* Keyword lookup functions */
void sysml2_keywords_init(void);
Sysml2TokenType sysml2_keyword_lookup(const char *str, size_t length);

#endif /* SYSML2_LEXER_H */
