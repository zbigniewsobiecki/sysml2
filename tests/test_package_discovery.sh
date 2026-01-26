#!/bin/bash
# Package Discovery Tests
#
# Tests that sysml2 can discover packages by parsing files,
# even when the filename doesn't match the package name.
#
# Usage: test_package_discovery.sh <sysml2_path>

set -e

SYSML2="$1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FIXTURES_DIR="$SCRIPT_DIR/fixtures/package_discovery"
PASSED=0
FAILED=0

if [ ! -x "$SYSML2" ]; then
    echo "FAIL: sysml2 executable not found: $SYSML2"
    exit 1
fi

# Helper function to run a test case
run_test() {
    local name="$1"
    local expected_exit="$2"
    shift 2

    set +e
    output=$("$@" 2>&1)
    local actual_exit=$?
    set -e

    if [ "$actual_exit" -eq "$expected_exit" ]; then
        echo "PASS: $name (exit $actual_exit)"
        PASSED=$((PASSED + 1))
    else
        echo "FAIL: $name - expected exit $expected_exit, got $actual_exit"
        echo "  Output: $output"
        FAILED=$((FAILED + 1))
    fi
}

echo "=== Package Discovery Tests ==="
echo ""

echo "--- Test 1: Import package from _index.sysml ---"
# The main.sysml imports SystemBehavior::* but the package is in behavior/_index.sysml
# Without package discovery, this would fail with "import not found"
# With package discovery, it should succeed
run_test "import from _index.sysml succeeds" 0 "$SYSML2" "$FIXTURES_DIR/main.sysml"

echo ""
echo "--- Test 2: Verbose mode shows package discovery ---"
# Check that verbose mode shows package discovery messages
set +e
output=$("$SYSML2" -v "$FIXTURES_DIR/main.sysml" 2>&1)
actual_exit=$?
set -e

if [ "$actual_exit" -eq 0 ]; then
    # Check that the output mentions package discovery
    if echo "$output" | grep -q "registered package 'SystemBehavior'"; then
        echo "PASS: verbose mode shows package registration"
        PASSED=$((PASSED + 1))
    else
        echo "FAIL: verbose mode should show 'registered package SystemBehavior'"
        echo "  Output: $output"
        FAILED=$((FAILED + 1))
    fi

    # Check that package map was used for resolution
    if echo "$output" | grep -q "found 'SystemBehavior' via package map"; then
        echo "PASS: package map used for import resolution"
        PASSED=$((PASSED + 1))
    else
        echo "FAIL: should show 'found SystemBehavior via package map'"
        echo "  Output: $output"
        FAILED=$((FAILED + 1))
    fi
else
    echo "FAIL: sysml2 returned exit code $actual_exit, expected 0"
    echo "  Output: $output"
    FAILED=$((FAILED + 2))
fi

echo ""
echo "--- Test 3: --no-resolve skips package discovery ---"
# With --no-resolve, import resolution is skipped entirely
set +e
output=$("$SYSML2" -v --no-resolve "$FIXTURES_DIR/main.sysml" 2>&1)
actual_exit=$?
set -e

if ! echo "$output" | grep -q "discovering packages"; then
    echo "PASS: --no-resolve skips package discovery"
    PASSED=$((PASSED + 1))
else
    echo "FAIL: --no-resolve should skip package discovery"
    FAILED=$((FAILED + 1))
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"

if [ "$FAILED" -gt 0 ]; then
    exit 1
fi

echo ""
echo "All package discovery tests passed!"
exit 0
