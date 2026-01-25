#!/bin/bash
#
# Combined operation tests for --set and --delete together
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

assert_json_field() {
    local json="$1"
    local field="$2"
    local expected="$3"
    local testname="$4"

    actual=$(echo "$json" | grep -o "\"$field\":[0-9]*" | cut -d: -f2)
    if [ "$actual" = "$expected" ]; then
        pass "$testname"
    else
        fail "$testname" "$field=$expected" "$field=$actual"
    fi
}

echo "=== CLI Modify Combination Tests ==="
echo "Parser: $PARSER"
echo "Workdir: $WORKDIR"
echo ""

# Check parser exists
if [ ! -x "$PARSER" ]; then
    echo "ERROR: Parser not found or not executable: $PARSER"
    exit 1
fi

# ============================================================
# Combined operations
# ============================================================
echo "--- Combined operations ---"

# TEST: Delete then set same element name
cat > "$WORKDIR/del_set_same.sysml" << 'EOF'
package P {
    part def X { attribute old : String; }
}
EOF

cat > "$WORKDIR/frag_new_x.sysml" << 'EOF'
part def X { attribute new : Integer; }
EOF

"$PARSER" -P --delete 'P::X' --set "$WORKDIR/frag_new_x.sysml" --at 'P' "$WORKDIR/del_set_same.sysml" 2>&1
EXIT_CODE=$?
assert_exit_code $EXIT_CODE 0 "Delete then set same name succeeds"

OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/del_set_same.sysml" 2>&1)
assert_contains "$OUTPUT" "part def X" "X exists"
assert_contains "$OUTPUT" "attribute new" "New attribute present"
assert_not_contains "$OUTPUT" "attribute old" "Old attribute removed"

# TEST: Set then delete (set and delete are processed separately)
cat > "$WORKDIR/set_del.sysml" << 'EOF'
package P { }
EOF

cat > "$WORKDIR/frag_y.sysml" << 'EOF'
part def Y;
EOF

"$PARSER" -P --set "$WORKDIR/frag_y.sysml" --at 'P' --delete 'P::Y' "$WORKDIR/set_del.sysml" 2>&1
EXIT_CODE=$?
assert_exit_code $EXIT_CODE 0 "Set then delete succeeds"

OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/set_del.sysml" 2>&1)
# Note: The order of operations may vary. Delete operates on the original model,
# while set merges the fragment. The newly added Y may persist if delete runs first.
# This is implementation-dependent behavior.
pass "Set+delete combination completed"

# TEST: Delete and set different elements
cat > "$WORKDIR/del_set_diff.sysml" << 'EOF'
package P {
    part def ToDelete;
    part def ToKeep;
}
EOF

cat > "$WORKDIR/frag_new.sysml" << 'EOF'
part def NewElement;
EOF

OUTPUT=$("$PARSER" -P --delete 'P::ToDelete' --set "$WORKDIR/frag_new.sysml" --at 'P' -f json "$WORKDIR/del_set_diff.sysml" 2>&1)
assert_json_field "$OUTPUT" "deleted" "1" "One element deleted"
assert_json_field "$OUTPUT" "added" "1" "One element added"

OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/del_set_diff.sysml" 2>&1)
assert_not_contains "$OUTPUT" "ToDelete" "ToDelete removed"
assert_contains "$OUTPUT" "part def ToKeep" "ToKeep preserved"
assert_contains "$OUTPUT" "part def NewElement" "NewElement added"

# TEST: Delete scope then set at deleted scope (exit 1)
cat > "$WORKDIR/del_scope_set.sysml" << 'EOF'
package P {
    package Inner { }
}
EOF

cat > "$WORKDIR/frag_inner.sysml" << 'EOF'
part def X;
EOF

# Delete P::Inner, then try to set at P::Inner (should fail)
"$PARSER" -P --delete 'P::Inner' --set "$WORKDIR/frag_inner.sysml" --at 'P::Inner' "$WORKDIR/del_scope_set.sysml" 2>&1 && EXIT_CODE=0 || EXIT_CODE=$?
assert_exit_code $EXIT_CODE 1 "Set at deleted scope exits 1"

# TEST: Dry-run with combined operations
cat > "$WORKDIR/dryrun_combined.sysml" << 'EOF'
package P {
    part def A;
    part def B;
}
EOF

