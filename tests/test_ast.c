/*
 * SysML v2 Parser - AST and JSON Writer Tests
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/common.h"
#include "sysml2/arena.h"
#include "sysml2/intern.h"
#include "sysml2/ast.h"
#include "sysml2/ast_builder.h"
#include "sysml2/json_writer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s...", #name); \
    fflush(stdout); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf(" PASSED\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("\n    FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)
#define ASSERT_NULL(a) ASSERT((a) == NULL)
#define ASSERT_NOT_NULL(a) ASSERT((a) != NULL)

/* ========== Kind to JSON Type Tests ========== */

TEST(kind_to_json_type_package) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_PACKAGE), "Package");
}

TEST(kind_to_json_type_library_package) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_LIBRARY_PACKAGE), "LibraryPackage");
}

TEST(kind_to_json_type_part_def) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_PART_DEF), "PartDef");
}

TEST(kind_to_json_type_part_usage) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_PART_USAGE), "Part");
}

TEST(kind_to_json_type_action_def) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_ACTION_DEF), "ActionDef");
}

TEST(kind_to_json_type_action_usage) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_ACTION_USAGE), "Action");
}

TEST(kind_to_json_type_state_def) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_STATE_DEF), "StateDef");
}

TEST(kind_to_json_type_state_usage) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_STATE_USAGE), "State");
}

TEST(kind_to_json_type_port_def) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_PORT_DEF), "PortDef");
}

TEST(kind_to_json_type_port_usage) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_PORT_USAGE), "Port");
}

TEST(kind_to_json_type_requirement_def) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_REQUIREMENT_DEF), "RequirementDef");
}

TEST(kind_to_json_type_requirement_usage) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_REQUIREMENT_USAGE), "Requirement");
}

TEST(kind_to_json_type_constraint_def) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_CONSTRAINT_DEF), "ConstraintDef");
}

TEST(kind_to_json_type_constraint_usage) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_CONSTRAINT_USAGE), "Constraint");
}

TEST(kind_to_json_type_connection_rel) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_REL_CONNECTION), "Connection");
}

TEST(kind_to_json_type_flow_rel) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_REL_FLOW), "Flow");
}

TEST(kind_to_json_type_unknown) {
    ASSERT_STR_EQ(sysml2_kind_to_json_type(SYSML_KIND_UNKNOWN), "Unknown");
}

/* ========== Kind to String Tests ========== */

TEST(kind_to_string_part_def) {
    ASSERT_STR_EQ(sysml2_kind_to_string(SYSML_KIND_PART_DEF), "PartDefinition");
}

TEST(kind_to_string_part_usage) {
    ASSERT_STR_EQ(sysml2_kind_to_string(SYSML_KIND_PART_USAGE), "PartUsage");
}

TEST(kind_to_string_connection_rel) {
    ASSERT_STR_EQ(sysml2_kind_to_string(SYSML_KIND_REL_CONNECTION), "ConnectionRelationship");
}

/* ========== Kind Classification Macro Tests ========== */

TEST(kind_is_package) {
    ASSERT(SYSML_KIND_IS_PACKAGE(SYSML_KIND_PACKAGE));
    ASSERT(SYSML_KIND_IS_PACKAGE(SYSML_KIND_LIBRARY_PACKAGE));
    ASSERT(!SYSML_KIND_IS_PACKAGE(SYSML_KIND_PART_DEF));
    ASSERT(!SYSML_KIND_IS_PACKAGE(SYSML_KIND_PART_USAGE));
}

TEST(kind_is_definition) {
    ASSERT(SYSML_KIND_IS_DEFINITION(SYSML_KIND_PART_DEF));
    ASSERT(SYSML_KIND_IS_DEFINITION(SYSML_KIND_ACTION_DEF));
    ASSERT(SYSML_KIND_IS_DEFINITION(SYSML_KIND_STATE_DEF));
    ASSERT(SYSML_KIND_IS_DEFINITION(SYSML_KIND_REQUIREMENT_DEF));
    ASSERT(!SYSML_KIND_IS_DEFINITION(SYSML_KIND_PACKAGE));
    ASSERT(!SYSML_KIND_IS_DEFINITION(SYSML_KIND_PART_USAGE));
}

