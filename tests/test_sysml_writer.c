/*
 * SysML v2 Parser - SysML Writer Tests
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/common.h"
#include "sysml2/arena.h"
#include "sysml2/intern.h"
#include "sysml2/ast.h"
#include "sysml2/sysml_writer.h"
#include "sysml2/pipeline.h"
#include "sysml2/cli.h"
#include "sysml_parser.h"

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

/* ========== Basic Serialization Tests ========== */

TEST(sysml_write_empty_package) {
    FIXTURE_SETUP();

    const char *input = "package TestPkg { }";
    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Should contain package declaration */
    ASSERT(strstr(output, "package TestPkg") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

TEST(sysml_write_package_with_part_def) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    part def Vehicle { }\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    ASSERT(strstr(output, "package TestPkg") != NULL);
    ASSERT(strstr(output, "part def Vehicle") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

TEST(sysml_write_nested_packages) {
    FIXTURE_SETUP();

    const char *input =
        "package Outer {\n"
        "    package Inner {\n"
        "        part def Nested { }\n"
        "    }\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    ASSERT(strstr(output, "package Outer") != NULL);
    ASSERT(strstr(output, "package Inner") != NULL);
    ASSERT(strstr(output, "part def Nested") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Specialization Tests ========== */

TEST(sysml_write_specialization) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    part def Base { }\n"
        "    part def Derived :> Base { }\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    ASSERT(strstr(output, "part def Base") != NULL);
    ASSERT(strstr(output, ":> Base") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

TEST(sysml_write_multiple_specializations) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    part def A { }\n"
        "    part def B { }\n"
        "    part def C :> A, B { }\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Both A and B should appear as supertypes */
    ASSERT(strstr(output, "part def A") != NULL);
    ASSERT(strstr(output, "part def B") != NULL);
    ASSERT(strstr(output, "part def C :>") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Usage Tests ========== */

TEST(sysml_write_part_usage) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    part def Vehicle { }\n"
        "    part myCar : Vehicle;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    ASSERT(strstr(output, "part myCar : Vehicle") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

TEST(sysml_write_attribute_usage) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    part def Vehicle {\n"
        "        attribute mass : Real;\n"
        "    }\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    ASSERT(strstr(output, "attribute mass") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Import Tests ========== */

TEST(sysml_write_import) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    import ScalarValues::*;\n"
        "    part def MyPart { }\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    ASSERT(strstr(output, "import ScalarValues::*") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Comment Preservation Tests ========== */

TEST(sysml_write_line_comment) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    // This is a comment\n"
        "    part def MyPart { }\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Comments should be preserved */
    ASSERT(strstr(output, "// This is a comment") != NULL ||
           strstr(output, "//This is a comment") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Indentation Tests ========== */

TEST(sysml_write_indentation) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    part def Vehicle {\n"
        "        part engine : Engine;\n"
        "    }\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Check that nested elements have proper indentation */
    /* 4 spaces is canonical indent */
    ASSERT(strstr(output, "    part def Vehicle") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Error Handling Tests ========== */

TEST(sysml_write_null_model) {
    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(NULL, &output);
    /* Should handle gracefully - either error or empty output */
    if (result == SYSML2_OK) {
        /* Empty output is acceptable */
        free(output);
    }
}

TEST(sysml_write_null_output) {
    FIXTURE_SETUP();

    const char *input = "package TestPkg { }";
    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);

    if (model) {
        Sysml2Result result = sysml2_sysml_write_string(model, NULL);
        /* Should return error for NULL output pointer */
        ASSERT_EQ(result, SYSML2_ERROR_SYNTAX);
    }

    FIXTURE_TEARDOWN();
}

/* ========== Round-Trip Tests ========== */

TEST(sysml_roundtrip_simple) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    part def Vehicle { }\n"
        "}\n";

    /* First parse */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);

    /* Write to string */
    char *output1 = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model1, &output1);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output1);

    /* Parse the output */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);

    /* Write again */
    char *output2 = NULL;
    result = sysml2_sysml_write_string(model2, &output2);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output2);

    /* Second output should match first (canonical form) */
    ASSERT_STR_EQ(output1, output2);

    free(output1);
    free(output2);
    FIXTURE_TEARDOWN();
}

/* ========== Main ========== */

int main(void) {
    printf("Running SysML writer tests...\n");

    /* Basic serialization */
    RUN_TEST(sysml_write_empty_package);
    RUN_TEST(sysml_write_package_with_part_def);
    RUN_TEST(sysml_write_nested_packages);

    /* Specialization */
    RUN_TEST(sysml_write_specialization);
    RUN_TEST(sysml_write_multiple_specializations);

    /* Usages */
    RUN_TEST(sysml_write_part_usage);
    RUN_TEST(sysml_write_attribute_usage);

    /* Imports */
    RUN_TEST(sysml_write_import);

    /* Comments */
    RUN_TEST(sysml_write_line_comment);

    /* Indentation */
    RUN_TEST(sysml_write_indentation);

    /* Error handling */
    RUN_TEST(sysml_write_null_model);
    RUN_TEST(sysml_write_null_output);

    /* Round-trip */
    RUN_TEST(sysml_roundtrip_simple);

    printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
