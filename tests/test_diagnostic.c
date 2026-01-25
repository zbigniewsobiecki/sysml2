/*
 * SysML v2 Parser - Diagnostic System Tests
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/common.h"
#include "sysml2/arena.h"
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

/* Arena-only fixture */
#define FIXTURE_ARENA_SETUP() \
    Sysml2Arena arena; \
    sysml2_arena_init(&arena)

#define FIXTURE_ARENA_TEARDOWN() \
    sysml2_arena_destroy(&arena)

/* ========== Context Initialization Tests ========== */

TEST(diag_context_init) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    ASSERT_EQ(ctx.arena, &arena);
    ASSERT_NULL(ctx.first);
    ASSERT_NULL(ctx.last);
    ASSERT_EQ(ctx.error_count, 0);
    ASSERT_EQ(ctx.warning_count, 0);
    ASSERT_EQ(ctx.parse_error_count, 0);
    ASSERT_EQ(ctx.semantic_error_count, 0);
    ASSERT_EQ(ctx.max_errors, 20);  /* Default */
    ASSERT_FALSE(ctx.treat_warnings_as_errors);
    ASSERT_FALSE(ctx.has_fatal);

    FIXTURE_ARENA_TEARDOWN();
}

TEST(diag_set_max_errors) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    sysml2_diag_set_max_errors(&ctx, 100);
    ASSERT_EQ(ctx.max_errors, 100);

    sysml2_diag_set_max_errors(&ctx, 0);  /* Unlimited */
    ASSERT_EQ(ctx.max_errors, 0);

    FIXTURE_ARENA_TEARDOWN();
}

/* ========== Diagnostic Creation Tests ========== */

TEST(diag_create_error) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    Sysml2SourceRange range = {{1, 5, 4}, {1, 10, 9}};
    Sysml2Diagnostic *diag = sysml2_diag_create(
        &ctx,
        SYSML2_DIAG_E3001_UNDEFINED_TYPE,
        SYSML2_SEVERITY_ERROR,
        NULL,
        range,
        "undefined type 'Foo'"
    );

    ASSERT_NOT_NULL(diag);
    ASSERT_EQ(diag->code, SYSML2_DIAG_E3001_UNDEFINED_TYPE);
    ASSERT_EQ(diag->severity, SYSML2_SEVERITY_ERROR);
    ASSERT_STR_EQ(diag->message, "undefined type 'Foo'");
    ASSERT_EQ(diag->range.start.line, 1);
    ASSERT_EQ(diag->range.start.column, 5);
    ASSERT_EQ(diag->range.end.column, 10);
    ASSERT_NULL(diag->help);
    ASSERT_NULL(diag->fixits);
    ASSERT_EQ(diag->fixit_count, 0);
    ASSERT_NULL(diag->notes);

    FIXTURE_ARENA_TEARDOWN();
}

TEST(diag_create_warning) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    Sysml2SourceRange range = SYSML2_RANGE_INVALID;
    Sysml2Diagnostic *diag = sysml2_diag_create(
        &ctx,
        SYSML2_DIAG_W1001_UNUSED_IMPORT,
        SYSML2_SEVERITY_WARNING,
        NULL,
        range,
        "unused import 'ISQ'"
    );

    ASSERT_NOT_NULL(diag);
    ASSERT_EQ(diag->code, SYSML2_DIAG_W1001_UNUSED_IMPORT);
    ASSERT_EQ(diag->severity, SYSML2_SEVERITY_WARNING);
    ASSERT_STR_EQ(diag->message, "unused import 'ISQ'");

    FIXTURE_ARENA_TEARDOWN();
}

/* ========== Help and Fix-it Tests ========== */

TEST(diag_add_help) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    Sysml2Diagnostic *diag = sysml2_diag_create(
        &ctx,
        SYSML2_DIAG_E3001_UNDEFINED_TYPE,
        SYSML2_SEVERITY_ERROR,
        NULL,
        SYSML2_RANGE_INVALID,
        "undefined type"
    );

    sysml2_diag_add_help(diag, &ctx, "did you mean 'Integer'?");
    ASSERT_NOT_NULL(diag->help);
    ASSERT_STR_EQ(diag->help, "did you mean 'Integer'?");

    FIXTURE_ARENA_TEARDOWN();
}

