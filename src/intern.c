/*
 * SysML v2 Parser - String Interning Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/intern.h"
#include <stdlib.h>
#include <string.h>

/* FNV-1a hash constants */
#define FNV_OFFSET_BASIS 2166136261u
#define FNV_PRIME 16777619u

uint32_t sysml2_hash_string(const char *str, size_t length) {
    uint32_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)str[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

void sysml2_intern_init(Sysml2Intern *intern, Sysml2Arena *arena) {
    sysml2_intern_init_with_capacity(intern, arena, SYSML2_INTERN_DEFAULT_CAPACITY);
}

void sysml2_intern_init_with_capacity(Sysml2Intern *intern, Sysml2Arena *arena, size_t capacity) {
    intern->arena = arena;
    intern->capacity = capacity;
    intern->count = 0;
    intern->buckets = sysml2_arena_calloc(arena, capacity, sizeof(Sysml2InternEntry *));
}

void sysml2_intern_destroy(Sysml2Intern *intern) {
    /* Entries are in the arena, nothing to free */
    intern->buckets = NULL;
    intern->capacity = 0;
    intern->count = 0;
}

/* Find or create an entry for a string */
static const char *intern_impl(Sysml2Intern *intern, const char *str, size_t length) {
    if (!intern->buckets) return NULL;

    uint32_t hash = sysml2_hash_string(str, length);
    size_t index = hash % intern->capacity;

    /* Search existing entries */
    for (Sysml2InternEntry *entry = intern->buckets[index]; entry; entry = entry->next) {
        if (entry->hash == hash && entry->length == length) {
            if (memcmp(entry->data, str, length) == 0) {
                return entry->data;
            }
        }
    }

    /* Allocate new entry */
    Sysml2InternEntry *new_entry = sysml2_arena_alloc(
        intern->arena,
        sizeof(Sysml2InternEntry) + length + 1
    );
    if (!new_entry) return NULL;

    new_entry->hash = hash;
    new_entry->length = length;
    memcpy(new_entry->data, str, length);
    new_entry->data[length] = '\0';

    /* Insert at head of bucket */
    new_entry->next = intern->buckets[index];
    intern->buckets[index] = new_entry;
    intern->count++;

    return new_entry->data;
}

const char *sysml2_intern(Sysml2Intern *intern, const char *str) {
    if (!str) return NULL;
    return intern_impl(intern, str, strlen(str));
}

const char *sysml2_intern_n(Sysml2Intern *intern, const char *str, size_t length) {
    if (!str) return NULL;
    return intern_impl(intern, str, length);
}

const char *sysml2_intern_sv(Sysml2Intern *intern, Sysml2StringView sv) {
    if (!sv.data) return NULL;
    return intern_impl(intern, sv.data, sv.length);
}

/* Lookup without inserting */
static const char *lookup_impl(const Sysml2Intern *intern, const char *str, size_t length) {
    if (!intern->buckets) return NULL;

    uint32_t hash = sysml2_hash_string(str, length);
    size_t index = hash % intern->capacity;

    for (Sysml2InternEntry *entry = intern->buckets[index]; entry; entry = entry->next) {
        if (entry->hash == hash && entry->length == length) {
            if (memcmp(entry->data, str, length) == 0) {
                return entry->data;
            }
        }
    }

    return NULL;
}

const char *sysml2_intern_lookup(const Sysml2Intern *intern, const char *str) {
    if (!str) return NULL;
    return lookup_impl(intern, str, strlen(str));
}

const char *sysml2_intern_lookup_sv(const Sysml2Intern *intern, Sysml2StringView sv) {
    if (!sv.data) return NULL;
    return lookup_impl(intern, sv.data, sv.length);
}

size_t sysml2_intern_count(const Sysml2Intern *intern) {
    return intern->count;
}
