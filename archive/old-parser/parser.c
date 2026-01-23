/*
 * SysML v2 Parser - Recursive Descent Parser Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/parser.h"
#include <string.h>
#include <stdlib.h>

/* Forward declarations for parsing functions */
static Sysml2AstMember *parse_namespace_body_element(Sysml2Parser *parser);
static Sysml2AstPackage *parse_package(Sysml2Parser *parser);
static Sysml2AstNamespace *parse_namespace(Sysml2Parser *parser);
static Sysml2AstImport *parse_import(Sysml2Parser *parser);
static Sysml2AstAlias *parse_alias(Sysml2Parser *parser);
static Sysml2AstClassifier *parse_classifier(Sysml2Parser *parser, Sysml2AstKind kind);
static Sysml2AstType *parse_type(Sysml2Parser *parser);
static Sysml2AstFeature *parse_feature(Sysml2Parser *parser);
static Sysml2AstComment *parse_comment(Sysml2Parser *parser);
static Sysml2AstQualifiedName *parse_qualified_name(Sysml2Parser *parser);
static Sysml2AstMultiplicity *parse_multiplicity(Sysml2Parser *parser);
static Sysml2TypePrefix parse_type_prefix(Sysml2Parser *parser);
static Sysml2Direction parse_direction(Sysml2Parser *parser);
static Sysml2AstRelationship *parse_relationships(Sysml2Parser *parser);
static Sysml2Visibility parse_visibility(Sysml2Parser *parser);

/* Helper macros */
#define CURRENT(p) ((p)->current)
#define PREVIOUS(p) ((p)->previous)
#define CHECK(p, tok_type) ((p)->current.type == (tok_type))
#define IS_AT_END(p) ((p)->current.type == SYSML2_TOKEN_EOF)

/* Advance to the next token */
static void advance(Sysml2Parser *parser) {
    parser->previous = parser->current;
    parser->current = sysml2_lexer_next(parser->lexer);
}

/* Check and consume a token */
static bool match(Sysml2Parser *parser, Sysml2TokenType type) {
    if (!CHECK(parser, type)) return false;
    advance(parser);
    return true;
}

/* Consume expected token or error */
static bool consume(Sysml2Parser *parser, Sysml2TokenType type, const char *message) {
    if (CHECK(parser, type)) {
        advance(parser);
        return true;
    }

    Sysml2DiagCode code;
    switch (type) {
        case SYSML2_TOKEN_SEMICOLON: code = SYSML2_DIAG_E2001_EXPECTED_SEMICOLON; break;
        case SYSML2_TOKEN_IDENTIFIER:
        case SYSML2_TOKEN_UNRESTRICTED_NAME:
            code = SYSML2_DIAG_E2002_EXPECTED_IDENTIFIER; break;
        case SYSML2_TOKEN_LBRACE: code = SYSML2_DIAG_E2003_EXPECTED_LBRACE; break;
        case SYSML2_TOKEN_RBRACE: code = SYSML2_DIAG_E2004_EXPECTED_RBRACE; break;
        case SYSML2_TOKEN_COLON: code = SYSML2_DIAG_E2005_EXPECTED_COLON; break;
        case SYSML2_TOKEN_LPAREN: code = SYSML2_DIAG_E2009_EXPECTED_LPAREN; break;
        case SYSML2_TOKEN_RPAREN: code = SYSML2_DIAG_E2010_EXPECTED_RPAREN; break;
        case SYSML2_TOKEN_LBRACKET: code = SYSML2_DIAG_E2011_EXPECTED_LBRACKET; break;
        case SYSML2_TOKEN_RBRACKET: code = SYSML2_DIAG_E2012_EXPECTED_RBRACKET; break;
        default: code = SYSML2_DIAG_E2006_UNEXPECTED_TOKEN; break;
    }

    Sysml2Diagnostic *diag = sysml2_diag_create(
        parser->diag,
        code,
        SYSML2_SEVERITY_ERROR,
        parser->lexer->source,
        CURRENT(parser).range,
        message
    );

    /* Add help suggestion */
    if (type == SYSML2_TOKEN_SEMICOLON) {
        sysml2_diag_add_help(diag, parser->diag, "add ';' to complete the declaration");
        sysml2_diag_add_fixit(diag, parser->diag, PREVIOUS(parser).range, ";");
    }

    sysml2_diag_emit(parser->diag, diag);
    parser->had_error = true;
    parser->panic_mode = true;
    return false;
}

/* Get identifier or unrestricted name text (interned) */
static const char *get_name(Sysml2Parser *parser) {
    if (PREVIOUS(parser).type == SYSML2_TOKEN_IDENTIFIER) {
        return sysml2_intern_n(
            parser->lexer->intern,
            PREVIOUS(parser).text.data,
            PREVIOUS(parser).text.length
        );
    } else if (PREVIOUS(parser).type == SYSML2_TOKEN_UNRESTRICTED_NAME) {
        /* Strip quotes */
        return sysml2_intern_n(
            parser->lexer->intern,
            PREVIOUS(parser).text.data + 1,
            PREVIOUS(parser).text.length - 2
        );
    }
    return NULL;
}

/* Check if current token could be an identifier (including unrestricted name) */
static bool is_identifier(Sysml2Parser *parser) {
    return CHECK(parser, SYSML2_TOKEN_IDENTIFIER) ||
           CHECK(parser, SYSML2_TOKEN_UNRESTRICTED_NAME);
}

/* Consume an identifier (regular or unrestricted) */
static bool consume_identifier(Sysml2Parser *parser, const char *message) {
    if (is_identifier(parser)) {
        advance(parser);
        return true;
    }
    return consume(parser, SYSML2_TOKEN_IDENTIFIER, message);
}

void sysml2_parser_init(
    Sysml2Parser *parser,
    Sysml2Lexer *lexer,
    Sysml2Arena *arena,
    Sysml2DiagContext *diag
) {
    parser->lexer = lexer;
    parser->arena = arena;
    parser->diag = diag;
    parser->had_error = false;
    parser->panic_mode = false;

    /* Prime the parser with the first token */
    parser->current = sysml2_lexer_next(lexer);
    parser->previous = parser->current;
}

void sysml2_parser_synchronize(Sysml2Parser *parser, Sysml2SyncPoint sync) {
    parser->panic_mode = false;

    while (!IS_AT_END(parser)) {
        /* Always sync at statement end */
        if (PREVIOUS(parser).type == SYSML2_TOKEN_SEMICOLON) {
            return;
        }

        switch (CURRENT(parser).type) {
            /* Top-level sync points */
            case SYSML2_TOKEN_KW_NAMESPACE:
            case SYSML2_TOKEN_KW_PACKAGE:
            case SYSML2_TOKEN_KW_LIBRARY:
                if (sync <= SYSML2_SYNC_NAMESPACE) return;
                break;

            /* Type-level sync points */
            case SYSML2_TOKEN_KW_TYPE:
            case SYSML2_TOKEN_KW_CLASSIFIER:
            case SYSML2_TOKEN_KW_CLASS:
            case SYSML2_TOKEN_KW_DATATYPE:
            case SYSML2_TOKEN_KW_STRUCT:
            case SYSML2_TOKEN_KW_ASSOC:
            case SYSML2_TOKEN_KW_BEHAVIOR:
            case SYSML2_TOKEN_KW_FUNCTION:
            case SYSML2_TOKEN_KW_PREDICATE:
                if (sync <= SYSML2_SYNC_TYPE) return;
                break;

            /* Feature-level sync points */
            case SYSML2_TOKEN_KW_FEATURE:
            case SYSML2_TOKEN_KW_IN:
            case SYSML2_TOKEN_KW_OUT:
            case SYSML2_TOKEN_KW_INOUT:
            case SYSML2_TOKEN_KW_ABSTRACT:
            case SYSML2_TOKEN_KW_READONLY:
            case SYSML2_TOKEN_KW_DERIVED:
                if (sync <= SYSML2_SYNC_FEATURE) return;
                break;

            /* Block end */
            case SYSML2_TOKEN_RBRACE:
                if (sync == SYSML2_SYNC_BLOCK) {
                    advance(parser);
                    return;
                }
                break;

            default:
                break;
        }

        advance(parser);
    }
}

bool sysml2_parser_at_sync_point(Sysml2Parser *parser) {
    switch (CURRENT(parser).type) {
        case SYSML2_TOKEN_KW_NAMESPACE:
        case SYSML2_TOKEN_KW_PACKAGE:
        case SYSML2_TOKEN_KW_LIBRARY:
        case SYSML2_TOKEN_KW_TYPE:
        case SYSML2_TOKEN_KW_CLASSIFIER:
        case SYSML2_TOKEN_KW_CLASS:
        case SYSML2_TOKEN_KW_DATATYPE:
        case SYSML2_TOKEN_KW_STRUCT:
        case SYSML2_TOKEN_KW_ASSOC:
        case SYSML2_TOKEN_KW_FEATURE:
        case SYSML2_TOKEN_RBRACE:
        case SYSML2_TOKEN_SEMICOLON:
        case SYSML2_TOKEN_EOF:
            return true;
        default:
            return false;
    }
}

/* Parse visibility prefix */
static Sysml2Visibility parse_visibility(Sysml2Parser *parser) {
    if (match(parser, SYSML2_TOKEN_KW_PUBLIC)) return SYSML2_VIS_PUBLIC;
    if (match(parser, SYSML2_TOKEN_KW_PRIVATE)) return SYSML2_VIS_PRIVATE;
    if (match(parser, SYSML2_TOKEN_KW_PROTECTED)) return SYSML2_VIS_PROTECTED;
    return SYSML2_VIS_PUBLIC; /* Default */
}

/* Parse direction prefix */
static Sysml2Direction parse_direction(Sysml2Parser *parser) {
    if (match(parser, SYSML2_TOKEN_KW_IN)) return SYSML2_DIR_IN;
    if (match(parser, SYSML2_TOKEN_KW_OUT)) return SYSML2_DIR_OUT;
    if (match(parser, SYSML2_TOKEN_KW_INOUT)) return SYSML2_DIR_INOUT;
    return SYSML2_DIR_NONE;
}

