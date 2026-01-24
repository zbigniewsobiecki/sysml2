/*
 * SysML v2 Parser - Processing Pipeline Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/pipeline.h"
#include "sysml2/utils.h"
#include "sysml2/lexer.h"
#include "sysml2/ast_builder.h"
#include "sysml2/json_writer.h"
#include "sysml2/sysml_writer.h"
#include "sysml2/validator.h"
#include "sysml2/query.h"
#include "sysml_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

Sysml2PipelineContext *sysml2_pipeline_create(
    Sysml2Arena *arena,
    Sysml2Intern *intern,
    const Sysml2CliOptions *options
) {
    Sysml2PipelineContext *ctx = malloc(sizeof(Sysml2PipelineContext));
    if (!ctx) return NULL;

    ctx->arena = arena;
    ctx->intern = intern;
    ctx->options = options;

    /* Initialize diagnostics */
    ctx->diag = malloc(sizeof(Sysml2DiagContext));
    if (!ctx->diag) {
        free(ctx);
        return NULL;
    }
    sysml2_diag_context_init(ctx->diag, arena);
    sysml2_diag_set_max_errors(ctx->diag, options->max_errors);
    ctx->diag->treat_warnings_as_errors = options->treat_warnings_as_errors;

    /* Create import resolver */
    ctx->resolver = sysml2_resolver_create(arena, intern);
    if (!ctx->resolver) {
        free(ctx->diag);
        free(ctx);
        return NULL;
    }

    ctx->resolver->verbose = options->verbose;
    ctx->resolver->disabled = options->no_resolve;

    /* Add library paths from environment and CLI */
    sysml2_resolver_add_paths_from_env(ctx->resolver);
    for (size_t i = 0; i < options->library_path_count; i++) {
        sysml2_resolver_add_path(ctx->resolver, options->library_paths[i]);
    }

    return ctx;
}

void sysml2_pipeline_destroy(Sysml2PipelineContext *ctx) {
    if (!ctx) return;

    if (ctx->resolver) {
        sysml2_resolver_destroy(ctx->resolver);
    }
    /* Note: diag memory is managed by arena, just free the struct */
    if (ctx->diag) {
        free(ctx->diag);
    }
    free(ctx);
}

