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
        Sysml2SourceLoc start = CURRENT(parser).range.start;

        if (match(parser, SYSML2_TOKEN_COLON_GT_GT)) {
            kind = SYSML2_REL_REDEFINES;
        } else if (match(parser, SYSML2_TOKEN_COLON_GT)) {
            kind = SYSML2_REL_SPECIALIZES;
        } else if (match(parser, SYSML2_TOKEN_COLON_COLON_GT)) {
            kind = SYSML2_REL_SUBSETS;
        } else if (match(parser, SYSML2_TOKEN_COLON)) {
            kind = SYSML2_REL_TYPED_BY;
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
        } else {
            break;
        }

        Sysml2AstQualifiedName *target = parse_qualified_name(parser);
        if (!target) break;

        Sysml2AstRelationship *rel = SYSML2_ARENA_NEW(parser->arena, Sysml2AstRelationship);
        rel->kind = kind;
        rel->target = target;
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

    import->target = parse_qualified_name(parser);

    /* Check for ::* or ::** */
    if (match(parser, SYSML2_TOKEN_COLON_COLON)) {
        if (match(parser, SYSML2_TOKEN_STAR)) {
            if (match(parser, SYSML2_TOKEN_STAR)) {
                import->is_recursive = true;
            }
            import->is_all = true;
        }
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

/* Parse a namespace body element */
static Sysml2AstMember *parse_namespace_body_element(Sysml2Parser *parser) {
    Sysml2Visibility visibility = parse_visibility(parser);
    Sysml2TypePrefix prefix = parse_type_prefix(parser);
    Sysml2Direction direction = parse_direction(parser);

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
        member->kind = SYSML2_AST_SUCCESSION;
        member->node = parse_feature(parser);
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
    }
    /* Handle identifiers at start (could be a feature without keyword) */
    else if (is_identifier(parser) || direction != SYSML2_DIR_NONE) {
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
