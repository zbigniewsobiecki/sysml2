/*
 * SysML v2 Parser - Expression Parsing (Pratt Parser)
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/parser.h"
#include <stdlib.h>
#include <string.h>

/* Precedence levels (higher = binds tighter) */
typedef enum {
    PREC_NONE = 0,
    PREC_OR,            /* or, xor */
    PREC_AND,           /* and */
    PREC_IMPLIES,       /* implies */
    PREC_EQUALITY,      /* ==, !=, ===, !== */
    PREC_COMPARISON,    /* <, >, <=, >= */
    PREC_TERM,          /* +, - */
    PREC_FACTOR,        /* *, /, % */
    PREC_POWER,         /* ** */
    PREC_UNARY,         /* !, -, + */
    PREC_CALL,          /* (), [], . */
    PREC_PRIMARY,
} Precedence;

/* Forward declarations */
static Sysml2AstExpr *parse_expression(Sysml2Parser *parser, Precedence min_prec);
static Sysml2AstExpr *parse_primary(Sysml2Parser *parser);
static Sysml2AstExpr *parse_prefix(Sysml2Parser *parser);
static Sysml2AstExpr *parse_infix(Sysml2Parser *parser, Sysml2AstExpr *left, Precedence prec);

/* Get precedence for a binary operator token */
static Precedence get_precedence(Sysml2TokenType type) {
    switch (type) {
        case SYSML2_TOKEN_KW_OR:
        case SYSML2_TOKEN_KW_XOR:
        case SYSML2_TOKEN_PIPE:
            return PREC_OR;

        case SYSML2_TOKEN_KW_AND:
        case SYSML2_TOKEN_AMP:
            return PREC_AND;

        case SYSML2_TOKEN_KW_IMPLIES:
            return PREC_IMPLIES;

        case SYSML2_TOKEN_EQ_EQ:
        case SYSML2_TOKEN_BANG_EQ:
        case SYSML2_TOKEN_EQ_EQ_EQ:
        case SYSML2_TOKEN_BANG_EQ_EQ:
            return PREC_EQUALITY;

        case SYSML2_TOKEN_LT:
        case SYSML2_TOKEN_GT:
        case SYSML2_TOKEN_LT_EQ:
        case SYSML2_TOKEN_GT_EQ:
            return PREC_COMPARISON;

        case SYSML2_TOKEN_PLUS:
        case SYSML2_TOKEN_MINUS:
            return PREC_TERM;

        case SYSML2_TOKEN_STAR:
        case SYSML2_TOKEN_SLASH:
        case SYSML2_TOKEN_PERCENT:
            return PREC_FACTOR;

        case SYSML2_TOKEN_STAR_STAR:
            return PREC_POWER;

        case SYSML2_TOKEN_DOT:
        case SYSML2_TOKEN_LPAREN:
        case SYSML2_TOKEN_LBRACKET:
            return PREC_CALL;

        default:
            return PREC_NONE;
    }
}

/* Get binary operator for a token */
static Sysml2BinaryOp get_binary_op(Sysml2TokenType type) {
    switch (type) {
        case SYSML2_TOKEN_PLUS: return SYSML2_BINOP_ADD;
        case SYSML2_TOKEN_MINUS: return SYSML2_BINOP_SUB;
        case SYSML2_TOKEN_STAR: return SYSML2_BINOP_MUL;
        case SYSML2_TOKEN_SLASH: return SYSML2_BINOP_DIV;
        case SYSML2_TOKEN_PERCENT: return SYSML2_BINOP_MOD;
        case SYSML2_TOKEN_STAR_STAR: return SYSML2_BINOP_POW;
        case SYSML2_TOKEN_EQ_EQ: return SYSML2_BINOP_EQ;
        case SYSML2_TOKEN_BANG_EQ: return SYSML2_BINOP_NE;
        case SYSML2_TOKEN_LT: return SYSML2_BINOP_LT;
        case SYSML2_TOKEN_GT: return SYSML2_BINOP_GT;
        case SYSML2_TOKEN_LT_EQ: return SYSML2_BINOP_LE;
        case SYSML2_TOKEN_GT_EQ: return SYSML2_BINOP_GE;
        case SYSML2_TOKEN_KW_AND:
        case SYSML2_TOKEN_AMP: return SYSML2_BINOP_AND;
        case SYSML2_TOKEN_KW_OR:
        case SYSML2_TOKEN_PIPE: return SYSML2_BINOP_OR;
        case SYSML2_TOKEN_KW_XOR:
        case SYSML2_TOKEN_CARET: return SYSML2_BINOP_XOR;
        case SYSML2_TOKEN_KW_IMPLIES: return SYSML2_BINOP_IMPLIES;
        case SYSML2_TOKEN_EQ_EQ_EQ: return SYSML2_BINOP_META_EQ;
        case SYSML2_TOKEN_BANG_EQ_EQ: return SYSML2_BINOP_META_NE;
        default: return SYSML2_BINOP_ADD; /* Should not happen */
    }
}

