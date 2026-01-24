/*
 * SysML v2 Parser - SysML/KerML Source Writer Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/sysml_writer.h"
#include "sysml2/query.h"
#include <stdlib.h>
#include <string.h>

/*
 * Internal writer state
 */
typedef struct {
    FILE *out;
    int indent_level;
    bool at_line_start;
} Sysml2Writer;

/*
 * Write indentation at the current level
 */
static void write_indent(Sysml2Writer *w) {
    if (!w->at_line_start) return;
    for (int i = 0; i < w->indent_level * SYSML_WRITER_INDENT_SIZE; i++) {
        fputc(' ', w->out);
    }
    w->at_line_start = false;
}

/*
 * Write a newline
 */
static void write_newline(Sysml2Writer *w) {
    fputc('\n', w->out);
    w->at_line_start = true;
}

/*
 * Write trivia (comments and blank lines)
 */
static void write_trivia(Sysml2Writer *w, SysmlTrivia *trivia) {
    while (trivia) {
        switch (trivia->kind) {
            case SYSML_TRIVIA_LINE_COMMENT:
                write_indent(w);
                fputs("// ", w->out);
                if (trivia->text) {
                    fputs(trivia->text, w->out);
                }
                write_newline(w);
                break;

            case SYSML_TRIVIA_BLOCK_COMMENT:
                write_indent(w);
                fputs("/**", w->out);
                if (trivia->text) {
                    fputs(trivia->text, w->out);
                }
                fputs("*/", w->out);
                write_newline(w);
                break;

            case SYSML_TRIVIA_BLANK_LINE:
                write_newline(w);
                break;
        }
        trivia = trivia->next;
    }
}

/*
 * Write trailing trivia (on same line)
 */
static void write_trailing_trivia(Sysml2Writer *w, SysmlTrivia *trivia) {
    while (trivia) {
        switch (trivia->kind) {
            case SYSML_TRIVIA_LINE_COMMENT:
                fputs("  // ", w->out);
                if (trivia->text) {
                    fputs(trivia->text, w->out);
                }
                break;

            case SYSML_TRIVIA_BLOCK_COMMENT:
                fputs("  /**", w->out);
                if (trivia->text) {
                    fputs(trivia->text, w->out);
                }
                fputs("*/", w->out);
                break;

            case SYSML_TRIVIA_BLANK_LINE:
                /* Blank lines don't make sense as trailing trivia */
                break;
        }
        trivia = trivia->next;
    }
}

/*
 * Check if a name needs to be quoted (unrestricted name)
 */
static bool needs_quoting(const char *name) {
    if (!name || !*name) return false;

    /* Check first character */
    char c = name[0];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')) {
        return true;
    }

    /* Check remaining characters */
    for (const char *p = name + 1; *p; p++) {
        c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            return true;
        }
    }

    return false;
}

/*
 * Write a name, quoting if necessary
 */
static void write_name(Sysml2Writer *w, const char *name) {
    if (!name) return;

    if (needs_quoting(name)) {
        fputc('\'', w->out);
        /* Escape single quotes and backslashes */
        for (const char *p = name; *p; p++) {
            if (*p == '\'' || *p == '\\') {
                fputc('\\', w->out);
            }
            fputc(*p, w->out);
        }
        fputc('\'', w->out);
    } else {
        fputs(name, w->out);
    }
}

/*
 * Forward declarations
 */
static void write_node(Sysml2Writer *w, const SysmlNode *node, const SysmlSemanticModel *model);

/*
 * Get child nodes for a parent
 */
static size_t get_children(const SysmlSemanticModel *model, const char *parent_id,
                           const SysmlNode ***out_children) {
    /* Count children first */
    size_t count = 0;
    for (size_t i = 0; i < model->element_count; i++) {
        const SysmlNode *node = model->elements[i];
        if (parent_id == NULL && node->parent_id == NULL) {
            count++;
        } else if (parent_id && node->parent_id && strcmp(parent_id, node->parent_id) == 0) {
            count++;
        }
    }

    if (count == 0) {
        *out_children = NULL;
        return 0;
    }

    /* Allocate and collect children */
    const SysmlNode **children = malloc(count * sizeof(SysmlNode *));
    if (!children) {
        *out_children = NULL;
        return 0;
    }

    size_t idx = 0;
    for (size_t i = 0; i < model->element_count; i++) {
        const SysmlNode *node = model->elements[i];
        if (parent_id == NULL && node->parent_id == NULL) {
            children[idx++] = node;
        } else if (parent_id && node->parent_id && strcmp(parent_id, node->parent_id) == 0) {
            children[idx++] = node;
        }
    }

    *out_children = children;
    return count;
}

/*
 * Get imports for a scope
 */
