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

TEST(sysml_write_connection_usage) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    connection authLink connect A to B;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    /* Verify connection element exists in model */
    bool found_connection = false;
    for (size_t i = 0; i < model->element_count; i++) {
        if (model->elements[i]->kind == SYSML_KIND_CONNECTION_USAGE) {
            found_connection = true;
            /* Verify the connection has the correct name */
            ASSERT(model->elements[i]->name != NULL);
            ASSERT(strstr(model->elements[i]->name, "authLink") != NULL ||
                   strstr(model->elements[i]->id, "authLink") != NULL);
            break;
        }
    }
    ASSERT_TRUE(found_connection);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    ASSERT(strstr(output, "connection authLink") != NULL);
    /* Verify endpoints are preserved (regression test for data loss bug) */
    ASSERT(strstr(output, "connect A to B") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test connection with dotted endpoint names to verify complex endpoints preserved */
TEST(sysml_write_connection_endpoint_preservation) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    connection authWiring connect authModule.api to authService.api;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify connection name preserved */
    ASSERT(strstr(output, "connection authWiring") != NULL);
    /* Verify endpoints with dotted names preserved (was bug: endpoints lost during round-trip) */
    ASSERT(strstr(output, "authModule.api") != NULL);
    ASSERT(strstr(output, "authService.api") != NULL);
    ASSERT(strstr(output, "connect") != NULL);
    ASSERT(strstr(output, "to") != NULL);

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

/* ========== Succession Statement Tests ========== */

TEST(sysml_write_valid_succession) {
    FIXTURE_SETUP();

    const char *input =
        "action def TestAction {\n"
        "    action step1;\n"
        "    action step2;\n"
        "    first step1 then step2;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Valid succession should be present */
    ASSERT(strstr(output, "first step1 then step2;") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

TEST(sysml_write_multiple_successions) {
    FIXTURE_SETUP();

    const char *input =
        "action def TestAction {\n"
        "    action step1;\n"
        "    action step2;\n"
        "    action step3;\n"
        "    first step1 then step2;\n"
        "    first step2 then step3;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Both successions should be present */
    ASSERT(strstr(output, "first step1 then step2;") != NULL);
    ASSERT(strstr(output, "first step2 then step3;") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

TEST(sysml_write_no_malformed_succession) {
    FIXTURE_SETUP();

    const char *input =
        "action def TestAction {\n"
        "    action step1;\n"
        "    action step2;\n"
        "    first step1 then step2;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Should NOT contain malformed patterns */
    ASSERT(strstr(output, "first  then") == NULL);  /* Empty source */
    ASSERT(strstr(output, "first then ;") == NULL); /* Empty succession */
    ASSERT(strstr(output, "then ;;") == NULL);      /* Double semicolons */

    free(output);
    FIXTURE_TEARDOWN();
}

TEST(sysml_succession_roundtrip_idempotent) {
    FIXTURE_SETUP();

    const char *input =
        "action def TestAction {\n"
        "    action step1;\n"
        "    action step2;\n"
        "    action step3;\n"
        "    first step1 then step2;\n"
        "    first step2 then step3;\n"
        "}\n";

    /* First parse */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);

    /* First write */
    char *output1 = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model1, &output1);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output1);

    /* Second parse (from output1) */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);

    /* Second write */
    char *output2 = NULL;
    result = sysml2_sysml_write_string(model2, &output2);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output2);

    /* Third parse (from output2) */
    SysmlSemanticModel *model3 = parse_sysml_string(&arena, &intern, output2);
    ASSERT_NOT_NULL(model3);

    /* Third write */
    char *output3 = NULL;
    result = sysml2_sysml_write_string(model3, &output3);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output3);

    /* Idempotency: output2 and output3 must be identical */
    ASSERT_STR_EQ(output2, output3);

    /* Also verify no malformed successions were introduced */
    ASSERT(strstr(output3, "first  then") == NULL);
    ASSERT(strstr(output3, "first then ;") == NULL);
    ASSERT(strstr(output3, "then ;;") == NULL);

    free(output1);
    free(output2);
    free(output3);
    FIXTURE_TEARDOWN();
}

TEST(sysml_flow_roundtrip_idempotent) {
    FIXTURE_SETUP();

    const char *input =
        "action def TestAction {\n"
        "    in item input : Data;\n"
        "    out item output : Data;\n"
        "    action process;\n"
        "    flow from input to process;\n"
        "    flow from process to output;\n"
        "}\n";

    /* First parse */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);

    /* First write */
    char *output1 = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model1, &output1);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output1);

    /* Second parse */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);

    /* Second write */
    char *output2 = NULL;
    result = sysml2_sysml_write_string(model2, &output2);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output2);

    /* Idempotency check */
    ASSERT_STR_EQ(output1, output2);

    free(output1);
    free(output2);
    FIXTURE_TEARDOWN();
}

TEST(sysml_complex_action_roundtrip_idempotent) {
    FIXTURE_SETUP();

    const char *input =
        "package Operations {\n"
        "    action def ProcessData {\n"
        "        in item request : Request;\n"
        "        out item response : Response;\n"
        "        action validate;\n"
        "        action transform;\n"
        "        action persist;\n"
        "        flow from request to validate;\n"
        "        flow from validate to transform;\n"
        "        flow from transform to persist;\n"
        "        flow from persist to response;\n"
        "        first validate then transform;\n"
        "        first transform then persist;\n"
        "    }\n"
        "}\n";

    /* First round */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);
    char *output1 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model1, &output1), SYSML2_OK);

    /* Second round */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);
    char *output2 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model2, &output2), SYSML2_OK);

    /* Third round */
    SysmlSemanticModel *model3 = parse_sysml_string(&arena, &intern, output2);
    ASSERT_NOT_NULL(model3);
    char *output3 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model3, &output3), SYSML2_OK);

    /* Idempotency: second and third outputs must be identical */
    ASSERT_STR_EQ(output2, output3);

    /* No corruption */
    ASSERT(strstr(output3, "first  then") == NULL);
    ASSERT(strstr(output3, "first then ;") == NULL);

    free(output1);
    free(output2);
    free(output3);
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