TEST(kind_is_usage) {
    ASSERT(SYSML_KIND_IS_USAGE(SYSML_KIND_PART_USAGE));
    ASSERT(SYSML_KIND_IS_USAGE(SYSML_KIND_ACTION_USAGE));
    ASSERT(SYSML_KIND_IS_USAGE(SYSML_KIND_STATE_USAGE));
    ASSERT(!SYSML_KIND_IS_USAGE(SYSML_KIND_PART_DEF));
    ASSERT(!SYSML_KIND_IS_USAGE(SYSML_KIND_REL_CONNECTION));
}

TEST(kind_is_relationship) {
    ASSERT(SYSML_KIND_IS_RELATIONSHIP(SYSML_KIND_REL_CONNECTION));
    ASSERT(SYSML_KIND_IS_RELATIONSHIP(SYSML_KIND_REL_FLOW));
    ASSERT(SYSML_KIND_IS_RELATIONSHIP(SYSML_KIND_REL_ALLOCATION));
    ASSERT(SYSML_KIND_IS_RELATIONSHIP(SYSML_KIND_REL_SATISFY));
    ASSERT(!SYSML_KIND_IS_RELATIONSHIP(SYSML_KIND_PART_USAGE));
}

/* ========== Build Context Tests ========== */

TEST(build_context_create) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test.sysml");
    ASSERT_NOT_NULL(ctx);
    ASSERT_NULL(sysml2_build_current_scope(ctx));
    ASSERT_EQ(ctx->element_count, 0);
    ASSERT_EQ(ctx->relationship_count, 0);

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(build_context_source_name) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "myfile.sysml");
    ASSERT_STR_EQ(ctx->source_name, "myfile.sysml");

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

/* ========== Scope Stack Tests ========== */

TEST(scope_push_pop) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    ASSERT_NULL(sysml2_build_current_scope(ctx));

    sysml2_build_push_scope(ctx, "Package");
    ASSERT_STR_EQ(sysml2_build_current_scope(ctx), "Package");

    sysml2_build_push_scope(ctx, "Package::Inner");
    ASSERT_STR_EQ(sysml2_build_current_scope(ctx), "Package::Inner");

    sysml2_build_pop_scope(ctx);
    ASSERT_STR_EQ(sysml2_build_current_scope(ctx), "Package");

    sysml2_build_pop_scope(ctx);
    ASSERT_NULL(sysml2_build_current_scope(ctx));

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(scope_depth) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    ASSERT_EQ(ctx->scope_depth, 0);

    sysml2_build_push_scope(ctx, "A");
    ASSERT_EQ(ctx->scope_depth, 1);

    sysml2_build_push_scope(ctx, "A::B");
    ASSERT_EQ(ctx->scope_depth, 2);

    sysml2_build_push_scope(ctx, "A::B::C");
    ASSERT_EQ(ctx->scope_depth, 3);

    sysml2_build_pop_scope(ctx);
    ASSERT_EQ(ctx->scope_depth, 2);

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(scope_pop_at_root_is_safe) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    /* Should not crash when popping at root */
    sysml2_build_pop_scope(ctx);
    sysml2_build_pop_scope(ctx);
    ASSERT_EQ(ctx->scope_depth, 0);

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

/* ========== ID Generation Tests ========== */

