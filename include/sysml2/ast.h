/*
 * SysML v2 Parser - AST Node Types
 *
 * Defines node kinds, structures for the semantic graph.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_AST_H
#define SYSML2_AST_H

#include "common.h"

/*
 * Trivia Kind Enumeration
 *
 * Trivia represents whitespace and comments that are preserved
 * for pretty-printing purposes.
 */
typedef enum {
    SYSML_TRIVIA_LINE_COMMENT,      /* // ... */
    SYSML_TRIVIA_BLOCK_COMMENT,     /* doc block comments: slash-star-star or slash-slash-star */
    SYSML_TRIVIA_REGULAR_COMMENT,   /* regular block comments: slash-star */
    SYSML_TRIVIA_BLANK_LINE,        /* Preserved blank lines */
} SysmlTriviaKind;

/*
 * Trivia - represents a comment or blank line
 *
 * Trivia is attached to AST nodes as leading or trailing.
 */
typedef struct SysmlTrivia {
    SysmlTriviaKind kind;
    const char *text;            /* Comment content (without delimiters) */
    Sysml2SourceLoc loc;
    struct SysmlTrivia *next;    /* Linked list */
    uint16_t count;              /* For BLANK_LINE: number of consecutive blank lines */
} SysmlTrivia;

/*
 * Node Kind Enumeration
 *
 * Organized by category with range-based prefixes:
 * - 0x0100: Packages
 * - 0x0200: Definitions
 * - 0x1000: Usages
 * - 0x3000: Relationships
 */
