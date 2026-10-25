#define _POSIX_C_SOURCE 200809L

#include "executor.h"
#include "builtins.h"
#include "redirection.h"
#include "signals.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

static void format_command_string(command_t *cmd, char *buf, size_t buf_size) {
    if (!cmd || cmd->argc == 0 || !buf || buf_size == 0) {
        if (buf && buf_size > 0) buf[0] = '\0';
        return;
    }
    buf[0] = '\0';
    size_t offset = 0;
    for (int i = 0; i < cmd->argc; i++) {
        size_t arg_len = strlen(cmd->argv[i]);
        if (offset + arg_len + 2 < buf_size) {
            if (offset > 0) {
                buf[offset++] = ' ';
            }
            memcpy(buf + offset, cmd->argv[i], arg_len);
            offset += arg_len;
            buf[offset] = '\0';
        }
    }
}

#if !defined(_WIN32)

/* POSIX Process and Pipeline Execution Engine */

static int execute_single_posix(shell_state_t *state, command_t *cmd, bool background) {
    /* If built-in command, run in parent process */
    if (cmd->argc > 0 && is_builtin(cmd->argv[0])) {
        saved_fds_t saved;
        saved_fds_init(&saved);

        if (redirection_apply(cmd->redirs, &saved) < 0) {
            redirection_restore(&saved);
            return SHELL_EXIT_FAILURE;
        }

        int status = execute_builtin(state, cmd);
        redirection_restore(&saved);
        return status;
    }

    pid_t pid = fork();
    if (pid < 0) {
        minishell_perror("fork");
        return SHELL_EXIT_FAILURE;
    }

    if (pid == 0) {
        /* Child process */
        signals_setup_child();

        if (redirection_apply(cmd->redirs, NULL) < 0) {
            _exit(SHELL_EXIT_FAILURE);
        }

        if (cmd->argc == 0) {
            _exit(SHELL_EXIT_SUCCESS);
        }

        execvp(cmd->argv[0], cmd->argv);

        /* Exec failed */
        if (errno == ENOENT) {
            fprintf(stderr, "minishell: %s: command not found\n", cmd->argv[0]);
            _exit(SHELL_EXIT_CMD_NOT_FOUND);
        } else if (errno == EACCES) {
            fprintf(stderr, "minishell: %s: Permission denied\n", cmd->argv[0]);
            _exit(SHELL_EXIT_CMD_CANNOT_EXEC);
        } else {
            minishell_perror(cmd->argv[0]);
            _exit(SHELL_EXIT_CMD_CANNOT_EXEC);
        }
    }

    /* Parent process */
    if (background) {
        char cmd_str[256];
        format_command_string(cmd, cmd_str, sizeof(cmd_str));
        int jid = jobs_add(&state->jobs, (long)pid, NULL, cmd_str);
        printf("[%d] %ld\n", jid, (long)pid);
        return SHELL_EXIT_SUCCESS;
    } else {
        int status = 0;
        while (waitpid(pid, &status, 0) < 0) {
            if (errno != EINTR) {
                minishell_perror("waitpid");
                return SHELL_EXIT_FAILURE;
            }
        }
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            return 128 + WTERMSIG(status);
        }
        return SHELL_EXIT_FAILURE;
    }
}

