#!/usr/bin/env bash
# test_basic.sh - Basic execution and error handling tests for MiniShell

MINISHELL="./minishell"
if [ -f "./minishell.exe" ]; then
    MINISHELL="./minishell.exe"
fi

PASSED=0
FAILED=0

normalize_path() {
    echo "$1" | tr '\\' '/'
}

run_test() {
    local name="$1"
    local input="$2"
    local expected_out="$3"
    local expected_exit="$4"

    local out
    local ec
    out=$(printf "%b\nexit\n" "$input" | $MINISHELL 2>&1)
    ec=$?

    local norm_out
    local norm_exp
    norm_out=$(normalize_path "$out")
    norm_exp=$(normalize_path "$expected_out")

    local match=1
    if [ -n "$expected_out" ] && [[ "$norm_out" != *"$norm_exp"* ]]; then
        match=0
    fi
    if [ -n "$expected_exit" ] && [ "$ec" -ne "$expected_exit" ]; then
        match=0
    fi

    if [ "$match" -eq 1 ]; then
        echo "  [PASS] $name"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] $name"
        echo "    Input: $input"
        echo "    Output: $out"
        echo "    Exit code: $ec (expected: $expected_exit)"
        FAILED=$((FAILED + 1))
    fi
}

echo "=== Running Basic Execution Tests ==="

# 1. Simple echo
run_test "echo multiple arguments" "echo Hello World from MiniShell" "Hello World from MiniShell" 0

# 2. pwd execution
CUR_DIR=$(pwd -W 2>/dev/null || pwd)
run_test "pwd returns current directory" "pwd" "$CUR_DIR" 0

# 3. Command by path
WHOAMI_BIN=$(command -v whoami 2>/dev/null || true)
if [ -n "$WHOAMI_BIN" ]; then
    run_test "Execute by path" "$WHOAMI_BIN" "$USER" 0
fi

# 4. Unknown command does not crash shell and returns 127
out=$(printf "non_existent_command_12345\nexit\n" | $MINISHELL 2>&1)
if [[ "$out" == *"command not found"* ]]; then
    echo "  [PASS] Unknown command error message"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Unknown command error message: $out"
    FAILED=$((FAILED + 1))
fi

# 5. Shell continues operating after an unknown command
run_test "Shell continues after error" "non_existent_command_12345\necho still alive" "still alive" 0

# 6. Blank lines and whitespace handling
run_test "Blank lines and spaces" "   \n\t\n   echo spaced   \n" "spaced" 0

# 7. Long command line execution
LONG_STR=$(printf 'A%.0s' {1..300})
run_test "Long command arguments" "echo $LONG_STR" "$LONG_STR" 0

# 8. Command-line flag -c
out=$($MINISHELL -c "echo flag_c_test")
ec=$?
if [ "$out" = "flag_c_test" ] && [ "$ec" -eq 0 ]; then
    echo "  [PASS] Non-interactive -c execution"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Non-interactive -c execution"
    FAILED=$((FAILED + 1))
fi

echo "Basic tests: $PASSED passed, $FAILED failed."
[ "$FAILED" -eq 0 ] || exit 1