static size_t get_imports(const SysmlSemanticModel *model, const char *scope_id,
                          const SysmlImport ***out_imports) {
    size_t count = 0;
    for (size_t i = 0; i < model->import_count; i++) {
        const SysmlImport *imp = model->imports[i];
        if (scope_id == NULL && imp->owner_scope == NULL) {
            count++;
        } else if (scope_id && imp->owner_scope && strcmp(scope_id, imp->owner_scope) == 0) {
            count++;
        }
    }

    if (count == 0) {
        *out_imports = NULL;
        return 0;
    }

    const SysmlImport **imports = malloc(count * sizeof(SysmlImport *));
    if (!imports) {
        *out_imports = NULL;
        return 0;
    }

    size_t idx = 0;
    for (size_t i = 0; i < model->import_count; i++) {
        const SysmlImport *imp = model->imports[i];
        if (scope_id == NULL && imp->owner_scope == NULL) {
            imports[idx++] = imp;
        } else if (scope_id && imp->owner_scope && strcmp(scope_id, imp->owner_scope) == 0) {
            imports[idx++] = imp;
        }
    }

    *out_imports = imports;
    return count;
}

/*
 * Forward declarations
 */
static void write_node(Sysml2Writer *w, const SysmlNode *node, const SysmlSemanticModel *model);
static void write_applied_metadata(Sysml2Writer *w, const SysmlNode *node);

/*
 * Write an import declaration
 */
static void write_import(Sysml2Writer *w, const SysmlImport *imp) {
    write_indent(w);
    fputs("import ", w->out);

    if (imp->target) {
        fputs(imp->target, w->out);
    }

    /* Add suffix based on kind */
    switch (imp->kind) {
        case SYSML_KIND_IMPORT_ALL:
            fputs("::*", w->out);
            break;
        case SYSML_KIND_IMPORT_RECURSIVE:
            fputs("::**", w->out);
            break;
        default:
            break;
    }

    fputc(';', w->out);
    write_newline(w);
}

/*
 * Write a body with children
 */
static void write_body(Sysml2Writer *w, const SysmlNode *node, const SysmlSemanticModel *model) {
    /* Get imports for this scope */
    const SysmlImport **imports = NULL;
    size_t import_count = get_imports(model, node->id, &imports);

    /* Get children */
    const SysmlNode **children = NULL;
    size_t child_count = get_children(model, node->id, &children);

    if (import_count == 0 && child_count == 0 && node->metadata_count == 0) {
        /* Empty body: use semicolon */
        fputc(';', w->out);
        if (node->trailing_trivia) {
            write_trailing_trivia(w, node->trailing_trivia);
        }
        write_newline(w);
    } else {
        /* Non-empty body: use braces */
        fputs(" {", w->out);
        if (node->trailing_trivia) {
            write_trailing_trivia(w, node->trailing_trivia);
        }
        write_newline(w);

        w->indent_level++;

        /* Write applied metadata first (inside the body) */
        if (node->metadata_count > 0) {
            write_applied_metadata(w, node);
            if (import_count > 0 || child_count > 0) {
                write_newline(w);
            }
        }

        /* Write imports */
        for (size_t i = 0; i < import_count; i++) {
            write_import(w, imports[i]);
        }

        /* Add blank line between imports and members if both present */
        if (import_count > 0 && child_count > 0) {
            write_newline(w);
        }

        /* Write children */
        for (size_t i = 0; i < child_count; i++) {
            write_node(w, children[i], model);
        }

        w->indent_level--;

        write_indent(w);
        fputc('}', w->out);
        write_newline(w);
    }

    free((void *)imports);
    free((void *)children);
}

/*
 * Write applied metadata (@Type { ... })
 */
static void write_applied_metadata(Sysml2Writer *w, const SysmlNode *node) {
    for (size_t i = 0; i < node->metadata_count; i++) {
        SysmlMetadataUsage *m = node->metadata[i];
        if (!m) continue;

        write_indent(w);
        fputc('@', w->out);
        fputs(m->type_ref, w->out);

        if (m->feature_count > 0) {
            fputs(" {", w->out);
            write_newline(w);
            w->indent_level++;
            for (size_t j = 0; j < m->feature_count; j++) {
                SysmlMetadataFeature *f = m->features[j];
                if (!f) continue;
                write_indent(w);
                fputs(f->name, w->out);
                if (f->value) {
                    fputs(" = ", w->out);
                    fputs(f->value, w->out);
                }
                fputc(';', w->out);
                write_newline(w);
            }
            w->indent_level--;
            write_indent(w);
            fputc('}', w->out);
        } else {
            fputc(';', w->out);
        }
        write_newline(w);
    }
}

/*
 * Write a definition or usage node
 */