/* Helper macros */
#define CURRENT(p) ((p)->current)
#define PREVIOUS(p) ((p)->previous)
#define CHECK(p, tok_type) ((p)->current.type == (tok_type))
#define IS_AT_END(p) ((p)->current.type == SYSML2_TOKEN_EOF)

static void advance(Sysml2Parser *parser) {
    parser->previous = parser->current;
    parser->current = sysml2_lexer_next(parser->lexer);
}

static bool match(Sysml2Parser *parser, Sysml2TokenType type) {
    if (!CHECK(parser, type)) return false;
    advance(parser);
    return true;
}

static bool consume(Sysml2Parser *parser, Sysml2TokenType type, const char *message) {
    if (CHECK(parser, type)) {
        advance(parser);
        return true;
    }

    Sysml2Diagnostic *diag = sysml2_diag_create(
        parser->diag,
        SYSML2_DIAG_E2006_UNEXPECTED_TOKEN,
        SYSML2_SEVERITY_ERROR,
        parser->lexer->source,
        CURRENT(parser).range,
        message
    );
    sysml2_diag_emit(parser->diag, diag);
    parser->had_error = true;
    return false;
}

/* Parse integer literal */
static int64_t parse_integer(const char *str, size_t len) {
    int64_t value = 0;
    bool negative = false;
    size_t i = 0;

    if (i < len && str[i] == '-') {
        negative = true;
        i++;
    }

    /* Hex */
    if (i + 1 < len && str[i] == '0' && (str[i+1] == 'x' || str[i+1] == 'X')) {
        i += 2;
        while (i < len) {
            char c = str[i];
            int digit;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = 10 + c - 'a';
            else if (c >= 'A' && c <= 'F') digit = 10 + c - 'A';
            else break;
            value = value * 16 + digit;
            i++;
        }
    } else {
        while (i < len && str[i] >= '0' && str[i] <= '9') {
            value = value * 10 + (str[i] - '0');
            i++;
        }
    }

    return negative ? -value : value;
}

/* Parse real literal */
static double parse_real(const char *str, size_t len) {
    char buf[64];
    size_t copy_len = len < 63 ? len : 63;
    memcpy(buf, str, copy_len);
    buf[copy_len] = '\0';
    return strtod(buf, NULL);
}

