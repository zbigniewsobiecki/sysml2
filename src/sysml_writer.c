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
 * Body element kinds for unified sorting
 */
typedef enum {
    BODY_ELEM_DOC,
    BODY_ELEM_METADATA,
    BODY_ELEM_IMPORT,
    BODY_ELEM_ALIAS,
    BODY_ELEM_STATEMENT,
    BODY_ELEM_CHILD,
    BODY_ELEM_COMMENT,
    BODY_ELEM_TEXTUAL_REP,
} BodyElementKind;

/*
 * Unified body element for source-order sorting
 */
typedef struct {
    BodyElementKind kind;
    uint32_t offset;           /* loc.offset for sorting */
    size_t insertion_order;    /* For stable sort when offset=0 */
    union {
        const char *doc_text;
        SysmlMetadataUsage *metadata;
        const SysmlImport *import;
        const SysmlAlias *alias;
        SysmlStatement *statement;
        const SysmlNode *child;
        SysmlNamedComment *comment;
        SysmlTextualRep *textual_rep;
    } data;
} BodyElement;

/*
 * Comparison: valid offsets first (ascending), offset=0 at end
 */
static int compare_body_elements(const void *a, const void *b) {
    const BodyElement *ea = a, *eb = b;

    if (ea->offset > 0 && eb->offset > 0) {
        if (ea->offset != eb->offset)
            return (ea->offset < eb->offset) ? -1 : 1;
    } else if (ea->offset > 0) return -1;
    else if (eb->offset > 0) return 1;

    /* Same/zero offset: preserve insertion order */
    return (ea->insertion_order < eb->insertion_order) ? -1 :
           (ea->insertion_order > eb->insertion_order) ? 1 : 0;
}

/*
 * Comparison: sort top-level nodes by source position
 * Elements with no position (loc.offset == 0) go last
 */
static int compare_nodes_by_position(const void *a, const void *b) {
    const SysmlNode *na = *(const SysmlNode **)a;
    const SysmlNode *nb = *(const SysmlNode **)b;

    /* Elements with no position go last (new elements from fragments) */
    if (na->loc.offset == 0 && nb->loc.offset == 0) return 0;
    if (na->loc.offset == 0) return 1;  /* a goes after b */
    if (nb->loc.offset == 0) return -1; /* b goes after a */

    return (na->loc.offset < nb->loc.offset) ? -1 :
           (na->loc.offset > nb->loc.offset) ? 1 : 0;
}

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

            case SYSML_TRIVIA_REGULAR_COMMENT:
                write_indent(w);
                fputs("/*", w->out);
                if (trivia->text) {
                    fputs(trivia->text, w->out);
                }
                fputs("*/", w->out);
                write_newline(w);
                break;

            case SYSML_TRIVIA_BLANK_LINE:
                /* Output count blank lines (or 1 if count is 0) */
                for (int i = 0; i < (trivia->count > 0 ? trivia->count : 1); i++) {
                    write_newline(w);
                }
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

            case SYSML_TRIVIA_REGULAR_COMMENT:
                fputs("  /*", w->out);
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

