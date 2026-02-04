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

/* Test fixture macros for arena/intern setup and teardown */
#define FIXTURE_SETUP() \
    Sysml2Arena arena; \
    sysml2_arena_init(&arena); \
    Sysml2Intern intern; \
    sysml2_intern_init(&intern, &arena)

#define FIXTURE_TEARDOWN() \
    sysml2_intern_destroy(&intern); \
    sysml2_arena_destroy(&arena)

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
    FIXTURE_SETUP();

    Sysml2SymbolTable symtab;
    sysml2_symtab_init(&symtab, &arena, &intern);

    ASSERT_NOT_NULL(symtab.root_scope);
    ASSERT_NULL(symtab.root_scope->id);
    ASSERT_NULL(symtab.root_scope->parent);
    ASSERT_EQ(symtab.scope_count, 0);

    sysml2_symtab_destroy(&symtab);
    FIXTURE_TEARDOWN();
}

TEST(symtab_get_or_create_scope) {
    FIXTURE_SETUP();

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
    FIXTURE_TEARDOWN();
}

TEST(symtab_nested_scopes) {
    FIXTURE_SETUP();

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
    FIXTURE_TEARDOWN();
}

TEST(symtab_add_symbol) {
    FIXTURE_SETUP();

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
    FIXTURE_TEARDOWN();
}

TEST(symtab_duplicate_returns_existing) {
    FIXTURE_SETUP();

    Sysml2SymbolTable symtab;
    sysml2_symtab_init(&symtab, &arena, &intern);

    Sysml2Scope *scope = sysml2_symtab_get_or_create_scope(&symtab, NULL);

    Sysml2Symbol *sym1 = sysml2_symtab_add(&symtab, scope, "X", "X", NULL);
    Sysml2Symbol *sym2 = sysml2_symtab_add(&symtab, scope, "X", "X", NULL);

    ASSERT_EQ(sym1, sym2); /* Should return existing */

    sysml2_symtab_destroy(&symtab);
    FIXTURE_TEARDOWN();
}

TEST(symtab_resolve_simple) {
    FIXTURE_SETUP();

    Sysml2SymbolTable symtab;
    sysml2_symtab_init(&symtab, &arena, &intern);

    /* Add symbol to root scope */
    sysml2_symtab_add(&symtab, symtab.root_scope, "Engine", "Engine", NULL);

    /* Resolve from root scope */
    Sysml2Symbol *found = sysml2_symtab_resolve(&symtab, symtab.root_scope, "Engine");
    ASSERT_NOT_NULL(found);
    ASSERT_STR_EQ(found->name, "Engine");

    sysml2_symtab_destroy(&symtab);
    FIXTURE_TEARDOWN();
}

TEST(symtab_resolve_parent_scope) {
    FIXTURE_SETUP();

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
    FIXTURE_TEARDOWN();
}

TEST(symtab_resolve_qualified) {
    FIXTURE_SETUP();

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
    FIXTURE_TEARDOWN();
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
    /* State usages can also be typed by action defs (for StateAction patterns) */
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_STATE_USAGE, SYSML_KIND_ACTION_DEF));
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

/* KerML type compatibility - features can be typed by KerML classifiers */
TEST(type_compat_kerml_class) {
    /* Feature typed by Class should be valid */
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_FEATURE, SYSML_KIND_CLASS));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_PART_USAGE, SYSML_KIND_CLASS));
}

TEST(type_compat_kerml_structure) {
    /* Feature typed by Structure should be valid */
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_FEATURE, SYSML_KIND_STRUCTURE));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_PART_USAGE, SYSML_KIND_STRUCTURE));
}

TEST(type_compat_kerml_behavior) {
    /* Feature typed by Behavior should be valid */
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_FEATURE, SYSML_KIND_BEHAVIOR));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_STEP, SYSML_KIND_BEHAVIOR));
}