typedef enum {
    SYSML_KIND_UNKNOWN = 0,

    /* Import kinds (0x00xx - special) */
    SYSML_KIND_IMPORT = 0x0010,
    SYSML_KIND_IMPORT_ALL,        /* Pkg::* */
    SYSML_KIND_IMPORT_RECURSIVE,  /* Pkg::** */

    /* Packages (0x01xx) */
    SYSML_KIND_PACKAGE = 0x0100,
    SYSML_KIND_LIBRARY_PACKAGE,

    /* Definitions (0x02xx) */
    SYSML_KIND_ATTRIBUTE_DEF = 0x0200,
    SYSML_KIND_ENUMERATION_DEF,
    SYSML_KIND_OCCURRENCE_DEF,
    SYSML_KIND_ITEM_DEF,
    SYSML_KIND_PART_DEF,
    SYSML_KIND_CONNECTION_DEF,
    SYSML_KIND_FLOW_DEF,
    SYSML_KIND_INTERFACE_DEF,
    SYSML_KIND_PORT_DEF,
    SYSML_KIND_ALLOCATION_DEF,
    SYSML_KIND_ACTION_DEF,
    SYSML_KIND_STATE_DEF,
    SYSML_KIND_CONSTRAINT_DEF,
    SYSML_KIND_REQUIREMENT_DEF,
    SYSML_KIND_CONCERN_DEF,
    SYSML_KIND_CALC_DEF,
    SYSML_KIND_CASE_DEF,
    SYSML_KIND_ANALYSIS_DEF,
    SYSML_KIND_VERIFICATION_DEF,
    SYSML_KIND_USE_CASE_DEF,
    SYSML_KIND_VIEW_DEF,
    SYSML_KIND_VIEWPOINT_DEF,
    SYSML_KIND_RENDERING_DEF,
    SYSML_KIND_METADATA_DEF,
    SYSML_KIND_DATATYPE,          /* KerML datatype */

    /* KerML Definitions (0x02xx continued) */
    SYSML_KIND_NAMESPACE,         /* KerML namespace */
    SYSML_KIND_TYPE,              /* KerML type */
    SYSML_KIND_CLASSIFIER,        /* KerML classifier */
    SYSML_KIND_CLASS,             /* KerML class */
    SYSML_KIND_STRUCTURE,         /* KerML struct */
    SYSML_KIND_METACLASS,         /* KerML metaclass */
    SYSML_KIND_ASSOCIATION,       /* KerML assoc */
    SYSML_KIND_ASSOC_STRUCT,      /* KerML assoc struct */
    SYSML_KIND_INTERACTION,       /* KerML interaction */
    SYSML_KIND_BEHAVIOR,          /* KerML behavior */
    SYSML_KIND_FUNCTION,          /* KerML function */
    SYSML_KIND_PREDICATE,         /* KerML predicate */
    SYSML_KIND_MULTIPLICITY_DEF,  /* KerML multiplicity */

    /* Usages (0x10xx) */
    SYSML_KIND_ATTRIBUTE_USAGE = 0x1000,
    SYSML_KIND_ENUMERATION_USAGE,
    SYSML_KIND_OCCURRENCE_USAGE,
    SYSML_KIND_ITEM_USAGE,
    SYSML_KIND_PART_USAGE,
    SYSML_KIND_CONNECTION_USAGE,
    SYSML_KIND_FLOW_USAGE,
    SYSML_KIND_INTERFACE_USAGE,
    SYSML_KIND_PORT_USAGE,
    SYSML_KIND_ALLOCATION_USAGE,
    SYSML_KIND_ACTION_USAGE,
    SYSML_KIND_STATE_USAGE,
    SYSML_KIND_CONSTRAINT_USAGE,
    SYSML_KIND_REQUIREMENT_USAGE,
    SYSML_KIND_CONCERN_USAGE,
    SYSML_KIND_CALC_USAGE,
    SYSML_KIND_CASE_USAGE,
    SYSML_KIND_ANALYSIS_USAGE,
    SYSML_KIND_VERIFICATION_USAGE,
    SYSML_KIND_USE_CASE_USAGE,
    SYSML_KIND_VIEW_USAGE,
    SYSML_KIND_VIEWPOINT_USAGE,
    SYSML_KIND_RENDERING_USAGE,
    SYSML_KIND_REFERENCE_USAGE,
    SYSML_KIND_EVENT_USAGE,
    SYSML_KIND_PORTION_USAGE,
    SYSML_KIND_SUBJECT_USAGE,
    SYSML_KIND_ACTOR_USAGE,
    SYSML_KIND_STAKEHOLDER_USAGE,
    SYSML_KIND_MESSAGE_USAGE,
    SYSML_KIND_PERFORM_ACTION_USAGE,  /* perform action usage */
    SYSML_KIND_PARAMETER,         /* Bare parameter (no keyword, just direction) */
    SYSML_KIND_SHORTHAND_USAGE,   /* Anonymous usage with :>> redefines (scope container) */

    /* KerML Features (0x10xx continued) */
    SYSML_KIND_FEATURE,           /* KerML feature */
    SYSML_KIND_STEP,              /* KerML step */
    SYSML_KIND_EXPRESSION,        /* KerML expr */
    SYSML_KIND_BOOL_EXPRESSION,   /* KerML bool */
    SYSML_KIND_INVARIANT,         /* KerML inv */
    SYSML_KIND_CONNECTOR,         /* KerML connector */
    SYSML_KIND_BINDING_CONNECTOR, /* KerML binding */
    SYSML_KIND_SUCCESSION,        /* KerML succession */
    SYSML_KIND_KERML_FLOW,        /* KerML flow */
    SYSML_KIND_SUCCESSION_FLOW,   /* KerML succession flow */
    SYSML_KIND_END_FEATURE,       /* End feature in connectors/interfaces */

    /* Relationships (0x30xx) */
    SYSML_KIND_REL_CONNECTION = 0x3000,
    SYSML_KIND_REL_FLOW,
    SYSML_KIND_REL_ALLOCATION,
    SYSML_KIND_REL_SATISFY,
    SYSML_KIND_REL_VERIFY,
    SYSML_KIND_REL_TRANSITION,
    SYSML_KIND_REL_SUCCESSION,
    SYSML_KIND_REL_BIND,

    /* KerML Relationships (0x30xx continued) */
    SYSML_KIND_REL_SPECIALIZATION,    /* subtype */
    SYSML_KIND_REL_CONJUGATION,       /* conjugate */
    SYSML_KIND_REL_SUBCLASSIFICATION, /* subclassifier */
    SYSML_KIND_REL_DISJOINING,        /* disjoint */
    SYSML_KIND_REL_INVERTING,         /* inverse */
    SYSML_KIND_REL_TYPING,            /* typing */
    SYSML_KIND_REL_SUBSETTING,        /* subset */
    SYSML_KIND_REL_REDEFINITION,      /* redefinition */
    SYSML_KIND_REL_FEATURING,         /* featuring */
} SysmlNodeKind;

