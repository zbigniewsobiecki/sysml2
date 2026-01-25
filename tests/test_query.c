/*
 * SysML v2 Parser - Query Tests
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/common.h"
#include "sysml2/arena.h"
#include "sysml2/ast.h"
#include "sysml2/query.h"

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
#define ASSERT_TRUE(a) ASSERT((a) == true)
#define ASSERT_FALSE(a) ASSERT((a) == false)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)
#define ASSERT_NULL(a) ASSERT((a) == NULL)
#define ASSERT_NOT_NULL(a) ASSERT((a) != NULL)

/* Test fixture macros for arena setup and teardown */
#define FIXTURE_ARENA_SETUP() \
    Sysml2Arena arena; \
    sysml2_arena_init(&arena)

#define FIXTURE_ARENA_TEARDOWN() \
    sysml2_arena_destroy(&arena)

/* ========== Pattern Parsing Tests ========== */

TEST(parse_exact_pattern) {
    FIXTURE_ARENA_SETUP();

    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::Element", &arena);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(p->kind, SYSML2_QUERY_EXACT);
    ASSERT_STR_EQ(p->base_path, "Pkg::Element");
    ASSERT_NULL(p->next);

    FIXTURE_ARENA_TEARDOWN();
}

TEST(parse_direct_wildcard_pattern) {
    FIXTURE_ARENA_SETUP();

    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::*", &arena);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(p->kind, SYSML2_QUERY_DIRECT);
    ASSERT_STR_EQ(p->base_path, "Pkg");
    ASSERT_NULL(p->next);

    FIXTURE_ARENA_TEARDOWN();
}

TEST(parse_recursive_wildcard_pattern) {
    FIXTURE_ARENA_SETUP();

    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::**", &arena);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(p->kind, SYSML2_QUERY_RECURSIVE);
    ASSERT_STR_EQ(p->base_path, "Pkg");
    ASSERT_NULL(p->next);

    FIXTURE_ARENA_TEARDOWN();
}

TEST(parse_nested_exact_pattern) {
    FIXTURE_ARENA_SETUP();

    Sysml2QueryPattern *p = sysml2_query_parse("DataModel::Entities::Car", &arena);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(p->kind, SYSML2_QUERY_EXACT);
    ASSERT_STR_EQ(p->base_path, "DataModel::Entities::Car");

    FIXTURE_ARENA_TEARDOWN();
}

TEST(parse_nested_direct_pattern) {
    FIXTURE_ARENA_SETUP();

    Sysml2QueryPattern *p = sysml2_query_parse("DataModel::Entities::*", &arena);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(p->kind, SYSML2_QUERY_DIRECT);
    ASSERT_STR_EQ(p->base_path, "DataModel::Entities");

    FIXTURE_ARENA_TEARDOWN();
}

TEST(parse_multi_patterns) {
    FIXTURE_ARENA_SETUP();

    const char *patterns[] = {"Pkg::A", "Pkg::B", "Other::*"};
    Sysml2QueryPattern *p = sysml2_query_parse_multi(patterns, 3, &arena);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(p->kind, SYSML2_QUERY_EXACT);
    ASSERT_STR_EQ(p->base_path, "Pkg::A");

    ASSERT_NOT_NULL(p->next);
    ASSERT_EQ(p->next->kind, SYSML2_QUERY_EXACT);
    ASSERT_STR_EQ(p->next->base_path, "Pkg::B");

    ASSERT_NOT_NULL(p->next->next);
    ASSERT_EQ(p->next->next->kind, SYSML2_QUERY_DIRECT);
    ASSERT_STR_EQ(p->next->next->base_path, "Other");
    ASSERT_NULL(p->next->next->next);

    FIXTURE_ARENA_TEARDOWN();
}

/* ========== Pattern Matching Tests ========== */

TEST(match_exact_success) {
    FIXTURE_ARENA_SETUP();

    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::Element", &arena);
    ASSERT_TRUE(sysml2_query_matches(p, "Pkg::Element"));

    FIXTURE_ARENA_TEARDOWN();
}