/* Parse a primary expression */
static Sysml2AstExpr *parse_primary(Sysml2Parser *parser) {
    Sysml2SourceRange range = CURRENT(parser).range;

    /* Integer literal */
    if (match(parser, SYSML2_TOKEN_INTEGER)) {
        int64_t value = parse_integer(PREVIOUS(parser).text.data, PREVIOUS(parser).text.length);
        return sysml2_ast_expr_int(parser->arena, value, PREVIOUS(parser).range);
    }

    /* Real literal */
    if (match(parser, SYSML2_TOKEN_REAL)) {
        double value = parse_real(PREVIOUS(parser).text.data, PREVIOUS(parser).text.length);
        return sysml2_ast_expr_real(parser->arena, value, PREVIOUS(parser).range);
    }

    /* String literal */
    if (match(parser, SYSML2_TOKEN_STRING)) {
        /* Strip quotes */
        const char *str = sysml2_intern_n(
            parser->lexer->intern,
            PREVIOUS(parser).text.data + 1,
            PREVIOUS(parser).text.length - 2
        );
        return sysml2_ast_expr_string(parser->arena, str, PREVIOUS(parser).range);
    }

    /* Boolean literals */
    if (match(parser, SYSML2_TOKEN_KW_TRUE)) {
        return sysml2_ast_expr_bool(parser->arena, true, PREVIOUS(parser).range);
    }
    if (match(parser, SYSML2_TOKEN_KW_FALSE)) {
        return sysml2_ast_expr_bool(parser->arena, false, PREVIOUS(parser).range);
    }

    /* Null literal */
    if (match(parser, SYSML2_TOKEN_KW_NULL)) {
        return sysml2_ast_expr_null(parser->arena, PREVIOUS(parser).range);
    }

    /* Identifier / qualified name */
    if (CHECK(parser, SYSML2_TOKEN_IDENTIFIER) ||
        CHECK(parser, SYSML2_TOKEN_UNRESTRICTED_NAME) ||
        CHECK(parser, SYSML2_TOKEN_COLON_COLON)) {

        /* Build qualified name */
        bool is_global = match(parser, SYSML2_TOKEN_COLON_COLON);
        const char *segments[64];
        size_t count = 0;

        do {
            if (!CHECK(parser, SYSML2_TOKEN_IDENTIFIER) &&
                !CHECK(parser, SYSML2_TOKEN_UNRESTRICTED_NAME)) {
                break;
            }
            advance(parser);

            const char *name;
            if (PREVIOUS(parser).type == SYSML2_TOKEN_UNRESTRICTED_NAME) {
                name = sysml2_intern_n(
                    parser->lexer->intern,
                    PREVIOUS(parser).text.data + 1,
                    PREVIOUS(parser).text.length - 2
                );
            } else {
                name = sysml2_intern_n(
                    parser->lexer->intern,
                    PREVIOUS(parser).text.data,
                    PREVIOUS(parser).text.length
                );
            }

            if (count < 64) {
                segments[count++] = name;
            }
        } while (match(parser, SYSML2_TOKEN_COLON_COLON));

        if (count == 0) {
            Sysml2Diagnostic *diag = sysml2_diag_create(
                parser->diag,
                SYSML2_DIAG_E2002_EXPECTED_IDENTIFIER,
                SYSML2_SEVERITY_ERROR,
                parser->lexer->source,
                range,
                "expected identifier"
            );
            sysml2_diag_emit(parser->diag, diag);
            parser->had_error = true;
            return NULL;
        }

        Sysml2AstQualifiedName *qname = sysml2_ast_qname_create(
            parser->arena,
            segments,
            count,
            is_global
        );
        qname->range = sysml2_range_from_locs(range.start, PREVIOUS(parser).range.end);

        return sysml2_ast_expr_name(parser->arena, qname);
    }

    /* Parenthesized expression */
    if (match(parser, SYSML2_TOKEN_LPAREN)) {
        Sysml2AstExpr *expr = parse_expression(parser, PREC_NONE);
        consume(parser, SYSML2_TOKEN_RPAREN, "expected ')' after expression");
        return expr;
    }

    /* Conditional expression: if condition then value else value */
    if (match(parser, SYSML2_TOKEN_KW_IF)) {
        Sysml2AstExpr *condition = parse_expression(parser, PREC_NONE);

        if (!consume(parser, SYSML2_TOKEN_KW_THEN, "expected 'then' after condition")) {
            return NULL;
        }

        Sysml2AstExpr *then_expr = parse_expression(parser, PREC_NONE);

        Sysml2AstExpr *else_expr = NULL;
        if (match(parser, SYSML2_TOKEN_KW_ELSE)) {
            else_expr = parse_expression(parser, PREC_NONE);
        }

        Sysml2AstExpr *expr = SYSML2_ARENA_NEW(parser->arena, Sysml2AstExpr);
        expr->kind = SYSML2_EXPR_CONDITIONAL;
        expr->range = sysml2_range_from_locs(range.start, PREVIOUS(parser).range.end);
        expr->conditional.condition = condition;
        expr->conditional.then_expr = then_expr;
        expr->conditional.else_expr = else_expr;
        return expr;
    }

    /* No valid primary expression */
    Sysml2Diagnostic *diag = sysml2_diag_create(
        parser->diag,
        SYSML2_DIAG_E2007_EXPECTED_EXPRESSION,
        SYSML2_SEVERITY_ERROR,
        parser->lexer->source,
        range,
        "expected expression"
    );
    sysml2_diag_emit(parser->diag, diag);
    parser->had_error = true;
    return NULL;
}

/* Parse prefix expression (unary operators) */
static Sysml2AstExpr *parse_prefix(Sysml2Parser *parser) {
    Sysml2SourceRange range = CURRENT(parser).range;

    /* Unary minus */
    if (match(parser, SYSML2_TOKEN_MINUS)) {
        Sysml2AstExpr *operand = parse_prefix(parser);
        if (!operand) return NULL;
        return sysml2_ast_expr_unary(
            parser->arena,
            SYSML2_UNOP_MINUS,
            operand,
            sysml2_range_from_locs(range.start, operand->range.end)
        );
    }

    /* Unary plus */
    if (match(parser, SYSML2_TOKEN_PLUS)) {
        Sysml2AstExpr *operand = parse_prefix(parser);
        if (!operand) return NULL;
        return sysml2_ast_expr_unary(
            parser->arena,
            SYSML2_UNOP_PLUS,
            operand,
            sysml2_range_from_locs(range.start, operand->range.end)
        );
    }

    /* Logical not */
    if (match(parser, SYSML2_TOKEN_BANG) || match(parser, SYSML2_TOKEN_KW_NOT)) {
        Sysml2AstExpr *operand = parse_prefix(parser);
        if (!operand) return NULL;
        return sysml2_ast_expr_unary(
            parser->arena,
            SYSML2_UNOP_NOT,
            operand,
            sysml2_range_from_locs(range.start, operand->range.end)
        );
    }

    return parse_primary(parser);
}

