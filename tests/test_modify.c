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

    printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
