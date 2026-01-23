/*
 * SysML v2 Parser - Abstract Syntax Tree
 *
 * Tagged union AST nodes for KerML and SysML v2.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_AST_H
#define SYSML2_AST_H

#include "common.h"
#include "arena.h"
#include "token.h"
#include <stdio.h>

/* Forward declarations */
typedef struct Sysml2AstNode Sysml2AstNode;
typedef struct Sysml2AstNamespace Sysml2AstNamespace;
typedef struct Sysml2AstPackage Sysml2AstPackage;
typedef struct Sysml2AstImport Sysml2AstImport;
typedef struct Sysml2AstAlias Sysml2AstAlias;
typedef struct Sysml2AstType Sysml2AstType;
typedef struct Sysml2AstClassifier Sysml2AstClassifier;
typedef struct Sysml2AstFeature Sysml2AstFeature;
typedef struct Sysml2AstComment Sysml2AstComment;
typedef struct Sysml2AstDocumentation Sysml2AstDocumentation;
typedef struct Sysml2AstMultiplicity Sysml2AstMultiplicity;
typedef struct Sysml2AstExpr Sysml2AstExpr;
typedef struct Sysml2AstQualifiedName Sysml2AstQualifiedName;

/* AST node kinds */
typedef enum {
    SYSML2_AST_NAMESPACE,
    SYSML2_AST_PACKAGE,
    SYSML2_AST_IMPORT,
    SYSML2_AST_ALIAS,
    SYSML2_AST_TYPE,
    SYSML2_AST_CLASSIFIER,
    SYSML2_AST_CLASS,
    SYSML2_AST_DATATYPE,
    SYSML2_AST_STRUCT,
    SYSML2_AST_ASSOC,
    SYSML2_AST_FEATURE,
    SYSML2_AST_CONNECTOR,
    SYSML2_AST_BINDING,
    SYSML2_AST_SUCCESSION,
    SYSML2_AST_BEHAVIOR,
    SYSML2_AST_FUNCTION,
    SYSML2_AST_PREDICATE,
    SYSML2_AST_INTERACTION,
    SYSML2_AST_COMMENT,
    SYSML2_AST_DOCUMENTATION,
    SYSML2_AST_METADATA,
    SYSML2_AST_MULTIPLICITY,
    SYSML2_AST_EXPR,

    /* SysML v2 specific */
    SYSML2_AST_PART_DEF,
    SYSML2_AST_PART_USAGE,
    SYSML2_AST_ACTION_DEF,
    SYSML2_AST_ACTION_USAGE,
    SYSML2_AST_STATE_DEF,
    SYSML2_AST_STATE_USAGE,
    SYSML2_AST_REQUIREMENT_DEF,
    SYSML2_AST_REQUIREMENT_USAGE,
    SYSML2_AST_CONSTRAINT_DEF,
    SYSML2_AST_CONSTRAINT_USAGE,
    SYSML2_AST_PORT_DEF,
    SYSML2_AST_PORT_USAGE,
    SYSML2_AST_INTERFACE_DEF,
    SYSML2_AST_CONNECTION_DEF,
    SYSML2_AST_FLOW_DEF,
    SYSML2_AST_ATTRIBUTE_DEF,
    SYSML2_AST_ATTRIBUTE_USAGE,
    SYSML2_AST_ITEM_DEF,
    SYSML2_AST_ITEM_USAGE,
    SYSML2_AST_VIEW_DEF,
    SYSML2_AST_VIEWPOINT_DEF,
    SYSML2_AST_INTERFACE_USAGE,
    SYSML2_AST_CONNECTION_USAGE,
    SYSML2_AST_FLOW_USAGE,
    SYSML2_AST_VIEW_USAGE,
    SYSML2_AST_VIEWPOINT_USAGE,
    SYSML2_AST_ENUM_DEF,
    SYSML2_AST_ENUM_USAGE,
    SYSML2_AST_CALC_DEF,
    SYSML2_AST_CALC_USAGE,
    SYSML2_AST_CASE_DEF,
    SYSML2_AST_CASE_USAGE,
    SYSML2_AST_ANALYSIS_DEF,
    SYSML2_AST_ANALYSIS_USAGE,
    SYSML2_AST_VERIFICATION_DEF,
    SYSML2_AST_VERIFICATION_USAGE,
    SYSML2_AST_USECASE_DEF,
    SYSML2_AST_USECASE_USAGE,
    SYSML2_AST_ALLOCATION_DEF,
    SYSML2_AST_ALLOCATION_USAGE,
    SYSML2_AST_RENDERING_DEF,
    SYSML2_AST_RENDERING_USAGE,
    SYSML2_AST_OCCURRENCE_DEF,
    SYSML2_AST_OCCURRENCE_USAGE,
    SYSML2_AST_TRANSITION,
    SYSML2_AST_ENTRY_ACTION,
    SYSML2_AST_EXIT_ACTION,
    SYSML2_AST_DO_ACTION,

    /* Control nodes */
    SYSML2_AST_THEN,
    SYSML2_AST_JOIN_NODE,
    SYSML2_AST_FORK_NODE,
    SYSML2_AST_MERGE_NODE,
    SYSML2_AST_FIRST,

    /* Individual prefix */
    SYSML2_AST_INDIVIDUAL_DEF,
    SYSML2_AST_INDIVIDUAL_USAGE,

    /* Perform action */
    SYSML2_AST_PERFORM,

    /* Succession flow */
    SYSML2_AST_SUCCESSION_FLOW,

    /* Behavioral action constructs */
    SYSML2_AST_BIND,
    SYSML2_AST_ACCEPT,
    SYSML2_AST_SEND,
    SYSML2_AST_DECIDE,
    SYSML2_AST_TERMINATE,
    SYSML2_AST_ASSIGN,
    SYSML2_AST_IF_ACTION,
    SYSML2_AST_WHILE_LOOP,
    SYSML2_AST_FOR_LOOP,
    SYSML2_AST_LOOP,

    /* Requirement constructs */
    SYSML2_AST_SUBJECT,
    SYSML2_AST_ACTOR,
    SYSML2_AST_OBJECTIVE,
    SYSML2_AST_SATISFY,
    SYSML2_AST_ASSUME,
    SYSML2_AST_ASSERT,
    SYSML2_AST_INCLUDE,
    SYSML2_AST_RETURN,

    /* Allocation and dependency */
    SYSML2_AST_ALLOCATE,
    SYSML2_AST_DEPENDENCY,

    /* View constructs */
    SYSML2_AST_CONCERN_DEF,
    SYSML2_AST_CONCERN_USAGE,
    SYSML2_AST_STAKEHOLDER,
    SYSML2_AST_FRAME,
    SYSML2_AST_EXPOSE,
    SYSML2_AST_RENDER,
    SYSML2_AST_VERIFY,

    /* Textual representation */
    SYSML2_AST_REP,
    SYSML2_AST_LANGUAGE,

    /* Connection constructs */
    SYSML2_AST_CONNECT,
    SYSML2_AST_MESSAGE,
} Sysml2AstKind;