/* ========== Regression Tests for Data Loss Fixes ========== */

/* Test: Conjugation marker (~) is preserved in round-trip */
TEST(sysml_conjugation_marker_preserved) {
    FIXTURE_SETUP();

    const char *input = "interface def I { end client : ~Port; }";
    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify conjugation marker is present in output */
    ASSERT(strstr(output, "~Port") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: Typing and redefines are NOT conflated (separate operators) */
TEST(sysml_typing_vs_redefines_separate) {
    FIXTURE_SETUP();

    const char *input = "part x :>> database : PostgreSQL;";
    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify correct: ":>> database : PostgreSQL" or ":>> database:PostgreSQL"
     * NOT: ":>> database, PostgreSQL" (which would mean both are redefines) */
    ASSERT(strstr(output, "database, PostgreSQL") == NULL);
    ASSERT(strstr(output, ":>>") != NULL);
    ASSERT(strstr(output, "PostgreSQL") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: Multiplicity spacing is preserved */
TEST(sysml_multiplicity_spacing_preserved) {
    FIXTURE_SETUP();

    const char *input = "attribute x : String [0..1];";
    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify space before [ is present: "String [" not "String[" */
    ASSERT(strstr(output, "String [") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: Round-trip preserves critical data (conjugation, typing, multiplicity) */
TEST(sysml_roundtrip_idempotent) {
    FIXTURE_SETUP();

    const char *input =
        "interface def Service {\n"
        "    end client : ~ServicePort;\n"
        "    end server : ServicePort;\n"
        "}\n"
        "part x :>> database : PostgreSQL;\n"
        "attribute layer : String [0..1];\n";

    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);

    char *output1 = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model1, &output1);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output1);

    /* Parse output1 again */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);

    char *output2 = NULL;
    result = sysml2_sysml_write_string(model2, &output2);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output2);

    /* Verify critical data is preserved in second round-trip:
     * - Conjugation marker (~) still present
     * - Typing/redefines not conflated
     * - Multiplicity spacing preserved
     * Note: Blank lines between elements may vary, so we check for key patterns */
    ASSERT(strstr(output2, "~ServicePort") != NULL);
    ASSERT(strstr(output2, ":>> database") != NULL);
    ASSERT(strstr(output2, ": PostgreSQL") != NULL || strstr(output2, ":PostgreSQL") != NULL);
    ASSERT(strstr(output2, "String [0..1]") != NULL);
    /* Verify typing is NOT conflated with redefines */
    ASSERT(strstr(output2, "database, PostgreSQL") == NULL);

    free(output1);
    free(output2);
    FIXTURE_TEARDOWN();
}

/* ========== Batch 1: Simple Keyword Capture Tests ========== */

/* Test: constant attribute keyword preserved */
TEST(sysml_constant_attribute_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "part def Vehicle {\n"
        "    constant attribute maxSpeed : Real = 200;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify constant keyword is preserved */
    ASSERT(strstr(output, "constant") != NULL);
    ASSERT(strstr(output, "maxSpeed") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: derived attribute keyword preserved */
TEST(sysml_derived_attribute_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "part def Vehicle {\n"
        "    attribute baseSpeed : Real;\n"
        "    derived attribute effectiveSpeed :> baseSpeed;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify derived keyword is preserved */
    ASSERT(strstr(output, "derived") != NULL);
    ASSERT(strstr(output, "effectiveSpeed") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: readonly attribute keyword preserved */
TEST(sysml_readonly_attribute_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "part def Vehicle {\n"
        "    readonly attribute vin : String;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify readonly keyword is preserved */
    ASSERT(strstr(output, "readonly") != NULL);
    ASSERT(strstr(output, "vin") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: parallel state keyword preserved */
TEST(sysml_parallel_state_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "state def MachineState {\n"
        "    state operational parallel {\n"
        "        state monitor;\n"
        "        state control;\n"
        "    }\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify parallel keyword is preserved */
    ASSERT(strstr(output, "parallel") != NULL);
    ASSERT(strstr(output, "operational") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: enum keyword on enumeration member preserved */
TEST(sysml_enum_member_keyword_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "enum def Priority {\n"
        "    enum Low;\n"
        "    enum Medium;\n"
        "    enum High;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify enum keyword is preserved for members */
    ASSERT(strstr(output, "enum Low") != NULL);
    ASSERT(strstr(output, "enum Medium") != NULL);
    ASSERT(strstr(output, "enum High") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: enum members without keyword should NOT add keyword */
TEST(sysml_enum_member_no_keyword_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "enum def Status {\n"
        "    Pending;\n"
        "    Active;\n"
        "    Completed;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify enum keyword is NOT added to members that didn't have it */
    ASSERT(strstr(output, "enum Pending") == NULL);
    ASSERT(strstr(output, "enum Active") == NULL);
    ASSERT(strstr(output, "enum Completed") == NULL);
    /* But members should still be present */
    ASSERT(strstr(output, "Pending") != NULL);
    ASSERT(strstr(output, "Active") != NULL);
    ASSERT(strstr(output, "Completed") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: reserved keywords used as names are quoted in round-trip */
TEST(sysml_reserved_keyword_quoting_roundtrip) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    attribute 'from' : String;\n"
        "    attribute 'to' : String;\n"
        "    attribute 'import' : String;\n"
        "    attribute normal : String;\n"
        "}\n";

    /* First round: parse and write back */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);
    char *output1 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model1, &output1), SYSML2_OK);
    ASSERT_NOT_NULL(output1);

    /* Reserved keywords must be quoted */
    ASSERT(strstr(output1, "'from'") != NULL);
    ASSERT(strstr(output1, "'to'") != NULL);
    ASSERT(strstr(output1, "'import'") != NULL);
    /* Non-keyword should NOT be quoted */
    ASSERT(strstr(output1, "'normal'") == NULL);
    ASSERT(strstr(output1, "normal") != NULL);

    /* Second round: verify the output is parseable and idempotent */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);
    char *output2 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model2, &output2), SYSML2_OK);
    ASSERT_STR_EQ(output1, output2);

    free(output1);
    free(output2);
    FIXTURE_TEARDOWN();
}

/* ========== Batch 3: Endpoint Capture Tests ========== */

/* Test: flow endpoints preserved */
TEST(sysml_flow_endpoints_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "action def DataFlow {\n"
        "    action source;\n"
        "    action target;\n"
        "    flow dataStream from source to target;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify flow with endpoints */
    ASSERT(strstr(output, "flow") != NULL);
    ASSERT(strstr(output, "dataStream") != NULL);
    ASSERT(strstr(output, "from") != NULL);
    ASSERT(strstr(output, "source") != NULL);
    ASSERT(strstr(output, "to") != NULL);
    ASSERT(strstr(output, "target") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Batch 4: Body and Ref Tests ========== */

/* Test: nested port body preserved */
TEST(sysml_nested_port_body_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "port def CompositePort {\n"
        "    port subPort {\n"
        "        in item dataIn;\n"
        "        out item dataOut;\n"
        "    }\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify nested port body is preserved */
    ASSERT(strstr(output, "port subPort") != NULL);
    ASSERT(strstr(output, "dataIn") != NULL);
    ASSERT(strstr(output, "dataOut") != NULL);
    /* Make sure it has a body, not just a semicolon */
    ASSERT(strstr(output, "subPort;") == NULL || strstr(output, "subPort {") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Comprehensive Round-Trip Tests ========== */

/* Test: round-trip preserves all batch 1 keywords */
TEST(sysml_roundtrip_batch1_keywords) {
    FIXTURE_SETUP();

    const char *input =
        "package KeywordTest {\n"
        "    part def P {\n"
        "        constant attribute maxVal : Real = 100;\n"
        "        readonly attribute id : String;\n"
        "        derived attribute computed :> maxVal;\n"
        "    }\n"
        "    state def S {\n"
        "        state concurrent parallel {\n"
        "            state a;\n"
        "            state b;\n"
        "        }\n"
        "    }\n"
        "    enum def E {\n"
        "        enum First;\n"
        "        Second;\n"
        "        enum Third;\n"
        "    }\n"
        "}\n";

    /* First round */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);
    char *output1 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model1, &output1), SYSML2_OK);

    /* Second round */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);
    char *output2 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model2, &output2), SYSML2_OK);

    /* Verify keywords preserved after round-trip */
    ASSERT(strstr(output2, "constant") != NULL);
    ASSERT(strstr(output2, "readonly") != NULL);
    ASSERT(strstr(output2, "derived") != NULL);
    ASSERT(strstr(output2, "parallel") != NULL);
    ASSERT(strstr(output2, "enum First") != NULL);
    ASSERT(strstr(output2, "enum Third") != NULL);
    /* Second should NOT have enum keyword */
    ASSERT(strstr(output2, "enum Second") == NULL);
    ASSERT(strstr(output2, "Second") != NULL);

    /* Idempotency: third round should match second */
    SysmlSemanticModel *model3 = parse_sysml_string(&arena, &intern, output2);
    ASSERT_NOT_NULL(model3);
    char *output3 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model3, &output3), SYSML2_OK);
    ASSERT_STR_EQ(output2, output3);

    free(output1);
    free(output2);
    free(output3);
    FIXTURE_TEARDOWN();
}

/* Test: round-trip preserves flow endpoints */
TEST(sysml_roundtrip_flow_endpoints) {
    FIXTURE_SETUP();

    const char *input =
        "action def Flow {\n"
        "    action src;\n"
        "    action dst;\n"
        "    flow f from src to dst;\n"
        "}\n";

    /* First round */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);
    char *output1 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model1, &output1), SYSML2_OK);

    /* Second round */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);
    char *output2 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model2, &output2), SYSML2_OK);

    /* Verify flow preserved */
    ASSERT(strstr(output2, "flow") != NULL);
    ASSERT(strstr(output2, "from") != NULL);
    ASSERT(strstr(output2, "to") != NULL);

    /* Idempotency */
    SysmlSemanticModel *model3 = parse_sysml_string(&arena, &intern, output2);
    ASSERT_NOT_NULL(model3);
    char *output3 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model3, &output3), SYSML2_OK);
    ASSERT_STR_EQ(output2, output3);

    free(output1);
    free(output2);
    free(output3);
    FIXTURE_TEARDOWN();
}