TEST(type_compat_kerml_type) {
    /* Feature typed by generic Type should be valid */
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_FEATURE, SYSML_KIND_TYPE));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_PART_USAGE, SYSML_KIND_TYPE));
}

TEST(type_compat_kerml_classifier) {
    /* Feature typed by Classifier should be valid */
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_FEATURE, SYSML_KIND_CLASSIFIER));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_PART_USAGE, SYSML_KIND_CLASSIFIER));
}

TEST(type_compat_kerml_association) {
    /* Connector typed by Association should be valid */
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_CONNECTOR, SYSML_KIND_ASSOCIATION));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_CONNECTOR, SYSML_KIND_ASSOC_STRUCT));
}

TEST(type_compat_kerml_interaction) {
    /* Feature typed by Interaction should be valid */
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_FEATURE, SYSML_KIND_INTERACTION));
}

TEST(type_compat_kerml_function) {
    /* Expression typed by Function should be valid */
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_EXPRESSION, SYSML_KIND_FUNCTION));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_BOOL_EXPRESSION, SYSML_KIND_PREDICATE));
}

TEST(type_compat_kerml_feature_by_definition) {
    /* KerML features can be typed by any SysML definition */
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_FEATURE, SYSML_KIND_ATTRIBUTE_DEF));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_FEATURE, SYSML_KIND_PART_DEF));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_STEP, SYSML_KIND_ACTION_DEF));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_CONNECTOR, SYSML_KIND_CONNECTION_DEF));
}

TEST(type_compat_parameter_flexible) {
    /* Parameters can be typed by any definition (part params, item params, etc.) */
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_PARAMETER, SYSML_KIND_PART_DEF));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_PARAMETER, SYSML_KIND_ITEM_DEF));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_PARAMETER, SYSML_KIND_ATTRIBUTE_DEF));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_PARAMETER, SYSML_KIND_ACTION_DEF));
    ASSERT(sysml2_is_type_compatible(SYSML_KIND_PARAMETER, SYSML_KIND_OCCURRENCE_DEF));
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
    memset(part2, 0, sizeof(SysmlNode));  /* Zero all fields first */
    part2->id = sysml2_intern(&ctx.intern, "sensor_dup");
    part2->name = sysml2_intern(&ctx.intern, "sensor");
    part2->kind = SYSML_KIND_PART_USAGE;
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
    FIXTURE_SETUP();

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
    FIXTURE_TEARDOWN();
}

/* ========== E3007 Multiplicity Validation Tests ========== */

TEST(validate_e3007_inverted_bounds) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create part with inverted multiplicity [5..2] */
    SysmlNode *part = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "wheels");
    part->multiplicity_lower = sysml2_intern(&ctx.intern, "5");
    part->multiplicity_upper = sysml2_intern(&ctx.intern, "2");
    sysml2_build_add_element(ctx.build_ctx, part);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT_EQ(ctx.diag_ctx.error_count, 1);

    test_ctx_destroy(&ctx);
}

TEST(validate_e3007_negative_bound) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create part with negative multiplicity [-1] */
    SysmlNode *part = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "wheels");
    part->multiplicity_lower = sysml2_intern(&ctx.intern, "-1");
    sysml2_build_add_element(ctx.build_ctx, part);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT_EQ(ctx.diag_ctx.error_count, 1);

    test_ctx_destroy(&ctx);
}

TEST(validate_e3007_valid_multiplicity) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create parts with valid multiplicities */
    SysmlNode *p1 = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "p1");
    p1->multiplicity_lower = sysml2_intern(&ctx.intern, "0");
    p1->multiplicity_upper = sysml2_intern(&ctx.intern, "1");
    sysml2_build_add_element(ctx.build_ctx, p1);

    SysmlNode *p2 = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "p2");
    p2->multiplicity_lower = sysml2_intern(&ctx.intern, "1");
    p2->multiplicity_upper = sysml2_intern(&ctx.intern, "*");
    sysml2_build_add_element(ctx.build_ctx, p2);

    SysmlNode *p3 = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "p3");
    p3->multiplicity_lower = sysml2_intern(&ctx.intern, "4");
    sysml2_build_add_element(ctx.build_ctx, p3);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_EQ(ctx.diag_ctx.error_count, 0);

    test_ctx_destroy(&ctx);
}

