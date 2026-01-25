/*
 * SysML v2 Parser - AST Builder
 *
 * Builder context for constructing the semantic graph during parsing.
 * Manages scope stack, ID generation, and element collection.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_AST_BUILDER_H
#define SYSML2_AST_BUILDER_H

#include "common.h"
#include "arena.h"
#include "intern.h"
#include "ast.h"

/* Default capacities */
#define SYSML_BUILD_DEFAULT_SCOPE_CAPACITY 32
#define SYSML_BUILD_DEFAULT_ELEMENT_CAPACITY 256
#define SYSML_BUILD_DEFAULT_REL_CAPACITY 64
#define SYSML_BUILD_DEFAULT_IMPORT_CAPACITY 32
#define SYSML_BUILD_DEFAULT_ALIAS_CAPACITY 16

/*
 * Build Context - manages AST construction during parsing
 */
typedef struct SysmlBuildContext {
    Sysml2Arena *arena;       /* Memory arena for allocations */
    Sysml2Intern *intern;     /* String interning table */
    const char *source_name;  /* Source file name */

    /* Scope stack for containment tracking */
    const char **scope_stack; /* Stack of scope IDs */
    size_t scope_depth;
    size_t scope_capacity;

    /* Anonymous element counter for ID generation */
    size_t anon_counter;

    /* Relationship counter for ID generation */
    size_t rel_counter;

    /* Collected elements */
    SysmlNode **elements;
    size_t element_count;
    size_t element_capacity;

    /* Collected relationships */
    SysmlRelationship **relationships;
    size_t relationship_count;
    size_t relationship_capacity;

    /* Collected imports */
    SysmlImport **imports;
    size_t import_count;
    size_t import_capacity;

    /* Collected aliases */
    SysmlAlias **aliases;
    size_t alias_count;
    size_t alias_capacity;

    /* Pending trivia for attachment to next node */
    SysmlTrivia *pending_trivia_head;
    SysmlTrivia *pending_trivia_tail;

    /* Pending modifiers for attachment to next node */
    bool pending_abstract;
    bool pending_variation;
    bool pending_readonly;
    bool pending_derived;
    bool pending_ref;
    SysmlDirection pending_direction;
    SysmlVisibility pending_visibility;

    /* Pending multiplicity for attachment to next node */
    const char *pending_multiplicity_lower;
    const char *pending_multiplicity_upper;

    /* Pending default value for attachment to next node */
    const char *pending_default_value;
    bool pending_has_default_keyword;

    /* Pending import visibility for next import */
    bool pending_import_private;
    bool pending_import_public;  /* Explicit public keyword */

    /* Pending parameter kind for parameter usages */
    SysmlNodeKind pending_param_kind;

    /* Pending prefix metadata for attachment to next node */
    const char **pending_prefix_metadata;
    size_t pending_prefix_metadata_count;
    size_t pending_prefix_metadata_capacity;

    /* Pending applied metadata (@Type {...}) for attachment to next node */
    SysmlMetadataUsage **pending_metadata;
    size_t pending_metadata_count;
    size_t pending_metadata_capacity;

    /* Current metadata usage being built */
    SysmlMetadataUsage *current_metadata;

    /* Pending body statements for attachment to current scope */
    SysmlStatement **pending_stmts;
    size_t pending_stmt_count;
    size_t pending_stmt_capacity;

    /* Pending parameter list for attachment to next node */
    const char *pending_param_list;

    /* Pending flow payload for attachment to next flow capture */
    const char *pending_flow_payload;

    /* Pending named comments for attachment to current scope */
    SysmlNamedComment **pending_comments;
    size_t pending_comment_count;
    size_t pending_comment_capacity;

    /* Pending textual representations for attachment to current scope */
    SysmlTextualRep **pending_reps;
    size_t pending_rep_count;
    size_t pending_rep_capacity;

    /* Counter for generating unique comment/rep IDs */
    size_t comment_counter;
    size_t rep_counter;
} SysmlBuildContext;