/* Parse type prefix (abstract, readonly, etc.) */
static Sysml2TypePrefix parse_type_prefix(Sysml2Parser *parser) {
    Sysml2TypePrefix prefix = {0};

    while (true) {
        if (match(parser, SYSML2_TOKEN_KW_ABSTRACT)) {
            prefix.is_abstract = true;
        } else if (match(parser, SYSML2_TOKEN_KW_READONLY)) {
            prefix.is_readonly = true;
        } else if (match(parser, SYSML2_TOKEN_KW_DERIVED)) {
            prefix.is_derived = true;
        } else if (match(parser, SYSML2_TOKEN_KW_END)) {
            prefix.is_end = true;
        } else if (match(parser, SYSML2_TOKEN_KW_COMPOSITE)) {
            prefix.is_composite = true;
        } else if (match(parser, SYSML2_TOKEN_KW_PORTION)) {
            prefix.is_portion = true;
        } else if (match(parser, SYSML2_TOKEN_KW_REF)) {
            prefix.is_ref = true;
        } else {
            break;
        }
    }

    return prefix;
}

/* Parse a qualified name */
static Sysml2AstQualifiedName *parse_qualified_name(Sysml2Parser *parser) {
    Sysml2SourceLoc start = CURRENT(parser).range.start;
    bool is_global = match(parser, SYSML2_TOKEN_COLON_COLON);

    /* Collect segments */
    const char *segments[64]; /* Max nesting depth */
    size_t count = 0;

    if (!consume_identifier(parser, "expected identifier in qualified name")) {
        return NULL;
    }
    segments[count++] = get_name(parser);

    while (CHECK(parser, SYSML2_TOKEN_COLON_COLON)) {
        /* Peek ahead - if next is not an identifier, stop (for import ::*) */
        Sysml2Token next = sysml2_lexer_peek(parser->lexer);
        if (next.type != SYSML2_TOKEN_IDENTIFIER &&
            next.type != SYSML2_TOKEN_UNRESTRICTED_NAME) {
            break;
        }
        advance(parser); /* Consume :: */
        if (!consume_identifier(parser, "expected identifier after '::'")) {
            break;
        }
        if (count < 64) {
            segments[count++] = get_name(parser);
        }
    }

    /* Allocate and copy */
    Sysml2AstQualifiedName *qname = SYSML2_ARENA_NEW(parser->arena, Sysml2AstQualifiedName);
    qname->segments = SYSML2_ARENA_NEW_ARRAY(parser->arena, const char *, count);
    for (size_t i = 0; i < count; i++) {
        qname->segments[i] = segments[i];
    }
    qname->segment_count = count;
    qname->is_global = is_global;
    qname->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);

    return qname;
}

/* Parse multiplicity [lower..upper] */
static Sysml2AstMultiplicity *parse_multiplicity(Sysml2Parser *parser) {
    if (!match(parser, SYSML2_TOKEN_LBRACKET)) {
        return NULL;
    }

    Sysml2SourceLoc start = PREVIOUS(parser).range.start;
    Sysml2AstMultiplicity *mult = SYSML2_ARENA_NEW(parser->arena, Sysml2AstMultiplicity);

    /* Check for * (unbounded) */
    if (match(parser, SYSML2_TOKEN_STAR)) {
        mult->lower = NULL;
        mult->upper = NULL;
    } else if (CHECK(parser, SYSML2_TOKEN_INTEGER)) {
        /* Parse lower bound */
        mult->lower = sysml2_parser_parse_expression(parser);

        if (match(parser, SYSML2_TOKEN_DOT_DOT)) {
            /* Range */
            if (match(parser, SYSML2_TOKEN_STAR)) {
                mult->upper = NULL; /* Unbounded */
            } else if (CHECK(parser, SYSML2_TOKEN_INTEGER)) {
                mult->upper = sysml2_parser_parse_expression(parser);
            }
        } else {
            /* Exact: [n] means [n..n] */
            mult->upper = mult->lower;
        }
    }

    /* Parse modifiers */
    while (true) {
        if (match(parser, SYSML2_TOKEN_KW_ORDERED)) {
            mult->is_ordered = true;
        } else if (match(parser, SYSML2_TOKEN_KW_NONUNIQUE)) {
            mult->is_nonunique = true;
        } else {
            break;
        }
    }

    consume(parser, SYSML2_TOKEN_RBRACKET, "expected ']' after multiplicity");
    mult->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);

    return mult;
}

/* Parse relationship declarations (:, :>, :>>, ::>, ~) */
static Sysml2AstRelationship *parse_relationships(Sysml2Parser *parser) {
    Sysml2AstRelationship *first = NULL;
    Sysml2AstRelationship *last = NULL;

    while (true) {
        Sysml2RelationshipKind kind;
        bool is_conjugated = false;
        Sysml2SourceLoc start = CURRENT(parser).range.start;

        if (match(parser, SYSML2_TOKEN_COLON_GT_GT)) {
            kind = SYSML2_REL_REDEFINES;
        } else if (match(parser, SYSML2_TOKEN_COLON_GT)) {
            kind = SYSML2_REL_SPECIALIZES;
        } else if (match(parser, SYSML2_TOKEN_COLON_COLON_GT)) {
            kind = SYSML2_REL_SUBSETS;
        } else if (match(parser, SYSML2_TOKEN_COLON)) {
            kind = SYSML2_REL_TYPED_BY;
            /* Check for conjugated type: ~TypeName */
            is_conjugated = match(parser, SYSML2_TOKEN_TILDE);
        } else if (match(parser, SYSML2_TOKEN_TILDE)) {
            kind = SYSML2_REL_CONJUGATES;
        } else if (match(parser, SYSML2_TOKEN_KW_SUBSETS)) {
            kind = SYSML2_REL_SUBSETS;
        } else if (match(parser, SYSML2_TOKEN_KW_REDEFINES)) {
            kind = SYSML2_REL_REDEFINES;
        } else if (match(parser, SYSML2_TOKEN_KW_REFERENCES)) {
            kind = SYSML2_REL_REFERENCES;
        } else if (match(parser, SYSML2_TOKEN_KW_SPECIALIZES)) {
            kind = SYSML2_REL_SPECIALIZES;
        } else if (match(parser, SYSML2_TOKEN_KW_CONJUGATES)) {
            kind = SYSML2_REL_CONJUGATES;
        } else {
            break;
        }

        Sysml2AstQualifiedName *target = parse_qualified_name(parser);
        if (!target) break;

        Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
        rel->kind = kind;
        rel->target = target;
        rel->is_conjugated = is_conjugated;
        rel->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        rel->next = NULL;

        if (!first) {
            first = rel;
            last = rel;
        } else {
            last->next = rel;
            last = rel;
        }

        /* Handle multiple targets with comma */
        while (match(parser, SYSML2_TOKEN_COMMA)) {
            target = parse_qualified_name(parser);
            if (!target) break;

            Sysml2AstRelationship *next_rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
            next_rel->kind = kind;
            next_rel->target = target;
            next_rel->is_conjugated = is_conjugated;
            next_rel->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
            next_rel->next = NULL;

            last->next = next_rel;
            last = next_rel;
        }
    }

    return first;
}

/* Parse a type/namespace body { members } or ; */
static Sysml2AstMember *parse_body(Sysml2Parser *parser) {
    if (match(parser, SYSML2_TOKEN_SEMICOLON)) {
        return NULL; /* Empty body */
    }

    if (!consume(parser, SYSML2_TOKEN_LBRACE, "expected '{' or ';'")) {
        return NULL;
    }

    Sysml2AstMember *first = NULL;
    Sysml2AstMember *last = NULL;

    while (!CHECK(parser, SYSML2_TOKEN_RBRACE) && !IS_AT_END(parser)) {
        Sysml2AstMember *member = parse_namespace_body_element(parser);
        if (member) {
            if (!first) {
                first = member;
                last = member;
            } else {
                last->next = member;
                last = member;
            }
        } else if (parser->panic_mode) {
            sysml2_parser_synchronize(parser, SYSML2_SYNC_TYPE);
        }
    }

    consume(parser, SYSML2_TOKEN_RBRACE, "expected '}' to close body");
    return first;
}

/* Parse import statement */
static Sysml2AstImport *parse_import(Sysml2Parser *parser) {
    Sysml2SourceLoc start = PREVIOUS(parser).range.start;
    Sysml2AstImport *import = SYSML2_ARENA_NEW(parser->arena, Sysml2AstImport);

    /* Check for 'import all' which includes private members */
    bool is_import_all = match(parser, SYSML2_TOKEN_KW_ALL);

    import->target = parse_qualified_name(parser);

    /* Check for ::* or ::** or ::*::** */
    if (match(parser, SYSML2_TOKEN_COLON_COLON)) {
        if (match(parser, SYSML2_TOKEN_STAR)) {
            import->is_all = true;
            /* Check for additional ::** for recursive wildcard import (::*::**) */
            if (match(parser, SYSML2_TOKEN_COLON_COLON)) {
                /* ** is tokenized as STAR_STAR */
                if (match(parser, SYSML2_TOKEN_STAR_STAR)) {
                    import->is_recursive = true;
                } else if (match(parser, SYSML2_TOKEN_STAR)) {
                    import->is_recursive = true;
                    match(parser, SYSML2_TOKEN_STAR);
                }
            }
        } else if (match(parser, SYSML2_TOKEN_STAR_STAR)) {
            /* ::** for recursive import */
            import->is_all = true;
            import->is_recursive = true;
        }
    }

    /* If 'import all' was used, mark as importing all (including private) */
    if (is_import_all) {
        import->is_namespace_import = true;  /* Reuse this flag for 'import all' */
    }

    consume(parser, SYSML2_TOKEN_SEMICOLON, "expected ';' after import");
    import->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);

    return import;
}

/* Parse alias declaration */
static Sysml2AstAlias *parse_alias(Sysml2Parser *parser) {
    Sysml2SourceLoc start = PREVIOUS(parser).range.start;
    Sysml2AstAlias *alias = SYSML2_ARENA_NEW(parser->arena, Sysml2AstAlias);

    if (!consume_identifier(parser, "expected alias name")) {
        return NULL;
    }
    alias->name = get_name(parser);

    if (!consume(parser, SYSML2_TOKEN_KW_FOR, "expected 'for' after alias name")) {
        return NULL;
    }

    alias->target = parse_qualified_name(parser);

    consume(parser, SYSML2_TOKEN_SEMICOLON, "expected ';' after alias");
    alias->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);

    return alias;
}