/* Check if a kind is a definition type */
#define SYSML_KIND_IS_DEFINITION(k) ((k) >= 0x0200 && (k) < 0x1000)

/* Check if a kind is a usage type */
#define SYSML_KIND_IS_USAGE(k) ((k) >= 0x1000 && (k) < 0x3000)

/* Check if a kind is a relationship type */
#define SYSML_KIND_IS_RELATIONSHIP(k) ((k) >= 0x3000)

/* Check if a kind is a package type */
#define SYSML_KIND_IS_PACKAGE(k) ((k) >= 0x0100 && (k) < 0x0200)

/* Check if a kind is a KerML classifier (can type features per KerML spec) */
#define SYSML_KIND_IS_KERML_CLASSIFIER(k) \
    ((k) == SYSML_KIND_TYPE || \
     (k) == SYSML_KIND_CLASSIFIER || \
     (k) == SYSML_KIND_CLASS || \
     (k) == SYSML_KIND_STRUCTURE || \
     (k) == SYSML_KIND_METACLASS || \
     (k) == SYSML_KIND_ASSOCIATION || \
     (k) == SYSML_KIND_ASSOC_STRUCT || \
     (k) == SYSML_KIND_INTERACTION || \
     (k) == SYSML_KIND_BEHAVIOR || \
     (k) == SYSML_KIND_FUNCTION || \
     (k) == SYSML_KIND_PREDICATE || \
     (k) == SYSML_KIND_DATATYPE)

/* Check if a kind is a KerML feature (can be typed by any classifier/definition) */
#define SYSML_KIND_IS_KERML_FEATURE(k) \
    ((k) == SYSML_KIND_FEATURE || \
     (k) == SYSML_KIND_STEP || \
     (k) == SYSML_KIND_EXPRESSION || \
     (k) == SYSML_KIND_BOOL_EXPRESSION || \
     (k) == SYSML_KIND_INVARIANT || \
     (k) == SYSML_KIND_CONNECTOR || \
     (k) == SYSML_KIND_BINDING_CONNECTOR || \
     (k) == SYSML_KIND_SUCCESSION || \
     (k) == SYSML_KIND_KERML_FLOW || \
     (k) == SYSML_KIND_SUCCESSION_FLOW || \
     (k) == SYSML_KIND_END_FEATURE)

/*
 * Statement Kind Enumeration
 *
 * Types of body statements that can appear within definitions/usages.
 * These are statements that don't create new named elements but express
 * relationships, control flow, or other behavioral content.
 */