/* compare_aliases removed - aliases now preserve source order */


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

    /* Preserve original import order instead of sorting alphabetically.
     * Sorting was causing imports to be reordered during upsert operations. */

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
            /* Skip malformed successions with no source or target */
            if (!stmt->source.target && !stmt->target.target) {
                return;  /* Don't write anything, skip newline too */
            }
            fputs("first ", w->out);
            if (stmt->source.target) {
                /* Check if source already contains " then " (parser captured full succession) */
                const char *then_pos = strstr(stmt->source.target, " then ");
                if (then_pos && !stmt->target.target) {
                    /* Source contains full succession, write as-is without adding another "then" */
                    fputs(stmt->source.target, w->out);
                    /* Already has semicolon if source ends with it */
                    if (stmt->source.target[strlen(stmt->source.target) - 1] != ';') {
                        fputs(";", w->out);
                    }
                    break;
                }
                fputs(stmt->source.target, w->out);
            }
            if (stmt->guard) {
                fputs(" if ", w->out);
                fputs(stmt->guard, w->out);
            }
            if (stmt->target.target) {
                fputs(" then ", w->out);
                fputs(stmt->target.target, w->out);
                fputs(";", w->out);
            } else {
                /* No target - check if source already ends with semicolon */
                if (!stmt->source.target || stmt->source.target[strlen(stmt->source.target) - 1] != ';') {
                    fputs(";", w->out);
                }
            }
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
            /* Skip malformed first statements with no content */
            if (!stmt->raw_text || stmt->raw_text[0] == '\0') {
                return;
            }
            fputs("first ", w->out);
            fputs(stmt->raw_text, w->out);
            break;

        case SYSML_STMT_THEN:
            /* Skip malformed then statements (e.g., "then ;" or "then ;;") */
            if (!stmt->raw_text || stmt->raw_text[0] == '\0') {
                return;
            }
            /* Check for malformed pattern: "then" followed by only whitespace/semicolons */
            {
                const char *p = stmt->raw_text;
                /* Skip leading whitespace */
                while (*p == ' ' || *p == '\t') p++;
                /* Skip optional "then" keyword */
                if (strncmp(p, "then", 4) == 0) p += 4;
                /* Skip whitespace */
                while (*p == ' ' || *p == '\t') p++;
                /* If only semicolons or nothing left, skip this statement */
                if (*p == '\0' || *p == ';') {
                    return;
                }
            }
            fputs(stmt->raw_text, w->out);
            break;

        case SYSML_STMT_RESULT_EXPR:
            /* Just the expression, no keyword */
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_METADATA_USAGE:
            /* metadata X about Y, Z; or metadata X { ... } */
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
                /* Only add semicolon if not already ending with } or ; */
                size_t len = strlen(stmt->raw_text);
                if (len > 0 && stmt->raw_text[len-1] != '}' && stmt->raw_text[len-1] != ';') {
                    fputs(";", w->out);
                }
            }
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

        case SYSML_STMT_SATISFY:
            /* raw_text already includes the full statement */
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_INCLUDE_USE_CASE:
            /* raw_text already includes the full statement */
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_EXPOSE:
            /* raw_text already includes the full statement */
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_RENDER:
            /* raw_text already includes the full statement */
            if (stmt->raw_text) {
                fputs(stmt->raw_text, w->out);
            }
            break;

        case SYSML_STMT_VERIFY:
            /* raw_text already includes the full statement */
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

    /* Preserve original alias order instead of sorting alphabetically.
     * Sorting was causing aliases to be reordered during upsert operations. */

    *out_aliases = aliases;
    return count;
}

/*
 * Collect all body elements into unified array for source-order sorting
 */