/* Parse comment */
static Sysml2AstComment *parse_comment(Sysml2Parser *parser) {
    Sysml2SourceLoc start = PREVIOUS(parser).range.start;
    Sysml2AstComment *comment = SYSML2_ARENA_NEW(parser->arena, Sysml2AstComment);

    /* Optional identifier */
    if (is_identifier(parser)) {
        advance(parser);
        /* Name ignored for now */
    }

    /* about clause */
    if (match(parser, SYSML2_TOKEN_KW_ABOUT)) {
        /* Parse list of qualified names */
        Sysml2AstQualifiedName *about[32];
        size_t count = 0;

        do {
            if (count < 32) {
                about[count] = parse_qualified_name(parser);
                if (about[count]) count++;
            }
        } while (match(parser, SYSML2_TOKEN_COMMA));

        comment->about = SYSML2_ARENA_NEW_ARRAY(parser->arena, Sysml2AstQualifiedName *, count);
        for (size_t i = 0; i < count; i++) {
            comment->about[i] = about[i];
        }
        comment->about_count = count;
    }

    /* locale */
    if (match(parser, SYSML2_TOKEN_KW_LOCALE)) {
        if (consume(parser, SYSML2_TOKEN_STRING, "expected locale string")) {
            comment->locale = sysml2_intern_n(
                parser->lexer->intern,
                PREVIOUS(parser).text.data + 1,
                PREVIOUS(parser).text.length - 2
            );
        }
    }

    /* Comment body - skip for now, just consume until end */
    if (match(parser, SYSML2_TOKEN_SLASH)) {
        /* Skip the comment body tokens */
        while (!IS_AT_END(parser) && !CHECK(parser, SYSML2_TOKEN_SEMICOLON) &&
               !CHECK(parser, SYSML2_TOKEN_RBRACE)) {
            advance(parser);
        }
    }

    comment->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
    return comment;
}

/* Parse namespace */
static Sysml2AstNamespace *parse_namespace(Sysml2Parser *parser) {
    Sysml2SourceLoc start = PREVIOUS(parser).range.start;
    Sysml2AstNamespace *ns = SYSML2_ARENA_NEW(parser->arena, Sysml2AstNamespace);

    /* Optional name */
    if (is_identifier(parser)) {
        advance(parser);
        ns->name = get_name(parser);
    }

    ns->members = parse_body(parser);
    ns->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);

    return ns;
}

/* Parse package */
static Sysml2AstPackage *parse_package(Sysml2Parser *parser) {
    Sysml2SourceLoc start = PREVIOUS(parser).range.start;
    Sysml2AstPackage *pkg = SYSML2_ARENA_NEW(parser->arena, Sysml2AstPackage);

    /* Package name */
    if (!consume_identifier(parser, "expected package name")) {
        return NULL;
    }
    pkg->name = get_name(parser);

    pkg->members = parse_body(parser);
    pkg->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);

    return pkg;
}

/* Parse type declaration */
static Sysml2AstType *parse_type(Sysml2Parser *parser) {
    Sysml2SourceLoc start = PREVIOUS(parser).range.start;
    Sysml2AstType *type = SYSML2_ARENA_NEW(parser->arena, Sysml2AstType);

    /* Type name */
    if (is_identifier(parser)) {
        advance(parser);
        type->name = get_name(parser);
    }

    /* Multiplicity */
    type->multiplicity = parse_multiplicity(parser);

    /* Relationships */
    type->relationships = parse_relationships(parser);

    /* Body */
    type->members = parse_body(parser);
    type->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);

    return type;
}

/* Parse classifier (class, datatype, struct, assoc) */
static Sysml2AstClassifier *parse_classifier(Sysml2Parser *parser, Sysml2AstKind kind) {
    Sysml2SourceLoc start = PREVIOUS(parser).range.start;
    Sysml2AstClassifier *cls = SYSML2_ARENA_NEW(parser->arena, Sysml2AstClassifier);
    cls->kind = kind;

    /* Short name syntax: <'shortname'> */
    if (match(parser, SYSML2_TOKEN_LT)) {
        if (CHECK(parser, SYSML2_TOKEN_UNRESTRICTED_NAME)) {
            advance(parser);
            cls->short_name = get_name(parser);
        }
        match(parser, SYSML2_TOKEN_GT);
    }

    /* Classifier name */
    if (is_identifier(parser)) {
        advance(parser);
        cls->name = get_name(parser);
    }

    /* Multiplicity */
    cls->multiplicity = parse_multiplicity(parser);

    /* Relationships */
    cls->relationships = parse_relationships(parser);

    /* Body */
    cls->members = parse_body(parser);
    cls->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);

    return cls;
}

/* Parse feature */
static Sysml2AstFeature *parse_feature(Sysml2Parser *parser) {
    Sysml2SourceLoc start = PREVIOUS(parser).range.start;
    Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

    /* Short name syntax: <'shortname'> */
    if (match(parser, SYSML2_TOKEN_LT)) {
        if (CHECK(parser, SYSML2_TOKEN_UNRESTRICTED_NAME)) {
            advance(parser);
            feat->short_name = get_name(parser);
        }
        match(parser, SYSML2_TOKEN_GT);
    }

    /* Feature name */
    if (is_identifier(parser)) {
        advance(parser);
        feat->name = get_name(parser);
    }

    /* Multiplicity */
    feat->multiplicity = parse_multiplicity(parser);

    /* Relationships */
    feat->relationships = parse_relationships(parser);

    /* Default value */
    if (match(parser, SYSML2_TOKEN_EQ) || match(parser, SYSML2_TOKEN_KW_DEFAULT)) {
        feat->default_value = sysml2_parser_parse_expression(parser);
    }

    /* Body or semicolon */
    if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
        feat->members = parse_body(parser);
    } else {
        consume(parser, SYSML2_TOKEN_SEMICOLON, "expected ';' after feature declaration");
    }

    feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
    return feat;
}

/* Parse a feature chain (dot-separated path like A2.a) */
static Sysml2AstQualifiedName *parse_feature_chain(Sysml2Parser *parser) {
    Sysml2SourceLoc start = CURRENT(parser).range.start;
    const char *segments[64];
    size_t count = 0;

    if (!consume_identifier(parser, "expected identifier in feature chain")) {
        return NULL;
    }
    segments[count++] = get_name(parser);

    /* Parse dot-separated segments */
    while (match(parser, SYSML2_TOKEN_DOT)) {
        if (!consume_identifier(parser, "expected identifier after '.'")) {
            break;
        }
        if (count < 64) {
            segments[count++] = get_name(parser);
        }
    }

    Sysml2AstQualifiedName *chain = SYSML2_ARENA_NEW(parser->arena, Sysml2AstQualifiedName);
    chain->segments = SYSML2_ARENA_NEW_ARRAY(parser->arena, const char *, count);
    for (size_t i = 0; i < count; i++) {
        chain->segments[i] = segments[i];
    }
    chain->segment_count = count;
    chain->is_global = false;
    chain->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);

    return chain;
}

/* Parse a flow connection: flow Source to Target; or flow name from Source to Target; */
static Sysml2AstFeature *parse_flow_usage(Sysml2Parser *parser) {
    Sysml2SourceLoc start = PREVIOUS(parser).range.start;
    Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

    /* Parse the first element - could be flow name or source path */
    Sysml2AstQualifiedName *first = parse_feature_chain(parser);

    /* Check for 'from' keyword: flow Name from Source to Target */
    if (match(parser, SYSML2_TOKEN_KW_FROM)) {
        /* First element is the flow name */
        if (first && first->segment_count > 0) {
            feat->name = first->segments[0];
        }

        /* Parse the source path */
        Sysml2AstQualifiedName *source = parse_feature_chain(parser);
        (void)source; /* Store if needed */

        /* Parse 'to' and target */
        if (match(parser, SYSML2_TOKEN_KW_TO)) {
            Sysml2AstQualifiedName *target = parse_feature_chain(parser);
            if (target) {
                Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
                rel->kind = SYSML2_REL_TYPED_BY;
                rel->target = target;
                rel->next = NULL;
                feat->relationships = rel;
            }
        }
        consume(parser, SYSML2_TOKEN_SEMICOLON, "expected ';' after flow connection");
    }
    /* Check for direct 'to' keyword: flow Source to Target */
    else if (match(parser, SYSML2_TOKEN_KW_TO)) {
        /* First element is the source path */
        if (first && first->segment_count > 0) {
            feat->name = first->segments[0];
        }

        /* Parse the target path */
        Sysml2AstQualifiedName *target = parse_feature_chain(parser);
        if (target) {
            Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
            rel->kind = SYSML2_REL_TYPED_BY;
            rel->target = target;
            rel->next = NULL;
            feat->relationships = rel;
        }
        consume(parser, SYSML2_TOKEN_SEMICOLON, "expected ';' after flow connection");
    } else {
        /* Standard flow feature syntax */
        if (first && first->segment_count > 0) {
            feat->name = first->segments[0];
        }
        feat->multiplicity = parse_multiplicity(parser);
        feat->relationships = parse_relationships(parser);

        if (match(parser, SYSML2_TOKEN_EQ) || match(parser, SYSML2_TOKEN_KW_DEFAULT)) {
            feat->default_value = sysml2_parser_parse_expression(parser);
        }

        if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
            feat->members = parse_body(parser);
        } else {
            consume(parser, SYSML2_TOKEN_SEMICOLON, "expected ';' after flow declaration");
        }
    }

    feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
    return feat;
}

