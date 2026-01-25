/*
 * SysML v2 Parser - JSON Writer Tests
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/common.h"
#include "sysml2/arena.h"
#include "sysml2/intern.h"
#include "sysml2/ast.h"
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

/* Helper: Create a simple model for testing */
static SysmlSemanticModel *create_test_model(
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    const char *source_name
) {
    SysmlSemanticModel *model = SYSML2_ARENA_NEW(arena, SysmlSemanticModel);
    model->source_name = sysml2_intern(intern, source_name);
    model->elements = NULL;
    model->element_count = 0;
    model->element_capacity = 0;
    model->relationships = NULL;
    model->relationship_count = 0;
    model->relationship_capacity = 0;
    return model;
}

/* ========== String Escaping Tests ========== */

TEST(json_escape_string_basic) {
    char buf[256];
    size_t len;

    /* Simple string */
    len = sysml2_json_escape_string("hello", buf, sizeof(buf));
    ASSERT_EQ(len, 5);
    ASSERT_STR_EQ(buf, "hello");

    /* Empty string */
    len = sysml2_json_escape_string("", buf, sizeof(buf));
    ASSERT_EQ(len, 0);
    ASSERT_STR_EQ(buf, "");
}

TEST(json_escape_string_quotes) {
    char buf[256];
    size_t len;

    len = sysml2_json_escape_string("say \"hello\"", buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "say \\\"hello\\\"");
    ASSERT_EQ(len, 13);
}

TEST(json_escape_string_backslash) {
    char buf[256];
    size_t len;

    len = sysml2_json_escape_string("path\\to\\file", buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "path\\\\to\\\\file");
    ASSERT_EQ(len, 14);
}

TEST(json_escape_string_newlines) {
    char buf[256];
    size_t len;

    len = sysml2_json_escape_string("line1\nline2\rline3", buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "line1\\nline2\\rline3");
    ASSERT_EQ(len, 19);
}

TEST(json_escape_string_tabs) {
    char buf[256];
    size_t len;

    len = sysml2_json_escape_string("col1\tcol2", buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "col1\\tcol2");
    ASSERT_EQ(len, 10);
}

TEST(json_escape_string_control_chars) {
    char buf[256];

    /* Bell character (0x07) should be escaped as \u0007 */
    /* Use explicit assignment to avoid shell escape issues */
    char input[10];
    input[0] = 't';
    input[1] = 'e';
    input[2] = 's';
    input[3] = 't';
    input[4] = 7;  /* Bell character */
    input[5] = 'e';
    input[6] = 'n';
    input[7] = 'd';
    input[8] = '\0';

    sysml2_json_escape_string(input, buf, sizeof(buf));
    /* Control character should be escaped as \uXXXX */
    ASSERT(strstr(buf, "\\u0007") != NULL);
    /* And verify start/end are present */
    ASSERT(strstr(buf, "test") != NULL);
    ASSERT(strstr(buf, "end") != NULL);
}

TEST(json_escape_string_null_input) {
    char buf[256];
    size_t len;

    len = sysml2_json_escape_string(NULL, buf, sizeof(buf));
    ASSERT_EQ(len, 0);
}

TEST(json_escape_string_small_buffer) {
    char buf[8];

    /* Buffer too small to fit full escaped string */
    sysml2_json_escape_string("hello world", buf, sizeof(buf));
    /* Should not overflow - just truncate */
    ASSERT_EQ(strlen(buf), 7);
}

/* ========== Model Serialization Tests ========== */

