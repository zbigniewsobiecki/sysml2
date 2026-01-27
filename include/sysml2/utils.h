/*
 * SysML v2 Parser - Utility Functions
 *
 * Common file I/O utilities and dynamic array growth macro.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_UTILS_H
#define SYSML2_UTILS_H

#include "common.h"
#include "arena.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Dynamic Array Growth Macro
 *
 * Grows an arena-allocated array when count reaches capacity.
 * Doubles the capacity (or starts at 8 if capacity is 0).
 *
 * @param arena Arena for allocation
 * @param array Pointer to array (will be updated)
 * @param count Current element count
 * @param capacity Current capacity (will be updated)
 * @param elem_type Type of array elements
 */
#define SYSML2_ARRAY_GROW(arena, array, count, capacity, elem_type) do { \
    if ((count) >= (capacity)) { \
        size_t new_cap = (capacity) ? (capacity) * 2 : 8; \
        elem_type *new_arr = SYSML2_ARENA_NEW_ARRAY((arena), elem_type, new_cap); \
        if (new_arr && (array)) { \
            memcpy(new_arr, (array), (count) * sizeof(elem_type)); \
        } \
        (array) = new_arr; \
        (capacity) = new_cap; \
    } \
} while(0)

/*
 * Read a file into memory
 *
 * @param path Path to file
 * @param out_size Output: file size (may be NULL)
 * @return Allocated buffer with file contents (caller must free), or NULL on error
 */
char *sysml2_read_file(const char *path, size_t *out_size);

/*
 * Read from stdin into memory
 *
 * @param out_size Output: data size (may be NULL)
 * @return Allocated buffer with stdin contents (caller must free), or NULL on error
 */
char *sysml2_read_stdin(size_t *out_size);

/*
 * Check if path exists and is a regular file
 *
 * @param path Path to check
 * @return true if path is a regular file
 */
bool sysml2_is_file(const char *path);

/*
 * Check if path exists and is a directory
 *
 * @param path Path to check
 * @return true if path is a directory
 */
bool sysml2_is_directory(const char *path);

/*
 * Join two path components
 *
 * Handles trailing slashes in dir component.
 *
 * @param dir Directory path
 * @param name Filename or subdirectory
 * @return Allocated path string (caller must free), or NULL on error
 */
char *sysml2_path_join(const char *dir, const char *name);

/*
 * Get canonical absolute path
 *
 * @param path Path to resolve
 * @return Allocated absolute path (caller must free), or NULL on error
 */
char *sysml2_get_realpath(const char *path);

/*
 * Build line offset table from content
 *
 * Creates an array of byte offsets for each line start.
 *
 * @param content File content
 * @param length Content length in bytes
 * @param out_count Output: number of lines
 * @return Allocated array of line offsets (caller must free), or NULL on error
 */
uint32_t *sysml2_build_line_offsets(const char *content, size_t length, uint32_t *out_count);

/*
 * Recursively find all files matching an extension in a directory
 *
 * Follows symbolic links but tracks visited inodes to detect cycles.
 * Prints warnings for permission errors and continues with accessible files.
 *
 * @param directory Root directory to search
 * @param extension File extension to match (e.g., ".sysml")
 * @param out_count Output: number of files found
 * @return Allocated array of file paths (caller must free with sysml2_free_file_list), or NULL on error
 */
char **sysml2_find_files_recursive(
    const char *directory,
    const char *extension,
    size_t *out_count
);

/*
 * Free a file list returned by sysml2_find_files_recursive
 *
 * @param files Array of file paths
 * @param count Number of files in array
 */
void sysml2_free_file_list(char **files, size_t count);

#endif /* SYSML2_UTILS_H */