/* ========== E3002 Undefined Feature Tests ========== */

TEST(validate_e3002_undefined_redefines) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create part def A with feature x */
    SysmlNode *def_a = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_DEF, "A");
    sysml2_build_add_element(ctx.build_ctx, def_a);
    sysml2_build_push_scope(ctx.build_ctx, def_a->id);

    SysmlNode *x = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "x");
    sysml2_build_add_element(ctx.build_ctx, x);

    sysml2_build_pop_scope(ctx.build_ctx);

    /* Create part def B :> A */
    SysmlNode *def_b = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_DEF, "B");
    sysml2_build_add_specializes(ctx.build_ctx, def_b, "A");
    sysml2_build_add_element(ctx.build_ctx, def_b);
    sysml2_build_push_scope(ctx.build_ctx, def_b->id);

    /* Create part redefines y (y doesn't exist in A) */
    SysmlNode *y = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "y");
    sysml2_build_add_redefines(ctx.build_ctx, y, "y");
    sysml2_build_add_element(ctx.build_ctx, y);

    sysml2_build_pop_scope(ctx.build_ctx);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT(ctx.diag_ctx.error_count >= 1);

    test_ctx_destroy(&ctx);
}

/* ========== E3008 Redefinition Compatibility Tests ========== */

TEST(validate_e3008_multiplicity_widening) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create part def A with x[0..5] */
    SysmlNode *def_a = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_DEF, "A");
    sysml2_build_add_element(ctx.build_ctx, def_a);
    sysml2_build_push_scope(ctx.build_ctx, def_a->id);

    SysmlNode *x = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "x");
    x->multiplicity_lower = sysml2_intern(&ctx.intern, "0");
    x->multiplicity_upper = sysml2_intern(&ctx.intern, "5");
    sysml2_build_add_element(ctx.build_ctx, x);

    sysml2_build_pop_scope(ctx.build_ctx);

    /* Create part def B :> A */
    SysmlNode *def_b = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_DEF, "B");
    sysml2_build_add_specializes(ctx.build_ctx, def_b, "A");
    sysml2_build_add_element(ctx.build_ctx, def_b);
    sysml2_build_push_scope(ctx.build_ctx, def_b->id);

    /* Create part redefines x[0..10] - widening is an error */
    SysmlNode *x_redef = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "x");
    x_redef->multiplicity_lower = sysml2_intern(&ctx.intern, "0");
    x_redef->multiplicity_upper = sysml2_intern(&ctx.intern, "10");
    sysml2_build_add_redefines(ctx.build_ctx, x_redef, "x");
    sysml2_build_add_element(ctx.build_ctx, x_redef);

    sysml2_build_pop_scope(ctx.build_ctx);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT(ctx.diag_ctx.error_count >= 1);

    test_ctx_destroy(&ctx);
}

TEST(validate_e3008_valid_narrowing) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create part def A with x[0..5] */
    SysmlNode *def_a = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_DEF, "A");
    sysml2_build_add_element(ctx.build_ctx, def_a);
    sysml2_build_push_scope(ctx.build_ctx, def_a->id);

    SysmlNode *x = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "x");
    x->multiplicity_lower = sysml2_intern(&ctx.intern, "0");
    x->multiplicity_upper = sysml2_intern(&ctx.intern, "5");
    sysml2_build_add_element(ctx.build_ctx, x);

    sysml2_build_pop_scope(ctx.build_ctx);

    /* Create part def B :> A */
    SysmlNode *def_b = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_DEF, "B");
    sysml2_build_add_specializes(ctx.build_ctx, def_b, "A");
    sysml2_build_add_element(ctx.build_ctx, def_b);
    sysml2_build_push_scope(ctx.build_ctx, def_b->id);

    /* Create part redefines x[1..3] - narrowing is valid */
    SysmlNode *x_redef = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "x");
    x_redef->multiplicity_lower = sysml2_intern(&ctx.intern, "1");
    x_redef->multiplicity_upper = sysml2_intern(&ctx.intern, "3");
    sysml2_build_add_redefines(ctx.build_ctx, x_redef, "x");
    sysml2_build_add_element(ctx.build_ctx, x_redef);

    sysml2_build_pop_scope(ctx.build_ctx);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_EQ(ctx.diag_ctx.error_count, 0);

    test_ctx_destroy(&ctx);
}

