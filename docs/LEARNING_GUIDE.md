# MiniShell Systems Programming Learning Guide

This guide connects foundational concepts in operating systems, C systems programming, and POSIX standards to their concrete implementations across the MiniShell codebase.

---

## 1. Concept to Code Map

| Systems Concept | Theoretical Purpose | MiniShell Source Implementation | Key Functions & Lines |
|---|---|---|---|
| **Process Forking** | Cloning a parent process image into a distinct address space. | `src/executor.c` | `fork()` in `execute_single_posix()`, `execute_pipeline_posix()` |
| **Image Replacement** | Replacing child memory image with an executable binary. | `src/executor.c` | `execvp()` in child execution blocks |
| **Zombie Prevention & Status** | Reaping child exit status via OS process table. | `src/executor.c` | `waitpid()`, `WIFEXITED()`, `WEXITSTATUS()` |
| **File Redirection** | Remapping standard streams (`stdin`, `stdout`, `stderr`). | `src/redirection.c` | `dup2()`, `open()`, `close()` in `redirection_apply()` |
| **Stream Restoration** | Restoring shell terminal streams after built-in redirection. | `src/redirection.c` | `saved_fds_init()`, `redirection_restore()` |
| **Pipes & Interprocess Comm** | Unidirectional kernel byte buffer connecting processes. | `src/executor.c` | `pipe()`, `dup2()` in `execute_pipeline_posix()` |
| **Deadlock Avoidance** | Closing unneeded pipe write ends so readers receive EOF. | `src/executor.c` | Close loops in parent and child pipelines |
| **Signal Safety** | Asynchronous signal interception without deadlocking memory heap. | `src/signals.c` | `sigaction()`, `volatile sig_atomic_t`, raw `write()` |
| **Dynamic Memory & Growth** | Arbitrary-length line reading without buffer overruns. | `src/util.c`, `src/history.c` | `string_builder_t`, `minishell_read_line()` |
| **Lexing & Quoting** | Distinguishing words, operators, single/double quotes, escapes. | `src/parser.c` | `tokenize()`, `expand_variable()` |
| **Environment Variable Management** | In-process environment modification and expansion. | `src/builtins.c`, `src/util.c` | `minishell_setenv()`, `minishell_unsetenv()`, `export` |
| **Asynchronous Job Tracking** | Monitoring non-blocking background tasks. | `src/shell.c` | `jobs_add()`, `jobs_check()`, `waitpid(..., WNOHANG)` |

---

## 2. In-Depth Pedagogical Walkthroughs

### 2.1. Why `cd` Must Be a Shell Built-in
If `cd` were executed as an external executable (`/bin/cd`), MiniShell would create a child process via `fork()`. The child would execute `chdir(path)` and change its own current working directory. However, upon terminating, the child's address space is destroyed, leaving the parent shell's working directory completely unchanged.
- **Reference:** `src/builtins.c`, function `builtin_cd()` operates directly within the parent process and updates `$PWD` and `$OLDPWD`.

### 2.2. The Anatomy of a Pipe Race Condition
When executing `cat file.txt | grep pattern`:
1. The shell calls `pipe(fds)` -> creates `fds[0]` (read) and `fds[1]` (write).
2. The shell forks child 1 (`cat`) and child 2 (`grep`).
3. If the parent does **not** close `fds[1]` in its own process, then three descriptors reference the write end: parent, child 1, child 2.
4. When `cat` reaches the end of `file.txt`, it exits and closes its `fds[1]`.
5. However, `grep` continues calling `read()` on `fds[0]`. The operating system sees that `fds[1]` is still open in the parent shell process, so it cannot deliver EOF to `grep`.
6. `grep` blocks indefinitely waiting for data that will never arrive. The entire shell hangs.
- **Reference:** `src/executor.c`, notice how the parent immediately closes all pipe descriptors once all children have been forked.

### 2.3. Safe Quoting and Variable Expansion
Consider:
```bash
echo "Hello $USER"
echo 'Hello $USER'
echo Hello\ \$USER
```
- Inside double quotes (`"`), MiniShell scans for `$` and expands `$USER` via `expand_variable()`.
- Inside single quotes (`'`), every character is treated as literal; `$` is never expanded.
- Outside quotes, `\ ` escapes the space into a single argument, and `\$` outputs literal `$`.
- **Reference:** `src/parser.c`, function `tokenize()`.
