#ifndef MINISHELL_SHELL_H
#define MINISHELL_SHELL_H

#include <stdbool.h>
#include <stdio.h>
#include "parser.h"

#define MAX_JOBS 64

typedef struct job {
    int id;
    long pid;           /* Process ID (long for cross-platform compatibility) */
    void *os_handle;    /* OS process handle for Windows or NULL on POSIX */
    char *command;
    bool running;
} job_t;

typedef struct job_list {
    job_t jobs[MAX_JOBS];
    int count;
    int next_id;
} job_list_t;

/* Forward declaration of history_t */
typedef struct history history_t;

typedef struct shell_state {
    int last_exit_status;
    long shell_pid;
    bool is_interactive;
    bool should_exit;
    int exit_code;
    history_t *history;
    job_list_t jobs;
} shell_state_t;

/* Shell lifecycle */
void shell_init(shell_state_t *state, int argc, char **argv);
void shell_cleanup(shell_state_t *state);
int shell_run_interactive(shell_state_t *state);
int shell_run_command_string(shell_state_t *state, const char *command_str);
int shell_run_script(shell_state_t *state, FILE *input_stream);

/* Background jobs management */
void jobs_init(job_list_t *jl);
int jobs_add(job_list_t *jl, long pid, void *os_handle, const char *cmd);
void jobs_remove(job_list_t *jl, long pid);
void jobs_check(shell_state_t *state);
void jobs_list(job_list_t *jl);
void jobs_cleanup(job_list_t *jl);

#endif /* MINISHELL_SHELL_H */
