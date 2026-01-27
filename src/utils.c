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
#include <dirent.h>
#include <limits.h>
#include <errno.h>

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

/* ========== Recursive Directory Traversal ========== */

/* Inode tracking to detect symlink cycles */
typedef struct {
    dev_t *devices;
    ino_t *inodes;
    size_t count;
    size_t capacity;
} InodeTracker;

static void inode_tracker_init(InodeTracker *tracker) {
    tracker->devices = NULL;
    tracker->inodes = NULL;
    tracker->count = 0;
    tracker->capacity = 0;
}

static void inode_tracker_destroy(InodeTracker *tracker) {
    free(tracker->devices);
    free(tracker->inodes);
}

static bool inode_tracker_has(InodeTracker *tracker, dev_t dev, ino_t ino) {
    for (size_t i = 0; i < tracker->count; i++) {
        if (tracker->devices[i] == dev && tracker->inodes[i] == ino) {
            return true;
        }
    }
    return false;
}

static bool inode_tracker_add(InodeTracker *tracker, dev_t dev, ino_t ino) {
    if (tracker->count >= tracker->capacity) {
        size_t new_cap = tracker->capacity == 0 ? 16 : tracker->capacity * 2;
        dev_t *new_devices = realloc(tracker->devices, new_cap * sizeof(dev_t));
        ino_t *new_inodes = realloc(tracker->inodes, new_cap * sizeof(ino_t));
        if (!new_devices || !new_inodes) {
            free(new_devices);
            free(new_inodes);
            return false;
        }
        tracker->devices = new_devices;
        tracker->inodes = new_inodes;
        tracker->capacity = new_cap;
    }
    tracker->devices[tracker->count] = dev;
    tracker->inodes[tracker->count] = ino;
    tracker->count++;
    return true;
}

/* Dynamic file list */
typedef struct {
    char **files;
    size_t count;
    size_t capacity;
} FileList;

static void file_list_init(FileList *list) {
    list->files = NULL;
    list->count = 0;
    list->capacity = 0;
}

static bool file_list_add(FileList *list, const char *path) {
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity == 0 ? 16 : list->capacity * 2;
        char **new_files = realloc(list->files, new_cap * sizeof(char *));
        if (!new_files) {
            return false;
        }
        list->files = new_files;
        list->capacity = new_cap;
    }
    list->files[list->count] = strdup(path);
    if (!list->files[list->count]) {
        return false;
    }
    list->count++;
    return true;
}

/* Check if string ends with suffix */
static bool ends_with(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) {
        return false;
    }
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

/* Recursive helper */
static void find_files_recursive_impl(
    const char *dir_path,
    const char *extension,
    FileList *list,
    InodeTracker *tracker
) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        if (errno == EACCES) {
            fprintf(stderr, "warning: cannot access directory '%s': %s\n",
                    dir_path, strerror(errno));
        }
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char *full_path = sysml2_path_join(dir_path, entry->d_name);
        if (!full_path) {
            continue;
        }

        struct stat st;
        if (stat(full_path, &st) != 0) {
            if (errno == EACCES) {
                fprintf(stderr, "warning: cannot access '%s': %s\n",
                        full_path, strerror(errno));
            }
            free(full_path);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            /* Check for symlink cycles */
            if (inode_tracker_has(tracker, st.st_dev, st.st_ino)) {
                free(full_path);
                continue;
            }
            if (!inode_tracker_add(tracker, st.st_dev, st.st_ino)) {
                free(full_path);
                continue;
            }
            /* Recurse into directory */
            find_files_recursive_impl(full_path, extension, list, tracker);
        } else if (S_ISREG(st.st_mode)) {
            /* Check extension */
            if (ends_with(entry->d_name, extension)) {
                file_list_add(list, full_path);
            }
        }

        free(full_path);
    }

    closedir(dir);
}

char **sysml2_find_files_recursive(
    const char *directory,
    const char *extension,
    size_t *out_count
) {
    if (!directory || !extension || !out_count) {
        return NULL;
    }

    *out_count = 0;

    /* Check that directory exists and is a directory */
    struct stat st;
    if (stat(directory, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return NULL;
    }

    FileList list;
    file_list_init(&list);

    InodeTracker tracker;
    inode_tracker_init(&tracker);

    /* Add the root directory to tracker to handle self-referencing symlinks */
    inode_tracker_add(&tracker, st.st_dev, st.st_ino);

    find_files_recursive_impl(directory, extension, &list, &tracker);

    inode_tracker_destroy(&tracker);

    *out_count = list.count;

    /* Return empty array for empty results (not NULL) */
    if (list.count == 0) {
        free(list.files);
        return malloc(sizeof(char *));  /* Empty but non-NULL array */
    }

    return list.files;
}

void sysml2_free_file_list(char **files, size_t count) {
    if (!files) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(files[i]);
    }
    free(files);
}
