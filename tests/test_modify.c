/*
 * SysML v2 Parser - Modification Tests
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/common.h"
#include "sysml2/arena.h"
#include "sysml2/intern.h"
#include "sysml2/ast.h"
#include "sysml2/query.h"
#include "sysml2/modify.h"
#include "sysml2/pipeline.h"
#include "sysml2/sysml_writer.h"
#include "sysml2/cli.h"

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

/* Helper: Create a simple model for testing */
static SysmlSemanticModel *create_test_model(
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    SysmlNode *nodes,
    size_t node_count,
    SysmlRelationship *rels,
    size_t rel_count
) {
    SysmlSemanticModel *model = sysml2_arena_alloc(arena, sizeof(SysmlSemanticModel));
    if (!model) return NULL;
    memset(model, 0, sizeof(SysmlSemanticModel));

    model->source_name = sysml2_intern(intern, "test.sysml");

    if (node_count > 0) {
        model->elements = sysml2_arena_alloc(arena, node_count * sizeof(SysmlNode *));
        if (!model->elements) return NULL;
        for (size_t i = 0; i < node_count; i++) {
            model->elements[i] = &nodes[i];
        }
        model->element_count = node_count;
        model->element_capacity = node_count;
    }

    if (rel_count > 0) {
        model->relationships = sysml2_arena_alloc(arena, rel_count * sizeof(SysmlRelationship *));
        if (!model->relationships) return NULL;
        for (size_t i = 0; i < rel_count; i++) {
            model->relationships[i] = &rels[i];
        }
        model->relationship_count = rel_count;
        model->relationship_capacity = rel_count;
    }

    return model;
}

/* ========== Plan Tests ========== */

TEST(plan_create) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2ModifyPlan *plan = sysml2_modify_plan_create(&arena);
    ASSERT_NOT_NULL(plan);
    ASSERT_NULL(plan->delete_patterns);
    ASSERT_NULL(plan->set_ops);
    ASSERT_FALSE(plan->dry_run);

    sysml2_arena_destroy(&arena);
}

TEST(plan_add_delete_exact) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2ModifyPlan *plan = sysml2_modify_plan_create(&arena);
    Sysml2Result result = sysml2_modify_plan_add_delete(plan, "Pkg::Element");

    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(plan->delete_patterns);
    ASSERT_EQ(plan->delete_patterns->kind, SYSML2_QUERY_EXACT);
    ASSERT_STR_EQ(plan->delete_patterns->base_path, "Pkg::Element");

    sysml2_arena_destroy(&arena);
}

TEST(plan_add_delete_recursive) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2ModifyPlan *plan = sysml2_modify_plan_create(&arena);
    Sysml2Result result = sysml2_modify_plan_add_delete(plan, "Pkg::**");

    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(plan->delete_patterns);
    ASSERT_EQ(plan->delete_patterns->kind, SYSML2_QUERY_RECURSIVE);
    ASSERT_STR_EQ(plan->delete_patterns->base_path, "Pkg");

    sysml2_arena_destroy(&arena);
}

TEST(plan_add_multiple_deletes) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2ModifyPlan *plan = sysml2_modify_plan_create(&arena);
    sysml2_modify_plan_add_delete(plan, "A::X");
    sysml2_modify_plan_add_delete(plan, "B::*");
    sysml2_modify_plan_add_delete(plan, "C::**");

    ASSERT_NOT_NULL(plan->delete_patterns);
    ASSERT_EQ(plan->delete_patterns->kind, SYSML2_QUERY_EXACT);
    ASSERT_NOT_NULL(plan->delete_patterns->next);
    ASSERT_EQ(plan->delete_patterns->next->kind, SYSML2_QUERY_DIRECT);
    ASSERT_NOT_NULL(plan->delete_patterns->next->next);
    ASSERT_EQ(plan->delete_patterns->next->next->kind, SYSML2_QUERY_RECURSIVE);

    sysml2_arena_destroy(&arena);
}

/* ========== ID Helper Tests ========== */

TEST(id_starts_with_true) {
    ASSERT_TRUE(sysml2_modify_id_starts_with("Pkg::A::B", "Pkg"));
    ASSERT_TRUE(sysml2_modify_id_starts_with("Pkg::A::B", "Pkg::A"));
}

TEST(id_starts_with_false) {
    ASSERT_FALSE(sysml2_modify_id_starts_with("Pkg::A", "Pkg::AB"));  /* Not a proper prefix */
    ASSERT_FALSE(sysml2_modify_id_starts_with("Pkg::A", "Other"));
    ASSERT_FALSE(sysml2_modify_id_starts_with("Pkg", "Pkg"));  /* Same, not a prefix */
}

TEST(get_local_name) {
    ASSERT_STR_EQ(sysml2_modify_get_local_name("A::B::C"), "C");
    ASSERT_STR_EQ(sysml2_modify_get_local_name("A::B"), "B");
    ASSERT_STR_EQ(sysml2_modify_get_local_name("A"), "A");
}

TEST(remap_id_toplevel) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    const char *result = sysml2_modify_remap_id(NULL, "Target", &arena, &intern);
    ASSERT_STR_EQ(result, "Target");

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(remap_id_nested) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    const char *result = sysml2_modify_remap_id("A::B", "Target", &arena, &intern);
    ASSERT_STR_EQ(result, "Target::A::B");

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* ========== Delete Tests ========== */