TEST(match_exact_failure_different) {
    FIXTURE_ARENA_SETUP();

    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::Element", &arena);
    ASSERT_FALSE(sysml2_query_matches(p, "Pkg::Other"));

    FIXTURE_ARENA_TEARDOWN();
}

TEST(match_exact_failure_child) {
    FIXTURE_ARENA_SETUP();

    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::Element", &arena);
    ASSERT_FALSE(sysml2_query_matches(p, "Pkg::Element::Child"));

    FIXTURE_ARENA_TEARDOWN();
}

TEST(match_direct_success) {
    FIXTURE_ARENA_SETUP();

    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::*", &arena);
    ASSERT_TRUE(sysml2_query_matches(p, "Pkg::A"));
    ASSERT_TRUE(sysml2_query_matches(p, "Pkg::B"));
    ASSERT_TRUE(sysml2_query_matches(p, "Pkg::Element"));

    FIXTURE_ARENA_TEARDOWN();
}

TEST(match_direct_failure_grandchild) {
    FIXTURE_ARENA_SETUP();

    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::*", &arena);
    ASSERT_FALSE(sysml2_query_matches(p, "Pkg::A::B"));
    ASSERT_FALSE(sysml2_query_matches(p, "Pkg::Element::Child"));

    FIXTURE_ARENA_TEARDOWN();
}

TEST(match_direct_failure_base) {
    FIXTURE_ARENA_SETUP();

    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::*", &arena);
    ASSERT_FALSE(sysml2_query_matches(p, "Pkg"));  /* The base itself doesn't match */

    FIXTURE_ARENA_TEARDOWN();
}

TEST(match_recursive_success) {
    FIXTURE_ARENA_SETUP();

    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::**", &arena);
    ASSERT_TRUE(sysml2_query_matches(p, "Pkg"));  /* Base matches for recursive */
    ASSERT_TRUE(sysml2_query_matches(p, "Pkg::A"));
    ASSERT_TRUE(sysml2_query_matches(p, "Pkg::A::B"));
    ASSERT_TRUE(sysml2_query_matches(p, "Pkg::A::B::C"));
    ASSERT_TRUE(sysml2_query_matches(p, "Pkg::Element::Child::Grandchild"));

    FIXTURE_ARENA_TEARDOWN();
}

TEST(match_recursive_failure_other_pkg) {
    FIXTURE_ARENA_SETUP();

    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::**", &arena);
    ASSERT_FALSE(sysml2_query_matches(p, "Other::A"));
    ASSERT_FALSE(sysml2_query_matches(p, "PkgExtra::A"));  /* Prefix but not same */

    FIXTURE_ARENA_TEARDOWN();
}

TEST(match_any_patterns) {
    FIXTURE_ARENA_SETUP();

    const char *patterns[] = {"A::X", "B::*", "C::**"};
    Sysml2QueryPattern *p = sysml2_query_parse_multi(patterns, 3, &arena);

    /* Matches first pattern */
    ASSERT_TRUE(sysml2_query_matches_any(p, "A::X"));
    ASSERT_FALSE(sysml2_query_matches_any(p, "A::Y"));

    /* Matches second pattern */
    ASSERT_TRUE(sysml2_query_matches_any(p, "B::Y"));
    ASSERT_FALSE(sysml2_query_matches_any(p, "B::Y::Z"));

    /* Matches third pattern */
    ASSERT_TRUE(sysml2_query_matches_any(p, "C"));
    ASSERT_TRUE(sysml2_query_matches_any(p, "C::D"));
    ASSERT_TRUE(sysml2_query_matches_any(p, "C::D::E"));

    /* Matches none */
    ASSERT_FALSE(sysml2_query_matches_any(p, "D::X"));
    ASSERT_FALSE(sysml2_query_matches_any(p, "Other"));

    FIXTURE_ARENA_TEARDOWN();
}

/* ========== Parent Path Tests ========== */

