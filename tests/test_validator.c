/*
 * SysML v2 Parser - Validator Tests
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/common.h"
#include "sysml2/arena.h"
#include "sysml2/intern.h"
#include "sysml2/ast.h"
#include "sysml2/ast_builder.h"
#include "sysml2/diagnostic.h"
#include "sysml2/validator.h"

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

/* ========== Test Helpers ========== */

typedef struct {
    Sysml2Arena arena;
    Sysml2Intern intern;
    Sysml2DiagContext diag_ctx;
    SysmlBuildContext *build_ctx;
} TestContext;

static void test_ctx_init(TestContext *ctx) {
    sysml2_arena_init(&ctx->arena);
    sysml2_intern_init(&ctx->intern, &ctx->arena);
    sysml2_diag_context_init(&ctx->diag_ctx, &ctx->arena);
    ctx->build_ctx = sysml2_build_context_create(&ctx->arena, &ctx->intern, "test.sysml");
}

static void test_ctx_destroy(TestContext *ctx) {
    sysml2_build_context_destroy(ctx->build_ctx);
    sysml2_intern_destroy(&ctx->intern);
    sysml2_arena_destroy(&ctx->arena);
}

/* ========== Symbol Table Tests ========== */

TEST(symtab_init) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2SymbolTable symtab;
    sysml2_symtab_init(&symtab, &arena, &intern);

    ASSERT_NOT_NULL(symtab.root_scope);
    ASSERT_NULL(symtab.root_scope->id);
    ASSERT_NULL(symtab.root_scope->parent);
    ASSERT_EQ(symtab.scope_count, 0);

    sysml2_symtab_destroy(&symtab);
    sysml2_arena_destroy(&arena);
}

TEST(symtab_get_or_create_scope) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2SymbolTable symtab;
    sysml2_symtab_init(&symtab, &arena, &intern);

    /* Get root scope */
    Sysml2Scope *root = sysml2_symtab_get_or_create_scope(&symtab, NULL);
    ASSERT_EQ(root, symtab.root_scope);

    /* Create new scope */
    Sysml2Scope *pkg = sysml2_symtab_get_or_create_scope(&symtab, "Package");
    ASSERT_NOT_NULL(pkg);
    ASSERT_STR_EQ(pkg->id, "Package");
    ASSERT_EQ(pkg->parent, symtab.root_scope);

    /* Get same scope again */
    Sysml2Scope *pkg2 = sysml2_symtab_get_or_create_scope(&symtab, "Package");
    ASSERT_EQ(pkg, pkg2);

    sysml2_symtab_destroy(&symtab);
    sysml2_arena_destroy(&arena);
}

TEST(symtab_nested_scopes) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2SymbolTable symtab;
    sysml2_symtab_init(&symtab, &arena, &intern);

    /* Create nested scope - parent is auto-created */
    Sysml2Scope *nested = sysml2_symtab_get_or_create_scope(&symtab, "Pkg::Inner");
    ASSERT_NOT_NULL(nested);
    ASSERT_STR_EQ(nested->id, "Pkg::Inner");

    /* Parent should have been created */
    ASSERT_NOT_NULL(nested->parent);
    ASSERT_STR_EQ(nested->parent->id, "Pkg");
    ASSERT_EQ(nested->parent->parent, symtab.root_scope);

    sysml2_symtab_destroy(&symtab);
    sysml2_arena_destroy(&arena);
}

TEST(symtab_add_symbol) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2SymbolTable symtab;
    sysml2_symtab_init(&symtab, &arena, &intern);

    Sysml2Scope *scope = sysml2_symtab_get_or_create_scope(&symtab, "Pkg");

    /* Add a symbol */
    Sysml2Symbol *sym = sysml2_symtab_add(&symtab, scope, "Engine", "Pkg::Engine", NULL);
    ASSERT_NOT_NULL(sym);
    ASSERT_STR_EQ(sym->name, "Engine");
    ASSERT_STR_EQ(sym->qualified_id, "Pkg::Engine");

    /* Look it up */
    Sysml2Symbol *found = sysml2_symtab_lookup(scope, "Engine");
    ASSERT_EQ(found, sym);

    /* Non-existent lookup */
    Sysml2Symbol *not_found = sysml2_symtab_lookup(scope, "NoSuch");
    ASSERT_NULL(not_found);

    sysml2_symtab_destroy(&symtab);
    sysml2_arena_destroy(&arena);
}

