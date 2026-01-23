/*
 * SysML v2 Parser - Arena Allocator Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* Create a new arena block */
static Sysml2ArenaBlock *arena_new_block(size_t size) {
    Sysml2ArenaBlock *block = malloc(sizeof(Sysml2ArenaBlock) + size);
    if (!block) return NULL;
    block->next = NULL;
    block->size = size;
    block->used = 0;
    return block;
}

void sysml2_arena_init(Sysml2Arena *arena) {
    sysml2_arena_init_with_size(arena, SYSML2_ARENA_DEFAULT_BLOCK_SIZE);
}

void sysml2_arena_init_with_size(Sysml2Arena *arena, size_t block_size) {
    arena->head = NULL;
    arena->blocks = NULL;
    arena->block_size = block_size;
    arena->total_allocated = 0;
}

void sysml2_arena_destroy(Sysml2Arena *arena) {
    Sysml2ArenaBlock *block = arena->blocks;
    while (block) {
        Sysml2ArenaBlock *next = block->next;
        free(block);
        block = next;
    }
    arena->head = NULL;
    arena->blocks = NULL;
    arena->total_allocated = 0;
}

void sysml2_arena_reset(Sysml2Arena *arena) {
    /* Free all blocks except the first one */
    if (arena->blocks) {
        Sysml2ArenaBlock *first = arena->blocks;
        Sysml2ArenaBlock *block = first->next;
        while (block) {
            Sysml2ArenaBlock *next = block->next;
            free(block);
            block = next;
        }
        first->next = NULL;
        first->used = 0;
        arena->head = first;
    }
    arena->total_allocated = 0;
}

void *sysml2_arena_alloc_aligned(Sysml2Arena *arena, size_t size, size_t alignment) {
    /* Ensure alignment is a power of 2 */
    assert(alignment > 0 && (alignment & (alignment - 1)) == 0);

    /* Try to allocate from current block */
    if (arena->head) {
        size_t aligned_used = SYSML2_ALIGN_UP(arena->head->used, alignment);
        if (aligned_used + size <= arena->head->size) {
            void *ptr = arena->head->data + aligned_used;
            arena->head->used = aligned_used + size;
            arena->total_allocated += size;
            return ptr;
        }
    }

    /* Need a new block */
    size_t block_size = arena->block_size;
    if (size + alignment > block_size) {
        /* Allocate a larger block for big allocations */
        block_size = size + alignment;
    }

    Sysml2ArenaBlock *new_block = arena_new_block(block_size);
    if (!new_block) return NULL;

    /* Link into block list */
    new_block->next = arena->blocks;
    arena->blocks = new_block;
    arena->head = new_block;

    /* Allocate from new block */
    size_t aligned_used = SYSML2_ALIGN_UP(0, alignment);
    void *ptr = new_block->data + aligned_used;
    new_block->used = aligned_used + size;
    arena->total_allocated += size;
    return ptr;
}

void *sysml2_arena_alloc(Sysml2Arena *arena, size_t size) {
    return sysml2_arena_alloc_aligned(arena, size, SYSML2_DEFAULT_ALIGN);
}

void *sysml2_arena_calloc(Sysml2Arena *arena, size_t count, size_t size) {
    size_t total = count * size;
    void *ptr = sysml2_arena_alloc(arena, total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

char *sysml2_arena_strdup(Sysml2Arena *arena, const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    return sysml2_arena_strndup(arena, str, len);
}

char *sysml2_arena_strndup(Sysml2Arena *arena, const char *str, size_t length) {
    if (!str) return NULL;
    char *dup = sysml2_arena_alloc(arena, length + 1);
    if (dup) {
        memcpy(dup, str, length);
        dup[length] = '\0';
    }
    return dup;
}

char *sysml2_arena_sv_dup(Sysml2Arena *arena, Sysml2StringView sv) {
    return sysml2_arena_strndup(arena, sv.data, sv.length);
}

char *sysml2_arena_sprintf(Sysml2Arena *arena, const char *fmt, ...) {
    va_list args, args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);

    /* Calculate required size */
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (len < 0) {
        va_end(args_copy);
        return NULL;
    }

    /* Allocate and format */
    char *str = sysml2_arena_alloc(arena, len + 1);
    if (str) {
        vsnprintf(str, len + 1, fmt, args_copy);
    }
    va_end(args_copy);
    return str;
}

size_t sysml2_arena_used(const Sysml2Arena *arena) {
    return arena->total_allocated;
}
