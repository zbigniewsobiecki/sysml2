/*
 * SysML v2 Parser - Semantic Validator
 *
 * Post-parse semantic validation: type resolution,
 * duplicate detection, circular specialization checks.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_VALIDATOR_H
#define SYSML2_VALIDATOR_H

#include "common.h"
#include "arena.h"
#include "intern.h"
#include "ast.h"
#include "diagnostic.h"
#include "symtab.h"

/*
 * Validation Options - controls which checks are performed
 */
typedef struct {
    bool check_undefined_types;    /* E3001 - default: true */
    bool check_duplicate_names;    /* E3004 - default: true */
    bool check_circular_specs;     /* E3005 - default: true */
    bool check_type_compatibility; /* E3006 - default: true */
    bool suggest_corrections;      /* "did you mean?" hints */
    size_t max_suggestions;        /* default: 3 */
} SysmlValidationOptions;

/* Default validation options (all checks enabled) */
#define SYSML_VALIDATION_OPTIONS_DEFAULT ((SysmlValidationOptions){ \
    .check_undefined_types = true, \
    .check_duplicate_names = true, \
    .check_circular_specs = true, \
    .check_type_compatibility = true, \
    .suggest_corrections = true, \
    .max_suggestions = 3 \
})

/*
 * Check if a usage kind is compatible with a definition kind
 *
 * @param usage_kind Kind of the usage (e.g., PART_USAGE)
 * @param def_kind Kind of the definition being referenced
 * @return true if compatible
 */
bool sysml_is_type_compatible(SysmlNodeKind usage_kind, SysmlNodeKind def_kind);

/*
 * Run semantic validation on a parsed model
 *
 * Performs the following checks based on options:
 * - E3001: Undefined type references
 * - E3004: Duplicate names in same scope
 * - E3005: Circular specializations
 * - E3006: Type compatibility mismatches
 *
 * @param model Parsed semantic model
 * @param diag_ctx Diagnostic context for error reporting
 * @param source_file Source file for error locations (may be NULL)
 * @param arena Memory arena
 * @param intern String interning table
 * @param options Validation options (NULL for defaults)
 * @return SYSML2_OK if valid, SYSML2_ERROR_SEMANTIC if errors found
 */
Sysml2Result sysml_validate(
    const SysmlSemanticModel *model,
    Sysml2DiagContext *diag_ctx,
    const Sysml2SourceFile *source_file,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    const SysmlValidationOptions *options
);

/*
 * Run unified semantic validation on multiple models
 *
 * This builds a single symbol table from all models before
 * performing type resolution, enabling cross-file imports.
 *
 * @param models Array of parsed semantic models
 * @param model_count Number of models
 * @param diag_ctx Diagnostic context for error reporting
 * @param arena Memory arena
 * @param intern String interning table
 * @param options Validation options (NULL for defaults)
 * @return SYSML2_OK if valid, SYSML2_ERROR_SEMANTIC if errors found
 */
Sysml2Result sysml_validate_multi(
    SysmlSemanticModel **models,
    size_t model_count,
    Sysml2DiagContext *diag_ctx,
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    const SysmlValidationOptions *options
);

#endif /* SYSML2_VALIDATOR_H */