static void write_node(Sysml2Writer *w, const SysmlNode *node, const SysmlSemanticModel *model) {
    if (!node) return;

    /* Write leading trivia */
    if (node->leading_trivia) {
        write_trivia(w, node->leading_trivia);
    }

    write_indent(w);

    /* Write prefix metadata before keyword */
    for (size_t i = 0; i < node->prefix_metadata_count; i++) {
        fputc('#', w->out);
        fputs(node->prefix_metadata[i], w->out);
        fputc(' ', w->out);
    }

    /* Write keyword */
    const char *keyword = sysml2_kind_to_keyword(node->kind);
    fputs(keyword, w->out);

    /* Write name if present */
    if (node->name) {
        fputc(' ', w->out);
        write_name(w, node->name);
    }

    /* Write type relationships with correct operators */
    bool first_rel = true;

    /* :> specializations first (most common for definitions) */
    for (size_t i = 0; i < node->specializes_count; i++) {
        fputs(first_rel ? " :> " : ", ", w->out);
        fputs(node->specializes[i], w->out);
        first_rel = false;
    }

    /* :>> redefinitions */
    for (size_t i = 0; i < node->redefines_count; i++) {
        fputs(first_rel ? " :>> " : ", ", w->out);
        fputs(node->redefines[i], w->out);
        first_rel = false;
    }

    /* ::> references */
    for (size_t i = 0; i < node->references_count; i++) {
        fputs(first_rel ? " ::> " : ", ", w->out);
        fputs(node->references[i], w->out);
        first_rel = false;
    }

    /* : typing last (most specific for usages) */
    for (size_t i = 0; i < node->typed_by_count; i++) {
        fputs(first_rel ? " : " : ", ", w->out);
        fputs(node->typed_by[i], w->out);
        first_rel = false;
    }

    /* Write body for container elements */
    if (SYSML_KIND_IS_PACKAGE(node->kind) ||
        SYSML_KIND_IS_DEFINITION(node->kind) ||
        SYSML_KIND_IS_USAGE(node->kind)) {
        write_body(w, node, model);
    } else {
        /* Simple element: just semicolon */
        fputc(';', w->out);
        if (node->trailing_trivia) {
            write_trailing_trivia(w, node->trailing_trivia);
        }
        write_newline(w);
    }
}

/*
 * Write the semantic model as formatted SysML/KerML source to a file
 */
Sysml2Result sysml2_sysml_write(
    const SysmlSemanticModel *model,
    FILE *out
) {
    if (!model || !out) {
        return SYSML2_ERROR_SYNTAX;
    }

    Sysml2Writer w = {
        .out = out,
        .indent_level = 0,
        .at_line_start = true
    };

    /* Get top-level imports (scope = NULL) */
    const SysmlImport **imports = NULL;
    size_t import_count = get_imports(model, NULL, &imports);

    /* Get top-level elements (parent = NULL) */
    const SysmlNode **children = NULL;
    size_t child_count = get_children(model, NULL, &children);

    /* Write top-level imports */
    for (size_t i = 0; i < import_count; i++) {
        write_import(&w, imports[i]);
    }

    /* Add blank line between imports and elements if both present */
    if (import_count > 0 && child_count > 0) {
        write_newline(&w);
    }

    /* Write top-level elements */
    for (size_t i = 0; i < child_count; i++) {
        write_node(&w, children[i], model);

        /* Add blank line between top-level elements */
        if (i + 1 < child_count) {
            write_newline(&w);
        }
    }

    free((void *)imports);
    free((void *)children);

    return SYSML2_OK;
}

/*
 * Write the semantic model as formatted SysML/KerML source to a string
 */
Sysml2Result sysml2_sysml_write_string(
    const SysmlSemanticModel *model,
    char **out_str
) {
    if (!model || !out_str) {
        return SYSML2_ERROR_SYNTAX;
    }

    /* Use memory stream */
    char *buffer = NULL;
    size_t size = 0;
    FILE *memstream = open_memstream(&buffer, &size);
    if (!memstream) {
        return SYSML2_ERROR_OUT_OF_MEMORY;
    }

    Sysml2Result result = sysml2_sysml_write(model, memstream);
    fclose(memstream);

    if (result == SYSML2_OK) {
        *out_str = buffer;
    } else {
        free(buffer);
        *out_str = NULL;
    }

    return result;
}

/*
 * Helper: Find a node by ID across multiple models
 */
static const SysmlNode *find_node_by_id(
    SysmlSemanticModel **models,
    size_t model_count,
    const char *id
) {
    if (!models || !id) return NULL;
    for (size_t m = 0; m < model_count; m++) {
        if (!models[m]) continue;
        for (size_t i = 0; i < models[m]->element_count; i++) {
            const SysmlNode *node = models[m]->elements[i];
            if (node && node->id && strcmp(node->id, id) == 0) {
                return node;
            }
        }
    }
    return NULL;
}