static int execute_pipeline_posix(shell_state_t *state, pipeline_t *pipe) {
    int n = pipe->count;
    int pipefds[2 * (n - 1)];

    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipefds + (i * 2)) < 0) {
            minishell_perror("pipe");
            return SHELL_EXIT_FAILURE;
        }
    }

    pid_t *pids = (pid_t *)safe_malloc(sizeof(pid_t) * n);

    for (int i = 0; i < n; i++) {
        command_t *cmd = pipe->commands[i];
        pids[i] = fork();

        if (pids[i] < 0) {
            minishell_perror("fork");
            /* Close all pipes */
            for (int j = 0; j < 2 * (n - 1); j++) {
                close(pipefds[j]);
            }
            free(pids);
            return SHELL_EXIT_FAILURE;
        }

        if (pids[i] == 0) {
            /* Child stage i */
            signals_setup_child();

            /* If not first command, connect stdin to read end of previous pipe */
            if (i > 0) {
                if (dup2(pipefds[(i - 1) * 2], STDIN_FILENO) < 0) {
                    minishell_perror("dup2");
                    _exit(SHELL_EXIT_FAILURE);
                }
            }

            /* If not last command, connect stdout to write end of current pipe */
            if (i < n - 1) {
                if (dup2(pipefds[i * 2 + 1], STDOUT_FILENO) < 0) {
                    minishell_perror("dup2");
                    _exit(SHELL_EXIT_FAILURE);
                }
            }

            /* Close all pipe file descriptors in child */
            for (int j = 0; j < 2 * (n - 1); j++) {
                close(pipefds[j]);
            }

            /* Apply stage-specific redirections */
            if (redirection_apply(cmd->redirs, NULL) < 0) {
                _exit(SHELL_EXIT_FAILURE);
            }

            if (cmd->argc == 0) {
                _exit(SHELL_EXIT_SUCCESS);
            }

            if (is_builtin(cmd->argv[0])) {
                int ret = execute_builtin(state, cmd);
                _exit(ret);
            } else {
                execvp(cmd->argv[0], cmd->argv);
                if (errno == ENOENT) {
                    fprintf(stderr, "minishell: %s: command not found\n", cmd->argv[0]);
                    _exit(SHELL_EXIT_CMD_NOT_FOUND);
                } else if (errno == EACCES) {
                    fprintf(stderr, "minishell: %s: Permission denied\n", cmd->argv[0]);
                    _exit(SHELL_EXIT_CMD_CANNOT_EXEC);
                } else {
                    minishell_perror(cmd->argv[0]);
                    _exit(SHELL_EXIT_CMD_CANNOT_EXEC);
                }
            }
        }
    }

    /* Parent closes all pipe descriptors so writers and readers see EOF */
    for (int j = 0; j < 2 * (n - 1); j++) {
        close(pipefds[j]);
    }

    if (pipe->background) {
        char cmd_str[256];
        format_command_string(pipe->commands[0], cmd_str, sizeof(cmd_str));
        int jid = jobs_add(&state->jobs, (long)pids[n - 1], NULL, cmd_str);
        printf("[%d] %ld\n", jid, (long)pids[n - 1]);
        free(pids);
        return SHELL_EXIT_SUCCESS;
    }

    /* Wait for all pipeline children */
    int last_status = SHELL_EXIT_SUCCESS;
    for (int i = 0; i < n; i++) {
        int status = 0;
        while (waitpid(pids[i], &status, 0) < 0) {
            if (errno != EINTR) break;
        }
        if (i == n - 1) {
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                last_status = 128 + WTERMSIG(status);
            }
        }
    }

    free(pids);
    return last_status;
}

#else

/* Windows (MinGW-w64) Process and Pipeline Execution Engine */

static char *build_win32_cmdline(char **argv) {
    if (!argv || !argv[0]) return safe_strdup("");

    string_builder_t sb;
    sb_init(&sb);

    char norm_buf[1024];

    for (int i = 0; argv[i]; i++) {
        if (i > 0) sb_append_char(&sb, ' ');
        const char *arg = argv[i];
        if (i == 0) {
            arg = minishell_normalize_path(arg, norm_buf, sizeof(norm_buf));
        }
        bool need_quotes = (strchr(arg, ' ') != NULL || strchr(arg, '\t') != NULL || *arg == '\0');

        if (need_quotes) sb_append_char(&sb, '"');
        for (const char *p = arg; *p; p++) {
            if (*p == '"') {
                sb_append_str(&sb, "\\\"");
            } else if (*p == '\\') {
                sb_append_char(&sb, '\\');
            } else {
                sb_append_char(&sb, *p);
            }
        }
        if (need_quotes) sb_append_char(&sb, '"');
    }

    return sb_detach(&sb);
}

static HANDLE open_win32_redir(const char *filename, redir_type_t type) {
    char path_buf[1024];
    const char *norm_path = minishell_normalize_path(filename, path_buf, sizeof(path_buf));

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    DWORD desired_access = 0;
    DWORD creation_disposition = 0;

    switch (type) {
        case REDIR_IN:
            desired_access = GENERIC_READ;
            creation_disposition = OPEN_EXISTING;
            break;
        case REDIR_OUT:
            desired_access = GENERIC_WRITE;
            creation_disposition = CREATE_ALWAYS;
            break;
        case REDIR_APPEND:
            desired_access = FILE_APPEND_DATA | SYNCHRONIZE;
            creation_disposition = OPEN_ALWAYS;
            break;
        case REDIR_ERR:
            desired_access = GENERIC_WRITE;
            creation_disposition = CREATE_ALWAYS;
            break;
    }

    HANDLE hFile = CreateFileA(norm_path, desired_access, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               &sa, creation_disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            minishell_error(filename, "No such file or directory");
        } else if (err == ERROR_ACCESS_DENIED) {
            minishell_error(filename, "Permission denied");
        } else {
            char err_msg[128];
            snprintf(err_msg, sizeof(err_msg), "error %lu", err);
            minishell_error(filename, err_msg);
        }
    }
    return hFile;
}

