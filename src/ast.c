/*
 * SysML v2 Parser - AST Utilities Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/ast.h"
#include "sysml2/arena.h"
#include <stdio.h>
#include <string.h>

/* Create a new qualified name */
Sysml2AstQualifiedName *sysml2_ast_qname_create(
    Sysml2Arena *arena,
    const char **segments,
    size_t count,
    bool is_global
) {
    Sysml2AstQualifiedName *qname = SYSML2_ARENA_NEW(arena, Sysml2AstQualifiedName);
    qname->segments = SYSML2_ARENA_NEW_ARRAY(arena, const char *, count);
    for (size_t i = 0; i < count; i++) {
        qname->segments[i] = segments[i];
    }
    qname->segment_count = count;
    qname->is_global = is_global;
    return qname;
}

/* Get qualified name as string (for display) */
char *sysml2_ast_qname_to_string(Sysml2Arena *arena, const Sysml2AstQualifiedName *name) {
    if (!name || name->segment_count == 0) {
        return sysml2_arena_strdup(arena, "");
    }

    /* Calculate total length */
    size_t total = name->is_global ? 2 : 0; /* :: prefix */
    for (size_t i = 0; i < name->segment_count; i++) {
        total += strlen(name->segments[i]);
        if (i < name->segment_count - 1) {
            total += 2; /* :: separator */
        }
    }

    char *result = sysml2_arena_alloc(arena, total + 1);
    char *p = result;

    if (name->is_global) {
        *p++ = ':';
        *p++ = ':';
    }

    for (size_t i = 0; i < name->segment_count; i++) {
        size_t len = strlen(name->segments[i]);
        memcpy(p, name->segments[i], len);
        p += len;
        if (i < name->segment_count - 1) {
            *p++ = ':';
            *p++ = ':';
        }
    }
    *p = '\0';

    return result;
}

/* Expression constructors */
Sysml2AstExpr *sysml2_ast_expr_int(Sysml2Arena *arena, int64_t value, Sysml2SourceRange range) {
    Sysml2AstExpr *expr = SYSML2_ARENA_NEW(arena, Sysml2AstExpr);
    expr->kind = SYSML2_EXPR_LITERAL_INT;
    expr->range = range;
    expr->int_value = value;
    return expr;
}

Sysml2AstExpr *sysml2_ast_expr_real(Sysml2Arena *arena, double value, Sysml2SourceRange range) {
    Sysml2AstExpr *expr = SYSML2_ARENA_NEW(arena, Sysml2AstExpr);
    expr->kind = SYSML2_EXPR_LITERAL_REAL;
    expr->range = range;
    expr->real_value = value;
    return expr;
}

Sysml2AstExpr *sysml2_ast_expr_string(Sysml2Arena *arena, const char *value, Sysml2SourceRange range) {
    Sysml2AstExpr *expr = SYSML2_ARENA_NEW(arena, Sysml2AstExpr);
    expr->kind = SYSML2_EXPR_LITERAL_STRING;
    expr->range = range;
    expr->string_value = value;
    return expr;
}

Sysml2AstExpr *sysml2_ast_expr_bool(Sysml2Arena *arena, bool value, Sysml2SourceRange range) {
    Sysml2AstExpr *expr = SYSML2_ARENA_NEW(arena, Sysml2AstExpr);
    expr->kind = SYSML2_EXPR_LITERAL_BOOL;
    expr->range = range;
    expr->bool_value = value;
    return expr;
}

Sysml2AstExpr *sysml2_ast_expr_null(Sysml2Arena *arena, Sysml2SourceRange range) {
    Sysml2AstExpr *expr = SYSML2_ARENA_NEW(arena, Sysml2AstExpr);
    expr->kind = SYSML2_EXPR_LITERAL_NULL;
    expr->range = range;
    return expr;
}

Sysml2AstExpr *sysml2_ast_expr_name(Sysml2Arena *arena, Sysml2AstQualifiedName *name) {
    Sysml2AstExpr *expr = SYSML2_ARENA_NEW(arena, Sysml2AstExpr);
    expr->kind = SYSML2_EXPR_NAME;
    expr->range = name->range;
    expr->name = name;
    return expr;
}

Sysml2AstExpr *sysml2_ast_expr_binary(
    Sysml2Arena *arena,
    Sysml2BinaryOp op,
    Sysml2AstExpr *left,
    Sysml2AstExpr *right,
    Sysml2SourceRange range
) {
    Sysml2AstExpr *expr = SYSML2_ARENA_NEW(arena, Sysml2AstExpr);
    expr->kind = SYSML2_EXPR_BINARY;
    expr->range = range;
    expr->binary.op = op;
    expr->binary.left = left;
    expr->binary.right = right;
    return expr;
}