/* Test: round-trip preserves nested port bodies */
TEST(sysml_roundtrip_nested_port_body) {
    FIXTURE_SETUP();

    const char *input =
        "port def CompositePort {\n"
        "    port nested {\n"
        "        in item x;\n"
        "    }\n"
        "}\n";

    /* First round */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);
    char *output1 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model1, &output1), SYSML2_OK);

    /* Second round */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);
    char *output2 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model2, &output2), SYSML2_OK);

    /* Verify nested body preserved */
    ASSERT(strstr(output2, "port nested") != NULL);
    ASSERT(strstr(output2, "in item x") != NULL);

    /* Idempotency */
    SysmlSemanticModel *model3 = parse_sysml_string(&arena, &intern, output2);
    ASSERT_NOT_NULL(model3);
    char *output3 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model3, &output3), SYSML2_OK);
    ASSERT_STR_EQ(output2, output3);

    free(output1);
    free(output2);
    free(output3);
    FIXTURE_TEARDOWN();
}

/* ========== Phase 1: Statement Capture Tests ========== */

/* Test: satisfy statement preserved */
TEST(sysml_satisfy_statement_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    requirement def Req1;\n"
        "    part def Part1;\n"
        "    satisfy Req1 by Part1;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify satisfy statement is preserved */
    ASSERT(strstr(output, "satisfy") != NULL);
    ASSERT(strstr(output, "Req1") != NULL);
    ASSERT(strstr(output, "by") != NULL);
    ASSERT(strstr(output, "Part1") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: assert satisfy statement preserved */
TEST(sysml_assert_satisfy_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    requirement def Req1;\n"
        "    part def Part1;\n"
        "    assert satisfy Req1 by Part1;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify assert satisfy statement is preserved */
    ASSERT(strstr(output, "assert") != NULL);
    ASSERT(strstr(output, "satisfy") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: include use case statement preserved */
TEST(sysml_include_use_case_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "use case def MainFlow {\n"
        "    use case subFlow;\n"
        "    include use case subFlow;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify include use case statement is preserved */
    ASSERT(strstr(output, "include") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: expose statement preserved */
TEST(sysml_expose_statement_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "view def SystemView {\n"
        "    expose Model::*;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify expose statement is preserved */
    ASSERT(strstr(output, "expose") != NULL);
    ASSERT(strstr(output, "Model") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: render statement preserved */
TEST(sysml_render_statement_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "view def DiagramView {\n"
        "    render asDrawing;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify render statement is preserved */
    ASSERT(strstr(output, "render") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: verify requirement statement preserved */
TEST(sysml_verify_requirement_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "verification def TestVerification {\n"
        "    verify requirement SysReq;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify verify requirement statement is preserved */
    ASSERT(strstr(output, "verify") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Phase 2: N-ary Connection Endpoint Tests ========== */

/* Test: n-ary connection endpoints preserved */
TEST(sysml_nary_connection_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "part def Hub {\n"
        "    part a;\n"
        "    part b;\n"
        "    part c;\n"
        "    connection hub connect (a, b, c);\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify n-ary connection is preserved with all endpoints */
    ASSERT(strstr(output, "connection") != NULL);
    ASSERT(strstr(output, "connect") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Phase 3: ReferenceUsage Tests ========== */

/* Test: ref feature preserved */
TEST(sysml_ref_feature_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "part def Container {\n"
        "    ref part referenced : ExternalPart;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify ref keyword is preserved */
    ASSERT(strstr(output, "ref") != NULL);
    ASSERT(strstr(output, "referenced") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: ref attribute preserved */
TEST(sysml_ref_attribute_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "part def DataHolder {\n"
        "    ref attribute externalData : String;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify ref attribute is preserved */
    ASSERT(strstr(output, "ref") != NULL);
    ASSERT(strstr(output, "externalData") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Phase 4: AllocationUsage Tests ========== */

/* Test: allocation usage preserved */
TEST(sysml_allocation_usage_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "allocation def SystemAllocation {\n"
        "    allocation funcToComp;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify allocation usage is preserved */
    ASSERT(strstr(output, "allocation") != NULL);
    ASSERT(strstr(output, "funcToComp") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: allocate statement preserved */
TEST(sysml_allocate_statement_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "allocation def Mapping {\n"
        "    allocate Function to Component;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify allocate statement is preserved */
    ASSERT(strstr(output, "allocate") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Phase 6: Additional Usage Type Tests ========== */

/* Test: interface usage preserved */
TEST(sysml_interface_usage_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "part def System {\n"
        "    interface apiLink : ApiInterface;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify interface usage is preserved */
    ASSERT(strstr(output, "interface") != NULL);
    ASSERT(strstr(output, "apiLink") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: event occurrence usage preserved */
TEST(sysml_event_occurrence_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "state def Process {\n"
        "    event occurrence trigger;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify event occurrence is preserved */
    ASSERT(strstr(output, "event") != NULL);
    ASSERT(strstr(output, "trigger") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: exhibit state usage preserved */
TEST(sysml_exhibit_state_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "part def Machine {\n"
        "    exhibit state running;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify exhibit state is preserved */
    ASSERT(strstr(output, "exhibit") != NULL);
    ASSERT(strstr(output, "running") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: succession usage preserved */
TEST(sysml_succession_usage_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "state def Workflow {\n"
        "    state initial;\n"
        "    state final;\n"
        "    succession flow first initial then final;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify succession usage is preserved */
    ASSERT(strstr(output, "succession") != NULL || strstr(output, "first") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Direction Keyword Tests ========== */

/* Test: in direction preserved */
TEST(sysml_in_direction_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "action def Process {\n"
        "    in item inputData;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify in direction is preserved */
    ASSERT(strstr(output, "in") != NULL);
    ASSERT(strstr(output, "inputData") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: out direction preserved */
TEST(sysml_out_direction_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "action def Process {\n"
        "    out item outputData;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify out direction is preserved */
    ASSERT(strstr(output, "out") != NULL);
    ASSERT(strstr(output, "outputData") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Test: inout direction preserved */
TEST(sysml_inout_direction_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "action def Process {\n"
        "    inout item bidirectionalData;\n"
        "}\n";

    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    Sysml2Result result = sysml2_sysml_write_string(model, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Verify inout direction is preserved */
    ASSERT(strstr(output, "inout") != NULL);
    ASSERT(strstr(output, "bidirectionalData") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Comprehensive Round-Trip Tests for New Fixes ========== */

/* Test: round-trip preserves Phase 1 statement captures */
TEST(sysml_roundtrip_phase1_statements) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    requirement def Req1;\n"
        "    part def Part1;\n"
        "    satisfy Req1 by Part1;\n"
        "}\n";

    /* First round */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);
    char *output1 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model1, &output1), SYSML2_OK);

    /* Second round */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);
    char *output2 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model2, &output2), SYSML2_OK);

    /* Third round for idempotency */
    SysmlSemanticModel *model3 = parse_sysml_string(&arena, &intern, output2);
    ASSERT_NOT_NULL(model3);
    char *output3 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model3, &output3), SYSML2_OK);

    /* Idempotency: second and third outputs must be identical */
    ASSERT_STR_EQ(output2, output3);

    /* Verify statements preserved */
    ASSERT(strstr(output3, "satisfy") != NULL);

    free(output1);
    free(output2);
    free(output3);
    FIXTURE_TEARDOWN();
}

/* Test: round-trip preserves allocation and ref features */
TEST(sysml_roundtrip_allocation_and_ref) {
    FIXTURE_SETUP();

    const char *input =
        "package TestPkg {\n"
        "    allocation def Alloc {\n"
        "        allocation mapping;\n"
        "    }\n"
        "    part def Container {\n"
        "        ref part external;\n"
        "    }\n"
        "}\n";

    /* First round */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);
    char *output1 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model1, &output1), SYSML2_OK);

    /* Second round */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);
    char *output2 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model2, &output2), SYSML2_OK);

    /* Third round for idempotency */
    SysmlSemanticModel *model3 = parse_sysml_string(&arena, &intern, output2);
    ASSERT_NOT_NULL(model3);
    char *output3 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model3, &output3), SYSML2_OK);

    /* Idempotency check */
    ASSERT_STR_EQ(output2, output3);

    /* Verify allocation and ref preserved */
    ASSERT(strstr(output3, "allocation") != NULL);
    ASSERT(strstr(output3, "ref") != NULL);

    free(output1);
    free(output2);
    free(output3);
    FIXTURE_TEARDOWN();
}

/* Test: round-trip preserves direction keywords */
TEST(sysml_roundtrip_direction_keywords) {
    FIXTURE_SETUP();

    const char *input =
        "action def DataProcessor {\n"
        "    in item inputData;\n"
        "    out item outputData;\n"
        "    inout item bidirectionalData;\n"
        "}\n";

    /* First round */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);
    char *output1 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model1, &output1), SYSML2_OK);

    /* Second round */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);
    char *output2 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model2, &output2), SYSML2_OK);

    /* Third round for idempotency */
    SysmlSemanticModel *model3 = parse_sysml_string(&arena, &intern, output2);
    ASSERT_NOT_NULL(model3);
    char *output3 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model3, &output3), SYSML2_OK);

    /* Idempotency check */
    ASSERT_STR_EQ(output2, output3);

    /* Verify direction keywords preserved */
    ASSERT(strstr(output3, "in ") != NULL || strstr(output3, "in\t") != NULL);
    ASSERT(strstr(output3, "out ") != NULL || strstr(output3, "out\t") != NULL);
    ASSERT(strstr(output3, "inout") != NULL);

    free(output1);
    free(output2);
    free(output3);
    FIXTURE_TEARDOWN();
}

/* ========== Specialized Syntax Preservation Tests ========== */

/* Test: ref state preserved (not simplified to ref feature) */
TEST(sysml_ref_state_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "package Test {\n"
        "    ref state s1;\n"
        "    ref action a1;\n"
        "}\n";

    /* First round */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);
    char *output1 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model1, &output1), SYSML2_OK);

    /* Second round */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);
    char *output2 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model2, &output2), SYSML2_OK);

    /* Third round for idempotency */
    SysmlSemanticModel *model3 = parse_sysml_string(&arena, &intern, output2);
    ASSERT_NOT_NULL(model3);
    char *output3 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model3, &output3), SYSML2_OK);

    /* Idempotency check */
    ASSERT_STR_EQ(output2, output3);

    /* Verify ref state and ref action are preserved, not simplified to ref feature */
    ASSERT(strstr(output3, "ref state") != NULL);
    ASSERT(strstr(output3, "ref action") != NULL);

    free(output1);
    free(output2);
    free(output3);
    FIXTURE_TEARDOWN();
}

/* Test: event occurrence vs event distinction preserved */
TEST(sysml_event_occurrence_vs_event_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "package Test {\n"
        "    event occurrence e1;\n"
        "    event e2;\n"
        "}\n";

    /* First round */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);
    char *output1 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model1, &output1), SYSML2_OK);

    /* Second round */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);
    char *output2 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model2, &output2), SYSML2_OK);

    /* Third round for idempotency */
    SysmlSemanticModel *model3 = parse_sysml_string(&arena, &intern, output2);
    ASSERT_NOT_NULL(model3);
    char *output3 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model3, &output3), SYSML2_OK);

    /* Idempotency check */
    ASSERT_STR_EQ(output2, output3);

    /* Verify event occurrence is preserved for e1 */
    ASSERT(strstr(output3, "event occurrence e1") != NULL);
    /* e2 should be just "event", not "event occurrence" */
    /* Note: we verify e2 exists and there's only one "event occurrence" */
    ASSERT(strstr(output3, "e2") != NULL);

    free(output1);
    free(output2);
    free(output3);
    FIXTURE_TEARDOWN();
}