TEST(diag_add_fixit) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    Sysml2Diagnostic *diag = sysml2_diag_create(
        &ctx,
        SYSML2_DIAG_E2001_EXPECTED_SEMICOLON,
        SYSML2_SEVERITY_ERROR,
        NULL,
        SYSML2_RANGE_INVALID,
        "expected ';'"
    );

    Sysml2SourceRange fix_range = {{1, 10, 9}, {1, 10, 9}};
    sysml2_diag_add_fixit(diag, &ctx, fix_range, ";");

    ASSERT_NOT_NULL(diag->fixits);
    ASSERT_EQ(diag->fixit_count, 1);
    ASSERT_STR_EQ(diag->fixits[0].replacement, ";");

    FIXTURE_ARENA_TEARDOWN();
}

TEST(diag_add_multiple_fixits) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    Sysml2Diagnostic *diag = sysml2_diag_create(
        &ctx,
        SYSML2_DIAG_E2006_UNEXPECTED_TOKEN,
        SYSML2_SEVERITY_ERROR,
        NULL,
        SYSML2_RANGE_INVALID,
        "unexpected token"
    );

    Sysml2SourceRange range = SYSML2_RANGE_INVALID;
    sysml2_diag_add_fixit(diag, &ctx, range, "option1");
    sysml2_diag_add_fixit(diag, &ctx, range, "option2");
    sysml2_diag_add_fixit(diag, &ctx, range, "option3");

    ASSERT_EQ(diag->fixit_count, 3);
    ASSERT_STR_EQ(diag->fixits[0].replacement, "option1");
    ASSERT_STR_EQ(diag->fixits[1].replacement, "option2");
    ASSERT_STR_EQ(diag->fixits[2].replacement, "option3");

    FIXTURE_ARENA_TEARDOWN();
}

/* ========== Note Tests ========== */

TEST(diag_add_note) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    Sysml2Diagnostic *diag = sysml2_diag_create(
        &ctx,
        SYSML2_DIAG_E3004_DUPLICATE_NAME,
        SYSML2_SEVERITY_ERROR,
        NULL,
        SYSML2_RANGE_INVALID,
        "duplicate definition of 'MyPart'"
    );

    Sysml2SourceRange note_range = {{5, 1, 50}, {5, 10, 59}};
    Sysml2Diagnostic *note = sysml2_diag_add_note(
        diag, &ctx, NULL, note_range, "previous definition was here"
    );

    ASSERT_NOT_NULL(note);
    ASSERT_NOT_NULL(diag->notes);
    ASSERT_EQ(diag->notes, note);
    ASSERT_EQ(note->severity, SYSML2_SEVERITY_NOTE);
    ASSERT_STR_EQ(note->message, "previous definition was here");

    FIXTURE_ARENA_TEARDOWN();
}

TEST(diag_add_multiple_notes) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    Sysml2Diagnostic *diag = sysml2_diag_create(
        &ctx,
        SYSML2_DIAG_E3005_CIRCULAR_SPECIALIZATION,
        SYSML2_SEVERITY_ERROR,
        NULL,
        SYSML2_RANGE_INVALID,
        "circular specialization detected"
    );

    sysml2_diag_add_note(diag, &ctx, NULL, SYSML2_RANGE_INVALID, "A specializes B");
    sysml2_diag_add_note(diag, &ctx, NULL, SYSML2_RANGE_INVALID, "B specializes C");
    sysml2_diag_add_note(diag, &ctx, NULL, SYSML2_RANGE_INVALID, "C specializes A");

    /* Count notes */
    int note_count = 0;
    for (Sysml2Diagnostic *n = diag->notes; n; n = n->next) {
        note_count++;
    }
    ASSERT_EQ(note_count, 3);

    FIXTURE_ARENA_TEARDOWN();
}

/* ========== Emit and Counting Tests ========== */

TEST(diag_emit_error_counting) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    Sysml2Diagnostic *diag = sysml2_diag_create(
        &ctx,
        SYSML2_DIAG_E3001_UNDEFINED_TYPE,
        SYSML2_SEVERITY_ERROR,
        NULL,
        SYSML2_RANGE_INVALID,
        "test error"
    );

    ASSERT_EQ(ctx.error_count, 0);
    sysml2_diag_emit(&ctx, diag);
    ASSERT_EQ(ctx.error_count, 1);
    ASSERT_EQ(ctx.semantic_error_count, 1);

    FIXTURE_ARENA_TEARDOWN();
}

TEST(diag_emit_warning_counting) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    Sysml2Diagnostic *diag = sysml2_diag_create(
        &ctx,
        SYSML2_DIAG_W1001_UNUSED_IMPORT,
        SYSML2_SEVERITY_WARNING,
        NULL,
        SYSML2_RANGE_INVALID,
        "test warning"
    );

    sysml2_diag_emit(&ctx, diag);
    ASSERT_EQ(ctx.warning_count, 1);
    ASSERT_EQ(ctx.error_count, 0);

    FIXTURE_ARENA_TEARDOWN();
}