Sysml2AstExpr *sysml2_ast_expr_unary(
    Sysml2Arena *arena,
    Sysml2UnaryOp op,
    Sysml2AstExpr *operand,
    Sysml2SourceRange range
) {
    Sysml2AstExpr *expr = SYSML2_ARENA_NEW(arena, Sysml2AstExpr);
    expr->kind = SYSML2_EXPR_UNARY;
    expr->range = range;
    expr->unary.op = op;
    expr->unary.operand = operand;
    return expr;
}

/* Get kind name as string */
const char *sysml2_ast_kind_to_string(Sysml2AstKind kind) {
    switch (kind) {
        case SYSML2_AST_NAMESPACE: return "namespace";
        case SYSML2_AST_PACKAGE: return "package";
        case SYSML2_AST_IMPORT: return "import";
        case SYSML2_AST_ALIAS: return "alias";
        case SYSML2_AST_TYPE: return "type";
        case SYSML2_AST_CLASSIFIER: return "classifier";
        case SYSML2_AST_CLASS: return "class";
        case SYSML2_AST_DATATYPE: return "datatype";
        case SYSML2_AST_STRUCT: return "struct";
        case SYSML2_AST_ASSOC: return "assoc";
        case SYSML2_AST_FEATURE: return "feature";
        case SYSML2_AST_CONNECTOR: return "connector";
        case SYSML2_AST_BINDING: return "binding";
        case SYSML2_AST_SUCCESSION: return "succession";
        case SYSML2_AST_BEHAVIOR: return "behavior";
        case SYSML2_AST_FUNCTION: return "function";
        case SYSML2_AST_PREDICATE: return "predicate";
        case SYSML2_AST_INTERACTION: return "interaction";
        case SYSML2_AST_COMMENT: return "comment";
        case SYSML2_AST_DOCUMENTATION: return "doc";
        case SYSML2_AST_METADATA: return "metadata";
        case SYSML2_AST_MULTIPLICITY: return "multiplicity";
        case SYSML2_AST_EXPR: return "expr";
        case SYSML2_AST_PART_DEF: return "part def";
        case SYSML2_AST_PART_USAGE: return "part";
        case SYSML2_AST_ACTION_DEF: return "action def";
        case SYSML2_AST_ACTION_USAGE: return "action";
        case SYSML2_AST_STATE_DEF: return "state def";
        case SYSML2_AST_STATE_USAGE: return "state";
        case SYSML2_AST_REQUIREMENT_DEF: return "requirement def";
        case SYSML2_AST_REQUIREMENT_USAGE: return "requirement";
        case SYSML2_AST_CONSTRAINT_DEF: return "constraint def";
        case SYSML2_AST_CONSTRAINT_USAGE: return "constraint";
        case SYSML2_AST_PORT_DEF: return "port def";
        case SYSML2_AST_PORT_USAGE: return "port";
        case SYSML2_AST_INTERFACE_DEF: return "interface def";
        case SYSML2_AST_CONNECTION_DEF: return "connection def";
        case SYSML2_AST_FLOW_DEF: return "flow def";
        case SYSML2_AST_ATTRIBUTE_DEF: return "attribute def";
        case SYSML2_AST_ATTRIBUTE_USAGE: return "attribute";
        case SYSML2_AST_ITEM_DEF: return "item def";
        case SYSML2_AST_ITEM_USAGE: return "item";
        case SYSML2_AST_VIEW_DEF: return "view def";
        case SYSML2_AST_VIEWPOINT_DEF: return "viewpoint def";
        case SYSML2_AST_INTERFACE_USAGE: return "interface";
        case SYSML2_AST_CONNECTION_USAGE: return "connection";
        case SYSML2_AST_FLOW_USAGE: return "flow";
        case SYSML2_AST_VIEW_USAGE: return "view";
        case SYSML2_AST_VIEWPOINT_USAGE: return "viewpoint";
        case SYSML2_AST_ENUM_DEF: return "enum def";
        case SYSML2_AST_ENUM_USAGE: return "enum";
        case SYSML2_AST_CALC_DEF: return "calc def";
        case SYSML2_AST_CALC_USAGE: return "calc";
        case SYSML2_AST_CASE_DEF: return "case def";
        case SYSML2_AST_CASE_USAGE: return "case";
        case SYSML2_AST_ANALYSIS_DEF: return "analysis def";
        case SYSML2_AST_ANALYSIS_USAGE: return "analysis";
        case SYSML2_AST_VERIFICATION_DEF: return "verification def";
        case SYSML2_AST_VERIFICATION_USAGE: return "verification";
        case SYSML2_AST_USECASE_DEF: return "use case def";
        case SYSML2_AST_USECASE_USAGE: return "use case";
        case SYSML2_AST_ALLOCATION_DEF: return "allocation def";
        case SYSML2_AST_ALLOCATION_USAGE: return "allocation";
        case SYSML2_AST_RENDERING_DEF: return "rendering def";
        case SYSML2_AST_RENDERING_USAGE: return "rendering";
        case SYSML2_AST_OCCURRENCE_DEF: return "occurrence def";
        case SYSML2_AST_OCCURRENCE_USAGE: return "occurrence";
        case SYSML2_AST_TRANSITION: return "transition";
        case SYSML2_AST_ENTRY_ACTION: return "entry";
        case SYSML2_AST_EXIT_ACTION: return "exit";
        case SYSML2_AST_DO_ACTION: return "do";
        case SYSML2_AST_THEN: return "then";
        case SYSML2_AST_JOIN_NODE: return "join";
        case SYSML2_AST_FORK_NODE: return "fork";
        case SYSML2_AST_MERGE_NODE: return "merge";
        case SYSML2_AST_FIRST: return "first";
        case SYSML2_AST_INDIVIDUAL_DEF: return "individual def";
        case SYSML2_AST_INDIVIDUAL_USAGE: return "individual";
        case SYSML2_AST_PERFORM: return "perform";
        case SYSML2_AST_SUCCESSION_FLOW: return "succession flow";
        case SYSML2_AST_BIND: return "bind";
        case SYSML2_AST_ACCEPT: return "accept";
        case SYSML2_AST_SEND: return "send";
        case SYSML2_AST_DECIDE: return "decide";
        case SYSML2_AST_TERMINATE: return "terminate";
        case SYSML2_AST_ASSIGN: return "assign";
        case SYSML2_AST_IF_ACTION: return "if";
        case SYSML2_AST_WHILE_LOOP: return "while";
        case SYSML2_AST_FOR_LOOP: return "for";
        case SYSML2_AST_LOOP: return "loop";
        case SYSML2_AST_SUBJECT: return "subject";
        case SYSML2_AST_ACTOR: return "actor";
        case SYSML2_AST_OBJECTIVE: return "objective";
        case SYSML2_AST_SATISFY: return "satisfy";
        case SYSML2_AST_ASSUME: return "assume";
        case SYSML2_AST_ASSERT: return "assert";
        case SYSML2_AST_INCLUDE: return "include";
        case SYSML2_AST_RETURN: return "return";
        case SYSML2_AST_ALLOCATE: return "allocate";
        case SYSML2_AST_DEPENDENCY: return "dependency";
        case SYSML2_AST_CONCERN_DEF: return "concern def";
        case SYSML2_AST_CONCERN_USAGE: return "concern";
        case SYSML2_AST_STAKEHOLDER: return "stakeholder";
        case SYSML2_AST_FRAME: return "frame";
        case SYSML2_AST_EXPOSE: return "expose";
        case SYSML2_AST_RENDER: return "render";
        case SYSML2_AST_VERIFY: return "verify";
        case SYSML2_AST_REP: return "rep";
        case SYSML2_AST_LANGUAGE: return "language";
        case SYSML2_AST_CONNECT: return "connect";
        case SYSML2_AST_MESSAGE: return "message";
        default: return "unknown";
    }
}

