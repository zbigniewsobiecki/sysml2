#!/bin/bash
# Test error reporting - verifies syntax errors are reported at correct locations
#
# Each .sysml file in fixtures/errors/ should have a matching .expected file
# containing the expected error location as "line:column"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PARSER="${1:-$SCRIPT_DIR/../build/test_packcc_parser}"
FIXTURES_DIR="$SCRIPT_DIR/fixtures/errors"

passed=0
failed=0

for sysml_file in "$FIXTURES_DIR"/*.sysml; do
    base_name=$(basename "$sysml_file" .sysml)
    expected_file="$FIXTURES_DIR/${base_name}.expected"

    if [ ! -f "$expected_file" ]; then
        echo "SKIP: $base_name (no .expected file)"
        continue
    fi

    # Run parser and capture stderr
    error_output=$("$PARSER" "$sysml_file" 2>&1)

    # Extract line:column from error output (format: "filename:line:col: error:")
    actual_loc=$(echo "$error_output" | grep -o ':[0-9]*:[0-9]*:' | head -1 | sed 's/^://;s/:$//')

    # Read expected location
    expected_loc=$(cat "$expected_file" | tr -d '\n')

    if [ "$actual_loc" = "$expected_loc" ]; then
        echo "PASS: $base_name (error at $actual_loc)"
        ((passed++))
    else
        echo "FAIL: $base_name"
        echo "  Expected: $expected_loc"
        echo "  Actual:   $actual_loc"
        echo "  Output:"
        echo "$error_output" | sed 's/^/    /'
        ((failed++))
    fi
done

echo ""
echo "Results: $passed passed, $failed failed"

if [ $failed -gt 0 ]; then
    exit 1
fi
exit 0
