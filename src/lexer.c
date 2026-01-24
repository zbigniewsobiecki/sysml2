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
        } else if (c == '/' && peek_next(lexer) == '/') {
            /* Single-line comment */
            while (!is_eof(lexer) && peek(lexer) != '\n') {
                advance(lexer);
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

/* Token type to string lookup table */
static const char *token_strings[] = {
    [SYSML2_TOKEN_EOF] = "EOF",
    [SYSML2_TOKEN_ERROR] = "ERROR",
    [SYSML2_TOKEN_IDENTIFIER] = "IDENTIFIER",
    [SYSML2_TOKEN_UNRESTRICTED_NAME] = "UNRESTRICTED_NAME",
    [SYSML2_TOKEN_INTEGER] = "INTEGER",
    [SYSML2_TOKEN_REAL] = "REAL",
    [SYSML2_TOKEN_STRING] = "STRING",
    /* KerML keywords */
    [SYSML2_TOKEN_KW_ABOUT] = "about",
    [SYSML2_TOKEN_KW_ABSTRACT] = "abstract",
    [SYSML2_TOKEN_KW_ALIAS] = "alias",
    [SYSML2_TOKEN_KW_ALL] = "all",
    [SYSML2_TOKEN_KW_AND] = "and",
    [SYSML2_TOKEN_KW_AS] = "as",
    [SYSML2_TOKEN_KW_ASSOC] = "assoc",
    [SYSML2_TOKEN_KW_BEHAVIOR] = "behavior",
    [SYSML2_TOKEN_KW_BINDING] = "binding",
    [SYSML2_TOKEN_KW_BOOL] = "bool",
    [SYSML2_TOKEN_KW_BY] = "by",
    [SYSML2_TOKEN_KW_CHAINS] = "chains",
    [SYSML2_TOKEN_KW_CLASS] = "class",
    [SYSML2_TOKEN_KW_CLASSIFIER] = "classifier",
    [SYSML2_TOKEN_KW_COMMENT] = "comment",
    [SYSML2_TOKEN_KW_COMPOSITE] = "composite",
    [SYSML2_TOKEN_KW_CONJUGATE] = "conjugate",
    [SYSML2_TOKEN_KW_CONJUGATES] = "conjugates",
    [SYSML2_TOKEN_KW_CONJUGATION] = "conjugation",
    [SYSML2_TOKEN_KW_CONNECTOR] = "connector",
    [SYSML2_TOKEN_KW_DATATYPE] = "datatype",
    [SYSML2_TOKEN_KW_DEFAULT] = "default",
    [SYSML2_TOKEN_KW_DERIVED] = "derived",
    [SYSML2_TOKEN_KW_DIFFERENCES] = "differences",
    [SYSML2_TOKEN_KW_DISJOINING] = "disjoining",
    [SYSML2_TOKEN_KW_DISJOINT] = "disjoint",
    [SYSML2_TOKEN_KW_DOC] = "doc",
    [SYSML2_TOKEN_KW_ELSE] = "else",
    [SYSML2_TOKEN_KW_END] = "end",
    [SYSML2_TOKEN_KW_EXPR] = "expr",
    [SYSML2_TOKEN_KW_FALSE] = "false",
    [SYSML2_TOKEN_KW_FEATURE] = "feature",
    [SYSML2_TOKEN_KW_FEATURED] = "featured",
    [SYSML2_TOKEN_KW_FEATURING] = "featuring",
    [SYSML2_TOKEN_KW_FILTER] = "filter",
    [SYSML2_TOKEN_KW_FIRST] = "first",
    [SYSML2_TOKEN_KW_FROM] = "from",
    [SYSML2_TOKEN_KW_FUNCTION] = "function",
    [SYSML2_TOKEN_KW_HASTYPE] = "hastype",
    [SYSML2_TOKEN_KW_IF] = "if",
    [SYSML2_TOKEN_KW_IMPLIES] = "implies",
    [SYSML2_TOKEN_KW_IMPORT] = "import",
    [SYSML2_TOKEN_KW_IN] = "in",
    [SYSML2_TOKEN_KW_INOUT] = "inout",
    [SYSML2_TOKEN_KW_INTERACTION] = "interaction",
    [SYSML2_TOKEN_KW_INTERSECTS] = "intersects",
    [SYSML2_TOKEN_KW_INTERSECTING] = "intersecting",
    [SYSML2_TOKEN_KW_INV] = "inv",
    [SYSML2_TOKEN_KW_INVERSE] = "inverse",
    [SYSML2_TOKEN_KW_ISTYPE] = "istype",
    [SYSML2_TOKEN_KW_LANGUAGE] = "language",
    [SYSML2_TOKEN_KW_LIBRARY] = "library",
    [SYSML2_TOKEN_KW_LOCALE] = "locale",
    [SYSML2_TOKEN_KW_LOOP] = "loop",
    [SYSML2_TOKEN_KW_MEMBER] = "member",
    [SYSML2_TOKEN_KW_METACLASS] = "metaclass",
    [SYSML2_TOKEN_KW_METADATA] = "metadata",
    [SYSML2_TOKEN_KW_MULTIPLICITY] = "multiplicity",
    [SYSML2_TOKEN_KW_NAMESPACE] = "namespace",
    [SYSML2_TOKEN_KW_NONUNIQUE] = "nonunique",
    [SYSML2_TOKEN_KW_NOT] = "not",
    [SYSML2_TOKEN_KW_NULL] = "null",
    [SYSML2_TOKEN_KW_OF] = "of",
    [SYSML2_TOKEN_KW_OR] = "or",
    [SYSML2_TOKEN_KW_ORDERED] = "ordered",
    [SYSML2_TOKEN_KW_OUT] = "out",
    [SYSML2_TOKEN_KW_PACKAGE] = "package",
    [SYSML2_TOKEN_KW_PORTION] = "portion",
    [SYSML2_TOKEN_KW_PREDICATE] = "predicate",
    [SYSML2_TOKEN_KW_PRIVATE] = "private",
    [SYSML2_TOKEN_KW_PROTECTED] = "protected",
    [SYSML2_TOKEN_KW_PUBLIC] = "public",
    [SYSML2_TOKEN_KW_READONLY] = "readonly",
    [SYSML2_TOKEN_KW_REDEFINES] = "redefines",
    [SYSML2_TOKEN_KW_REDEFINITION] = "redefinition",
    [SYSML2_TOKEN_KW_REF] = "ref",
    [SYSML2_TOKEN_KW_REFERENCES] = "references",
    [SYSML2_TOKEN_KW_REP] = "rep",
    [SYSML2_TOKEN_KW_RETURN] = "return",
    [SYSML2_TOKEN_KW_SPECIALIZATION] = "specialization",
    [SYSML2_TOKEN_KW_SPECIALIZES] = "specializes",
    [SYSML2_TOKEN_KW_STEP] = "step",
    [SYSML2_TOKEN_KW_STRUCT] = "struct",
    [SYSML2_TOKEN_KW_SUBCLASSIFIER] = "subclassifier",
    [SYSML2_TOKEN_KW_SUBSET] = "subset",
    [SYSML2_TOKEN_KW_SUBSETS] = "subsets",
    [SYSML2_TOKEN_KW_SUBTYPE] = "subtype",
    [SYSML2_TOKEN_KW_SUCCESSION] = "succession",
    [SYSML2_TOKEN_KW_THEN] = "then",
    [SYSML2_TOKEN_KW_TO] = "to",
    [SYSML2_TOKEN_KW_TRUE] = "true",
    [SYSML2_TOKEN_KW_TYPE] = "type",
    [SYSML2_TOKEN_KW_TYPED] = "typed",
    [SYSML2_TOKEN_KW_TYPING] = "typing",
    [SYSML2_TOKEN_KW_UNIONS] = "unions",
    [SYSML2_TOKEN_KW_UNIONING] = "unioning",
    [SYSML2_TOKEN_KW_XOR] = "xor",
    /* SysML keywords */
    [SYSML2_TOKEN_KW_ACCEPT] = "accept",
    [SYSML2_TOKEN_KW_ACTION] = "action",
    [SYSML2_TOKEN_KW_ACTOR] = "actor",
    [SYSML2_TOKEN_KW_AFTER] = "after",
    [SYSML2_TOKEN_KW_ALLOCATION] = "allocation",
    [SYSML2_TOKEN_KW_ANALYSIS] = "analysis",
    [SYSML2_TOKEN_KW_ASSERT] = "assert",
    [SYSML2_TOKEN_KW_ASSIGN] = "assign",
    [SYSML2_TOKEN_KW_ASSUMPTION] = "assumption",
    [SYSML2_TOKEN_KW_AT] = "at",
    [SYSML2_TOKEN_KW_ATTRIBUTE] = "attribute",
    [SYSML2_TOKEN_KW_CALC] = "calc",
    [SYSML2_TOKEN_KW_CASE] = "case",
    [SYSML2_TOKEN_KW_CONCERN] = "concern",
    [SYSML2_TOKEN_KW_CONNECT] = "connect",
    [SYSML2_TOKEN_KW_CONNECTION] = "connection",
    [SYSML2_TOKEN_KW_CONSTRAINT] = "constraint",
    [SYSML2_TOKEN_KW_DECIDE] = "decide",
    [SYSML2_TOKEN_KW_DEF] = "def",
    [SYSML2_TOKEN_KW_DEPENDENCY] = "dependency",
    [SYSML2_TOKEN_KW_DO] = "do",
    [SYSML2_TOKEN_KW_ENTRY] = "entry",
    [SYSML2_TOKEN_KW_ENUM] = "enum",
    [SYSML2_TOKEN_KW_EVENT] = "event",
    [SYSML2_TOKEN_KW_EXHIBIT] = "exhibit",
    [SYSML2_TOKEN_KW_EXIT] = "exit",
    [SYSML2_TOKEN_KW_EXPOSE] = "expose",
    [SYSML2_TOKEN_KW_FLOW] = "flow",
    [SYSML2_TOKEN_KW_FOR] = "for",
    [SYSML2_TOKEN_KW_FORK] = "fork",
    [SYSML2_TOKEN_KW_FRAME] = "frame",
    [SYSML2_TOKEN_KW_INCLUDE] = "include",
    [SYSML2_TOKEN_KW_INDIVIDUAL] = "individual",
    [SYSML2_TOKEN_KW_INTERFACE] = "interface",
    [SYSML2_TOKEN_KW_ITEM] = "item",
    [SYSML2_TOKEN_KW_JOIN] = "join",
    [SYSML2_TOKEN_KW_MERGE] = "merge",
    [SYSML2_TOKEN_KW_MESSAGE] = "message",
    [SYSML2_TOKEN_KW_OBJECTIVE] = "objective",
    [SYSML2_TOKEN_KW_OCCURRENCE] = "occurrence",
    [SYSML2_TOKEN_KW_PARALLEL] = "parallel",
    [SYSML2_TOKEN_KW_PART] = "part",
    [SYSML2_TOKEN_KW_PERFORM] = "perform",
    [SYSML2_TOKEN_KW_PORT] = "port",
    [SYSML2_TOKEN_KW_RECEIVE] = "receive",
    [SYSML2_TOKEN_KW_RENDERING] = "rendering",
    [SYSML2_TOKEN_KW_REQ] = "req",
    [SYSML2_TOKEN_KW_REQUIRE] = "require",
    [SYSML2_TOKEN_KW_REQUIREMENT] = "requirement",
    [SYSML2_TOKEN_KW_SATISFY] = "satisfy",
    [SYSML2_TOKEN_KW_SEND] = "send",
    [SYSML2_TOKEN_KW_SNAPSHOT] = "snapshot",
    [SYSML2_TOKEN_KW_STAKEHOLDER] = "stakeholder",
    [SYSML2_TOKEN_KW_STANDARD] = "standard",
    [SYSML2_TOKEN_KW_STATE] = "state",
    [SYSML2_TOKEN_KW_SUBJECT] = "subject",
    [SYSML2_TOKEN_KW_TIMESLICE] = "timeslice",
    [SYSML2_TOKEN_KW_TRANSITION] = "transition",
    [SYSML2_TOKEN_KW_USE] = "use",
    [SYSML2_TOKEN_KW_VARIANT] = "variant",
    [SYSML2_TOKEN_KW_VERIFICATION] = "verification",
    [SYSML2_TOKEN_KW_VERIFY] = "verify",
    [SYSML2_TOKEN_KW_VIA] = "via",
    [SYSML2_TOKEN_KW_VIEW] = "view",
    [SYSML2_TOKEN_KW_VIEWPOINT] = "viewpoint",
    [SYSML2_TOKEN_KW_WHEN] = "when",
    [SYSML2_TOKEN_KW_WHILE] = "while",
    [SYSML2_TOKEN_KW_BIND] = "bind",
    [SYSML2_TOKEN_KW_TERMINATE] = "terminate",
    [SYSML2_TOKEN_KW_UNTIL] = "until",
    [SYSML2_TOKEN_KW_DONE] = "done",
    [SYSML2_TOKEN_KW_RENDER] = "render",
    [SYSML2_TOKEN_KW_ASSUME] = "assume",
    [SYSML2_TOKEN_KW_ALLOCATE] = "allocate",
    [SYSML2_TOKEN_KW_NEW] = "new",
    /* Operators */
    [SYSML2_TOKEN_LBRACE] = "{",
    [SYSML2_TOKEN_RBRACE] = "}",
    [SYSML2_TOKEN_LBRACKET] = "[",
    [SYSML2_TOKEN_RBRACKET] = "]",
    [SYSML2_TOKEN_LPAREN] = "(",
    [SYSML2_TOKEN_RPAREN] = ")",
    [SYSML2_TOKEN_SEMICOLON] = ";",
    [SYSML2_TOKEN_COMMA] = ",",
    [SYSML2_TOKEN_DOT] = ".",
    [SYSML2_TOKEN_COLON] = ":",
    [SYSML2_TOKEN_COLON_GT] = ":>",
    [SYSML2_TOKEN_COLON_COLON] = "::",
    [SYSML2_TOKEN_COLON_COLON_GT] = "::>",
    [SYSML2_TOKEN_COLON_GT_GT] = ":>>",
    [SYSML2_TOKEN_TILDE] = "~",
    [SYSML2_TOKEN_DOT_DOT] = "..",
    [SYSML2_TOKEN_ARROW] = "->",
    [SYSML2_TOKEN_AT] = "@",
    [SYSML2_TOKEN_HASH] = "#",
    [SYSML2_TOKEN_QUESTION] = "?",
    [SYSML2_TOKEN_PLUS] = "+",
    [SYSML2_TOKEN_MINUS] = "-",
    [SYSML2_TOKEN_STAR] = "*",
    [SYSML2_TOKEN_SLASH] = "/",
    [SYSML2_TOKEN_PERCENT] = "%",
    [SYSML2_TOKEN_STAR_STAR] = "**",
    [SYSML2_TOKEN_EQ] = "=",
    [SYSML2_TOKEN_EQ_EQ] = "==",
    [SYSML2_TOKEN_BANG_EQ] = "!=",
    [SYSML2_TOKEN_EQ_EQ_EQ] = "===",
    [SYSML2_TOKEN_BANG_EQ_EQ] = "!==",
    [SYSML2_TOKEN_LT] = "<",
    [SYSML2_TOKEN_GT] = ">",
    [SYSML2_TOKEN_LT_EQ] = "<=",
    [SYSML2_TOKEN_GT_EQ] = ">=",
    [SYSML2_TOKEN_AMP] = "&",
    [SYSML2_TOKEN_PIPE] = "|",
    [SYSML2_TOKEN_BANG] = "!",
    [SYSML2_TOKEN_CARET] = "^",
    [SYSML2_TOKEN_DOT_DOT_DOT] = "...",
};

/* Token type to string */
const char *sysml2_token_type_to_string(Sysml2TokenType type) {
    if (type >= 0 && type < SYSML2_TOKEN_COUNT && token_strings[type]) {
        return token_strings[type];
    }
    return "UNKNOWN";
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