Sysml2Result sysml2_pipeline_process_input(
    Sysml2PipelineContext *ctx,
    const char *display_name,
    const char *content,
    size_t content_length,
    SysmlSemanticModel **out_model
) {
    if (out_model) *out_model = NULL;
    if (ctx->options->verbose) {
        fprintf(stderr, "Processing: %s\n", display_name);
    }

    /* Dump tokens if requested (uses the old lexer) */
    if (ctx->options->dump_tokens) {
        /* Build line offsets for lexer */
        uint32_t line_count;
        uint32_t *line_offsets = sysml2_build_line_offsets(content, content_length, &line_count);
        if (!line_offsets) {
            return SYSML2_ERROR_OUT_OF_MEMORY;
        }

        Sysml2SourceFile source_file = {
            .path = sysml2_intern(ctx->intern, display_name),
            .content = content,
            .content_length = content_length,
            .line_offsets = line_offsets,
            .line_count = line_count,
        };

        Sysml2Lexer lexer;
        sysml2_lexer_init(&lexer, &source_file, ctx->intern, ctx->diag);

        Sysml2Token token;
        printf("Tokens for %s:\n", display_name);
        printf("%-6s %-20s %-10s %s\n", "Line", "Type", "Loc", "Text");
        printf("%-6s %-20s %-10s %s\n", "----", "----", "---", "----");

        while (true) {
            token = sysml2_lexer_next(&lexer);
            printf("%-6u %-20s %u:%-8u " SYSML2_SV_FMT "\n",
                token.range.start.line,
                sysml2_token_type_to_string(token.type),
                token.range.start.line,
                token.range.start.column,
                SYSML2_SV_ARG(token.text));

            if (token.type == SYSML2_TOKEN_EOF) {
                break;
            }
        }
        printf("\n");
        free(line_offsets);
    }

    /* Create build context for AST building and semantic validation */
    SysmlBuildContext *build_ctx = sysml2_build_context_create(ctx->arena, ctx->intern, display_name);
    if (!build_ctx) {
        fprintf(stderr, "error: failed to create build context\n");
        return SYSML2_ERROR_OUT_OF_MEMORY;
    }

    /* Parse input using packcc-generated parser */
    SysmlParserContext pctx = {
        .filename = display_name,
        .input = content,
        .input_len = content_length,
        .input_pos = 0,
        .error_count = 0,
        .line = 1,
        .col = 1,
        /* Furthest failure tracking */
        .furthest_pos = 0,
        .furthest_line = 0,
        .furthest_col = 0,
        .failed_rule_count = 0,
        .context_rule = NULL,
        /* AST building context */
        .build_ctx = build_ctx,
    };

    sysml2_context_t *parser = sysml2_create(&pctx);
    if (!parser) {
        fprintf(stderr, "error: failed to create parser\n");
        return SYSML2_ERROR_OUT_OF_MEMORY;
    }

    void *result = NULL;
    int parse_ok = sysml2_parse(parser, &result);

    /* Track errors in the diagnostic context */
    if (pctx.error_count > 0) {
        ctx->diag->error_count += pctx.error_count;
    }

    /* Dump AST if requested - not yet implemented */
    if (ctx->options->dump_ast) {
        if (parse_ok) {
            fprintf(stderr, "note: --dump-ast not yet implemented with new parser\n");
        }
    }

    /* Finalize model */
    Sysml2Result final_result = (parse_ok && pctx.error_count == 0) ? SYSML2_OK : SYSML2_ERROR_SYNTAX;

    if (parse_ok) {
        SysmlSemanticModel *model = sysml2_build_finalize(build_ctx);
        if (model) {
            /* If caller wants the model back, return it */
            if (out_model) {
                *out_model = model;
            }
        }
    }

    sysml2_destroy(parser);

    return final_result;
}

Sysml2Result sysml2_pipeline_process_file(
    Sysml2PipelineContext *ctx,
    const char *path,
    SysmlSemanticModel **out_model
) {
    /* Read file */
    size_t content_length;
    char *content = sysml2_read_file(path, &content_length);
    if (!content) {
        fprintf(stderr, "error: cannot read file '%s': %s\n", path, strerror(errno));
        return SYSML2_ERROR_FILE_READ;
    }

    Sysml2Result result = sysml2_pipeline_process_input(ctx, path, content, content_length, out_model);
    free(content);
    return result;
}

Sysml2Result sysml2_pipeline_process_stdin(
    Sysml2PipelineContext *ctx,
    SysmlSemanticModel **out_model
) {
    size_t content_length;
    char *content = sysml2_read_stdin(&content_length);
    if (!content) {
        fprintf(stderr, "error: failed to read from stdin\n");
        return SYSML2_ERROR_FILE_READ;
    }

    Sysml2Result result = sysml2_pipeline_process_input(ctx, "<stdin>", content, content_length, out_model);
    free(content);
    return result;
}

Sysml2Result sysml2_pipeline_resolve_all(Sysml2PipelineContext *ctx) {
    if (!ctx || ctx->options->no_resolve) {
        return SYSML2_OK;
    }

    /* Get all cached models and resolve their imports */
    size_t model_count;
    SysmlSemanticModel **models = sysml2_resolver_get_all_models(ctx->resolver, &model_count);
    if (!models || model_count == 0) {
        return SYSML2_OK;
    }

    Sysml2Result overall_result = SYSML2_OK;
    for (size_t i = 0; i < model_count; i++) {
        if (models[i]) {
            Sysml2Result result = sysml2_resolver_resolve_imports(ctx->resolver, models[i], ctx->diag);
            if (result != SYSML2_OK && overall_result == SYSML2_OK) {
                overall_result = result;
            }
        }
        if (sysml2_diag_should_stop(ctx->diag)) {
            break;
        }
    }

    free(models);
    return overall_result;
}