typedef enum {
    SYSML_STMT_NONE = 0,

    /* Relationship statements */
    SYSML_STMT_BIND,           /* bind x = y; */
    SYSML_STMT_CONNECT,        /* connect x to y; */
    SYSML_STMT_FLOW,           /* flow from x to y; */
    SYSML_STMT_ALLOCATE,       /* allocate x to y; */
    SYSML_STMT_SUCCESSION,     /* first x then y; */

    /* State body statements */
    SYSML_STMT_ENTRY,          /* entry action {...} */
    SYSML_STMT_EXIT,           /* exit action {...} */
    SYSML_STMT_DO,             /* do action {...} */
    SYSML_STMT_TRANSITION,     /* transition first X then Y; */
    SYSML_STMT_ACCEPT,         /* accept sig : T do {...} then S; */

    /* Action body statements */
    SYSML_STMT_SEND,           /* send expr via port to target; */
    SYSML_STMT_ACCEPT_ACTION,  /* accept payload via port; */
    SYSML_STMT_ASSIGN,         /* assign x := expr; */
    SYSML_STMT_IF,             /* if cond {...} else {...} */
    SYSML_STMT_WHILE,          /* while cond {...} */
    SYSML_STMT_FOR,            /* for x in expr {...} */
    SYSML_STMT_LOOP,           /* loop {...} until cond; */
    SYSML_STMT_TERMINATE,      /* terminate; */

    /* Control nodes */
    SYSML_STMT_MERGE,          /* merge node */
    SYSML_STMT_DECIDE,         /* decide node */
    SYSML_STMT_JOIN,           /* join node */
    SYSML_STMT_FORK,           /* fork node */

    /* Succession members */
    SYSML_STMT_FIRST,          /* first x then y; */
    SYSML_STMT_THEN,           /* then x; */

    /* Metadata */
    SYSML_STMT_METADATA_USAGE, /* metadata X about Y, Z; */

    /* Shorthand features */
    SYSML_STMT_SHORTHAND_FEATURE, /* :> name : Type; or :>> name = value; */

    /* Requirement body statements */
    SYSML_STMT_REQUIRE_CONSTRAINT, /* require constraint {...} */
    SYSML_STMT_ASSUME_CONSTRAINT,  /* assume constraint {...} */
    SYSML_STMT_SUBJECT,            /* subject x : Type; */
    SYSML_STMT_ACTOR,              /* actor a : Type; */
    SYSML_STMT_STAKEHOLDER,        /* stakeholder s : Type; */
    SYSML_STMT_OBJECTIVE,          /* objective o {...} */
    SYSML_STMT_FRAME,              /* frame requirement r {...} */
    SYSML_STMT_SATISFY,            /* satisfy requirement by part; */
    SYSML_STMT_INCLUDE_USE_CASE,   /* include use case x; */
    SYSML_STMT_EXPOSE,             /* expose Vehicle::*; */
    SYSML_STMT_RENDER,             /* render rendering r : R; */
    SYSML_STMT_VERIFY,             /* verify requirement x; */

    /* Connection/interface body statements */
    SYSML_STMT_END_MEMBER,         /* end port x : Type; */

    /* Return usage */
    SYSML_STMT_RETURN,             /* return : Type; */

    /* Other */
    SYSML_STMT_RESULT_EXPR,    /* bare expression at end of calc/constraint */
} SysmlStatementKind;

/*
 * Connector Endpoint - for relationship statements
 *
 * Represents one end of a connection/flow/bind/etc.
 */
typedef struct SysmlConnectorEnd {
    const char *target;           /* Qualified name */
    const char *feature_chain;    /* Optional .x.y chain */
    const char *multiplicity;     /* Optional [mult] */
} SysmlConnectorEnd;

/*
 * Body Statement - generic container for body content
 *
 * Used for relationship statements, control flow, state actions, etc.
 * Uses raw_text for complex nested content to preserve formatting.
 */
typedef struct SysmlStatement {
    SysmlStatementKind kind;
    Sysml2SourceLoc loc;
    const char *raw_text;         /* Verbatim captured text */
    SysmlConnectorEnd source;     /* For relationships */
    SysmlConnectorEnd target;     /* For relationships */
    const char *name;             /* Optional statement name */
    const char *guard;            /* For guarded succession/if */
    const char *payload;          /* For flow/message payload */
    struct SysmlStatement **nested; /* For control structures */
    size_t nested_count;
} SysmlStatement;

/*
 * Named Comment - comment annotation with name and target
 *
 * Represents: comment name about X { text }
 */
typedef struct SysmlNamedComment {
    const char *id;
    const char *name;
    const char **about;           /* Target elements */
    size_t about_count;
    const char *locale;           /* Optional locale */
    const char *text;             /* Includes block comment delimiters */
    Sysml2SourceLoc loc;
} SysmlNamedComment;

/*
 * Textual Representation - rep language annotation
 *
 * Represents: rep language "python" { code }
 */
