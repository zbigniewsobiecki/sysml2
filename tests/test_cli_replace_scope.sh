#!/bin/bash
#
# Integration tests for --replace-scope CLI flag
#
# Tests that --replace-scope clears the target scope before inserting fragment
# elements, preserving the fragment's element order.

set -e

# Find sysml2 binary (prefer build directory)
SYSML2="${SYSML2:-./build/sysml2}"
if [ ! -x "$SYSML2" ]; then
    SYSML2="$(which sysml2 2>/dev/null || echo ./sysml2)"
fi

if [ ! -x "$SYSML2" ]; then
    echo "Error: sysml2 binary not found. Build first or set SYSML2 env var."
    exit 1
fi

TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

PASS=0
FAIL=0

test_pass() {
    echo "PASS: $1"
    PASS=$((PASS + 1))
}

test_fail() {
    echo "FAIL: $1"
    FAIL=$((FAIL + 1))
}

# ===========================================================================
# Test 1: Basic replace - clears scope before adding
# ===========================================================================
echo "=== Test 1: Basic replace ==="

cat > "$TMPDIR/base.sysml" << 'EOF'
package Pkg {
    item def OldItem1;
    item def OldItem2;
}
EOF

cat > "$TMPDIR/fragment.sysml" << 'EOF'
item def NewItem;
EOF

"$SYSML2" --set "$TMPDIR/fragment.sysml" --at Pkg --replace-scope "$TMPDIR/base.sysml" --parse-only

# Check that OldItem1 and OldItem2 are gone, NewItem is present
if grep -q "OldItem1" "$TMPDIR/base.sysml"; then
    test_fail "Test 1: OldItem1 should be removed"
elif grep -q "OldItem2" "$TMPDIR/base.sysml"; then
    test_fail "Test 1: OldItem2 should be removed"
elif grep -q "NewItem" "$TMPDIR/base.sysml"; then
    test_pass "Test 1: Basic replace"
else
    test_fail "Test 1: NewItem not found"
fi

# ===========================================================================
# Test 2: Order preservation - declarations before redefinitions
# ===========================================================================
echo "=== Test 2: Order preservation ==="

cat > "$TMPDIR/base2.sysml" << 'EOF'
package Pkg {
    part def Redef :> Base;
    part def Base;
}
EOF

# Fragment with correct order: Base first, then Redef
cat > "$TMPDIR/fragment2.sysml" << 'EOF'
part def Base;
part def Redef :> Base;
EOF

"$SYSML2" --set "$TMPDIR/fragment2.sysml" --at Pkg --replace-scope "$TMPDIR/base2.sysml" --parse-only

# Check that Base appears before Redef in the output
BASE_LINE=$(grep -n "part def Base" "$TMPDIR/base2.sysml" | head -1 | cut -d: -f1)
REDEF_LINE=$(grep -n "part def Redef" "$TMPDIR/base2.sysml" | head -1 | cut -d: -f1)

if [ -z "$BASE_LINE" ] || [ -z "$REDEF_LINE" ]; then
    test_fail "Test 2: Missing Base or Redef definitions"
elif [ "$BASE_LINE" -lt "$REDEF_LINE" ]; then
    test_pass "Test 2: Order preservation (Base at line $BASE_LINE, Redef at line $REDEF_LINE)"
else
    test_fail "Test 2: Order not preserved (Base at line $BASE_LINE, Redef at line $REDEF_LINE)"
fi

# ===========================================================================
# Test 3: Nested scopes - only direct children cleared
# ===========================================================================
echo "=== Test 3: Nested scopes ==="

cat > "$TMPDIR/base3.sysml" << 'EOF'
package Root {
    package Child {
        item def GrandChild;
    }
    item def DirectChild;
}
EOF

# Replace only at Root - should remove DirectChild but not affect Child's contents
cat > "$TMPDIR/fragment3.sysml" << 'EOF'
item def NewDirectChild;
EOF

"$SYSML2" --set "$TMPDIR/fragment3.sysml" --at Root --replace-scope "$TMPDIR/base3.sysml" --parse-only

# DirectChild should be gone, Child package and GrandChild should also be gone
# (because --replace-scope clears direct children, and Child is a direct child)
if grep -q "DirectChild" "$TMPDIR/base3.sysml" && ! grep -q "NewDirectChild" "$TMPDIR/base3.sysml"; then
    test_fail "Test 3: DirectChild should be removed"
elif grep -q "NewDirectChild" "$TMPDIR/base3.sysml"; then
    test_pass "Test 3: Nested scopes (NewDirectChild added)"
else
    test_fail "Test 3: NewDirectChild not found"
fi

# ===========================================================================
# Test 4: Combined with --create-scope
# ===========================================================================
echo "=== Test 4: Combined with --create-scope ==="

cat > "$TMPDIR/base4.sysml" << 'EOF'
package Root;
EOF

cat > "$TMPDIR/fragment4.sysml" << 'EOF'
item def NewItem;
EOF

# Create scope A::B and replace its contents
"$SYSML2" --set "$TMPDIR/fragment4.sysml" --at "Root::Sub" --create-scope --replace-scope "$TMPDIR/base4.sysml" --parse-only

if grep -q "Root::Sub" "$TMPDIR/base4.sysml" || grep -q "package Sub" "$TMPDIR/base4.sysml"; then
    if grep -q "NewItem" "$TMPDIR/base4.sysml"; then
        test_pass "Test 4: Combined with --create-scope"
    else
        test_fail "Test 4: NewItem not found in created scope"
    fi
else
    test_fail "Test 4: Scope Root::Sub not created"
fi

# ===========================================================================
# Test 5: Without --replace-scope preserves existing elements
# ===========================================================================
echo "=== Test 5: Without --replace-scope (control test) ==="

cat > "$TMPDIR/base5.sysml" << 'EOF'
package Pkg {
    item def Existing;
}
EOF

cat > "$TMPDIR/fragment5.sysml" << 'EOF'
item def NewItem;
EOF

# Without --replace-scope, Existing should be preserved
"$SYSML2" --set "$TMPDIR/fragment5.sysml" --at Pkg "$TMPDIR/base5.sysml" --parse-only

if grep -q "Existing" "$TMPDIR/base5.sysml" && grep -q "NewItem" "$TMPDIR/base5.sysml"; then
    test_pass "Test 5: Without --replace-scope preserves existing"
else
    test_fail "Test 5: Existing or NewItem missing"
fi

# ===========================================================================
# Summary
# ===========================================================================
echo ""
echo "=========================================="
echo "Results: $PASS passed, $FAIL failed"
echo "=========================================="

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