/* Visibility modifiers */
typedef enum {
    SYSML2_VIS_PUBLIC,
    SYSML2_VIS_PRIVATE,
    SYSML2_VIS_PROTECTED,
} Sysml2Visibility;

/* Feature direction */
typedef enum {
    SYSML2_DIR_NONE,
    SYSML2_DIR_IN,
    SYSML2_DIR_OUT,
    SYSML2_DIR_INOUT,
} Sysml2Direction;

/* Expression kinds */
typedef enum {
    SYSML2_EXPR_LITERAL_INT,
    SYSML2_EXPR_LITERAL_REAL,
    SYSML2_EXPR_LITERAL_STRING,
    SYSML2_EXPR_LITERAL_BOOL,
    SYSML2_EXPR_LITERAL_NULL,
    SYSML2_EXPR_NAME,
    SYSML2_EXPR_FEATURE_CHAIN,
    SYSML2_EXPR_INVOCATION,
    SYSML2_EXPR_BINARY,
    SYSML2_EXPR_UNARY,
    SYSML2_EXPR_CONDITIONAL,
    SYSML2_EXPR_BRACKET,
    SYSML2_EXPR_SELECT,
    SYSML2_EXPR_COLLECT,
} Sysml2ExprKind;

/* Binary operators */
typedef enum {
    SYSML2_BINOP_ADD,
    SYSML2_BINOP_SUB,
    SYSML2_BINOP_MUL,
    SYSML2_BINOP_DIV,
    SYSML2_BINOP_MOD,
    SYSML2_BINOP_POW,
    SYSML2_BINOP_EQ,
    SYSML2_BINOP_NE,
    SYSML2_BINOP_LT,
    SYSML2_BINOP_GT,
    SYSML2_BINOP_LE,
    SYSML2_BINOP_GE,
    SYSML2_BINOP_AND,
    SYSML2_BINOP_OR,
    SYSML2_BINOP_XOR,
    SYSML2_BINOP_IMPLIES,
    SYSML2_BINOP_META_EQ,    /* === */
    SYSML2_BINOP_META_NE,    /* !== */
} Sysml2BinaryOp;