typedef struct SysmlTextualRep {
    const char *id;
    const char *name;
    const char *language;         /* Language name including quotes */
    const char *text;             /* Includes block comment delimiters */
    Sysml2SourceLoc loc;
} SysmlTextualRep;

/*
 * Metadata Feature - attribute assignment within a metadata usage
 */
typedef struct SysmlMetadataFeature {
    const char *name;               /* Feature name (e.g., "filePath") */
    const char *value;              /* String value or expression */
} SysmlMetadataFeature;

/*
 * Metadata Usage - applied metadata (@Type { attr = val; })
 */
typedef struct SysmlMetadataUsage {
    const char *type_ref;           /* Metadata type (e.g., "SourceLink") */
    const char **about;             /* Target elements if standalone */
    size_t about_count;
    SysmlMetadataFeature **features; /* Attribute assignments */
    size_t feature_count;
    Sysml2SourceLoc loc;            /* Source location for ordering */
} SysmlMetadataUsage;

/*
 * Direction enumeration for parameters
 */
typedef enum {
    SYSML_DIR_NONE = 0,
    SYSML_DIR_IN,
    SYSML_DIR_OUT,
    SYSML_DIR_INOUT
} SysmlDirection;

/*
 * Visibility enumeration for members and imports
 */
typedef enum {
    SYSML_VIS_PUBLIC = 0,
    SYSML_VIS_PRIVATE,
    SYSML_VIS_PROTECTED
} SysmlVisibility;

/*
 * AST Node - represents an element in the semantic graph
 *
 * Uses interned strings for memory efficiency.
 */
typedef struct SysmlNode {
    const char *id;           /* Path-based ID (e.g., "Pkg::PartDef::attr") */
    const char *name;         /* Local name */
    SysmlNodeKind kind;
    const char *parent_id;    /* Parent element ID (containment) */

    /* Type relationships */
    const char **typed_by;       /* : Type (typing) */
    size_t typed_by_count;
    bool *typed_by_conjugated;   /* Parallel array: true if type has ~ prefix */

    const char **specializes;    /* :> Type (specialization/subsetting) */
    size_t specializes_count;

    const char **redefines;      /* :>> Type (redefinition) */
    size_t redefines_count;

    const char **references;     /* ::> Type (referencing) */
    size_t references_count;

    /* Multiplicity bounds (e.g., [0..1], [1..*], [4]) */
    const char *multiplicity_lower;  /* "0", "1", "*", etc. */
    const char *multiplicity_upper;  /* "1", "*", etc. (NULL if same as lower) */

    /* Default/initial value */
    const char *default_value;       /* Expression text */
    bool has_default_keyword;        /* true if "default =" vs just "=" */

    /* Modifiers */
    bool is_abstract;
    bool is_variation;
    bool is_readonly;
    bool is_derived;
    bool is_constant;
    bool is_ref;
    bool is_end;                 /* true if 'end' keyword was present */
    bool is_parallel;            /* true if 'parallel' keyword on state */
    bool is_exhibit;             /* true if 'exhibit' keyword on state usage */
    bool is_event_occurrence;    /* true if 'event occurrence' vs just 'event' */
    bool is_standard_library;    /* true if library package declared with 'standard' prefix */
    bool is_public_explicit;     /* true if 'public' keyword was explicit */
    bool has_enum_keyword;       /* true if enum usage has 'enum' keyword prefix */
    bool is_asserted;            /* true if 'assert' keyword was present */
    bool is_negated;             /* true if 'not' keyword was present (assert not) */
    bool has_connect_keyword;    /* true if 'connect' keyword was present in interface */
    bool has_action_keyword;     /* true if 'action' keyword was present in perform */

    /* Specialized keyword for ref behavioral features (state, action, etc.) */
    const char *ref_behavioral_keyword;  /* NULL or "state", "action", etc. */

    /* Portion kind for snapshot/timeslice usages */
    const char *portion_kind;    /* NULL or "snapshot", "timeslice" */

    /* Direction (for parameters) */
    SysmlDirection direction;

    /* Visibility */
    SysmlVisibility visibility;

    /* Parameter list for action/state definitions */
    const char *parameter_list;  /* Raw "(in x : T, out y)" for definitions */

    /* Connector/allocation parts for connections and allocations */
    const char *connector_part;  /* Raw "connect (a, b, c)" or "allocate X to Y" */

    /* Source location for debugging */
    Sysml2SourceLoc loc;

    /* Documentation comment (doc comment text) */
    const char *documentation;
    Sysml2SourceLoc doc_loc;      /* Source location for ordering */

    /* Applied metadata inside element body (@Metadata { ... }) */
    SysmlMetadataUsage **metadata;
    size_t metadata_count;

    /* Prefix metadata before element (#Metadata) */
    const char **prefix_metadata;
    size_t prefix_metadata_count;

    /* Prefix applied metadata before element (@Metadata {...}) */
    SysmlMetadataUsage **prefix_applied_metadata;
    size_t prefix_applied_metadata_count;

    /* Trivia for pretty printing */
    SysmlTrivia *leading_trivia;   /* Comments before node */
    SysmlTrivia *trailing_trivia;  /* Same-line comment after */

    /* Body statements (relationship statements, control flow, etc.) */
    SysmlStatement **body_stmts;
    size_t body_stmt_count;

    /* Named comments within body */
    SysmlNamedComment **comments;
    size_t comment_count;

    /* Textual representations within body */
    SysmlTextualRep **textual_reps;
    size_t textual_rep_count;

    /* Result expression (for calc/constraint bodies) */
    const char *result_expression;
} SysmlNode;