TEST(make_id_at_root) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    const char *id = sysml2_build_make_id(ctx, "MyPackage");
    ASSERT_STR_EQ(id, "MyPackage");

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(make_id_in_scope) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    sysml2_build_push_scope(ctx, "Parent");
    const char *id = sysml2_build_make_id(ctx, "Child");
    ASSERT_STR_EQ(id, "Parent::Child");

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(make_id_nested) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    sysml2_build_push_scope(ctx, "Pkg");
    sysml2_build_push_scope(ctx, "Pkg::PartDef");
    const char *id = sysml2_build_make_id(ctx, "attr");
    ASSERT_STR_EQ(id, "Pkg::PartDef::attr");

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(make_id_anonymous) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    const char *id1 = sysml2_build_make_id(ctx, NULL);
    ASSERT_STR_EQ(id1, "_anon_1");

    const char *id2 = sysml2_build_make_id(ctx, NULL);
    ASSERT_STR_EQ(id2, "_anon_2");

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(make_id_anonymous_in_scope) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    sysml2_build_push_scope(ctx, "Pkg");
    const char *id = sysml2_build_make_id(ctx, NULL);
    ASSERT_STR_EQ(id, "Pkg::_anon_1");

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(make_rel_id) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    const char *id1 = sysml2_build_make_rel_id(ctx, "conn");
    ASSERT_STR_EQ(id1, "_conn_1");

    const char *id2 = sysml2_build_make_rel_id(ctx, "flow");
    ASSERT_STR_EQ(id2, "_flow_2");

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(make_rel_id_in_scope) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    sysml2_build_push_scope(ctx, "Pkg");
    const char *id = sysml2_build_make_rel_id(ctx, "conn");
    ASSERT_STR_EQ(id, "Pkg::_conn_1");

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

/* ========== Node Creation Tests ========== */

TEST(node_creation) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    SysmlNode *node = sysml2_build_node(ctx, SYSML_KIND_PACKAGE, "MyPackage");
    ASSERT_NOT_NULL(node);
    ASSERT_STR_EQ(node->id, "MyPackage");
    ASSERT_STR_EQ(node->name, "MyPackage");
    ASSERT_EQ(node->kind, SYSML_KIND_PACKAGE);
    ASSERT_NULL(node->parent_id);
    ASSERT_NULL(node->typed_by);
    ASSERT_EQ(node->typed_by_count, 0);

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(node_with_parent) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    sysml2_build_push_scope(ctx, "Pkg");
    SysmlNode *node = sysml2_build_node(ctx, SYSML_KIND_PART_DEF, "Engine");
    ASSERT_NOT_NULL(node);
    ASSERT_STR_EQ(node->id, "Pkg::Engine");
    ASSERT_STR_EQ(node->name, "Engine");
    ASSERT_STR_EQ(node->parent_id, "Pkg");

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(node_anonymous) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    SysmlNode *node = sysml2_build_node(ctx, SYSML_KIND_PART_USAGE, NULL);
    ASSERT_NOT_NULL(node);
    ASSERT_STR_EQ(node->id, "_anon_1");
    ASSERT_NULL(node->name);

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

/* ========== Element Collection Tests ========== */

TEST(add_element) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    SysmlNode *node = sysml2_build_node(ctx, SYSML_KIND_PACKAGE, "Pkg");
    sysml2_build_add_element(ctx, node);
    ASSERT_EQ(ctx->element_count, 1);
    ASSERT_EQ(ctx->elements[0], node);

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(add_multiple_elements) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Element%d", i);
        SysmlNode *node = sysml2_build_node(ctx, SYSML_KIND_PART_DEF, name);
        sysml2_build_add_element(ctx, node);
    }
    ASSERT_EQ(ctx->element_count, 10);

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

/* ========== Type Specialization Tests ========== */

TEST(typed_by_single) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    SysmlNode *node = sysml2_build_node(ctx, SYSML_KIND_PART_USAGE, "engine");
    sysml2_build_add_typed_by(ctx, node, "Engine");

    ASSERT_EQ(node->typed_by_count, 1);
    ASSERT_STR_EQ(node->typed_by[0], "Engine");

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(typed_by_multiple) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    SysmlNode *node = sysml2_build_node(ctx, SYSML_KIND_PART_USAGE, "myPart");
    sysml2_build_add_typed_by(ctx, node, "TypeA");
    sysml2_build_add_typed_by(ctx, node, "TypeB");

    ASSERT_EQ(node->typed_by_count, 2);
    ASSERT_STR_EQ(node->typed_by[0], "TypeA");
    ASSERT_STR_EQ(node->typed_by[1], "TypeB");

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

