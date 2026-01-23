/*
 * SysML v2 Parser - Parser Tests
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/common.h"
#include "sysml2/arena.h"
#include "sysml2/intern.h"
#include "sysml2/lexer.h"
#include "sysml2/parser.h"
#include "sysml2/ast.h"
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
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/* Build line offsets for test source */
static uint32_t *build_line_offsets(const char *content, size_t length, uint32_t *out_count) {
    uint32_t count = 1;
    for (size_t i = 0; i < length; i++) {
        if (content[i] == '\n') count++;
    }

    uint32_t *offsets = malloc(count * sizeof(uint32_t));
    offsets[0] = 0;
    uint32_t line = 1;
    for (size_t i = 0; i < length && line < count; i++) {
        if (content[i] == '\n') {
            offsets[line++] = i + 1;
        }
    }

    *out_count = count;
    return offsets;
}

/* Helper to parse a string and return AST */
static Sysml2AstNamespace *parse_string(
    const char *source,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    Sysml2DiagContext *diag
) {
    uint32_t line_count;
    size_t len = strlen(source);
    uint32_t *line_offsets = build_line_offsets(source, len, &line_count);

    Sysml2SourceFile file = {
        .path = "test.kerml",
        .content = source,
        .content_length = len,
        .line_offsets = line_offsets,
        .line_count = line_count,
    };

    Sysml2Lexer lexer;
    sysml2_lexer_init(&lexer, &file, intern, diag);

    Sysml2Parser parser;
    sysml2_parser_init(&parser, &lexer, arena, diag);

    Sysml2AstNamespace *ast = sysml2_parser_parse(&parser);

    free(line_offsets);
    return ast;
}

TEST(empty_input) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2AstNamespace *ast = parse_string("", &arena, &intern, &diag);

    ASSERT_NOT_NULL(ast);
    ASSERT_EQ(ast->members, NULL);

    sysml2_arena_destroy(&arena);
}

TEST(simple_package) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2AstNamespace *ast = parse_string(
        "package MyPackage {\n"
        "}",
        &arena, &intern, &diag
    );

    ASSERT_NOT_NULL(ast);
    ASSERT_NOT_NULL(ast->members);
    ASSERT_EQ(ast->members->kind, SYSML2_AST_PACKAGE);

    Sysml2AstPackage *pkg = (Sysml2AstPackage *)ast->members->node;
    ASSERT_NOT_NULL(pkg);
    ASSERT_STR_EQ(pkg->name, "MyPackage");

    sysml2_arena_destroy(&arena);
}

TEST(nested_package) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2AstNamespace *ast = parse_string(
        "package Outer {\n"
        "    package Inner {\n"
        "    }\n"
        "}",
        &arena, &intern, &diag
    );

    ASSERT_NOT_NULL(ast);
    ASSERT_NOT_NULL(ast->members);

    Sysml2AstPackage *outer = (Sysml2AstPackage *)ast->members->node;
    ASSERT_NOT_NULL(outer);
    ASSERT_STR_EQ(outer->name, "Outer");
    ASSERT_NOT_NULL(outer->members);

    Sysml2AstPackage *inner = (Sysml2AstPackage *)outer->members->node;
    ASSERT_NOT_NULL(inner);
    ASSERT_STR_EQ(inner->name, "Inner");

    sysml2_arena_destroy(&arena);
}

TEST(simple_class) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2AstNamespace *ast = parse_string(
        "class Vehicle {\n"
        "}",
        &arena, &intern, &diag
    );

    ASSERT_NOT_NULL(ast);
    ASSERT_NOT_NULL(ast->members);
    ASSERT_EQ(ast->members->kind, SYSML2_AST_CLASS);

    Sysml2AstClassifier *cls = (Sysml2AstClassifier *)ast->members->node;
    ASSERT_NOT_NULL(cls);
    ASSERT_STR_EQ(cls->name, "Vehicle");

    sysml2_arena_destroy(&arena);
}

TEST(abstract_class) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2AstNamespace *ast = parse_string(
        "abstract class AbstractVehicle {\n"
        "}",
        &arena, &intern, &diag
    );

    ASSERT_NOT_NULL(ast);
    ASSERT_NOT_NULL(ast->members);

    Sysml2AstClassifier *cls = (Sysml2AstClassifier *)ast->members->node;
    ASSERT_NOT_NULL(cls);
    ASSERT_STR_EQ(cls->name, "AbstractVehicle");
    ASSERT(cls->prefix.is_abstract);

    sysml2_arena_destroy(&arena);
}

TEST(class_with_specialization) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2AstNamespace *ast = parse_string(
        "class Car :> Vehicle {\n"
        "}",
        &arena, &intern, &diag
    );

    ASSERT_NOT_NULL(ast);
    ASSERT_NOT_NULL(ast->members);

    Sysml2AstClassifier *cls = (Sysml2AstClassifier *)ast->members->node;
    ASSERT_NOT_NULL(cls);
    ASSERT_STR_EQ(cls->name, "Car");
    ASSERT_NOT_NULL(cls->relationships);
    ASSERT_EQ(cls->relationships->kind, SYSML2_REL_SPECIALIZES);
    ASSERT_NOT_NULL(cls->relationships->target);
    ASSERT_STR_EQ(cls->relationships->target->segments[0], "Vehicle");

    sysml2_arena_destroy(&arena);
}