/*
 * Create a new build context
 *
 * @param arena Memory arena for allocations
 * @param intern String interning table
 * @param source_name Name of the source file
 * @return New build context, or NULL on failure
 */
SysmlBuildContext *sysml2_build_context_create(
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    const char *source_name
);

/*
 * Destroy a build context
 *
 * Note: Elements allocated via the arena are not freed here.
 */
void sysml2_build_context_destroy(SysmlBuildContext *ctx);

/*
 * Push a new scope onto the scope stack
 *
 * @param ctx Build context
 * @param scope_id ID of the new scope (interned)
 */
void sysml2_build_push_scope(SysmlBuildContext *ctx, const char *scope_id);

/*
 * Pop the current scope from the stack
 *
 * @param ctx Build context
 */
void sysml2_build_pop_scope(SysmlBuildContext *ctx);

/*
 * Get the current scope ID
 *
 * @param ctx Build context
 * @return Current scope ID, or NULL if at root
 */
const char *sysml2_build_current_scope(SysmlBuildContext *ctx);

/*
 * Generate a path-based ID for an element
 *
 * If name is NULL, generates an anonymous ID like "Pkg::_anon_1"
 *
 * @param ctx Build context
 * @param name Local name (can be NULL for anonymous)
 * @return Interned path ID
 */
const char *sysml2_build_make_id(SysmlBuildContext *ctx, const char *name);

/*
 * Generate a unique relationship ID
 *
 * @param ctx Build context
 * @param kind Type hint for the ID prefix (e.g., "conn", "flow")
 * @return Interned relationship ID
 */
const char *sysml2_build_make_rel_id(SysmlBuildContext *ctx, const char *kind);

/*
 * Create a new AST node
 *
 * The node is allocated from the arena but not yet added to the model.
 *
 * @param ctx Build context
 * @param kind Node kind
 * @param name Local name (can be NULL)
 * @return New node, or NULL on failure
 */
SysmlNode *sysml2_build_node(
    SysmlBuildContext *ctx,
    SysmlNodeKind kind,
    const char *name
);

/*
 * Add an element to the model
 *
 * @param ctx Build context
 * @param node Node to add
 */
void sysml2_build_add_element(SysmlBuildContext *ctx, SysmlNode *node);

/*
 * Create a new relationship
 *
 * @param ctx Build context
 * @param kind Relationship kind
 * @param source Source element/feature path
 * @param target Target element/feature path
 * @return New relationship, or NULL on failure
 */
SysmlRelationship *sysml2_build_relationship(
    SysmlBuildContext *ctx,
    SysmlNodeKind kind,
    const char *source,
    const char *target
);

/*
 * Add a relationship to the model
 *
 * @param ctx Build context
 * @param rel Relationship to add
 */
void sysml2_build_add_relationship(SysmlBuildContext *ctx, SysmlRelationship *rel);

/*
 * Add a type reference to a node (: operator)
 *
 * @param ctx Build context
 * @param node Node to modify
 * @param type_ref Type reference (qualified name)
 */
void sysml2_build_add_typed_by(
    SysmlBuildContext *ctx,
    SysmlNode *node,
    const char *type_ref
);

/*
 * Add a specialization to a node (:> operator)
 *
 * @param ctx Build context
 * @param node Node to modify
 * @param type_ref Type reference (qualified name)
 */
void sysml2_build_add_specializes(
    SysmlBuildContext *ctx,
    SysmlNode *node,
    const char *type_ref
);

/*
 * Add a redefinition to a node (:>> operator)
 *
 * @param ctx Build context
 * @param node Node to modify
 * @param type_ref Type reference (qualified name)
 */
void sysml2_build_add_redefines(
    SysmlBuildContext *ctx,
    SysmlNode *node,
    const char *type_ref
);

/*
 * Add a reference to a node (::> operator)
 *
 * @param ctx Build context
 * @param node Node to modify
 * @param type_ref Type reference (qualified name)
 */
void sysml2_build_add_references(
    SysmlBuildContext *ctx,
    SysmlNode *node,
    const char *type_ref
);

