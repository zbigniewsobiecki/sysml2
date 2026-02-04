#!/bin/bash
# Validation integration test runner
# Usage: test_validation.sh <sysml2_path> <fixture.sysml> <fixture.expected>

set -e

SYSML2="$1"
FIXTURE="$2"
EXPECTED="$3"

if [ ! -x "$SYSML2" ]; then
    echo "FAIL: sysml2 executable not found: $SYSML2"
    exit 1
fi

if [ ! -f "$FIXTURE" ]; then
    echo "FAIL: Fixture file not found: $FIXTURE"
    exit 1
fi

if [ ! -f "$EXPECTED" ]; then
    echo "FAIL: Expected file not found: $EXPECTED"
    exit 1
fi

# Run sysml2 and capture output and exit code
set +e
output=$("$SYSML2" "$FIXTURE" 2>&1)
actual_exit=$?
set -e

# Parse expected file
expected_exit=$(grep '^exit:' "$EXPECTED" | cut -d: -f2)
expected_errors=$(grep '^error:' "$EXPECTED" | cut -d: -f2)
expected_lines=$(grep '^line:' "$EXPECTED" | cut -d: -f2)

# Check exit code
if [ "$actual_exit" != "$expected_exit" ]; then
    echo "FAIL: Exit code $actual_exit, expected $expected_exit"
    echo "Output:"
    echo "$output"
    exit 1
fi

# Check each expected error code appears
for err in $expected_errors; do
    if ! echo "$output" | grep -q "\[$err\]"; then
        echo "FAIL: Expected error $err not found in output"
        echo "Output:"
        echo "$output"
        exit 1
    fi
done

# Check each expected line number appears with an error
for ln in $expected_lines; do
    if ! echo "$output" | grep -q ":${ln}:.*error"; then
        echo "FAIL: Expected error at line $ln not found in output"
        echo "Output:"
        echo "$output"
        exit 1
    fi
done

echo "PASS: $(basename "$FIXTURE")"
exit 0