Sysml2Result sysml2_pipeline_validate_all(Sysml2PipelineContext *ctx) {
    if (!ctx || ctx->options->parse_only) {
        return SYSML2_OK;
    }

    /* Get all cached models */
    size_t model_count;
    SysmlSemanticModel **models = sysml2_resolver_get_all_models(ctx->resolver, &model_count);
    if (!models || model_count == 0) {
        return SYSML2_OK;
    }

    Sysml2ValidationOptions val_opts = SYSML_VALIDATION_OPTIONS_DEFAULT;
    Sysml2Result result = sysml2_validate_multi(
        models, model_count, ctx->diag, ctx->arena, ctx->intern, &val_opts
    );

    free(models);
    return result;
}

Sysml2Result sysml2_pipeline_write_json(
    Sysml2PipelineContext *ctx,
    SysmlSemanticModel *model,
    FILE *out
) {
    if (!ctx || !model || !out) {
        return SYSML2_ERROR_SEMANTIC;
    }

    Sysml2JsonOptions json_opts = SYSML_JSON_OPTIONS_DEFAULT;
    sysml2_json_write(model, out, &json_opts);
    return SYSML2_OK;
}

Sysml2Result sysml2_pipeline_write_sysml(
    Sysml2PipelineContext *ctx,
    SysmlSemanticModel *model,
    FILE *out
) {
    if (!ctx || !model || !out) {
        return SYSML2_ERROR_SEMANTIC;
    }

    sysml2_sysml_write(model, out);
    return SYSML2_OK;
}

void sysml2_pipeline_print_diagnostics(Sysml2PipelineContext *ctx, FILE *output) {
    if (!ctx || !ctx->diag) return;

    Sysml2DiagOptions diag_options = {
        .output = output,
        .color_mode = ctx->options->color_mode,
        .show_source_context = true,
        .show_column_numbers = true,
        .show_error_codes = true,
    };
    sysml2_diag_print_all(ctx->diag, &diag_options);
    sysml2_diag_print_summary(ctx->diag, output);
}

Sysml2DiagContext *sysml2_pipeline_get_diag(Sysml2PipelineContext *ctx) {
    return ctx ? ctx->diag : NULL;
}

Sysml2ImportResolver *sysml2_pipeline_get_resolver(Sysml2PipelineContext *ctx) {
    return ctx ? ctx->resolver : NULL;
}

bool sysml2_pipeline_has_errors(Sysml2PipelineContext *ctx) {
    return ctx && ctx->diag && ctx->diag->error_count > 0;
}

Sysml2Arena *sysml2_pipeline_get_arena(Sysml2PipelineContext *ctx) {
    return ctx ? ctx->arena : NULL;
}

Sysml2Result sysml2_pipeline_write_query_json(
    Sysml2PipelineContext *ctx,
    const Sysml2QueryResult *result,
    FILE *out
) {
    if (!ctx || !result || !out) {
        return SYSML2_ERROR_SEMANTIC;
    }

    Sysml2JsonOptions json_opts = SYSML_JSON_OPTIONS_DEFAULT;
    sysml2_json_write_query(result, out, &json_opts);
    return SYSML2_OK;
}

Sysml2Result sysml2_pipeline_write_query_sysml(
    Sysml2PipelineContext *ctx,
    const Sysml2QueryResult *result,
    SysmlSemanticModel **models,
    size_t model_count,
    FILE *out
) {
    if (!ctx || !result || !out) {
        return SYSML2_ERROR_SEMANTIC;
    }

    sysml2_sysml_write_query(result, models, model_count, ctx->arena, out);
    return SYSML2_OK;
}
