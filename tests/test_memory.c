/*
 * SysML v2 Parser - Memory (Arena/Intern) Tests
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/common.h"
#include "sysml2/arena.h"
#include "sysml2/intern.h"

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

/* ========== Arena Basic Tests ========== */

TEST(arena_init_default) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    ASSERT_EQ(arena.block_size, SYSML2_ARENA_DEFAULT_BLOCK_SIZE);
    ASSERT_EQ(arena.total_allocated, 0);
    /* No blocks allocated until first allocation */

    sysml2_arena_destroy(&arena);
}

TEST(arena_init_with_size) {
    Sysml2Arena arena;
    size_t custom_size = 1024;
    sysml2_arena_init_with_size(&arena, custom_size);

    ASSERT_EQ(arena.block_size, custom_size);

    sysml2_arena_destroy(&arena);
}

TEST(arena_alloc_basic) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    void *p = sysml2_arena_alloc(&arena, 100);
    ASSERT_NOT_NULL(p);

    /* Should track allocation */
    ASSERT(sysml2_arena_used(&arena) >= 100);

    sysml2_arena_destroy(&arena);
}

TEST(arena_alloc_multiple) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    void *p1 = sysml2_arena_alloc(&arena, 50);
    void *p2 = sysml2_arena_alloc(&arena, 50);
    void *p3 = sysml2_arena_alloc(&arena, 50);

    ASSERT_NOT_NULL(p1);
    ASSERT_NOT_NULL(p2);
    ASSERT_NOT_NULL(p3);

    /* Pointers should be different */
    ASSERT(p1 != p2);
    ASSERT(p2 != p3);
    ASSERT(p1 != p3);

    sysml2_arena_destroy(&arena);
}

TEST(arena_calloc) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    /* Allocate and zero-initialize */
    int *arr = sysml2_arena_calloc(&arena, 10, sizeof(int));
    ASSERT_NOT_NULL(arr);

    /* All elements should be zero */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(arr[i], 0);
    }

    sysml2_arena_destroy(&arena);
}

TEST(arena_alloc_aligned) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    /* Allocate with default alignment (sizeof(void*)) */
    void *p = sysml2_arena_alloc_aligned(&arena, 100, sizeof(void *));
    ASSERT_NOT_NULL(p);

    /* Check default alignment */
    ASSERT_EQ((uintptr_t)p % sizeof(void *), 0);

    /* Allocate with 8-byte alignment (guaranteed on all platforms) */
    void *p2 = sysml2_arena_alloc_aligned(&arena, 100, 8);
    ASSERT_NOT_NULL(p2);
    ASSERT_EQ((uintptr_t)p2 % 8, 0);

    /* Multiple allocations should all return non-NULL */
    void *p3 = sysml2_arena_alloc_aligned(&arena, 50, sizeof(void *));
    ASSERT_NOT_NULL(p3);

    sysml2_arena_destroy(&arena);
}

TEST(arena_strdup) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    const char *original = "Hello, World!";
    char *dup = sysml2_arena_strdup(&arena, original);

    ASSERT_NOT_NULL(dup);
    ASSERT_STR_EQ(dup, original);
    ASSERT(dup != original);  /* Should be a copy */

    sysml2_arena_destroy(&arena);
}

TEST(arena_strdup_null) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    char *dup = sysml2_arena_strdup(&arena, NULL);
    ASSERT_NULL(dup);

    sysml2_arena_destroy(&arena);
}

TEST(arena_strndup) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    const char *original = "Hello, World!";
    char *dup = sysml2_arena_strndup(&arena, original, 5);

    ASSERT_NOT_NULL(dup);
    ASSERT_STR_EQ(dup, "Hello");

    sysml2_arena_destroy(&arena);
}

TEST(arena_reset) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    /* Allocate some memory */
    sysml2_arena_alloc(&arena, 1000);
    sysml2_arena_alloc(&arena, 1000);

    size_t used_before = sysml2_arena_used(&arena);
    ASSERT(used_before >= 2000);

    /* Reset arena */
    sysml2_arena_reset(&arena);

    /* Should be able to allocate again from the beginning */
    void *p = sysml2_arena_alloc(&arena, 100);
    ASSERT_NOT_NULL(p);

    sysml2_arena_destroy(&arena);
}

TEST(arena_large_allocation) {
    Sysml2Arena arena;
    sysml2_arena_init_with_size(&arena, 1024);  /* Small blocks */

    /* Allocate larger than block size */
    void *p = sysml2_arena_alloc(&arena, 2048);
    ASSERT_NOT_NULL(p);

    /* Should still work for additional allocations */
    void *p2 = sysml2_arena_alloc(&arena, 100);
    ASSERT_NOT_NULL(p2);

    sysml2_arena_destroy(&arena);
}

TEST(arena_many_allocations) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    /* Many small allocations */
    for (int i = 0; i < 10000; i++) {
        void *p = sysml2_arena_alloc(&arena, 64);
        ASSERT_NOT_NULL(p);
    }

    sysml2_arena_destroy(&arena);
}

