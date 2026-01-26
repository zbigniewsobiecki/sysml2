#!/bin/bash
#
# Comprehensive Data Loss Test
#
# This test verifies that parsing and writing SysML files back preserves
# all data without loss. It performs multiple round-trips:
# 1. Parse original file, write to SysML
# 2. Parse written file, write again
# 3. Compare to ensure no further changes (idempotency)
#
# A pass means the written output stabilizes after one round-trip.

set -e

# Find sysml2 binary
SYSML2="${SYSML2:-./build/sysml2}"
if [ ! -x "$SYSML2" ]; then
    SYSML2="$(which sysml2 2>/dev/null || echo ./sysml2)"
fi

if [ ! -x "$SYSML2" ]; then
    echo "Error: sysml2 binary not found. Build first or set SYSML2 env var."
    exit 1
fi

FIXTURE="tests/fixtures/comprehensive_data_loss.sysml"
if [ ! -f "$FIXTURE" ]; then
    echo "Error: Test fixture not found: $FIXTURE"
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
# Test 1: Parse-only succeeds
# ===========================================================================
echo "=== Test 1: Parse-only succeeds ==="

# Only check for errors in the specific fixture file
if "$SYSML2" "$FIXTURE" --parse-only 2>&1 | grep -q "comprehensive_data_loss.sysml.*error:"; then
    test_fail "Test 1: Parse failed"
else
    test_pass "Test 1: Parse-only succeeds"
fi

# ===========================================================================
# Test 2: Round-trip write preserves data
# ===========================================================================
echo "=== Test 2: Round-trip write preserves data ==="

# First pass: parse and write to SysML
"$SYSML2" "$FIXTURE" -f sysml -o "$TMPDIR/pass1.sysml" --parse-only 2>/dev/null

# Second pass: parse written file and write again
"$SYSML2" "$TMPDIR/pass1.sysml" -f sysml -o "$TMPDIR/pass2.sysml" --parse-only 2>/dev/null

# Compare: should be identical (idempotency)
if diff -q "$TMPDIR/pass1.sysml" "$TMPDIR/pass2.sysml" > /dev/null 2>&1; then
    test_pass "Test 2: Round-trip is idempotent"
else
    test_fail "Test 2: Round-trip changed output"
    echo "  First 10 lines of diff:"
    diff "$TMPDIR/pass1.sysml" "$TMPDIR/pass2.sysml" | head -20 || true
fi

# ===========================================================================
# Test 3: Element count preserved
# ===========================================================================
echo "=== Test 3: Element count preserved ==="

ORIGINAL_ELEMENTS=$("$SYSML2" "$FIXTURE" -f json --parse-only 2>/dev/null | grep -c '"id":' || echo "0")
ROUNDTRIP_ELEMENTS=$("$SYSML2" "$TMPDIR/pass1.sysml" -f json --parse-only 2>/dev/null | grep -c '"id":' || echo "0")

if [ "$ORIGINAL_ELEMENTS" -eq "$ROUNDTRIP_ELEMENTS" ]; then
    test_pass "Test 3: Element count preserved ($ORIGINAL_ELEMENTS elements)"
else
    test_fail "Test 3: Element count changed (original: $ORIGINAL_ELEMENTS, roundtrip: $ROUNDTRIP_ELEMENTS)"
fi

# ===========================================================================
# Test 4: Specific feature preservation checks
# ===========================================================================
echo "=== Test 4: Specific feature preservation ==="

# Check that key elements are present in round-tripped file
CHECKS=(
    "abstract part def Vehicle"
    "private part def Engine"
    "port def FuelPort"
    "action def StartVehicle"
    "action def DriveAction"
    "state def VehicleState"
    "constraint def SpeedLimit"
    "requirement def VehicleSafetyRequirement"
    "connection def FuelConnection"
    "metadata def Important"
    "enum def Color"
    "enum def Priority"
    "library package TypeLibrary"
    "part def ComplexSystem"
    "part def Derived2 :> Derived1"
    "item def DataPacket"
    "occurrence def Event1"
    "calc def TotalMass"
    "use case def DriveVehicle"
    "verification def SafetyVerification"
    "viewpoint def EngineerViewpoint"
    "allocation def FunctionToComponent"
    "concern def SafetyConcern"
)

