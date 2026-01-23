/*
 * SysML v2 Parser - Common Types and Macros
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_COMMON_H
#define SYSML2_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

/* Version information */
#define SYSML2_VERSION_MAJOR 0
#define SYSML2_VERSION_MINOR 1
#define SYSML2_VERSION_PATCH 0
#define SYSML2_VERSION_STRING "0.1.0"

/* Compiler attributes */
#if defined(__GNUC__) || defined(__clang__)
    #define SYSML2_UNUSED __attribute__((unused))
    #define SYSML2_NORETURN __attribute__((noreturn))
    #define SYSML2_PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
    #define SYSML2_LIKELY(x) __builtin_expect(!!(x), 1)
    #define SYSML2_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define SYSML2_INLINE static inline __attribute__((always_inline))
#else
    #define SYSML2_UNUSED
    #define SYSML2_NORETURN
    #define SYSML2_PRINTF(fmt, args)
    #define SYSML2_LIKELY(x) (x)
    #define SYSML2_UNLIKELY(x) (x)
    #define SYSML2_INLINE static inline
#endif

/* Utility macros */
#define SYSML2_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define SYSML2_MAX(a, b) ((a) > (b) ? (a) : (b))
#define SYSML2_MIN(a, b) ((a) < (b) ? (a) : (b))

/* Alignment macros */
#define SYSML2_ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
#define SYSML2_ALIGN_DOWN(x, align) ((x) & ~((align) - 1))
#define SYSML2_DEFAULT_ALIGN (sizeof(void*))

/* String view - non-owning reference to a string */
typedef struct {
    const char *data;
    size_t length;
} Sysml2StringView;

#define SYSML2_SV_NULL ((Sysml2StringView){NULL, 0})
#define SYSML2_SV_LITERAL(s) ((Sysml2StringView){(s), sizeof(s) - 1})
#define SYSML2_SV_FMT "%.*s"
#define SYSML2_SV_ARG(sv) (int)(sv).length, (sv).data

/* Create a string view from a C string */
SYSML2_INLINE Sysml2StringView sysml2_sv_from_cstr(const char *str) {
    if (!str) return SYSML2_SV_NULL;
    size_t len = 0;
    while (str[len]) len++;
    return (Sysml2StringView){str, len};
}

/* Create a string view from a pointer and length */
SYSML2_INLINE Sysml2StringView sysml2_sv_from_parts(const char *data, size_t length) {
    return (Sysml2StringView){data, length};
}

/* Compare two string views for equality */
SYSML2_INLINE bool sysml2_sv_equals(Sysml2StringView a, Sysml2StringView b) {
    if (a.length != b.length) return false;
    for (size_t i = 0; i < a.length; i++) {
        if (a.data[i] != b.data[i]) return false;
    }
    return true;
}

/* Compare string view to C string */
SYSML2_INLINE bool sysml2_sv_equals_cstr(Sysml2StringView sv, const char *str) {
    return sysml2_sv_equals(sv, sysml2_sv_from_cstr(str));
}

/* Source location - tracks position in source file */
typedef struct {
    uint32_t line;      /* 1-based line number */
    uint32_t column;    /* 1-based column number (byte offset) */
    uint32_t offset;    /* Byte offset from start of file */
} Sysml2SourceLoc;

#define SYSML2_LOC_INVALID ((Sysml2SourceLoc){0, 0, 0})

/* Source range - span from start to end */
typedef struct {
    Sysml2SourceLoc start;
    Sysml2SourceLoc end;
} Sysml2SourceRange;

#define SYSML2_RANGE_INVALID ((Sysml2SourceRange){SYSML2_LOC_INVALID, SYSML2_LOC_INVALID})

/* Create a source range from two locations */
SYSML2_INLINE Sysml2SourceRange sysml2_range_from_locs(Sysml2SourceLoc start, Sysml2SourceLoc end) {
    return (Sysml2SourceRange){start, end};
}

/* Source file - represents a loaded source file */
typedef struct {
    const char *path;           /* File path (interned) */
    const char *content;        /* File content (null-terminated) */
    size_t content_length;      /* Length of content */
    const uint32_t *line_offsets; /* Array of byte offsets for each line start */
    uint32_t line_count;        /* Number of lines */
} Sysml2SourceFile;

/* Result type for operations that can fail */
typedef enum {
    SYSML2_OK = 0,
    SYSML2_ERROR_FILE_NOT_FOUND,
    SYSML2_ERROR_FILE_READ,
    SYSML2_ERROR_OUT_OF_MEMORY,
    SYSML2_ERROR_INVALID_UTF8,
    SYSML2_ERROR_SYNTAX,
    SYSML2_ERROR_SEMANTIC,
} Sysml2Result;

/* Get string description of result code */
const char *sysml2_result_to_string(Sysml2Result result);

#endif /* SYSML2_COMMON_H */