TEST(json_write_empty_model) {
    FIXTURE_SETUP();

    SysmlSemanticModel *model = create_test_model(&arena, &intern, "empty.sysml");

    char *output = NULL;
    Sysml2Result result = sysml2_json_write_string(model, NULL, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Check structure */
    ASSERT(strstr(output, "\"meta\"") != NULL);
    ASSERT(strstr(output, "\"elements\"") != NULL);
    ASSERT(strstr(output, "\"relationships\"") != NULL);
    ASSERT(strstr(output, "\"version\"") != NULL);
    ASSERT(strstr(output, "\"empty.sysml\"") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

TEST(json_write_single_element) {
    FIXTURE_SETUP();

    SysmlSemanticModel *model = create_test_model(&arena, &intern, "test.sysml");

    /* Add one element */
    SysmlNode *node = SYSML2_ARENA_NEW(&arena, SysmlNode);
    node->id = sysml2_intern(&intern, "Pkg::MyPart");
    node->name = sysml2_intern(&intern, "MyPart");
    node->kind = SYSML_KIND_PART_DEF;
    node->parent_id = sysml2_intern(&intern, "Pkg");

    model->elements = SYSML2_ARENA_NEW_ARRAY(&arena, SysmlNode *, 1);
    model->elements[0] = node;
    model->element_count = 1;

    char *output = NULL;
    Sysml2Result result = sysml2_json_write_string(model, NULL, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Check element is present */
    ASSERT(strstr(output, "\"Pkg::MyPart\"") != NULL);
    ASSERT(strstr(output, "\"MyPart\"") != NULL);
    ASSERT(strstr(output, "\"parent\"") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

TEST(json_write_with_relationships) {
    FIXTURE_SETUP();

    SysmlSemanticModel *model = create_test_model(&arena, &intern, "test.sysml");

    /* Add elements */
    SysmlNode *node1 = SYSML2_ARENA_NEW(&arena, SysmlNode);
    node1->id = sysml2_intern(&intern, "A");
    node1->name = sysml2_intern(&intern, "A");
    node1->kind = SYSML_KIND_PART_DEF;

    SysmlNode *node2 = SYSML2_ARENA_NEW(&arena, SysmlNode);
    node2->id = sysml2_intern(&intern, "B");
    node2->name = sysml2_intern(&intern, "B");
    node2->kind = SYSML_KIND_PART_DEF;

    model->elements = SYSML2_ARENA_NEW_ARRAY(&arena, SysmlNode *, 2);
    model->elements[0] = node1;
    model->elements[1] = node2;
    model->element_count = 2;

    /* Add relationship */
    SysmlRelationship *rel = SYSML2_ARENA_NEW(&arena, SysmlRelationship);
    rel->id = sysml2_intern(&intern, "rel1");
    rel->kind = SYSML_KIND_REL_SPECIALIZATION;
    rel->source = sysml2_intern(&intern, "A");
    rel->target = sysml2_intern(&intern, "B");

    model->relationships = SYSML2_ARENA_NEW_ARRAY(&arena, SysmlRelationship *, 1);
    model->relationships[0] = rel;
    model->relationship_count = 1;

    char *output = NULL;
    Sysml2Result result = sysml2_json_write_string(model, NULL, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Check relationship is present */
    ASSERT(strstr(output, "\"source\"") != NULL);
    ASSERT(strstr(output, "\"target\"") != NULL);
    ASSERT(strstr(output, "\"rel1\"") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Options Tests ========== */

TEST(json_write_compact) {
    FIXTURE_SETUP();

    SysmlSemanticModel *model = create_test_model(&arena, &intern, "test.sysml");

    Sysml2JsonOptions options = {
        .pretty = false,
        .indent_size = 0,
        .include_source = true
    };

    char *output = NULL;
    Sysml2Result result = sysml2_json_write_string(model, &options, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Compact output should not have leading spaces for indentation */
    /* But it's still valid JSON */
    ASSERT(strstr(output, "\"meta\"") != NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

TEST(json_write_no_source) {
    FIXTURE_SETUP();

    SysmlSemanticModel *model = create_test_model(&arena, &intern, "test.sysml");

    Sysml2JsonOptions options = {
        .pretty = true,
        .indent_size = 2,
        .include_source = false
    };

    char *output = NULL;
    Sysml2Result result = sysml2_json_write_string(model, &options, &output);
    ASSERT_EQ(result, SYSML2_OK);
    ASSERT_NOT_NULL(output);

    /* Source should not be in meta */
    ASSERT(strstr(output, "\"test.sysml\"") == NULL);

    free(output);
    FIXTURE_TEARDOWN();
}

/* ========== Error Handling Tests ========== */

TEST(json_write_null_model) {
    Sysml2Result result = sysml2_json_write(NULL, stdout, NULL);
    ASSERT_EQ(result, SYSML2_ERROR_SYNTAX);
}

TEST(json_write_null_output) {
    FIXTURE_SETUP();

    SysmlSemanticModel *model = create_test_model(&arena, &intern, "test.sysml");

    Sysml2Result result = sysml2_json_write(model, NULL, NULL);
    ASSERT_EQ(result, SYSML2_ERROR_SYNTAX);

    FIXTURE_TEARDOWN();
}

TEST(json_write_string_null_model) {
    char *output = NULL;
    Sysml2Result result = sysml2_json_write_string(NULL, NULL, &output);
    ASSERT_EQ(result, SYSML2_ERROR_SYNTAX);
    ASSERT_NULL(output);
}

TEST(json_write_string_null_out) {
    FIXTURE_SETUP();

    SysmlSemanticModel *model = create_test_model(&arena, &intern, "test.sysml");

    Sysml2Result result = sysml2_json_write_string(model, NULL, NULL);
    ASSERT_EQ(result, SYSML2_ERROR_SYNTAX);

    FIXTURE_TEARDOWN();
}

/* ========== Main ========== */

int main(void) {
    printf("Running JSON writer tests...\n");

    /* String escaping */
    RUN_TEST(json_escape_string_basic);
    RUN_TEST(json_escape_string_quotes);
    RUN_TEST(json_escape_string_backslash);
    RUN_TEST(json_escape_string_newlines);
    RUN_TEST(json_escape_string_tabs);
    RUN_TEST(json_escape_string_control_chars);
    RUN_TEST(json_escape_string_null_input);
    RUN_TEST(json_escape_string_small_buffer);

    /* Model serialization */
    RUN_TEST(json_write_empty_model);
    RUN_TEST(json_write_single_element);
    RUN_TEST(json_write_with_relationships);

    /* Options */
    RUN_TEST(json_write_compact);
    RUN_TEST(json_write_no_source);

    /* Error handling */
    RUN_TEST(json_write_null_model);
    RUN_TEST(json_write_null_output);
    RUN_TEST(json_write_string_null_model);
    RUN_TEST(json_write_string_null_out);

    printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