/* ========== Relationship Tests ========== */

TEST(relationship_creation) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    SysmlRelationship *rel = sysml2_build_relationship(ctx,
        SYSML_KIND_REL_CONNECTION, "partA.portX", "partB.portY");

    ASSERT_NOT_NULL(rel);
    ASSERT_EQ(rel->kind, SYSML_KIND_REL_CONNECTION);
    ASSERT_STR_EQ(rel->source, "partA.portX");
    ASSERT_STR_EQ(rel->target, "partB.portY");
    ASSERT_STR_EQ(rel->id, "_conn_1");

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(relationship_flow) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    SysmlRelationship *rel = sysml2_build_relationship(ctx,
        SYSML_KIND_REL_FLOW, "src", "dst");

    ASSERT_STR_EQ(rel->id, "_flow_1");

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

TEST(add_relationship) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test");

    SysmlRelationship *rel = sysml2_build_relationship(ctx,
        SYSML_KIND_REL_CONNECTION, "a", "b");
    sysml2_build_add_relationship(ctx, rel);

    ASSERT_EQ(ctx->relationship_count, 1);
    ASSERT_EQ(ctx->relationships[0], rel);

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

/* ========== Finalize Tests ========== */

TEST(build_finalize) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlBuildContext *ctx = sysml2_build_context_create(&arena, &intern, "test.sysml");

    SysmlNode *pkg = sysml2_build_node(ctx, SYSML_KIND_PACKAGE, "Pkg");
    sysml2_build_add_element(ctx, pkg);

    SysmlRelationship *rel = sysml2_build_relationship(ctx,
        SYSML_KIND_REL_CONNECTION, "a", "b");
    sysml2_build_add_relationship(ctx, rel);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx);

    ASSERT_NOT_NULL(model);
    ASSERT_STR_EQ(model->source_name, "test.sysml");
    ASSERT_EQ(model->element_count, 1);
    ASSERT_EQ(model->relationship_count, 1);

    sysml2_build_context_destroy(ctx);
    sysml2_arena_destroy(&arena);
}

/* ========== JSON Escape String Tests ========== */

TEST(json_escape_plain_string) {
    char buf[256];
    size_t len = sysml2_json_escape_string("hello", buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "hello");
    ASSERT_EQ(len, 5);
}

TEST(json_escape_quote) {
    char buf[256];
    sysml2_json_escape_string("hello\"world", buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "hello\\\"world");
}

TEST(json_escape_backslash) {
    char buf[256];
    sysml2_json_escape_string("path\\file", buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "path\\\\file");
}

TEST(json_escape_newline) {
    char buf[256];
    sysml2_json_escape_string("line1\nline2", buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "line1\\nline2");
}

TEST(json_escape_tab) {
    char buf[256];
    sysml2_json_escape_string("col1\tcol2", buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "col1\\tcol2");
}

TEST(json_escape_carriage_return) {
    char buf[256];
    sysml2_json_escape_string("line\r\n", buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "line\\r\\n");
}

TEST(json_escape_control_char) {
    char buf[256];
    /* Use octal escape to avoid \x01e being parsed as \x1e */
    char input[] = "test\001end";
    sysml2_json_escape_string(input, buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "test\\u0001end");
}

TEST(json_escape_null_string) {
    char buf[256];
    size_t len = sysml2_json_escape_string(NULL, buf, sizeof(buf));
    ASSERT_EQ(len, 0);
}

TEST(json_escape_empty_string) {
    char buf[256];
    size_t len = sysml2_json_escape_string("", buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "");
    ASSERT_EQ(len, 0);
}

TEST(json_escape_multiple_specials) {
    char buf[256];
    sysml2_json_escape_string("\"quoted\"\tand\\slashed", buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "\\\"quoted\\\"\\tand\\\\slashed");
}

