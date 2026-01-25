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
 * Comparison function for sorting imports alphabetically by target
 */
static int compare_imports(const void *a, const void *b) {
    const SysmlImport *imp_a = *(const SysmlImport **)a;
    const SysmlImport *imp_b = *(const SysmlImport **)b;

    const char *target_a = imp_a->target ? imp_a->target : "";
    const char *target_b = imp_b->target ? imp_b->target : "";

    return strcmp(target_a, target_b);
}

/*
 * Comparison function for sorting aliases alphabetically by name
 */
static int compare_aliases(const void *a, const void *b) {
    const SysmlAlias *alias_a = *(const SysmlAlias **)a;
    const SysmlAlias *alias_b = *(const SysmlAlias **)b;

    const char *name_a = alias_a->name ? alias_a->name : "";
    const char *name_b = alias_b->name ? alias_b->name : "";

    return strcmp(name_a, name_b);
}


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

    /* Sort imports alphabetically by target */
    if (count > 1) {
        qsort(imports, count, sizeof(SysmlImport *), compare_imports);
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

    /* Write visibility */
    if (imp->is_private) {
        fputs("private ", w->out);
    } else if (imp->is_public_explicit) {
        fputs("public ", w->out);
    }

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
 * Write a body statement
 */
static void write_statement(Sysml2Writer *w, const SysmlStatement *stmt) {
    if (!stmt) return;

    write_indent(w);

    switch (stmt->kind) {
        case SYSML_STMT_BIND:
            fputs("bind ", w->out);
            if (stmt->source.target) {
                fputs(stmt->source.target, w->out);
            }
            fputs(" = ", w->out);
            if (stmt->target.target) {
                fputs(stmt->target.target, w->out);
            }
            fputs(";", w->out);
            break;

        case SYSML_STMT_CONNECT:
            fputs("connect ", w->out);
            if (stmt->source.target) {
                fputs(stmt->source.target, w->out);
            }
            fputs(" to ", w->out);
            if (stmt->target.target) {
                fputs(stmt->target.target, w->out);
            }
            fputs(";", w->out);
            break;

        case SYSML_STMT_FLOW:
            fputs("flow ", w->out);
            if (stmt->payload) {
                fputs("of ", w->out);
                fputs(stmt->payload, w->out);
                fputc(' ', w->out);
            }
            fputs("from ", w->out);
            if (stmt->source.target) {
                fputs(stmt->source.target, w->out);
            }
            fputs(" to ", w->out);
            if (stmt->target.target) {
                fputs(stmt->target.target, w->out);
            }
            fputs(";", w->out);
            break;

        case SYSML_STMT_ALLOCATE:
            fputs("allocate ", w->out);
            if (stmt->source.target) {
                fputs(stmt->source.target, w->out);
            }
            fputs(" to ", w->out);
            if (stmt->target.target) {
                fputs(stmt->target.target, w->out);
            }
            fputs(";", w->out);
            break;

        case SYSML_STMT_SUCCESSION:
            fputs("first ", w->out);
            if (stmt->source.target) {
                fputs(stmt->source.target, w->out);
            }
            if (stmt->guard) {
                fputs(" if ", w->out);
                fputs(stmt->guard, w->out);
            }
            fputs(" then ", w->out);
            if (stmt->target.target) {
                fputs(stmt->target.target, w->out);
            }
            fputs(";", w->out);
            break;

        case SYSML_STMT_ENTRY:
            fputs("entry ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_EXIT:
            fputs("exit ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_DO:
            fputs("do ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_TRANSITION:
            fputs("transition ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_ACCEPT:
            fputs("accept ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_SEND:
            fputs("send ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            fputs(";", w->out);
            break;

        case SYSML_STMT_ACCEPT_ACTION:
            fputs("accept ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_ASSIGN:
            fputs("assign ", w->out);
            if (stmt->target.target) {
                fputs(stmt->target.target, w->out);
            }
            fputs(" := ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            fputs(";", w->out);
            break;

        case SYSML_STMT_IF:
            fputs("if ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_WHILE:
            fputs("while ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_FOR:
            fputs("for ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_LOOP:
            fputs("loop ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_TERMINATE:
            fputs("terminate;", w->out);
            break;

        case SYSML_STMT_MERGE:
            fputs("merge ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_DECIDE:
            fputs("decide ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_JOIN:
            fputs("join ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_FORK:
            fputs("fork ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_FIRST:
            fputs("first ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_THEN:
            /* Raw text includes keyword, output as-is */
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_RESULT_EXPR:
            /* Just the expression, no keyword */
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_METADATA_USAGE:
            /* metadata X about Y, Z; */
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            fputs(";", w->out);
            break;

        case SYSML_STMT_SHORTHAND_FEATURE:
            /* :> name : Type; or :>> name = value; */
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_REQUIRE_CONSTRAINT:
            fputs("require ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_ASSUME_CONSTRAINT:
            fputs("assume ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_SUBJECT:
            fputs("subject ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_END_MEMBER:
            fputs("end ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_RETURN:
            fputs("return ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_ACTOR:
            fputs("actor ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_STAKEHOLDER:
            fputs("stakeholder ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_OBJECTIVE:
            fputs("objective ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_FRAME:
            fputs("frame ", w->out);
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        default:
            /* Unknown statement - write raw_text if available */
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;
    }

    write_newline(w);
}

/*
 * Write a named comment
 */
static void write_named_comment(Sysml2Writer *w, const SysmlNamedComment *comment) {
    if (!comment) return;

    write_indent(w);
    fputs("comment", w->out);

    if (comment->name) {
        fputc(' ', w->out);
        write_name(w, comment->name);
    }

    if (comment->about_count > 0 && comment->about) {
        fputs(" about ", w->out);
        for (size_t i = 0; i < comment->about_count; i++) {
            if (i > 0) fputs(", ", w->out);
            fputs(comment->about[i], w->out);
        }
    }

    if (comment->locale) {
        fputs(" locale ", w->out);
        fputs(comment->locale, w->out);
    }

    fputc(' ', w->out);
    if (comment->text) {
        fputs(comment->text, w->out);
    }

    write_newline(w);
}

/*
 * Write a textual representation
 */
static void write_textual_rep(Sysml2Writer *w, const SysmlTextualRep *rep) {
    if (!rep) return;

    write_indent(w);
    fputs("rep", w->out);

    if (rep->name) {
        fputc(' ', w->out);
        write_name(w, rep->name);
    }

    fputs(" language ", w->out);
    if (rep->language) {
        fputs(rep->language, w->out);
    }

    fputc(' ', w->out);
    if (rep->text) {
        fputs(rep->text, w->out);
    }

    write_newline(w);
}

/*
 * Write an alias declaration
 */
static void write_alias(Sysml2Writer *w, const SysmlAlias *alias) {
    write_indent(w);
    fputs("alias ", w->out);

    if (alias->name) {
        write_name(w, alias->name);
    }

    fputs(" for ", w->out);

    if (alias->target) {
        fputs(alias->target, w->out);
    }

    fputc(';', w->out);
    write_newline(w);
}

/*
 * Get aliases for a scope
 */
static size_t get_aliases(const SysmlSemanticModel *model, const char *scope_id,
                          const SysmlAlias ***out_aliases) {
    size_t count = 0;
    for (size_t i = 0; i < model->alias_count; i++) {
        const SysmlAlias *alias = model->aliases[i];
        if (scope_id == NULL && alias->owner_scope == NULL) {
            count++;
        } else if (scope_id && alias->owner_scope && strcmp(scope_id, alias->owner_scope) == 0) {
            count++;
        }
    }

    if (count == 0) {
        *out_aliases = NULL;
        return 0;
    }

    const SysmlAlias **aliases = malloc(count * sizeof(SysmlAlias *));
    if (!aliases) {
        *out_aliases = NULL;
        return 0;
    }

    size_t idx = 0;
    for (size_t i = 0; i < model->alias_count; i++) {
        const SysmlAlias *alias = model->aliases[i];
        if (scope_id == NULL && alias->owner_scope == NULL) {
            aliases[idx++] = alias;
        } else if (scope_id && alias->owner_scope && strcmp(scope_id, alias->owner_scope) == 0) {
            aliases[idx++] = alias;
        }
    }

    /* Sort aliases alphabetically by name */
    if (count > 1) {
        qsort(aliases, count, sizeof(SysmlAlias *), compare_aliases);
    }

    *out_aliases = aliases;
    return count;
}

/*
 * Write a body with children
 */
static void write_body(Sysml2Writer *w, const SysmlNode *node, const SysmlSemanticModel *model) {
    /* Get imports for this scope */
    const SysmlImport **imports = NULL;
    size_t import_count = get_imports(model, node->id, &imports);

    /* Get aliases for this scope */
    const SysmlAlias **aliases = NULL;
    size_t alias_count = get_aliases(model, node->id, &aliases);

    /* Get children */
    const SysmlNode **children = NULL;
    size_t child_count = get_children(model, node->id, &children);

    bool has_doc = (node->documentation != NULL);
    bool has_body_stmts = (node->body_stmt_count > 0);
    bool has_comments = (node->comment_count > 0);
    bool has_reps = (node->textual_rep_count > 0);
    bool has_result_expr = (node->result_expression != NULL);

    if (import_count == 0 && alias_count == 0 && child_count == 0 &&
        node->metadata_count == 0 && !has_doc && !has_body_stmts &&
        !has_comments && !has_reps && !has_result_expr) {
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

        /* Write documentation first if present */
        if (has_doc) {
            write_indent(w);
            fputs("doc ", w->out);
            fputs(node->documentation, w->out);
            write_newline(w);
            if (node->metadata_count > 0 || import_count > 0 || alias_count > 0 || child_count > 0) {
                write_newline(w);
            }
        }

        /* Write applied metadata (inside the body) */
        if (node->metadata_count > 0) {
            write_applied_metadata(w, node);
            if (import_count > 0 || alias_count > 0 || child_count > 0) {
                write_newline(w);
            }
        }

        /* Write imports */
        for (size_t i = 0; i < import_count; i++) {
            write_import(w, imports[i]);
        }

        /* Write aliases */
        for (size_t i = 0; i < alias_count; i++) {
            write_alias(w, aliases[i]);
        }

        /* Add blank line between imports/aliases and members if both present */
        if ((import_count > 0 || alias_count > 0) && (child_count > 0 || has_body_stmts)) {
            write_newline(w);
        }

        /* Write body statements (relationship statements, control flow, etc.) */
        for (size_t i = 0; i < node->body_stmt_count; i++) {
            write_statement(w, node->body_stmts[i]);
        }

        /* Write children */
        for (size_t i = 0; i < child_count; i++) {
            write_node(w, children[i], model);
        }

        /* Write named comments */
        for (size_t i = 0; i < node->comment_count; i++) {
            write_named_comment(w, node->comments[i]);
        }

        /* Write textual representations */
        for (size_t i = 0; i < node->textual_rep_count; i++) {
            write_textual_rep(w, node->textual_reps[i]);
        }

        /* Write result expression (at end) */
        if (node->result_expression) {
            write_indent(w);
            fputs(node->result_expression, w->out);
            write_newline(w);
        }

        w->indent_level--;

        write_indent(w);
        fputc('}', w->out);
        write_newline(w);
    }

    free((void *)imports);
    free((void *)aliases);
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
                /* Use :>> syntax for metadata attribute redefinitions */
                fputs(":>> ", w->out);
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

    /* Write prefix applied metadata (@Type {...}) before element */
    for (size_t i = 0; i < node->prefix_applied_metadata_count; i++) {
        SysmlMetadataUsage *m = node->prefix_applied_metadata[i];
        if (!m) continue;

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
                /* Use :>> syntax for metadata attribute redefinitions */
                fputs(":>> ", w->out);
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
        write_indent(w);
    }

    /* Write prefix metadata before keyword (#Type) */
    for (size_t i = 0; i < node->prefix_metadata_count; i++) {
        fputc('#', w->out);
        fputs(node->prefix_metadata[i], w->out);
        fputc(' ', w->out);
    }

    /* Write direction for parameters (in/out/inout) */
    switch (node->direction) {
        case SYSML_DIR_IN:
            fputs("in ", w->out);
            break;
        case SYSML_DIR_OUT:
            fputs("out ", w->out);
            break;
        case SYSML_DIR_INOUT:
            fputs("inout ", w->out);
            break;
        default:
            break;
    }

    /* Write abstract modifier */
    if (node->is_abstract) {
        fputs("abstract ", w->out);
    }

    /* Write variation modifier */
    if (node->is_variation) {
        fputs("variation ", w->out);
    }

    /* Write ref modifier */
    if (node->is_ref) {
        fputs("ref ", w->out);
    }

    /* Write keyword */
    const char *keyword = sysml2_kind_to_keyword(node->kind);
    bool has_keyword = keyword && keyword[0];
    if (has_keyword) {
        fputs(keyword, w->out);
    }

    /* For end features, multiplicity comes right after keyword */
    bool end_feature_mult_written = false;
    if (node->kind == SYSML_KIND_END_FEATURE && node->multiplicity_lower) {
        fputs(" [", w->out);
        fputs(node->multiplicity_lower, w->out);
        if (node->multiplicity_upper) {
            fputs("..", w->out);
            fputs(node->multiplicity_upper, w->out);
        }
        fputc(']', w->out);
        end_feature_mult_written = true;
    }

    /* Write name if present */
    if (node->name) {
        /* Only add space before name if there was a keyword
           (direction/abstract/variation already added their own trailing space) */
        if (has_keyword) {
            fputc(' ', w->out);
        }
        write_name(w, node->name);
    }

    /* Write parameter list if present (for action/state definitions) */
    if (node->parameter_list) {
        fputs(node->parameter_list, w->out);
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
    /* End features use compact format (no spaces around colon) */
    for (size_t i = 0; i < node->typed_by_count; i++) {
        if (node->kind == SYSML_KIND_END_FEATURE) {
            fputs(first_rel ? ":" : ", ", w->out);
        } else {
            fputs(first_rel ? " : " : ", ", w->out);
        }
        fputs(node->typed_by[i], w->out);
        first_rel = false;
    }

    /* Write multiplicity (skip for end features - already written after keyword) */
    if (node->multiplicity_lower && !end_feature_mult_written) {
        fputc('[', w->out);
        fputs(node->multiplicity_lower, w->out);
        if (node->multiplicity_upper) {
            fputs("..", w->out);
            fputs(node->multiplicity_upper, w->out);
        }
        fputc(']', w->out);
    }

    /* Write default value */
    if (node->default_value) {
        if (node->has_default_keyword) {
            fputs(" default", w->out);
        }
        fputs(" = ", w->out);
        fputs(node->default_value, w->out);
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

    /* Get top-level aliases (scope = NULL) */
    const SysmlAlias **aliases = NULL;
    size_t alias_count = get_aliases(model, NULL, &aliases);

    /* Get top-level elements (parent = NULL) */
    const SysmlNode **children = NULL;
    size_t child_count = get_children(model, NULL, &children);

    /* Write top-level imports */
    for (size_t i = 0; i < import_count; i++) {
        write_import(&w, imports[i]);
    }

    /* Write top-level aliases */
    for (size_t i = 0; i < alias_count; i++) {
        write_alias(&w, aliases[i]);
    }

    /* Add blank line between imports/aliases and elements if both present */
    if ((import_count > 0 || alias_count > 0) && child_count > 0) {
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
    free((void *)aliases);
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