TEST(delete_exact_element) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Create model: Pkg, Pkg::A, Pkg::B */
    SysmlNode nodes[3] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::B", .name = "B", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
    };
    SysmlSemanticModel *model = create_test_model(&arena, &intern, nodes, 3, NULL, 0);

    /* Delete Pkg::A */
    Sysml2QueryPattern *pattern = sysml2_query_parse("Pkg::A", &arena);
    size_t deleted_count = 0;
    SysmlSemanticModel *result = sysml2_modify_clone_with_deletions(
        model, pattern, &arena, &intern, &deleted_count
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(deleted_count, 1);
    ASSERT_EQ(result->element_count, 2);  /* Pkg and Pkg::B remain */

    /* Verify Pkg::A is gone */
    bool found_a = false;
    for (size_t i = 0; i < result->element_count; i++) {
        if (strcmp(result->elements[i]->id, "Pkg::A") == 0) found_a = true;
    }
    ASSERT_FALSE(found_a);

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(delete_cascades_to_children) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Create model: Pkg, Pkg::A, Pkg::A::Child */
    SysmlNode nodes[3] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::A::Child", .name = "Child", .kind = SYSML_KIND_ATTRIBUTE_USAGE, .parent_id = "Pkg::A"},
    };
    SysmlSemanticModel *model = create_test_model(&arena, &intern, nodes, 3, NULL, 0);

    /* Delete Pkg::A (should also delete Pkg::A::Child) */
    Sysml2QueryPattern *pattern = sysml2_query_parse("Pkg::A", &arena);
    size_t deleted_count = 0;
    SysmlSemanticModel *result = sysml2_modify_clone_with_deletions(
        model, pattern, &arena, &intern, &deleted_count
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(deleted_count, 2);  /* A and Child */
    ASSERT_EQ(result->element_count, 1);  /* Only Pkg remains */

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(delete_direct_children_only) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Create model: Pkg, Pkg::A, Pkg::B, Pkg::A::Child */
    SysmlNode nodes[4] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::B", .name = "B", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::A::Child", .name = "Child", .kind = SYSML_KIND_ATTRIBUTE_USAGE, .parent_id = "Pkg::A"},
    };
    SysmlSemanticModel *model = create_test_model(&arena, &intern, nodes, 4, NULL, 0);

    /* Delete Pkg::* (direct children A and B, but not Pkg itself) */
    Sysml2QueryPattern *pattern = sysml2_query_parse("Pkg::*", &arena);
    size_t deleted_count = 0;
    SysmlSemanticModel *result = sysml2_modify_clone_with_deletions(
        model, pattern, &arena, &intern, &deleted_count
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(deleted_count, 3);  /* A, B, and A::Child (cascaded) */
    ASSERT_EQ(result->element_count, 1);  /* Only Pkg remains */

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(delete_recursive) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Create model: Root, Pkg, Pkg::A, Pkg::A::Child */
    SysmlNode nodes[4] = {
        {.id = "Root", .name = "Root", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::A::Child", .name = "Child", .kind = SYSML_KIND_ATTRIBUTE_USAGE, .parent_id = "Pkg::A"},
    };
    SysmlSemanticModel *model = create_test_model(&arena, &intern, nodes, 4, NULL, 0);

    /* Delete Pkg::** (Pkg and all descendants) */
    Sysml2QueryPattern *pattern = sysml2_query_parse("Pkg::**", &arena);
    size_t deleted_count = 0;
    SysmlSemanticModel *result = sysml2_modify_clone_with_deletions(
        model, pattern, &arena, &intern, &deleted_count
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(deleted_count, 3);  /* Pkg, A, Child */
    ASSERT_EQ(result->element_count, 1);  /* Only Root remains */

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(delete_removes_relationships) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Create model with relationship */
    SysmlNode nodes[3] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::B", .name = "B", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
    };
    SysmlRelationship rels[1] = {
        {.id = "rel1", .kind = SYSML_KIND_REL_SPECIALIZATION, .source = "Pkg::A", .target = "Pkg::B"},
    };
    SysmlSemanticModel *model = create_test_model(&arena, &intern, nodes, 3, rels, 1);

    /* Delete Pkg::A (should also remove relationship) */
    Sysml2QueryPattern *pattern = sysml2_query_parse("Pkg::A", &arena);
    size_t deleted_count = 0;
    SysmlSemanticModel *result = sysml2_modify_clone_with_deletions(
        model, pattern, &arena, &intern, &deleted_count
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(result->relationship_count, 0);  /* Relationship removed */

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(delete_nonexistent_returns_copy) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Create model */
    SysmlNode nodes[2] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
    };
    SysmlSemanticModel *model = create_test_model(&arena, &intern, nodes, 2, NULL, 0);

    /* Delete nonexistent element */
    Sysml2QueryPattern *pattern = sysml2_query_parse("Pkg::NonExistent", &arena);
    size_t deleted_count = 0;
    SysmlSemanticModel *result = sysml2_modify_clone_with_deletions(
        model, pattern, &arena, &intern, &deleted_count
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(deleted_count, 0);  /* Nothing deleted */
    ASSERT_EQ(result->element_count, 2);  /* All elements preserved */

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* ========== Scope Tests ========== */

TEST(scope_exists_true) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlNode nodes[2] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
    };
    SysmlSemanticModel *model = create_test_model(&arena, &intern, nodes, 2, NULL, 0);

    ASSERT_TRUE(sysml2_modify_scope_exists(model, "Pkg"));
    ASSERT_TRUE(sysml2_modify_scope_exists(model, "Pkg::A"));

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(scope_exists_false) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    SysmlNode nodes[1] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
    };
    SysmlSemanticModel *model = create_test_model(&arena, &intern, nodes, 1, NULL, 0);

    ASSERT_FALSE(sysml2_modify_scope_exists(model, "Other"));
    ASSERT_FALSE(sysml2_modify_scope_exists(model, "Pkg::A"));

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(create_scope_chain) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Empty model */
    SysmlSemanticModel empty = {0};
    empty.source_name = "test.sysml";

    /* Create A::B::C scope chain */
    SysmlSemanticModel *result = sysml2_modify_create_scope_chain(&empty, "A::B::C", &arena, &intern);

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(result->element_count, 3);  /* A, A::B, A::B::C */

    /* Verify all scopes exist */
    ASSERT_TRUE(sysml2_modify_scope_exists(result, "A"));
    ASSERT_TRUE(sysml2_modify_scope_exists(result, "A::B"));
    ASSERT_TRUE(sysml2_modify_scope_exists(result, "A::B::C"));

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* ========== Merge Tests ========== */

TEST(merge_into_existing_scope) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base model: Pkg */
    SysmlNode base_nodes[1] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
    };
    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 1, NULL, 0);

    /* Fragment: NewDef */
    SysmlNode frag_nodes[1] = {
        {.id = "NewDef", .name = "NewDef", .kind = SYSML_KIND_PART_DEF, .parent_id = NULL},
    };
    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 1, NULL, 0);

    /* Merge into Pkg */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, fragment, "Pkg", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(added, 1);
    ASSERT_EQ(replaced, 0);
    ASSERT_EQ(result->element_count, 2);  /* Pkg and Pkg::NewDef */

    /* Verify Pkg::NewDef exists */
    ASSERT_TRUE(sysml2_modify_scope_exists(result, "Pkg::NewDef"));

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(merge_replaces_existing) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base model: Pkg, Pkg::A (PART_DEF) */
    SysmlNode base_nodes[2] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
    };
    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 2, NULL, 0);

    /* Fragment: A (ITEM_DEF - different kind) */
    SysmlNode frag_nodes[1] = {
        {.id = "A", .name = "A", .kind = SYSML_KIND_ITEM_DEF, .parent_id = NULL},
    };
    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 1, NULL, 0);

    /* Merge into Pkg (should replace Pkg::A) */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, fragment, "Pkg", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(added, 0);
    ASSERT_EQ(replaced, 1);
    ASSERT_EQ(result->element_count, 2);

    /* Verify Pkg::A is now ITEM_DEF */
    for (size_t i = 0; i < result->element_count; i++) {
        if (strcmp(result->elements[i]->id, "Pkg::A") == 0) {
            ASSERT_EQ(result->elements[i]->kind, SYSML_KIND_ITEM_DEF);
        }
    }

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(merge_with_create_scope) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Empty base model */
    SysmlSemanticModel base = {0};
    base.source_name = "test.sysml";

    /* Fragment: NewDef */
    SysmlNode frag_nodes[1] = {
        {.id = "NewDef", .name = "NewDef", .kind = SYSML_KIND_PART_DEF, .parent_id = NULL},
    };
    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 1, NULL, 0);

    /* Merge into non-existent A::B scope with create_scope=true */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        &base, fragment, "A::B", true, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);
    /* Should have: A, A::B, A::B::NewDef */
    ASSERT_TRUE(sysml2_modify_scope_exists(result, "A"));
    ASSERT_TRUE(sysml2_modify_scope_exists(result, "A::B"));
    ASSERT_TRUE(sysml2_modify_scope_exists(result, "A::B::NewDef"));

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(merge_without_create_scope_fails) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Empty base model */
    SysmlSemanticModel base = {0};
    base.source_name = "test.sysml";

    /* Fragment */
    SysmlNode frag_nodes[1] = {
        {.id = "NewDef", .name = "NewDef", .kind = SYSML_KIND_PART_DEF, .parent_id = NULL},
    };
    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 1, NULL, 0);

    /* Merge into non-existent scope with create_scope=false */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        &base, fragment, "NonExistent", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NULL(result);  /* Should fail */

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(merge_remaps_relationships) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base model: Pkg */
    SysmlNode base_nodes[1] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
    };
    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 1, NULL, 0);

    /* Fragment: A, B with relationship */
    SysmlNode frag_nodes[2] = {
        {.id = "A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = NULL},
        {.id = "B", .name = "B", .kind = SYSML_KIND_PART_DEF, .parent_id = NULL},
    };
    SysmlRelationship frag_rels[1] = {
        {.id = "rel1", .kind = SYSML_KIND_REL_SPECIALIZATION, .source = "A", .target = "B"},
    };
    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 2, frag_rels, 1);

    /* Merge into Pkg */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, fragment, "Pkg", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(result->relationship_count, 1);

    /* Verify relationship IDs are remapped */
    ASSERT_STR_EQ(result->relationships[0]->source, "Pkg::A");
    ASSERT_STR_EQ(result->relationships[0]->target, "Pkg::B");

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* ========== Additional Delete Tests ========== */

