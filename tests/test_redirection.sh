#!/usr/bin/env bash
# test_redirection.sh - Tests for <, >, >>, 2> and builtin redirection

MINISHELL="./minishell"
if [ -f "./minishell.exe" ]; then
    MINISHELL="./minishell.exe"
fi

RAW_TEMP=$(mktemp -d -t minishell_test_redir_XXXXXX)
TEMP_DIR=$(cygpath -m "$RAW_TEMP" 2>/dev/null || echo "$RAW_TEMP")
trap 'rm -rf "$RAW_TEMP"' EXIT

PASSED=0
FAILED=0

echo "=== Running Redirection Tests ==="

# 1. Output redirection >
printf "echo 'Hello Redir' > \"%s/out.txt\"\nexit\n" "$TEMP_DIR" | $MINISHELL >/dev/null 2>&1
if [ -f "$RAW_TEMP/out.txt" ] && grep -q "Hello Redir" "$RAW_TEMP/out.txt"; then
    echo "  [PASS] Output redirection > creates file with content"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Output redirection > failed"
    FAILED=$((FAILED + 1))
fi

# 2. Output truncation >
printf "echo 'First Line' > \"%s/trunc.txt\"\necho 'Second Line' > \"%s/trunc.txt\"\nexit\n" "$TEMP_DIR" "$TEMP_DIR" | $MINISHELL >/dev/null 2>&1
if [ -f "$RAW_TEMP/trunc.txt" ] && ! grep -q "First Line" "$RAW_TEMP/trunc.txt" && grep -q "Second Line" "$RAW_TEMP/trunc.txt"; then
    echo "  [PASS] Output redirection > truncates previous content"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Output redirection truncation failed"
    FAILED=$((FAILED + 1))
fi

# 3. Append redirection >>
printf "echo 'Line 1' > \"%s/append.txt\"\necho 'Line 2' >> \"%s/append.txt\"\nexit\n" "$TEMP_DIR" "$TEMP_DIR" | $MINISHELL >/dev/null 2>&1
lines=$(wc -l < "$RAW_TEMP/append.txt" 2>/dev/null | tr -d ' ')
if [ "$lines" -eq 2 ] && grep -q "Line 1" "$RAW_TEMP/append.txt" && grep -q "Line 2" "$RAW_TEMP/append.txt"; then
    echo "  [PASS] Append redirection >> preserves existing content"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Append redirection >> failed (lines: $lines)"
    FAILED=$((FAILED + 1))
fi

# 4. Input redirection <
echo "Input file data 12345" > "$RAW_TEMP/in.txt"
out=$(printf "cat < \"%s/in.txt\"\nexit\n" "$TEMP_DIR" | $MINISHELL 2>&1)
if [[ "$out" == *"Input file data 12345"* ]]; then
    echo "  [PASS] Input redirection < correctly reads file"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Input redirection < failed: $out"
    FAILED=$((FAILED + 1))
fi

# 5. Stderr redirection 2>
printf "cat \"%s/does_not_exist.txt\" 2> \"%s/err.txt\"\nexit\n" "$TEMP_DIR" "$TEMP_DIR" | $MINISHELL >/dev/null 2>&1
if [ -f "$RAW_TEMP/err.txt" ] && [ -s "$RAW_TEMP/err.txt" ]; then
    echo "  [PASS] Standard error redirection 2> captures error"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Standard error redirection 2> failed"
    FAILED=$((FAILED + 1))
fi

# 6. Builtin redirection does not permanently redirect shell stdout
out=$(printf "help > \"%s/help.txt\"\necho 'terminal stdout restored'\nexit\n" "$TEMP_DIR" | $MINISHELL 2>&1)
if [ -f "$RAW_TEMP/help.txt" ] && grep -q "Built-in" "$RAW_TEMP/help.txt" && [[ "$out" == *"terminal stdout restored"* ]]; then
    echo "  [PASS] Built-in redirection restores parent shell stdout"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Built-in redirection restore failed: $out"
    FAILED=$((FAILED + 1))
fi

# 7. Redirection failure on non-existent input aborts command safely
out=$(printf "cat < \"%s/missing.txt\"\necho 'continued after redir fail'\nexit\n" "$TEMP_DIR" | $MINISHELL 2>&1)
if [[ "$out" == *"No such file"* ]] && [[ "$out" == *"continued after redir fail"* ]]; then
    echo "  [PASS] Input redirection failure reported safely and shell continues"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Redirection failure handling failed: $out"
    FAILED=$((FAILED + 1))
fi

echo "Redirection tests: $PASSED passed, $FAILED failed."
[ "$FAILED" -eq 0 ] || exit 1
