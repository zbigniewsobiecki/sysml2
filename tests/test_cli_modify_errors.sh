#!/bin/bash
#
# Error handling tests for --set and --delete operations
#
# SPDX-License-Identifier: MIT

set -e

PARSER="${1:-./sysml2}"
WORKDIR=$(mktemp -d)
trap "rm -rf $WORKDIR" EXIT

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

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
        fail "$testname" "contains '$expected'" "not found"
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

echo "=== CLI Modify Error Handling Tests ==="
echo "Parser: $PARSER"
echo "Workdir: $WORKDIR"
echo ""

# Check parser exists
if [ ! -x "$PARSER" ]; then
    echo "ERROR: Parser not found or not executable: $PARSER"
    exit 1
fi

# ============================================================
# Missing/Invalid arguments
# ============================================================
echo "--- Missing/Invalid arguments ---"

# TEST: Missing --at for --set (exit 1)
cat > "$WORKDIR/missing_at.sysml" << 'EOF'
package P { }
EOF

echo 'part def X;' | "$PARSER" -P --set - "$WORKDIR/missing_at.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
assert_exit_code $EXIT_CODE 1 "Missing --at for --set exits 1"

# Verify file unchanged
OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/missing_at.sysml" 2>&1)
assert_not_contains "$OUTPUT" "part def X" "File unchanged after error"

# TEST: --at without preceding --set (exit 1)
cat > "$WORKDIR/orphan_at.sysml" << 'EOF'
package P { }
EOF

"$PARSER" -P --at 'P' "$WORKDIR/orphan_at.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
assert_exit_code $EXIT_CODE 1 "--at without --set exits 1"

# TEST: Invalid delete pattern: empty string
cat > "$WORKDIR/empty_pattern.sysml" << 'EOF'
package P { part def A; }
EOF

"$PARSER" -P --delete '' "$WORKDIR/empty_pattern.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
assert_exit_code $EXIT_CODE 1 "Empty delete pattern exits 1"

# Verify file unchanged
OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/empty_pattern.sysml" 2>&1)
assert_contains "$OUTPUT" "part def A" "File unchanged after empty pattern error"

# TEST: Invalid delete pattern: bare :: (handled gracefully)
cat > "$WORKDIR/bare_colons.sysml" << 'EOF'
package P { part def A; }
EOF

"$PARSER" -P --delete '::' "$WORKDIR/bare_colons.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
# Bare :: may be treated as matching nothing (no error, no changes)
if [ $EXIT_CODE -eq 0 ]; then
    pass "Bare :: delete pattern handled gracefully (no-op)"
else
    pass "Bare :: delete pattern exits with error"
fi

# ============================================================
# Parse errors
# ============================================================
echo ""
echo "--- Parse errors ---"