/*
 * Add an import declaration to the model
 *
 * @param ctx Build context
 * @param kind Import kind (IMPORT, IMPORT_ALL, IMPORT_RECURSIVE)
 * @param target Target qualified name (what is being imported)
 */
void sysml2_build_add_import(
    SysmlBuildContext *ctx,
    SysmlNodeKind kind,
    const char *target
);

/*
 * Add an import declaration with source location
 *
 * @param ctx Build context
 * @param kind Import kind (IMPORT, IMPORT_ALL, IMPORT_RECURSIVE)
 * @param target Target qualified name (what is being imported)
 * @param offset Source byte offset for ordering
 */
void sysml2_build_add_import_with_loc(
    SysmlBuildContext *ctx,
    SysmlNodeKind kind,
    const char *target,
    uint32_t offset
);

/*
 * Finalize the build and return the semantic model
 *
 * After calling this, the build context should not be used.
 *
 * @param ctx Build context
 * @return Semantic model
 */
SysmlSemanticModel *sysml2_build_finalize(SysmlBuildContext *ctx);

/*
 * Add a trivia node to the pending list
 *
 * Trivia is accumulated until the next AST node is created,
 * at which point it becomes leading trivia for that node.
 *
 * @param ctx Build context
 * @param trivia Trivia to add
 */
void sysml2_build_add_pending_trivia(SysmlBuildContext *ctx, SysmlTrivia *trivia);

/*
 * Attach pending trivia to a node
 *
 * Called after creating a node to attach accumulated trivia
 * as leading trivia.
 *
 * @param ctx Build context
 * @param node Node to attach trivia to
 */
void sysml2_build_attach_pending_trivia(SysmlBuildContext *ctx, SysmlNode *node);

/**
 * Attach pending trivia as trailing trivia of a node
 *
 * Used for comments at end of body before closing brace.
 *
 * @param ctx Build context
 * @param node Node to attach trailing trivia to
 */
void sysml2_build_attach_pending_trailing_trivia(SysmlBuildContext *ctx, SysmlNode *node);

/*
 * Create a trivia node
 *
 * @param ctx Build context
 * @param kind Trivia kind
 * @param text Text content (without delimiters)
 * @param loc Source location
 * @return New trivia node
 */
SysmlTrivia *sysml2_build_trivia(
    SysmlBuildContext *ctx,
    SysmlTriviaKind kind,
    const char *text,
    Sysml2SourceLoc loc
);

/*
 * Create a metadata usage
 *
 * @param ctx Build context
 * @param type_ref Metadata type (e.g., "SourceLink")
 * @return New metadata usage, or NULL on failure
 */
SysmlMetadataUsage *sysml2_build_metadata_usage(
    SysmlBuildContext *ctx,
    const char *type_ref
);

/*
 * Add a feature to metadata usage
 *
 * @param ctx Build context
 * @param meta Metadata usage to modify
 * @param name Feature name
 * @param value Feature value (string or expression)
 */
void sysml2_build_metadata_add_feature(
    SysmlBuildContext *ctx,
    SysmlMetadataUsage *meta,
    const char *name,
    const char *value
);

/*
 * Add an "about" target to metadata
 *
 * @param ctx Build context
 * @param meta Metadata usage to modify
 * @param target_ref Target element reference
 */
void sysml2_build_metadata_add_about(
    SysmlBuildContext *ctx,
    SysmlMetadataUsage *meta,
    const char *target_ref
);

/*
 * Attach metadata to a node
 *
 * @param ctx Build context
 * @param node Node to attach metadata to
 * @param meta Metadata usage to attach
 */
void sysml2_build_add_metadata(
    SysmlBuildContext *ctx,
    SysmlNode *node,
    SysmlMetadataUsage *meta
);

/*
 * Add prefix metadata to a node
 *
 * @param ctx Build context
 * @param node Node to modify
 * @param metadata_ref Metadata type reference (e.g., "SourceLink")
 */
void sysml2_build_add_prefix_metadata(
    SysmlBuildContext *ctx,
    SysmlNode *node,
    const char *metadata_ref
);