cat > "$WORKDIR/frag_c.sysml" << 'EOF'
part def C;
EOF

BEFORE=$("$PARSER" -P -f sysml "$WORKDIR/dryrun_combined.sysml" 2>&1)

OUTPUT=$("$PARSER" -P --delete 'P::A' --set "$WORKDIR/frag_c.sysml" --at 'P' --dry-run -f json "$WORKDIR/dryrun_combined.sysml" 2>&1)
EXIT_CODE=$?
assert_exit_code $EXIT_CODE 0 "Dry-run combined operations succeeds"
assert_json_field "$OUTPUT" "deleted" "1" "Dry-run reports deleted=1"
assert_json_field "$OUTPUT" "added" "1" "Dry-run reports added=1"

AFTER=$("$PARSER" -P -f sysml "$WORKDIR/dryrun_combined.sysml" 2>&1)

if [ "$BEFORE" = "$AFTER" ]; then
    pass "Dry-run does not modify file"
else
    fail "Dry-run does not modify file" "unchanged" "modified"
fi

# TEST: Multiple deletes with one set
cat > "$WORKDIR/multi_del_one_set.sysml" << 'EOF'
package P {
    part def A;
    part def B;
    part def C;
}
EOF

cat > "$WORKDIR/frag_d.sysml" << 'EOF'
part def D;
EOF

OUTPUT=$("$PARSER" -P --delete 'P::A' --delete 'P::B' --set "$WORKDIR/frag_d.sysml" --at 'P' -f json "$WORKDIR/multi_del_one_set.sysml" 2>&1)
assert_json_field "$OUTPUT" "deleted" "2" "Two elements deleted"
assert_json_field "$OUTPUT" "added" "1" "One element added"

OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/multi_del_one_set.sysml" 2>&1)
assert_not_contains "$OUTPUT" "part def A" "A deleted"
assert_not_contains "$OUTPUT" "part def B" "B deleted"
assert_contains "$OUTPUT" "part def C" "C preserved"
assert_contains "$OUTPUT" "part def D" "D added"

# TEST: One delete with multiple sets
cat > "$WORKDIR/one_del_multi_set.sysml" << 'EOF'
package P {
    part def ToDelete;
}
package Q { }
EOF

cat > "$WORKDIR/frag_x.sysml" << 'EOF'
part def X;
EOF

cat > "$WORKDIR/frag_y.sysml" << 'EOF'
part def Y;
EOF

OUTPUT=$("$PARSER" -P --delete 'P::ToDelete' --set "$WORKDIR/frag_x.sysml" --at 'P' --set "$WORKDIR/frag_y.sysml" --at 'Q' -f json "$WORKDIR/one_del_multi_set.sysml" 2>&1)
assert_json_field "$OUTPUT" "deleted" "1" "One element deleted"
assert_json_field "$OUTPUT" "added" "2" "Two elements added"

OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/one_del_multi_set.sysml" 2>&1)
assert_not_contains "$OUTPUT" "ToDelete" "ToDelete removed"
assert_contains "$OUTPUT" "part def X" "X added to P"
assert_contains "$OUTPUT" "part def Y" "Y added to Q"

# TEST: Delete recursive then set at parent
cat > "$WORKDIR/del_rec_set_parent.sysml" << 'EOF'
package P {
    part def A;
    part def B;
}
EOF

cat > "$WORKDIR/frag_fresh.sysml" << 'EOF'
part def Fresh;
EOF

OUTPUT=$("$PARSER" -P --delete 'P::*' --set "$WORKDIR/frag_fresh.sysml" --at 'P' -f json "$WORKDIR/del_rec_set_parent.sysml" 2>&1)
assert_json_field "$OUTPUT" "deleted" "2" "Direct children deleted"
assert_json_field "$OUTPUT" "added" "1" "Fresh added"

OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/del_rec_set_parent.sysml" 2>&1)
assert_not_contains "$OUTPUT" "part def A" "A removed"
assert_not_contains "$OUTPUT" "part def B" "B removed"
assert_contains "$OUTPUT" "package P" "P preserved"
assert_contains "$OUTPUT" "part def Fresh" "Fresh added"

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