/* Unary operators */
typedef enum {
    SYSML2_UNOP_NOT,
    SYSML2_UNOP_MINUS,
    SYSML2_UNOP_PLUS,
} Sysml2UnaryOp;

/* Relationship kind */
typedef enum {
    SYSML2_REL_SPECIALIZES,      /* :> */
    SYSML2_REL_TYPED_BY,         /* : */
    SYSML2_REL_SUBSETS,          /* ::> or subsets */
    SYSML2_REL_REDEFINES,        /* :>> or redefines */
    SYSML2_REL_CONJUGATES,       /* ~ */
    SYSML2_REL_REFERENCES,       /* references */
} Sysml2RelationshipKind;

/* Relationship target */
typedef struct Sysml2AstRelationship {
    struct Sysml2AstRelationship *next;
    Sysml2RelationshipKind kind;
    Sysml2AstQualifiedName *target;
    bool is_conjugated;         /* For typed-by: ~Type means conjugated type */
    Sysml2SourceRange range;
} Sysml2AstRelationship;

/* Qualified name (e.g., Package::Subpackage::Type) */
struct Sysml2AstQualifiedName {
    const char **segments;      /* Array of name segments (interned) */
    size_t segment_count;
    Sysml2SourceRange range;
    bool is_global;             /* Starts with :: */
};

/* Multiplicity bounds */
struct Sysml2AstMultiplicity {
    Sysml2AstExpr *lower;       /* Lower bound (NULL means 0) */
    Sysml2AstExpr *upper;       /* Upper bound (NULL means *) */
    bool is_ordered;
    bool is_nonunique;
    Sysml2SourceRange range;
};

/* Expression node */
struct Sysml2AstExpr {
    Sysml2ExprKind kind;
    Sysml2SourceRange range;

    union {
        /* Literals */
        int64_t int_value;
        double real_value;
        const char *string_value;
        bool bool_value;

        /* Name reference */
        Sysml2AstQualifiedName *name;

        /* Feature chain: a.b.c */
        struct {
            Sysml2AstExpr *base;
            const char *member;
        } chain;

        /* Invocation: f(a, b) */
        struct {
            Sysml2AstExpr *target;
            Sysml2AstExpr **arguments;
            size_t argument_count;
        } invocation;

        /* Binary: a + b */
        struct {
            Sysml2BinaryOp op;
            Sysml2AstExpr *left;
            Sysml2AstExpr *right;
        } binary;

        /* Unary: -a, !b */
        struct {
            Sysml2UnaryOp op;
            Sysml2AstExpr *operand;
        } unary;

        /* Conditional: if a then b else c */
        struct {
            Sysml2AstExpr *condition;
            Sysml2AstExpr *then_expr;
            Sysml2AstExpr *else_expr;
        } conditional;

        /* Bracket: [a] */
        Sysml2AstExpr *bracket_expr;
    };
};

/* Member of a namespace/type */
typedef struct Sysml2AstMember {
    struct Sysml2AstMember *next;
    Sysml2AstKind kind;
    Sysml2Visibility visibility;
    void *node;                 /* Actual node based on kind */
} Sysml2AstMember;

/* Comment node */
struct Sysml2AstComment {
    const char *text;           /* Comment body */
    const char *locale;         /* Optional locale */
    Sysml2AstQualifiedName **about; /* What this comment is about */
    size_t about_count;
    Sysml2SourceRange range;
};

/* Documentation node */
struct Sysml2AstDocumentation {
    const char *text;           /* Documentation body */
    const char *locale;         /* Optional locale */
    Sysml2SourceRange range;
};

/* Import statement */
struct Sysml2AstImport {
    Sysml2AstQualifiedName *target;
    bool is_all;                /* import all (*) */
    bool is_recursive;          /* import ** */
    bool is_namespace_import;   /* Import namespace members */
    const char *alias;          /* Optional alias (interned) */
    Sysml2Visibility visibility;
    Sysml2SourceRange range;
};

/* Alias declaration */
struct Sysml2AstAlias {
    const char *name;           /* Alias name (interned) */
    Sysml2AstQualifiedName *target;
    Sysml2Visibility visibility;
    Sysml2SourceRange range;
};

/* Namespace node (root) */
struct Sysml2AstNamespace {
    const char *name;           /* May be NULL for root */
    bool is_library;            /* library namespace */
    Sysml2AstMember *members;   /* Linked list of members */
    Sysml2AstComment **comments;
    size_t comment_count;
    Sysml2SourceRange range;
};