TEST(delete_preserves_unrelated_sibling) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Create model: Pkg, Pkg::A, Pkg::B, Pkg::C */
    SysmlNode nodes[4] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::B", .name = "B", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::C", .name = "C", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
    };
    SysmlSemanticModel *model = create_test_model(&arena, &intern, nodes, 4, NULL, 0);

    /* Delete Pkg::B */
    Sysml2QueryPattern *pattern = sysml2_query_parse("Pkg::B", &arena);
    size_t deleted_count = 0;
    SysmlSemanticModel *result = sysml2_modify_clone_with_deletions(
        model, pattern, &arena, &intern, &deleted_count
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(deleted_count, 1);
    ASSERT_EQ(result->element_count, 3);  /* Pkg, A, C remain */

    /* Verify siblings preserved */
    bool found_a = false, found_b = false, found_c = false;
    for (size_t i = 0; i < result->element_count; i++) {
        if (strcmp(result->elements[i]->id, "Pkg::A") == 0) found_a = true;
        if (strcmp(result->elements[i]->id, "Pkg::B") == 0) found_b = true;
        if (strcmp(result->elements[i]->id, "Pkg::C") == 0) found_c = true;
    }
    ASSERT_TRUE(found_a);
    ASSERT_FALSE(found_b);
    ASSERT_TRUE(found_c);

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(delete_handles_root_level_elements) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Create model with two root packages */
    SysmlNode nodes[2] = {
        {.id = "PkgA", .name = "PkgA", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "PkgB", .name = "PkgB", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
    };
    SysmlSemanticModel *model = create_test_model(&arena, &intern, nodes, 2, NULL, 0);

    /* Delete PkgA (root level) */
    Sysml2QueryPattern *pattern = sysml2_query_parse("PkgA", &arena);
    size_t deleted_count = 0;
    SysmlSemanticModel *result = sysml2_modify_clone_with_deletions(
        model, pattern, &arena, &intern, &deleted_count
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(deleted_count, 1);
    ASSERT_EQ(result->element_count, 1);  /* Only PkgB remains */
    ASSERT_STR_EQ(result->elements[0]->id, "PkgB");

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(delete_handles_empty_model) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Empty model */
    SysmlSemanticModel model = {0};
    model.source_name = "test.sysml";

    /* Try to delete something */
    Sysml2QueryPattern *pattern = sysml2_query_parse("NonExistent", &arena);
    size_t deleted_count = 0;
    SysmlSemanticModel *result = sysml2_modify_clone_with_deletions(
        &model, pattern, &arena, &intern, &deleted_count
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(deleted_count, 0);
    ASSERT_EQ(result->element_count, 0);

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(delete_relationship_when_source_deleted) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Model: A :> B */
    SysmlNode nodes[3] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::B", .name = "B", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
    };
    SysmlRelationship rels[1] = {
        {.id = "rel1", .kind = SYSML_KIND_REL_SPECIALIZATION, .source = "Pkg::A", .target = "Pkg::B"},
    };
    SysmlSemanticModel *model = create_test_model(&arena, &intern, nodes, 3, rels, 1);

    /* Delete A (source of relationship) */
    Sysml2QueryPattern *pattern = sysml2_query_parse("Pkg::A", &arena);
    size_t deleted_count = 0;
    SysmlSemanticModel *result = sysml2_modify_clone_with_deletions(
        model, pattern, &arena, &intern, &deleted_count
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(result->relationship_count, 0);  /* Relationship removed */
    ASSERT_EQ(result->element_count, 2);  /* Pkg and B remain */

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(delete_relationship_when_target_deleted) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Model: A :> B */
    SysmlNode nodes[3] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::B", .name = "B", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
    };
    SysmlRelationship rels[1] = {
        {.id = "rel1", .kind = SYSML_KIND_REL_SPECIALIZATION, .source = "Pkg::A", .target = "Pkg::B"},
    };
    SysmlSemanticModel *model = create_test_model(&arena, &intern, nodes, 3, rels, 1);

    /* Delete B (target of relationship) */
    Sysml2QueryPattern *pattern = sysml2_query_parse("Pkg::B", &arena);
    size_t deleted_count = 0;
    SysmlSemanticModel *result = sysml2_modify_clone_with_deletions(
        model, pattern, &arena, &intern, &deleted_count
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(result->relationship_count, 0);  /* Relationship removed */
    ASSERT_EQ(result->element_count, 2);  /* Pkg and A remain */

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(delete_import_when_owner_deleted) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Model: Pkg imports Target */
    SysmlNode nodes[2] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "Target", .name = "Target", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
    };
    SysmlRelationship rels[1] = {
        {.id = "imp1", .kind = SYSML_KIND_IMPORT, .source = "Pkg", .target = "Target"},
    };
    SysmlSemanticModel *model = create_test_model(&arena, &intern, nodes, 2, rels, 1);

    /* Delete Pkg (owner of import) */
    Sysml2QueryPattern *pattern = sysml2_query_parse("Pkg", &arena);
    size_t deleted_count = 0;
    SysmlSemanticModel *result = sysml2_modify_clone_with_deletions(
        model, pattern, &arena, &intern, &deleted_count
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(result->relationship_count, 0);  /* Import removed with owner */
    ASSERT_EQ(result->element_count, 1);  /* Only Target remains */

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(delete_multiple_patterns_no_double_count) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Create model: Pkg, Pkg::A, Pkg::B */
    SysmlNode nodes[3] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "Pkg::A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
        {.id = "Pkg::B", .name = "B", .kind = SYSML_KIND_PART_DEF, .parent_id = "Pkg"},
    };
    SysmlSemanticModel *model = create_test_model(&arena, &intern, nodes, 3, NULL, 0);

    /* Parse multiple patterns that overlap */
    Sysml2QueryPattern *p1 = sysml2_query_parse("Pkg::A", &arena);
    Sysml2QueryPattern *p2 = sysml2_query_parse("Pkg::*", &arena);
    p1->next = p2;  /* Chain patterns */

    size_t deleted_count = 0;
    SysmlSemanticModel *result = sysml2_modify_clone_with_deletions(
        model, p1, &arena, &intern, &deleted_count
    );

    ASSERT_NOT_NULL(result);
    /* A matches both patterns but should only be counted once */
    ASSERT_EQ(deleted_count, 2);  /* A and B, not 3 */
    ASSERT_EQ(result->element_count, 1);  /* Only Pkg remains */

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* ========== Additional Merge Tests ========== */

TEST(merge_empty_fragment) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base model: Pkg */
    SysmlNode base_nodes[1] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
    };
    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 1, NULL, 0);

    /* Empty fragment */
    SysmlSemanticModel fragment = {0};
    fragment.source_name = "fragment.sysml";

    /* Merge empty fragment */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, &fragment, "Pkg", false, &arena, &intern, &added, &replaced
    );

    /* Empty fragment merge should succeed but add nothing */
    if (result != NULL) {
        ASSERT_EQ(added, 0);
        ASSERT_EQ(replaced, 0);
        ASSERT_EQ(result->element_count, 1);  /* Just Pkg */
    }
    /* Or it may return NULL for empty fragment - both are acceptable */

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(merge_remap_deep_nesting) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base: A::B */
    SysmlNode base_nodes[2] = {
        {.id = "A", .name = "A", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "A::B", .name = "B", .kind = SYSML_KIND_PACKAGE, .parent_id = "A"},
    };
    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 2, NULL, 0);

    /* Fragment: X::Y::Z (nested) */
    SysmlNode frag_nodes[3] = {
        {.id = "X", .name = "X", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "X::Y", .name = "Y", .kind = SYSML_KIND_PACKAGE, .parent_id = "X"},
        {.id = "X::Y::Z", .name = "Z", .kind = SYSML_KIND_PART_DEF, .parent_id = "X::Y"},
    };
    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 3, NULL, 0);

    /* Merge into A::B */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, fragment, "A::B", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(added, 3);

    /* Verify deep nesting remapped correctly */
    ASSERT_TRUE(sysml2_modify_scope_exists(result, "A::B::X"));
    ASSERT_TRUE(sysml2_modify_scope_exists(result, "A::B::X::Y"));
    ASSERT_TRUE(sysml2_modify_scope_exists(result, "A::B::X::Y::Z"));

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(merge_preserves_element_properties) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base: Pkg */
    SysmlNode base_nodes[1] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
    };
    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 1, NULL, 0);

    /* Fragment: Element with documentation */
    SysmlNode frag_nodes[1] = {
        {.id = "Elem", .name = "Elem", .kind = SYSML_KIND_PART_DEF, .parent_id = NULL,
         .documentation = "Test documentation"},
    };
    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 1, NULL, 0);

    /* Merge */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, fragment, "Pkg", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);

    /* Find merged element and check documentation preserved */
    for (size_t i = 0; i < result->element_count; i++) {
        if (strcmp(result->elements[i]->name, "Elem") == 0) {
            ASSERT_NOT_NULL(result->elements[i]->documentation);
            ASSERT_STR_EQ(result->elements[i]->documentation, "Test documentation");
        }
    }

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(merge_relationship_remap_both_endpoints) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base: Pkg */
    SysmlNode base_nodes[1] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
    };
    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 1, NULL, 0);

    /* Fragment: A :> B (both in fragment) */
    SysmlNode frag_nodes[2] = {
        {.id = "A", .name = "A", .kind = SYSML_KIND_PART_DEF, .parent_id = NULL},
        {.id = "B", .name = "B", .kind = SYSML_KIND_PART_DEF, .parent_id = NULL},
    };
    SysmlRelationship frag_rels[1] = {
        {.id = "rel1", .kind = SYSML_KIND_REL_SPECIALIZATION, .source = "A", .target = "B"},
    };
    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 2, frag_rels, 1);

    /* Merge into Pkg */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, fragment, "Pkg", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(result->relationship_count, 1);

    /* Both endpoints should be remapped */
    ASSERT_STR_EQ(result->relationships[0]->source, "Pkg::A");
    ASSERT_STR_EQ(result->relationships[0]->target, "Pkg::B");

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(merge_import_source_remapped) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base: Pkg, External */
    SysmlNode base_nodes[2] = {
        {.id = "Pkg", .name = "Pkg", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
        {.id = "External", .name = "External", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
    };
    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 2, NULL, 0);

    /* Fragment: Inner that imports External */
    SysmlNode frag_nodes[1] = {
        {.id = "Inner", .name = "Inner", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
    };
    SysmlRelationship frag_rels[1] = {
        {.id = "imp1", .kind = SYSML_KIND_IMPORT, .source = "Inner", .target = "External"},
    };
    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 1, frag_rels, 1);

    /* Merge into Pkg */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, fragment, "Pkg", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(result->relationship_count, 1);

    /* Source is remapped to Pkg::Inner */
    ASSERT_STR_EQ(result->relationships[0]->source, "Pkg::Inner");
    /* Target is also remapped since it's being merged into the scope */
    /* (All fragment IDs get remapped to the target scope) */
    ASSERT_NOT_NULL(result->relationships[0]->target);

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* ========== Find Containing File Tests ========== */

TEST(find_containing_file) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Model 1: A */
    SysmlNode nodes1[1] = {
        {.id = "A", .name = "A", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
    };
    SysmlSemanticModel *model1 = create_test_model(&arena, &intern, nodes1, 1, NULL, 0);

    /* Model 2: B */
    SysmlNode nodes2[1] = {
        {.id = "B", .name = "B", .kind = SYSML_KIND_PACKAGE, .parent_id = NULL},
    };
    SysmlSemanticModel *model2 = create_test_model(&arena, &intern, nodes2, 1, NULL, 0);

    SysmlSemanticModel *models[2] = {model1, model2};

    ASSERT_EQ(sysml2_modify_find_containing_file("A", models, 2), 0);
    ASSERT_EQ(sysml2_modify_find_containing_file("B", models, 2), 1);
    ASSERT_EQ(sysml2_modify_find_containing_file("C", models, 2), -1);

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* ========== Shorthand Feature Regression Tests ========== */

/* Helper: Parse SysML from string and return model */
static SysmlSemanticModel *parse_sysml_string(
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    const char *input
) {
    Sysml2CliOptions options = {0};
    options.parse_only = true;
    options.no_resolve = true;

    Sysml2PipelineContext *ctx = sysml2_pipeline_create(arena, intern, &options);
    if (!ctx) return NULL;

    SysmlSemanticModel *model = NULL;
    Sysml2Result result = sysml2_pipeline_process_input(
        ctx, "<test>", input, strlen(input), &model
    );

    sysml2_pipeline_destroy(ctx);

    return (result == SYSML2_OK) ? model : NULL;
}

/* Test: Shorthand feature value doesn't leak to sibling elements */
TEST(shorthand_value_no_leak_to_sibling) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    const char *input =
        "package TestPkg {\n"
        "    part parent : ParentType {\n"
        "        :>> name = \"Parent Name\";\n"
        "        part child : ChildType { }\n"
        "    }\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    /* Write model back to string */
    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* The child should NOT have the parent's shorthand value leaked to it */
    /* Look for 'child : ChildType = "Parent Name"' which would be the bug */
    ASSERT(strstr(output, "child : ChildType = \"Parent Name\"") == NULL);

    /* The child should appear without a default value */
    ASSERT(strstr(output, "part child : ChildType") != NULL);

    /* The shorthand feature should still be present in the body */
    ASSERT(strstr(output, ":>> name = \"Parent Name\";") != NULL);

    free(output);
    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* Test: Shorthand feature preserved in body, not moved to parent's default value */
TEST(shorthand_feature_preserved_in_body) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    const char *input =
        "package TestPkg {\n"
        "    part http_client : ExternalDependency {\n"
        "        :>> version = \"^8.11.0\";\n"
        "    }\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    /* Write model back to string */
    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* The http_client should NOT have a default value at the declaration level */
    /* Bug would be: 'http_client : ExternalDependency = "^8.11.0" {' */
    ASSERT(strstr(output, "http_client : ExternalDependency = \"^8.11.0\"") == NULL);

    /* The shorthand feature should be preserved inside the body */
    ASSERT(strstr(output, ":>> version = \"^8.11.0\";") != NULL);

    free(output);
    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* ========== Import Order and Comment Preservation Tests ========== */

/* Test: Import order is preserved during write (not alphabetically sorted) */
TEST(import_order_preserved_during_write) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Input with specific import order: ZModule before AModule */
    const char *input =
        "package TestPkg {\n"
        "    import ZModule::*;\n"
        "    import AModule::*;\n"
        "    import MModule::*;\n"
        "    part def MyPart { }\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    /* Write model back to string */
    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify imports are NOT alphabetically sorted */
    /* ZModule should appear BEFORE AModule (original order preserved) */
    char *z_pos = strstr(output, "import ZModule");
    char *a_pos = strstr(output, "import AModule");
    char *m_pos = strstr(output, "import MModule");

    ASSERT_NOT_NULL(z_pos);
    ASSERT_NOT_NULL(a_pos);
    ASSERT_NOT_NULL(m_pos);

    /* ZModule should come before AModule, and AModule before MModule */
    ASSERT(z_pos < a_pos);
    ASSERT(a_pos < m_pos);

    free(output);
    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* Test: Line comments are not duplicated during upsert */
TEST(no_comment_duplication_during_upsert) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Input with a line comment */
    const char *input =
        "package TestPkg {\n"
        "    import SomeModule::*;\n"
        "\n"
        "    // Important comment about the element below\n"
        "    part def MyElement { }\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    /* Write model back to string */
    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Count occurrences of the comment - should be exactly 1 */
    int comment_count = 0;
    const char *search = "// Important comment";
    char *pos = output;
    while ((pos = strstr(pos, search)) != NULL) {
        comment_count++;
        pos += strlen(search);
    }

    ASSERT_EQ(comment_count, 1);

    free(output);
    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* ========== Metadata Accumulation Regression Tests ========== */

/* Test: Metadata on target scope is cleared to prevent accumulation */
TEST(merge_no_metadata_accumulation) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base model: Pkg with prefix_applied_metadata, Pkg::A */
    SysmlNode base_nodes[2];
    memset(base_nodes, 0, sizeof(base_nodes));
    base_nodes[0].id = "Pkg";
    base_nodes[0].name = "Pkg";
    base_nodes[0].kind = SYSML_KIND_PACKAGE;

    /* Simulate accumulated @SourceFile on package from prior upsert */
    SysmlMetadataUsage *old_meta = sysml2_arena_alloc(&arena, sizeof(SysmlMetadataUsage));
    memset(old_meta, 0, sizeof(SysmlMetadataUsage));
    old_meta->type_ref = "SourceFile";
    base_nodes[0].prefix_applied_metadata = sysml2_arena_alloc(&arena, sizeof(SysmlMetadataUsage *));
    base_nodes[0].prefix_applied_metadata[0] = old_meta;
    base_nodes[0].prefix_applied_metadata_count = 1;

    base_nodes[1].id = "Pkg::A";
    base_nodes[1].name = "A";
    base_nodes[1].kind = SYSML_KIND_PART_DEF;
    base_nodes[1].parent_id = "Pkg";

    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 2, NULL, 0);

    /* Fragment: A with its own @SourceFile */
    SysmlNode frag_nodes[1];
    memset(frag_nodes, 0, sizeof(frag_nodes));
    frag_nodes[0].id = "A";
    frag_nodes[0].name = "A";
    frag_nodes[0].kind = SYSML_KIND_PART_DEF;

    SysmlMetadataUsage *new_meta = sysml2_arena_alloc(&arena, sizeof(SysmlMetadataUsage));
    memset(new_meta, 0, sizeof(SysmlMetadataUsage));
    new_meta->type_ref = "SourceFile";
    frag_nodes[0].prefix_applied_metadata = sysml2_arena_alloc(&arena, sizeof(SysmlMetadataUsage *));
    frag_nodes[0].prefix_applied_metadata[0] = new_meta;
    frag_nodes[0].prefix_applied_metadata_count = 1;

    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 1, NULL, 0);

    /* Merge into Pkg */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, fragment, "Pkg", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);

    /* Verify: Package should have NO prefix_applied_metadata (cleared) */
    for (size_t i = 0; i < result->element_count; i++) {
        if (strcmp(result->elements[i]->id, "Pkg") == 0) {
            ASSERT_EQ(result->elements[i]->prefix_applied_metadata_count, 0);
        }
    }

    /* Verify: Pkg::A should have exactly 1 @SourceFile (from fragment) */
    for (size_t i = 0; i < result->element_count; i++) {
        if (strcmp(result->elements[i]->id, "Pkg::A") == 0) {
            ASSERT_EQ(result->elements[i]->prefix_applied_metadata_count, 1);
        }
    }

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* Test: Non-target scopes preserve their metadata during merge */
TEST(merge_preserves_sibling_metadata) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base model: Pkg, Pkg::A with metadata, Pkg::B (sibling) with metadata */
    SysmlNode base_nodes[3];
    memset(base_nodes, 0, sizeof(base_nodes));
    base_nodes[0].id = "Pkg";
    base_nodes[0].name = "Pkg";
    base_nodes[0].kind = SYSML_KIND_PACKAGE;

    base_nodes[1].id = "Pkg::A";
    base_nodes[1].name = "A";
    base_nodes[1].kind = SYSML_KIND_PART_DEF;
    base_nodes[1].parent_id = "Pkg";

    /* B has metadata that should be preserved (not being replaced) */
    base_nodes[2].id = "Pkg::B";
    base_nodes[2].name = "B";
    base_nodes[2].kind = SYSML_KIND_PART_DEF;
    base_nodes[2].parent_id = "Pkg";

    SysmlMetadataUsage *b_meta = sysml2_arena_alloc(&arena, sizeof(SysmlMetadataUsage));
    memset(b_meta, 0, sizeof(SysmlMetadataUsage));
    b_meta->type_ref = "PreservedMeta";
    base_nodes[2].prefix_applied_metadata = sysml2_arena_alloc(&arena, sizeof(SysmlMetadataUsage *));
    base_nodes[2].prefix_applied_metadata[0] = b_meta;
    base_nodes[2].prefix_applied_metadata_count = 1;

    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 3, NULL, 0);

    /* Fragment: A replacement (only A, not B) */
    SysmlNode frag_nodes[1];
    memset(frag_nodes, 0, sizeof(frag_nodes));
    frag_nodes[0].id = "A";
    frag_nodes[0].name = "A";
    frag_nodes[0].kind = SYSML_KIND_PART_DEF;

    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 1, NULL, 0);

    /* Merge into Pkg */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, fragment, "Pkg", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);

    /* Verify: Pkg::B still has its metadata (sibling not touched) */
    for (size_t i = 0; i < result->element_count; i++) {
        if (strcmp(result->elements[i]->id, "Pkg::B") == 0) {
            ASSERT_EQ(result->elements[i]->prefix_applied_metadata_count, 1);
            ASSERT_STR_EQ(result->elements[i]->prefix_applied_metadata[0]->type_ref, "PreservedMeta");
        }
    }

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* Test: Body metadata on target scope is also cleared */
TEST(merge_clears_body_metadata) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base model: Pkg with body metadata */
    SysmlNode base_nodes[1];
    memset(base_nodes, 0, sizeof(base_nodes));
    base_nodes[0].id = "Pkg";
    base_nodes[0].name = "Pkg";
    base_nodes[0].kind = SYSML_KIND_PACKAGE;

    /* Add body metadata to package */
    SysmlMetadataUsage *body_meta = sysml2_arena_alloc(&arena, sizeof(SysmlMetadataUsage));
    memset(body_meta, 0, sizeof(SysmlMetadataUsage));
    body_meta->type_ref = "OldBodyMeta";
    base_nodes[0].metadata = sysml2_arena_alloc(&arena, sizeof(SysmlMetadataUsage *));
    base_nodes[0].metadata[0] = body_meta;
    base_nodes[0].metadata_count = 1;

    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 1, NULL, 0);

    /* Fragment: new element */
    SysmlNode frag_nodes[1];
    memset(frag_nodes, 0, sizeof(frag_nodes));
    frag_nodes[0].id = "NewElem";
    frag_nodes[0].name = "NewElem";
    frag_nodes[0].kind = SYSML_KIND_PART_DEF;

    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 1, NULL, 0);

    /* Merge into Pkg */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, fragment, "Pkg", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);

    /* Verify: Package body metadata is cleared */
    for (size_t i = 0; i < result->element_count; i++) {
        if (strcmp(result->elements[i]->id, "Pkg") == 0) {
            ASSERT_EQ(result->elements[i]->metadata_count, 0);
        }
    }

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* Test: Trailing trivia on target scope is cleared */
TEST(merge_clears_trailing_trivia) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base model: Pkg with trailing trivia */
    SysmlNode base_nodes[1];
    memset(base_nodes, 0, sizeof(base_nodes));
    base_nodes[0].id = "Pkg";
    base_nodes[0].name = "Pkg";
    base_nodes[0].kind = SYSML_KIND_PACKAGE;

    /* Add trailing trivia to package */
    SysmlTrivia *trivia = sysml2_arena_alloc(&arena, sizeof(SysmlTrivia));
    memset(trivia, 0, sizeof(SysmlTrivia));
    trivia->kind = SYSML_TRIVIA_LINE_COMMENT;
    trivia->text = "// accumulated trailing comment";
    trivia->next = NULL;
    base_nodes[0].trailing_trivia = trivia;

    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 1, NULL, 0);

    /* Fragment: new element */
    SysmlNode frag_nodes[1];
    memset(frag_nodes, 0, sizeof(frag_nodes));
    frag_nodes[0].id = "NewElem";
    frag_nodes[0].name = "NewElem";
    frag_nodes[0].kind = SYSML_KIND_PART_DEF;

    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 1, NULL, 0);

    /* Merge into Pkg */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, fragment, "Pkg", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);

    /* Verify: Package trailing trivia is cleared */
    for (size_t i = 0; i < result->element_count; i++) {
        if (strcmp(result->elements[i]->id, "Pkg") == 0) {
            ASSERT_NULL(result->elements[i]->trailing_trivia);
        }
    }

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* Test: Leading trivia on target scope is cleared */
TEST(merge_clears_leading_trivia) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base model: Pkg with leading trivia */
    SysmlNode base_nodes[1];
    memset(base_nodes, 0, sizeof(base_nodes));
    base_nodes[0].id = "Pkg";
    base_nodes[0].name = "Pkg";
    base_nodes[0].kind = SYSML_KIND_PACKAGE;

    /* Add leading trivia to package */
    SysmlTrivia *trivia = sysml2_arena_alloc(&arena, sizeof(SysmlTrivia));
    memset(trivia, 0, sizeof(SysmlTrivia));
    trivia->kind = SYSML_TRIVIA_BLOCK_COMMENT;
    trivia->text = "/* accumulated comment */";
    trivia->next = NULL;
    base_nodes[0].leading_trivia = trivia;

    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 1, NULL, 0);

    /* Fragment: new element */
    SysmlNode frag_nodes[1];
    memset(frag_nodes, 0, sizeof(frag_nodes));
    frag_nodes[0].id = "NewElem";
    frag_nodes[0].name = "NewElem";
    frag_nodes[0].kind = SYSML_KIND_PART_DEF;

    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 1, NULL, 0);

    /* Merge into Pkg */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, fragment, "Pkg", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);

    /* Verify: Package leading trivia is cleared */
    for (size_t i = 0; i < result->element_count; i++) {
        if (strcmp(result->elements[i]->id, "Pkg") == 0) {
            ASSERT_NULL(result->elements[i]->leading_trivia);
        }
    }

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* Test: Multiple upserts to same file - simulated accumulation scenario */
TEST(merge_repeated_upserts_no_accumulation) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Start with empty Pkg */
    SysmlNode base_nodes[1];
    memset(base_nodes, 0, sizeof(base_nodes));
    base_nodes[0].id = "Pkg";
    base_nodes[0].name = "Pkg";
    base_nodes[0].kind = SYSML_KIND_PACKAGE;

    SysmlSemanticModel *model = create_test_model(&arena, &intern, base_nodes, 1, NULL, 0);

    /* Simulate 3 upserts with metadata */
    for (int round = 0; round < 3; round++) {
        SysmlNode frag_nodes[1];
        memset(frag_nodes, 0, sizeof(frag_nodes));
        frag_nodes[0].id = "Elem";
        frag_nodes[0].name = "Elem";
        frag_nodes[0].kind = SYSML_KIND_PART_DEF;

        /* Each upsert brings @SourceFile */
        SysmlMetadataUsage *meta = sysml2_arena_alloc(&arena, sizeof(SysmlMetadataUsage));
        memset(meta, 0, sizeof(SysmlMetadataUsage));
        meta->type_ref = "SourceFile";
        frag_nodes[0].prefix_applied_metadata = sysml2_arena_alloc(&arena, sizeof(SysmlMetadataUsage *));
        frag_nodes[0].prefix_applied_metadata[0] = meta;
        frag_nodes[0].prefix_applied_metadata_count = 1;

        SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 1, NULL, 0);

        size_t added = 0, replaced = 0;
        model = sysml2_modify_merge_fragment(
            model, fragment, "Pkg", false, &arena, &intern, &added, &replaced
        );
        ASSERT_NOT_NULL(model);
    }

    /* After 3 rounds, Pkg should have 0 metadata (cleared each time) */
    for (size_t i = 0; i < model->element_count; i++) {
        if (strcmp(model->elements[i]->id, "Pkg") == 0) {
            ASSERT_EQ(model->elements[i]->prefix_applied_metadata_count, 0);
        }
    }

    /* And Pkg::Elem should have exactly 1 @SourceFile (not 3) */
    for (size_t i = 0; i < model->element_count; i++) {
        if (strcmp(model->elements[i]->id, "Pkg::Elem") == 0) {
            ASSERT_EQ(model->elements[i]->prefix_applied_metadata_count, 1);
        }
    }

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* Test: Comments are stable across multiple upserts (no duplication, no movement) */
TEST(upsert_comment_stability) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Input with comments in various positions */
    const char *input =
        "package TestPkg {\n"
        "    // Comment before first\n"
        "    part def First { }\n"
        "\n"
        "    // Comment before second\n"
        "    part def Second { }\n"
        "}\n";

    /* Fragment to upsert */
    const char *fragment_src =
        "part def NewPart { }\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    /* Parse fragment */
    SysmlSemanticModel *fragment = parse_sysml_string(&arena, &intern, fragment_src);
    ASSERT_NOT_NULL(fragment);

    /* Run 3 upserts */
    for (int round = 0; round < 3; round++) {
        size_t added = 0, replaced = 0;
        model = sysml2_modify_merge_fragment(
            model, fragment, "TestPkg", false, &arena, &intern, &added, &replaced
        );
        ASSERT_NOT_NULL(model);
    }

    /* Write result to string */
    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Count occurrences of each comment - should be exactly 1 each */
    int count_first = 0, count_second = 0;
    char *pos = output;
    while ((pos = strstr(pos, "// Comment before first")) != NULL) {
        count_first++;
        pos++;
    }
    pos = output;
    while ((pos = strstr(pos, "// Comment before second")) != NULL) {
        count_second++;
        pos++;
    }

    ASSERT_EQ(count_first, 1);
    ASSERT_EQ(count_second, 1);

    free(output);
    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* Test: Children of replaced parent are preserved if not in fragment */