/* ========== Main ========== */

int main(void) {
    printf("Running AST and JSON tests:\n");

    /* Kind to JSON type tests */
    printf("\n  Kind to JSON type tests:\n");
    RUN_TEST(kind_to_json_type_package);
    RUN_TEST(kind_to_json_type_library_package);
    RUN_TEST(kind_to_json_type_part_def);
    RUN_TEST(kind_to_json_type_part_usage);
    RUN_TEST(kind_to_json_type_action_def);
    RUN_TEST(kind_to_json_type_action_usage);
    RUN_TEST(kind_to_json_type_state_def);
    RUN_TEST(kind_to_json_type_state_usage);
    RUN_TEST(kind_to_json_type_port_def);
    RUN_TEST(kind_to_json_type_port_usage);
    RUN_TEST(kind_to_json_type_requirement_def);
    RUN_TEST(kind_to_json_type_requirement_usage);
    RUN_TEST(kind_to_json_type_constraint_def);
    RUN_TEST(kind_to_json_type_constraint_usage);
    RUN_TEST(kind_to_json_type_connection_rel);
    RUN_TEST(kind_to_json_type_flow_rel);
    RUN_TEST(kind_to_json_type_unknown);

    /* Kind to string tests */
    printf("\n  Kind to string tests:\n");
    RUN_TEST(kind_to_string_part_def);
    RUN_TEST(kind_to_string_part_usage);
    RUN_TEST(kind_to_string_connection_rel);

    /* Kind classification tests */
    printf("\n  Kind classification tests:\n");
    RUN_TEST(kind_is_package);
    RUN_TEST(kind_is_definition);
    RUN_TEST(kind_is_usage);
    RUN_TEST(kind_is_relationship);

    /* Build context tests */
    printf("\n  Build context tests:\n");
    RUN_TEST(build_context_create);
    RUN_TEST(build_context_source_name);

    /* Scope stack tests */
    printf("\n  Scope stack tests:\n");
    RUN_TEST(scope_push_pop);
    RUN_TEST(scope_depth);
    RUN_TEST(scope_pop_at_root_is_safe);

    /* ID generation tests */
    printf("\n  ID generation tests:\n");
    RUN_TEST(make_id_at_root);
    RUN_TEST(make_id_in_scope);
    RUN_TEST(make_id_nested);
    RUN_TEST(make_id_anonymous);
    RUN_TEST(make_id_anonymous_in_scope);
    RUN_TEST(make_rel_id);
    RUN_TEST(make_rel_id_in_scope);

    /* Node creation tests */
    printf("\n  Node creation tests:\n");
    RUN_TEST(node_creation);
    RUN_TEST(node_with_parent);
    RUN_TEST(node_anonymous);

    /* Element collection tests */
    printf("\n  Element collection tests:\n");
    RUN_TEST(add_element);
    RUN_TEST(add_multiple_elements);

    /* Type specialization tests */
    printf("\n  Type specialization tests:\n");
    RUN_TEST(typed_by_single);
    RUN_TEST(typed_by_multiple);

    /* Relationship tests */
    printf("\n  Relationship tests:\n");
    RUN_TEST(relationship_creation);
    RUN_TEST(relationship_flow);
    RUN_TEST(add_relationship);

    /* Finalize tests */
    printf("\n  Finalize tests:\n");
    RUN_TEST(build_finalize);

    /* JSON escape string tests */
    printf("\n  JSON escape string tests:\n");
    RUN_TEST(json_escape_plain_string);
    RUN_TEST(json_escape_quote);
    RUN_TEST(json_escape_backslash);
    RUN_TEST(json_escape_newline);
    RUN_TEST(json_escape_tab);
    RUN_TEST(json_escape_carriage_return);
    RUN_TEST(json_escape_control_char);
    RUN_TEST(json_escape_null_string);
    RUN_TEST(json_escape_empty_string);
    RUN_TEST(json_escape_multiple_specials);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