/*
 * Add a pending prefix metadata that will be attached to the next node
 *
 * @param ctx Build context
 * @param metadata_ref Metadata type reference (e.g., "SourceLink")
 */
void sysml2_build_add_pending_prefix_metadata(
    SysmlBuildContext *ctx,
    const char *metadata_ref
);

/*
 * Start building a metadata usage
 *
 * @param ctx Build context
 * @param type_ref Metadata type reference
 * @return New metadata usage, or NULL on failure
 */
SysmlMetadataUsage *sysml2_build_start_metadata(
    SysmlBuildContext *ctx,
    const char *type_ref
);

/*
 * Finish building a metadata usage and attach to current scope's node
 *
 * @param ctx Build context
 */
void sysml2_build_end_metadata(SysmlBuildContext *ctx);

/*
 * Add a feature to the current metadata usage being built
 *
 * @param ctx Build context
 * @param name Feature name
 * @param value Feature value (string or expression)
 */
void sysml2_build_current_metadata_add_feature(
    SysmlBuildContext *ctx,
    const char *name,
    const char *value
);

/*
 * Capture an alias declaration
 *
 * @param ctx Build context
 * @param name Alias name
 * @param name_len Length of name
 * @param target Target qualified name
 * @param target_len Length of target
 */
void sysml2_build_alias(
    SysmlBuildContext *ctx,
    const char *name,
    size_t name_len,
    const char *target,
    size_t target_len
);

/*
 * Capture an alias declaration with source location
 *
 * @param ctx Build context
 * @param name Alias name
 * @param name_len Length of name
 * @param target Target qualified name
 * @param target_len Length of target
 * @param offset Source byte offset for ordering
 */
void sysml2_build_alias_with_loc(
    SysmlBuildContext *ctx,
    const char *name,
    size_t name_len,
    const char *target,
    size_t target_len,
    uint32_t offset
);

/*
 * Capture multiplicity bounds
 *
 * Captured multiplicity will be applied to the next created node.
 *
 * @param ctx Build context
 * @param text Multiplicity text (e.g., "0..1", "1..*", "4")
 * @param len Length of text
 */
