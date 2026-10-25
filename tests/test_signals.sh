#!/usr/bin/env bash
# test_signals.sh - Tests for signals, background jobs, and EOF handling

MINISHELL="./minishell"
if [ -f "./minishell.exe" ]; then
    MINISHELL="./minishell.exe"
fi

PASSED=0
FAILED=0

echo "=== Running Signals and Process Control Tests ==="

# 1. EOF (Ctrl+D) handling closes cleanly
out=$(printf "" | $MINISHELL 2>&1)
ec=$?
if [ "$ec" -eq 0 ]; then
    echo "  [PASS] Clean exit on EOF (Ctrl+D simulation)"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] EOF handling failed with exit code $ec"
    FAILED=$((FAILED + 1))
fi

# 2. Background process launching (&)
out=$(printf "sleep 1 &\njobs\nexit\n" | $MINISHELL 2>&1)
if [[ "$out" == *"[1]"* ]] && [[ "$out" == *"Running"* ]]; then
    echo "  [PASS] Background job launched and tracked via jobs builtin"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Background job launch failed: $out"
    FAILED=$((FAILED + 1))
fi

# 3. Background process completion notification
out=$(printf "sleep 1 &\nsleep 1.2\necho done_waiting\nexit\n" | $MINISHELL 2>&1)
if [[ "$out" == *"Done"* ]] && [[ "$out" == *"done_waiting"* ]]; then
    echo "  [PASS] Background job asynchronous completion reaped and reported"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Background job completion failed: $out"
    FAILED=$((FAILED + 1))
fi

# 4. Multiple background jobs
out=$(printf "sleep 1 &\nsleep 1 &\njobs\nexit\n" | $MINISHELL 2>&1)
if [[ "$out" == *"[1]"* ]] && [[ "$out" == *"[2]"* ]]; then
    echo "  [PASS] Multiple background jobs tracking"
    PASSED=$((PASSED + 1))
else
    echo "  [FAIL] Multiple background jobs failed: $out"
    FAILED=$((FAILED + 1))
fi

echo "Signals and jobs tests: $PASSED passed, $FAILED failed."
[ "$FAILED" -eq 0 ] || exit 1
