#!/bin/bash
# Test --allow-semantic-errors flag
# Verifies that:
#   - Without flag: semantic errors abort, no files modified
#   - With flag: semantic errors reported, files written, exit code 2
#   - Parse errors always abort (even with flag)
#
# Usage: test_allow_semantic_errors.sh <sysml2_path>

set -e

SYSML2="$1"
BASETMPDIR="${TMPDIR:-/tmp}"
TMPDIR="$BASETMPDIR/sysml2-allow-semantic-tests-$$"
mkdir -p "$TMPDIR"
PASSED=0
FAILED=0

if [ ! -x "$SYSML2" ]; then
    echo "FAIL: sysml2 executable not found: $SYSML2"
    exit 1
fi

cleanup() {
    rm -rf "$TMPDIR"
}
trap cleanup EXIT

# Helper function to run a test case
run_test() {
    local name="$1"
    local expected_exit="$2"
    shift 2

    set +e
    "$@" >/dev/null 2>&1
    local actual_exit=$?
    set -e

    if [ "$actual_exit" -eq "$expected_exit" ]; then
        echo "PASS: $name (exit $actual_exit)"
        PASSED=$((PASSED + 1))
    else
        echo "FAIL: $name - expected exit $expected_exit, got $actual_exit"
        FAILED=$((FAILED + 1))
    fi
}

# Helper to check if file was modified (contains expected content)
check_content() {
    local file="$1"
    local expected="$2"
    local name="$3"

    if grep -q "$expected" "$file" 2>/dev/null; then
        echo "PASS: $name"
        PASSED=$((PASSED + 1))
    else
        echo "FAIL: $name - expected content '$expected' not found"
        echo "  Actual content:"
        cat "$file" 2>/dev/null || echo "  (file not found)"
        FAILED=$((FAILED + 1))
    fi
}

# Helper to check file was NOT modified
check_unchanged() {
    local file="$1"
    local original="$2"
    local name="$3"

    local current
    current=$(cat "$file" 2>/dev/null)
    if [ "$current" = "$original" ]; then
        echo "PASS: $name"
        PASSED=$((PASSED + 1))
    else
        echo "FAIL: $name - file was modified when it shouldn't be"
        FAILED=$((FAILED + 1))
    fi
}

echo "=== --allow-semantic-errors Tests ==="
echo ""

# ============================================================================
echo "--- Basic behavior without flag ---"
# ============================================================================

MODEL1="$TMPDIR/model1.sysml"
FRAG1="$TMPDIR/frag1.sysml"

# Create base model with semantic error
echo 'package Test { item def X { attribute foo : UndefinedType; } }' > "$MODEL1"
ORIGINAL1=$(cat "$MODEL1")

# Create fragment that adds another item
echo 'item def NewItem { attribute bar : String; }' > "$FRAG1"

# Without flag, should abort with exit 2 (semantic error)
run_test "without flag: semantic error aborts" 2 "$SYSML2" --set "$FRAG1" --at 'Test' -f json "$MODEL1"

# File should be unchanged
check_unchanged "$MODEL1" "$ORIGINAL1" "without flag: file unchanged after abort"

# ============================================================================
echo ""
echo "--- Basic behavior with flag ---"
# ============================================================================

MODEL2="$TMPDIR/model2.sysml"
FRAG2="$TMPDIR/frag2.sysml"

# Create base model with semantic error
echo 'package Test { item def X { attribute foo : UndefinedType; } }' > "$MODEL2"

# Create fragment
echo 'item def NewItem { attribute bar : String; }' > "$FRAG2"

# With flag, should write file and exit 2
run_test "with flag: semantic error continues" 2 "$SYSML2" --allow-semantic-errors --set "$FRAG2" --at 'Test' -f json "$MODEL2"

# File should be modified
check_content "$MODEL2" "NewItem" "with flag: file was modified"

# ============================================================================
echo ""
echo "--- Parse errors always abort (even with flag) ---"
# ============================================================================

MODEL3="$TMPDIR/model3.sysml"
FRAG3="$TMPDIR/frag3.sysml"