TEST(feature_with_type) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2AstNamespace *ast = parse_string(
        "class Vehicle {\n"
        "    feature engine : Engine;\n"
        "}",
        &arena, &intern, &diag
    );

    ASSERT_NOT_NULL(ast);
    ASSERT_NOT_NULL(ast->members);

    Sysml2AstClassifier *cls = (Sysml2AstClassifier *)ast->members->node;
    ASSERT_NOT_NULL(cls);
    ASSERT_NOT_NULL(cls->members);
    ASSERT_EQ(cls->members->kind, SYSML2_AST_FEATURE);

    Sysml2AstFeature *feat = (Sysml2AstFeature *)cls->members->node;
    ASSERT_NOT_NULL(feat);
    ASSERT_STR_EQ(feat->name, "engine");
    ASSERT_NOT_NULL(feat->relationships);
    ASSERT_EQ(feat->relationships->kind, SYSML2_REL_TYPED_BY);

    sysml2_arena_destroy(&arena);
}

TEST(multiplicity) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2AstNamespace *ast = parse_string(
        "class Vehicle {\n"
        "    feature wheels[4] : Wheel;\n"
        "}",
        &arena, &intern, &diag
    );

    ASSERT_NOT_NULL(ast);
    ASSERT_NOT_NULL(ast->members);

    Sysml2AstClassifier *cls = (Sysml2AstClassifier *)ast->members->node;
    ASSERT_NOT_NULL(cls);
    ASSERT_NOT_NULL(cls->members);

    Sysml2AstFeature *feat = (Sysml2AstFeature *)cls->members->node;
    ASSERT_NOT_NULL(feat);
    ASSERT_NOT_NULL(feat->multiplicity);
    ASSERT_NOT_NULL(feat->multiplicity->lower);
    ASSERT_EQ(feat->multiplicity->lower->kind, SYSML2_EXPR_LITERAL_INT);
    ASSERT_EQ(feat->multiplicity->lower->int_value, 4);

    sysml2_arena_destroy(&arena);
}

TEST(import_statement) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2AstNamespace *ast = parse_string(
        "package MyPkg {\n"
        "    import OtherPkg::*;\n"
        "}",
        &arena, &intern, &diag
    );

    ASSERT_NOT_NULL(ast);
    ASSERT_NOT_NULL(ast->members);

    Sysml2AstPackage *pkg = (Sysml2AstPackage *)ast->members->node;
    ASSERT_NOT_NULL(pkg);
    ASSERT_NOT_NULL(pkg->members);
    ASSERT_EQ(pkg->members->kind, SYSML2_AST_IMPORT);

    Sysml2AstImport *imp = (Sysml2AstImport *)pkg->members->node;
    ASSERT_NOT_NULL(imp);
    ASSERT(imp->is_all);

    sysml2_arena_destroy(&arena);
}

TEST(datatype) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2AstNamespace *ast = parse_string(
        "datatype Point {\n"
        "    feature x : Real;\n"
        "    feature y : Real;\n"
        "}",
        &arena, &intern, &diag
    );

    ASSERT_NOT_NULL(ast);
    ASSERT_NOT_NULL(ast->members);
    ASSERT_EQ(ast->members->kind, SYSML2_AST_DATATYPE);

    Sysml2AstClassifier *dt = (Sysml2AstClassifier *)ast->members->node;
    ASSERT_NOT_NULL(dt);
    ASSERT_STR_EQ(dt->name, "Point");

    /* Count members */
    int count = 0;
    for (Sysml2AstMember *m = dt->members; m; m = m->next) {
        count++;
    }
    ASSERT_EQ(count, 2);

    sysml2_arena_destroy(&arena);
}

TEST(error_recovery) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    /* Missing semicolon */
    Sysml2AstNamespace *ast = parse_string(
        "class A {\n"
        "    feature x : X\n"  /* Missing semicolon */
        "    feature y : Y;\n"
        "}",
        &arena, &intern, &diag
    );

    ASSERT_NOT_NULL(ast);
    /* Should have parsed something despite error */
    ASSERT_NOT_NULL(ast->members);
    /* Should have reported an error */
    ASSERT(diag.error_count > 0);

    sysml2_arena_destroy(&arena);
}

int main(void) {
    printf("Running parser tests:\n");

    RUN_TEST(empty_input);
    RUN_TEST(simple_package);
    RUN_TEST(nested_package);
    RUN_TEST(simple_class);
    RUN_TEST(abstract_class);
    RUN_TEST(class_with_specialization);
    RUN_TEST(feature_with_type);
    RUN_TEST(multiplicity);
    RUN_TEST(import_statement);
    RUN_TEST(datatype);
    RUN_TEST(error_recovery);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
