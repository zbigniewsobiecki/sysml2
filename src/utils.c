/*
 * SysML v2 Parser - Utility Functions Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>

char *sysml2_read_file(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0) {
        fclose(file);
        return NULL;
    }

    char *content = malloc(size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(content, 1, size, file);
    fclose(file);

    content[read] = '\0';
    if (out_size) {
        *out_size = read;
    }

    return content;
}

char *sysml2_read_stdin(size_t *out_size) {
    size_t capacity = 4096;
    size_t length = 0;
    char *content = malloc(capacity);
    if (!content) return NULL;

    size_t bytes_read;
    while ((bytes_read = fread(content + length, 1, capacity - length, stdin)) > 0) {
        length += bytes_read;
        if (length == capacity) {
            capacity *= 2;
            char *new_content = realloc(content, capacity);
            if (!new_content) {
                free(content);
                return NULL;
            }
            content = new_content;
        }
    }

    content[length] = '\0';
    if (out_size) *out_size = length;
    return content;
}

bool sysml2_is_file(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool sysml2_is_directory(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

char *sysml2_path_join(const char *dir, const char *name) {
    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);

    /* Remove trailing slash from dir if present */
    while (dir_len > 0 && dir[dir_len - 1] == '/') {
        dir_len--;
    }

    char *result = malloc(dir_len + 1 + name_len + 1);
    if (!result) return NULL;

    memcpy(result, dir, dir_len);
    result[dir_len] = '/';
    memcpy(result + dir_len + 1, name, name_len);
    result[dir_len + 1 + name_len] = '\0';

    return result;
}

char *sysml2_get_realpath(const char *path) {
    char *resolved = realpath(path, NULL);
    return resolved;
}

uint32_t *sysml2_build_line_offsets(const char *content, size_t length, uint32_t *out_count) {
    /* Count lines first */
    uint32_t count = 1;
    for (size_t i = 0; i < length; i++) {
        if (content[i] == '\n') {
            count++;
        }
    }

    uint32_t *offsets = malloc(count * sizeof(uint32_t));
    if (!offsets) {
        return NULL;
    }

    offsets[0] = 0;
    uint32_t line = 1;
    for (size_t i = 0; i < length; i++) {
        if (content[i] == '\n' && line < count) {
            offsets[line++] = i + 1;
        }
    }

    *out_count = count;
    return offsets;
}