/* ========== E3003 Undefined Namespace Tests ========== */

TEST(validate_e3003_undefined_namespace) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create package with import of non-existent namespace */
    SysmlNode *pkg = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PACKAGE, "TestPkg");
    sysml2_build_add_element(ctx.build_ctx, pkg);
    sysml2_build_push_scope(ctx.build_ctx, pkg->id);

    sysml2_build_add_import(ctx.build_ctx, SYSML_KIND_IMPORT_ALL, "NonExistent::*");

    sysml2_build_pop_scope(ctx.build_ctx);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT(ctx.diag_ctx.error_count >= 1);

    test_ctx_destroy(&ctx);
}

TEST(validate_e3003_valid_import) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create library package */
    SysmlNode *lib = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PACKAGE, "LibPkg");
    sysml2_build_add_element(ctx.build_ctx, lib);

    /* Create package with import of existing namespace */
    SysmlNode *pkg = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PACKAGE, "TestPkg");
    sysml2_build_add_element(ctx.build_ctx, pkg);
    sysml2_build_push_scope(ctx.build_ctx, pkg->id);

    sysml2_build_add_import(ctx.build_ctx, SYSML_KIND_IMPORT_ALL, "LibPkg::*");

    sysml2_build_pop_scope(ctx.build_ctx);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_EQ(ctx.diag_ctx.error_count, 0);

    test_ctx_destroy(&ctx);
}

/* ========== Abstract Instantiation Tests ========== */

TEST(validate_abstract_instantiation_warning) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create abstract part def */
    SysmlNode *abs_def = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_DEF, "Vehicle");
    abs_def->is_abstract = true;
    sysml2_build_add_element(ctx.build_ctx, abs_def);

    /* Create concrete part typed by abstract def */
    SysmlNode *part = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "myVehicle");
    sysml2_build_add_typed_by(ctx.build_ctx, part, "Vehicle");
    sysml2_build_add_element(ctx.build_ctx, part);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    /* This should succeed but emit a warning */
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_EQ(ctx.diag_ctx.warning_count, 1);

    test_ctx_destroy(&ctx);
}

TEST(validate_options_disable_new_checks) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create part with inverted multiplicity */
    SysmlNode *part = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "wheels");
    part->multiplicity_lower = sysml2_intern(&ctx.intern, "5");
    part->multiplicity_upper = sysml2_intern(&ctx.intern, "2");
    sysml2_build_add_element(ctx.build_ctx, part);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    /* Disable multiplicity check */
    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    opts.check_multiplicity = false;

    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_EQ(ctx.diag_ctx.error_count, 0);

    test_ctx_destroy(&ctx);
}

/* ========== Multi-Model Validation Tests ========== */

