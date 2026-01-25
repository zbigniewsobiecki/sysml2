#!/bin/bash
# Test validation script for sysml2
# Counts how many files pass/fail validation

cd /Users/zbigniew/Code/sysml2/build

echo "Running validation tests..."
RESULTS=$(find /Users/zbigniew/Code/SysML-v2-Release \( -name "*.sysml" -o -name "*.kerml" \) -type f \
    -exec sh -c './sysml2 --no-resolve "$1" >/dev/null 2>&1 && echo ok || echo fail' _ {} \;)

OK=$(echo "$RESULTS" | grep -c "^ok$")
FAIL=$(echo "$RESULTS" | grep -c "^fail$")
TOTAL=$((OK + FAIL))

echo "=================================="
echo "Validation Results:"
echo "  OK:   $OK / $TOTAL"
echo "  FAIL: $FAIL / $TOTAL"
echo "=================================="

# Show E3004 files if requested
if [ "$1" = "-v" ] || [ "$1" = "--verbose" ]; then
    echo ""
    echo "Files with E3004 (duplicate) errors:"
    find /Users/zbigniew/Code/SysML-v2-Release \( -name "*.sysml" -o -name "*.kerml" \) -type f 2>/dev/null | \
        while IFS= read -r f; do
            if ./sysml2 --no-resolve "$f" 2>&1 | grep -q "E3004"; then
                echo "  $f"
            fi
        done
fi