static BodyElement *collect_body_elements(
    const SysmlNode *node,
    const SysmlSemanticModel *model,
    size_t *out_count
) {
    /* Get imports for this scope */
    const SysmlImport **imports = NULL;
    size_t import_count = get_imports(model, node->id, &imports);

    /* Get aliases for this scope */
    const SysmlAlias **aliases = NULL;
    size_t alias_count = get_aliases(model, node->id, &aliases);

    /* Get children */
    const SysmlNode **children = NULL;
    size_t child_count = get_children(model, node->id, &children);

    /* Count total elements */
    size_t total = 0;
    if (node->documentation) total++;
    total += node->metadata_count;
    total += import_count;
    total += alias_count;
    total += node->body_stmt_count;
    total += child_count;
    total += node->comment_count;
    total += node->textual_rep_count;

    if (total == 0) {
        free((void *)imports);
        free((void *)aliases);
        free((void *)children);
        *out_count = 0;
        return NULL;
    }

    /* Allocate elements array */
    BodyElement *elements = malloc(total * sizeof(BodyElement));
    if (!elements) {
        free((void *)imports);
        free((void *)aliases);
        free((void *)children);
        *out_count = 0;
        return NULL;
    }

    size_t idx = 0;
    size_t insertion_order = 0;

    /* Add documentation */
    if (node->documentation) {
        elements[idx].kind = BODY_ELEM_DOC;
        elements[idx].offset = node->doc_loc.offset;
        elements[idx].insertion_order = insertion_order++;
        elements[idx].data.doc_text = node->documentation;
        idx++;
    }

    /* Add metadata */
    for (size_t i = 0; i < node->metadata_count; i++) {
        elements[idx].kind = BODY_ELEM_METADATA;
        elements[idx].offset = node->metadata[i]->loc.offset;
        elements[idx].insertion_order = insertion_order++;
        elements[idx].data.metadata = node->metadata[i];
        idx++;
    }

    /* Add imports */
    for (size_t i = 0; i < import_count; i++) {
        elements[idx].kind = BODY_ELEM_IMPORT;
        elements[idx].offset = imports[i]->loc.offset;
        elements[idx].insertion_order = insertion_order++;
        elements[idx].data.import = imports[i];
        idx++;
    }

    /* Add aliases */
    for (size_t i = 0; i < alias_count; i++) {
        elements[idx].kind = BODY_ELEM_ALIAS;
        elements[idx].offset = aliases[i]->loc.offset;
        elements[idx].insertion_order = insertion_order++;
        elements[idx].data.alias = aliases[i];
        idx++;
    }

    /* Add body statements */
    for (size_t i = 0; i < node->body_stmt_count; i++) {
        elements[idx].kind = BODY_ELEM_STATEMENT;
        elements[idx].offset = node->body_stmts[i]->loc.offset;
        elements[idx].insertion_order = insertion_order++;
        elements[idx].data.statement = node->body_stmts[i];
        idx++;
    }

    /* Add children */
    for (size_t i = 0; i < child_count; i++) {
        elements[idx].kind = BODY_ELEM_CHILD;
        elements[idx].offset = children[i]->loc.offset;
        elements[idx].insertion_order = insertion_order++;
        elements[idx].data.child = children[i];
        idx++;
    }

    /* Add named comments */
    for (size_t i = 0; i < node->comment_count; i++) {
        elements[idx].kind = BODY_ELEM_COMMENT;
        elements[idx].offset = node->comments[i]->loc.offset;
        elements[idx].insertion_order = insertion_order++;
        elements[idx].data.comment = node->comments[i];
        idx++;
    }

    /* Add textual representations */
    for (size_t i = 0; i < node->textual_rep_count; i++) {
        elements[idx].kind = BODY_ELEM_TEXTUAL_REP;
        elements[idx].offset = node->textual_reps[i]->loc.offset;
        elements[idx].insertion_order = insertion_order++;
        elements[idx].data.textual_rep = node->textual_reps[i];
        idx++;
    }

    free((void *)imports);
    free((void *)aliases);
    free((void *)children);

    *out_count = idx;
    return elements;
}

/*
 * Write single body element based on kind
 */
static void write_body_element(
    Sysml2Writer *w,
    const BodyElement *elem,
    const SysmlSemanticModel *model
) {
    switch (elem->kind) {
        case BODY_ELEM_DOC:
            write_indent(w);
            fputs("doc ", w->out);
            fputs(elem->data.doc_text, w->out);
            write_newline(w);
            break;

        case BODY_ELEM_METADATA: {
            SysmlMetadataUsage *m = elem->data.metadata;
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
            break;
        }

        case BODY_ELEM_IMPORT:
            write_import(w, elem->data.import);
            break;

        case BODY_ELEM_ALIAS:
            write_alias(w, elem->data.alias);
            break;

        case BODY_ELEM_STATEMENT:
            write_statement(w, elem->data.statement);
            break;

        case BODY_ELEM_CHILD:
            write_node(w, elem->data.child, model);
            break;

        case BODY_ELEM_COMMENT:
            write_named_comment(w, elem->data.comment);
            break;

        case BODY_ELEM_TEXTUAL_REP:
            write_textual_rep(w, elem->data.textual_rep);
            break;
    }
}

/*
 * Write a body with children - source-ordered implementation
 */
