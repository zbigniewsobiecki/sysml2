/*
 * SysML v2 Parser - Import Resolver Tests
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/common.h"
#include "sysml2/arena.h"
#include "sysml2/intern.h"
#include "sysml2/ast.h"
#include "sysml2/import_resolver.h"
#include "sysml2/diagnostic.h"

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

/* Test fixture macros for arena/intern setup and teardown */
#define FIXTURE_SETUP() \
    Sysml2Arena arena; \
    sysml2_arena_init(&arena); \
    Sysml2Intern intern; \
    sysml2_intern_init(&intern, &arena)

#define FIXTURE_TEARDOWN() \
    sysml2_intern_destroy(&intern); \
    sysml2_arena_destroy(&arena)

/* ========== Resolver Lifecycle Tests ========== */

TEST(resolver_create) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);
    ASSERT_NOT_NULL(resolver);
    ASSERT_NOT_NULL(resolver->library_paths);
    ASSERT_EQ(resolver->path_count, 0);
    ASSERT_NULL(resolver->cache);
    ASSERT_FALSE(resolver->verbose);
    ASSERT_FALSE(resolver->disabled);

    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

TEST(resolver_destroy_null) {
    /* Should not crash on NULL */
    sysml2_resolver_destroy(NULL);
}

TEST(resolver_add_path) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);
    ASSERT_NOT_NULL(resolver);

    sysml2_resolver_add_path(resolver, "/tmp/test-lib");
    ASSERT_EQ(resolver->path_count, 1);

    /* Adding same path again should not duplicate */
    sysml2_resolver_add_path(resolver, "/tmp/test-lib");
    ASSERT_EQ(resolver->path_count, 1);

    /* Adding different path should work */
    sysml2_resolver_add_path(resolver, "/tmp/other-lib");
    ASSERT_EQ(resolver->path_count, 2);

    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

TEST(resolver_add_path_null_handling) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);
    ASSERT_NOT_NULL(resolver);

    /* Should not crash on NULL path */
    sysml2_resolver_add_path(resolver, NULL);
    ASSERT_EQ(resolver->path_count, 0);

    /* Should not crash on NULL resolver */
    sysml2_resolver_add_path(NULL, "/tmp/test");

    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

TEST(resolver_add_multiple_paths) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);
    ASSERT_NOT_NULL(resolver);

    /* Add more paths than initial capacity to test resizing */
    for (int i = 0; i < 20; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/tmp/lib%d", i);
        sysml2_resolver_add_path(resolver, path);
    }

    ASSERT_EQ(resolver->path_count, 20);

    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

/* ========== Caching Tests ========== */

TEST(resolver_cache_model) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);
    ASSERT_NOT_NULL(resolver);

    /* Create a dummy model */
    SysmlSemanticModel *model = SYSML2_ARENA_NEW(&arena, SysmlSemanticModel);
    model->source_name = "test.sysml";

    /* Cache it */
    sysml2_resolver_cache_model(resolver, "/tmp/test.sysml", model);
    ASSERT_NOT_NULL(resolver->cache);

    /* Retrieve it */
    SysmlSemanticModel *cached = sysml2_resolver_get_cached(resolver, "/tmp/test.sysml");
    ASSERT_EQ(cached, model);

    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

TEST(resolver_cache_model_null_handling) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);
    SysmlSemanticModel *model = SYSML2_ARENA_NEW(&arena, SysmlSemanticModel);

    /* Should not crash on NULL inputs */
    sysml2_resolver_cache_model(NULL, "/tmp/test.sysml", model);
    sysml2_resolver_cache_model(resolver, NULL, model);
    sysml2_resolver_cache_model(resolver, "/tmp/test.sysml", NULL);

    /* Cache should still be empty */
    ASSERT_NULL(resolver->cache);

    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

TEST(resolver_get_cached_nonexistent) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);
    ASSERT_NOT_NULL(resolver);

    /* Looking up non-existent path should return NULL */
    SysmlSemanticModel *cached = sysml2_resolver_get_cached(resolver, "/tmp/nonexistent.sysml");
    ASSERT_NULL(cached);

    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

TEST(resolver_get_cached_null_handling) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);

    /* Should not crash on NULL inputs */
    SysmlSemanticModel *result = sysml2_resolver_get_cached(NULL, "/tmp/test.sysml");
    ASSERT_NULL(result);

    result = sysml2_resolver_get_cached(resolver, NULL);
    ASSERT_NULL(result);

    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

TEST(resolver_cache_update_existing) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);

    /* Create two models */
    SysmlSemanticModel *model1 = SYSML2_ARENA_NEW(&arena, SysmlSemanticModel);
    model1->source_name = "model1";

    SysmlSemanticModel *model2 = SYSML2_ARENA_NEW(&arena, SysmlSemanticModel);
    model2->source_name = "model2";

    /* Cache first model */
    sysml2_resolver_cache_model(resolver, "/tmp/test.sysml", model1);
    ASSERT_EQ(sysml2_resolver_get_cached(resolver, "/tmp/test.sysml"), model1);

    /* Cache second model at same path - should update */
    sysml2_resolver_cache_model(resolver, "/tmp/test.sysml", model2);
    ASSERT_EQ(sysml2_resolver_get_cached(resolver, "/tmp/test.sysml"), model2);

    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