TEST(merge_preserves_children_of_replaced_parent) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base model: Pkg, Pkg::Parent, Pkg::Parent::Child1, Pkg::Parent::Child2 */
    SysmlNode base_nodes[4];
    memset(base_nodes, 0, sizeof(base_nodes));
    base_nodes[0].id = "Pkg";
    base_nodes[0].name = "Pkg";
    base_nodes[0].kind = SYSML_KIND_PACKAGE;

    base_nodes[1].id = "Pkg::Parent";
    base_nodes[1].name = "Parent";
    base_nodes[1].kind = SYSML_KIND_PART_USAGE;
    base_nodes[1].parent_id = "Pkg";

    base_nodes[2].id = "Pkg::Parent::Child1";
    base_nodes[2].name = "Child1";
    base_nodes[2].kind = SYSML_KIND_PART_USAGE;
    base_nodes[2].parent_id = "Pkg::Parent";

    base_nodes[3].id = "Pkg::Parent::Child2";
    base_nodes[3].name = "Child2";
    base_nodes[3].kind = SYSML_KIND_PART_USAGE;
    base_nodes[3].parent_id = "Pkg::Parent";

    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 4, NULL, 0);

    /* Fragment: Parent with new attribute (no children) */
    SysmlNode frag_nodes[2];
    memset(frag_nodes, 0, sizeof(frag_nodes));
    frag_nodes[0].id = "Parent";
    frag_nodes[0].name = "Parent";
    frag_nodes[0].kind = SYSML_KIND_PART_USAGE;

    frag_nodes[1].id = "Parent::NewAttr";
    frag_nodes[1].name = "NewAttr";
    frag_nodes[1].kind = SYSML_KIND_ATTRIBUTE_USAGE;
    frag_nodes[1].parent_id = "Parent";

    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 2, NULL, 0);

    /* Merge into Pkg */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, fragment, "Pkg", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);
    ASSERT_EQ(replaced, 1);  /* Parent was replaced */
    ASSERT_EQ(added, 1);     /* NewAttr was added */

    /* Verify: Child1 and Child2 are preserved */
    bool found_child1 = false, found_child2 = false, found_new_attr = false;
    for (size_t i = 0; i < result->element_count; i++) {
        if (strcmp(result->elements[i]->id, "Pkg::Parent::Child1") == 0) found_child1 = true;
        if (strcmp(result->elements[i]->id, "Pkg::Parent::Child2") == 0) found_child2 = true;
        if (strcmp(result->elements[i]->id, "Pkg::Parent::NewAttr") == 0) found_new_attr = true;
    }
    ASSERT_TRUE(found_child1);
    ASSERT_TRUE(found_child2);
    ASSERT_TRUE(found_new_attr);

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* Test: Child with same name as fragment child is replaced */
TEST(merge_replaces_matching_children) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Base model: Pkg, Pkg::Parent, Pkg::Parent::Attr (ATTRIBUTE) */
    SysmlNode base_nodes[3];
    memset(base_nodes, 0, sizeof(base_nodes));
    base_nodes[0].id = "Pkg";
    base_nodes[0].name = "Pkg";
    base_nodes[0].kind = SYSML_KIND_PACKAGE;

    base_nodes[1].id = "Pkg::Parent";
    base_nodes[1].name = "Parent";
    base_nodes[1].kind = SYSML_KIND_PART_USAGE;
    base_nodes[1].parent_id = "Pkg";

    base_nodes[2].id = "Pkg::Parent::Attr";
    base_nodes[2].name = "Attr";
    base_nodes[2].kind = SYSML_KIND_ATTRIBUTE_USAGE;
    base_nodes[2].parent_id = "Pkg::Parent";
    base_nodes[2].documentation = "Old doc";

    SysmlSemanticModel *base = create_test_model(&arena, &intern, base_nodes, 3, NULL, 0);

    /* Fragment: Parent with Attr (same name, different content) */
    SysmlNode frag_nodes[2];
    memset(frag_nodes, 0, sizeof(frag_nodes));
    frag_nodes[0].id = "Parent";
    frag_nodes[0].name = "Parent";
    frag_nodes[0].kind = SYSML_KIND_PART_USAGE;

    frag_nodes[1].id = "Parent::Attr";
    frag_nodes[1].name = "Attr";
    frag_nodes[1].kind = SYSML_KIND_ATTRIBUTE_USAGE;
    frag_nodes[1].parent_id = "Parent";
    frag_nodes[1].documentation = "New doc";

    SysmlSemanticModel *fragment = create_test_model(&arena, &intern, frag_nodes, 2, NULL, 0);

    /* Merge into Pkg */
    size_t added = 0, replaced = 0;
    SysmlSemanticModel *result = sysml2_modify_merge_fragment(
        base, fragment, "Pkg", false, &arena, &intern, &added, &replaced
    );

    ASSERT_NOT_NULL(result);

    /* Verify: Attr is replaced (has new doc) */
    for (size_t i = 0; i < result->element_count; i++) {
        if (strcmp(result->elements[i]->id, "Pkg::Parent::Attr") == 0) {
            ASSERT_STR_EQ(result->elements[i]->documentation, "New doc");
        }
    }

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* ========== Main ========== */