static void write_body(Sysml2Writer *w, const SysmlNode *node, const SysmlSemanticModel *model) {
    size_t count = 0;
    BodyElement *elements = collect_body_elements(node, model, &count);

    bool has_result = (node->result_expression != NULL);

    if (count == 0 && !has_result) {
        /* Empty body: use semicolon */
        fputc(';', w->out);
        if (node->trailing_trivia) {
            write_trailing_trivia(w, node->trailing_trivia);
        }
        write_newline(w);
        return;
    }

    /* Non-empty body: use braces */
    fputs(" {", w->out);
    write_newline(w);
    w->indent_level++;

    /* Sort by source position */
    if (count > 1) {
        qsort(elements, count, sizeof(BodyElement), compare_body_elements);
    }

    /* Write in sorted order */
    for (size_t i = 0; i < count; i++) {
        write_body_element(w, &elements[i], model);
    }

    /* Result expression always last (semantic requirement for calc/constraint bodies) */
    if (has_result) {
        write_indent(w);
        fputs(node->result_expression, w->out);
        write_newline(w);
    }

    /* Write trailing trivia (comments at end of body before closing brace) */
    if (node->trailing_trivia) {
        for (SysmlTrivia *t = node->trailing_trivia; t; t = t->next) {
            if (t->kind == SYSML_TRIVIA_BLANK_LINE) {
                /* Output count blank lines (or 1 if count is 0) */
                for (int i = 0; i < (t->count > 0 ? t->count : 1); i++) {
                    write_newline(w);
                }
            } else if (t->kind == SYSML_TRIVIA_LINE_COMMENT) {
                write_indent(w);
                fputs("// ", w->out);
                if (t->text) fputs(t->text, w->out);
                write_newline(w);
            } else if (t->kind == SYSML_TRIVIA_BLOCK_COMMENT) {
                write_indent(w);
                fputs("/**", w->out);
                if (t->text) fputs(t->text, w->out);
                fputs("*/", w->out);
                write_newline(w);
            } else if (t->kind == SYSML_TRIVIA_REGULAR_COMMENT) {
                write_indent(w);
                fputs("/*", w->out);
                if (t->text) fputs(t->text, w->out);
                fputs("*/", w->out);
                write_newline(w);
            }
        }
    }

    w->indent_level--;
    write_indent(w);
    fputc('}', w->out);
    write_newline(w);

    free(elements);
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

    /* Write visibility modifier (public/private/protected) */
    switch (node->visibility) {
        case SYSML_VIS_PRIVATE:
            fputs("private ", w->out);
            break;
        case SYSML_VIS_PROTECTED:
            fputs("protected ", w->out);
            break;
        case SYSML_VIS_PUBLIC:
            /* Public is the default, only write if explicitly marked */
            if (node->is_public_explicit) {
                fputs("public ", w->out);
            }
            break;
        default:
            break;
    }

    /* Write prefix metadata before keyword (#Type) */
    for (size_t i = 0; i < node->prefix_metadata_count; i++) {
        fputc('#', w->out);
        fputs(node->prefix_metadata[i], w->out);
        fputc(' ', w->out);
    }

    /* Write direction for parameters/usages (in/out/inout).
     * Definitions (port def, part def, etc.) should NOT have direction. */
    if (!SYSML_KIND_IS_DEFINITION(node->kind)) {
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
    }

    /* Write assert modifier (for asserted constraints) */
    if (node->is_asserted) {
        fputs("assert ", w->out);
        if (node->is_negated) {
            fputs("not ", w->out);
        }
    }

    /* Write abstract modifier */
    if (node->is_abstract) {
        fputs("abstract ", w->out);
    }

    /* Write variation modifier */
    if (node->is_variation) {
        fputs("variation ", w->out);
    }

    /* Write parallel modifier (for states) */
    if (node->is_parallel && node->kind == SYSML_KIND_STATE_USAGE) {
        fputs("parallel ", w->out);
    }

    /* Write attribute prefixes */
    if (node->is_readonly) {
        fputs("readonly ", w->out);
    }
    if (node->is_derived) {
        fputs("derived ", w->out);
    }
    if (node->is_constant) {
        fputs("constant ", w->out);
    }

    /* Write ref modifier */
    if (node->is_ref) {
        fputs("ref ", w->out);
        if (node->ref_behavioral_keyword) {
            fputs(node->ref_behavioral_keyword, w->out);
            fputc(' ', w->out);
        }
    }

    /* Write end modifier */
    if (node->is_end) {
        fputs("end ", w->out);
    }

    /* Write exhibit modifier (for state usages) */
    if (node->is_exhibit && node->kind == SYSML_KIND_STATE_USAGE) {
        fputs("exhibit ", w->out);
    }

    /* Write keyword */
    const char *keyword;
    /* Handle event occurrence: use "event occurrence" instead of just "event" */
    if (node->kind == SYSML_KIND_EVENT_USAGE && node->is_event_occurrence) {
        keyword = "event occurrence";
    } else if (node->kind == SYSML_KIND_PORTION_USAGE && node->portion_kind) {
        /* Use snapshot/timeslice instead of generic "portion" */
        keyword = node->portion_kind;
    } else if (node->kind == SYSML_KIND_PERFORM_ACTION_USAGE && node->has_action_keyword) {
        /* Use "perform action" when action keyword was present, otherwise just "perform" */
        keyword = "perform action";
    } else if (node->ref_behavioral_keyword) {
        /* If ref behavioral keyword is set, it replaces the kind keyword */
        keyword = NULL;
    } else {
        keyword = sysml2_kind_to_keyword(node->kind);
    }
    bool has_keyword = keyword && keyword[0];

    /* Enum literals (ENUMERATION_USAGE) inside enum defs:
     * - Use the "enum" keyword if it was explicitly present (has_enum_keyword)
     * - Otherwise skip the keyword (they're written as bare names like "Unit;") */
    if (has_keyword && node->kind == SYSML_KIND_ENUMERATION_USAGE && node->parent_id) {
        /* Check if parent is an enumeration def */
        for (size_t i = 0; i < model->element_count; i++) {
            if (model->elements[i] && model->elements[i]->id &&
                strcmp(model->elements[i]->id, node->parent_id) == 0) {
                if (model->elements[i]->kind == SYSML_KIND_ENUMERATION_DEF) {
                    /* Inside enum def - only write keyword if explicitly marked */
                    has_keyword = node->has_enum_keyword;
                }
                break;
            }
        }
    }

    if (has_keyword) {
        /* Handle standard library package prefix */
        if (node->kind == SYSML_KIND_LIBRARY_PACKAGE && node->is_standard_library) {
            fputs("standard ", w->out);
        }
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

    /* : typing - ALWAYS start fresh with : operator, never comma-continue from redefines/specializes
     * Multiple types are comma-separated from each other, but typing itself uses : */
    if (node->typed_by_count > 0) {
        /* End features use compact format (no spaces around colon) */
        if (node->kind == SYSML_KIND_END_FEATURE) {
            fputc(':', w->out);
        } else {
            fputs(" : ", w->out);
        }
        for (size_t i = 0; i < node->typed_by_count; i++) {
            if (i > 0) fputs(", ", w->out);  /* Comma only between multiple types */
            /* Output ~ prefix for conjugated port types */
            if (node->typed_by_conjugated && node->typed_by_conjugated[i]) {
                fputc('~', w->out);
            }
            fputs(node->typed_by[i], w->out);
        }
    }

    /* Write multiplicity (skip for end features - already written after keyword) */
    if (node->multiplicity_lower && !end_feature_mult_written) {
        /* Add space before [ for non-end features to preserve spacing: "String [0..1]" */
        if (node->kind != SYSML_KIND_END_FEATURE) {
            fputc(' ', w->out);
        }
        fputc('[', w->out);
        fputs(node->multiplicity_lower, w->out);
        if (node->multiplicity_upper) {
            fputs("..", w->out);
            fputs(node->multiplicity_upper, w->out);
        }
        fputc(']', w->out);
    }

    /* Write default value - only for usages, not definitions */
    if (node->default_value && !SYSML_KIND_IS_DEFINITION(node->kind)) {
        if (node->has_default_keyword) {
            fputs(" default", w->out);
        }
        fputs(" = ", w->out);
        fputs(node->default_value, w->out);
    }

    /* Write connector/allocation part (connect (a, b, c) or allocate X to Y) */
    if (node->connector_part) {
        fputc(' ', w->out);
        /* Interface usages: output "connect" keyword if it was present */
        if (node->has_connect_keyword && node->kind == SYSML_KIND_INTERFACE_USAGE) {
            fputs("connect ", w->out);
        }
        fputs(node->connector_part, w->out);
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

    /* Sort top-level elements by source position to preserve original order */
    if (child_count > 1) {
        qsort((void *)children, child_count, sizeof(SysmlNode*), compare_nodes_by_position);
    }

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