TEST(validate_multi_source_file_on_diagnostics) {
    /* Verify that sysml2_validate_multi sets source_file on diagnostics */
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create a source file for the model */
    Sysml2SourceFile *sf = sysml2_arena_alloc(&ctx.arena, sizeof(Sysml2SourceFile));
    *sf = (Sysml2SourceFile){
        .path = sysml2_intern(&ctx.intern, "operations.sysml"),
        .content = NULL, .content_length = 0,
        .line_offsets = NULL, .line_count = 0,
    };

    /* Create a model with an undefined type error */
    SysmlNode *part = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "engine");
    sysml2_build_add_typed_by(ctx.build_ctx, part, "NoSuchType");
    sysml2_build_add_element(ctx.build_ctx, part);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);
    model->source_file = sf;

    SysmlSemanticModel *models[] = { model };
    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate_multi(models, 1, &ctx.diag_ctx,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT_EQ(ctx.diag_ctx.error_count, 1);

    /* The diagnostic should carry the source_file from the model */
    ASSERT_NOT_NULL(ctx.diag_ctx.first);
    ASSERT_EQ(ctx.diag_ctx.first->file, sf);
    ASSERT_STR_EQ(ctx.diag_ctx.first->file->path, "operations.sysml");

    test_ctx_destroy(&ctx);
}

TEST(validate_multi_different_source_files) {
    /* Verify that diagnostics from different models get the correct source_file */
    Sysml2Arena arena;
    sysml2_arena_init(&arena);
    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);
    Sysml2DiagContext diag_ctx;
    sysml2_diag_context_init(&diag_ctx, &arena);

    /* Create source files for two models */
    Sysml2SourceFile *sf1 = sysml2_arena_alloc(&arena, sizeof(Sysml2SourceFile));
    *sf1 = (Sysml2SourceFile){
        .path = sysml2_intern(&intern, "model_a.sysml"),
        .content = NULL, .content_length = 0,
        .line_offsets = NULL, .line_count = 0,
    };
    Sysml2SourceFile *sf2 = sysml2_arena_alloc(&arena, sizeof(Sysml2SourceFile));
    *sf2 = (Sysml2SourceFile){
        .path = sysml2_intern(&intern, "model_b.sysml"),
        .content = NULL, .content_length = 0,
        .line_offsets = NULL, .line_count = 0,
    };

    /* Model A: part with undefined type */
    SysmlBuildContext *build1 = sysml2_build_context_create(&arena, &intern, "model_a.sysml");
    SysmlNode *part1 = sysml2_build_node(build1, SYSML_KIND_PART_USAGE, "foo");
    sysml2_build_add_typed_by(build1, part1, "UndefinedA");
    sysml2_build_add_element(build1, part1);
    SysmlSemanticModel *m1 = sysml2_build_finalize(build1);
    m1->source_file = sf1;

    /* Model B: part with undefined type */
    SysmlBuildContext *build2 = sysml2_build_context_create(&arena, &intern, "model_b.sysml");
    SysmlNode *part2 = sysml2_build_node(build2, SYSML_KIND_PART_USAGE, "bar");
    sysml2_build_add_typed_by(build2, part2, "UndefinedB");
    sysml2_build_add_element(build2, part2);
    SysmlSemanticModel *m2 = sysml2_build_finalize(build2);
    m2->source_file = sf2;

    SysmlSemanticModel *models[] = { m1, m2 };
    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate_multi(models, 2, &diag_ctx,
        &arena, &intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT_EQ(diag_ctx.error_count, 2);

    /* First diagnostic should reference model_a's source_file */
    ASSERT_NOT_NULL(diag_ctx.first);
    ASSERT_NOT_NULL(diag_ctx.first->file);
    ASSERT_STR_EQ(diag_ctx.first->file->path, "model_a.sysml");

    /* Second diagnostic should reference model_b's source_file */
    Sysml2Diagnostic *second = diag_ctx.first->next;
    ASSERT_NOT_NULL(second);
    ASSERT_NOT_NULL(second->file);
    ASSERT_STR_EQ(second->file->path, "model_b.sysml");

    sysml2_build_context_destroy(build1);
    sysml2_build_context_destroy(build2);
    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(validate_multi_null_source_file_safe) {
    /* Verify that validate_multi works when model->source_file is NULL */
    TestContext ctx;
    test_ctx_init(&ctx);

    SysmlNode *part = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "engine");
    sysml2_build_add_typed_by(ctx.build_ctx, part, "NoSuchType");
    sysml2_build_add_element(ctx.build_ctx, part);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);
    /* source_file is NULL from build_finalize — this must not crash */

    SysmlSemanticModel *models[] = { model };
    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate_multi(models, 1, &ctx.diag_ctx,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT_EQ(ctx.diag_ctx.error_count, 1);

    /* Diagnostic should have NULL file (backward compat with no source_file) */
    ASSERT_NOT_NULL(ctx.diag_ctx.first);
    ASSERT_NULL(ctx.diag_ctx.first->file);

    test_ctx_destroy(&ctx);
}

