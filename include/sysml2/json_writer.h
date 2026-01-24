/*
 * SysML v2 Parser - JSON Writer
 *
 * Serializes the semantic model to JSON format suitable for
 * diagramming tools (Cytoscape.js, D3.js).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_JSON_WRITER_H
#define SYSML2_JSON_WRITER_H

#include "common.h"
#include "ast.h"
#include <stdio.h>

/*
 * JSON output options
 */
typedef struct {
    bool pretty;          /* Pretty print with indentation */
    int indent_size;      /* Spaces per indent level (default: 2) */
    bool include_source;  /* Include source file in meta */
} Sysml2JsonOptions;

/* Default JSON options */
#define SYSML_JSON_OPTIONS_DEFAULT { .pretty = true, .indent_size = 2, .include_source = true }

/*
 * Write the semantic model as JSON to a file
 *
 * Output format:
 * {
 *   "meta": { "version": "1.0", "source": "file.sysml" },
 *   "elements": [ ... ],
 *   "relationships": [ ... ]
 * }
 *
 * @param model Semantic model to serialize
 * @param out Output file handle
 * @param options Output options (can be NULL for defaults)
 * @return SYSML2_OK on success, error code on failure
 */
Sysml2Result sysml2_json_write(
    const SysmlSemanticModel *model,
    FILE *out,
    const Sysml2JsonOptions *options
);

/*
 * Write the semantic model as JSON to a string
 *
 * The returned string is allocated and must be freed by the caller.
 *
 * @param model Semantic model to serialize
 * @param options Output options (can be NULL for defaults)
 * @param out_str Output string pointer
 * @return SYSML2_OK on success, error code on failure
 */
Sysml2Result sysml2_json_write_string(
    const SysmlSemanticModel *model,
    const Sysml2JsonOptions *options,
    char **out_str
);

/*
 * Escape a string for JSON output
 *
 * Handles special characters: ", \, \n, \r, \t, etc.
 *
 * @param str Input string
 * @param out Output buffer
 * @param out_size Size of output buffer
 * @return Number of characters written (excluding null terminator)
 */
size_t sysml2_json_escape_string(const char *str, char *out, size_t out_size);

#endif /* SYSML2_JSON_WRITER_H */