TEST(parent_path_nested) {
    FIXTURE_ARENA_SETUP();

    const char *parent = sysml2_query_parent_path("A::B::C", &arena);
    ASSERT_NOT_NULL(parent);
    ASSERT_STR_EQ(parent, "A::B");

    FIXTURE_ARENA_TEARDOWN();
}

TEST(parent_path_top_level) {
    FIXTURE_ARENA_SETUP();

    const char *parent = sysml2_query_parent_path("TopLevel", &arena);
    ASSERT_NULL(parent);

    FIXTURE_ARENA_TEARDOWN();
}

TEST(parent_path_one_level) {
    FIXTURE_ARENA_SETUP();

    const char *parent = sysml2_query_parent_path("Parent::Child", &arena);
    ASSERT_NOT_NULL(parent);
    ASSERT_STR_EQ(parent, "Parent");

    FIXTURE_ARENA_TEARDOWN();
}

/* ========== Query Execution Tests ========== */

TEST(execute_exact_query) {
    FIXTURE_ARENA_SETUP();

    /* Create a simple model */
    SysmlSemanticModel model = {0};
    SysmlNode nodes[3] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::B", .name = "B", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
    };
    SysmlNode *node_ptrs[3] = {&nodes[0], &nodes[1], &nodes[2]};
    model.elements = node_ptrs;
    model.element_count = 3;

    SysmlSemanticModel *models[] = {&model};

    /* Query for Pkg::A */
    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::A", &arena);
    Sysml2QueryResult *result = sysml2_query_execute(p, models, 1, &arena);

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(result->element_count, 1);
    ASSERT_STR_EQ(result->elements[0]->id, "Pkg::A");

    FIXTURE_ARENA_TEARDOWN();
}

TEST(execute_direct_query) {
    FIXTURE_ARENA_SETUP();

    /* Create a model with nested structure */
    SysmlSemanticModel model = {0};
    SysmlNode nodes[5] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::B", .name = "B", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::A::Child", .name = "Child", .kind = SYSML_KIND_ATTRIBUTE_USAGE, .parent_id = "Pkg::A"},
        {.id = "Other", .name = "Other", .kind = SYSML_KIND_PACKAGE},
    };
    SysmlNode *node_ptrs[5] = {&nodes[0], &nodes[1], &nodes[2], &nodes[3], &nodes[4]};
    model.elements = node_ptrs;
    model.element_count = 5;

    SysmlSemanticModel *models[] = {&model};

    /* Query for Pkg::* (direct children only) */
    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::*", &arena);
    Sysml2QueryResult *result = sysml2_query_execute(p, models, 1, &arena);

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(result->element_count, 2);  /* Pkg::A and Pkg::B, not Pkg::A::Child */

    /* Verify the correct elements are in result */
    bool found_a = false, found_b = false;
    for (size_t i = 0; i < result->element_count; i++) {
        if (strcmp(result->elements[i]->id, "Pkg::A") == 0) found_a = true;
        if (strcmp(result->elements[i]->id, "Pkg::B") == 0) found_b = true;
    }
    ASSERT_TRUE(found_a);
    ASSERT_TRUE(found_b);

    FIXTURE_ARENA_TEARDOWN();
}

TEST(execute_recursive_query) {
    FIXTURE_ARENA_SETUP();

    /* Create a model with nested structure */
    SysmlSemanticModel model = {0};
    SysmlNode nodes[5] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::A::Child", .name = "Child", .kind = SYSML_KIND_ATTRIBUTE_USAGE, .parent_id = "Pkg::A"},
        {.id = "Pkg::A::Child::Deep", .name = "Deep", .kind = SYSML_KIND_ATTRIBUTE_USAGE, .parent_id = "Pkg::A::Child"},
        {.id = "Other", .name = "Other", .kind = SYSML_KIND_PACKAGE},
    };
    SysmlNode *node_ptrs[5] = {&nodes[0], &nodes[1], &nodes[2], &nodes[3], &nodes[4]};
    model.elements = node_ptrs;
    model.element_count = 5;

    SysmlSemanticModel *models[] = {&model};

    /* Query for Pkg::** (all descendants) */
    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::**", &arena);
    Sysml2QueryResult *result = sysml2_query_execute(p, models, 1, &arena);

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(result->element_count, 4);  /* Pkg, Pkg::A, Pkg::A::Child, Pkg::A::Child::Deep */

    FIXTURE_ARENA_TEARDOWN();
}