/*
 * Helper: Get the local name from a qualified ID
 *
 * "A::B::C" -> "C"
 */
static const char *get_local_name(const char *id) {
    if (!id) return NULL;
    const char *last_sep = NULL;
    const char *p = id;
    while (*p) {
        if (p[0] == ':' && p[1] == ':') {
            last_sep = p;
        }
        p++;
    }
    if (last_sep) {
        return last_sep + 2;
    }
    return id;
}

/*
 * Recursive function to write query result as SysML
 *
 * For a given parent path, writes all elements that are direct children.
 */
static void write_query_children(
    Sysml2Writer *w,
    const Sysml2QueryResult *result,
    SysmlSemanticModel **models,
    size_t model_count,
    const char **ancestors,
    size_t ancestor_count,
    const char *parent_path,
    size_t parent_path_len
) {
    bool first = true;

    /* Write elements that are direct children of parent_path */
    for (size_t i = 0; i < result->element_count; i++) {
        const SysmlNode *node = result->elements[i];
        if (!node || !node->id) continue;

        /* Check if this node's parent matches parent_path */
        const char *last_sep = NULL;
        const char *p = node->id;
        while (*p) {
            if (p[0] == ':' && p[1] == ':') {
                last_sep = p;
            }
            p++;
        }

        bool is_direct_child;
        if (parent_path == NULL) {
            /* Top-level: no :: in the ID */
            is_direct_child = (last_sep == NULL);
        } else {
            /* Check if parent matches exactly */
            if (!last_sep) {
                is_direct_child = false;
            } else {
                size_t parent_len = last_sep - node->id;
                is_direct_child = (parent_len == parent_path_len &&
                                   strncmp(node->id, parent_path, parent_len) == 0);
            }
        }

        if (is_direct_child) {
            if (!first) {
                write_newline(w);
            }
            first = false;
            write_node(w, node, models[0]);  /* Use first model for structure */
        }
    }

    /* Write ancestor stubs that are direct children of parent_path */
    for (size_t i = 0; i < ancestor_count; i++) {
        const char *anc_id = ancestors[i];
        if (!anc_id) continue;

        /* Check if this ancestor's parent matches parent_path */
        const char *last_sep = NULL;
        const char *p = anc_id;
        while (*p) {
            if (p[0] == ':' && p[1] == ':') {
                last_sep = p;
            }
            p++;
        }

        bool is_direct_child;
        if (parent_path == NULL) {
            is_direct_child = (last_sep == NULL);
        } else {
            if (!last_sep) {
                is_direct_child = false;
            } else {
                size_t parent_len = last_sep - anc_id;
                is_direct_child = (parent_len == parent_path_len &&
                                   strncmp(anc_id, parent_path, parent_len) == 0);
            }
        }

        if (is_direct_child) {
            if (!first) {
                write_newline(w);
            }
            first = false;

            /* Write the ancestor as a stub package */
            const SysmlNode *anc_node = find_node_by_id(models, model_count, anc_id);
            const char *local_name = get_local_name(anc_id);

            write_indent(w);

            /* Determine keyword based on node kind or default to package */
            const char *keyword = "package";
            if (anc_node) {
                keyword = sysml2_kind_to_keyword(anc_node->kind);
            }
            fputs(keyword, w->out);

            if (local_name) {
                fputc(' ', w->out);
                if (needs_quoting(local_name)) {
                    fputc('\'', w->out);
                    fputs(local_name, w->out);
                    fputc('\'', w->out);
                } else {
                    fputs(local_name, w->out);
                }
            }

            fputs(" {", w->out);
            write_newline(w);
            w->indent_level++;

            /* Recursively write children of this ancestor */
            write_query_children(
                w, result, models, model_count,
                ancestors, ancestor_count,
                anc_id, strlen(anc_id)
            );

            w->indent_level--;
            write_indent(w);
            fputc('}', w->out);
            write_newline(w);
        }
    }
}

/*
 * Write a query result as formatted SysML/KerML source to a file
 */
Sysml2Result sysml2_sysml_write_query(
    const Sysml2QueryResult *result,
    SysmlSemanticModel **models,
    size_t model_count,
    Sysml2Arena *arena,
    FILE *out
) {
    if (!result || !out) {
        return SYSML2_ERROR_SYNTAX;
    }

    /* Get ancestors needed for valid output */
    const char **ancestors = NULL;
    size_t ancestor_count = 0;
    sysml2_query_get_ancestors(result, models, model_count, arena, &ancestors, &ancestor_count);

    Sysml2Writer w = {
        .out = out,
        .indent_level = 0,
        .at_line_start = true
    };

    /* Write hierarchical output starting from root (NULL parent) */
    write_query_children(
        &w, result, models, model_count,
        ancestors, ancestor_count,
        NULL, 0
    );

    return SYSML2_OK;
}
