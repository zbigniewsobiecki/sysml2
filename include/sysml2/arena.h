/*
 * SysML v2 Parser - Arena Allocator
 *
 * Fast bump allocator for parser memory management.
 * Memory is allocated in large blocks and freed all at once.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_ARENA_H
#define SYSML2_ARENA_H

#include "common.h"

/* Default block size: 64KB */
#define SYSML2_ARENA_DEFAULT_BLOCK_SIZE (64 * 1024)

/* Arena block - linked list of memory blocks */
typedef struct Sysml2ArenaBlock {
    struct Sysml2ArenaBlock *next;
    size_t size;
    size_t used;
    /* Data follows... */
    char data[];
} Sysml2ArenaBlock;

/* Arena allocator */
typedef struct {
    Sysml2ArenaBlock *head;     /* Current block */
    Sysml2ArenaBlock *blocks;   /* All blocks (for freeing) */
    size_t block_size;          /* Default size for new blocks */
    size_t total_allocated;     /* Total bytes allocated */
} Sysml2Arena;

/* Initialize an arena with default block size */
void sysml2_arena_init(Sysml2Arena *arena);

/* Initialize an arena with custom block size */
void sysml2_arena_init_with_size(Sysml2Arena *arena, size_t block_size);

/* Free all memory in the arena */
void sysml2_arena_destroy(Sysml2Arena *arena);

/* Reset arena for reuse (keeps first block allocated) */
void sysml2_arena_reset(Sysml2Arena *arena);

/* Allocate memory from the arena (uninitialized) */
void *sysml2_arena_alloc(Sysml2Arena *arena, size_t size);

/* Allocate aligned memory from the arena */
void *sysml2_arena_alloc_aligned(Sysml2Arena *arena, size_t size, size_t alignment);

/* Allocate zeroed memory from the arena */
void *sysml2_arena_calloc(Sysml2Arena *arena, size_t count, size_t size);

/* Duplicate a string into the arena */
char *sysml2_arena_strdup(Sysml2Arena *arena, const char *str);

/* Duplicate a string with explicit length into the arena */
char *sysml2_arena_strndup(Sysml2Arena *arena, const char *str, size_t length);

/* Duplicate a string view into the arena (null-terminated) */
char *sysml2_arena_sv_dup(Sysml2Arena *arena, Sysml2StringView sv);

/* Printf into the arena, returning allocated string */
char *sysml2_arena_sprintf(Sysml2Arena *arena, const char *fmt, ...) SYSML2_PRINTF(2, 3);

/* Get total memory used by the arena */
size_t sysml2_arena_used(const Sysml2Arena *arena);

/* Typed allocation macros */
#define SYSML2_ARENA_NEW(arena, type) \
    ((type *)sysml2_arena_calloc((arena), 1, sizeof(type)))

#define SYSML2_ARENA_NEW_ARRAY(arena, type, count) \
    ((type *)sysml2_arena_calloc((arena), (count), sizeof(type)))

#endif /* SYSML2_ARENA_H */