TEST(execute_multi_pattern_query) {
    FIXTURE_ARENA_SETUP();

    /* Create a model */
    SysmlSemanticModel model = {0};
    SysmlNode nodes[4] = {
        {.id = "A", .name = "A", .kind = SYSML_KIND_PACKAGE},
        {.id = "A::X", .name = "X", .kind = SYSML_KIND_PART_DEF, .parent_id = "A"},
        {.id = "B", .name = "B", .kind = SYSML_KIND_PACKAGE},
        {.id = "B::Y", .name = "Y", .kind = SYSML_KIND_PART_DEF, .parent_id = "B"},
    };
    SysmlNode *node_ptrs[4] = {&nodes[0], &nodes[1], &nodes[2], &nodes[3]};
    model.elements = node_ptrs;
    model.element_count = 4;

    SysmlSemanticModel *models[] = {&model};

    /* Query for A::X and B::Y */
    const char *patterns[] = {"A::X", "B::Y"};
    Sysml2QueryPattern *p = sysml2_query_parse_multi(patterns, 2, &arena);
    Sysml2QueryResult *result = sysml2_query_execute(p, models, 1, &arena);

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(result->element_count, 2);

    FIXTURE_ARENA_TEARDOWN();
}

TEST(result_contains) {
    FIXTURE_ARENA_SETUP();

    SysmlSemanticModel model = {0};
    SysmlNode nodes[2] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
    };
    SysmlNode *node_ptrs[2] = {&nodes[0], &nodes[1]};
    model.elements = node_ptrs;
    model.element_count = 2;

    SysmlSemanticModel *models[] = {&model};

    Sysml2QueryPattern *p = sysml2_query_parse("Pkg::A", &arena);
    Sysml2QueryResult *result = sysml2_query_execute(p, models, 1, &arena);

    ASSERT_TRUE(sysml2_query_result_contains(result, "Pkg::A"));
    ASSERT_FALSE(sysml2_query_result_contains(result, "Pkg"));
    ASSERT_FALSE(sysml2_query_result_contains(result, "Other"));

    FIXTURE_ARENA_TEARDOWN();
}

/* ========== Main ========== */

int main(void) {
    printf("Running query tests...\n");

    /* Pattern parsing tests */
    RUN_TEST(parse_exact_pattern);
    RUN_TEST(parse_direct_wildcard_pattern);
    RUN_TEST(parse_recursive_wildcard_pattern);
    RUN_TEST(parse_nested_exact_pattern);
    RUN_TEST(parse_nested_direct_pattern);
    RUN_TEST(parse_multi_patterns);

    /* Pattern matching tests */
    RUN_TEST(match_exact_success);
    RUN_TEST(match_exact_failure_different);
    RUN_TEST(match_exact_failure_child);
    RUN_TEST(match_direct_success);
    RUN_TEST(match_direct_failure_grandchild);
    RUN_TEST(match_direct_failure_base);
    RUN_TEST(match_recursive_success);
    RUN_TEST(match_recursive_failure_other_pkg);
    RUN_TEST(match_any_patterns);

    /* Parent path tests */
    RUN_TEST(parent_path_nested);
    RUN_TEST(parent_path_top_level);
    RUN_TEST(parent_path_one_level);

    /* Query execution tests */
    RUN_TEST(execute_exact_query);
    RUN_TEST(execute_direct_query);
    RUN_TEST(execute_recursive_query);
    RUN_TEST(execute_multi_pattern_query);
    RUN_TEST(result_contains);

    printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