/* ========== Intern Basic Tests ========== */

TEST(intern_init) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    ASSERT_EQ(sysml2_intern_count(&intern), 0);

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(intern_basic) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    const char *str1 = sysml2_intern(&intern, "hello");
    ASSERT_NOT_NULL(str1);
    ASSERT_STR_EQ(str1, "hello");
    ASSERT_EQ(sysml2_intern_count(&intern), 1);

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(intern_deduplication) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Intern same string twice */
    const char *str1 = sysml2_intern(&intern, "hello");
    const char *str2 = sysml2_intern(&intern, "hello");

    /* Should return same pointer */
    ASSERT_EQ(str1, str2);
    ASSERT_EQ(sysml2_intern_count(&intern), 1);

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(intern_different_strings) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    const char *str1 = sysml2_intern(&intern, "hello");
    const char *str2 = sysml2_intern(&intern, "world");

    /* Should be different pointers */
    ASSERT(str1 != str2);
    ASSERT_EQ(sysml2_intern_count(&intern), 2);

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(intern_n) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Intern with explicit length */
    const char *str = sysml2_intern_n(&intern, "hello world", 5);
    ASSERT_NOT_NULL(str);
    ASSERT_STR_EQ(str, "hello");

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(intern_sv) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    Sysml2StringView sv = {.data = "test string", .length = 4};
    const char *str = sysml2_intern_sv(&intern, sv);

    ASSERT_NOT_NULL(str);
    ASSERT_STR_EQ(str, "test");

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(intern_lookup) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Intern a string */
    const char *interned = sysml2_intern(&intern, "findme");

    /* Lookup should return same pointer */
    const char *found = sysml2_intern_lookup(&intern, "findme");
    ASSERT_EQ(found, interned);

    /* Lookup non-existent should return NULL */
    const char *not_found = sysml2_intern_lookup(&intern, "nothere");
    ASSERT_NULL(not_found);

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(intern_many_strings) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    Sysml2Intern intern;
    sysml2_intern_init(&intern, &arena);

    /* Intern many unique strings */
    for (int i = 0; i < 1000; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "string_%d", i);
        const char *s = sysml2_intern(&intern, buf);
        ASSERT_NOT_NULL(s);
    }

    ASSERT_EQ(sysml2_intern_count(&intern), 1000);

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

TEST(intern_hash_collision) {
    Sysml2Arena arena;
    sysml2_arena_init(&arena);

    /* Use small capacity to force collisions */
    Sysml2Intern intern;
    sysml2_intern_init_with_capacity(&intern, &arena, 4);

    /* Intern many strings to guarantee collisions */
    const char *strings[20];
    for (int i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "str%d", i);
        strings[i] = sysml2_intern(&intern, buf);
        ASSERT_NOT_NULL(strings[i]);
    }

    /* All should be retrievable */
    for (int i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "str%d", i);
        const char *found = sysml2_intern(&intern, buf);
        ASSERT_EQ(found, strings[i]);
    }

    ASSERT_EQ(sysml2_intern_count(&intern), 20);

    sysml2_intern_destroy(&intern);
    sysml2_arena_destroy(&arena);
}

/* ========== Hash Function Tests ========== */

TEST(hash_string_basic) {
    /* Same string should produce same hash */
    uint32_t h1 = sysml2_hash_string("hello", 5);
    uint32_t h2 = sysml2_hash_string("hello", 5);
    ASSERT_EQ(h1, h2);

    /* Different strings should (usually) produce different hashes */
    uint32_t h3 = sysml2_hash_string("world", 5);
    ASSERT(h1 != h3);
}

TEST(hash_string_empty) {
    /* Empty string should not crash */
    uint32_t h = sysml2_hash_string("", 0);
    /* Just check it returns something */
    (void)h;
}

/* ========== Main ========== */

int main(void) {
    printf("Running memory tests...\n");

    /* Arena basic tests */
    RUN_TEST(arena_init_default);
    RUN_TEST(arena_init_with_size);
    RUN_TEST(arena_alloc_basic);
    RUN_TEST(arena_alloc_multiple);
    RUN_TEST(arena_calloc);
    RUN_TEST(arena_alloc_aligned);
    RUN_TEST(arena_strdup);
    RUN_TEST(arena_strdup_null);
    RUN_TEST(arena_strndup);
    RUN_TEST(arena_reset);
    RUN_TEST(arena_large_allocation);
    RUN_TEST(arena_many_allocations);

    /* Intern tests */
    RUN_TEST(intern_init);
    RUN_TEST(intern_basic);
    RUN_TEST(intern_deduplication);
    RUN_TEST(intern_different_strings);
    RUN_TEST(intern_n);
    RUN_TEST(intern_sv);
    RUN_TEST(intern_lookup);
    RUN_TEST(intern_many_strings);
    RUN_TEST(intern_hash_collision);

    /* Hash function tests */
    RUN_TEST(hash_string_basic);
    RUN_TEST(hash_string_empty);

    printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
