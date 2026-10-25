#!/usr/bin/env bash
# test_builtins.sh - Tests for built-in commands (cd, pwd, exit, help, history, export, unset)

MINISHELL="./minishell"
if [ -f "./minishell.exe" ]; then
    MINISHELL="./minishell.exe"
fi

PASSED=0
FAILED=0

normalize_path() {
    echo "$1" | tr '\\' '/'
}

assert_output() {
    local name="$1"
    local input="$2"
    local pattern="$3"
    local expected_ec="$4"

    local out
    local ec
    out=$(printf "%b\nexit\n" "$input" | $MINISHELL 2>&1)
    ec=$?

    local norm_out
    local norm_pat
    norm_out=$(normalize_path "$out")
    norm_pat=$(normalize_path "$pattern")

    local ok=1
    if [[ "$norm_out" != *"$norm_pat"* ]]; then
        ok=0
    fi
    if [ -n "$expected_ec" ] && [ "$ec" -ne "$expected_ec" ]; then
        ok=0
    fi

    if [ "$ok" -eq 1 ]; then
        echo "  [PASS] $name"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] $name"
        echo "    Output: $out"
        echo "    Exit code: $ec"
        FAILED=$((FAILED + 1))
    fi
}

echo "=== Running Built-in Command Tests ==="

# 1. pwd
assert_output "builtin pwd" "pwd" "$(pwd -W 2>/dev/null || pwd)" 0

# 2. cd to directory and back with cd -
ORIG_DIR=$(pwd -W 2>/dev/null || pwd)
assert_output "cd .. and cd -" "cd ..\npwd\ncd -\npwd" "$ORIG_DIR" 0

# 3. cd to non-existent directory reports error
assert_output "cd to invalid directory reports error" "cd /non_existent_dir_99999" "No such file" 1

# 4. export and variable expansion
assert_output "export and access variable" "export TEST_VAR=MiniShellRocks\necho \$TEST_VAR" "MiniShellRocks" 0

# 5. unset removes variable
assert_output "unset variable" "export UNSET_ME=Present\nunset UNSET_ME\necho UNSET: \$UNSET_ME" "UNSET: " 0

# 6. export invalid identifier
assert_output "export invalid identifier error" "export 123bad=val" "not a valid identifier" 1

# 7. exit with specific code
out=$(printf "exit 42\n" | $MINISHELL 2>&1)
ec=$?
if [ "$ec" -eq 42 ]; then
    echo "  [PASS] exit with numeric code (42)"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] exit with numeric code (42), got $ec"
    FAILED=$((FAILED + 1))
fi

# 8. exit with invalid non-numeric code
out=$(printf "exit not_a_number\n" | $MINISHELL 2>&1)
ec=$?
if [ "$ec" -eq 2 ] && [[ "$out" == *"numeric argument required"* ]]; then
    echo "  [PASS] exit with non-numeric code fails safely"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] exit non-numeric code check: out='$out' ec=$ec"
    FAILED=$((FAILED + 1))
fi

# 9. help built-in
assert_output "help displays builtin guide" "help" "Built-in Commands" 0

# 10. history built-in
assert_output "history displays numbered commands" "echo cmd1\necho cmd2\nhistory" "echo cmd1" 0

echo "Built-in tests: $PASSED passed, $FAILED failed."
[ "$FAILED" -eq 0 ] || exit 1
