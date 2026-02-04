#!/bin/bash
#
# Integration test for --list discovery mode
#
# Tests: --list flag with and without --select, JSON/text output,
# incompatible flag validation, stdin input
#
# SPDX-License-Identifier: MIT

set -e

PARSER="${1:-./sysml2}"
WORKDIR=$(mktemp -d)
trap "rm -rf $WORKDIR" EXIT

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

TESTS_PASSED=0
TESTS_FAILED=0

# Test helper functions
pass() {
    echo -e "${GREEN}PASS${NC}: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail() {
    echo -e "${RED}FAIL${NC}: $1"
    echo "  Expected: $2"
    echo "  Got:      $3"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

assert_contains() {
    local output="$1"
    local expected="$2"
    local testname="$3"

    if echo "$output" | grep -q "$expected"; then
        pass "$testname"
    else
        fail "$testname" "contains '$expected'" "not found in: $output"
    fi
}

assert_not_contains() {
    local output="$1"
    local pattern="$2"
    local testname="$3"

    if echo "$output" | grep -q "$pattern"; then
        fail "$testname" "should not contain '$pattern'" "found"
    else
        pass "$testname"
    fi
}

assert_exit_code() {
    local actual="$1"
    local expected="$2"
    local testname="$3"

    if [ "$actual" -eq "$expected" ]; then
        pass "$testname"
    else
        fail "$testname" "exit code $expected" "exit code $actual"
    fi
}

assert_equals() {
    local actual="$1"
    local expected="$2"
    local testname="$3"

    if [ "$actual" = "$expected" ]; then
        pass "$testname"
    else
        fail "$testname" "$expected" "$actual"
    fi
}

echo "=== CLI --list Discovery Mode Tests ==="
echo "Parser: $PARSER"
echo "Workdir: $WORKDIR"
echo ""

# Check parser exists
if [ ! -x "$PARSER" ]; then
    echo "ERROR: Parser not found or not executable: $PARSER"
    exit 1
fi

# Create test fixtures
cat > "$WORKDIR/model.sysml" << 'EOF'
package Vehicles {
    part def Car {
        part engine : Engine;
    }
    part def Truck;
    part def Engine;
}
EOF

cat > "$WORKDIR/multi.sysml" << 'EOF'
package Animals {
    part def Dog;
    part def Cat;
}
package Plants {
    part def Tree;
}
EOF

# ============================================================
# TEST 1: --list shows root elements (text format)
# ============================================================
echo "--- Test 1: --list shows root elements (text) ---"

OUTPUT=$("$PARSER" -P --list "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "List root elements exits 0"
assert_contains "$OUTPUT" "Vehicles" "Root element Vehicles listed"
assert_contains "$OUTPUT" "package" "Root element kind is package"
assert_not_contains "$OUTPUT" "Car" "Children not shown without --select"

# ============================================================
# TEST 2: --list with --select shows children (text format)
# ============================================================
echo ""
echo "--- Test 2: --list -s shows children (text) ---"

OUTPUT=$("$PARSER" -P --list -s 'Vehicles::*' "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "List children exits 0"
assert_contains "$OUTPUT" "Vehicles::Car" "Child Car listed"
assert_contains "$OUTPUT" "Vehicles::Truck" "Child Truck listed"
assert_contains "$OUTPUT" "Vehicles::Engine" "Child Engine listed"
assert_contains "$OUTPUT" "part def" "Kind shows part def"

# ============================================================
# TEST 3: --list with --select shows nested children
# ============================================================
echo ""
echo "--- Test 3: --list -s shows nested children ---"

OUTPUT=$("$PARSER" -P --list -s 'Vehicles::Car::*' "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "List nested children exits 0"
assert_contains "$OUTPUT" "Vehicles::Car::engine" "Nested child engine listed"

# ============================================================
# TEST 4: --list with recursive select (**)
# ============================================================
echo ""
echo "--- Test 4: --list -s with ** recursive ---"

OUTPUT=$("$PARSER" -P --list -s 'Vehicles::**' "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "List recursive exits 0"
assert_contains "$OUTPUT" "Vehicles::Car" "Recursive includes Car"
assert_contains "$OUTPUT" "Vehicles::Truck" "Recursive includes Truck"
assert_contains "$OUTPUT" "Vehicles::Car::engine" "Recursive includes nested engine"

# ============================================================
# TEST 5: --list -f json output
# ============================================================
echo ""
echo "--- Test 5: --list -f json ---"

OUTPUT=$("$PARSER" -P --list -f json "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "List JSON exits 0"
assert_contains "$OUTPUT" '"id": "Vehicles"' "JSON has id field"
assert_contains "$OUTPUT" '"name": "Vehicles"' "JSON has name field"
assert_contains "$OUTPUT" '"kind": "package"' "JSON has kind field"

# ============================================================
# TEST 6: --list -f json with --select
# ============================================================
echo ""
echo "--- Test 6: --list -f json -s ---"

OUTPUT=$("$PARSER" -P --list -f json -s 'Vehicles::*' "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "List JSON with select exits 0"
assert_contains "$OUTPUT" '"id": "Vehicles::Car"' "JSON select has Car id"
assert_contains "$OUTPUT" '"name": "Car"' "JSON select has Car name"
assert_contains "$OUTPUT" '"kind": "part def"' "JSON select has part def kind"
assert_contains "$OUTPUT" '"id": "Vehicles::Truck"' "JSON select has Truck id"

# ============================================================
# TEST 7: --list with stdin
# ============================================================
echo ""
echo "--- Test 7: --list with stdin ---"

OUTPUT=$(echo 'package P { part def X; }' | "$PARSER" -P --list 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "List from stdin exits 0"
assert_contains "$OUTPUT" "P" "Stdin root element listed"
assert_contains "$OUTPUT" "package" "Stdin root kind correct"

# ============================================================
# TEST 8: --list -f json with stdin
# ============================================================
echo ""
echo "--- Test 8: --list -f json with stdin ---"

OUTPUT=$(echo 'package Q { part def Y; }' | "$PARSER" -P --list -f json 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "List JSON from stdin exits 0"
assert_contains "$OUTPUT" '"id": "Q"' "Stdin JSON has id"
assert_contains "$OUTPUT" '"name": "Q"' "Stdin JSON has name"
assert_contains "$OUTPUT" '"kind": "package"' "Stdin JSON has kind"

# ============================================================
# TEST 9: --list with multiple root elements
# ============================================================
echo ""
echo "--- Test 9: --list with multiple roots ---"

OUTPUT=$("$PARSER" -P --list "$WORKDIR/multi.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "List multiple roots exits 0"
assert_contains "$OUTPUT" "Animals" "First root listed"
assert_contains "$OUTPUT" "Plants" "Second root listed"

# ============================================================
# TEST 10: --list -f json with multiple roots
# ============================================================
echo ""
echo "--- Test 10: --list -f json multiple roots ---"

OUTPUT=$("$PARSER" -P --list -f json "$WORKDIR/multi.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "List JSON multiple roots exits 0"
assert_contains "$OUTPUT" '"id": "Animals"' "JSON has Animals"
assert_contains "$OUTPUT" '"id": "Plants"' "JSON has Plants"

# ============================================================
# TEST 11: --list --fix produces error
# ============================================================
echo ""
echo "--- Test 11: --list --fix error ---"

set +e
OUTPUT=$("$PARSER" --list --fix "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?
set -e

assert_exit_code $EXIT_CODE 1 "List with --fix exits 1"
assert_contains "$OUTPUT" "cannot be combined" "Error message mentions incompatibility"

# ============================================================
# TEST 12: --list --delete produces error
# ============================================================
echo ""
echo "--- Test 12: --list --delete error ---"

set +e
OUTPUT=$("$PARSER" --list --delete 'X' "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?
set -e

assert_exit_code $EXIT_CODE 1 "List with --delete exits 1"
assert_contains "$OUTPUT" "cannot be combined" "Error message for --delete"

# ============================================================
# TEST 13: --list --set produces error
# ============================================================
echo ""
echo "--- Test 13: --list --set error ---"

set +e
OUTPUT=$("$PARSER" --list --set "$WORKDIR/model.sysml" --at 'X' "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?
set -e

assert_exit_code $EXIT_CODE 1 "List with --set exits 1"
assert_contains "$OUTPUT" "cannot be combined" "Error message for --set"

# ============================================================
# TEST 14: --list produces tab-separated output
# ============================================================
echo ""
echo "--- Test 14: Tab-separated output format ---"

OUTPUT=$("$PARSER" -P --list "$WORKDIR/model.sysml" 2>&1)

# Check that output contains a tab character between name and kind
if echo "$OUTPUT" | grep -q "$(printf 'Vehicles\tpackage')"; then
    pass "Tab separates id and kind"
else
    fail "Tab separates id and kind" "Vehicles<TAB>package" "$OUTPUT"
fi

# ============================================================
# TEST 15: --list -s with specific element (not wildcard)
# ============================================================
echo ""
echo "--- Test 15: --list -s with specific element ---"

OUTPUT=$("$PARSER" -P --list -s 'Vehicles::Car' "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "List specific element exits 0"
assert_contains "$OUTPUT" "Vehicles::Car" "Specific element listed"
assert_not_contains "$OUTPUT" "Vehicles::Truck" "Other elements not listed"

# ============================================================
# TEST 16: --list without -f still produces output
# ============================================================
echo ""
echo "--- Test 16: --list without -f produces output ---"

OUTPUT=$("$PARSER" -P --list "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "List without -f exits 0"
# Verify non-empty output (unlike bare --select which requires -f)
if [ -n "$OUTPUT" ]; then
    pass "--list produces output without -f"
else
    fail "--list produces output without -f" "non-empty output" "empty"
fi

# ============================================================
# TEST 17: -l short flag works
# ============================================================
echo ""
echo "--- Test 17: -l short flag ---"

OUTPUT=$("$PARSER" -P -l "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "Short -l flag exits 0"
assert_contains "$OUTPUT" "Vehicles" "Short flag lists root"
assert_contains "$OUTPUT" "package" "Short flag shows kind"

# ============================================================
# TEST 18: --list -o writes to file
# ============================================================
echo ""
echo "--- Test 18: --list -o output file ---"

"$PARSER" -P --list -o "$WORKDIR/list_output.txt" "$WORKDIR/model.sysml" 2>&1
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "List to file exits 0"
FILE_CONTENT=$(cat "$WORKDIR/list_output.txt")
assert_contains "$FILE_CONTENT" "Vehicles" "Output file has root element"
assert_contains "$FILE_CONTENT" "package" "Output file has kind"

# ============================================================
# Summary
# ============================================================
echo ""
echo "=== Test Summary ==="
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
echo ""

if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
fi