FEATURE_FAIL=0
for check in "${CHECKS[@]}"; do
    if ! grep -q "$check" "$TMPDIR/pass1.sysml"; then
        echo "  Missing: $check"
        FEATURE_FAIL=$((FEATURE_FAIL + 1))
    fi
done

if [ "$FEATURE_FAIL" -eq 0 ]; then
    test_pass "Test 4: All ${#CHECKS[@]} key features preserved"
else
    test_fail "Test 4: $FEATURE_FAIL features missing"
fi

# ===========================================================================
# Test 5: Metadata annotations preserved
# ===========================================================================
echo "=== Test 5: Metadata annotations preserved ==="

METADATA_CHECKS=(
    "@Deprecated"
    "@Important"
    "#Important"
    "#CustomTag"
)

METADATA_FAIL=0
for check in "${METADATA_CHECKS[@]}"; do
    if ! grep -q "$check" "$TMPDIR/pass1.sysml"; then
        echo "  Missing metadata: $check"
        METADATA_FAIL=$((METADATA_FAIL + 1))
    fi
done

if [ "$METADATA_FAIL" -eq 0 ]; then
    test_pass "Test 5: All ${#METADATA_CHECKS[@]} metadata annotations preserved"
else
    test_fail "Test 5: $METADATA_FAIL metadata annotations missing"
fi

# ===========================================================================
# Test 6: Documentation preserved
# ===========================================================================
echo "=== Test 6: Documentation preserved ==="

DOC_COUNT_ORIG=$(grep -c "doc /\*" "$FIXTURE" || echo "0")
DOC_COUNT_ROUNDTRIP=$(grep -c "doc /\*" "$TMPDIR/pass1.sysml" || echo "0")

if [ "$DOC_COUNT_ORIG" -eq "$DOC_COUNT_ROUNDTRIP" ]; then
    test_pass "Test 6: Documentation count preserved ($DOC_COUNT_ORIG docs)"
else
    test_fail "Test 6: Documentation count changed (original: $DOC_COUNT_ORIG, roundtrip: $DOC_COUNT_ROUNDTRIP)"
fi

# ===========================================================================
# Test 7: Shorthand statements preserved (redefinitions)
# ===========================================================================
echo "=== Test 7: Shorthand statements preserved ==="

REDEF_COUNT_ORIG=$(grep -c ":>>" "$FIXTURE" || echo "0")
REDEF_COUNT_ROUNDTRIP=$(grep -c ":>>" "$TMPDIR/pass1.sysml" || echo "0")

if [ "$REDEF_COUNT_ORIG" -le "$REDEF_COUNT_ROUNDTRIP" ]; then
    test_pass "Test 7: Redefinition count preserved ($REDEF_COUNT_ROUNDTRIP redefinitions)"
else
    test_fail "Test 7: Redefinition count decreased (original: $REDEF_COUNT_ORIG, roundtrip: $REDEF_COUNT_ROUNDTRIP)"
fi

# ===========================================================================
# Test 8: JSON output stability
# ===========================================================================
echo "=== Test 8: JSON output stability ==="

"$SYSML2" "$FIXTURE" -f json --parse-only 2>/dev/null > "$TMPDIR/json1.json"
"$SYSML2" "$TMPDIR/pass1.sysml" -f json --parse-only 2>/dev/null > "$TMPDIR/json2.json"

# Compare element IDs (sorted)
ORIG_IDS=$(grep -o '"id": "[^"]*"' "$TMPDIR/json1.json" | sort | uniq)
ROUNDTRIP_IDS=$(grep -o '"id": "[^"]*"' "$TMPDIR/json2.json" | sort | uniq)

if [ "$ORIG_IDS" = "$ROUNDTRIP_IDS" ]; then
    test_pass "Test 8: JSON element IDs match"
else
    test_fail "Test 8: JSON element IDs differ"
    echo "  Original IDs: $(echo "$ORIG_IDS" | wc -l | tr -d ' ') unique"
    echo "  Roundtrip IDs: $(echo "$ROUNDTRIP_IDS" | wc -l | tr -d ' ') unique"
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
