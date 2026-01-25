#!/bin/bash
# Exit code integration tests
# Verifies that sysml2 returns correct exit codes:
#   0 - Success (no errors)
#   1 - Parse/syntax error
#   2 - Semantic/validation error
#
# Usage: test_exit_codes.sh <sysml2_path>

set -e

SYSML2="$1"
TMPDIR="${TMPDIR:-/tmp}"
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

# Helper to run test with stdin input
run_stdin_test() {
    local name="$1"
    local expected_exit="$2"
    local input="$3"

    set +e
    echo "$input" | "$SYSML2" >/dev/null 2>&1
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

echo "=== Exit Code Tests ==="
echo ""

# Create temp files for testing
PARSE_ERROR_FILE="$TMPDIR/sysml2_test_parse_error_$$.sysml"
SEMANTIC_ERROR_FILE="$TMPDIR/sysml2_test_semantic_error_$$.sysml"
VALID_FILE="$TMPDIR/sysml2_test_valid_$$.sysml"
FRAGMENT_FILE="$TMPDIR/sysml2_test_fragment_$$.sysml"

cleanup() {
    rm -f "$PARSE_ERROR_FILE" "$SEMANTIC_ERROR_FILE" "$VALID_FILE" "$FRAGMENT_FILE"
}
trap cleanup EXIT

# Create test files
echo 'invalid {{{' > "$PARSE_ERROR_FILE"
echo 'package P { part p: UndefinedType; }' > "$SEMANTIC_ERROR_FILE"
echo 'package P { part p; }' > "$VALID_FILE"
echo 'part def NewPart;' > "$FRAGMENT_FILE"

echo "--- Normal mode (file input) ---"
run_test "file: parse error returns 1" 1 "$SYSML2" "$PARSE_ERROR_FILE"
run_test "file: semantic error returns 2" 2 "$SYSML2" "$SEMANTIC_ERROR_FILE"
run_test "file: success returns 0" 0 "$SYSML2" "$VALID_FILE"

echo ""
echo "--- Normal mode (stdin) ---"
run_stdin_test "stdin: parse error returns 1" 1 'invalid {{{'
run_stdin_test "stdin: semantic error returns 2" 2 'package P { part p: UndefinedType; }'
run_stdin_test "stdin: success returns 0" 0 'package P { part p; }'

echo ""
echo "--- Parse-only mode ---"
run_test "parse-only: parse error returns 1" 1 "$SYSML2" --parse-only "$PARSE_ERROR_FILE"
run_test "parse-only: would-be semantic error returns 0" 0 "$SYSML2" --parse-only "$SEMANTIC_ERROR_FILE"
run_test "parse-only: success returns 0" 0 "$SYSML2" --parse-only "$VALID_FILE"

echo ""
echo "--- Fix mode ---"
# Create fresh copies for fix mode tests (since --fix modifies files)
PARSE_ERROR_FIX="$TMPDIR/sysml2_test_parse_fix_$$.sysml"
SEMANTIC_ERROR_FIX="$TMPDIR/sysml2_test_semantic_fix_$$.sysml"
VALID_FIX="$TMPDIR/sysml2_test_valid_fix_$$.sysml"

echo 'invalid {{{' > "$PARSE_ERROR_FIX"
echo 'package P { part p: UndefinedType; }' > "$SEMANTIC_ERROR_FIX"
echo 'package P { part p; }' > "$VALID_FIX"

run_test "fix: parse error returns 1" 1 "$SYSML2" --fix "$PARSE_ERROR_FIX"
run_test "fix: semantic error returns 2" 2 "$SYSML2" --fix "$SEMANTIC_ERROR_FIX"
run_test "fix: success returns 0" 0 "$SYSML2" --fix "$VALID_FIX"

rm -f "$PARSE_ERROR_FIX" "$SEMANTIC_ERROR_FIX" "$VALID_FIX"

echo ""
echo "--- Modify mode (--delete) ---"
# Create files for modify mode tests
MODIFY_VALID="$TMPDIR/sysml2_test_modify_$$.sysml"
MODIFY_SEMANTIC="$TMPDIR/sysml2_test_modify_sem_$$.sysml"

echo 'package P { part a; part b; }' > "$MODIFY_VALID"
echo 'package P { part a: UndefinedType; part b; }' > "$MODIFY_SEMANTIC"

run_test "delete: success returns 0" 0 "$SYSML2" --delete 'P::a' --dry-run "$MODIFY_VALID"
run_test "delete: semantic error in remaining content returns 2" 2 "$SYSML2" --delete 'P::b' "$MODIFY_SEMANTIC"

rm -f "$MODIFY_VALID" "$MODIFY_SEMANTIC"

echo ""
echo "--- Modify mode (--set) ---"
SET_BASE="$TMPDIR/sysml2_test_set_base_$$.sysml"
SET_FRAG_VALID="$TMPDIR/sysml2_test_set_frag_valid_$$.sysml"
SET_FRAG_PARSE_ERR="$TMPDIR/sysml2_test_set_frag_parse_$$.sysml"
SET_FRAG_SEM_ERR="$TMPDIR/sysml2_test_set_frag_sem_$$.sysml"

echo 'package P { part existing; }' > "$SET_BASE"
echo 'part def NewPart;' > "$SET_FRAG_VALID"
echo 'part def {{{' > "$SET_FRAG_PARSE_ERR"
echo 'part newPart : UndefinedType;' > "$SET_FRAG_SEM_ERR"

run_test "set: success returns 0" 0 "$SYSML2" --set "$SET_FRAG_VALID" --at 'P' --dry-run "$SET_BASE"
run_test "set: fragment parse error returns 1" 1 "$SYSML2" --set "$SET_FRAG_PARSE_ERR" --at 'P' --dry-run "$SET_BASE"
run_test "set: semantic error after merge returns 2" 2 "$SYSML2" --set "$SET_FRAG_SEM_ERR" --at 'P' "$SET_BASE"

rm -f "$SET_BASE" "$SET_FRAG_VALID" "$SET_FRAG_PARSE_ERR" "$SET_FRAG_SEM_ERR"

echo ""
echo "--- Edge cases ---"

# Multiple parse errors
MULTI_PARSE="$TMPDIR/sysml2_test_multi_parse_$$.sysml"
echo 'invalid1 {{{
invalid2 }}}' > "$MULTI_PARSE"
run_test "multiple parse errors returns 1" 1 "$SYSML2" "$MULTI_PARSE"
rm -f "$MULTI_PARSE"

# Multiple semantic errors
MULTI_SEM="$TMPDIR/sysml2_test_multi_sem_$$.sysml"
echo 'package P {
    part a: Undefined1;
    part b: Undefined2;
}' > "$MULTI_SEM"
run_test "multiple semantic errors returns 2" 2 "$SYSML2" "$MULTI_SEM"
rm -f "$MULTI_SEM"

# Lexical errors (E1xxx) should also return 1
LEXICAL_ERROR="$TMPDIR/sysml2_test_lexical_$$.sysml"
echo 'package P { attribute x = "unterminated string; }' > "$LEXICAL_ERROR"
run_test "lexical error returns 1" 1 "$SYSML2" "$LEXICAL_ERROR"
rm -f "$LEXICAL_ERROR"

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"

if [ "$FAILED" -gt 0 ]; then
    exit 1
fi

echo ""
echo "All exit code tests passed!"
exit 0
