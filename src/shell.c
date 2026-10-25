#define _POSIX_C_SOURCE 200809L

#include "shell.h"
#include "parser.h"
#include "executor.h"
#include "history.h"
#include "signals.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <process.h>
#define isatty _isatty
#define getpid _getpid
#else
#include <sys/wait.h>
#endif

void jobs_init(job_list_t *jl) {
    if (!jl) return;
    jl->count = 0;
    jl->next_id = 1;
    for (int i = 0; i < MAX_JOBS; i++) {
        jl->jobs[i].id = 0;
        jl->jobs[i].pid = 0;
        jl->jobs[i].os_handle = NULL;
        jl->jobs[i].command = NULL;
        jl->jobs[i].running = false;
    }
}

int jobs_add(job_list_t *jl, long pid, void *os_handle, const char *cmd) {
    if (!jl || jl->count >= MAX_JOBS) return -1;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jl->jobs[i].running) {
            jl->jobs[i].id = jl->next_id++;
            jl->jobs[i].pid = pid;
            jl->jobs[i].os_handle = os_handle;
            jl->jobs[i].command = safe_strdup(cmd ? cmd : "");
            jl->jobs[i].running = true;
            jl->count++;
            return jl->jobs[i].id;
        }
    }
    return -1;
}

void jobs_remove(job_list_t *jl, long pid) {
    if (!jl) return;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jl->jobs[i].running && jl->jobs[i].pid == pid) {
            jl->jobs[i].running = false;
            if (jl->jobs[i].command) {
                free(jl->jobs[i].command);
                jl->jobs[i].command = NULL;
            }
#if defined(_WIN32)
            if (jl->jobs[i].os_handle) {
                CloseHandle((HANDLE)jl->jobs[i].os_handle);
                jl->jobs[i].os_handle = NULL;
            }
#endif
            jl->count--;
            break;
        }
    }
}

void jobs_check(shell_state_t *state) {
    if (!state) return;
    job_list_t *jl = &state->jobs;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jl->jobs[i].running) continue;

#if defined(_WIN32)
        if (jl->jobs[i].os_handle) {
            DWORD res = WaitForSingleObject((HANDLE)jl->jobs[i].os_handle, 0);
            if (res == WAIT_OBJECT_0) {
                printf("[%d]+  Done                    %s\n", jl->jobs[i].id, jl->jobs[i].command);
                jobs_remove(jl, jl->jobs[i].pid);
            }
        }
#else
        int status;
        pid_t res = waitpid((pid_t)jl->jobs[i].pid, &status, WNOHANG);
        if (res > 0) {
            printf("[%d]+  Done                    %s\n", jl->jobs[i].id, jl->jobs[i].command);
            jobs_remove(jl, jl->jobs[i].pid);
        }
#endif
    }
}

void jobs_list(job_list_t *jl) {
    if (!jl) return;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jl->jobs[i].running) {
            printf("[%d]   Running                 %s (PID: %ld)\n",
                   jl->jobs[i].id, jl->jobs[i].command, jl->jobs[i].pid);
        }
    }
}

void jobs_cleanup(job_list_t *jl) {
    if (!jl) return;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jl->jobs[i].running) {
            if (jl->jobs[i].command) {
                free(jl->jobs[i].command);
                jl->jobs[i].command = NULL;
            }
#if defined(_WIN32)
            if (jl->jobs[i].os_handle) {
                CloseHandle((HANDLE)jl->jobs[i].os_handle);
                jl->jobs[i].os_handle = NULL;
            }
#endif
            jl->jobs[i].running = false;
        }
    }
    jl->count = 0;
}

#if defined(_WIN32)
static void win32_ensure_git_usr_bin_in_path(void) {
    /* If C:\Program Files\Git\usr\bin exists, ensure it is in PATH so POSIX utils are found */
    DWORD attr = GetFileAttributesA("C:\\Program Files\\Git\\usr\\bin");
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        const char *cur_path = getenv("PATH");
        if (cur_path && !strstr(cur_path, "Git\\usr\\bin") && !strstr(cur_path, "Git/usr/bin")) {
            size_t new_len = strlen(cur_path) + 64;
            char *new_path = (char *)safe_malloc(new_len);
            snprintf(new_path, new_len, "%s;C:\\Program Files\\Git\\usr\\bin", cur_path);
            minishell_setenv("PATH", new_path);
            free(new_path);
        }
    }
}
#endif

void shell_init(shell_state_t *state, int argc, char **argv) {
    (void)argc;
    (void)argv;
    if (!state) return;

    state->last_exit_status = 0;
    state->shell_pid = (long)getpid();
    state->should_exit = false;
    state->exit_code = 0;
    state->is_interactive = isatty(STDIN_FILENO);

    state->history = history_init(HISTORY_DEFAULT_CAPACITY);
    char *hist_path = history_get_default_filepath();
    if (hist_path) {
        history_load(state->history, hist_path);
        free(hist_path);
    }

    jobs_init(&state->jobs);
    signals_setup_parent();

#if defined(_WIN32)
    win32_ensure_git_usr_bin_in_path();
#endif
}

void shell_cleanup(shell_state_t *state) {
    if (!state) return;

    char *hist_path = history_get_default_filepath();
    if (hist_path) {
        history_save(state->history, hist_path);
        free(hist_path);
    }

    if (state->history) {
        history_free(state->history);
        state->history = NULL;
    }

    jobs_cleanup(&state->jobs);
}

int shell_run_command_string(shell_state_t *state, const char *command_str) {
    if (!state || !command_str) return SHELL_EXIT_SUCCESS;

    pipeline_t *pipe = parse_command_line(command_str, state);
    if (pipe) {
        int status = execute_pipeline(state, pipe);
        pipeline_free(pipe);
        return status;
    }

    return state->last_exit_status;
}

int shell_run_script(shell_state_t *state, FILE *input_stream) {
    if (!state || !input_stream) return SHELL_EXIT_SUCCESS;

    while (!state->should_exit) {
        jobs_check(state);
        char *line = minishell_read_line(input_stream, NULL, false);
        if (!line) {
            break; /* EOF */
        }

        if (signals_check_and_reset()) {
            free(line);
            state->last_exit_status = 130;
            continue;
        }

        /* Check if line has any non-whitespace content */
        const char *p = line;
        while (*p && is_whitespace_char(*p)) p++;

        if (*p != '\0') {
            history_add(state->history, line);

            pipeline_t *pipe = parse_command_line(line, state);
            if (pipe) {
                execute_pipeline(state, pipe);
                pipeline_free(pipe);
            }
        }

        free(line);
    }

    return state->last_exit_status;
}

int shell_run_interactive(shell_state_t *state) {
    if (!state) return SHELL_EXIT_SUCCESS;

    const char *prompt = "minishell$ ";

    while (!state->should_exit) {
        jobs_check(state);

        char *line = minishell_read_line(stdin, prompt, true);
        if (!line) {
            /* EOF received (Ctrl+D) */
            printf("exit\n");
            state->should_exit = true;
            break;
        }

        if (signals_check_and_reset()) {
            free(line);
            state->last_exit_status = 130;
            continue;
        }

        /* Check if line has any non-whitespace content */
        const char *p = line;
        while (*p && is_whitespace_char(*p)) p++;

        if (*p != '\0') {
            /* Add to history */
            history_add(state->history, line);

            /* Parse and execute */
            pipeline_t *pipe = parse_command_line(line, state);
            if (pipe) {
                execute_pipeline(state, pipe);
                pipeline_free(pipe);
            }
        }

        free(line);
    }

    return state->exit_code;
}
