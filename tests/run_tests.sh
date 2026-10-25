#!/usr/bin/env bash
# run_tests.sh - Master test runner for MiniShell test suite

set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
cd "$PROJECT_ROOT"

echo "=========================================================="
echo "          MINISHELL AUTOMATED VERIFICATION SUITE         "
echo "=========================================================="

TEST_SCRIPTS=(
    "tests/test_basic.sh"
    "tests/test_builtins.sh"
    "tests/test_parser.sh"
    "tests/test_redirection.sh"
    "tests/test_pipelines.sh"
    "tests/test_signals.sh"
)

TOTAL_SUITES=${#TEST_SCRIPTS[@]}
SUITES_PASSED=0
SUITES_FAILED=0

for script in "${TEST_SCRIPTS[@]}"; do
    if [ -f "$script" ]; then
        echo ""
        if bash "$script"; then
            SUITES_PASSED=$((SUITES_PASSED + 1))
        else
            echo "FAILED: $script"
            SUITES_FAILED=$((SUITES_FAILED + 1))
        fi
    else
        echo "Missing test script: $script"
        SUITES_FAILED=$((SUITES_FAILED + 1))
    fi
done

echo ""
echo "=========================================================="
echo "                 TEST SUMMARY RESULTS                     "
echo "=========================================================="
echo "Total Test Suites: $TOTAL_SUITES"
echo "Suites Passed:     $SUITES_PASSED"
echo "Suites Failed:     $SUITES_FAILED"
echo "=========================================================="

if [ "$SUITES_FAILED" -eq 0 ]; then
    echo "ALL TEST SUITES PASSED SUCCESSFULLY!"
    exit 0
else
    echo "SOME TESTS FAILED!"
    exit 1
fi