/* ========== Diagnostic Location Tests ========== */

TEST(validate_e3001_diagnostic_has_line_number) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create part typed by non-existent type with specific location */
    SysmlNode *part = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "engine");
    part->loc.line = 42;
    part->loc.column = 10;
    part->loc.offset = 200;
    sysml2_build_add_typed_by(ctx.build_ctx, part, "NoSuchType");
    sysml2_build_add_element(ctx.build_ctx, part);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT_EQ(ctx.diag_ctx.error_count, 1);
    ASSERT_NOT_NULL(ctx.diag_ctx.first);
    ASSERT_EQ(ctx.diag_ctx.first->range.start.line, 42);
    ASSERT_EQ(ctx.diag_ctx.first->range.start.column, 10);

    test_ctx_destroy(&ctx);
}

TEST(validate_e3004_diagnostic_has_line_numbers) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* First definition at line 5 */
    SysmlNode *part1 = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "sensor");
    part1->loc.line = 5;
    part1->loc.column = 5;
    part1->loc.offset = 40;
    sysml2_build_add_element(ctx.build_ctx, part1);

    /* Duplicate at line 12 (manually created to avoid ID collision) */
    SysmlNode *part2 = sysml2_arena_alloc(&ctx.arena, sizeof(SysmlNode));
    memset(part2, 0, sizeof(SysmlNode));
    part2->id = sysml2_intern(&ctx.intern, "sensor_dup2");
    part2->name = sysml2_intern(&ctx.intern, "sensor");
    part2->kind = SYSML_KIND_PART_USAGE;
    part2->loc.line = 12;
    part2->loc.column = 5;
    part2->loc.offset = 100;
    sysml2_build_add_element(ctx.build_ctx, part2);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT_EQ(ctx.diag_ctx.error_count, 1);

    /* Diagnostic should point to the duplicate (line 12) */
    ASSERT_NOT_NULL(ctx.diag_ctx.first);
    ASSERT_EQ(ctx.diag_ctx.first->range.start.line, 12);

    /* Note should point to the original (line 5) */
    ASSERT_NOT_NULL(ctx.diag_ctx.first->notes);
    ASSERT_EQ(ctx.diag_ctx.first->notes->range.start.line, 5);

    test_ctx_destroy(&ctx);
}

TEST(validate_e3005_diagnostic_has_line_number) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create A :> A (direct cycle) at line 20 */
    SysmlNode *part_def = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_DEF, "A");
    part_def->loc.line = 20;
    part_def->loc.column = 3;
    part_def->loc.offset = 150;
    sysml2_build_add_typed_by(ctx.build_ctx, part_def, "A");
    sysml2_build_add_element(ctx.build_ctx, part_def);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT(ctx.diag_ctx.error_count >= 1);

    /* Find E3005 diagnostic - may coexist with E3006 */
    Sysml2Diagnostic *diag = ctx.diag_ctx.first;
    while (diag && diag->code != SYSML2_DIAG_E3005_CIRCULAR_SPECIALIZATION)
        diag = diag->next;
    ASSERT_NOT_NULL(diag);
    ASSERT_EQ(diag->range.start.line, 20);

    test_ctx_destroy(&ctx);
}