TEST(symtab_duplicate_returns_existing) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2SymbolTable symtab;
    sysml2_symtab_init(&symtab, &arena, &intern);

    Sysml2Scope *scope = sysml2_symtab_get_or_create_scope(&symtab, NULL);

    Sysml2Symbol *sym1 = sysml2_symtab_add(&symtab, scope, "X", "X", NULL);
    Sysml2Symbol *sym2 = sysml2_symtab_add(&symtab, scope, "X", "X", NULL);

    ASSERT_EQ(sym1, sym2); /* Should return existing */

    sysml2_symtab_destroy(&symtab);
    sysml2_arena_destroy(&arena);
}

TEST(symtab_resolve_simple) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2SymbolTable symtab;
    sysml2_symtab_init(&symtab, &arena, &intern);

    /* Add symbol to root scope */
    sysml2_symtab_add(&symtab, symtab.root_scope, "Engine", "Engine", NULL);

    /* Resolve from root scope */
    Sysml2Symbol *found = sysml2_symtab_resolve(&symtab, symtab.root_scope, "Engine");
    ASSERT_NOT_NULL(found);
    ASSERT_STR_EQ(found->name, "Engine");

    sysml2_symtab_destroy(&symtab);
    sysml2_arena_destroy(&arena);
}

TEST(symtab_resolve_parent_scope) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2SymbolTable symtab;
    sysml2_symtab_init(&symtab, &arena, &intern);

    /* Add symbol to root */
    sysml2_symtab_add(&symtab, symtab.root_scope, "GlobalType", "GlobalType", NULL);

    /* Create child scope */
    Sysml2Scope *child = sysml2_symtab_get_or_create_scope(&symtab, "Package");

    /* Should be able to resolve from child scope */
    Sysml2Symbol *found = sysml2_symtab_resolve(&symtab, child, "GlobalType");
    ASSERT_NOT_NULL(found);
    ASSERT_STR_EQ(found->name, "GlobalType");

    sysml2_symtab_destroy(&symtab);
    sysml2_arena_destroy(&arena);
}

TEST(symtab_resolve_qualified) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2SymbolTable symtab;
    sysml2_symtab_init(&symtab, &arena, &intern);

    /* Create Pkg::Engine */
    sysml2_symtab_add(&symtab, symtab.root_scope, "Pkg", "Pkg", NULL);
    Sysml2Scope *pkg_scope = sysml2_symtab_get_or_create_scope(&symtab, "Pkg");
    sysml2_symtab_add(&symtab, pkg_scope, "Engine", "Pkg::Engine", NULL);

    /* Resolve qualified name from root */
    Sysml2Symbol *found = sysml2_symtab_resolve(&symtab, symtab.root_scope, "Pkg::Engine");
    ASSERT_NOT_NULL(found);
    ASSERT_STR_EQ(found->qualified_id, "Pkg::Engine");

    sysml2_symtab_destroy(&symtab);
    sysml2_arena_destroy(&arena);
}

/* ========== Type Compatibility Tests ========== */

TEST(type_compat_part_def) {
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_PART_USAGE, SYSML_KIND_PART_DEF));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_PART_USAGE, SYSML_KIND_ITEM_DEF));
    ASSERT(!sysml2_is_type_compatible(SYSML_KIND_PART_USAGE, SYSML_KIND_ACTION_DEF));
}

TEST(type_compat_action_def) {
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_ACTION_USAGE, SYSML_KIND_ACTION_DEF));
    ASSERT(!sysml2_is_type_compatible(SYSML_KIND_ACTION_USAGE, SYSML_KIND_PART_DEF));
}

TEST(type_compat_state_def) {
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_STATE_USAGE, SYSML_KIND_STATE_DEF));
    ASSERT(!sysml2_is_type_compatible(SYSML_KIND_STATE_USAGE, SYSML_KIND_ACTION_DEF));
}

TEST(type_compat_port_def) {
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_PORT_USAGE, SYSML_KIND_PORT_DEF));
    ASSERT(!sysml2_is_type_compatible(SYSML_KIND_PORT_USAGE, SYSML_KIND_PART_DEF));
}