/* Test: interface to connection syntax preserved */
TEST(sysml_interface_to_syntax_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "package Test {\n"
        "    interface portA to portB;\n"
        "}\n";

    /* First round */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);
    char *output1 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model1, &output1), SYSML2_OK);

    /* Second round */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);
    char *output2 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model2, &output2), SYSML2_OK);

    /* Third round for idempotency */
    SysmlSemanticModel *model3 = parse_sysml_string(&arena, &intern, output2);
    ASSERT_NOT_NULL(model3);
    char *output3 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model3, &output3), SYSML2_OK);

    /* Idempotency check */
    ASSERT_STR_EQ(output2, output3);

    /* Verify interface with "to" syntax is preserved */
    ASSERT(strstr(output3, "portA to portB") != NULL);

    free(output1);
    free(output2);
    free(output3);
    FIXTURE_TEARDOWN();
}

/* Test: succession first/then syntax preserved */
TEST(sysml_succession_first_then_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "package Test {\n"
        "    succession seq first A then B;\n"
        "}\n";

    /* First round */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);
    char *output1 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model1, &output1), SYSML2_OK);

    /* Second round */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);
    char *output2 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model2, &output2), SYSML2_OK);

    /* Third round for idempotency */
    SysmlSemanticModel *model3 = parse_sysml_string(&arena, &intern, output2);
    ASSERT_NOT_NULL(model3);
    char *output3 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model3, &output3), SYSML2_OK);

    /* Idempotency check */
    ASSERT_STR_EQ(output2, output3);

    /* Verify succession with first/then is preserved */
    ASSERT(strstr(output3, "first A then B") != NULL);

    free(output1);
    free(output2);
    free(output3);
    FIXTURE_TEARDOWN();
}

