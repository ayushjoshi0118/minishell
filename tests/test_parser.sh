#!/usr/bin/env bash
# test_parser.sh - Tests for parsing quotes, escapes, variables, syntax errors

MINISHELL="./minishell"
if [ -f "./minishell.exe" ]; then
    MINISHELL="./minishell.exe"
fi

PASSED=0
FAILED=0

assert_parse() {
    local name="$1"
    local input="$2"
    local expected_pattern="$3"
    local expected_ec="$4"

    local out
    local ec
    out=$(printf "%b\nexit\n" "$input" | $MINISHELL 2>&1)
    ec=$?

    local ok=1
    if [ -n "$expected_pattern" ] && [[ "$out" != *"$expected_pattern"* ]]; then
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
        echo "    Input: $input"
        echo "    Output: $out"
        echo "    Exit code: $ec (expected: $expected_ec)"
        FAILED=$((FAILED + 1))
    fi
}

echo "=== Running Parser Tests ==="

# 1. Single quotes preserve literal text with spaces
assert_parse "Single quotes preserve spaces" "echo 'hello   world'" "hello   world" 0

# 2. Double quotes preserve spaces and allow expansion
assert_parse "Double quotes expansion" "export V=test\necho \"value is: \$V\"" "value is: test" 0

# 3. Escaped spaces outside quotes
assert_parse "Escaped space" "echo foo\\ bar" "foo bar" 0

# 4. Mixed quotes concatenated
assert_parse "Concatenated quotes" "echo 'hello '\"world\"" "hello world" 0

# 5. Unmatched single quote rejected
assert_parse "Unmatched single quote detection" "echo 'unmatched" "unmatched single quote" 2

# 6. Unmatched double quote rejected
assert_parse "Unmatched double quote detection" "echo \"unmatched" "unmatched double quote" 2

# 7. Redirection missing filename rejected
assert_parse "Redirection missing target" "echo hello >" "syntax error near unexpected token" 2

# 8. Leading pipe rejected
assert_parse "Leading pipe error" "| echo bad" "syntax error near unexpected token '|'" 2

# 9. Trailing pipe rejected
assert_parse "Trailing pipe error" "echo bad |" "syntax error near unexpected token" 2

# 10. Consecutive pipes rejected
assert_parse "Double pipe rejected" "echo a || echo b" "syntax error near unexpected token" 2

# 11. Special variable $? and $$
assert_parse "Special variable expansion" "echo status=\$? pid=\$\$" "status=0 pid=" 0

echo "Parser tests: $PASSED passed, $FAILED failed."
[ "$FAILED" -eq 0 ] || exit 1