TEST(type_compat_requirement_def) {
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_REQUIREMENT_USAGE, SYSML_KIND_REQUIREMENT_DEF));
    ASSERT(!sysml2_is_type_compatible(SYSML_KIND_REQUIREMENT_USAGE, SYSML_KIND_CONSTRAINT_DEF));
}

TEST(type_compat_package_allows_all) {
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_PART_USAGE, SYSML_KIND_PACKAGE));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_ACTION_USAGE, SYSML_KIND_PACKAGE));
}

/* ========== Validation Tests ========== */

TEST(validate_empty_model) {
    TestContext ctx;
    test_ctx_init(&ctx);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);
    ASSERT_NOT_NULL(model);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_EQ(ctx.diag_ctx.error_count, 0);

    test_ctx_destroy(&ctx);
}

TEST(validate_no_errors) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create a valid model: package with part def */
    SysmlNode *pkg = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PACKAGE, "VehiclePkg");
    sysml2_build_add_element(ctx.build_ctx, pkg);
    sysml2_build_push_scope(ctx.build_ctx, pkg->id);

    SysmlNode *part_def = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_DEF, "Engine");
    sysml2_build_add_element(ctx.build_ctx, part_def);
    sysml2_build_push_scope(ctx.build_ctx, part_def->id);

    /* Create part usage typed by Engine */
    SysmlNode *part = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "engine");
    sysml2_build_add_typed_by(ctx.build_ctx, part, "Engine");
    sysml2_build_add_element(ctx.build_ctx, part);

    sysml2_build_pop_scope(ctx.build_ctx);
    sysml2_build_pop_scope(ctx.build_ctx);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_EQ(ctx.diag_ctx.error_count, 0);

    test_ctx_destroy(&ctx);
}

TEST(validate_e3001_undefined_type) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create part typed by non-existent type */
    SysmlNode *part = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "engine");
    sysml2_build_add_typed_by(ctx.build_ctx, part, "NoSuchType");
    sysml2_build_add_element(ctx.build_ctx, part);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT_EQ(ctx.diag_ctx.error_count, 1);

    test_ctx_destroy(&ctx);
}

TEST(validate_e3004_duplicate_name) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create two parts with same name in same scope */
    SysmlNode *part1 = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "sensor");
    part1->loc.line = 5;
    sysml2_build_add_element(ctx.build_ctx, part1);

    /* Reset anon counter to avoid ID collision */
    SysmlNode *part2 = sysml2_arena_alloc(&ctx.arena, sizeof(SysmlNode));
    part2->id = sysml2_intern(&ctx.intern, "sensor_dup");
    part2->name = sysml2_intern(&ctx.intern, "sensor");
    part2->kind = SYSML_KIND_PART_USAGE;
    part2->parent_id = NULL;
    part2->typed_by = NULL;
    part2->typed_by_count = 0;
    part2->specializes = NULL;
    part2->specializes_count = 0;
    part2->redefines = NULL;
    part2->redefines_count = 0;
    part2->references = NULL;
    part2->references_count = 0;
    part2->metadata = NULL;
    part2->metadata_count = 0;
    part2->prefix_metadata = NULL;
    part2->prefix_metadata_count = 0;
    part2->leading_trivia = NULL;
    part2->trailing_trivia = NULL;
    part2->loc.line = 8;
    sysml2_build_add_element(ctx.build_ctx, part2);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT_EQ(ctx.diag_ctx.error_count, 1);

    test_ctx_destroy(&ctx);
}

TEST(validate_e3005_circular_direct) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create A :> A (direct cycle) */
    SysmlNode *part_def = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_DEF, "A");
    sysml2_build_add_typed_by(ctx.build_ctx, part_def, "A");
    sysml2_build_add_element(ctx.build_ctx, part_def);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT(ctx.diag_ctx.error_count >= 1);

    test_ctx_destroy(&ctx);
}

TEST(validate_e3005_circular_indirect) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create A :> B, B :> A (indirect cycle) */
    SysmlNode *a = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_DEF, "A");
    sysml2_build_add_typed_by(ctx.build_ctx, a, "B");
    sysml2_build_add_element(ctx.build_ctx, a);

    SysmlNode *b = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_DEF, "B");
    sysml2_build_add_typed_by(ctx.build_ctx, b, "A");
    sysml2_build_add_element(ctx.build_ctx, b);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT(ctx.diag_ctx.error_count >= 1);

    test_ctx_destroy(&ctx);
}