/* Print indentation */
static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

/* Print prefix flags */
static void print_prefix(Sysml2TypePrefix prefix) {
    if (prefix.is_abstract) printf("abstract ");
    if (prefix.is_readonly) printf("readonly ");
    if (prefix.is_derived) printf("derived ");
    if (prefix.is_end) printf("end ");
    if (prefix.is_composite) printf("composite ");
    if (prefix.is_portion) printf("portion ");
    if (prefix.is_ref) printf("ref ");
}

/* Print direction */
static void print_direction(Sysml2Direction dir) {
    switch (dir) {
        case SYSML2_DIR_IN: printf("in "); break;
        case SYSML2_DIR_OUT: printf("out "); break;
        case SYSML2_DIR_INOUT: printf("inout "); break;
        default: break;
    }
}

/* Print relationships */
static void print_relationships(Sysml2AstRelationship *rel) {
    while (rel) {
        switch (rel->kind) {
            case SYSML2_REL_SPECIALIZES: printf(" :> "); break;
            case SYSML2_REL_TYPED_BY: printf(" : "); break;
            case SYSML2_REL_SUBSETS: printf(" ::> "); break;
            case SYSML2_REL_REDEFINES: printf(" :>> "); break;
            case SYSML2_REL_CONJUGATES: printf(" ~ "); break;
            case SYSML2_REL_REFERENCES: printf(" references "); break;
        }
        if (rel->target) {
            for (size_t i = 0; i < rel->target->segment_count; i++) {
                if (i > 0) printf("::");
                printf("%s", rel->target->segments[i]);
            }
        }
        rel = rel->next;
    }
}