/* Package node */
struct Sysml2AstPackage {
    const char *name;           /* Package name (interned) */
    const char *short_name;     /* Optional short name */
    bool is_library;
    bool is_standard;           /* Standard library package */
    Sysml2AstMember *members;
    Sysml2AstComment **comments;
    size_t comment_count;
    Sysml2SourceRange range;
};

/* Type prefixes */
typedef struct {
    bool is_abstract;
    bool is_readonly;
    bool is_derived;
    bool is_end;
    bool is_composite;
    bool is_portion;
    bool is_ref;
} Sysml2TypePrefix;

/* Type node */
struct Sysml2AstType {
    const char *name;
    const char *short_name;
    Sysml2TypePrefix prefix;
    Sysml2AstRelationship *relationships; /* specializations, conjugates */
    Sysml2AstMultiplicity *multiplicity;
    Sysml2AstMember *members;
    Sysml2SourceRange range;
};

/* Classifier node (class, datatype, struct, assoc) */
struct Sysml2AstClassifier {
    Sysml2AstKind kind;         /* CLASS, DATATYPE, STRUCT, ASSOC */
    const char *name;
    const char *short_name;
    Sysml2TypePrefix prefix;
    Sysml2AstRelationship *relationships;
    Sysml2AstMultiplicity *multiplicity;
    Sysml2AstMember *members;
    Sysml2SourceRange range;
};

/* Feature node */
struct Sysml2AstFeature {
    const char *name;
    const char *short_name;
    Sysml2TypePrefix prefix;
    Sysml2Direction direction;
    Sysml2AstRelationship *relationships; /* typed-by, subsets, redefines */
    Sysml2AstMultiplicity *multiplicity;
    Sysml2AstExpr *default_value;
    Sysml2AstMember *members;   /* Nested features */
    Sysml2SourceRange range;
};

/* Generic AST node (for iteration) */
struct Sysml2AstNode {
    Sysml2AstKind kind;
    Sysml2SourceRange range;
    void *data;                 /* Points to actual node */
};

/* AST helper functions */

/* Create a new qualified name */
Sysml2AstQualifiedName *sysml2_ast_qname_create(
    Sysml2Arena *arena,
    const char **segments,
    size_t count,
    bool is_global
);

/* Get qualified name as string (for display) */
char *sysml2_ast_qname_to_string(Sysml2Arena *arena, const Sysml2AstQualifiedName *name);

/* Create an integer literal expression */
Sysml2AstExpr *sysml2_ast_expr_int(Sysml2Arena *arena, int64_t value, Sysml2SourceRange range);

/* Create a real literal expression */
Sysml2AstExpr *sysml2_ast_expr_real(Sysml2Arena *arena, double value, Sysml2SourceRange range);

/* Create a string literal expression */
Sysml2AstExpr *sysml2_ast_expr_string(Sysml2Arena *arena, const char *value, Sysml2SourceRange range);

/* Create a boolean literal expression */
Sysml2AstExpr *sysml2_ast_expr_bool(Sysml2Arena *arena, bool value, Sysml2SourceRange range);

/* Create a null literal expression */
Sysml2AstExpr *sysml2_ast_expr_null(Sysml2Arena *arena, Sysml2SourceRange range);

/* Create a name reference expression */
Sysml2AstExpr *sysml2_ast_expr_name(Sysml2Arena *arena, Sysml2AstQualifiedName *name);

/* Create a binary expression */
Sysml2AstExpr *sysml2_ast_expr_binary(
    Sysml2Arena *arena,
    Sysml2BinaryOp op,
    Sysml2AstExpr *left,
    Sysml2AstExpr *right,
    Sysml2SourceRange range
);

/* Create a unary expression */
Sysml2AstExpr *sysml2_ast_expr_unary(
    Sysml2Arena *arena,
    Sysml2UnaryOp op,
    Sysml2AstExpr *operand,
    Sysml2SourceRange range
);

/* Get kind name as string */
const char *sysml2_ast_kind_to_string(Sysml2AstKind kind);

/* Print AST for debugging */
void sysml2_ast_print(Sysml2AstNamespace *ast, int indent);

/* Print members recursively */
void sysml2_ast_print_members(Sysml2AstMember *members, int indent);

/* Convert AST to JSON */
void sysml2_ast_to_json(Sysml2AstNamespace *ast, FILE *output);

#endif /* SYSML2_AST_H */