TEST(validate_e3006_type_mismatch) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create action def */
    SysmlNode *action_def = sysml2_build_node(ctx.build_ctx, SYSML_KIND_ACTION_DEF, "DoSomething");
    sysml2_build_add_element(ctx.build_ctx, action_def);

    /* Try to type a part by the action def */
    SysmlNode *part = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "myPart");
    sysml2_build_add_typed_by(ctx.build_ctx, part, "DoSomething");
    sysml2_build_add_element(ctx.build_ctx, part);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT_EQ(ctx.diag_ctx.error_count, 1);

    test_ctx_destroy(&ctx);
}

TEST(validate_options_disable_checks) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create part typed by non-existent type */
    SysmlNode *part = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "engine");
    sysml2_build_add_typed_by(ctx.build_ctx, part, "NoSuchType");
    sysml2_build_add_element(ctx.build_ctx, part);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    /* Disable undefined type check */
    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    opts.check_undefined_types = false;

    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_EQ(ctx.diag_ctx.error_count, 0);

    test_ctx_destroy(&ctx);
}

TEST(validate_suggestions) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create Engine type */
    SysmlNode *eng_def = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_DEF, "Engine");
    sysml2_build_add_element(ctx.build_ctx, eng_def);

    /* Create part typed by "Egine" (typo) */
    SysmlNode *part = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "engine");
    sysml2_build_add_typed_by(ctx.build_ctx, part, "Egine");
    sysml2_build_add_element(ctx.build_ctx, part);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    opts.suggest_corrections = true;

    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT_EQ(ctx.diag_ctx.error_count, 1);

    /* Check that help text was added */
    ASSERT_NOT_NULL(ctx.diag_ctx.first);
    ASSERT_NOT_NULL(ctx.diag_ctx.first->help);

    test_ctx_destroy(&ctx);
}

/* ========== Find Similar Names Tests ========== */

TEST(find_similar_basic) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2SymbolTable symtab;
    sysml2_symtab_init(&symtab, &arena, &intern);

    /* Add some symbols */
    sysml2_symtab_add(&symtab, symtab.root_scope, "Engine", "Engine", NULL);
    sysml2_symtab_add(&symtab, symtab.root_scope, "Sensor", "Sensor", NULL);
    sysml2_symtab_add(&symtab, symtab.root_scope, "Motor", "Motor", NULL);

    const char *suggestions[3];
    size_t count = sysml2_symtab_find_similar(&symtab, symtab.root_scope, "Egine", suggestions, 3);

    ASSERT(count >= 1);
    ASSERT_STR_EQ(suggestions[0], "Engine");

    sysml2_symtab_destroy(&symtab);
    sysml2_arena_destroy(&arena);
}

/* ========== Main ========== */

int main(void) {
    printf("Running Validator tests:\n");

    /* Symbol Table tests */
    printf("\n  Symbol Table tests:\n");
    RUN_TEST(symtab_init);
    RUN_TEST(symtab_get_or_create_scope);
    RUN_TEST(symtab_nested_scopes);
    RUN_TEST(symtab_add_symbol);
    RUN_TEST(symtab_duplicate_returns_existing);
    RUN_TEST(symtab_resolve_simple);
    RUN_TEST(symtab_resolve_parent_scope);
    RUN_TEST(symtab_resolve_qualified);

    /* Type Compatibility tests */
    printf("\n  Type Compatibility tests:\n");
    RUN_TEST(type_compat_part_def);
    RUN_TEST(type_compat_action_def);
    RUN_TEST(type_compat_state_def);
    RUN_TEST(type_compat_port_def);
    RUN_TEST(type_compat_requirement_def);
    RUN_TEST(type_compat_package_allows_all);

    /* Validation tests */
    printf("\n  Validation tests:\n");
    RUN_TEST(validate_empty_model);
    RUN_TEST(validate_no_errors);
    RUN_TEST(validate_e3001_undefined_type);
    RUN_TEST(validate_e3004_duplicate_name);
    RUN_TEST(validate_e3005_circular_direct);
    RUN_TEST(validate_e3005_circular_indirect);
    RUN_TEST(validate_e3006_type_mismatch);
    RUN_TEST(validate_options_disable_checks);
    RUN_TEST(validate_suggestions);

    /* Find Similar tests */
    printf("\n  Find Similar tests:\n");
    RUN_TEST(find_similar_basic);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
