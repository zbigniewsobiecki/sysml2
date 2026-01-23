/*
 * SysML v2 Parser - String Interning
 *
 * Deduplicates strings by storing only one copy of each unique string.
 * Enables fast pointer comparison for string equality.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_INTERN_H
#define SYSML2_INTERN_H

#include "common.h"
#include "arena.h"

/* Default hash table capacity */
#define SYSML2_INTERN_DEFAULT_CAPACITY 1024

/* Interned string entry */
typedef struct Sysml2InternEntry {
    struct Sysml2InternEntry *next; /* Hash collision chain */
    uint32_t hash;                  /* Cached hash value */
    uint32_t length;                /* String length */
    char data[];                    /* Null-terminated string */
} Sysml2InternEntry;

/* String intern table */
typedef struct {
    Sysml2Arena *arena;             /* Arena for string storage */
    Sysml2InternEntry **buckets;    /* Hash table buckets */
    size_t capacity;                /* Number of buckets */
    size_t count;                   /* Number of unique strings */
} Sysml2Intern;

/* Initialize the intern table with default capacity */
void sysml2_intern_init(Sysml2Intern *intern, Sysml2Arena *arena);

/* Initialize the intern table with custom capacity */
void sysml2_intern_init_with_capacity(Sysml2Intern *intern, Sysml2Arena *arena, size_t capacity);

/* Clean up the intern table (strings remain valid until arena is destroyed) */
void sysml2_intern_destroy(Sysml2Intern *intern);

/* Intern a null-terminated string, returns interned pointer */
const char *sysml2_intern(Sysml2Intern *intern, const char *str);

/* Intern a string with explicit length, returns interned pointer */
const char *sysml2_intern_n(Sysml2Intern *intern, const char *str, size_t length);

/* Intern a string view, returns interned pointer */
const char *sysml2_intern_sv(Sysml2Intern *intern, Sysml2StringView sv);

/* Check if a string is already interned (returns NULL if not) */
const char *sysml2_intern_lookup(const Sysml2Intern *intern, const char *str);

/* Check if a string view is already interned (returns NULL if not) */
const char *sysml2_intern_lookup_sv(const Sysml2Intern *intern, Sysml2StringView sv);

/* Get the number of unique interned strings */
size_t sysml2_intern_count(const Sysml2Intern *intern);

/* FNV-1a hash function */
uint32_t sysml2_hash_string(const char *str, size_t length);

#endif /* SYSML2_INTERN_H */