int main(void) {
    printf("Running modification tests...\n");

    /* Plan tests */
    RUN_TEST(plan_create);
    RUN_TEST(plan_add_delete_exact);
    RUN_TEST(plan_add_delete_recursive);
    RUN_TEST(plan_add_multiple_deletes);

    /* ID helper tests */
    RUN_TEST(id_starts_with_true);
    RUN_TEST(id_starts_with_false);
    RUN_TEST(get_local_name);
    RUN_TEST(remap_id_toplevel);
    RUN_TEST(remap_id_nested);

    /* Delete tests */
    RUN_TEST(delete_exact_element);
    RUN_TEST(delete_cascades_to_children);
    RUN_TEST(delete_direct_children_only);
    RUN_TEST(delete_recursive);
    RUN_TEST(delete_removes_relationships);
    RUN_TEST(delete_nonexistent_returns_copy);

    /* Additional delete tests */
    RUN_TEST(delete_preserves_unrelated_sibling);
    RUN_TEST(delete_handles_root_level_elements);
    RUN_TEST(delete_handles_empty_model);
    RUN_TEST(delete_relationship_when_source_deleted);
    RUN_TEST(delete_relationship_when_target_deleted);
    RUN_TEST(delete_import_when_owner_deleted);
    RUN_TEST(delete_multiple_patterns_no_double_count);

    /* Scope tests */
    RUN_TEST(scope_exists_true);
    RUN_TEST(scope_exists_false);
    RUN_TEST(create_scope_chain);

    /* Merge tests */
    RUN_TEST(merge_into_existing_scope);
    RUN_TEST(merge_replaces_existing);
    RUN_TEST(merge_with_create_scope);
    RUN_TEST(merge_without_create_scope_fails);
    RUN_TEST(merge_remaps_relationships);

    /* Additional merge tests */
    RUN_TEST(merge_empty_fragment);
    RUN_TEST(merge_remap_deep_nesting);
    RUN_TEST(merge_preserves_element_properties);
    RUN_TEST(merge_relationship_remap_both_endpoints);
    RUN_TEST(merge_import_source_remapped);

    /* Find containing file tests */
    RUN_TEST(find_containing_file);

    /* Shorthand feature regression tests */
    RUN_TEST(shorthand_value_no_leak_to_sibling);
    RUN_TEST(shorthand_feature_preserved_in_body);

    /* Import order and comment preservation tests */
    RUN_TEST(import_order_preserved_during_write);
    RUN_TEST(no_comment_duplication_during_upsert);
    RUN_TEST(upsert_comment_stability);

    /* Metadata accumulation regression tests */
    RUN_TEST(merge_no_metadata_accumulation);
    RUN_TEST(merge_preserves_sibling_metadata);
    RUN_TEST(merge_clears_body_metadata);
    RUN_TEST(merge_clears_trailing_trivia);
    RUN_TEST(merge_clears_leading_trivia);
    RUN_TEST(merge_repeated_upserts_no_accumulation);

    /* Child preservation tests */
    RUN_TEST(merge_preserves_children_of_replaced_parent);
    RUN_TEST(merge_replaces_matching_children);

    printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