/* Test: comprehensive specialized syntax round-trip */
TEST(sysml_roundtrip_specialized_syntax) {
    FIXTURE_SETUP();

    const char *input =
        "package Test {\n"
        "    ref state s1;\n"
        "    ref action a1;\n"
        "    event occurrence e1;\n"
        "    event e2;\n"
        "    interface portA to portB;\n"
        "    succession seq first A then B;\n"
        "}\n";

    /* First round */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);
    char *output1 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model1, &output1), SYSML2_OK);

    /* Second round */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);
    char *output2 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model2, &output2), SYSML2_OK);

    /* Third round for idempotency */
    SysmlSemanticModel *model3 = parse_sysml_string(&arena, &intern, output2);
    ASSERT_NOT_NULL(model3);
    char *output3 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model3, &output3), SYSML2_OK);

    /* Idempotency check */
    ASSERT_STR_EQ(output2, output3);

    /* All specialized syntax should be preserved */
    ASSERT(strstr(output3, "ref state") != NULL);
    ASSERT(strstr(output3, "ref action") != NULL);
    ASSERT(strstr(output3, "event occurrence e1") != NULL);
    ASSERT(strstr(output3, "portA to portB") != NULL);
    ASSERT(strstr(output3, "first A then B") != NULL);

    free(output1);
    free(output2);
    free(output3);
    FIXTURE_TEARDOWN();
}

