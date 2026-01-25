#!/bin/bash
#
# Test SysML pretty printing output against expected fixture files
#
# Usage: test_sysml_output.sh <parser_executable> <input.sysml>
#
# Expects: For input file "foo.input.sysml", an expected file "foo.expected.sysml"
#          in the same directory.
#
# SPDX-License-Identifier: MIT

set -e

PARSER="$1"
INPUT="$2"

if [ -z "$PARSER" ] || [ -z "$INPUT" ]; then
    echo "Usage: $0 <parser> <input.sysml>"
    exit 1
fi

if [ ! -x "$PARSER" ]; then
    echo "ERROR: Parser not found or not executable: $PARSER"
    exit 1
fi

if [ ! -f "$INPUT" ]; then
    echo "ERROR: Input file not found: $INPUT"
    exit 1
fi

# Derive expected file path (replace .input.sysml with .expected.sysml)
EXPECTED="${INPUT%.input.sysml}.expected.sysml"

if [ ! -f "$EXPECTED" ]; then
    echo "ERROR: Expected file not found: $EXPECTED"
    exit 1
fi

# Get base name for reporting
base=$(basename "$INPUT" .input.sysml)

# Run parser and capture output (skip validation for format tests)
ACTUAL=$("$PARSER" --no-validate -f sysml "$INPUT" 2>&1)
parser_exit=$?

if [ $parser_exit -ne 0 ]; then
    echo "FAIL: $base - parser returned error (exit code $parser_exit)"
    echo "=== Output ==="
    echo "$ACTUAL"
    exit 1
fi

# Read expected content
EXPECTED_CONTENT=$(cat "$EXPECTED")

if [ "$ACTUAL" = "$EXPECTED_CONTENT" ]; then
    echo "PASS: $base"
    exit 0
else
    echo "FAIL: $base - output mismatch"
    echo ""
    echo "=== Expected ==="
    echo "$EXPECTED_CONTENT"
    echo ""
    echo "=== Actual ==="
    echo "$ACTUAL"
    echo ""
    echo "=== Diff ==="
    diff <(echo "$EXPECTED_CONTENT") <(echo "$ACTUAL") || true
    exit 1
fi
