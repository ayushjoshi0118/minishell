# MiniShell - A Unix-Like Command-Line Shell in C

[![CI](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Language](https://img.shields.io/badge/language-C11-blue.svg)]()
[![Standard](https://img.shields.io/badge/standard-POSIX.1--2008-orange.svg)]()
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)]()

MiniShell is an educational, robust, modular Unix-like command-line shell written in standard C11. It demonstrates the internal architecture of operating system shells: process management (`fork`, `execvp`, `waitpid`), file descriptor manipulation (`open`, `dup2`, `close`), interprocess communication via anonymous pipelines (`pipe`), signal handling (`SIGINT`, `SIGQUIT`), variable expansion, and command-line parsing.

> **Note on Compatibility:** MiniShell is designed as an educational, POSIX-first command interpreter implementing a well-defined subset of shell features. It is not an exhaustive or full POSIX/Bash replacement.

---

## Features

- **Interactive & Batch Execution:**
  - Interactive terminal REPL with `minishell$ ` prompt and EOF handling (Ctrl+D).
  - Non-interactive one-shot command execution via `-c "command string"`.
  - Batch script execution via pipeline input (`cat script.sh | ./minishell`) or file argument (`./minishell script.sh`).
- **Process Management & Execution:**
  - Executes system binaries via `PATH` lookup or relative/absolute paths.
  - Proper foreground child process waiting and reaping to prevent zombie processes.
  - Signal-safe handling of Ctrl+C (`SIGINT`) without terminating the shell.
- **Input and Output Redirection:**
  - Input redirection: `< file`
  - Output truncation redirection: `> file`
  - Output append redirection: `>> file`
  - Standard error redirection: `2> file`
  - Built-in command redirection with automatic parent file descriptor restoration.
- **Multi-Stage Pipelines:**
  - Connects arbitrary numbers of commands: `cmd1 | cmd2 | cmd3 ...`
  - Descriptor discipline: closes unneeded pipe descriptors to prevent buffer deadlocks.
  - Returns the exit status of the final pipeline stage according to standard POSIX convention.
- **Robust Argument Parsing:**
  - Single quotes (`'...'`): preserves literal text with spaces.
  - Double quotes (`"..."`): preserves spaces, handles escape sequences (`\"`, `\\`, `\$`), and expands variables.
  - Backslash escaping (`\<char>`): escapes whitespace and special characters outside quotes.
  - Variable expansion: `$VAR`, `${VAR}`, `$?` (last exit status), `$$` (shell PID).
  - Graceful syntax error detection (unmatched quotes, trailing pipes, missing redirection targets).
- **Comprehensive Built-ins:**
  - `cd [DIR]`: changes working directory (supports `~`, `-` for previous directory, updates `$PWD` and `$OLDPWD`).
  - `pwd`: displays current working directory.
  - `echo [-n] [args...]`: displays arguments, optional newline suppression.
  - `exit [STATUS]`: terminates shell with numeric status (validates numeric argument).
  - `export [NAME[=VALUE]]`: sets or lists environment variables.
  - `unset NAME`: unsets environment variables.
  - `history`: lists numbered command history.
  - `jobs`: lists background jobs.
  - `help`: displays available commands and syntax.
- **Background Jobs (`&`):**
  - Runs commands asynchronously: `cmd &`
  - Asynchronous child status monitoring and completion notification (`[id]+ Done cmd`).
- **Cross-Platform Readiness:**
  - Native POSIX engine for Linux/Unix/macOS (`fork`, `execvp`, `waitpid`, `pipe`, `dup2`).
  - Windows MinGW-w64 engine (`CreateProcess`, `CreatePipe`, `DuplicateHandle`).

---

## Prerequisites

- **C Compiler:** Any C11 (or C99) compatible compiler (`gcc`, `clang`).
- **Build Tool:** GNU Make (`make` or `mingw32-make`).
- **Standard Libraries:** Standard C library (`libc`).
- **Optional:** GNU Readline library (`libreadline-dev`). If absent, MiniShell automatically uses its built-in portable line reader.

---

## Build Instructions

To compile MiniShell using the provided `Makefile`:

```bash
# Standard production build
make

# Debug build with symbols (-g -O0 -DDEBUG)
make debug

# Address & Undefined Behavior Sanitizer build (Linux/Clang/GCC with libasan)
make sanitize

# Run the complete automated test suite
make test

# Clean compiled objects and binaries
make clean
```

On Windows with MinGW:
```bash
mingw32-make
```

---

## Usage Guide

### 1. Launching Interactive Mode
Simply run the executable with no arguments:
```bash
$ ./minishell
minishell$ pwd
/home/user/workspace
minishell$ echo "Hello from MiniShell"
Hello from MiniShell
minishell$ exit
```

### 2. Running a Single Command String (`-c`)
```bash
$ ./minishell -c "echo Hello World"
Hello World
```

### 3. Piping Commands Non-Interactively
```bash
$ printf 'pwd\necho "Status: $?"\nexit 0\n' | ./minishell
```

### 4. Running a Script File
```bash
$ ./minishell myscript.sh
```

---

## Built-in Command Reference

| Command | Usage | Description |
|---|---|---|
| `cd` | `cd [DIR]` | Change working directory. With no arguments or `~`, changes to `$HOME`. If `-`, switches to `$OLDPWD` and prints directory. |
| `pwd` | `pwd` | Display the absolute path of the current working directory. |
| `echo` | `echo [-n] [ARGS...]` | Print arguments separated by spaces. If `-n` is specified, omits trailing newline. |
| `exit` | `exit [STATUS]` | Terminate shell with optional exit code (0–255). Non-numeric arguments return exit status 2. |
| `export` | `export [NAME=VAL]` | Set environment variable. Without arguments, prints all exported variables. |
| `unset` | `unset NAME` | Remove environment variable from the shell environment. |
| `history` | `history` | Display numbered list of commands executed in the current session. |
| `jobs` | `jobs` | Display running background jobs and their process IDs. |
| `help` | `help` | Display quick reference guide of built-in commands and syntax. |

---

## Command Examples

### Redirection
```bash
# Truncate and write to file
minishell$ echo "Build starting" > build.log

# Append to file
minishell$ echo "Step 1 complete" >> build.log

# Read standard input from file
minishell$ cat < build.log

# Redirect standard error
minishell$ cat non_existent_file.txt 2> error.log
```

### Pipelines
```bash
# Count lines in directory listing
minishell$ ls | wc -l

# Multi-stage filtering and sorting
minishell$ printf "banana\napple\norange\napple\n" | sort | uniq

# Pipeline combined with redirection
minishell$ cat < data.txt | grep "error" | sort > errors_sorted.txt
```

### Quotes & Escaping
```bash
# Preserving whitespace inside arguments
minishell$ echo "arg with   spaces" 'single  spaces'

# Escaped characters outside quotes
minishell$ echo file\ with\ spaces.txt

# Variable expansion in double quotes vs literal in single quotes
minishell$ export USERNAME=Alice
minishell$ echo "Hello $USERNAME"
Hello Alice
minishell$ echo 'Literal $USERNAME'
Literal $USERNAME
```

### Background Jobs
```bash
# Launch in background
minishell$ sleep 2 &
[1] 12345

# Inspect active jobs
minishell$ jobs
[1]   Running                 sleep 2 (PID: 12345)

# Automatic completion notification appears before subsequent prompt
[1]+  Done                    sleep 2
```

---

## Exit Status Standards

MiniShell adheres to standard POSIX exit status conventions:

| Exit Code | Meaning |
|---|---|
| `0` | Success (`SHELL_EXIT_SUCCESS`) |
| `1` | General command or built-in error (`SHELL_EXIT_FAILURE`) |
| `2` | Syntax error in command line or malformed numeric argument (`SHELL_EXIT_SYNTAX_ERROR`) |
| `126` | Command found but permission denied / cannot execute (`SHELL_EXIT_CMD_CANNOT_EXEC`) |
| `127` | Command not found (`SHELL_EXIT_CMD_NOT_FOUND`) |
| `128 + N` | Terminated by signal number `N` (e.g. 130 for `SIGINT`, 141 for `SIGPIPE`) |

The `$?` parameter always expands to the exit code of the most recent foreground command or pipeline.

---

## Testing

An automated test suite is located under `tests/` covering functional, edge-case, and error paths:

- `test_basic.sh`: Basic process execution, path lookup, arguments, empty lines, and long inputs.
- `test_builtins.sh`: Verification of `cd`, `pwd`, `export`, `unset`, `exit`, `history`, and `help`.
- `test_parser.sh`: Quotes, escaping, variable expansion, unmatched quotes, and invalid syntax.
- `test_redirection.sh`: Input, output truncation, append, stderr, and stream restoration.
- `test_pipelines.sh`: 2-stage, 3-stage pipelines, exit status propagation, and deadlock prevention.
- `test_signals.sh`: EOF handling, background execution (`&`), and completion reaping.

To run the full suite:
```bash
bash tests/run_tests.sh
```

---

## Known Limitations

1. **Job Control (Ctrl+Z / fg / bg):** MiniShell implements asynchronous background job tracking (`cmd &`, `jobs`), but does not implement full POSIX terminal job control (`tcsetpgrp`, process group suspension, and foreground handoff).
2. **Compound Grammars:** Command substitution (`$(cmd)` or `` `cmd` ``), logical operators (`&&`, `||`), subshell parentheses `(...)`, and control structures (`if`, `for`, `while`) are intentionally omitted to focus on core OS process management.
3. **Globbing:** Wildcard path expansion (`*`, `?`) is not performed by the parser; arguments are passed directly to executables.

---

## Architecture Documentation

Detailed systems documentation is available in the `docs/` folder:
- [Architecture & Execution Flow](docs/ARCHITECTURE.md)
- [Parser Grammar & EBNF](docs/PARSER_GRAMMAR.md)
- [Process Management & OS Interfaces](docs/PROCESS_MANAGEMENT.md)
- [Systems Programming Learning Guide](docs/LEARNING_GUIDE.md)