static int execute_single_win32(shell_state_t *state, command_t *cmd, bool background) {
    /* If built-in command, run in shell process */
    if (cmd->argc > 0 && is_builtin(cmd->argv[0])) {
        saved_fds_t saved;
        saved_fds_init(&saved);

        if (redirection_apply(cmd->redirs, &saved) < 0) {
            redirection_restore(&saved);
            return SHELL_EXIT_FAILURE;
        }

        int status = execute_builtin(state, cmd);
        redirection_restore(&saved);
        return status;
    }

    if (cmd->argc == 0) {
        saved_fds_t saved;
        saved_fds_init(&saved);
        int res = redirection_apply(cmd->redirs, &saved);
        redirection_restore(&saved);
        return res < 0 ? SHELL_EXIT_FAILURE : SHELL_EXIT_SUCCESS;
    }

    /* Setup standard handles */
    HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);

    HANDLE hFileIn = INVALID_HANDLE_VALUE;
    HANDLE hFileOut = INVALID_HANDLE_VALUE;
    HANDLE hFileErr = INVALID_HANDLE_VALUE;

    redirection_t *r = cmd->redirs;
    while (r) {
        if (r->type == REDIR_IN) {
            if (hFileIn != INVALID_HANDLE_VALUE) CloseHandle(hFileIn);
            hFileIn = open_win32_redir(r->filename, r->type);
            if (hFileIn == INVALID_HANDLE_VALUE) return SHELL_EXIT_FAILURE;
            hStdIn = hFileIn;
        } else if (r->type == REDIR_OUT || r->type == REDIR_APPEND) {
            if (hFileOut != INVALID_HANDLE_VALUE) CloseHandle(hFileOut);
            hFileOut = open_win32_redir(r->filename, r->type);
            if (hFileOut == INVALID_HANDLE_VALUE) {
                if (hFileIn != INVALID_HANDLE_VALUE) CloseHandle(hFileIn);
                return SHELL_EXIT_FAILURE;
            }
            hStdOut = hFileOut;
        } else if (r->type == REDIR_ERR) {
            if (hFileErr != INVALID_HANDLE_VALUE) CloseHandle(hFileErr);
            hFileErr = open_win32_redir(r->filename, r->type);
            if (hFileErr == INVALID_HANDLE_VALUE) {
                if (hFileIn != INVALID_HANDLE_VALUE) CloseHandle(hFileIn);
                if (hFileOut != INVALID_HANDLE_VALUE) CloseHandle(hFileOut);
                return SHELL_EXIT_FAILURE;
            }
            hStdErr = hFileErr;
        }
        r = r->next;
    }

    SetHandleInformation(hStdIn, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    SetHandleInformation(hStdOut, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    SetHandleInformation(hStdErr, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = hStdIn;
    si.hStdOutput = hStdOut;
    si.hStdError = hStdErr;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    char *cmd_line = build_win32_cmdline(cmd->argv);

    BOOL success = CreateProcessA(NULL, cmd_line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    free(cmd_line);

    if (hFileIn != INVALID_HANDLE_VALUE) CloseHandle(hFileIn);
    if (hFileOut != INVALID_HANDLE_VALUE) CloseHandle(hFileOut);
    if (hFileErr != INVALID_HANDLE_VALUE) CloseHandle(hFileErr);

    if (!success) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            fprintf(stderr, "minishell: %s: command not found\n", cmd->argv[0]);
            return SHELL_EXIT_CMD_NOT_FOUND;
        } else if (err == ERROR_ACCESS_DENIED) {
            fprintf(stderr, "minishell: %s: Permission denied\n", cmd->argv[0]);
            return SHELL_EXIT_CMD_CANNOT_EXEC;
        } else {
            fprintf(stderr, "minishell: %s: execution failed (Windows error %lu)\n", cmd->argv[0], err);
            return SHELL_EXIT_CMD_CANNOT_EXEC;
        }
    }

    if (background) {
        char cmd_str[256];
        format_command_string(cmd, cmd_str, sizeof(cmd_str));
        int jid = jobs_add(&state->jobs, (long)pi.dwProcessId, pi.hProcess, cmd_str);
        CloseHandle(pi.hThread);
        printf("[%d] %lu\n", jid, pi.dwProcessId);
        return SHELL_EXIT_SUCCESS;
    } else {
        CloseHandle(pi.hThread);
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        return (int)exit_code;
    }
}

static int execute_pipeline_win32(shell_state_t *state, pipeline_t *pipe) {
    int n = pipe->count;
    HANDLE *hPipesRead = (HANDLE *)safe_malloc(sizeof(HANDLE) * (n - 1));
    HANDLE *hPipesWrite = (HANDLE *)safe_malloc(sizeof(HANDLE) * (n - 1));

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE; /* Non-inheritable by default; enabled only per child */
    sa.lpSecurityDescriptor = NULL;

    for (int i = 0; i < n - 1; i++) {
        if (!CreatePipe(&hPipesRead[i], &hPipesWrite[i], &sa, 0)) {
            minishell_error("pipe", "failed to create anonymous pipe");
            free(hPipesRead);
            free(hPipesWrite);
            return SHELL_EXIT_FAILURE;
        }
    }

    PROCESS_INFORMATION *pi = (PROCESS_INFORMATION *)safe_malloc(sizeof(PROCESS_INFORMATION) * n);
    HANDLE *custom_in_files = (HANDLE *)safe_malloc(sizeof(HANDLE) * n);
    HANDLE *custom_out_files = (HANDLE *)safe_malloc(sizeof(HANDLE) * n);
    HANDLE *custom_err_files = (HANDLE *)safe_malloc(sizeof(HANDLE) * n);

    for (int i = 0; i < n; i++) {
        custom_in_files[i] = INVALID_HANDLE_VALUE;
        custom_out_files[i] = INVALID_HANDLE_VALUE;
        custom_err_files[i] = INVALID_HANDLE_VALUE;
    }

    for (int i = 0; i < n; i++) {
        command_t *cmd = pipe->commands[i];

        HANDLE hIn = (i == 0) ? GetStdHandle(STD_INPUT_HANDLE) : hPipesRead[i - 1];
        HANDLE hOut = (i == n - 1) ? GetStdHandle(STD_OUTPUT_HANDLE) : hPipesWrite[i];
        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);

        /* Apply command redirections if specified */
        redirection_t *r = cmd->redirs;
        while (r) {
            if (r->type == REDIR_IN) {
                if (custom_in_files[i] != INVALID_HANDLE_VALUE) CloseHandle(custom_in_files[i]);
                custom_in_files[i] = open_win32_redir(r->filename, r->type);
                if (custom_in_files[i] != INVALID_HANDLE_VALUE) hIn = custom_in_files[i];
            } else if (r->type == REDIR_OUT || r->type == REDIR_APPEND) {
                if (custom_out_files[i] != INVALID_HANDLE_VALUE) CloseHandle(custom_out_files[i]);
                custom_out_files[i] = open_win32_redir(r->filename, r->type);
                if (custom_out_files[i] != INVALID_HANDLE_VALUE) hOut = custom_out_files[i];
            } else if (r->type == REDIR_ERR) {
                if (custom_err_files[i] != INVALID_HANDLE_VALUE) CloseHandle(custom_err_files[i]);
                custom_err_files[i] = open_win32_redir(r->filename, r->type);
                if (custom_err_files[i] != INVALID_HANDLE_VALUE) hErr = custom_err_files[i];
            }
            r = r->next;
        }

        /* Enable inheritance ONLY on the specific handles passed to this child */
        SetHandleInformation(hIn, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        SetHandleInformation(hOut, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        SetHandleInformation(hErr, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput = hIn;
        si.hStdOutput = hOut;
        si.hStdError = hErr;

        ZeroMemory(&pi[i], sizeof(PROCESS_INFORMATION));

        char *cmd_line = build_win32_cmdline(cmd->argv);
        BOOL ok = CreateProcessA(NULL, cmd_line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi[i]);
        free(cmd_line);

        /* Disable inheritance in parent immediately */
        SetHandleInformation(hIn, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hOut, HANDLE_FLAG_INHERIT, 0);

        /* Crucial: close parent's write end of current pipe immediately */
        if (i < n - 1) {
            CloseHandle(hPipesWrite[i]);
            hPipesWrite[i] = INVALID_HANDLE_VALUE;
        }
        /* Crucial: close parent's read end of previous pipe immediately */
        if (i > 0 && hPipesRead[i - 1] != INVALID_HANDLE_VALUE) {
            CloseHandle(hPipesRead[i - 1]);
            hPipesRead[i - 1] = INVALID_HANDLE_VALUE;
        }

        if (!ok) {
            DWORD err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
                fprintf(stderr, "minishell: %s: command not found\n", cmd->argv[0]);
            } else {
                fprintf(stderr, "minishell: %s: execution failed (Windows error %lu)\n", cmd->argv[0], err);
            }
        }
    }

    /* Close any remaining pipe handles */
    for (int i = 0; i < n - 1; i++) {
        if (hPipesRead[i] != INVALID_HANDLE_VALUE) CloseHandle(hPipesRead[i]);
        if (hPipesWrite[i] != INVALID_HANDLE_VALUE) CloseHandle(hPipesWrite[i]);
    }
    for (int i = 0; i < n; i++) {
        if (custom_in_files[i] != INVALID_HANDLE_VALUE) CloseHandle(custom_in_files[i]);
        if (custom_out_files[i] != INVALID_HANDLE_VALUE) CloseHandle(custom_out_files[i]);
        if (custom_err_files[i] != INVALID_HANDLE_VALUE) CloseHandle(custom_err_files[i]);
    }

    free(hPipesRead);
    free(hPipesWrite);
    free(custom_in_files);
    free(custom_out_files);
    free(custom_err_files);

    if (pipe->background) {
        char cmd_str[256];
        format_command_string(pipe->commands[0], cmd_str, sizeof(cmd_str));
        int jid = jobs_add(&state->jobs, (long)pi[n - 1].dwProcessId, pi[n - 1].hProcess, cmd_str);
        for (int i = 0; i < n; i++) {
            if (pi[i].hThread) CloseHandle(pi[i].hThread);
            if (i < n - 1 && pi[i].hProcess) CloseHandle(pi[i].hProcess);
        }
        printf("[%d] %lu\n", jid, pi[n - 1].dwProcessId);
        free(pi);
        return SHELL_EXIT_SUCCESS;
    }

    /* Wait for the final command in the pipeline first */
    DWORD last_status = SHELL_EXIT_SUCCESS;
    if (pi[n - 1].hProcess) {
        WaitForSingleObject(pi[n - 1].hProcess, INFINITE);
        GetExitCodeProcess(pi[n - 1].hProcess, &last_status);
        CloseHandle(pi[n - 1].hProcess);
        CloseHandle(pi[n - 1].hThread);
        pi[n - 1].hProcess = NULL;
    } else {
        last_status = SHELL_EXIT_CMD_NOT_FOUND;
    }

    /* For upstream processes, wait briefly; if reader exited, simulate SIGPIPE */
    for (int i = 0; i < n - 1; i++) {
        if (pi[i].hProcess) {
            DWORD wait_res = WaitForSingleObject(pi[i].hProcess, 1000);
            if (wait_res == WAIT_TIMEOUT) {
                TerminateProcess(pi[i].hProcess, 141); /* 128 + 13 (SIGPIPE) */
            }
            CloseHandle(pi[i].hProcess);
            CloseHandle(pi[i].hThread);
        }
    }

    free(pi);
    return (int)last_status;
}

#endif /* _WIN32 */

int execute_pipeline(shell_state_t *state, pipeline_t *pipe) {
    if (!pipe || pipe->count == 0) {
        return SHELL_EXIT_SUCCESS;
    }

    int status = SHELL_EXIT_SUCCESS;

    if (pipe->count == 1) {
#if defined(_WIN32)
        status = execute_single_win32(state, pipe->commands[0], pipe->background);
#else
        status = execute_single_posix(state, pipe->commands[0], pipe->background);
#endif
    } else {
#if defined(_WIN32)
        status = execute_pipeline_win32(state, pipe);
#else
        status = execute_pipeline_posix(state, pipe);
#endif
    }

    if (state) {
        state->last_exit_status = status;
    }
    return status;
}