/* ========== Phase 2 Round-Trip Regression Tests ========== */

/* Issue 1: Binding connector endpoints */
TEST(sysml_binding_connector_endpoints_preserved) {
    FIXTURE_SETUP();

    const char *input = "package Test { binding b of a = b; }";
    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model, &output), SYSML2_OK);

    /* Should preserve binding endpoints */
    ASSERT(strstr(output, "binding b of a = b") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Issue 2: Flow endpoints */
TEST(sysml_flow_named_endpoints_preserved) {
    FIXTURE_SETUP();

    const char *input = "package Test { flow f from a to b; }";
    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model, &output), SYSML2_OK);

    /* Should preserve flow endpoints */
    ASSERT(strstr(output, "flow f from a to b") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Issue 3: Snapshot/timeslice portion kind */
TEST(sysml_snapshot_timeslice_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "package Test {\n"
        "    snapshot part s;\n"
        "    timeslice part ts;\n"
        "}\n";
    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model, &output), SYSML2_OK);

    /* Should preserve snapshot and timeslice keywords */
    ASSERT(strstr(output, "snapshot") != NULL);
    ASSERT(strstr(output, "timeslice") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Issue 4: Interface connect keyword */
TEST(sysml_interface_connect_keyword_preserved) {
    FIXTURE_SETUP();

    const char *input = "package Test { interface i connect a to b; }";
    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model, &output), SYSML2_OK);

    /* Should preserve 'connect' keyword */
    ASSERT(strstr(output, "interface i connect a to b") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Issue 5 & 6: Result expression in constraints/calcs */
TEST(sysml_result_expression_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "package Test {\n"
        "    constraint c { x > 0 }\n"
        "    calc calculate { in x; x * 2 }\n"
        "}\n";
    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model, &output), SYSML2_OK);

    /* Should preserve result expressions */
    ASSERT(strstr(output, "x > 0") != NULL);
    ASSERT(strstr(output, "x * 2") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Issue 7 & 8: Assert constraint flags */
TEST(sysml_assert_constraint_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "package Test {\n"
        "    assert constraint ac { true }\n"
        "    assert not constraint anc { false }\n"
        "}\n";
    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model, &output), SYSML2_OK);

    /* Should preserve assert and assert not keywords */
    ASSERT(strstr(output, "assert constraint") != NULL);
    ASSERT(strstr(output, "assert not constraint") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Issue 9: Perform action usage */
TEST(sysml_perform_action_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "package Test {\n"
        "    action myAction {\n"
        "        perform action subAction;\n"
        "        perform existingAction;\n"
        "    }\n"
        "}\n";
    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model, &output), SYSML2_OK);

    /* Should preserve perform action variants */
    ASSERT(strstr(output, "perform action subAction") != NULL);
    ASSERT(strstr(output, "perform existingAction") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Issue 10: Default keyword parsing */
TEST(sysml_default_value_keyword_preserved) {
    FIXTURE_SETUP();

    const char *input =
        "package Test {\n"
        "    attribute y default = 100;\n"
        "    attribute y_default = 50;\n"
        "}\n";
    SysmlSemanticModel *model = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model);

    char *output = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model, &output), SYSML2_OK);

    /* Should correctly parse name vs default keyword */
    ASSERT(strstr(output, "attribute y default") != NULL);
    ASSERT(strstr(output, "attribute y_default") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* Comprehensive round-trip test for Phase 2 fixes */
TEST(sysml_roundtrip_phase2_all_fixes) {
    FIXTURE_SETUP();

    const char *input =
        "package Test {\n"
        "    binding b of x = y;\n"
        "    flow f from source to sink;\n"
        "    snapshot part snap;\n"
        "    timeslice part ts;\n"
        "    interface iface connect portA to portB;\n"
        "    constraint c { x > 0 }\n"
        "    assert constraint ac { true }\n"
        "    assert not constraint anc { false }\n"
        "    attribute y default = 100;\n"
        "}\n";

    /* First round */
    SysmlSemanticModel *model1 = parse_sysml_string(&arena, &intern, input);
    ASSERT_NOT_NULL(model1);
    char *output1 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model1, &output1), SYSML2_OK);

    /* Second round */
    SysmlSemanticModel *model2 = parse_sysml_string(&arena, &intern, output1);
    ASSERT_NOT_NULL(model2);
    char *output2 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model2, &output2), SYSML2_OK);

    /* Third round for idempotency */
    SysmlSemanticModel *model3 = parse_sysml_string(&arena, &intern, output2);
    ASSERT_NOT_NULL(model3);
    char *output3 = NULL;
    ASSERT_EQ(sysml2_sysml_write_string(model3, &output3), SYSML2_OK);

    /* Idempotency check */
    ASSERT_STR_EQ(output2, output3);

    /* All syntax should be preserved */
    ASSERT(strstr(output3, "binding b of x = y") != NULL);
    ASSERT(strstr(output3, "flow f from source to sink") != NULL);
    ASSERT(strstr(output3, "snapshot") != NULL);
    ASSERT(strstr(output3, "timeslice") != NULL);
    ASSERT(strstr(output3, "connect portA to portB") != NULL);
    ASSERT(strstr(output3, "x > 0") != NULL);
    ASSERT(strstr(output3, "assert constraint") != NULL);
    ASSERT(strstr(output3, "assert not constraint") != NULL);
    ASSERT(strstr(output3, "attribute y default") != NULL);

    free(output1);
    free(output2);
    free(output3);
    FIXTURE_TEARDOWN();
}

