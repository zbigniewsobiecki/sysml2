#!/bin/bash
#
# Integration test for CLI CRUD operations
#
# Tests: create, format, set, delete, select operations
# Ensures each operation produces expected results
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

echo "=== CLI CRUD Integration Tests ==="
echo "Parser: $PARSER"
echo "Workdir: $WORKDIR"
echo ""

# Check parser exists
if [ ! -x "$PARSER" ]; then
    echo "ERROR: Parser not found or not executable: $PARSER"
    exit 1
fi

# ============================================================
# TEST 1: Create and parse a new file
# ============================================================
echo "--- Test 1: Create and parse new file ---"

cat > "$WORKDIR/model.sysml" << 'EOF'
package TestModel {
    part def Component;
    part def System {
        part comp : Component;
    }
}
EOF

OUTPUT=$("$PARSER" -P "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "Parse new file successfully"

# ============================================================
# TEST 2: Format file and verify structure
# ============================================================
echo ""
echo "--- Test 2: Format file ---"

OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "Format file successfully"
assert_contains "$OUTPUT" "package TestModel" "Contains package declaration"
assert_contains "$OUTPUT" "part def Component" "Contains part def Component"
assert_contains "$OUTPUT" "part def System" "Contains part def System"
assert_contains "$OUTPUT" "part comp : Component" "Contains part usage"

# ============================================================
# TEST 3: Set operation - add new element
# ============================================================
echo ""
echo "--- Test 3: Set operation - add element ---"

echo 'part def NewPart;' | "$PARSER" -P --set - --at 'TestModel' "$WORKDIR/model.sysml" 2>&1
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "Set operation completes"

OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/model.sysml" 2>&1)
assert_contains "$OUTPUT" "part def NewPart" "New element added"

# ============================================================
# TEST 4: Set operation - add nested element
# ============================================================
echo ""
echo "--- Test 4: Set operation - add nested element ---"

echo 'attribute name : String;' | "$PARSER" -P --set - --at 'TestModel::System' "$WORKDIR/model.sysml" 2>&1
EXIT_CODE=$?

OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/model.sysml" 2>&1)
assert_contains "$OUTPUT" "attribute name" "Nested attribute added"

# ============================================================
# TEST 5: Delete operation - remove element
# ============================================================
echo ""
echo "--- Test 5: Delete operation ---"

"$PARSER" -P --delete 'TestModel::NewPart' "$WORKDIR/model.sysml" 2>&1
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "Delete operation completes"

OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/model.sysml" 2>&1)
assert_not_contains "$OUTPUT" "part def NewPart" "Element removed"
assert_contains "$OUTPUT" "part def Component" "Other elements preserved"

# ============================================================
# TEST 6: Select operation - query elements
# ============================================================
echo ""
echo "--- Test 6: Select operation ---"

OUTPUT=$("$PARSER" -P --select 'TestModel::System' -f sysml "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "Select operation completes"
assert_contains "$OUTPUT" "part def System" "Selected element returned"

# ============================================================
# TEST 7: Select with wildcard
# ============================================================
echo ""
echo "--- Test 7: Select with wildcard ---"

OUTPUT=$("$PARSER" -P --select 'TestModel::*' -f sysml "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "Select wildcard completes"
assert_contains "$OUTPUT" "part def Component" "Wildcard selects Component"
assert_contains "$OUTPUT" "part def System" "Wildcard selects System"

# ============================================================
# TEST 8: Dry-run mode
# ============================================================
echo ""
echo "--- Test 8: Dry-run mode ---"

BEFORE=$("$PARSER" -P -f sysml "$WORKDIR/model.sysml" 2>&1)

echo 'part def DryRunPart;' | "$PARSER" -P --set - --at 'TestModel' --dry-run "$WORKDIR/model.sysml" 2>&1
EXIT_CODE=$?

AFTER=$("$PARSER" -P -f sysml "$WORKDIR/model.sysml" 2>&1)

assert_exit_code $EXIT_CODE 0 "Dry-run completes"
assert_not_contains "$AFTER" "DryRunPart" "Dry-run does not modify file"

# ============================================================
# TEST 9: Create scope with --create-scope
# ============================================================
echo ""
echo "--- Test 9: Create scope ---"

cat > "$WORKDIR/model2.sysml" << 'EOF'
package Root;
EOF

echo 'part def Leaf;' | "$PARSER" -P --set - --at 'Root::NewPackage' --create-scope "$WORKDIR/model2.sysml" 2>&1
EXIT_CODE=$?

OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/model2.sysml" 2>&1)
assert_contains "$OUTPUT" "package NewPackage" "New scope created"
assert_contains "$OUTPUT" "part def Leaf" "Element added to new scope"

# ============================================================
# TEST 10: JSON output format
# ============================================================
echo ""
echo "--- Test 10: JSON output format ---"

OUTPUT=$("$PARSER" -P -f json "$WORKDIR/model.sysml" 2>&1)
EXIT_CODE=$?

assert_exit_code $EXIT_CODE 0 "JSON output completes"
assert_contains "$OUTPUT" '"type":' "JSON contains type field"
assert_contains "$OUTPUT" '"name":' "JSON contains name field"

# ============================================================
# TEST 11: Multiple deletes
# ============================================================
echo ""
echo "--- Test 11: Multiple deletes ---"

cat > "$WORKDIR/model3.sysml" << 'EOF'
package Multi {
    part def A;
    part def B;
    part def C;
}
EOF

"$PARSER" -P --delete 'Multi::A' --delete 'Multi::C' "$WORKDIR/model3.sysml" 2>&1
EXIT_CODE=$?

OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/model3.sysml" 2>&1)
assert_not_contains "$OUTPUT" "part def A" "A deleted"
assert_contains "$OUTPUT" "part def B" "B preserved"
assert_not_contains "$OUTPUT" "part def C" "C deleted"

# ============================================================
# TEST 12: Recursive delete
# ============================================================
echo ""
echo "--- Test 12: Recursive delete ---"

cat > "$WORKDIR/model4.sysml" << 'EOF'
package ToDelete {
    part def Inner1;
    package Nested {
        part def Inner2;
    }
}
package ToKeep {
    part def Keeper;
}
EOF

"$PARSER" -P --delete 'ToDelete::**' "$WORKDIR/model4.sysml" 2>&1

OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/model4.sysml" 2>&1)
assert_not_contains "$OUTPUT" "Inner1" "Recursive deletes Inner1"
assert_not_contains "$OUTPUT" "Inner2" "Recursive deletes Inner2"
assert_contains "$OUTPUT" "package ToKeep" "ToKeep preserved"
assert_contains "$OUTPUT" "part def Keeper" "Keeper preserved"

# ============================================================
# TEST 13: Set with complex element
# ============================================================
echo ""
echo "--- Test 13: Set with complex element ---"

cat > "$WORKDIR/model5.sysml" << 'EOF'
package Complex;
EOF

echo 'requirement def UserReq { doc /*User requirement*/ attribute priority : String; }' | \
    "$PARSER" -P --set - --at 'Complex' "$WORKDIR/model5.sysml" 2>&1

OUTPUT=$("$PARSER" -P -f sysml "$WORKDIR/model5.sysml" 2>&1)
assert_contains "$OUTPUT" "requirement def UserReq" "Complex element added"
assert_contains "$OUTPUT" "doc /\*User requirement\*/" "Doc comment preserved"
assert_contains "$OUTPUT" "attribute priority" "Attribute preserved"

# ============================================================
# TEST 14: Fix mode (in-place formatting)
# ============================================================
echo ""
echo "--- Test 14: Fix mode ---"

cat > "$WORKDIR/unformatted.sysml" << 'EOF'
package Ugly{part def A;part def B;}
EOF

"$PARSER" -P --fix "$WORKDIR/unformatted.sysml" 2>&1
EXIT_CODE=$?

OUTPUT=$(cat "$WORKDIR/unformatted.sysml")
assert_exit_code $EXIT_CODE 0 "Fix mode completes"
assert_contains "$OUTPUT" "package Ugly {" "Fixed has proper spacing"

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