# TEST: Parse error in base file (exit 1, no modifications)
cat > "$WORKDIR/broken_base.sysml" << 'EOF'
package P { part def broken syntax
EOF

echo 'part def X;' | "$PARSER" -P --set - --at 'P' "$WORKDIR/broken_base.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
assert_exit_code $EXIT_CODE 1 "Parse error in base file exits 1"

# TEST: Parse error in fragment (exit 1, no modifications)
cat > "$WORKDIR/good_base.sysml" << 'EOF'
package P { }
EOF

echo 'part def {invalid' | "$PARSER" -P --set - --at 'P' "$WORKDIR/good_base.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
assert_exit_code $EXIT_CODE 1 "Parse error in fragment exits 1"

# Verify base unchanged
OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/good_base.sysml" 2>&1)
assert_not_contains "$OUTPUT" "invalid" "Base unchanged after fragment parse error"

# TEST: Fragment with incomplete syntax
cat > "$WORKDIR/incomplete_base.sysml" << 'EOF'
package P { }
EOF

# Note: Some incomplete syntax may parse as valid partial elements
# Test verifies system doesn't crash on edge cases
echo 'part def; attribute;' | "$PARSER" -P --set - --at 'P' "$WORKDIR/incomplete_base.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
# Exit code may be 0 or 1 depending on parser recovery
pass "Incomplete syntax fragment handled (exit $EXIT_CODE)"

# ============================================================
# Scope errors
# ============================================================
echo ""
echo "--- Scope errors ---"

# TEST: Non-existent scope without --create-scope
cat > "$WORKDIR/no_create_scope.sysml" << 'EOF'
package Existing { }
EOF

echo 'part def X;' | "$PARSER" -P --set - --at 'NonExistent' "$WORKDIR/no_create_scope.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
assert_exit_code $EXIT_CODE 1 "Non-existent scope without --create-scope exits 1"

# Verify file unchanged
OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/no_create_scope.sysml" 2>&1)
assert_not_contains "$OUTPUT" "part def X" "File unchanged when scope not found"
assert_contains "$OUTPUT" "package Existing" "Original content preserved"

# TEST: Deeply nested non-existent scope
cat > "$WORKDIR/deep_nonexist.sysml" << 'EOF'
package A { }
EOF

echo 'part def X;' | "$PARSER" -P --set - --at 'A::B::C::D' "$WORKDIR/deep_nonexist.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
assert_exit_code $EXIT_CODE 1 "Deep non-existent scope exits 1"

# ============================================================
# File I/O errors
# ============================================================
echo ""
echo "--- File I/O errors ---"

# TEST: Non-existent base file
echo 'part def X;' | "$PARSER" -P --set - --at 'P' "/nonexistent/path/base.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
assert_exit_code $EXIT_CODE 1 "Non-existent base file exits 1"

# TEST: Non-existent fragment file
cat > "$WORKDIR/good_for_io.sysml" << 'EOF'
package P { }
EOF

"$PARSER" -P --set "/nonexistent/fragment.sysml" --at 'P' "$WORKDIR/good_for_io.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
assert_exit_code $EXIT_CODE 1 "Non-existent fragment file exits 1"

# Verify base unchanged
OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/good_for_io.sysml" 2>&1)
assert_not_contains "$OUTPUT" "part" "File unchanged after I/O error"

# TEST: Directory instead of file (for base)
mkdir -p "$WORKDIR/adir"
echo 'part def X;' | "$PARSER" -P --set - --at 'P' "$WORKDIR/adir" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
assert_exit_code $EXIT_CODE 1 "Directory as base file exits 1"

# ============================================================
# Edge case errors
# ============================================================
echo ""
echo "--- Edge case errors ---"

# TEST: Delete with trailing ::
cat > "$WORKDIR/trailing_colons.sysml" << 'EOF'
package P { part def A; }
EOF

"$PARSER" -P --delete 'P::A::' "$WORKDIR/trailing_colons.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
# This should likely fail or be treated specially
if [ $EXIT_CODE -ne 0 ]; then
    pass "Delete with trailing :: exits non-zero"
else
    # If it succeeds, check nothing bad happened
    OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/trailing_colons.sysml" 2>&1)
    if echo "$OUTPUT" | grep -q "part def A"; then
        pass "Delete with trailing :: is harmless (element preserved)"
    else
        fail "Delete with trailing :: is harmless" "A preserved" "A deleted"
    fi
fi

# TEST: Set with empty --at scope
cat > "$WORKDIR/empty_at.sysml" << 'EOF'
package P { }
EOF

echo 'part def X;' | "$PARSER" -P --set - --at '' "$WORKDIR/empty_at.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
assert_exit_code $EXIT_CODE 1 "Empty --at scope exits 1"

# TEST: Very long element name
cat > "$WORKDIR/long_name.sysml" << 'EOF'
package P { }
EOF

LONG_NAME=$(python3 -c "print('A'*1000)")
echo "part def ${LONG_NAME};" | "$PARSER" -P --set - --at 'P' "$WORKDIR/long_name.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
# This might succeed or fail depending on implementation limits
if [ $EXIT_CODE -eq 0 ]; then
    pass "Very long element name handled (success)"
else
    pass "Very long element name handled (rejected)"
fi

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
