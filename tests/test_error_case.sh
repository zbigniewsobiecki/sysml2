#!/bin/bash
# Test a single error case - verifies parser fails at expected location
#
# Usage: test_error_case.sh <parser> <sysml_file> <expected_line:col>

PARSER="$1"
SYSML_FILE="$2"
EXPECTED_LOC="$3"

if [ -z "$PARSER" ] || [ -z "$SYSML_FILE" ] || [ -z "$EXPECTED_LOC" ]; then
    echo "Usage: $0 <parser> <sysml_file> <expected_line:col>"
    exit 1
fi

# Run parser and capture stderr
error_output=$("$PARSER" "$SYSML_FILE" 2>&1)
exit_code=$?

# Extract line:column from error output (format: "filename:line:col: error:")
actual_loc=$(echo "$error_output" | grep -o ':[0-9]*:[0-9]*:' | head -1 | sed 's/^://;s/:$//')

# Check that parser failed (exit code non-zero)
if [ $exit_code -eq 0 ]; then
    echo "FAIL: Parser should have failed but returned success"
    echo "Output: $error_output"
    exit 1
fi

# Check error location matches
if [ "$actual_loc" = "$EXPECTED_LOC" ]; then
    echo "PASS: Error at $actual_loc"
    exit 0
else
    echo "FAIL: Expected error at $EXPECTED_LOC, got $actual_loc"
    echo "Output:"
    echo "$error_output"
    exit 1
fi
