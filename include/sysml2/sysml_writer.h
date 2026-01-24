/*
 * SysML v2 Parser - SysML/KerML Source Writer
 *
 * Pretty prints the semantic model back to SysML/KerML source code
 * with canonical formatting and comment preservation.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_SYSML_WRITER_H
#define SYSML2_SYSML_WRITER_H

#include "common.h"
#include "ast.h"
#include <stdio.h>

/* Canonical indent size: 4 spaces */
#define SYSML_WRITER_INDENT_SIZE 4

/*
 * Write the semantic model as formatted SysML/KerML source to a file
 *
 * Output is canonical:
 * - 4-space indentation
 * - Imports sorted and grouped
 * - Comments preserved from trivia
 * - Consistent spacing around operators
 *
 * @param model Semantic model to serialize
 * @param out Output file handle
 * @return SYSML2_OK on success, error code on failure
 */
Sysml2Result sysml2_sysml_write(
    const SysmlSemanticModel *model,
    FILE *out
);

/*
 * Write the semantic model as formatted SysML/KerML source to a string
 *
 * The returned string is allocated and must be freed by the caller.
 *
 * @param model Semantic model to serialize
 * @param out_str Output string pointer
 * @return SYSML2_OK on success, error code on failure
 */
Sysml2Result sysml2_sysml_write_string(
    const SysmlSemanticModel *model,
    char **out_str
);

#endif /* SYSML2_SYSML_WRITER_H */