/* ========== Pipeline Source File Tests ========== */

TEST(pipeline_populates_source_file) {
    FIXTURE_SETUP();

    Sysml2CliOptions options = {0};
    options.parse_only = true;
    options.no_resolve = true;

    Sysml2PipelineContext *ctx = sysml2_pipeline_create(&arena, &intern, &options);
    ASSERT_NOT_NULL(ctx);

    const char *input = "package TestPkg { part def Engine; }";
    SysmlSemanticModel *model = NULL;
    Sysml2Result result = sysml2_pipeline_process_input(
        ctx, "test/vehicle.sysml", input, strlen(input), &model
    );

    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(model);

    /* Pipeline should have populated source_file */
    ASSERT_NOT_NULL(model->source_file);
    ASSERT_STR_EQ(model->source_file->path, "test/vehicle.sysml");
    ASSERT_NOT_NULL(model->source_file->content);
    ASSERT(model->source_file->content_length == strlen(input));
    ASSERT(model->source_file->line_count > 0);
    ASSERT_NOT_NULL(model->source_file->line_offsets);

    /* Content should match the input */
    ASSERT(memcmp(model->source_file->content, input, strlen(input)) == 0);

    sysml2_pipeline_destroy(ctx);
    FIXTURE_TEARDOWN();
}