/* Print multiplicity */
static void print_multiplicity(Sysml2AstMultiplicity *mult) {
    if (!mult) return;
    printf("[");
    if (mult->lower) {
        if (mult->lower->kind == SYSML2_EXPR_LITERAL_INT) {
            printf("%lld", (long long)mult->lower->int_value);
        }
    } else {
        printf("0");
    }
    printf("..");
    if (mult->upper) {
        if (mult->upper->kind == SYSML2_EXPR_LITERAL_INT) {
            printf("%lld", (long long)mult->upper->int_value);
        }
    } else {
        printf("*");
    }
    if (mult->is_ordered) printf(" ordered");
    if (mult->is_nonunique) printf(" nonunique");
    printf("]");
}

/* Print members recursively */
void sysml2_ast_print_members(Sysml2AstMember *members, int indent) {
    for (Sysml2AstMember *m = members; m; m = m->next) {
        print_indent(indent);
        printf("%s", sysml2_ast_kind_to_string(m->kind));

        switch (m->kind) {
            case SYSML2_AST_NAMESPACE: {
                Sysml2AstNamespace *ns = (Sysml2AstNamespace *)m->node;
                if (ns && ns->name) printf(" %s", ns->name);
                printf("\n");
                if (ns && ns->members) {
                    sysml2_ast_print_members(ns->members, indent + 1);
                }
                break;
            }
            case SYSML2_AST_PACKAGE: {
                Sysml2AstPackage *pkg = (Sysml2AstPackage *)m->node;
                if (pkg) {
                    if (pkg->is_library) printf(" (library)");
                    if (pkg->name) printf(" %s", pkg->name);
                }
                printf("\n");
                if (pkg && pkg->members) {
                    sysml2_ast_print_members(pkg->members, indent + 1);
                }
                break;
            }
            case SYSML2_AST_IMPORT: {
                Sysml2AstImport *imp = (Sysml2AstImport *)m->node;
                if (imp && imp->target) {
                    printf(" ");
                    for (size_t i = 0; i < imp->target->segment_count; i++) {
                        if (i > 0) printf("::");
                        printf("%s", imp->target->segments[i]);
                    }
                    if (imp->is_all) printf("::*");
                    if (imp->is_recursive) printf("*");
                }
                printf("\n");
                break;
            }
            case SYSML2_AST_ALIAS: {
                Sysml2AstAlias *alias = (Sysml2AstAlias *)m->node;
                if (alias && alias->name) printf(" %s", alias->name);
                printf("\n");
                break;
            }
            case SYSML2_AST_TYPE:
            case SYSML2_AST_CLASSIFIER:
            case SYSML2_AST_CLASS:
            case SYSML2_AST_DATATYPE:
            case SYSML2_AST_STRUCT:
            case SYSML2_AST_ASSOC:
            case SYSML2_AST_BEHAVIOR:
            case SYSML2_AST_FUNCTION:
            case SYSML2_AST_PREDICATE:
            case SYSML2_AST_PART_DEF:
            case SYSML2_AST_ACTION_DEF:
            case SYSML2_AST_STATE_DEF:
            case SYSML2_AST_REQUIREMENT_DEF:
            case SYSML2_AST_CONSTRAINT_DEF:
            case SYSML2_AST_PORT_DEF:
            case SYSML2_AST_ATTRIBUTE_DEF:
            case SYSML2_AST_ITEM_DEF:
            case SYSML2_AST_INTERFACE_DEF:
            case SYSML2_AST_CONNECTION_DEF:
            case SYSML2_AST_FLOW_DEF:
            case SYSML2_AST_VIEW_DEF:
            case SYSML2_AST_VIEWPOINT_DEF:
            case SYSML2_AST_ENUM_DEF:
            case SYSML2_AST_CALC_DEF:
            case SYSML2_AST_CASE_DEF:
            case SYSML2_AST_ANALYSIS_DEF:
            case SYSML2_AST_VERIFICATION_DEF:
            case SYSML2_AST_USECASE_DEF:
            case SYSML2_AST_ALLOCATION_DEF:
            case SYSML2_AST_RENDERING_DEF:
            case SYSML2_AST_OCCURRENCE_DEF:
            case SYSML2_AST_INDIVIDUAL_DEF: {
                Sysml2AstClassifier *cls = (Sysml2AstClassifier *)m->node;
                if (cls) {
                    printf(" ");
                    print_prefix(cls->prefix);
                    if (cls->name) printf("%s", cls->name);
                    print_multiplicity(cls->multiplicity);
                    print_relationships(cls->relationships);
                }
                printf("\n");
                if (cls && cls->members) {
                    sysml2_ast_print_members(cls->members, indent + 1);
                }
                break;
            }
            case SYSML2_AST_FEATURE:
            case SYSML2_AST_CONNECTOR:
            case SYSML2_AST_BINDING:
            case SYSML2_AST_SUCCESSION:
            case SYSML2_AST_PART_USAGE:
            case SYSML2_AST_ACTION_USAGE:
            case SYSML2_AST_STATE_USAGE:
            case SYSML2_AST_REQUIREMENT_USAGE:
            case SYSML2_AST_CONSTRAINT_USAGE:
            case SYSML2_AST_PORT_USAGE:
            case SYSML2_AST_ATTRIBUTE_USAGE:
            case SYSML2_AST_ITEM_USAGE:
            case SYSML2_AST_INTERFACE_USAGE:
            case SYSML2_AST_CONNECTION_USAGE:
            case SYSML2_AST_FLOW_USAGE:
            case SYSML2_AST_VIEW_USAGE:
            case SYSML2_AST_VIEWPOINT_USAGE:
            case SYSML2_AST_ENUM_USAGE:
            case SYSML2_AST_CALC_USAGE:
            case SYSML2_AST_CASE_USAGE:
            case SYSML2_AST_ANALYSIS_USAGE:
            case SYSML2_AST_VERIFICATION_USAGE:
            case SYSML2_AST_USECASE_USAGE:
            case SYSML2_AST_ALLOCATION_USAGE:
            case SYSML2_AST_RENDERING_USAGE:
            case SYSML2_AST_OCCURRENCE_USAGE:
            case SYSML2_AST_TRANSITION:
            case SYSML2_AST_ENTRY_ACTION:
            case SYSML2_AST_EXIT_ACTION:
            case SYSML2_AST_DO_ACTION:
            case SYSML2_AST_THEN:
            case SYSML2_AST_JOIN_NODE:
            case SYSML2_AST_FORK_NODE:
            case SYSML2_AST_MERGE_NODE:
            case SYSML2_AST_FIRST:
            case SYSML2_AST_INDIVIDUAL_USAGE: {
                Sysml2AstFeature *feat = (Sysml2AstFeature *)m->node;
                if (feat) {
                    printf(" ");
                    print_direction(feat->direction);
                    print_prefix(feat->prefix);
                    if (feat->name) printf("%s", feat->name);
                    print_multiplicity(feat->multiplicity);
                    print_relationships(feat->relationships);
                }
                printf("\n");
                if (feat && feat->members) {
                    sysml2_ast_print_members(feat->members, indent + 1);
                }
                break;
            }
            case SYSML2_AST_COMMENT:
            case SYSML2_AST_DOCUMENTATION: {
                Sysml2AstComment *cmt = (Sysml2AstComment *)m->node;
                if (cmt && cmt->text) printf(" \"%s\"", cmt->text);
                printf("\n");
                break;
            }
            default:
                printf("\n");
                break;
        }
    }
}