TEST(validate_e3006_diagnostic_has_line_number) {
    TestContext ctx;
    test_ctx_init(&ctx);

    /* Create action def */
    SysmlNode *action_def = sysml2_build_node(ctx.build_ctx, SYSML_KIND_ACTION_DEF, "DoSomething");
    sysml2_build_add_element(ctx.build_ctx, action_def);

    /* Try to type a part by the action def at line 7 */
    SysmlNode *part = sysml2_build_node(ctx.build_ctx, SYSML_KIND_PART_USAGE, "myPart");
    part->loc.line = 7;
    part->loc.column = 5;
    part->loc.offset = 60;
    sysml2_build_add_typed_by(ctx.build_ctx, part, "DoSomething");
    sysml2_build_add_element(ctx.build_ctx, part);

    SysmlSemanticModel *model = sysml2_build_finalize(ctx.build_ctx);

    Sysml2ValidationOptions opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate(model, &ctx.diag_ctx, NULL,
        &ctx.arena, &ctx.intern, &opts);

    ASSERT_EQ(result, SYSML2_ERROR_SEMANTIC);
    ASSERT_EQ(ctx.diag_ctx.error_count, 1);
    ASSERT_NOT_NULL(ctx.diag_ctx.first);
    ASSERT_EQ(ctx.diag_ctx.first->range.start.line, 7);

    test_ctx_destroy(&ctx);
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

    /* KerML Type Compatibility tests */
    printf("\n  KerML Type Compatibility tests:\n");
    RUN_TEST(type_compat_kerml_class);
    RUN_TEST(type_compat_kerml_structure);
    RUN_TEST(type_compat_kerml_behavior);
    RUN_TEST(type_compat_kerml_type);
    RUN_TEST(type_compat_kerml_classifier);
    RUN_TEST(type_compat_kerml_association);
    RUN_TEST(type_compat_kerml_interaction);
    RUN_TEST(type_compat_kerml_function);
    RUN_TEST(type_compat_kerml_feature_by_definition);
    RUN_TEST(type_compat_parameter_flexible);

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

    /* E3007 Multiplicity Validation tests */
    printf("\n  E3007 Multiplicity Validation tests:\n");
    RUN_TEST(validate_e3007_inverted_bounds);
    RUN_TEST(validate_e3007_negative_bound);
    RUN_TEST(validate_e3007_valid_multiplicity);

    /* E3002 Undefined Feature tests */
    printf("\n  E3002 Undefined Feature tests:\n");
    RUN_TEST(validate_e3002_undefined_redefines);

    /* E3008 Redefinition Compatibility tests */
    printf("\n  E3008 Redefinition Compatibility tests:\n");
    RUN_TEST(validate_e3008_multiplicity_widening);
    RUN_TEST(validate_e3008_valid_narrowing);

    /* E3003 Undefined Namespace tests */
    printf("\n  E3003 Undefined Namespace tests:\n");
    RUN_TEST(validate_e3003_undefined_namespace);
    RUN_TEST(validate_e3003_valid_import);

    /* Abstract Instantiation tests */
    printf("\n  Abstract Instantiation tests:\n");
    RUN_TEST(validate_abstract_instantiation_warning);

    /* Validation Options tests */
    printf("\n  Validation Options tests:\n");
    RUN_TEST(validate_options_disable_new_checks);

    /* Multi-Model Validation tests */
    printf("\n  Multi-Model Validation tests:\n");
    RUN_TEST(validate_multi_source_file_on_diagnostics);
    RUN_TEST(validate_multi_different_source_files);
    RUN_TEST(validate_multi_null_source_file_safe);

    /* Diagnostic Location tests */
    printf("\n  Diagnostic Location tests:\n");
    RUN_TEST(validate_e3001_diagnostic_has_line_number);
    RUN_TEST(validate_e3004_diagnostic_has_line_numbers);
    RUN_TEST(validate_e3005_diagnostic_has_line_number);
    RUN_TEST(validate_e3006_diagnostic_has_line_number);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
