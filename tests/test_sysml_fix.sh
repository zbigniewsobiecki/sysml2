#!/bin/bash
#
# Test SysML --fix mode for cross-file formatting
#
# Usage: test_sysml_fix.sh <parser_executable> <fixture_dir>
#
# Tests that --fix correctly formats all .input.sysml files and produces
# output matching the corresponding .expected.sysml files.
#
# SPDX-License-Identifier: MIT

set -e

PARSER="$1"
FIXTURE_DIR="$2"

if [ -z "$PARSER" ] || [ -z "$FIXTURE_DIR" ]; then
    echo "Usage: $0 <parser> <fixture_dir>"
    exit 1
fi

if [ ! -x "$PARSER" ]; then
    echo "ERROR: Parser not found or not executable: $PARSER"
    exit 1
fi

if [ ! -d "$FIXTURE_DIR" ]; then
    echo "ERROR: Fixture directory not found: $FIXTURE_DIR"
    exit 1
fi

# Create temp directory for test
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

# Copy input files to temp directory
for input in "$FIXTURE_DIR"/*.input.sysml; do
    if [ -f "$input" ]; then
        base=$(basename "$input" .input.sysml)
        cp "$input" "$TEMP_DIR/${base}.sysml"
    fi
done

# Run --fix on all files
"$PARSER" --fix "$TEMP_DIR"/*.sysml 2>&1

# Compare each output with expected
has_failures=false
for input in "$FIXTURE_DIR"/*.input.sysml; do
    if [ -f "$input" ]; then
        base=$(basename "$input" .input.sysml)
        expected="$FIXTURE_DIR/${base}.expected.sysml"
        actual="$TEMP_DIR/${base}.sysml"

        if [ ! -f "$expected" ]; then
            echo "WARN: No expected file for $base"
            continue
        fi

        if diff -q "$expected" "$actual" > /dev/null 2>&1; then
            echo "PASS: $base"
        else
            echo "FAIL: $base"
            echo "=== Expected ==="
            cat "$expected"
            echo "=== Actual ==="
            cat "$actual"
            echo "=== Diff ==="
            diff "$expected" "$actual" || true
            has_failures=true
        fi
    fi
done

if [ "$has_failures" = true ]; then
    exit 1
fi
exit 0