# Create valid model
echo 'package Test { item def X; }' > "$MODEL3"
ORIGINAL3=$(cat "$MODEL3")

# Create fragment with parse error
echo 'item def Bad {{{' > "$FRAG3"

# With flag, parse errors still abort with exit 1
run_test "with flag: parse error still aborts" 1 "$SYSML2" --allow-semantic-errors --set "$FRAG3" --at 'Test' "$MODEL3"

# File should be unchanged
check_unchanged "$MODEL3" "$ORIGINAL3" "with flag: file unchanged after parse error"

# ============================================================================
echo ""
echo "--- Clean model with flag returns 0 ---"
# ============================================================================

MODEL4="$TMPDIR/model4.sysml"
FRAG4="$TMPDIR/frag4.sysml"

# Create valid model
echo 'package Test { item def X; }' > "$MODEL4"

# Create valid fragment
echo 'item def Y;' > "$FRAG4"

# With flag, valid model returns 0
run_test "with flag: clean model returns 0" 0 "$SYSML2" --allow-semantic-errors --set "$FRAG4" --at 'Test' "$MODEL4"

check_content "$MODEL4" "item def Y" "with flag: valid fragment added"

# ============================================================================
echo ""
echo "--- JSON output with flag ---"
# ============================================================================

MODEL5="$TMPDIR/model5.sysml"
FRAG5="$TMPDIR/frag5.sysml"
JSON5="$TMPDIR/output5.json"

# Create base model with semantic error
echo 'package Test { item def X { attribute foo : UndefinedType; } }' > "$MODEL5"

# Create fragment
echo 'item def A; item def B;' > "$FRAG5"

# Run with JSON output
set +e
"$SYSML2" --allow-semantic-errors --set "$FRAG5" --at 'Test' -f json "$MODEL5" > "$JSON5" 2>/dev/null
local_exit=$?
set -e

# Should return exit 2
if [ "$local_exit" -eq 2 ]; then
    echo "PASS: JSON mode returns exit 2"
    PASSED=$((PASSED + 1))
else
    echo "FAIL: JSON mode - expected exit 2, got $local_exit"
    FAILED=$((FAILED + 1))
fi

# JSON output should have counts
if grep -q '"added":' "$JSON5" 2>/dev/null; then
    echo "PASS: JSON output has 'added' count"
    PASSED=$((PASSED + 1))
else
    echo "FAIL: JSON output missing 'added' count"
    echo "  JSON content:"
    cat "$JSON5" 2>/dev/null || echo "  (file empty/missing)"
    FAILED=$((FAILED + 1))
fi

# ============================================================================
echo ""
echo "--- Delete mode with flag ---"
# ============================================================================

MODEL6="$TMPDIR/model6.sysml"

# Create model with semantic error
echo 'package Test { item def A { attribute x : UndefinedType; } item def B; }' > "$MODEL6"

# Delete should work with semantic errors
run_test "delete with flag: semantic error continues" 2 "$SYSML2" --allow-semantic-errors --delete 'Test::B' "$MODEL6"

# B should be deleted
if ! grep -q "item def B" "$MODEL6" 2>/dev/null; then
    echo "PASS: item B was deleted"
    PASSED=$((PASSED + 1))
else
    echo "FAIL: item B should have been deleted"
    FAILED=$((FAILED + 1))
fi

# A should still be there
check_content "$MODEL6" "item def A" "delete: item A still exists"

# ============================================================================
echo ""
echo "--- Flag only affects modify mode ---"
# ============================================================================

MODEL7="$TMPDIR/model7.sysml"

# Create model with semantic error
echo 'package Test { item def X { attribute foo : UndefinedType; } }' > "$MODEL7"

# Normal validation mode (not modify) - flag has no effect, returns 2
run_test "validation mode: flag has no effect" 2 "$SYSML2" --allow-semantic-errors "$MODEL7"

# ============================================================================
echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"

if [ "$FAILED" -gt 0 ]; then
    exit 1
fi

echo ""
echo "All --allow-semantic-errors tests passed!"
exit 0