/* ========== Get All Models Tests ========== */

TEST(resolver_get_all_models_empty) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);

    size_t count;
    SysmlSemanticModel **models = sysml2_resolver_get_all_models(resolver, &count);
    ASSERT_EQ(count, 0);
    ASSERT_NULL(models);

    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

TEST(resolver_get_all_models) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);

    /* Create and cache models */
    SysmlSemanticModel *model1 = SYSML2_ARENA_NEW(&arena, SysmlSemanticModel);
    model1->source_name = "model1.sysml";
    sysml2_resolver_cache_model(resolver, "/tmp/model1.sysml", model1);

    SysmlSemanticModel *model2 = SYSML2_ARENA_NEW(&arena, SysmlSemanticModel);
    model2->source_name = "model2.sysml";
    sysml2_resolver_cache_model(resolver, "/tmp/model2.sysml", model2);

    size_t count;
    SysmlSemanticModel **models = sysml2_resolver_get_all_models(resolver, &count);
    ASSERT_EQ(count, 2);
    ASSERT_NOT_NULL(models);

    /* Verify both models are present (order may vary) */
    bool found1 = false, found2 = false;
    for (size_t i = 0; i < count; i++) {
        if (models[i] == model1) found1 = true;
        if (models[i] == model2) found2 = true;
    }
    ASSERT_TRUE(found1);
    ASSERT_TRUE(found2);

    free(models);
    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

TEST(resolver_get_all_models_null_handling) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);

    /* NULL resolver */
    size_t count = 999;
    SysmlSemanticModel **models = sysml2_resolver_get_all_models(NULL, &count);
    ASSERT_NULL(models);
    ASSERT_EQ(count, 0);

    /* NULL count pointer */
    models = sysml2_resolver_get_all_models(resolver, NULL);
    ASSERT_NULL(models);

    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

/* ========== Disabled Resolver Tests ========== */

TEST(resolver_disabled_skips_resolution) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);
    resolver->disabled = true;

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    /* Create model with imports */
    SysmlSemanticModel *model = SYSML2_ARENA_NEW(&arena, SysmlSemanticModel);
    model->source_name = "test.sysml";
    model->imports = SYSML2_ARENA_NEW_ARRAY(&arena, SysmlImport *, 1);
    model->imports[0] = SYSML2_ARENA_NEW(&arena, SysmlImport);
    model->imports[0]->target = "NonExistent";
    model->import_count = 1;

    /* Should succeed because resolver is disabled */
    Sysml2Result result = sysml2_resolver_resolve_imports(resolver, model, &diag);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_EQ(diag.error_count, 0);

    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

/* ========== Find File Tests ========== */

TEST(resolver_find_file_null_handling) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);

    /* NULL inputs should return NULL */
    char *result = sysml2_resolver_find_file(NULL, "ScalarValues");
    ASSERT_NULL(result);

    result = sysml2_resolver_find_file(resolver, NULL);
    ASSERT_NULL(result);

    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

TEST(resolver_find_file_no_paths) {
    FIXTURE_SETUP();

    Sysml2ImportResolver *resolver = sysml2_resolver_create(&arena, &intern);

    /* With no library paths, should not find anything */
    char *result = sysml2_resolver_find_file(resolver, "ScalarValues");
    ASSERT_NULL(result);

    sysml2_resolver_destroy(resolver);
    FIXTURE_TEARDOWN();
}

/* ========== Main ========== */

int main(void) {
    printf("Running import resolver tests...\n");

    /* Lifecycle tests */
    RUN_TEST(resolver_create);
    RUN_TEST(resolver_destroy_null);
    RUN_TEST(resolver_add_path);
    RUN_TEST(resolver_add_path_null_handling);
    RUN_TEST(resolver_add_multiple_paths);

    /* Caching tests */
    RUN_TEST(resolver_cache_model);
    RUN_TEST(resolver_cache_model_null_handling);
    RUN_TEST(resolver_get_cached_nonexistent);
    RUN_TEST(resolver_get_cached_null_handling);
    RUN_TEST(resolver_cache_update_existing);

    /* Get all models tests */
    RUN_TEST(resolver_get_all_models_empty);
    RUN_TEST(resolver_get_all_models);
    RUN_TEST(resolver_get_all_models_null_handling);

    /* Disabled resolver tests */
    RUN_TEST(resolver_disabled_skips_resolution);

    /* Find file tests */
    RUN_TEST(resolver_find_file_null_handling);
    RUN_TEST(resolver_find_file_no_paths);

    printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