/* Print AST for debugging */
void sysml2_ast_print(Sysml2AstNamespace *ast, int indent) {
    if (!ast) {
        printf("(null AST)\n");
        return;
    }

    print_indent(indent);
    printf("RootNamespace");
    if (ast->name) printf(" %s", ast->name);
    printf("\n");

    if (ast->members) {
        sysml2_ast_print_members(ast->members, indent + 1);
    }
}

/* JSON helpers */
static void json_string(FILE *out, const char *str) {
    if (!str) {
        fprintf(out, "null");
        return;
    }
    fprintf(out, "\"");
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"': fprintf(out, "\\\""); break;
            case '\\': fprintf(out, "\\\\"); break;
            case '\n': fprintf(out, "\\n"); break;
            case '\r': fprintf(out, "\\r"); break;
            case '\t': fprintf(out, "\\t"); break;
            default:
                if ((unsigned char)*p < 32) {
                    fprintf(out, "\\u%04x", (unsigned char)*p);
                } else {
                    fputc(*p, out);
                }
        }
    }
    fprintf(out, "\"");
}

static void json_range(FILE *out, Sysml2SourceRange range) {
    fprintf(out, "{\"start\":{\"line\":%u,\"column\":%u,\"offset\":%u},"
            "\"end\":{\"line\":%u,\"column\":%u,\"offset\":%u}}",
            range.start.line, range.start.column, range.start.offset,
            range.end.line, range.end.column, range.end.offset);
}