void sysml2_capture_multiplicity(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture a default value
 *
 * Captured default value will be applied to the next created node.
 *
 * @param ctx Build context
 * @param text Default value expression text
 * @param len Length of text
 * @param has_default_keyword True if "default =" was used, false for just "="
 */
void sysml2_capture_default_value(SysmlBuildContext *ctx, const char *text, size_t len, bool has_default_keyword);

/*
 * Capture the abstract modifier
 *
 * Will be applied to the next created node.
 *
 * @param ctx Build context
 */
void sysml2_capture_abstract(SysmlBuildContext *ctx);

/*
 * Capture the variation modifier
 *
 * Will be applied to the next created node.
 *
 * @param ctx Build context
 */
void sysml2_capture_variation(SysmlBuildContext *ctx);

/*
 * Capture the ref modifier
 *
 * Will be applied to the next created node.
 *
 * @param ctx Build context
 */
void sysml2_capture_ref(SysmlBuildContext *ctx);

/*
 * Capture direction (in/out/inout)
 *
 * Will be applied to the next created node.
 *
 * @param ctx Build context
 * @param dir Direction value
 */
void sysml2_capture_direction(SysmlBuildContext *ctx, SysmlDirection dir);

/*
 * Capture import visibility (private/public)
 *
 * Will be applied to the next import.
 *
 * @param ctx Build context
 * @param is_private True for private import
 */
void sysml2_capture_import_visibility(SysmlBuildContext *ctx, bool is_private);

/*
 * Capture parameter kind (item, part, attribute, etc.)
 *
 * Will be applied to the next parameter usage.
 *
 * @param ctx Build context
 * @param kind Node kind for the parameter
 */
void sysml2_capture_param_kind(SysmlBuildContext *ctx, SysmlNodeKind kind);

/*
 * Clear all pending modifiers
 *
 * Called after applying to a node to reset state.
 *
 * @param ctx Build context
 */
void sysml2_build_clear_pending_modifiers(SysmlBuildContext *ctx);

/*
 * Capture a bind statement
 *
 * @param ctx Build context
 * @param source Source qualified name
 * @param source_len Length of source
 * @param target Target qualified name
 * @param target_len Length of target
 */
void sysml2_capture_bind(
    SysmlBuildContext *ctx,
    const char *source, size_t source_len,
    const char *target, size_t target_len
);

/*
 * Capture a connect statement
 *
 * @param ctx Build context
 * @param source Source qualified name
 * @param source_len Length of source
 * @param target Target qualified name
 * @param target_len Length of target
 */
void sysml2_capture_connect(
    SysmlBuildContext *ctx,
    const char *source, size_t source_len,
    const char *target, size_t target_len
);

/*
 * Capture a flow statement
 *
 * @param ctx Build context
 * @param payload Payload type name (can be NULL)
 * @param payload_len Length of payload
 * @param source Source qualified name
 * @param source_len Length of source
 * @param target Target qualified name
 * @param target_len Length of target
 */
void sysml2_capture_flow(
    SysmlBuildContext *ctx,
    const char *payload, size_t payload_len,
    const char *source, size_t source_len,
    const char *target, size_t target_len
);

/*
 * Capture a succession statement (first x then y)
 *
 * @param ctx Build context
 * @param source Source qualified name
 * @param source_len Length of source
 * @param target Target qualified name
 * @param target_len Length of target
 * @param guard Guard expression (can be NULL)
 * @param guard_len Length of guard
 */
void sysml2_capture_succession(
    SysmlBuildContext *ctx,
    const char *source, size_t source_len,
    const char *target, size_t target_len,
    const char *guard, size_t guard_len
);

/*
 * Capture an entry action
 *
 * @param ctx Build context
 * @param text Raw text of the entry action body
 * @param len Length of text
 */
void sysml2_capture_entry(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture an exit action
 *
 * @param ctx Build context
 * @param text Raw text of the exit action body
 * @param len Length of text
 */
void sysml2_capture_exit(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture a do action
 *
 * @param ctx Build context
 * @param text Raw text of the do action body
 * @param len Length of text
 */
void sysml2_capture_do(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture a transition
 *
 * @param ctx Build context
 * @param text Raw text of the transition
 * @param len Length of text
 */
void sysml2_capture_transition(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture an entry transition (then X; without entry keyword)
 *
 * @param ctx Build context
 * @param text Raw text of the transition
 * @param len Length of text
 */
void sysml2_capture_entry_transition(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture a send action
 *
 * @param ctx Build context
 * @param text Raw text of the send action
 * @param len Length of text
 */
void sysml2_capture_send(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture an accept action
 *
 * @param ctx Build context
 * @param text Raw text of the accept action
 * @param len Length of text
 */
void sysml2_capture_accept_action(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture an assignment action
 *
 * @param ctx Build context
 * @param target Target variable
 * @param target_len Length of target
 * @param expr Expression to assign
 * @param expr_len Length of expression
 */
void sysml2_capture_assign(
    SysmlBuildContext *ctx,
    const char *target, size_t target_len,
    const char *expr, size_t expr_len
);

/*
 * Capture an if action
 *
 * @param ctx Build context
 * @param text Raw text of the if action
 * @param len Length of text
 */
void sysml2_capture_if(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture a while/loop
 *
 * @param ctx Build context
 * @param text Raw text of the while/loop
 * @param len Length of text
 */
void sysml2_capture_while(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture a for loop
 *
 * @param ctx Build context
 * @param text Raw text of the for loop
 * @param len Length of text
 */
void sysml2_capture_for(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture a control node (merge, decide, join, fork)
 *
 * @param ctx Build context
 * @param kind Statement kind (SYSML_STMT_MERGE, etc.)
 * @param text Raw text of the control node
 * @param len Length of text
 */
void sysml2_capture_control_node(
    SysmlBuildContext *ctx,
    SysmlStatementKind kind,
    const char *text, size_t len
);

/*
 * Capture a terminate action
 *
 * @param ctx Build context
 */
void sysml2_capture_terminate(SysmlBuildContext *ctx);

/*
 * Capture a named comment
 *
 * @param ctx Build context
 * @param name Comment name (can be NULL)
 * @param name_len Length of name
 * @param about Target reference (can be NULL)
 * @param about_len Length of about
 * @param text Comment text including delimiters
 * @param text_len Length of text
 */
void sysml2_capture_named_comment(
    SysmlBuildContext *ctx,
    const char *name, size_t name_len,
    const char *about, size_t about_len,
    const char *text, size_t text_len
);

/*
 * Capture a textual representation
 *
 * @param ctx Build context
 * @param name Rep name (can be NULL)
 * @param name_len Length of name
 * @param lang Language string including quotes
 * @param lang_len Length of lang
 * @param text Content text including delimiters
 * @param text_len Length of text
 */
void sysml2_capture_textual_rep(
    SysmlBuildContext *ctx,
    const char *name, size_t name_len,
    const char *lang, size_t lang_len,
    const char *text, size_t text_len
);

/*
 * Capture a result expression (bare expression at end of calc/constraint body)
 *
 * @param ctx Build context
 * @param expr Expression text
 * @param len Length of expression
 */
void sysml2_capture_result_expr(SysmlBuildContext *ctx, const char *expr, size_t len);

/*
 * Capture a standalone metadata usage (metadata X about Y, Z;)
 *
 * @param ctx Build context
 * @param text Raw text of the metadata usage
 * @param len Length of text
 */
void sysml2_capture_metadata_usage(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture a shorthand feature (:> name : Type; or :>> name = value;)
 *
 * @param ctx Build context
 * @param text Raw text of the shorthand feature
 * @param len Length of text
 */
void sysml2_capture_shorthand_feature(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Attach pending body statements to a node
 *
 * @param ctx Build context
 * @param node Node to attach statements to
 */
void sysml2_attach_pending_stmts(SysmlBuildContext *ctx, SysmlNode *node);

/*
 * Capture a require constraint statement
 *
 * @param ctx Build context
 * @param text Raw text of the require constraint
 * @param len Length of text
 */
void sysml2_capture_require_constraint(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture an assume constraint statement
 *
 * @param ctx Build context
 * @param text Raw text of the assume constraint
 * @param len Length of text
 */
void sysml2_capture_assume_constraint(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture a subject usage statement
 *
 * @param ctx Build context
 * @param text Raw text of the subject usage
 * @param len Length of text
 */
void sysml2_capture_subject(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture an end member statement
 *
 * @param ctx Build context
 * @param text Raw text of the end member
 * @param len Length of text
 */
void sysml2_capture_end_member(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture a return usage statement
 *
 * @param ctx Build context
 * @param text Raw text of the return usage
 * @param len Length of text
 */
void sysml2_capture_return_usage(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture an action parameter list
 *
 * @param ctx Build context
 * @param text Raw text of the parameter list including parentheses
 * @param len Length of text
 */
void sysml2_capture_action_params(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture a flow payload type
 *
 * Will be used by the next flow capture.
 *
 * @param ctx Build context
 * @param text Raw text of the payload type
 * @param len Length of text
 */
void sysml2_capture_flow_payload(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture an actor usage statement
 *
 * @param ctx Build context
 * @param text Raw text of the actor usage
 * @param len Length of text
 */
void sysml2_capture_actor(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture a stakeholder usage statement
 *
 * @param ctx Build context
 * @param text Raw text of the stakeholder usage
 * @param len Length of text
 */
void sysml2_capture_stakeholder(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture an objective usage statement
 *
 * @param ctx Build context
 * @param text Raw text of the objective usage
 * @param len Length of text
 */
void sysml2_capture_objective(SysmlBuildContext *ctx, const char *text, size_t len);

/*
 * Capture a frame usage statement
 *
 * @param ctx Build context
 * @param text Raw text of the frame usage
 * @param len Length of text
 */
void sysml2_capture_frame(SysmlBuildContext *ctx, const char *text, size_t len);

#endif /* SYSML2_AST_BUILDER_H */