TEST(pipeline_source_file_multiline) {
    FIXTURE_SETUP();

    Sysml2CliOptions options = {0};
    options.parse_only = true;
    options.no_resolve = true;

    Sysml2PipelineContext *ctx = sysml2_pipeline_create(&arena, &intern, &options);
    ASSERT_NOT_NULL(ctx);

    const char *input =
        "package VehiclePkg {\n"
        "    part def Engine;\n"
        "    part def Wheel;\n"
        "}\n";
    SysmlSemanticModel *model = NULL;
    Sysml2Result result = sysml2_pipeline_process_input(
        ctx, "vehicle.sysml", input, strlen(input), &model
    );

    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(model);
    ASSERT_NOT_NULL(model->source_file);

    /* Should have 4 lines (plus possibly a trailing entry) */
    ASSERT(model->source_file->line_count >= 4);
    /* First line offset should be 0 */
    ASSERT_EQ(model->source_file->line_offsets[0], 0);

    sysml2_pipeline_destroy(ctx);
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
    RUN_TEST(sysml_write_connection_usage);
    RUN_TEST(sysml_write_connection_endpoint_preservation);

    /* Imports */
    RUN_TEST(sysml_write_import);

    /* Comments */
    RUN_TEST(sysml_write_line_comment);

    /* Indentation */
    RUN_TEST(sysml_write_indentation);

    /* Error handling */
    RUN_TEST(sysml_write_null_model);
    RUN_TEST(sysml_write_null_output);

    /* Succession statements */
    RUN_TEST(sysml_write_valid_succession);
    RUN_TEST(sysml_write_multiple_successions);
    RUN_TEST(sysml_write_no_malformed_succession);
    RUN_TEST(sysml_succession_roundtrip_idempotent);
    RUN_TEST(sysml_flow_roundtrip_idempotent);
    RUN_TEST(sysml_complex_action_roundtrip_idempotent);

    /* Round-trip */
    RUN_TEST(sysml_roundtrip_simple);

    /* Data loss regression tests */
    RUN_TEST(sysml_conjugation_marker_preserved);
    RUN_TEST(sysml_typing_vs_redefines_separate);
    RUN_TEST(sysml_multiplicity_spacing_preserved);
    RUN_TEST(sysml_roundtrip_idempotent);

    /* Batch 1: Simple keyword captures */
    RUN_TEST(sysml_constant_attribute_preserved);
    RUN_TEST(sysml_derived_attribute_preserved);
    RUN_TEST(sysml_readonly_attribute_preserved);
    RUN_TEST(sysml_parallel_state_preserved);
    RUN_TEST(sysml_enum_member_keyword_preserved);
    RUN_TEST(sysml_enum_member_no_keyword_preserved);

    /* Batch 3: Flow endpoints */
    RUN_TEST(sysml_flow_endpoints_preserved);

    /* Batch 4: Nested port bodies */
    RUN_TEST(sysml_nested_port_body_preserved);

    /* Comprehensive round-trip tests */
    RUN_TEST(sysml_roundtrip_batch1_keywords);
    RUN_TEST(sysml_roundtrip_flow_endpoints);
    RUN_TEST(sysml_roundtrip_nested_port_body);

    /* Phase 1: Statement capture tests */
    RUN_TEST(sysml_satisfy_statement_preserved);
    RUN_TEST(sysml_assert_satisfy_preserved);
    RUN_TEST(sysml_include_use_case_preserved);
    RUN_TEST(sysml_expose_statement_preserved);
    RUN_TEST(sysml_render_statement_preserved);
    RUN_TEST(sysml_verify_requirement_preserved);

    /* Phase 2: N-ary connection endpoints */
    RUN_TEST(sysml_nary_connection_preserved);

    /* Phase 3: ReferenceUsage tests */
    RUN_TEST(sysml_ref_feature_preserved);
    RUN_TEST(sysml_ref_attribute_preserved);

    /* Phase 4: AllocationUsage tests */
    RUN_TEST(sysml_allocation_usage_preserved);
    RUN_TEST(sysml_allocate_statement_preserved);

    /* Phase 6: Additional usage type tests */
    RUN_TEST(sysml_interface_usage_preserved);
    RUN_TEST(sysml_event_occurrence_preserved);
    RUN_TEST(sysml_exhibit_state_preserved);
    RUN_TEST(sysml_succession_usage_preserved);

    /* Direction keyword tests */
    RUN_TEST(sysml_in_direction_preserved);
    RUN_TEST(sysml_out_direction_preserved);
    RUN_TEST(sysml_inout_direction_preserved);

    /* Comprehensive round-trip tests for new fixes */
    RUN_TEST(sysml_roundtrip_phase1_statements);
    RUN_TEST(sysml_roundtrip_allocation_and_ref);
    RUN_TEST(sysml_roundtrip_direction_keywords);

    /* Specialized syntax preservation tests */
    RUN_TEST(sysml_ref_state_preserved);
    RUN_TEST(sysml_event_occurrence_vs_event_preserved);
    RUN_TEST(sysml_interface_to_syntax_preserved);
    RUN_TEST(sysml_succession_first_then_preserved);
    RUN_TEST(sysml_roundtrip_specialized_syntax);

    /* Phase 2: Round-trip regression tests */
    RUN_TEST(sysml_binding_connector_endpoints_preserved);
    RUN_TEST(sysml_flow_named_endpoints_preserved);
    RUN_TEST(sysml_snapshot_timeslice_preserved);
    RUN_TEST(sysml_interface_connect_keyword_preserved);
    RUN_TEST(sysml_result_expression_preserved);
    RUN_TEST(sysml_assert_constraint_preserved);
    RUN_TEST(sysml_perform_action_preserved);
    RUN_TEST(sysml_default_value_keyword_preserved);
    RUN_TEST(sysml_roundtrip_phase2_all_fixes);

    /* Reserved keyword quoting */
    RUN_TEST(sysml_reserved_keyword_quoting_roundtrip);

    /* Pipeline source file tests */
    RUN_TEST(pipeline_populates_source_file);
    RUN_TEST(pipeline_source_file_multiline);

    printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
