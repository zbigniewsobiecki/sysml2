/*
 * SysML v2 Parser - Lexer Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/lexer.h"
#include <ctype.h>
#include <string.h>

/* Character classification helpers */
static inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static inline bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static inline bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

static inline bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline bool is_hex_digit(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* Lexer helper functions */
static inline bool is_eof(Sysml2Lexer *lexer) {
    return lexer->current >= lexer->end;
}

static inline char peek(Sysml2Lexer *lexer) {
    if (is_eof(lexer)) return '\0';
    return *lexer->current;
}

static inline char peek_next(Sysml2Lexer *lexer) {
    if (lexer->current + 1 >= lexer->end) return '\0';
    return lexer->current[1];
}

static inline char peek_ahead(Sysml2Lexer *lexer, int offset) {
    if (lexer->current + offset >= lexer->end) return '\0';
    return lexer->current[offset];
}

static char advance(Sysml2Lexer *lexer) {
    if (is_eof(lexer)) return '\0';
    char c = *lexer->current++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static bool match(Sysml2Lexer *lexer, char expected) {
    if (is_eof(lexer) || *lexer->current != expected) return false;
    advance(lexer);
    return true;
}

static Sysml2SourceLoc current_loc(Sysml2Lexer *lexer) {
    return (Sysml2SourceLoc){
        .line = lexer->line,
        .column = lexer->column,
        .offset = (uint32_t)(lexer->current - lexer->source->content)
    };
}

static Sysml2SourceLoc token_start_loc(Sysml2Lexer *lexer) {
    return (Sysml2SourceLoc){
        .line = lexer->token_line,
        .column = lexer->token_column,
        .offset = (uint32_t)(lexer->start - lexer->source->content)
    };
}

static Sysml2Token make_token(Sysml2Lexer *lexer, Sysml2TokenType type) {
    return (Sysml2Token){
        .type = type,
        .range = {token_start_loc(lexer), current_loc(lexer)},
        .text = sysml2_sv_from_parts(lexer->start, lexer->current - lexer->start)
    };
}

static Sysml2Token make_error_token(Sysml2Lexer *lexer, const char *message) {
    lexer->had_error = true;

    Sysml2Diagnostic *diag = sysml2_diag_create(
        lexer->diag,
        SYSML2_DIAG_E1001_INVALID_CHAR,
        SYSML2_SEVERITY_ERROR,
        lexer->source,
        (Sysml2SourceRange){token_start_loc(lexer), current_loc(lexer)},
        message
    );
    sysml2_diag_emit(lexer->diag, diag);

    return make_token(lexer, SYSML2_TOKEN_ERROR);
}

/* Skip whitespace and comments */
static void skip_whitespace(Sysml2Lexer *lexer) {
    while (!is_eof(lexer)) {
        char c = peek(lexer);

        if (is_whitespace(c)) {
            advance(lexer);
        } else if (c == '/' && peek_next(lexer) == '/') {
            /* Single-line comment */
            while (!is_eof(lexer) && peek(lexer) != '\n') {
                advance(lexer);
            }
        } else if (c == '/' && peek_next(lexer) == '*') {
            /* Multi-line comment */
            advance(lexer); /* / */
            advance(lexer); /* * */
            int nesting = 1;

            while (!is_eof(lexer) && nesting > 0) {
                if (peek(lexer) == '/' && peek_next(lexer) == '*') {
                    advance(lexer);
                    advance(lexer);
                    nesting++;
                } else if (peek(lexer) == '*' && peek_next(lexer) == '/') {
                    advance(lexer);
                    advance(lexer);
                    nesting--;
                } else {
                    advance(lexer);
                }
            }

            if (nesting > 0) {
                /* Unterminated comment */
                Sysml2Diagnostic *diag = sysml2_diag_create(
                    lexer->diag,
                    SYSML2_DIAG_E1003_UNTERMINATED_COMMENT,
                    SYSML2_SEVERITY_ERROR,
                    lexer->source,
                    (Sysml2SourceRange){token_start_loc(lexer), current_loc(lexer)},
                    "unterminated block comment"
                );
                sysml2_diag_emit(lexer->diag, diag);
            }
        } else {
            break;
        }
    }
}

/* Scan identifier or keyword */
static Sysml2Token scan_identifier(Sysml2Lexer *lexer) {
    while (is_alnum(peek(lexer))) {
        advance(lexer);
    }

    size_t length = lexer->current - lexer->start;
    Sysml2TokenType type = sysml2_keyword_lookup(lexer->start, length);

    return make_token(lexer, type);
}

/* Scan unrestricted name: 'name with spaces' */
static Sysml2Token scan_unrestricted_name(Sysml2Lexer *lexer) {
    advance(lexer); /* Opening ' */

    while (!is_eof(lexer) && peek(lexer) != '\'') {
        if (peek(lexer) == '\n') {
            /* Unrestricted names cannot span lines */
            return make_error_token(lexer, "unterminated unrestricted name (newline in name)");
        }
        if (peek(lexer) == '\\' && peek_next(lexer) != '\0') {
            advance(lexer); /* Skip escape char */
        }
        advance(lexer);
    }

    if (is_eof(lexer)) {
        return make_error_token(lexer, "unterminated unrestricted name");
    }

    advance(lexer); /* Closing ' */
    return make_token(lexer, SYSML2_TOKEN_UNRESTRICTED_NAME);
}

/* Scan string literal: "string" */
static Sysml2Token scan_string(Sysml2Lexer *lexer) {
    advance(lexer); /* Opening " */

    while (!is_eof(lexer) && peek(lexer) != '"') {
        if (peek(lexer) == '\n') {
            return make_error_token(lexer, "unterminated string literal (newline in string)");
        }
        if (peek(lexer) == '\\') {
            advance(lexer); /* Skip escape char */
            if (!is_eof(lexer)) {
                advance(lexer);
            }
        } else {
            advance(lexer);
        }
    }

    if (is_eof(lexer)) {
        return make_error_token(lexer, "unterminated string literal");
    }

    advance(lexer); /* Closing " */
    return make_token(lexer, SYSML2_TOKEN_STRING);
}

/* Scan number: integer or real */
static Sysml2Token scan_number(Sysml2Lexer *lexer) {
    bool is_real = false;

    /* Hex number */
    if (peek(lexer) == '0' && (peek_next(lexer) == 'x' || peek_next(lexer) == 'X')) {
        advance(lexer); /* 0 */
        advance(lexer); /* x */

        while (is_hex_digit(peek(lexer))) {
            advance(lexer);
        }
        return make_token(lexer, SYSML2_TOKEN_INTEGER);
    }

    /* Integer part */
    while (is_digit(peek(lexer))) {
        advance(lexer);
    }

    /* Decimal point */
    if (peek(lexer) == '.' && is_digit(peek_next(lexer))) {
        is_real = true;
        advance(lexer); /* . */
        while (is_digit(peek(lexer))) {
            advance(lexer);
        }
    }

    /* Exponent */
    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        is_real = true;
        advance(lexer); /* e or E */
        if (peek(lexer) == '+' || peek(lexer) == '-') {
            advance(lexer);
        }
        if (!is_digit(peek(lexer))) {
            return make_error_token(lexer, "invalid number: expected exponent digits");
        }
        while (is_digit(peek(lexer))) {
            advance(lexer);
        }
    }

    return make_token(lexer, is_real ? SYSML2_TOKEN_REAL : SYSML2_TOKEN_INTEGER);
}

void sysml2_lexer_init(
    Sysml2Lexer *lexer,
    const Sysml2SourceFile *source,
    Sysml2Intern *intern,
    Sysml2DiagContext *diag
) {
    lexer->source = source;
    lexer->intern = intern;
    lexer->diag = diag;

    lexer->start = source->content;
    lexer->current = source->content;
    lexer->end = source->content + source->content_length;

    lexer->line = 1;
    lexer->column = 1;
    lexer->token_line = 1;
    lexer->token_column = 1;

    lexer->had_error = false;

    /* Initialize keyword table */
    sysml2_keywords_init();
}

Sysml2Token sysml2_lexer_next(Sysml2Lexer *lexer) {
    skip_whitespace(lexer);

    lexer->start = lexer->current;
    lexer->token_line = lexer->line;
    lexer->token_column = lexer->column;

    if (is_eof(lexer)) {
        return make_token(lexer, SYSML2_TOKEN_EOF);
    }

    char c = advance(lexer);

    /* Identifiers and keywords */
    if (is_alpha(c)) {
        lexer->current--; /* Back up */
        lexer->column--;
        return scan_identifier(lexer);
    }

    /* Numbers */
    if (is_digit(c)) {
        lexer->current--;
        lexer->column--;
        return scan_number(lexer);
    }

    switch (c) {
        /* Single-character tokens */
        case '{': return make_token(lexer, SYSML2_TOKEN_LBRACE);
        case '}': return make_token(lexer, SYSML2_TOKEN_RBRACE);
        case '[': return make_token(lexer, SYSML2_TOKEN_LBRACKET);
        case ']': return make_token(lexer, SYSML2_TOKEN_RBRACKET);
        case '(': return make_token(lexer, SYSML2_TOKEN_LPAREN);
        case ')': return make_token(lexer, SYSML2_TOKEN_RPAREN);
        case ';': return make_token(lexer, SYSML2_TOKEN_SEMICOLON);
        case ',': return make_token(lexer, SYSML2_TOKEN_COMMA);
        case '~': return make_token(lexer, SYSML2_TOKEN_TILDE);
        case '@': return make_token(lexer, SYSML2_TOKEN_AT);
        case '#': return make_token(lexer, SYSML2_TOKEN_HASH);
        case '?': return make_token(lexer, SYSML2_TOKEN_QUESTION);
        case '+': return make_token(lexer, SYSML2_TOKEN_PLUS);
        case '%': return make_token(lexer, SYSML2_TOKEN_PERCENT);
        case '^': return make_token(lexer, SYSML2_TOKEN_CARET);
        case '&': return make_token(lexer, SYSML2_TOKEN_AMP);
        case '|': return make_token(lexer, SYSML2_TOKEN_PIPE);

        /* Two-character tokens */
        case '.':
            if (match(lexer, '.')) {
                if (match(lexer, '.')) {
                    return make_token(lexer, SYSML2_TOKEN_DOT_DOT_DOT);
                }
                return make_token(lexer, SYSML2_TOKEN_DOT_DOT);
            }
            return make_token(lexer, SYSML2_TOKEN_DOT);

        case ':':
            if (match(lexer, ':')) {
                if (match(lexer, '>')) {
                    return make_token(lexer, SYSML2_TOKEN_COLON_COLON_GT);
                }
                return make_token(lexer, SYSML2_TOKEN_COLON_COLON);
            }
            if (match(lexer, '>')) {
                if (match(lexer, '>')) {
                    return make_token(lexer, SYSML2_TOKEN_COLON_GT_GT);
                }
                return make_token(lexer, SYSML2_TOKEN_COLON_GT);
            }
            return make_token(lexer, SYSML2_TOKEN_COLON);

        case '-':
            if (match(lexer, '>')) {
                return make_token(lexer, SYSML2_TOKEN_ARROW);
            }
            return make_token(lexer, SYSML2_TOKEN_MINUS);

        case '*':
            if (match(lexer, '*')) {
                return make_token(lexer, SYSML2_TOKEN_STAR_STAR);
            }
            return make_token(lexer, SYSML2_TOKEN_STAR);

        case '/':
            return make_token(lexer, SYSML2_TOKEN_SLASH);

        case '=':
            if (match(lexer, '=')) {
                if (match(lexer, '=')) {
                    return make_token(lexer, SYSML2_TOKEN_EQ_EQ_EQ);
                }
                return make_token(lexer, SYSML2_TOKEN_EQ_EQ);
            }
            return make_token(lexer, SYSML2_TOKEN_EQ);

        case '!':
            if (match(lexer, '=')) {
                if (match(lexer, '=')) {
                    return make_token(lexer, SYSML2_TOKEN_BANG_EQ_EQ);
                }
                return make_token(lexer, SYSML2_TOKEN_BANG_EQ);
            }
            return make_token(lexer, SYSML2_TOKEN_BANG);

        case '<':
            if (match(lexer, '=')) {
                return make_token(lexer, SYSML2_TOKEN_LT_EQ);
            }
            return make_token(lexer, SYSML2_TOKEN_LT);

        case '>':
            if (match(lexer, '=')) {
                return make_token(lexer, SYSML2_TOKEN_GT_EQ);
            }
            return make_token(lexer, SYSML2_TOKEN_GT);

        /* String and name literals */
        case '\'':
            lexer->current--;
            lexer->column--;
            return scan_unrestricted_name(lexer);

        case '"':
            lexer->current--;
            lexer->column--;
            return scan_string(lexer);

        default:
            return make_error_token(lexer, "unexpected character");
    }
}

Sysml2Token sysml2_lexer_peek(Sysml2Lexer *lexer) {
    /* Save state */
    const char *start = lexer->start;
    const char *current = lexer->current;
    uint32_t line = lexer->line;
    uint32_t column = lexer->column;
    uint32_t token_line = lexer->token_line;
    uint32_t token_column = lexer->token_column;

    Sysml2Token token = sysml2_lexer_next(lexer);

    /* Restore state */
    lexer->start = start;
    lexer->current = current;
    lexer->line = line;
    lexer->column = column;
    lexer->token_line = token_line;
    lexer->token_column = token_column;

    return token;
}

bool sysml2_lexer_is_eof(const Sysml2Lexer *lexer) {
    return lexer->current >= lexer->end;
}

Sysml2SourceLoc sysml2_lexer_current_loc(const Sysml2Lexer *lexer) {
    return (Sysml2SourceLoc){
        .line = lexer->line,
        .column = lexer->column,
        .offset = (uint32_t)(lexer->current - lexer->source->content)
    };
}

/* Token type to string */
const char *sysml2_token_type_to_string(Sysml2TokenType type) {
    switch (type) {
        case SYSML2_TOKEN_EOF: return "EOF";
        case SYSML2_TOKEN_ERROR: return "ERROR";
        case SYSML2_TOKEN_IDENTIFIER: return "IDENTIFIER";
        case SYSML2_TOKEN_UNRESTRICTED_NAME: return "UNRESTRICTED_NAME";
        case SYSML2_TOKEN_INTEGER: return "INTEGER";
        case SYSML2_TOKEN_REAL: return "REAL";
        case SYSML2_TOKEN_STRING: return "STRING";

        /* Keywords */
        case SYSML2_TOKEN_KW_ABOUT: return "about";
        case SYSML2_TOKEN_KW_ABSTRACT: return "abstract";
        case SYSML2_TOKEN_KW_ALIAS: return "alias";
        case SYSML2_TOKEN_KW_ALL: return "all";
        case SYSML2_TOKEN_KW_AND: return "and";
        case SYSML2_TOKEN_KW_AS: return "as";
        case SYSML2_TOKEN_KW_ASSOC: return "assoc";
        case SYSML2_TOKEN_KW_BEHAVIOR: return "behavior";
        case SYSML2_TOKEN_KW_BINDING: return "binding";
        case SYSML2_TOKEN_KW_BOOL: return "bool";
        case SYSML2_TOKEN_KW_BY: return "by";
        case SYSML2_TOKEN_KW_CHAINS: return "chains";
        case SYSML2_TOKEN_KW_CLASS: return "class";
        case SYSML2_TOKEN_KW_CLASSIFIER: return "classifier";
        case SYSML2_TOKEN_KW_COMMENT: return "comment";
        case SYSML2_TOKEN_KW_COMPOSITE: return "composite";
        case SYSML2_TOKEN_KW_CONJUGATE: return "conjugate";
        case SYSML2_TOKEN_KW_CONJUGATES: return "conjugates";
        case SYSML2_TOKEN_KW_CONJUGATION: return "conjugation";
        case SYSML2_TOKEN_KW_CONNECTOR: return "connector";
        case SYSML2_TOKEN_KW_DATATYPE: return "datatype";
        case SYSML2_TOKEN_KW_DEFAULT: return "default";
        case SYSML2_TOKEN_KW_DERIVED: return "derived";
        case SYSML2_TOKEN_KW_DIFFERENCES: return "differences";
        case SYSML2_TOKEN_KW_DISJOINING: return "disjoining";
        case SYSML2_TOKEN_KW_DISJOINT: return "disjoint";
        case SYSML2_TOKEN_KW_DOC: return "doc";
        case SYSML2_TOKEN_KW_ELSE: return "else";
        case SYSML2_TOKEN_KW_END: return "end";
        case SYSML2_TOKEN_KW_EXPR: return "expr";
        case SYSML2_TOKEN_KW_FALSE: return "false";
        case SYSML2_TOKEN_KW_FEATURE: return "feature";
        case SYSML2_TOKEN_KW_FEATURED: return "featured";
        case SYSML2_TOKEN_KW_FEATURING: return "featuring";
        case SYSML2_TOKEN_KW_FILTER: return "filter";
        case SYSML2_TOKEN_KW_FIRST: return "first";
        case SYSML2_TOKEN_KW_FROM: return "from";
        case SYSML2_TOKEN_KW_FUNCTION: return "function";
        case SYSML2_TOKEN_KW_HASTYPE: return "hastype";
        case SYSML2_TOKEN_KW_IF: return "if";
        case SYSML2_TOKEN_KW_IMPLIES: return "implies";
        case SYSML2_TOKEN_KW_IMPORT: return "import";
        case SYSML2_TOKEN_KW_IN: return "in";
        case SYSML2_TOKEN_KW_INOUT: return "inout";
        case SYSML2_TOKEN_KW_INTERACTION: return "interaction";
        case SYSML2_TOKEN_KW_INTERSECTS: return "intersects";
        case SYSML2_TOKEN_KW_INTERSECTING: return "intersecting";
        case SYSML2_TOKEN_KW_INV: return "inv";
        case SYSML2_TOKEN_KW_INVERSE: return "inverse";
        case SYSML2_TOKEN_KW_ISTYPE: return "istype";
        case SYSML2_TOKEN_KW_LANGUAGE: return "language";
        case SYSML2_TOKEN_KW_LIBRARY: return "library";
        case SYSML2_TOKEN_KW_LOCALE: return "locale";
        case SYSML2_TOKEN_KW_LOOP: return "loop";
        case SYSML2_TOKEN_KW_MEMBER: return "member";
        case SYSML2_TOKEN_KW_METACLASS: return "metaclass";
        case SYSML2_TOKEN_KW_METADATA: return "metadata";
        case SYSML2_TOKEN_KW_MULTIPLICITY: return "multiplicity";
        case SYSML2_TOKEN_KW_NAMESPACE: return "namespace";
        case SYSML2_TOKEN_KW_NONUNIQUE: return "nonunique";
        case SYSML2_TOKEN_KW_NOT: return "not";
        case SYSML2_TOKEN_KW_NULL: return "null";
        case SYSML2_TOKEN_KW_OF: return "of";
        case SYSML2_TOKEN_KW_OR: return "or";
        case SYSML2_TOKEN_KW_ORDERED: return "ordered";
        case SYSML2_TOKEN_KW_OUT: return "out";
        case SYSML2_TOKEN_KW_PACKAGE: return "package";
        case SYSML2_TOKEN_KW_PORTION: return "portion";
        case SYSML2_TOKEN_KW_PREDICATE: return "predicate";
        case SYSML2_TOKEN_KW_PRIVATE: return "private";
        case SYSML2_TOKEN_KW_PROTECTED: return "protected";
        case SYSML2_TOKEN_KW_PUBLIC: return "public";
        case SYSML2_TOKEN_KW_READONLY: return "readonly";
        case SYSML2_TOKEN_KW_REDEFINES: return "redefines";
        case SYSML2_TOKEN_KW_REDEFINITION: return "redefinition";
        case SYSML2_TOKEN_KW_REF: return "ref";
        case SYSML2_TOKEN_KW_REFERENCES: return "references";
        case SYSML2_TOKEN_KW_REP: return "rep";
        case SYSML2_TOKEN_KW_RETURN: return "return";
        case SYSML2_TOKEN_KW_SPECIALIZATION: return "specialization";
        case SYSML2_TOKEN_KW_SPECIALIZES: return "specializes";
        case SYSML2_TOKEN_KW_STEP: return "step";
        case SYSML2_TOKEN_KW_STRUCT: return "struct";
        case SYSML2_TOKEN_KW_SUBCLASSIFIER: return "subclassifier";
        case SYSML2_TOKEN_KW_SUBSET: return "subset";
        case SYSML2_TOKEN_KW_SUBSETS: return "subsets";
        case SYSML2_TOKEN_KW_SUBTYPE: return "subtype";
        case SYSML2_TOKEN_KW_SUCCESSION: return "succession";
        case SYSML2_TOKEN_KW_THEN: return "then";
        case SYSML2_TOKEN_KW_TO: return "to";
        case SYSML2_TOKEN_KW_TRUE: return "true";
        case SYSML2_TOKEN_KW_TYPE: return "type";
        case SYSML2_TOKEN_KW_TYPED: return "typed";
        case SYSML2_TOKEN_KW_TYPING: return "typing";
        case SYSML2_TOKEN_KW_UNIONS: return "unions";
        case SYSML2_TOKEN_KW_UNIONING: return "unioning";
        case SYSML2_TOKEN_KW_XOR: return "xor";

        /* SysML keywords */
        case SYSML2_TOKEN_KW_ACCEPT: return "accept";
        case SYSML2_TOKEN_KW_ACTION: return "action";
        case SYSML2_TOKEN_KW_ACTOR: return "actor";
        case SYSML2_TOKEN_KW_AFTER: return "after";
        case SYSML2_TOKEN_KW_ALLOCATION: return "allocation";
        case SYSML2_TOKEN_KW_ANALYSIS: return "analysis";
        case SYSML2_TOKEN_KW_ASSERT: return "assert";
        case SYSML2_TOKEN_KW_ASSIGN: return "assign";
        case SYSML2_TOKEN_KW_ASSUMPTION: return "assumption";
        case SYSML2_TOKEN_KW_AT: return "at";
        case SYSML2_TOKEN_KW_ATTRIBUTE: return "attribute";
        case SYSML2_TOKEN_KW_CALC: return "calc";
        case SYSML2_TOKEN_KW_CASE: return "case";
        case SYSML2_TOKEN_KW_CONCERN: return "concern";
        case SYSML2_TOKEN_KW_CONNECT: return "connect";
        case SYSML2_TOKEN_KW_CONNECTION: return "connection";
        case SYSML2_TOKEN_KW_CONSTRAINT: return "constraint";
        case SYSML2_TOKEN_KW_DECIDE: return "decide";
        case SYSML2_TOKEN_KW_DEF: return "def";
        case SYSML2_TOKEN_KW_DEPENDENCY: return "dependency";
        case SYSML2_TOKEN_KW_DO: return "do";
        case SYSML2_TOKEN_KW_ENTRY: return "entry";
        case SYSML2_TOKEN_KW_ENUM: return "enum";
        case SYSML2_TOKEN_KW_EVENT: return "event";
        case SYSML2_TOKEN_KW_EXHIBIT: return "exhibit";
        case SYSML2_TOKEN_KW_EXIT: return "exit";
        case SYSML2_TOKEN_KW_EXPOSE: return "expose";
        case SYSML2_TOKEN_KW_FLOW: return "flow";
        case SYSML2_TOKEN_KW_FOR: return "for";
        case SYSML2_TOKEN_KW_FORK: return "fork";
        case SYSML2_TOKEN_KW_FRAME: return "frame";
        case SYSML2_TOKEN_KW_INCLUDE: return "include";
        case SYSML2_TOKEN_KW_INDIVIDUAL: return "individual";
        case SYSML2_TOKEN_KW_INTERFACE: return "interface";
        case SYSML2_TOKEN_KW_ITEM: return "item";
        case SYSML2_TOKEN_KW_JOIN: return "join";
        case SYSML2_TOKEN_KW_MERGE: return "merge";
        case SYSML2_TOKEN_KW_MESSAGE: return "message";
        case SYSML2_TOKEN_KW_OBJECTIVE: return "objective";
        case SYSML2_TOKEN_KW_OCCURRENCE: return "occurrence";
        case SYSML2_TOKEN_KW_PARALLEL: return "parallel";
        case SYSML2_TOKEN_KW_PART: return "part";
        case SYSML2_TOKEN_KW_PERFORM: return "perform";
        case SYSML2_TOKEN_KW_PORT: return "port";
        case SYSML2_TOKEN_KW_RECEIVE: return "receive";
        case SYSML2_TOKEN_KW_RENDERING: return "rendering";
        case SYSML2_TOKEN_KW_REQ: return "req";
        case SYSML2_TOKEN_KW_REQUIRE: return "require";
        case SYSML2_TOKEN_KW_REQUIREMENT: return "requirement";
        case SYSML2_TOKEN_KW_SATISFY: return "satisfy";
        case SYSML2_TOKEN_KW_SEND: return "send";
        case SYSML2_TOKEN_KW_SNAPSHOT: return "snapshot";
        case SYSML2_TOKEN_KW_STAKEHOLDER: return "stakeholder";
        case SYSML2_TOKEN_KW_STATE: return "state";
        case SYSML2_TOKEN_KW_SUBJECT: return "subject";
        case SYSML2_TOKEN_KW_TIMESLICE: return "timeslice";
        case SYSML2_TOKEN_KW_TRANSITION: return "transition";
        case SYSML2_TOKEN_KW_USE: return "use";
        case SYSML2_TOKEN_KW_VARIANT: return "variant";
        case SYSML2_TOKEN_KW_VERIFICATION: return "verification";
        case SYSML2_TOKEN_KW_VERIFY: return "verify";
        case SYSML2_TOKEN_KW_VIA: return "via";
        case SYSML2_TOKEN_KW_VIEW: return "view";
        case SYSML2_TOKEN_KW_VIEWPOINT: return "viewpoint";
        case SYSML2_TOKEN_KW_WHEN: return "when";
        case SYSML2_TOKEN_KW_WHILE: return "while";

        /* Operators */
        case SYSML2_TOKEN_LBRACE: return "{";
        case SYSML2_TOKEN_RBRACE: return "}";
        case SYSML2_TOKEN_LBRACKET: return "[";
        case SYSML2_TOKEN_RBRACKET: return "]";
        case SYSML2_TOKEN_LPAREN: return "(";
        case SYSML2_TOKEN_RPAREN: return ")";
        case SYSML2_TOKEN_SEMICOLON: return ";";
        case SYSML2_TOKEN_COMMA: return ",";
        case SYSML2_TOKEN_DOT: return ".";
        case SYSML2_TOKEN_COLON: return ":";
        case SYSML2_TOKEN_COLON_GT: return ":>";
        case SYSML2_TOKEN_COLON_COLON: return "::";
        case SYSML2_TOKEN_COLON_COLON_GT: return "::>";
        case SYSML2_TOKEN_COLON_GT_GT: return ":>>";
        case SYSML2_TOKEN_TILDE: return "~";
        case SYSML2_TOKEN_DOT_DOT: return "..";
        case SYSML2_TOKEN_ARROW: return "->";
        case SYSML2_TOKEN_AT: return "@";
        case SYSML2_TOKEN_HASH: return "#";
        case SYSML2_TOKEN_QUESTION: return "?";
        case SYSML2_TOKEN_PLUS: return "+";
        case SYSML2_TOKEN_MINUS: return "-";
        case SYSML2_TOKEN_STAR: return "*";
        case SYSML2_TOKEN_SLASH: return "/";
        case SYSML2_TOKEN_PERCENT: return "%";
        case SYSML2_TOKEN_STAR_STAR: return "**";
        case SYSML2_TOKEN_EQ: return "=";
        case SYSML2_TOKEN_EQ_EQ: return "==";
        case SYSML2_TOKEN_BANG_EQ: return "!=";
        case SYSML2_TOKEN_EQ_EQ_EQ: return "===";
        case SYSML2_TOKEN_BANG_EQ_EQ: return "!==";
        case SYSML2_TOKEN_LT: return "<";
        case SYSML2_TOKEN_GT: return ">";
        case SYSML2_TOKEN_LT_EQ: return "<=";
        case SYSML2_TOKEN_GT_EQ: return ">=";
        case SYSML2_TOKEN_AMP: return "&";
        case SYSML2_TOKEN_PIPE: return "|";
        case SYSML2_TOKEN_BANG: return "!";
        case SYSML2_TOKEN_CARET: return "^";
        case SYSML2_TOKEN_DOT_DOT_DOT: return "...";

        default: return "UNKNOWN";
    }
}

bool sysml2_token_is_keyword(Sysml2TokenType type) {
    return type >= SYSML2_TOKEN_KW_ABOUT && type <= SYSML2_TOKEN_KW_WHILE;
}

bool sysml2_token_is_literal(Sysml2TokenType type) {
    return type >= SYSML2_TOKEN_INTEGER && type <= SYSML2_TOKEN_STRING;
}

bool sysml2_token_is_operator(Sysml2TokenType type) {
    return type >= SYSML2_TOKEN_LBRACE && type < SYSML2_TOKEN_COUNT;
}