/*
 * Relationship - represents a connection between elements
 */
typedef struct SysmlRelationship {
    const char *id;           /* Unique relationship ID */
    SysmlNodeKind kind;       /* Relationship type (connection, flow, etc.) */
    const char *source;       /* Source element/feature path */
    const char *target;       /* Target element/feature path */

    /* Source location for debugging */
    Sysml2SourceLoc loc;
} SysmlRelationship;

/*
 * Import - represents an import declaration
 */
typedef struct SysmlImport {
    const char *id;           /* Unique import ID */
    SysmlNodeKind kind;       /* IMPORT, IMPORT_ALL, or IMPORT_RECURSIVE */
    const char *target;       /* Imported qualified name */
    const char *owner_scope;  /* Scope where import appears */

    /* Visibility for the import (private import X::*;) */
    bool is_private;
    bool is_public_explicit;  /* True if 'public' keyword was explicit */

    /* Source location for debugging */
    Sysml2SourceLoc loc;
} SysmlImport;

/*
 * Alias - represents an alias declaration (alias X for Y;)
 */
typedef struct SysmlAlias {
    const char *id;           /* Unique alias ID */
    const char *name;         /* Alias name */
    const char *target;       /* What it aliases (qualified name) */
    const char *owner_scope;  /* Containing scope */

    /* Source location for debugging */
    Sysml2SourceLoc loc;
} SysmlAlias;

/*
 * Semantic Model - the complete parsed model
 */
typedef struct {
    const char *source_name;  /* Source file name */

    SysmlNode **elements;     /* Array of all elements */
    size_t element_count;
    size_t element_capacity;

    SysmlRelationship **relationships; /* Array of all relationships */
    size_t relationship_count;
    size_t relationship_capacity;

    SysmlImport **imports;    /* Array of all imports */
    size_t import_count;
    size_t import_capacity;

    SysmlAlias **aliases;     /* Array of all aliases */
    size_t alias_count;
    size_t alias_capacity;
} SysmlSemanticModel;

/* Get the JSON type string for a node kind */
const char *sysml2_kind_to_json_type(SysmlNodeKind kind);

/* Get a human-readable name for a node kind */
const char *sysml2_kind_to_string(SysmlNodeKind kind);

/* Get the SysML/KerML keyword for a node kind (for pretty printing) */
const char *sysml2_kind_to_keyword(SysmlNodeKind kind);

#endif /* SYSML2_AST_H */