TEST(diag_emit_fatal_sets_flag) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    Sysml2Diagnostic *diag = sysml2_diag_create(
        &ctx,
        SYSML2_DIAG_E1001_INVALID_CHAR,
        SYSML2_SEVERITY_FATAL,
        NULL,
        SYSML2_RANGE_INVALID,
        "fatal error"
    );

    ASSERT_FALSE(ctx.has_fatal);
    sysml2_diag_emit(&ctx, diag);
    ASSERT_TRUE(ctx.has_fatal);
    ASSERT_EQ(ctx.error_count, 1);

    FIXTURE_ARENA_TEARDOWN();
}

TEST(diag_treat_warnings_as_errors) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);
    ctx.treat_warnings_as_errors = true;

    Sysml2Diagnostic *diag = sysml2_diag_create(
        &ctx,
        SYSML2_DIAG_W1001_UNUSED_IMPORT,
        SYSML2_SEVERITY_WARNING,
        NULL,
        SYSML2_RANGE_INVALID,
        "test warning"
    );

    sysml2_diag_emit(&ctx, diag);
    /* Should be counted as error */
    ASSERT_EQ(ctx.error_count, 1);
    ASSERT_EQ(ctx.warning_count, 0);
    ASSERT_EQ(diag->severity, SYSML2_SEVERITY_ERROR);

    FIXTURE_ARENA_TEARDOWN();
}

TEST(diag_parse_error_counting) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    /* E1xxx (lexical error) */
    Sysml2Diagnostic *d1 = sysml2_diag_create(
        &ctx, SYSML2_DIAG_E1001_INVALID_CHAR, SYSML2_SEVERITY_ERROR,
        NULL, SYSML2_RANGE_INVALID, "invalid char"
    );
    sysml2_diag_emit(&ctx, d1);

    /* E2xxx (syntax error) */
    Sysml2Diagnostic *d2 = sysml2_diag_create(
        &ctx, SYSML2_DIAG_E2001_EXPECTED_SEMICOLON, SYSML2_SEVERITY_ERROR,
        NULL, SYSML2_RANGE_INVALID, "expected semicolon"
    );
    sysml2_diag_emit(&ctx, d2);

    ASSERT_EQ(ctx.parse_error_count, 2);
    ASSERT_EQ(ctx.semantic_error_count, 0);
    ASSERT_EQ(ctx.error_count, 2);

    FIXTURE_ARENA_TEARDOWN();
}

/* ========== Should Stop Tests ========== */

TEST(diag_should_stop_after_fatal) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    ASSERT_FALSE(sysml2_diag_should_stop(&ctx));

    ctx.has_fatal = true;
    ASSERT_TRUE(sysml2_diag_should_stop(&ctx));

    FIXTURE_ARENA_TEARDOWN();
}

TEST(diag_should_stop_after_max_errors) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);
    sysml2_diag_set_max_errors(&ctx, 5);

    ctx.error_count = 4;
    ASSERT_FALSE(sysml2_diag_should_stop(&ctx));

    ctx.error_count = 5;
    ASSERT_TRUE(sysml2_diag_should_stop(&ctx));

    FIXTURE_ARENA_TEARDOWN();
}

TEST(diag_unlimited_errors) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);
    sysml2_diag_set_max_errors(&ctx, 0);  /* Unlimited */

    ctx.error_count = 1000;
    ASSERT_FALSE(sysml2_diag_should_stop(&ctx));

    FIXTURE_ARENA_TEARDOWN();
}

/* ========== Has Errors Tests ========== */

TEST(diag_has_parse_errors) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    ASSERT_FALSE(sysml2_diag_has_parse_errors(&ctx));

    ctx.parse_error_count = 1;
    ASSERT_TRUE(sysml2_diag_has_parse_errors(&ctx));

    FIXTURE_ARENA_TEARDOWN();
}

TEST(diag_has_semantic_errors) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    ASSERT_FALSE(sysml2_diag_has_semantic_errors(&ctx));

    ctx.semantic_error_count = 1;
    ASSERT_TRUE(sysml2_diag_has_semantic_errors(&ctx));

    FIXTURE_ARENA_TEARDOWN();
}

/* ========== Severity String Tests ========== */

TEST(severity_to_string) {
    ASSERT_STR_EQ(sysml2_severity_to_string(SYSML2_SEVERITY_NOTE), "note");
    ASSERT_STR_EQ(sysml2_severity_to_string(SYSML2_SEVERITY_WARNING), "warning");
    ASSERT_STR_EQ(sysml2_severity_to_string(SYSML2_SEVERITY_ERROR), "error");
    ASSERT_STR_EQ(sysml2_severity_to_string(SYSML2_SEVERITY_FATAL), "fatal error");
}

