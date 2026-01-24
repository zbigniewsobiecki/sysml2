/*
 * Test program for packcc-generated SysML v2 parser
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/sysml_parser.h"

/* Read entire file into memory */
static char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open file: %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(buf, 1, size, f);
    buf[read] = '\0';
    fclose(f);

    if (len) *len = read;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.sysml>\n", argv[0]);
        return 1;
    }

    /* Read input file */
    size_t input_len;
    char *input = read_file(argv[1], &input_len);
    if (!input) {
        return 1;
    }

    /* Set up parser context */
    SysmlParserContext ctx = {
        .filename = argv[1],
        .input = input,
        .input_len = input_len,
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
    };

    /* Create parser */
    sysml2_context_t *parser = sysml2_create(&ctx);
    if (!parser) {
        fprintf(stderr, "Failed to create parser\n");
        free(input);
        return 1;
    }

    /* Parse */
    void *result = NULL;
    int ret = sysml2_parse(parser, &result);

    /* Check both parse result and error count */
    int success = ret && (ctx.error_count == 0);

    if (success) {
        printf("PASS: %s\n", argv[1]);
    } else {
        printf("FAIL: %s (%d error%s)\n", argv[1], ctx.error_count,
               ctx.error_count == 1 ? "" : "s");
    }

    /* Cleanup */
    sysml2_destroy(parser);
    free(input);

    return success ? 0 : 1;
}
