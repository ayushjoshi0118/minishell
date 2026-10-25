# Process Management & Operating System Interfaces

## 1. Process Creation and Execution Lifecycle

In a Unix operating system, process creation, execution, and monitoring revolve around three fundamental system calls: `fork()`, `execvp()`, and `waitpid()`.

```
Parent (MiniShell)
  |
  +--- fork() ---------------------------------+
  |                                            |
  | (Child PID > 0)                            | (PID == 0)
  |                                            v
  |                                       Child Process
  |                                            |
  |                                     signals_setup_child()
  |                                            |
  |                                     redirection_apply()
  |                                            |
  |                                        execvp()
  |                                            |
  |                                            v (Replaces process image)
  |                                       New Executable
  |                                            |
  v                                            v
waitpid(pid, &status, 0) <------------------ exit(code) / _exit()
  |
  +---> Extract Exit Code: WIFEXITED(status) ? WEXITSTATUS(status) : ...
```

### 1.1. `fork()`
- Creates an exact duplicate of the parent process.
- Returns:
  - `-1` on error (e.g. process limit reached `EAGAIN` or memory exhaustion `ENOMEM`).
  - `0` inside the newly created child process.
  - Child PID (> 0) inside the parent shell process.

### 1.2. `execvp()`
- Replaces the current process image with a new process image specified by `argv[0]`.
- Searches directories in the `PATH` environment variable if the executable name does not contain a slash (`/`).
- If `execvp()` returns, an error occurred:
  - `ENOENT`: Executable not found in `PATH` -> MiniShell prints `command not found` and exits child with status `127`.
  - `EACCES`: File is not executable or permission denied -> MiniShell prints `Permission denied` and exits child with status `126`.
- **Critical Rule:** The child must ALWAYS call `_exit()` upon failure, never continuing the shell's main execution loop.

### 1.3. `waitpid()`
- Suspends parent execution until the specified child process changes state.
- Inspects completion status via POSIX macros:
  - `WIFEXITED(status)`: True if the child terminated normally.
  - `WEXITSTATUS(status)`: Evaluates to the low-order 8 bits of the child's exit status.
  - `WIFSIGNALED(status)`: True if child was terminated by an unhandled signal.
  - `WTERMSIG(status)`: Returns the signal number that caused child termination.

---

## 2. File Descriptors & Redirection with `dup2()`

File descriptors (FDs) are non-negative integers indexing a per-process open file table:
- `STDIN_FILENO` (0): Standard Input
- `STDOUT_FILENO` (1): Standard Output
- `STDERR_FILENO` (2): Standard Error

### 2.1. System Call Mechanics
- `open(path, flags, mode)`: Acquires a new file descriptor.
  - Flags used: `O_RDONLY` for input, `O_WRONLY | O_CREAT | O_TRUNC` for output, `O_WRONLY | O_CREAT | O_APPEND` for append.
- `dup(oldfd)`: Duplicates `oldfd` into the lowest-numbered unused descriptor.
- `dup2(oldfd, newfd)`: Atomically closes `newfd` (if open) and duplicates `oldfd` onto `newfd`.
- `close(fd)`: Closes descriptor, freeing kernel resources.

### 2.2. Preserving Parent File Descriptors for Built-ins
When a built-in command like `pwd > out.txt` runs, it must execute within the parent shell process to maintain environment state.
1. `saved->saved_stdout = dup(STDOUT_FILENO)` saves original terminal output descriptor.
2. `dup2(target_fd, STDOUT_FILENO)` redirects terminal output to file.
3. Builtin executes and writes to `STDOUT_FILENO`.
4. `fflush(stdout)` forces buffered data out to disk.
5. `dup2(saved->saved_stdout, STDOUT_FILENO)` restores original terminal descriptor.
6. `close(saved->saved_stdout)` releases saved descriptor.

---

## 3. Pipelines & Anonymous Pipes

A Unix pipeline `cmd1 | cmd2 | ... | cmdN` connects the standard output of stage `i` to the standard input of stage `i + 1`.

```
[cmd1 stdout] ---> (pipe 0: write) === PIPE BUFFER === (pipe 0: read) ---> [cmd2 stdin]
```

### 3.1. Avoiding Pipeline Deadlocks (Descriptor Discipline)
1. `pipe(pipefds)` creates a unidirectional data channel returning two file descriptors:
   - `pipefds[0]`: read end
   - `pipefds[1]`: write end
2. **The EOF Condition:** A reader reading from a pipe only encounters End-Of-File (EOF) when **ALL** descriptors referring to the write end of that pipe are closed across the entire operating system.
3. If the parent shell fails to close its copy of `pipefds[1]`, the reader (`cmd2`) will wait forever for more data when `cmd1` terminates, resulting in a permanent pipeline hang / deadlock.
4. **MiniShell Implementation Guarantee:**
   - In every child process: all unused pipe descriptors are closed immediately after calling `dup2()`.
   - In the parent process: each pipe descriptor is closed as soon as the respective child process is launched.

---

## 4. Signal Handling and Terminal Behavior

Signals are asynchronous notifications delivered by the operating system kernel to a process.

- **SIGINT (Ctrl+C):**
  - Handled by `sigint_handler()`.
  - In the interactive parent shell: MiniShell intercepts `SIGINT`, writes a newline using signal-safe I/O (`write()`), sets a `volatile sig_atomic_t` flag, and redraws the prompt without terminating the shell.
  - In child processes: Handlers are reset to `SIG_DFL` before `execvp()`, allowing terminal interrupt signals to terminate foreground children normally.
- **SIGQUIT (Ctrl+\) and SIGTSTP (Ctrl+Z):**
  - Ignored in the parent shell (`SIG_IGN`) to prevent accidental termination or background suspension during routine terminal interaction.
- **Signal Safety:**
  - Signal handlers never call non-reentrant functions (`malloc`, `free`, `printf`). State changes are recorded solely through `volatile sig_atomic_t g_signal_received`.
