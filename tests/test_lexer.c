/*
 * SysML v2 Parser - Lexer Tests
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/common.h"
#include "sysml2/arena.h"
#include "sysml2/intern.h"
#include "sysml2/lexer.h"
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
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/* Helper to create lexer from string */
static Sysml2Lexer create_lexer(
    const char *source,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    Sysml2DiagContext *diag
) {
    static Sysml2SourceFile file;
    static uint32_t line_offsets[1] = {0};

    file.path = "test.kerml";
    file.content = source;
    file.content_length = strlen(source);
    file.line_offsets = line_offsets;
    file.line_count = 1;

    Sysml2Lexer lexer;
    sysml2_lexer_init(&lexer, &file, intern, diag);
    return lexer;
}

TEST(empty_input) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2Lexer lexer = create_lexer("", &arena, &intern, &diag);
    Sysml2Token token = sysml2_lexer_next(&lexer);

    ASSERT_EQ(token.type, SYSML2_TOKEN_EOF);

    sysml2_arena_destroy(&arena);
}

TEST(single_keyword) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2Lexer lexer = create_lexer("package", &arena, &intern, &diag);
    Sysml2Token token = sysml2_lexer_next(&lexer);

    ASSERT_EQ(token.type, SYSML2_TOKEN_KW_PACKAGE);
    ASSERT_EQ(token.text.length, 7);

    token = sysml2_lexer_next(&lexer);
    ASSERT_EQ(token.type, SYSML2_TOKEN_EOF);

    sysml2_arena_destroy(&arena);
}

TEST(identifier) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2Lexer lexer = create_lexer("myIdentifier", &arena, &intern, &diag);
    Sysml2Token token = sysml2_lexer_next(&lexer);

    ASSERT_EQ(token.type, SYSML2_TOKEN_IDENTIFIER);
    ASSERT_EQ(token.text.length, 12);

    sysml2_arena_destroy(&arena);
}

TEST(unrestricted_name) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2Lexer lexer = create_lexer("'My Complex Name'", &arena, &intern, &diag);
    Sysml2Token token = sysml2_lexer_next(&lexer);

    ASSERT_EQ(token.type, SYSML2_TOKEN_UNRESTRICTED_NAME);
    ASSERT_EQ(token.text.length, 17);

    sysml2_arena_destroy(&arena);
}

TEST(integer_literal) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2Lexer lexer = create_lexer("42", &arena, &intern, &diag);
    Sysml2Token token = sysml2_lexer_next(&lexer);

    ASSERT_EQ(token.type, SYSML2_TOKEN_INTEGER);
    ASSERT_EQ(token.text.length, 2);

    sysml2_arena_destroy(&arena);
}

TEST(real_literal) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2Lexer lexer = create_lexer("3.14159", &arena, &intern, &diag);
    Sysml2Token token = sysml2_lexer_next(&lexer);

    ASSERT_EQ(token.type, SYSML2_TOKEN_REAL);
    ASSERT_EQ(token.text.length, 7);

    sysml2_arena_destroy(&arena);
}

TEST(string_literal) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2Lexer lexer = create_lexer("\"hello world\"", &arena, &intern, &diag);
    Sysml2Token token = sysml2_lexer_next(&lexer);

    ASSERT_EQ(token.type, SYSML2_TOKEN_STRING);
    ASSERT_EQ(token.text.length, 13);

    sysml2_arena_destroy(&arena);
}

TEST(operators) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2Lexer lexer = create_lexer(": :> :>> ::>", &arena, &intern, &diag);

    Sysml2Token token = sysml2_lexer_next(&lexer);
    ASSERT_EQ(token.type, SYSML2_TOKEN_COLON);

    token = sysml2_lexer_next(&lexer);
    ASSERT_EQ(token.type, SYSML2_TOKEN_COLON_GT);

    token = sysml2_lexer_next(&lexer);
    ASSERT_EQ(token.type, SYSML2_TOKEN_COLON_GT_GT);

    token = sysml2_lexer_next(&lexer);
    ASSERT_EQ(token.type, SYSML2_TOKEN_COLON_COLON_GT);

    sysml2_arena_destroy(&arena);
}

TEST(comments) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2Lexer lexer = create_lexer(
        "package // line comment\n"
        "Test /* block comment */ {",
        &arena, &intern, &diag
    );

    Sysml2Token token = sysml2_lexer_next(&lexer);
    ASSERT_EQ(token.type, SYSML2_TOKEN_KW_PACKAGE);

    token = sysml2_lexer_next(&lexer);
    ASSERT_EQ(token.type, SYSML2_TOKEN_IDENTIFIER);

    token = sysml2_lexer_next(&lexer);
    ASSERT_EQ(token.type, SYSML2_TOKEN_LBRACE);

    sysml2_arena_destroy(&arena);
}

TEST(full_declaration) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2DiagContext diag;
    sysml2_diag_context_init(&diag, &arena);

    Sysml2Lexer lexer = create_lexer(
        "class Vehicle :> Object {\n"
        "    feature engine : Engine;\n"
        "}",
        &arena, &intern, &diag
    );

    Sysml2Token expected[] = {
        {SYSML2_TOKEN_KW_CLASS},
        {SYSML2_TOKEN_IDENTIFIER},       /* Vehicle */
        {SYSML2_TOKEN_COLON_GT},         /* :> */
        {SYSML2_TOKEN_IDENTIFIER},       /* Object */
        {SYSML2_TOKEN_LBRACE},           /* { */
        {SYSML2_TOKEN_KW_FEATURE},
        {SYSML2_TOKEN_IDENTIFIER},       /* engine */
        {SYSML2_TOKEN_COLON},            /* : */
        {SYSML2_TOKEN_IDENTIFIER},       /* Engine */
        {SYSML2_TOKEN_SEMICOLON},        /* ; */
        {SYSML2_TOKEN_RBRACE},           /* } */
        {SYSML2_TOKEN_EOF},
    };

    for (size_t i = 0; i < sizeof(expected)/sizeof(expected[0]); i++) {
        Sysml2Token token = sysml2_lexer_next(&lexer);
        ASSERT_EQ(token.type, expected[i].type);
    }

    sysml2_arena_destroy(&arena);
}

int main(void) {
    printf("Running lexer tests:\n");

    RUN_TEST(empty_input);
    RUN_TEST(single_keyword);
    RUN_TEST(identifier);
    RUN_TEST(unrestricted_name);
    RUN_TEST(integer_literal);
    RUN_TEST(real_literal);
    RUN_TEST(string_literal);
    RUN_TEST(operators);
    RUN_TEST(comments);
    RUN_TEST(full_declaration);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