/* Parse a namespace body element */
static Sysml2AstMember *parse_namespace_body_element(Sysml2Parser *parser) {
    Sysml2Visibility visibility = parse_visibility(parser);

    /* Skip metadata annotations (#Name and @Name) */
    while (CHECK(parser, SYSML2_TOKEN_HASH) || CHECK(parser, SYSML2_TOKEN_AT)) {
        advance(parser);  /* Skip # or @ */
        if (is_identifier(parser)) {
            advance(parser);  /* Skip the name */
        }
        /* Skip body if present */
        if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
            parse_body(parser);
        }
        /* Handle standalone @Name; (semicolon terminates the annotation) */
        if (CHECK(parser, SYSML2_TOKEN_SEMICOLON)) {
            advance(parser);
            /* Return an empty metadata member */
            Sysml2AstMember *member = SYSML2_ARENA_NEW(parser->arena, Sysml2AstMember);
            member->kind = SYSML2_AST_METADATA;
            member->visibility = visibility;
            member->node = NULL;
            return member;
        }
    }

    /* Parse direction - can come before or after prefix */
    Sysml2Direction direction = parse_direction(parser);

    Sysml2TypePrefix prefix = parse_type_prefix(parser);

    /* Skip metadata annotations after type prefix */
    while (CHECK(parser, SYSML2_TOKEN_HASH) || CHECK(parser, SYSML2_TOKEN_AT)) {
        advance(parser);  /* Skip # or @ */
        if (is_identifier(parser)) {
            advance(parser);  /* Skip the name */
        }
        /* Skip body if present */
        if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
            parse_body(parser);
        }
    }

    /* Parse direction again if it came after prefix (for backward compat) */
    if (direction == SYSML2_DIR_NONE) {
        direction = parse_direction(parser);
    }

    Sysml2AstMember *member = SYSML2_ARENA_NEW(parser->arena, Sysml2AstMember);
    member->visibility = visibility;

    /* Dispatch based on keyword */
    if (match(parser, SYSML2_TOKEN_KW_NAMESPACE)) {
        member->kind = SYSML2_AST_NAMESPACE;
        Sysml2AstNamespace *ns = parse_namespace(parser);
        member->node = ns;
    } else if (match(parser, SYSML2_TOKEN_KW_PACKAGE)) {
        member->kind = SYSML2_AST_PACKAGE;
        Sysml2AstPackage *pkg = parse_package(parser);
        member->node = pkg;
    } else if (match(parser, SYSML2_TOKEN_KW_STANDARD)) {
        /* standard library package */
        if (match(parser, SYSML2_TOKEN_KW_LIBRARY)) {
            if (match(parser, SYSML2_TOKEN_KW_PACKAGE)) {
                member->kind = SYSML2_AST_PACKAGE;
                Sysml2AstPackage *pkg = parse_package(parser);
                if (pkg) {
                    pkg->is_library = true;
                    pkg->is_standard = true;
                }
                member->node = pkg;
            }
        }
    } else if (match(parser, SYSML2_TOKEN_KW_LIBRARY)) {
        if (match(parser, SYSML2_TOKEN_KW_PACKAGE)) {
            member->kind = SYSML2_AST_PACKAGE;
            Sysml2AstPackage *pkg = parse_package(parser);
            if (pkg) pkg->is_library = true;
            member->node = pkg;
        }
    } else if (match(parser, SYSML2_TOKEN_KW_IMPORT)) {
        member->kind = SYSML2_AST_IMPORT;
        member->node = parse_import(parser);
    } else if (match(parser, SYSML2_TOKEN_KW_ALIAS)) {
        member->kind = SYSML2_AST_ALIAS;
        member->node = parse_alias(parser);
    } else if (match(parser, SYSML2_TOKEN_KW_COMMENT)) {
        member->kind = SYSML2_AST_COMMENT;
        member->node = parse_comment(parser);
    } else if (match(parser, SYSML2_TOKEN_KW_DOC)) {
        member->kind = SYSML2_AST_DOCUMENTATION;
        member->node = parse_comment(parser); /* Similar syntax */
    } else if (match(parser, SYSML2_TOKEN_KW_TYPE)) {
        member->kind = SYSML2_AST_TYPE;
        Sysml2AstType *type = parse_type(parser);
        if (type) type->prefix = prefix;
        member->node = type;
    } else if (match(parser, SYSML2_TOKEN_KW_CLASSIFIER)) {
        member->kind = SYSML2_AST_CLASSIFIER;
        Sysml2AstClassifier *cls = parse_classifier(parser, SYSML2_AST_CLASSIFIER);
        if (cls) cls->prefix = prefix;
        member->node = cls;
    } else if (match(parser, SYSML2_TOKEN_KW_CLASS)) {
        member->kind = SYSML2_AST_CLASS;
        Sysml2AstClassifier *cls = parse_classifier(parser, SYSML2_AST_CLASS);
        if (cls) cls->prefix = prefix;
        member->node = cls;
    } else if (match(parser, SYSML2_TOKEN_KW_DATATYPE)) {
        member->kind = SYSML2_AST_DATATYPE;
        Sysml2AstClassifier *cls = parse_classifier(parser, SYSML2_AST_DATATYPE);
        if (cls) cls->prefix = prefix;
        member->node = cls;
    } else if (match(parser, SYSML2_TOKEN_KW_STRUCT)) {
        member->kind = SYSML2_AST_STRUCT;
        Sysml2AstClassifier *cls = parse_classifier(parser, SYSML2_AST_STRUCT);
        if (cls) cls->prefix = prefix;
        member->node = cls;
    } else if (match(parser, SYSML2_TOKEN_KW_ASSOC)) {
        member->kind = SYSML2_AST_ASSOC;
        Sysml2AstClassifier *cls = parse_classifier(parser, SYSML2_AST_ASSOC);
        if (cls) cls->prefix = prefix;
        member->node = cls;
    } else if (match(parser, SYSML2_TOKEN_KW_BEHAVIOR)) {
        member->kind = SYSML2_AST_BEHAVIOR;
        Sysml2AstClassifier *cls = parse_classifier(parser, SYSML2_AST_BEHAVIOR);
        if (cls) cls->prefix = prefix;
        member->node = cls;
    } else if (match(parser, SYSML2_TOKEN_KW_FUNCTION)) {
        member->kind = SYSML2_AST_FUNCTION;
        Sysml2AstClassifier *cls = parse_classifier(parser, SYSML2_AST_FUNCTION);
        if (cls) cls->prefix = prefix;
        member->node = cls;
    } else if (match(parser, SYSML2_TOKEN_KW_PREDICATE)) {
        member->kind = SYSML2_AST_PREDICATE;
        Sysml2AstClassifier *cls = parse_classifier(parser, SYSML2_AST_PREDICATE);
        if (cls) cls->prefix = prefix;
        member->node = cls;
    } else if (match(parser, SYSML2_TOKEN_KW_METADATA)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_METADATA;
            member->node = parse_classifier(parser, SYSML2_AST_METADATA);
        } else {
            /* metadata usage */
            member->kind = SYSML2_AST_METADATA;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_FEATURE)) {
        member->kind = SYSML2_AST_FEATURE;
        Sysml2AstFeature *feat = parse_feature(parser);
        if (feat) {
            feat->prefix = prefix;
            feat->direction = direction;
        }
        member->node = feat;
    } else if (match(parser, SYSML2_TOKEN_KW_CONNECTOR)) {
        member->kind = SYSML2_AST_CONNECTOR;
        member->node = parse_feature(parser);
    } else if (match(parser, SYSML2_TOKEN_KW_BINDING)) {
        member->kind = SYSML2_AST_BINDING;
        member->node = parse_feature(parser);
    } else if (match(parser, SYSML2_TOKEN_KW_SUCCESSION)) {
        /* Check for 'succession flow' syntax */
        if (match(parser, SYSML2_TOKEN_KW_FLOW)) {
            member->kind = SYSML2_AST_SUCCESSION_FLOW;
            member->node = parse_flow_usage(parser);
        } else {
            member->kind = SYSML2_AST_SUCCESSION;
            member->node = parse_feature(parser);
        }
    }
    /* SysML v2 definitions */
    else if (match(parser, SYSML2_TOKEN_KW_PART)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_PART_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_PART_DEF);
        } else {
            member->kind = SYSML2_AST_PART_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_ACTION)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_ACTION_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_ACTION_DEF);
        } else {
            member->kind = SYSML2_AST_ACTION_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_PERFORM)) {
        /* perform actionReference { bindings } */
        member->kind = SYSML2_AST_PERFORM;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Parse the action reference (feature chain like illuminateRegion.sendOnOffCmd) */
        Sysml2AstQualifiedName *action_ref = parse_feature_chain(parser);
        if (action_ref && action_ref->segment_count > 0) {
            feat->name = action_ref->segments[action_ref->segment_count - 1];
            /* Store full reference as relationship */
            Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
            rel->kind = SYSML2_REL_TYPED_BY;
            rel->target = action_ref;
            rel->next = NULL;
            feat->relationships = rel;
        }

        /* Parse body or semicolon */
        if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
            feat->members = parse_body(parser);
        } else {
            consume(parser, SYSML2_TOKEN_SEMICOLON, "expected ';' after perform declaration");
        }

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    } else if (match(parser, SYSML2_TOKEN_KW_STATE)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_STATE_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_STATE_DEF);
        } else {
            member->kind = SYSML2_AST_STATE_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_REQUIREMENT)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_REQUIREMENT_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_REQUIREMENT_DEF);
        } else {
            member->kind = SYSML2_AST_REQUIREMENT_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_CONSTRAINT)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_CONSTRAINT_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_CONSTRAINT_DEF);
        } else {
            member->kind = SYSML2_AST_CONSTRAINT_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_PORT)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_PORT_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_PORT_DEF);
        } else {
            member->kind = SYSML2_AST_PORT_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_ATTRIBUTE)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_ATTRIBUTE_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_ATTRIBUTE_DEF);
        } else {
            member->kind = SYSML2_AST_ATTRIBUTE_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_ITEM)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_ITEM_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_ITEM_DEF);
        } else {
            member->kind = SYSML2_AST_ITEM_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_INTERFACE)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_INTERFACE_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_INTERFACE_DEF);
        } else {
            /* interface usage - could be 'interface name connect ... to ...' */
            member->kind = SYSML2_AST_INTERFACE_USAGE;
            Sysml2SourceLoc start = PREVIOUS(parser).range.start;
            Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

            /* Parse optional name */
            if (is_identifier(parser)) {
                advance(parser);
                feat->name = get_name(parser);
            }

            /* Check for 'connect' keyword */
            if (match(parser, SYSML2_TOKEN_KW_CONNECT)) {
                /* Parse source feature chain */
                Sysml2AstQualifiedName *source = parse_feature_chain(parser);
                (void)source; /* Store in feature if needed */

                /* Parse 'to' keyword and target */
                if (match(parser, SYSML2_TOKEN_KW_TO)) {
                    Sysml2AstQualifiedName *target = parse_feature_chain(parser);
                    /* Store target as relationship */
                    if (target) {
                        Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
                        rel->kind = SYSML2_REL_TYPED_BY;
                        rel->target = target;
                        rel->next = NULL;
                        feat->relationships = rel;
                    }
                }
            } else {
                /* Regular interface usage, parse multiplicity and relationships */
                feat->multiplicity = parse_multiplicity(parser);
                feat->relationships = parse_relationships(parser);
            }

            /* Parse body or semicolon */
            if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
                feat->members = parse_body(parser);
            } else {
                consume(parser, SYSML2_TOKEN_SEMICOLON, "expected ';' after interface declaration");
            }

            feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
            member->node = feat;
        }
    } else if (match(parser, SYSML2_TOKEN_KW_CONNECTION)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_CONNECTION_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_CONNECTION_DEF);
        } else {
            member->kind = SYSML2_AST_CONNECTION_USAGE;
            Sysml2SourceLoc start = PREVIOUS(parser).range.start;
            Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

            /* Parse name */
            if (is_identifier(parser)) {
                advance(parser);
                feat->name = get_name(parser);
            }

            /* Parse relationships (type) */
            feat->relationships = parse_relationships(parser);

            /* Parse 'connect (...)' clause */
            if (match(parser, SYSML2_TOKEN_KW_CONNECT)) {
                if (match(parser, SYSML2_TOKEN_LPAREN)) {
                    /* Connection list: connect (d1, d2, d3, d4) */
                    while (!CHECK(parser, SYSML2_TOKEN_RPAREN) && !IS_AT_END(parser)) {
                        if (is_identifier(parser)) {
                            parse_feature_chain(parser);
                        }
                        if (!match(parser, SYSML2_TOKEN_COMMA)) break;
                    }
                    match(parser, SYSML2_TOKEN_RPAREN);
                } else {
                    /* Simple: connect a to b */
                    parse_feature_chain(parser);
                    if (match(parser, SYSML2_TOKEN_KW_TO)) {
                        parse_feature_chain(parser);
                    }
                }
            }

            /* Parse body or semicolon */
            if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
                feat->members = parse_body(parser);
            } else {
                match(parser, SYSML2_TOKEN_SEMICOLON);
            }

            feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
            member->node = feat;
        }
    } else if (match(parser, SYSML2_TOKEN_KW_FLOW)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_FLOW_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_FLOW_DEF);
        } else {
            member->kind = SYSML2_AST_FLOW_USAGE;
            member->node = parse_flow_usage(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_VIEW)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_VIEW_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_VIEW_DEF);
        } else {
            member->kind = SYSML2_AST_VIEW_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_VIEWPOINT)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_VIEWPOINT_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_VIEWPOINT_DEF);
        } else {
            member->kind = SYSML2_AST_VIEWPOINT_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_ENUM)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_ENUM_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_ENUM_DEF);
        } else {
            member->kind = SYSML2_AST_ENUM_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_CALC)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_CALC_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_CALC_DEF);
        } else {
            member->kind = SYSML2_AST_CALC_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_CASE)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_CASE_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_CASE_DEF);
        } else {
            member->kind = SYSML2_AST_CASE_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_ANALYSIS)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_ANALYSIS_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_ANALYSIS_DEF);
        } else {
            member->kind = SYSML2_AST_ANALYSIS_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_VERIFICATION)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_VERIFICATION_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_VERIFICATION_DEF);
        } else {
            member->kind = SYSML2_AST_VERIFICATION_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_USE)) {
        /* use case def / use case */
        if (match(parser, SYSML2_TOKEN_KW_CASE)) {
            if (match(parser, SYSML2_TOKEN_KW_DEF)) {
                member->kind = SYSML2_AST_USECASE_DEF;
                member->node = parse_classifier(parser, SYSML2_AST_USECASE_DEF);
            } else {
                member->kind = SYSML2_AST_USECASE_USAGE;
                member->node = parse_feature(parser);
            }
        } else {
            /* Just 'use' as a keyword - treat as a feature */
            member->kind = SYSML2_AST_FEATURE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_ALLOCATION)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_ALLOCATION_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_ALLOCATION_DEF);
        } else {
            member->kind = SYSML2_AST_ALLOCATION_USAGE;
            Sysml2SourceLoc start = PREVIOUS(parser).range.start;
            Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

            /* Parse name */
            if (is_identifier(parser)) {
                advance(parser);
                feat->name = get_name(parser);
            }

            /* Parse relationships (type) */
            feat->relationships = parse_relationships(parser);

            /* Parse 'allocate ... to ...' clause */
            if (match(parser, SYSML2_TOKEN_KW_ALLOCATE)) {
                /* Parse source or list */
                if (match(parser, SYSML2_TOKEN_LPAREN)) {
                    /* Binding list syntax: allocate (logical ::> l, physical ::> p) */
                    while (!CHECK(parser, SYSML2_TOKEN_RPAREN) && !IS_AT_END(parser)) {
                        advance(parser);
                    }
                    match(parser, SYSML2_TOKEN_RPAREN);
                } else {
                    /* Simple allocate: allocate l to p */
                    parse_feature_chain(parser);
                    if (match(parser, SYSML2_TOKEN_KW_TO)) {
                        parse_feature_chain(parser);
                    }
                }
            }

            /* Parse body or semicolon */
            if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
                feat->members = parse_body(parser);
            } else {
                match(parser, SYSML2_TOKEN_SEMICOLON);
            }

            feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
            member->node = feat;
        }
    } else if (match(parser, SYSML2_TOKEN_KW_RENDERING)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_RENDERING_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_RENDERING_DEF);
        } else {
            member->kind = SYSML2_AST_RENDERING_USAGE;
            member->node = parse_feature(parser);
        }
    } else if (match(parser, SYSML2_TOKEN_KW_OCCURRENCE)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_OCCURRENCE_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_OCCURRENCE_DEF);
        } else {
            member->kind = SYSML2_AST_OCCURRENCE_USAGE;
            member->node = parse_feature(parser);
        }
    }
    /* State machine behavioral modifiers: entry, exit, do */
    else if (match(parser, SYSML2_TOKEN_KW_ENTRY)) {
        if (match(parser, SYSML2_TOKEN_KW_STATE)) {
            /* entry state name; */
            member->kind = SYSML2_AST_STATE_USAGE;
            Sysml2AstFeature *feat = parse_feature(parser);
            if (feat) {
                feat->prefix = prefix;
                feat->prefix.is_composite = true;  /* Mark as entry state */
            }
            member->node = feat;
        } else if (match(parser, SYSML2_TOKEN_KW_ACTION)) {
            /* entry action name { ... } */
            member->kind = SYSML2_AST_ACTION_USAGE;
            member->node = parse_feature(parser);
        } else {
            /* entry; - shorthand for entry action */
            member->kind = SYSML2_AST_ENTRY_ACTION;
            Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);
            feat->range = PREVIOUS(parser).range;
            if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
                feat->members = parse_body(parser);
            } else {
                consume(parser, SYSML2_TOKEN_SEMICOLON, "expected ';' after entry");
            }
            member->node = feat;
        }
    } else if (match(parser, SYSML2_TOKEN_KW_EXIT)) {
        if (match(parser, SYSML2_TOKEN_KW_ACTION)) {
            /* exit action name { ... } */
            member->kind = SYSML2_AST_ACTION_USAGE;
            member->node = parse_feature(parser);
        } else {
            /* exit; - shorthand for exit action */
            member->kind = SYSML2_AST_EXIT_ACTION;
            Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);
            feat->range = PREVIOUS(parser).range;
            if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
                feat->members = parse_body(parser);
            } else {
                consume(parser, SYSML2_TOKEN_SEMICOLON, "expected ';' after exit");
            }
            member->node = feat;
        }
    } else if (match(parser, SYSML2_TOKEN_KW_DO)) {
        if (match(parser, SYSML2_TOKEN_KW_ACTION)) {
            /* do action name { ... } */
            member->kind = SYSML2_AST_ACTION_USAGE;
            member->node = parse_feature(parser);
        } else {
            /* do; - shorthand for do action */
            member->kind = SYSML2_AST_DO_ACTION;
            Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);
            feat->range = PREVIOUS(parser).range;
            if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
                feat->members = parse_body(parser);
            } else {
                consume(parser, SYSML2_TOKEN_SEMICOLON, "expected ';' after do");
            }
            member->node = feat;
        }
    }
    /* Transition support */
    else if (match(parser, SYSML2_TOKEN_KW_TRANSITION)) {
        member->kind = SYSML2_AST_TRANSITION;
        member->node = parse_feature(parser);
    }
    /* Requirement body: require constraint { ... } */
    else if (match(parser, SYSML2_TOKEN_KW_REQUIRE)) {
        if (match(parser, SYSML2_TOKEN_KW_CONSTRAINT)) {
            member->kind = SYSML2_AST_CONSTRAINT_USAGE;
            member->node = parse_feature(parser);
        } else {
            /* require without constraint - treat as requirement usage */
            member->kind = SYSML2_AST_REQUIREMENT_USAGE;
            member->node = parse_feature(parser);
        }
    }
    /* Control flow: then keyword - succession to next action */
    else if (match(parser, SYSML2_TOKEN_KW_THEN)) {
        /* Check if followed by fork/join/merge keywords */
        if (match(parser, SYSML2_TOKEN_KW_FORK)) {
            member->kind = SYSML2_AST_FORK_NODE;
            member->node = parse_feature(parser);
        } else if (match(parser, SYSML2_TOKEN_KW_JOIN)) {
            member->kind = SYSML2_AST_JOIN_NODE;
            member->node = parse_feature(parser);
        } else if (match(parser, SYSML2_TOKEN_KW_MERGE)) {
            member->kind = SYSML2_AST_MERGE_NODE;
            member->node = parse_feature(parser);
        } else {
            member->kind = SYSML2_AST_THEN;
            member->node = parse_feature(parser);
        }
    }
    /* Join node */
    else if (match(parser, SYSML2_TOKEN_KW_JOIN)) {
        member->kind = SYSML2_AST_JOIN_NODE;
        member->node = parse_feature(parser);
    }
    /* Fork node */
    else if (match(parser, SYSML2_TOKEN_KW_FORK)) {
        member->kind = SYSML2_AST_FORK_NODE;
        member->node = parse_feature(parser);
    }
    /* Merge node */
    else if (match(parser, SYSML2_TOKEN_KW_MERGE)) {
        member->kind = SYSML2_AST_MERGE_NODE;
        member->node = parse_feature(parser);
    }
    /* First (for 'first start' pattern) */
    else if (match(parser, SYSML2_TOKEN_KW_FIRST)) {
        member->kind = SYSML2_AST_FIRST;
        member->node = parse_feature(parser);
    }
    /* Bind: bind x = y.z; */
    else if (match(parser, SYSML2_TOKEN_KW_BIND)) {
        member->kind = SYSML2_AST_BIND;
        member->node = parse_feature(parser);
    }
    /* Binding (KerML): binding name { ... } */
    else if (match(parser, SYSML2_TOKEN_KW_BINDING)) {
        member->kind = SYSML2_AST_BINDING;
        member->node = parse_feature(parser);
    }
    /* Accept: accept S; or accept sig after 10[s]; */
    else if (match(parser, SYSML2_TOKEN_KW_ACCEPT)) {
        member->kind = SYSML2_AST_ACCEPT;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Parse optional name and type: accept sig : Sig or accept s : Sig */
        if (is_identifier(parser)) {
            advance(parser);
            feat->name = get_name(parser);
        }

        /* Parse relationships for typed accept */
        feat->relationships = parse_relationships(parser);

        /* Handle 'accept ... after/at/when/via' variants */
        if (match(parser, SYSML2_TOKEN_KW_AFTER) || match(parser, SYSML2_TOKEN_KW_AT) ||
            match(parser, SYSML2_TOKEN_KW_WHEN) || match(parser, SYSML2_TOKEN_KW_VIA)) {
            /* Skip the expression/path following the keyword */
            while (!CHECK(parser, SYSML2_TOKEN_SEMICOLON) &&
                   !CHECK(parser, SYSML2_TOKEN_KW_THEN) &&
                   !CHECK(parser, SYSML2_TOKEN_KW_DO) &&
                   !CHECK(parser, SYSML2_TOKEN_LBRACE) &&
                   !IS_AT_END(parser)) {
                advance(parser);
            }
        }

        /* Parse do action */
        if (match(parser, SYSML2_TOKEN_KW_DO)) {
            match(parser, SYSML2_TOKEN_KW_ACTION);
            /* Skip action body */
            if (is_identifier(parser)) {
                advance(parser);
            }
        }

        /* Parse then clause */
        if (match(parser, SYSML2_TOKEN_KW_THEN)) {
            /* Skip target state */
            if (is_identifier(parser) || CHECK(parser, SYSML2_TOKEN_KW_DONE)) {
                while (is_identifier(parser) || CHECK(parser, SYSML2_TOKEN_DOT) ||
                       CHECK(parser, SYSML2_TOKEN_KW_DONE)) {
                    advance(parser);
                }
            }
        }

        if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
            feat->members = parse_body(parser);
        } else if (!CHECK(parser, SYSML2_TOKEN_RBRACE)) {
            match(parser, SYSML2_TOKEN_SEMICOLON);
        }

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Send: send new S() to target; */
    else if (match(parser, SYSML2_TOKEN_KW_SEND)) {
        member->kind = SYSML2_AST_SEND;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Skip the expression until 'to' or ';' */
        while (!CHECK(parser, SYSML2_TOKEN_KW_TO) &&
               !CHECK(parser, SYSML2_TOKEN_SEMICOLON) &&
               !CHECK(parser, SYSML2_TOKEN_LBRACE) &&
               !IS_AT_END(parser)) {
            advance(parser);
        }

        /* Parse 'to' target */
        if (match(parser, SYSML2_TOKEN_KW_TO)) {
            Sysml2AstQualifiedName *target = parse_feature_chain(parser);
            if (target) {
                Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
                rel->kind = SYSML2_REL_TYPED_BY;
                rel->target = target;
                rel->next = NULL;
                feat->relationships = rel;
            }
        }

        if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
            feat->members = parse_body(parser);
        } else {
            match(parser, SYSML2_TOKEN_SEMICOLON);
        }

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Decide: decide; if cond then X; else Y; */
    else if (match(parser, SYSML2_TOKEN_KW_DECIDE)) {
        member->kind = SYSML2_AST_DECIDE;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Optional name */
        if (is_identifier(parser)) {
            advance(parser);
            feat->name = get_name(parser);
        }

        /* Parse body or semicolon */
        if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
            feat->members = parse_body(parser);
        } else {
            match(parser, SYSML2_TOKEN_SEMICOLON);
        }

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* If statement (control flow): if cond { ... } else { ... } */
    else if (match(parser, SYSML2_TOKEN_KW_IF)) {
        member->kind = SYSML2_AST_IF_ACTION;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Skip the condition expression */
        while (!CHECK(parser, SYSML2_TOKEN_KW_THEN) &&
               !CHECK(parser, SYSML2_TOKEN_LBRACE) &&
               !CHECK(parser, SYSML2_TOKEN_SEMICOLON) &&
               !IS_AT_END(parser)) {
            advance(parser);
        }

        /* Handle 'if cond then Target;' decision branch syntax */
        if (match(parser, SYSML2_TOKEN_KW_THEN)) {
            /* Skip target name */
            if (is_identifier(parser) || CHECK(parser, SYSML2_TOKEN_KW_DONE)) {
                while (is_identifier(parser) || CHECK(parser, SYSML2_TOKEN_DOT) ||
                       CHECK(parser, SYSML2_TOKEN_KW_DONE)) {
                    advance(parser);
                }
            }
            match(parser, SYSML2_TOKEN_SEMICOLON);
        } else if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
            /* Parse if body */
            feat->members = parse_body(parser);

            /* Handle else if / else */
            while (match(parser, SYSML2_TOKEN_KW_ELSE)) {
                if (match(parser, SYSML2_TOKEN_KW_IF)) {
                    /* Skip condition */
                    while (!CHECK(parser, SYSML2_TOKEN_LBRACE) &&
                           !CHECK(parser, SYSML2_TOKEN_SEMICOLON) &&
                           !IS_AT_END(parser)) {
                        advance(parser);
                    }
                    if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
                        parse_body(parser);
                    }
                } else if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
                    parse_body(parser);
                    break;
                } else {
                    /* 'else Target;' - skip target and semicolon */
                    if (is_identifier(parser) || CHECK(parser, SYSML2_TOKEN_KW_DONE)) {
                        while (is_identifier(parser) || CHECK(parser, SYSML2_TOKEN_DOT) ||
                               CHECK(parser, SYSML2_TOKEN_KW_DONE)) {
                            advance(parser);
                        }
                    }
                    match(parser, SYSML2_TOKEN_SEMICOLON);
                    break;
                }
            }
        }

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* While loop: while cond { ... } until b; */
    else if (match(parser, SYSML2_TOKEN_KW_WHILE)) {
        member->kind = SYSML2_AST_WHILE_LOOP;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Skip condition */
        while (!CHECK(parser, SYSML2_TOKEN_LBRACE) &&
               !CHECK(parser, SYSML2_TOKEN_SEMICOLON) &&
               !IS_AT_END(parser)) {
            advance(parser);
        }

        if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
            feat->members = parse_body(parser);
        }

        /* Handle 'until' clause */
        if (match(parser, SYSML2_TOKEN_KW_UNTIL)) {
            /* Skip until expression */
            while (!CHECK(parser, SYSML2_TOKEN_SEMICOLON) &&
                   !CHECK(parser, SYSML2_TOKEN_RBRACE) &&
                   !IS_AT_END(parser)) {
                advance(parser);
            }
        }
        match(parser, SYSML2_TOKEN_SEMICOLON);

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* For loop: for n : Type in (...) { ... } */
    else if (match(parser, SYSML2_TOKEN_KW_FOR)) {
        member->kind = SYSML2_AST_FOR_LOOP;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Parse loop variable */
        if (is_identifier(parser)) {
            advance(parser);
            feat->name = get_name(parser);
        }

        /* Skip type and 'in' expression */
        while (!CHECK(parser, SYSML2_TOKEN_LBRACE) &&
               !CHECK(parser, SYSML2_TOKEN_SEMICOLON) &&
               !IS_AT_END(parser)) {
            advance(parser);
        }

        if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
            feat->members = parse_body(parser);
        }

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Loop: loop { ... } until cond; */
    else if (match(parser, SYSML2_TOKEN_KW_LOOP)) {
        member->kind = SYSML2_AST_LOOP;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
            feat->members = parse_body(parser);
        }

        /* Handle 'until' clause */
        if (match(parser, SYSML2_TOKEN_KW_UNTIL)) {
            /* Skip until expression */
            while (!CHECK(parser, SYSML2_TOKEN_SEMICOLON) &&
                   !CHECK(parser, SYSML2_TOKEN_RBRACE) &&
                   !IS_AT_END(parser)) {
                advance(parser);
            }
        }
        match(parser, SYSML2_TOKEN_SEMICOLON);

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Assign: assign x := value; */
    else if (match(parser, SYSML2_TOKEN_KW_ASSIGN)) {
        member->kind = SYSML2_AST_ASSIGN;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Parse target feature chain */
        Sysml2AstQualifiedName *target = parse_feature_chain(parser);
        if (target && target->segment_count > 0) {
            feat->name = target->segments[0];
            Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
            rel->kind = SYSML2_REL_TYPED_BY;
            rel->target = target;
            rel->next = NULL;
            feat->relationships = rel;
        }

        /* Parse := and value expression */
        if (match(parser, SYSML2_TOKEN_COLON) && match(parser, SYSML2_TOKEN_EQ)) {
            feat->default_value = sysml2_parser_parse_expression(parser);
        }

        match(parser, SYSML2_TOKEN_SEMICOLON);
        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Subject: subject s : Type; */
    else if (match(parser, SYSML2_TOKEN_KW_SUBJECT)) {
        member->kind = SYSML2_AST_SUBJECT;
        member->node = parse_feature(parser);
    }
    /* Actor: actor user : User; */
    else if (match(parser, SYSML2_TOKEN_KW_ACTOR)) {
        member->kind = SYSML2_AST_ACTOR;
        member->node = parse_feature(parser);
    }
    /* Objective: objective { ... } */
    else if (match(parser, SYSML2_TOKEN_KW_OBJECTIVE)) {
        member->kind = SYSML2_AST_OBJECTIVE;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Optional name and relationships */
        if (is_identifier(parser)) {
            advance(parser);
            feat->name = get_name(parser);
        }
        feat->relationships = parse_relationships(parser);

        if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
            feat->members = parse_body(parser);
        } else {
            match(parser, SYSML2_TOKEN_SEMICOLON);
        }

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Include: include use case uc1; */
    else if (match(parser, SYSML2_TOKEN_KW_INCLUDE)) {
        member->kind = SYSML2_AST_INCLUDE;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Optional 'use case' keywords */
        match(parser, SYSML2_TOKEN_KW_USE);
        match(parser, SYSML2_TOKEN_KW_CASE);

        /* Parse name or feature chain */
        if (is_identifier(parser)) {
            Sysml2AstQualifiedName *chain = parse_feature_chain(parser);
            if (chain && chain->segment_count > 0) {
                feat->name = chain->segments[chain->segment_count - 1];
                Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
                rel->kind = SYSML2_REL_TYPED_BY;
                rel->target = chain;
                rel->next = NULL;
                feat->relationships = rel;
            }
        }

        /* Parse optional relationships */
        if (feat->relationships == NULL) {
            feat->relationships = parse_relationships(parser);
        }

        if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
            feat->members = parse_body(parser);
        } else {
            match(parser, SYSML2_TOKEN_SEMICOLON);
        }

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Satisfy: satisfy r by p; */
    else if (match(parser, SYSML2_TOKEN_KW_SATISFY)) {
        member->kind = SYSML2_AST_SATISFY;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Parse requirement reference */
        if (is_identifier(parser)) {
            Sysml2AstQualifiedName *chain = parse_feature_chain(parser);
            if (chain && chain->segment_count > 0) {
                feat->name = chain->segments[0];
            }
        }

        /* Parse 'by' clause */
        if (match(parser, SYSML2_TOKEN_KW_BY)) {
            Sysml2AstQualifiedName *target = parse_feature_chain(parser);
            if (target) {
                Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
                rel->kind = SYSML2_REL_TYPED_BY;
                rel->target = target;
                rel->next = NULL;
                feat->relationships = rel;
            }
        }

        match(parser, SYSML2_TOKEN_SEMICOLON);
        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Assume: assume constraint c; */
    else if (match(parser, SYSML2_TOKEN_KW_ASSUME)) {
        member->kind = SYSML2_AST_ASSUME;
        match(parser, SYSML2_TOKEN_KW_CONSTRAINT);
        member->node = parse_feature(parser);
    }
    /* Assumption (alternate form): assumption constraint c; */
    else if (match(parser, SYSML2_TOKEN_KW_ASSUMPTION)) {
        member->kind = SYSML2_AST_ASSUME;
        match(parser, SYSML2_TOKEN_KW_CONSTRAINT);
        member->node = parse_feature(parser);
    }
    /* Assert: assert satisfy r by p; */
    else if (match(parser, SYSML2_TOKEN_KW_ASSERT)) {
        member->kind = SYSML2_AST_ASSERT;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Check for 'not' prefix */
        match(parser, SYSML2_TOKEN_KW_NOT);

        /* Check for 'satisfy' or 'constraint' */
        if (match(parser, SYSML2_TOKEN_KW_SATISFY)) {
            /* Parse requirement reference */
            if (is_identifier(parser)) {
                Sysml2AstQualifiedName *chain = parse_feature_chain(parser);
                if (chain && chain->segment_count > 0) {
                    feat->name = chain->segments[0];
                }
            }

            /* Parse 'by' clause */
            if (match(parser, SYSML2_TOKEN_KW_BY)) {
                Sysml2AstQualifiedName *target = parse_feature_chain(parser);
                if (target) {
                    Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
                    rel->kind = SYSML2_REL_TYPED_BY;
                    rel->target = target;
                    rel->next = NULL;
                    feat->relationships = rel;
                }
            }
            match(parser, SYSML2_TOKEN_SEMICOLON);
        } else if (match(parser, SYSML2_TOKEN_KW_CONSTRAINT)) {
            Sysml2AstFeature *constraint = parse_feature(parser);
            if (constraint) {
                feat->name = constraint->name;
                feat->relationships = constraint->relationships;
                feat->members = constraint->members;
            }
        } else {
            /* Just 'assert' - parse as feature */
            Sysml2AstFeature *inner = parse_feature(parser);
            if (inner) {
                feat->name = inner->name;
                feat->relationships = inner->relationships;
                feat->members = inner->members;
            }
        }

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Not: not satisfy r by p; */
    else if (match(parser, SYSML2_TOKEN_KW_NOT)) {
        /* Handle 'not satisfy' */
        if (match(parser, SYSML2_TOKEN_KW_SATISFY)) {
            member->kind = SYSML2_AST_SATISFY;
            Sysml2SourceLoc start = PREVIOUS(parser).range.start;
            Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

            /* Parse requirement reference */
            if (is_identifier(parser)) {
                Sysml2AstQualifiedName *chain = parse_feature_chain(parser);
                if (chain && chain->segment_count > 0) {
                    feat->name = chain->segments[0];
                }
            }

            /* Parse 'by' clause */
            if (match(parser, SYSML2_TOKEN_KW_BY)) {
                Sysml2AstQualifiedName *target = parse_feature_chain(parser);
                if (target) {
                    Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
                    rel->kind = SYSML2_REL_TYPED_BY;
                    rel->target = target;
                    rel->next = NULL;
                    feat->relationships = rel;
                }
            }

            match(parser, SYSML2_TOKEN_SEMICOLON);
            feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
            member->node = feat;
        } else {
            /* Other 'not' prefixed constructs - treat as feature */
            member->kind = SYSML2_AST_FEATURE;
            member->node = parse_feature(parser);
        }
    }
    /* Return: return : Type; */
    else if (match(parser, SYSML2_TOKEN_KW_RETURN)) {
        member->kind = SYSML2_AST_RETURN;
        member->node = parse_feature(parser);
    }
    /* Allocate statement: allocate l to p; */
    else if (match(parser, SYSML2_TOKEN_KW_ALLOCATE)) {
        member->kind = SYSML2_AST_ALLOCATE;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Parse source feature chain */
        if (is_identifier(parser)) {
            Sysml2AstQualifiedName *source = parse_feature_chain(parser);
            if (source && source->segment_count > 0) {
                feat->name = source->segments[0];
            }
        }

        /* Parse 'to' clause */
        if (match(parser, SYSML2_TOKEN_KW_TO)) {
            Sysml2AstQualifiedName *target = parse_feature_chain(parser);
            if (target) {
                Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
                rel->kind = SYSML2_REL_TYPED_BY;
                rel->target = target;
                rel->next = NULL;
                feat->relationships = rel;
            }
        } else if (CHECK(parser, SYSML2_TOKEN_LPAREN)) {
            /* Parse allocate (...) with bindings */
            advance(parser);  /* Skip ( */
            while (!CHECK(parser, SYSML2_TOKEN_RPAREN) && !IS_AT_END(parser)) {
                advance(parser);
            }
            match(parser, SYSML2_TOKEN_RPAREN);
        }

        match(parser, SYSML2_TOKEN_SEMICOLON);
        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Dependency: dependency from X to Y; */
    else if (match(parser, SYSML2_TOKEN_KW_DEPENDENCY)) {
        member->kind = SYSML2_AST_DEPENDENCY;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Optional name */
        if (is_identifier(parser) && !CHECK(parser, SYSML2_TOKEN_KW_FROM)) {
            /* Check if this is a name or the start of 'from' */
            advance(parser);
            if (CHECK(parser, SYSML2_TOKEN_KW_FROM) || CHECK(parser, SYSML2_TOKEN_KW_TO)) {
                feat->name = get_name(parser);
            } else {
                /* It's actually the first path segment - unparse */
                /* Store as relationships */
                Sysml2AstQualifiedName *source = SYSML2_ARENA_NEW(parser->arena, Sysml2AstQualifiedName);
                source->segments = SYSML2_ARENA_NEW_ARRAY(parser->arena, const char *, 1);
                source->segments[0] = get_name(parser);
                source->segment_count = 1;
                feat->name = source->segments[0];
            }
        }

        /* Parse 'from' clause */
        if (match(parser, SYSML2_TOKEN_KW_FROM)) {
            Sysml2AstQualifiedName *source = parse_qualified_name(parser);
            (void)source;
        }

        /* Parse 'to' clause with potentially multiple targets */
        if (match(parser, SYSML2_TOKEN_KW_TO)) {
            Sysml2AstRelationship *first_rel = NULL;
            Sysml2AstRelationship *last_rel = NULL;

            do {
                Sysml2AstQualifiedName *target = parse_qualified_name(parser);
                if (target) {
                    Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
                    rel->kind = SYSML2_REL_TYPED_BY;
                    rel->target = target;
                    rel->next = NULL;
                    if (!first_rel) {
                        first_rel = rel;
                        last_rel = rel;
                    } else {
                        last_rel->next = rel;
                        last_rel = rel;
                    }
                }
            } while (match(parser, SYSML2_TOKEN_COMMA));

            feat->relationships = first_rel;
        }

        match(parser, SYSML2_TOKEN_SEMICOLON);
        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Concern: concern def C { ... } or concern c : C { ... } */
    else if (match(parser, SYSML2_TOKEN_KW_CONCERN)) {
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_CONCERN_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_CONCERN_DEF);
        } else {
            member->kind = SYSML2_AST_CONCERN_USAGE;
            member->node = parse_feature(parser);
        }
    }
    /* Stakeholder: stakeholder s : S; */
    else if (match(parser, SYSML2_TOKEN_KW_STAKEHOLDER)) {
        member->kind = SYSML2_AST_STAKEHOLDER;
        member->node = parse_feature(parser);
    }
    /* Frame: frame c; */
    else if (match(parser, SYSML2_TOKEN_KW_FRAME)) {
        member->kind = SYSML2_AST_FRAME;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Optional 'concern' keyword */
        match(parser, SYSML2_TOKEN_KW_CONCERN);

        /* Parse name/reference */
        if (is_identifier(parser)) {
            Sysml2AstQualifiedName *chain = parse_feature_chain(parser);
            if (chain && chain->segment_count > 0) {
                feat->name = chain->segments[0];
            }
        }

        match(parser, SYSML2_TOKEN_SEMICOLON);
        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Expose: expose P::*; */
    else if (match(parser, SYSML2_TOKEN_KW_EXPOSE)) {
        member->kind = SYSML2_AST_EXPOSE;
        /* Reuse import parsing */
        Sysml2AstImport *imp = SYSML2_ARENA_NEW(parser->arena, Sysml2AstImport);
        imp->target = parse_qualified_name(parser);

        /* Check for ::* */
        if (match(parser, SYSML2_TOKEN_COLON_COLON)) {
            if (match(parser, SYSML2_TOKEN_STAR)) {
                imp->is_all = true;
            }
        }

        consume(parser, SYSML2_TOKEN_SEMICOLON, "expected ';' after expose");
        member->node = imp;
    }
    /* Render: render r; */
    else if (match(parser, SYSML2_TOKEN_KW_RENDERING)) {
        /* Check if it's 'rendering def' */
        if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            member->kind = SYSML2_AST_RENDERING_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_RENDERING_DEF);
        } else {
            member->kind = SYSML2_AST_RENDERING_USAGE;
            member->node = parse_feature(parser);
        }
    }
    /* Render statement: render r; or render rendering r; */
    else if (match(parser, SYSML2_TOKEN_KW_RENDER)) {
        member->kind = SYSML2_AST_RENDER;
        /* Skip optional 'rendering' keyword */
        match(parser, SYSML2_TOKEN_KW_RENDERING);
        member->node = parse_feature(parser);
    }
    /* Verify: verify r; */
    else if (match(parser, SYSML2_TOKEN_KW_VERIFY)) {
        member->kind = SYSML2_AST_VERIFY;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Optional 'requirement' keyword */
        match(parser, SYSML2_TOKEN_KW_REQUIREMENT);

        /* Parse name/reference */
        feat->relationships = parse_relationships(parser);

        if (is_identifier(parser)) {
            Sysml2AstQualifiedName *chain = parse_feature_chain(parser);
            if (chain && chain->segment_count > 0) {
                feat->name = chain->segments[0];
            }
        }

        match(parser, SYSML2_TOKEN_SEMICOLON);
        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Rep: rep inOCL language "ocl" (comment block) */
    else if (match(parser, SYSML2_TOKEN_KW_REP)) {
        member->kind = SYSML2_AST_REP;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Parse optional name */
        if (is_identifier(parser)) {
            advance(parser);
            feat->name = get_name(parser);
        }

        /* Skip to language keyword or end */
        if (match(parser, SYSML2_TOKEN_KW_LANGUAGE)) {
            /* Parse language string */
            if (CHECK(parser, SYSML2_TOKEN_STRING)) {
                advance(parser);
            }
            /* Parse body (comment block) */
            if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
                feat->members = parse_body(parser);
            }
        }

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Language: language "alf" (comment block) */
    else if (match(parser, SYSML2_TOKEN_KW_LANGUAGE)) {
        member->kind = SYSML2_AST_LANGUAGE;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Parse language string */
        if (CHECK(parser, SYSML2_TOKEN_STRING)) {
            advance(parser);
            feat->name = sysml2_intern_n(
                parser->lexer->intern,
                PREVIOUS(parser).text.data + 1,
                PREVIOUS(parser).text.length - 2
            );
        }

        /* Parse body (comment block) */
        if (CHECK(parser, SYSML2_TOKEN_LBRACE)) {
            feat->members = parse_body(parser);
        }

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Connect: connect p to y; */
    else if (match(parser, SYSML2_TOKEN_KW_CONNECT)) {
        member->kind = SYSML2_AST_CONNECT;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Check for connect (...) syntax */
        if (match(parser, SYSML2_TOKEN_LPAREN)) {
            /* Parse list of connected parts */
            while (!CHECK(parser, SYSML2_TOKEN_RPAREN) && !IS_AT_END(parser)) {
                if (is_identifier(parser)) {
                    parse_feature_chain(parser);
                }
                if (!match(parser, SYSML2_TOKEN_COMMA)) break;
            }
            match(parser, SYSML2_TOKEN_RPAREN);
            match(parser, SYSML2_TOKEN_SEMICOLON);
        } else {
            /* Parse source feature chain */
            Sysml2AstQualifiedName *source = parse_feature_chain(parser);
            if (source && source->segment_count > 0) {
                feat->name = source->segments[0];
            }

            /* Parse 'to' and target */
            if (match(parser, SYSML2_TOKEN_KW_TO)) {
                Sysml2AstQualifiedName *target = parse_feature_chain(parser);
                if (target) {
                    Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
                    rel->kind = SYSML2_REL_TYPED_BY;
                    rel->target = target;
                    rel->next = NULL;
                    feat->relationships = rel;
                }
            }

            match(parser, SYSML2_TOKEN_SEMICOLON);
        }

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Message: message : F from p to p; */
    else if (match(parser, SYSML2_TOKEN_KW_MESSAGE)) {
        member->kind = SYSML2_AST_MESSAGE;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Optional name */
        if (is_identifier(parser)) {
            advance(parser);
            feat->name = get_name(parser);
        }

        /* Parse relationships */
        feat->relationships = parse_relationships(parser);

        /* Parse 'from ... to ...' */
        if (match(parser, SYSML2_TOKEN_KW_FROM)) {
            parse_feature_chain(parser);
        }
        if (match(parser, SYSML2_TOKEN_KW_TO)) {
            parse_feature_chain(parser);
        }

        match(parser, SYSML2_TOKEN_SEMICOLON);
        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Locale standalone: locale "en_US" (comment block) */
    else if (match(parser, SYSML2_TOKEN_KW_LOCALE)) {
        member->kind = SYSML2_AST_COMMENT;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstComment *comment = SYSML2_ARENA_NEW(parser->arena, Sysml2AstComment);

        /* Parse locale string */
        if (CHECK(parser, SYSML2_TOKEN_STRING)) {
            advance(parser);
            comment->locale = sysml2_intern_n(
                parser->lexer->intern,
                PREVIOUS(parser).text.data + 1,
                PREVIOUS(parser).text.length - 2
            );
        }

        /* Skip comment body */
        while (!IS_AT_END(parser) && !CHECK(parser, SYSML2_TOKEN_SEMICOLON) &&
               !CHECK(parser, SYSML2_TOKEN_RBRACE) &&
               !CHECK(parser, SYSML2_TOKEN_KW_DOC) &&
               !CHECK(parser, SYSML2_TOKEN_KW_COMMENT) &&
               !CHECK(parser, SYSML2_TOKEN_KW_PART) &&
               !CHECK(parser, SYSML2_TOKEN_KW_PACKAGE)) {
            advance(parser);
        }

        comment->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = comment;
    }
    /* Terminate: terminate; or terminate name; */
    else if (match(parser, SYSML2_TOKEN_KW_TERMINATE)) {
        member->kind = SYSML2_AST_TERMINATE;
        Sysml2SourceLoc start = PREVIOUS(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        /* Optional name */
        if (is_identifier(parser)) {
            advance(parser);
            feat->name = get_name(parser);
        }

        match(parser, SYSML2_TOKEN_SEMICOLON);
        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Individual prefix - for individual occurrence/item/part/action */
    else if (match(parser, SYSML2_TOKEN_KW_INDIVIDUAL)) {
        if (match(parser, SYSML2_TOKEN_KW_OCCURRENCE)) {
            if (match(parser, SYSML2_TOKEN_KW_DEF)) {
                member->kind = SYSML2_AST_INDIVIDUAL_DEF;
                member->node = parse_classifier(parser, SYSML2_AST_INDIVIDUAL_DEF);
            } else {
                member->kind = SYSML2_AST_INDIVIDUAL_USAGE;
                member->node = parse_feature(parser);
            }
        } else if (match(parser, SYSML2_TOKEN_KW_ITEM)) {
            if (match(parser, SYSML2_TOKEN_KW_DEF)) {
                member->kind = SYSML2_AST_INDIVIDUAL_DEF;
                member->node = parse_classifier(parser, SYSML2_AST_INDIVIDUAL_DEF);
            } else {
                member->kind = SYSML2_AST_INDIVIDUAL_USAGE;
                member->node = parse_feature(parser);
            }
        } else if (match(parser, SYSML2_TOKEN_KW_PART)) {
            if (match(parser, SYSML2_TOKEN_KW_DEF)) {
                member->kind = SYSML2_AST_INDIVIDUAL_DEF;
                member->node = parse_classifier(parser, SYSML2_AST_INDIVIDUAL_DEF);
            } else {
                member->kind = SYSML2_AST_INDIVIDUAL_USAGE;
                member->node = parse_feature(parser);
            }
        } else if (match(parser, SYSML2_TOKEN_KW_ACTION)) {
            if (match(parser, SYSML2_TOKEN_KW_DEF)) {
                member->kind = SYSML2_AST_INDIVIDUAL_DEF;
                member->node = parse_classifier(parser, SYSML2_AST_INDIVIDUAL_DEF);
            } else {
                member->kind = SYSML2_AST_INDIVIDUAL_USAGE;
                member->node = parse_feature(parser);
            }
        } else if (match(parser, SYSML2_TOKEN_KW_DEF)) {
            /* Just 'individual def' */
            member->kind = SYSML2_AST_INDIVIDUAL_DEF;
            member->node = parse_classifier(parser, SYSML2_AST_INDIVIDUAL_DEF);
        } else {
            /* Just 'individual' as a prefix - treat as feature */
            member->kind = SYSML2_AST_INDIVIDUAL_USAGE;
            member->node = parse_feature(parser);
        }
    }
    /* Bare value assignment: = value; (used in enum defs) */
    else if (CHECK(parser, SYSML2_TOKEN_EQ)) {
        member->kind = SYSML2_AST_ENUM_USAGE;
        Sysml2SourceLoc start = CURRENT(parser).range.start;
        Sysml2AstFeature *feat = SYSML2_ARENA_NEW(parser->arena, Sysml2AstFeature);

        advance(parser);  /* Skip = */
        feat->default_value = sysml2_parser_parse_expression(parser);
        match(parser, SYSML2_TOKEN_SEMICOLON);

        feat->range = sysml2_range_from_locs(start, PREVIOUS(parser).range.end);
        member->node = feat;
    }
    /* Handle identifiers at start (could be a feature without keyword) */
    /* Also handle shorthand relationship syntax where feature starts with :>> or : */
    else if (is_identifier(parser) || direction != SYSML2_DIR_NONE ||
             CHECK(parser, SYSML2_TOKEN_COLON_GT_GT) ||
             CHECK(parser, SYSML2_TOKEN_COLON_GT) ||
             CHECK(parser, SYSML2_TOKEN_COLON_COLON_GT) ||
             CHECK(parser, SYSML2_TOKEN_COLON) ||
             prefix.is_ref || prefix.is_abstract || prefix.is_derived ||
             prefix.is_readonly || prefix.is_composite || prefix.is_portion || prefix.is_end) {
        member->kind = SYSML2_AST_FEATURE;
        Sysml2AstFeature *feat = parse_feature(parser);
        if (feat) {
            feat->prefix = prefix;
            feat->direction = direction;
        }
        member->node = feat;
    } else {
        /* Unexpected token */
        Sysml2Diagnostic *diag = sysml2_diag_create(
            parser->diag,
            SYSML2_DIAG_E2006_UNEXPECTED_TOKEN,
            SYSML2_SEVERITY_ERROR,
            parser->lexer->source,
            CURRENT(parser).range,
            "unexpected token in namespace body"
        );
        sysml2_diag_emit(parser->diag, diag);
        parser->had_error = true;
        parser->panic_mode = true;
        advance(parser);
        return NULL;
    }

    return member;
}

Sysml2AstMember *sysml2_parser_parse_member(Sysml2Parser *parser) {
    return parse_namespace_body_element(parser);
}

Sysml2AstNamespace *sysml2_parser_parse(Sysml2Parser *parser) {
    Sysml2AstNamespace *root = SYSML2_ARENA_NEW(parser->arena, Sysml2AstNamespace);
    root->name = NULL; /* Root namespace has no name */

    Sysml2AstMember *first = NULL;
    Sysml2AstMember *last = NULL;

    while (!IS_AT_END(parser)) {
        Sysml2AstMember *member = parse_namespace_body_element(parser);
        if (member) {
            if (!first) {
                first = member;
                last = member;
            } else {
                last->next = member;
                last = member;
            }
        } else if (parser->panic_mode) {
            sysml2_parser_synchronize(parser, SYSML2_SYNC_NAMESPACE);
        }

        if (sysml2_diag_should_stop(parser->diag)) {
            break;
        }
    }

    root->members = first;
    return root;
}
