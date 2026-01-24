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
    SYSML_TRIVIA_LINE_COMMENT,   /* // ... */
    SYSML_TRIVIA_BLOCK_COMMENT,  /* block comment */
    SYSML_TRIVIA_BLANK_LINE,     /* Preserved blank lines */
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
} SysmlMetadataUsage;

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

    const char **specializes;    /* :> Type (specialization/subsetting) */
    size_t specializes_count;

    const char **redefines;      /* :>> Type (redefinition) */
    size_t redefines_count;

    const char **references;     /* ::> Type (referencing) */
    size_t references_count;

    /* Source location for debugging */
    Sysml2SourceLoc loc;

    /* Documentation comment (doc comment text) */
    const char *documentation;

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

    /* Source location for debugging */
    Sysml2SourceLoc loc;
} SysmlImport;

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
} SysmlSemanticModel;

/* Get the JSON type string for a node kind */
const char *sysml2_kind_to_json_type(SysmlNodeKind kind);

/* Get a human-readable name for a node kind */
const char *sysml2_kind_to_string(SysmlNodeKind kind);

/* Get the SysML/KerML keyword for a node kind (for pretty printing) */
const char *sysml2_kind_to_keyword(SysmlNodeKind kind);

#endif /* SYSML2_AST_H */
