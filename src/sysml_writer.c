/*
 * SysML v2 Parser - SysML/KerML Source Writer Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#include "sysml2/sysml_writer.h"
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

    if (import_count == 0 && child_count == 0) {
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

        /* Write imports first */
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
 * Write a definition or usage node
 */
static void write_node(Sysml2Writer *w, const SysmlNode *node, const SysmlSemanticModel *model) {
    if (!node) return;

    /* Write leading trivia */
    if (node->leading_trivia) {
        write_trivia(w, node->leading_trivia);
    }

    write_indent(w);

    /* Write keyword */
    const char *keyword = sysml2_kind_to_keyword(node->kind);
    fputs(keyword, w->out);

    /* Write name if present */
    if (node->name) {
        fputc(' ', w->out);
        write_name(w, node->name);
    }

    /* Write type specializations */
    if (node->typed_by && node->typed_by_count > 0) {
        fputs(" : ", w->out);
        for (size_t i = 0; i < node->typed_by_count; i++) {
            if (i > 0) {
                fputs(", ", w->out);
            }
            fputs(node->typed_by[i], w->out);
        }
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
