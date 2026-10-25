#!/usr/bin/env bash
# test_pipelines.sh - Tests for two-stage and multi-stage pipelines and deadlock prevention

MINISHELL="./minishell"
if [ -f "./minishell.exe" ]; then
    MINISHELL="./minishell.exe"
fi

RAW_TEMP=$(mktemp -d -t minishell_test_pipe_XXXXXX)
TEMP_DIR=$(cygpath -m "$RAW_TEMP" 2>/dev/null || echo "$RAW_TEMP")
trap 'rm -rf "$RAW_TEMP"' EXIT

PASSED=0
FAILED=0

echo "=== Running Pipeline Tests ==="

# 1. Basic two-stage pipeline
out=$(printf "echo 'hello world' | grep 'world'\nexit\n" | $MINISHELL 2>&1)
if [[ "$out" == *"hello world"* ]]; then
    echo "  [PASS] Two-stage pipeline (echo | grep)"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Two-stage pipeline failed: $out"
    FAILED=$((FAILED + 1))
fi

# 2. Three-stage pipeline
out=$(printf "printf 'apple\\nbanana\\napple\\n' | sort | uniq\nexit\n" | $MINISHELL 2>&1)
if [[ "$out" == *"apple"* ]] && [[ "$out" == *"banana"* ]]; then
    echo "  [PASS] Three-stage pipeline (printf | sort | uniq)"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Three-stage pipeline failed: $out"
    FAILED=$((FAILED + 1))
fi

# 3. Pipeline exit status is from the last command
out=$(printf "false | true\necho status=\$?\nexit\n" | $MINISHELL 2>&1)
if [[ "$out" == *"status=0"* ]]; then
    echo "  [PASS] Pipeline exit status matches last command (0)"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Pipeline exit status failed: $out"
    FAILED=$((FAILED + 1))
fi

out=$(printf "true | false\necho status=\$?\nexit\n" | $MINISHELL 2>&1)
if [[ "$out" == *"status=1"* ]]; then
    echo "  [PASS] Pipeline exit status matches last command (1)"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Pipeline exit status failed: $out"
    FAILED=$((FAILED + 1))
fi

# 4. Pipeline with redirection on first and last stages
echo "one" > "$RAW_TEMP/nums.txt"
echo "two" >> "$RAW_TEMP/nums.txt"
echo "three" >> "$RAW_TEMP/nums.txt"
printf "cat < \"%s/nums.txt\" | sort > \"%s/sorted.txt\"\nexit\n" "$TEMP_DIR" "$TEMP_DIR" | $MINISHELL >/dev/null 2>&1
if [ -f "$RAW_TEMP/sorted.txt" ] && grep -q "one" "$RAW_TEMP/sorted.txt" && grep -q "three" "$RAW_TEMP/sorted.txt"; then
    echo "  [PASS] Pipeline with input and output redirection"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Pipeline with redirection failed"
    FAILED=$((FAILED + 1))
fi

# 5. Large data throughput without deadlock
out=$(printf "seq 1 2000 | wc -l\nexit\n" | $MINISHELL 2>&1)
if [[ "$out" == *"2000"* ]]; then
    echo "  [PASS] High-volume pipeline output without deadlock (2000 items)"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] High-volume pipeline failed: $out"
    FAILED=$((FAILED + 1))
fi

# 6. Pipe buffer exhaustion and early consumer exit (simulating SIGPIPE/broken pipe)
out=$(printf "yes hello | head -n 50 | wc -l\nexit\n" | $MINISHELL 2>&1)
if [[ "$out" == *"50"* ]]; then
    echo "  [PASS] Early consumer exit termination (yes | head -n 50 | wc -l)"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Early consumer exit failed: $out"
    FAILED=$((FAILED + 1))
fi

echo "Pipeline tests: $PASSED passed, $FAILED failed."
[ "$FAILED" -eq 0 ] || exit 1