/* ========== Diagnostic Code String Tests ========== */

TEST(diag_code_to_string) {
    ASSERT_STR_EQ(sysml2_diag_code_to_string(SYSML2_DIAG_E1001_INVALID_CHAR), "E1001");
    ASSERT_STR_EQ(sysml2_diag_code_to_string(SYSML2_DIAG_E2001_EXPECTED_SEMICOLON), "E2001");
    ASSERT_STR_EQ(sysml2_diag_code_to_string(SYSML2_DIAG_E3001_UNDEFINED_TYPE), "E3001");
    ASSERT_STR_EQ(sysml2_diag_code_to_string(SYSML2_DIAG_W1001_UNUSED_IMPORT), "W1001");
}

/* ========== Color Mode Tests ========== */

TEST(should_use_color) {
    /* ALWAYS mode should always return true */
    ASSERT_TRUE(sysml2_should_use_color(SYSML2_COLOR_ALWAYS, stdout));
    ASSERT_TRUE(sysml2_should_use_color(SYSML2_COLOR_ALWAYS, stderr));

    /* NEVER mode should always return false */
    ASSERT_FALSE(sysml2_should_use_color(SYSML2_COLOR_NEVER, stdout));
    ASSERT_FALSE(sysml2_should_use_color(SYSML2_COLOR_NEVER, stderr));

    /* AUTO mode depends on isatty - we can't easily test this without mocking */
}

/* ========== Diagnostic List Tests ========== */

TEST(diag_list_ordering) {
    FIXTURE_ARENA_SETUP();

    Sysml2DiagContext ctx;
    sysml2_diag_context_init(&ctx, &arena);

    /* Emit multiple diagnostics */
    Sysml2Diagnostic *d1 = sysml2_diag_create(
        &ctx, SYSML2_DIAG_E3001_UNDEFINED_TYPE, SYSML2_SEVERITY_ERROR,
        NULL, SYSML2_RANGE_INVALID, "first"
    );
    sysml2_diag_emit(&ctx, d1);

    Sysml2Diagnostic *d2 = sysml2_diag_create(
        &ctx, SYSML2_DIAG_E3002_UNDEFINED_FEATURE, SYSML2_SEVERITY_ERROR,
        NULL, SYSML2_RANGE_INVALID, "second"
    );
    sysml2_diag_emit(&ctx, d2);

    Sysml2Diagnostic *d3 = sysml2_diag_create(
        &ctx, SYSML2_DIAG_W1001_UNUSED_IMPORT, SYSML2_SEVERITY_WARNING,
        NULL, SYSML2_RANGE_INVALID, "third"
    );
    sysml2_diag_emit(&ctx, d3);

    /* Check order is preserved */
    ASSERT_EQ(ctx.first, d1);
    ASSERT_EQ(ctx.first->next, d2);
    ASSERT_EQ(ctx.first->next->next, d3);
    ASSERT_EQ(ctx.last, d3);

    FIXTURE_ARENA_TEARDOWN();
}

/* ========== Main ========== */

int main(void) {
    printf("Running diagnostic tests...\n");

    /* Context initialization */
    RUN_TEST(diag_context_init);
    RUN_TEST(diag_set_max_errors);

    /* Diagnostic creation */
    RUN_TEST(diag_create_error);
    RUN_TEST(diag_create_warning);

    /* Help and fix-its */
    RUN_TEST(diag_add_help);
    RUN_TEST(diag_add_fixit);
    RUN_TEST(diag_add_multiple_fixits);

    /* Notes */
    RUN_TEST(diag_add_note);
    RUN_TEST(diag_add_multiple_notes);

    /* Emit and counting */
    RUN_TEST(diag_emit_error_counting);
    RUN_TEST(diag_emit_warning_counting);
    RUN_TEST(diag_emit_fatal_sets_flag);
    RUN_TEST(diag_treat_warnings_as_errors);
    RUN_TEST(diag_parse_error_counting);

    /* Should stop */
    RUN_TEST(diag_should_stop_after_fatal);
    RUN_TEST(diag_should_stop_after_max_errors);
    RUN_TEST(diag_unlimited_errors);

    /* Has errors */
    RUN_TEST(diag_has_parse_errors);
    RUN_TEST(diag_has_semantic_errors);

    /* String conversions */
    RUN_TEST(severity_to_string);
    RUN_TEST(diag_code_to_string);

    /* Color mode */
    RUN_TEST(should_use_color);

    /* List ordering */
    RUN_TEST(diag_list_ordering);

    printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