/* Parse infix expression (binary operators, calls, etc.) */
static Sysml2AstExpr *parse_infix(Sysml2Parser *parser, Sysml2AstExpr *left, Precedence prec) {
    Sysml2SourceLoc start = left->range.start;

    /* Member access: a.b */
    if (prec == PREC_CALL && match(parser, SYSML2_TOKEN_DOT)) {
        if (!CHECK(parser, SYSML2_TOKEN_IDENTIFIER) &&
            !CHECK(parser, SYSML2_TOKEN_UNRESTRICTED_NAME)) {
            Sysml2Diagnostic *diag = sysml2_diag_create(
                parser->diag,
                SYSML2_DIAG_E2002_EXPECTED_IDENTIFIER,
                SYSML2_SEVERITY_ERROR,
                parser->lexer->source,
                CURRENT(parser).range,
                "expected member name after '.'"
            );
            sysml2_diag_emit(parser->diag, diag);
            parser->had_error = true;
            return left;
        }

        advance(parser);
        const char *member;
        if (PREVIOUS(parser).type == SYSML2_TOKEN_UNRESTRICTED_NAME) {
            member = sysml2_intern_n(
                parser->lexer->intern,
                PREVIOUS(parser).text.data + 1,
                PREVIOUS(parser).text.length - 2
            );
        } else {
            member = sysml2_intern_n(
                parser->lexer->intern,
                PREVIOUS(parser).text.data,
                PREVIOUS(parser).text.length
            );
        }

        Sysml2AstExpr *expr = SYSML2_ARENA_NEW(parser->arena, Sysml2AstExpr);
        expr->kind = SYSML2_EXPR_FEATURE_CHAIN;
        expr->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        expr->chain.base = left;
        expr->chain.member = member;
        return expr;
    }

    /* Function call: f(a, b) */
    if (prec == PREC_CALL && match(parser, SYSML2_TOKEN_LPAREN)) {
        Sysml2AstExpr *args[64];
        size_t arg_count = 0;

        if (!CHECK(parser, SYSML2_TOKEN_RPAREN)) {
            do {
                if (arg_count >= 64) break;
                args[arg_count] = parse_expression(parser, PREC_NONE);
                if (args[arg_count]) arg_count++;
            } while (match(parser, SYSML2_TOKEN_COMMA));
        }

        consume(parser, SYSML2_TOKEN_RPAREN, "expected ')' after arguments");

        Sysml2AstExpr *expr = SYSML2_ARENA_NEW(parser->arena, Sysml2AstExpr);
        expr->kind = SYSML2_EXPR_INVOCATION;
        expr->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        expr->invocation.target = left;
        expr->invocation.arguments = SYSML2_ARENA_NEW_ARRAY(parser->arena, Sysml2AstExpr *, arg_count);
        for (size_t i = 0; i < arg_count; i++) {
            expr->invocation.arguments[i] = args[i];
        }
        expr->invocation.argument_count = arg_count;
        return expr;
    }

    /* Index/bracket expression: a[i] */
    if (prec == PREC_CALL && match(parser, SYSML2_TOKEN_LBRACKET)) {
        Sysml2AstExpr *index = parse_expression(parser, PREC_NONE);
        consume(parser, SYSML2_TOKEN_RBRACKET, "expected ']' after index");

        Sysml2AstExpr *expr = SYSML2_ARENA_NEW(parser->arena, Sysml2AstExpr);
        expr->kind = SYSML2_EXPR_BRACKET;
        expr->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        expr->bracket_expr = index;
        /* Note: we'd need to store the base expression too */
        /* This is a simplification */
        return expr;
    }

    /* Binary operators */
    Sysml2TokenType op_token = CURRENT(parser).type;
    advance(parser);

    /* Right associativity for power operator */
    Precedence next_prec = (op_token == SYSML2_TOKEN_STAR_STAR) ? prec : prec + 1;

    Sysml2AstExpr *right = parse_expression(parser, next_prec);
    if (!right) return left;

    return sysml2_ast_expr_binary(
        parser->arena,
        get_binary_op(op_token),
        left,
        right,
        sysml2_range_from_locs(start, right->range.end)
    );
}

/* Main expression parsing (Pratt parser) */
static Sysml2AstExpr *parse_expression(Sysml2Parser *parser, Precedence min_prec) {
    Sysml2AstExpr *left = parse_prefix(parser);
    if (!left) return NULL;

    while (!IS_AT_END(parser)) {
        Precedence prec = get_precedence(CURRENT(parser).type);
        if (prec <= min_prec) break;

        left = parse_infix(parser, left, prec);
        if (!left) return NULL;
    }

    return left;
}

/* Public API */
Sysml2AstExpr *sysml2_parser_parse_expression(Sysml2Parser *parser) {
    return parse_expression(parser, PREC_NONE);
}
