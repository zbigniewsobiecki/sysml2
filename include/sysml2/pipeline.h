/*
 * SysML v2 Parser - Processing Pipeline
 *
 * Encapsulates the file parsing, import resolution, validation,
 * and output generation pipeline.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SYSML2_PIPELINE_H
#define SYSML2_PIPELINE_H

#include "common.h"
#include "arena.h"
#include "intern.h"
#include "diagnostic.h"
#include "ast.h"
#include "cli.h"
#include "import_resolver.h"

/*
 * Pipeline Context - manages state for processing files
 */
typedef struct SysmlPipelineContext {
    Sysml2Arena *arena;
    Sysml2Intern *intern;
    Sysml2DiagContext *diag;
    SysmlImportResolver *resolver;
    const Sysml2CliOptions *options;
} SysmlPipelineContext;

/*
 * Create a pipeline context
 *
 * @param arena Memory arena (caller owns)
 * @param intern String interning table (caller owns)
 * @param options CLI options (caller owns)
 * @return New pipeline context, or NULL on error
 */
SysmlPipelineContext *sysml_pipeline_create(
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    const Sysml2CliOptions *options
);

/*
 * Destroy a pipeline context
 *
 * @param ctx Pipeline context to destroy
 */
void sysml_pipeline_destroy(SysmlPipelineContext *ctx);

/*
 * Process a single file
 *
 * Parses the file and optionally returns the model.
 *
 * @param ctx Pipeline context
 * @param path Path to file
 * @param out_model Output: parsed model (may be NULL if model not needed)
 * @return SYSML2_OK on success, error code otherwise
 */
Sysml2Result sysml_pipeline_process_file(
    SysmlPipelineContext *ctx,
    const char *path,
    SysmlSemanticModel **out_model
);

/*
 * Process stdin
 *
 * Parses content from stdin and optionally returns the model.
 *
 * @param ctx Pipeline context
 * @param out_model Output: parsed model (may be NULL if model not needed)
 * @return SYSML2_OK on success, error code otherwise
 */
Sysml2Result sysml_pipeline_process_stdin(
    SysmlPipelineContext *ctx,
    SysmlSemanticModel **out_model
);

/*
 * Process input content (shared implementation)
 *
 * @param ctx Pipeline context
 * @param display_name Name to display in diagnostics (e.g., "<stdin>" or file path)
 * @param content Input content
 * @param content_length Content length in bytes
 * @param out_model Output: parsed model (may be NULL if model not needed)
 * @return SYSML2_OK on success, error code otherwise
 */
Sysml2Result sysml_pipeline_process_input(
    SysmlPipelineContext *ctx,
    const char *display_name,
    const char *content,
    size_t content_length,
    SysmlSemanticModel **out_model
);

/*
 * Resolve imports for all cached models
 *
 * @param ctx Pipeline context
 * @return SYSML2_OK on success, error code otherwise
 */
Sysml2Result sysml_pipeline_resolve_all(SysmlPipelineContext *ctx);

/*
 * Run validation on all cached models
 *
 * @param ctx Pipeline context
 * @return SYSML2_OK on success, error code otherwise
 */
Sysml2Result sysml_pipeline_validate_all(SysmlPipelineContext *ctx);

/*
 * Write model as JSON to output stream
 *
 * @param ctx Pipeline context
 * @param model Model to write
 * @param out Output stream
 * @return SYSML2_OK on success, error code otherwise
 */
Sysml2Result sysml_pipeline_write_json(
    SysmlPipelineContext *ctx,
    SysmlSemanticModel *model,
    FILE *out
);

/*
 * Write model as SysML to output stream
 *
 * @param ctx Pipeline context
 * @param model Model to write
 * @param out Output stream
 * @return SYSML2_OK on success, error code otherwise
 */
Sysml2Result sysml_pipeline_write_sysml(
    SysmlPipelineContext *ctx,
    SysmlSemanticModel *model,
    FILE *out
);

/*
 * Print diagnostics summary
 *
 * @param ctx Pipeline context
 * @param output Output stream
 */
void sysml_pipeline_print_diagnostics(SysmlPipelineContext *ctx, FILE *output);

/*
 * Get diagnostic context from pipeline
 *
 * @param ctx Pipeline context
 * @return Diagnostic context
 */
Sysml2DiagContext *sysml_pipeline_get_diag(SysmlPipelineContext *ctx);

/*
 * Get import resolver from pipeline
 *
 * @param ctx Pipeline context
 * @return Import resolver
 */
SysmlImportResolver *sysml_pipeline_get_resolver(SysmlPipelineContext *ctx);

/*
 * Check if pipeline has errors
 *
 * @param ctx Pipeline context
 * @return true if errors have been recorded
 */
bool sysml_pipeline_has_errors(SysmlPipelineContext *ctx);

#endif /* SYSML2_PIPELINE_H */
