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
        .col = 1
    };

    /* Create parser */
    sysml_context_t *parser = sysml_create(&ctx);
    if (!parser) {
        fprintf(stderr, "Failed to create parser\n");
        free(input);
        return 1;
    }

    /* Parse */
    void *result = NULL;
    int ret = sysml_parse(parser, &result);

    if (ret) {
        printf("PASS: %s\n", argv[1]);
    } else {
        printf("FAIL: %s (parse error)\n", argv[1]);
    }

    /* Cleanup */
    sysml_destroy(parser);
    free(input);

    return ret ? 0 : 1;
}
