#!/bin/bash
#
# Test JSON output from parser against expected fixture files
#
# Usage: test_json_output.sh <parser_executable> <input_sysml_file>
#
# SPDX-License-Identifier: MIT

set -e

PARSER="$1"
FIXTURE="$2"

if [ -z "$PARSER" ] || [ -z "$FIXTURE" ]; then
    echo "Usage: $0 <parser> <fixture.sysml>"
    exit 1
fi

if [ ! -x "$PARSER" ]; then
    echo "ERROR: Parser not found or not executable: $PARSER"
    exit 1
fi

if [ ! -f "$FIXTURE" ]; then
    echo "ERROR: Fixture file not found: $FIXTURE"
    exit 1
fi

# Determine expected JSON file (supports .sysml and .kerml)
base=$(basename "$FIXTURE" .sysml)
base=$(basename "$base" .kerml)
dir=$(dirname "$FIXTURE")
expected="$dir/${base}.json"

if [ ! -f "$expected" ]; then
    echo "SKIP: No expected file for $base"
    exit 0
fi

# Check for jq
if ! command -v jq &> /dev/null; then
    echo "ERROR: jq is required for JSON comparison"
    exit 1
fi

# Run parser and capture output (skip validation for JSON output tests)
actual=$("$PARSER" --no-validate -f json "$FIXTURE" 2>/dev/null)
if [ $? -ne 0 ]; then
    echo "FAIL: $base - parser returned error"
    exit 1
fi

# Normalize both JSON outputs:
# - Remove meta.source (path differs between runs)
# - Sort keys for consistent comparison
actual_norm=$(echo "$actual" | jq -S 'del(.meta.source)')
expected_norm=$(jq -S 'del(.meta.source)' "$expected")

if [ "$actual_norm" = "$expected_norm" ]; then
    echo "PASS: $base"
    exit 0
else
    echo "FAIL: $base"
    echo ""
    echo "=== Expected ==="
    echo "$expected_norm" | head -50
    echo ""
    echo "=== Actual ==="
    echo "$actual_norm" | head -50
    echo ""

    # Show diff for more detail
    echo "=== Diff ==="
    diff <(echo "$expected_norm") <(echo "$actual_norm") | head -30 || true

    exit 1
fi
