# MiniShell Architecture

## 1. System Overview

MiniShell is an autonomous, modular, high-reliability command-line shell written in standard C11. It implements a POSIX-compliant process execution engine with cross-platform adaptations for Windows environments, featuring dynamic command parsing, pipeline routing, input/output redirection, background process management, and built-in commands.

```
                           +----------------------+
                           | User Input / Stream  |
                           +----------+-----------+
                                      |
                                      v
                           +----------------------+
                           |  history / readline  |
                           +----------+-----------+
                                      |
                                      v
                           +----------------------+
                           |   Tokenizer / Lexer  |
                           +----------+-----------+
                                      |
                                      v
                           +----------------------+
                           |     AST Parser       |
                           |  (pipeline_t node)   |
                           +----------+-----------+
                                      |
                                      v
                           +----------------------+
                           |   Executor Engine    |
                           +----+------------+----+
                                |            |
            +-------------------+            +-------------------+
            |                                                    |
            v                                                    v
+-----------------------+                            +-----------------------+
|   Built-in Handler    |                            | External Subprocesses |
| (cd, pwd, export, etc)|                            | (fork/execvp/CreatePr)|
+-----------+-----------+                            +-----------+-----------+
            |                                                    |
            +-------------------+            +-------------------+
                                |            |
                                v            v
                           +----------------------+
                           |  Redirection Handler |
                           |  (dup2 / File Descr) |
                           +----------+-----------+
                                      |
                                      v
                           +----------------------+
                           | Process Status / $?  |
                           +----------------------+
```

---

## 2. Source Code Modules

The application is structured into clearly separated modules with strict header interfaces:

| Module Header | Implementation | Responsibility |
|---|---|---|
| `src/main.c` | `src/main.c` | Entry point, CLI argument parsing (`-c`, `-h`, `-v`, script file), mode dispatcher. |
| `src/shell.h` | `src/shell.c` | Shell lifecycle state (`shell_state_t`), interactive loop, script runner, background job registry. |
| `src/parser.h` | `src/parser.c` | Lexer and token parsing, quote handling (`'...'`, `"..."`), escape sequences, variable expansion (`$VAR`, `$?`, `$$`), AST construction. |
| `src/executor.h` | `src/executor.c` | Pipeline orchestration, process fork/exec, pipeline descriptor management, process waiting, and status reporting. |
| `src/builtins.h` | `src/builtins.c` | Shell built-ins (`cd`, `pwd`, `echo`, `exit`, `export`, `unset`, `history`, `jobs`, `help`). |
| `src/redirection.h` | `src/redirection.c` | File descriptor redirection (`<`, `>`, `>>`, `2>`), saving and restoring parent descriptors for builtins. |
| `src/history.h` | `src/history.c` | Command history storage, duplicate suppression, persistence (`~/.minishell_history`), portable line reading. |
| `src/signals.h` | `src/signals.c` | Signal installation (`SIGINT`, `SIGQUIT`, `SIGTSTP`), signal-safety flags, child handler resets. |
| `src/util.h` | `src/util.c` | Dynamic string builder (`string_builder_t`), safe memory allocators, path normalization, error formatting. |

---

## 3. Data Flow & Execution Model

### 3.1. Parsing Phase
1. `minishell_read_line()` gathers arbitrary-length input dynamically until newline or EOF.
2. `tokenize()` splits the input stream into classified tokens (`TOK_WORD`, `TOK_PIPE`, `TOK_REDIR_*`, `TOK_BG`).
3. Single quotes preserve literal text without interpretation.
4. Double quotes allow variable expansion (`$NAME`, `$?`, `$$`) and escape parsing (`\"`, `\\`, `\$`).
5. `parse_command_line()` organizes commands into a `pipeline_t` consisting of an array of `command_t` nodes and an optional background flag.

### 3.2. Execution Phase
- **Single Built-in Command:** Executed directly in the parent process to preserve state changes (such as directory changes via `cd` or environment updates via `export`). Any associated redirections (`pwd > out.txt`) use `redirection_apply()` with a `saved_fds_t` context; the parent file descriptors are restored via `redirection_restore()` immediately after execution.
- **Single External Command:**
  - *POSIX:* Fork a child via `fork()`, configure default signals, redirect file descriptors with `dup2()`, and invoke `execvp()`. The parent reaps the child using `waitpid()`.
  - *Windows:* Uses `CreateProcessA()` with configured `STARTUPINFOA` standard handles, waiting with `WaitForSingleObject()`.
- **Multi-Stage Pipeline:**
  - An array of anonymous pipes is created (`n - 1` pipes for `n` stages).
  - Each child process is spawned with its `stdin` connected to the prior pipe and `stdout` connected to the next pipe.
  - Crucial: The parent process closes all pipe descriptors immediately after spawning each process, preventing descriptor leaks and deadlocks when pipe buffers fill up.
  - The exit status of the pipeline is determined by the exit code of the final command in the sequence.

### 3.3. Memory Management
All parser memory, string buffers, and pipeline structures are freed immediately after execution via `pipeline_free()`, ensuring clean operation without memory leaks.