static void json_qname(FILE *out, Sysml2AstQualifiedName *name) {
    if (!name) {
        fprintf(out, "null");
        return;
    }
    fprintf(out, "{\"segments\":[");
    for (size_t i = 0; i < name->segment_count; i++) {
        if (i > 0) fprintf(out, ",");
        json_string(out, name->segments[i]);
    }
    fprintf(out, "],\"isGlobal\":%s}", name->is_global ? "true" : "false");
}

static void json_members(FILE *out, Sysml2AstMember *members);

static void json_member(FILE *out, Sysml2AstMember *m) {
    fprintf(out, "{\"kind\":");
    json_string(out, sysml2_ast_kind_to_string(m->kind));

    switch (m->kind) {
        case SYSML2_AST_PACKAGE: {
            Sysml2AstPackage *pkg = (Sysml2AstPackage *)m->node;
            if (pkg) {
                fprintf(out, ",\"name\":");
                json_string(out, pkg->name);
                fprintf(out, ",\"isLibrary\":%s", pkg->is_library ? "true" : "false");
                fprintf(out, ",\"range\":");
                json_range(out, pkg->range);
                if (pkg->members) {
                    fprintf(out, ",\"members\":");
                    json_members(out, pkg->members);
                }
            }
            break;
        }
        case SYSML2_AST_CLASS:
        case SYSML2_AST_DATATYPE:
        case SYSML2_AST_STRUCT:
        case SYSML2_AST_CLASSIFIER: {
            Sysml2AstClassifier *cls = (Sysml2AstClassifier *)m->node;
            if (cls) {
                fprintf(out, ",\"name\":");
                json_string(out, cls->name);
                fprintf(out, ",\"isAbstract\":%s", cls->prefix.is_abstract ? "true" : "false");
                fprintf(out, ",\"range\":");
                json_range(out, cls->range);
                if (cls->members) {
                    fprintf(out, ",\"members\":");
                    json_members(out, cls->members);
                }
            }
            break;
        }
        case SYSML2_AST_FEATURE: {
            Sysml2AstFeature *feat = (Sysml2AstFeature *)m->node;
            if (feat) {
                fprintf(out, ",\"name\":");
                json_string(out, feat->name);
                fprintf(out, ",\"range\":");
                json_range(out, feat->range);
            }
            break;
        }
        case SYSML2_AST_IMPORT: {
            Sysml2AstImport *imp = (Sysml2AstImport *)m->node;
            if (imp) {
                fprintf(out, ",\"target\":");
                json_qname(out, imp->target);
                fprintf(out, ",\"isAll\":%s", imp->is_all ? "true" : "false");
            }
            break;
        }
        default:
            break;
    }

    fprintf(out, "}");
}

static void json_members(FILE *out, Sysml2AstMember *members) {
    fprintf(out, "[");
    bool first = true;
    for (Sysml2AstMember *m = members; m; m = m->next) {
        if (!first) fprintf(out, ",");
        first = false;
        json_member(out, m);
    }
    fprintf(out, "]");
}

/* Convert AST to JSON */
void sysml2_ast_to_json(Sysml2AstNamespace *ast, FILE *output) {
    if (!ast) {
        fprintf(output, "null\n");
        return;
    }

    fprintf(output, "{\"kind\":\"RootNamespace\"");
    if (ast->name) {
        fprintf(output, ",\"name\":");
        json_string(output, ast->name);
    }
    if (ast->members) {
        fprintf(output, ",\"members\":");
        json_members(output, ast->members);
    }
    fprintf(output, "}\n");
}
