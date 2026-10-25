#define _POSIX_C_SOURCE 200809L

#include "builtins.h"
#include "util.h"
#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#if defined(_WIN32)
#include <direct.h>
#define getcwd _getcwd
#define chdir _chdir
#endif

extern char **environ;

bool is_builtin(const char *cmd_name) {
    if (!cmd_name) return false;
    return (strcmp(cmd_name, "echo") == 0 ||
            strcmp(cmd_name, "cd") == 0 ||
            strcmp(cmd_name, "pwd") == 0 ||
            strcmp(cmd_name, "exit") == 0 ||
            strcmp(cmd_name, "help") == 0 ||
            strcmp(cmd_name, "history") == 0 ||
            strcmp(cmd_name, "export") == 0 ||
            strcmp(cmd_name, "unset") == 0 ||
            strcmp(cmd_name, "jobs") == 0);
}

int execute_builtin(shell_state_t *state, command_t *cmd) {
    if (!cmd || cmd->argc == 0 || !cmd->argv || !cmd->argv[0]) {
        return SHELL_EXIT_SUCCESS;
    }

    const char *name = cmd->argv[0];
    int status = SHELL_EXIT_SUCCESS;

    if (strcmp(name, "echo") == 0) {
        status = builtin_echo(state, cmd->argc, cmd->argv);
    } else if (strcmp(name, "cd") == 0) {
        status = builtin_cd(state, cmd->argc, cmd->argv);
    } else if (strcmp(name, "pwd") == 0) {
        status = builtin_pwd(state, cmd->argc, cmd->argv);
    } else if (strcmp(name, "exit") == 0) {
        status = builtin_exit(state, cmd->argc, cmd->argv);
    } else if (strcmp(name, "help") == 0) {
        status = builtin_help(state, cmd->argc, cmd->argv);
    } else if (strcmp(name, "history") == 0) {
        status = builtin_history(state, cmd->argc, cmd->argv);
    } else if (strcmp(name, "export") == 0) {
        status = builtin_export(state, cmd->argc, cmd->argv);
    } else if (strcmp(name, "unset") == 0) {
        status = builtin_unset(state, cmd->argc, cmd->argv);
    } else if (strcmp(name, "jobs") == 0) {
        status = builtin_jobs(state, cmd->argc, cmd->argv);
    } else {
        status = SHELL_EXIT_CMD_NOT_FOUND;
    }

    if (state) {
        state->last_exit_status = status;
    }
    return status;
}

int builtin_echo(shell_state_t *state, int argc, char **argv) {
    (void)state;
    bool newline = true;
    int start_idx = 1;

    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        newline = false;
        start_idx = 2;
    }

    for (int i = start_idx; i < argc; i++) {
        if (i > start_idx) {
            putchar(' ');
        }
        fputs(argv[i], stdout);
    }

    if (newline) {
        putchar('\n');
    }
    fflush(stdout);

    return SHELL_EXIT_SUCCESS;
}

int builtin_cd(shell_state_t *state, int argc, char **argv) {
    (void)state;
    const char *target = NULL;
    char current_cwd[4096];

    if (!getcwd(current_cwd, sizeof(current_cwd))) {
        current_cwd[0] = '\0';
    }

    if (argc == 1 || strcmp(argv[1], "~") == 0) {
        target = getenv("HOME");
#if defined(_WIN32)
        if (!target) target = getenv("USERPROFILE");
#endif
        if (!target) {
            minishell_error("cd", "HOME not set");
            return SHELL_EXIT_FAILURE;
        }
    } else if (strcmp(argv[1], "-") == 0) {
        target = getenv("OLDPWD");
        if (!target) {
            minishell_error("cd", "OLDPWD not set");
            return SHELL_EXIT_FAILURE;
        }
        printf("%s\n", target);
    } else {
        target = argv[1];
    }

    char norm_buf[1024];
    target = minishell_normalize_path(target, norm_buf, sizeof(norm_buf));

    if (chdir(target) != 0) {
        minishell_error("cd", strerror(errno));
        return SHELL_EXIT_FAILURE;
    }

    /* Update PWD and OLDPWD environment variables */
    if (current_cwd[0] != '\0') {
        minishell_setenv("OLDPWD", current_cwd);
    }
    char new_cwd[4096];
    if (getcwd(new_cwd, sizeof(new_cwd))) {
        minishell_setenv("PWD", new_cwd);
    }

    return SHELL_EXIT_SUCCESS;
}

int builtin_pwd(shell_state_t *state, int argc, char **argv) {
    (void)state;
    (void)argc;
    (void)argv;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s\n", cwd);
        return SHELL_EXIT_SUCCESS;
    } else {
        minishell_perror("pwd");
        return SHELL_EXIT_FAILURE;
    }
}

int builtin_exit(shell_state_t *state, int argc, char **argv) {
    if (!state) return SHELL_EXIT_SUCCESS;

    if (argc == 1) {
        state->should_exit = true;
        state->exit_code = state->last_exit_status;
        return state->exit_code;
    }

    if (argc > 2) {
        minishell_error("exit", "too many arguments");
        return SHELL_EXIT_FAILURE;
    }

    /* Validate that argument is numeric */
    char *endptr = NULL;
    const char *str = argv[1];
    while (*str && is_whitespace_char(*str)) str++;
    if (*str == '+' || *str == '-') str++;

    if (*str == '\0') {
        minishell_error("exit", "numeric argument required");
        state->should_exit = true;
        state->exit_code = 2;
        return 2;
    }

    long val = strtol(argv[1], &endptr, 10);
    while (endptr && *endptr && is_whitespace_char(*endptr)) endptr++;

    if (endptr && *endptr != '\0') {
        minishell_error("exit", "numeric argument required");
        state->should_exit = true;
        state->exit_code = 2;
        return 2;
    }

    state->should_exit = true;
    state->exit_code = (int)(val & 0xFF);
    return state->exit_code;
}

int builtin_help(shell_state_t *state, int argc, char **argv) {
    (void)state;
    (void)argc;
    (void)argv;
    printf("MiniShell - Complete Unix Command-Line Shell in C\n");
    printf("Built-in Commands:\n");
    printf("  cd [DIR]             Change the current directory. Default: HOME; '-' switches to OLDPWD.\n");
    printf("  pwd                  Print the current working directory.\n");
    printf("  exit [STATUS]        Exit the shell with optional integer status (0-255).\n");
    printf("  export [NAME=VAL]    Set environment variable. With no arguments, prints exported variables.\n");
    printf("  unset NAME           Remove environment variable.\n");
    printf("  history              Display command history with line numbers.\n");
    printf("  jobs                 List background jobs.\n");
    printf("  help                 Display this help reference.\n\n");
    printf("Supported Syntax:\n");
    printf("  Quotes:              'single quotes' preserve literals; \"double quotes\" allow $VAR expansion.\n");
    printf("  Escaping:            \\<char> escapes special characters, quotes, or spaces.\n");
    printf("  Variable Expansion:  $NAME, ${NAME}, $? (last status), $$ (shell PID).\n");
    printf("  Pipelines:           cmd1 | cmd2 | cmd3 ...\n");
    printf("  Redirection:         < in.txt, > out.txt, >> append.txt, 2> errors.txt\n");
    printf("  Background Jobs:     cmd & runs command asynchronously.\n");
    return SHELL_EXIT_SUCCESS;
}

int builtin_history(shell_state_t *state, int argc, char **argv) {
    (void)argc;
    (void)argv;
    if (state && state->history) {
        history_print(state->history);
    }
    return SHELL_EXIT_SUCCESS;
}

static bool is_valid_identifier(const char *name) {
    if (!name || *name == '\0') return false;
    if (!isalpha((unsigned char)*name) && *name != '_') return false;
    for (const char *p = name + 1; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_') return false;
    }
    return true;
}

int builtin_export(shell_state_t *state, int argc, char **argv) {
    (void)state;
    if (argc == 1) {
        /* Print all environment variables */
        if (environ) {
            for (char **env = environ; *env; env++) {
                printf("export %s\n", *env);
            }
        }
        return SHELL_EXIT_SUCCESS;
    }

    int ret_status = SHELL_EXIT_SUCCESS;
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        char *eq = strchr(arg, '=');
        if (eq) {
            size_t name_len = eq - arg;
            char name[256];
            if (name_len < sizeof(name)) {
                memcpy(name, arg, name_len);
                name[name_len] = '\0';
                if (!is_valid_identifier(name)) {
                    minishell_error("export", "not a valid identifier");
                    ret_status = SHELL_EXIT_FAILURE;
                    continue;
                }
                const char *value = eq + 1;
                minishell_setenv(name, value);
            }
        } else {
            if (!is_valid_identifier(arg)) {
                minishell_error("export", "not a valid identifier");
                ret_status = SHELL_EXIT_FAILURE;
                continue;
            }
            /* Export existing variable or set empty if not exists */
            char *existing = getenv(arg);
            if (existing) {
                minishell_setenv(arg, existing);
            }
        }
    }
    return ret_status;
}

int builtin_unset(shell_state_t *state, int argc, char **argv) {
    (void)state;
    if (argc == 1) return SHELL_EXIT_SUCCESS;

    int ret_status = SHELL_EXIT_SUCCESS;
    for (int i = 1; i < argc; i++) {
        if (!is_valid_identifier(argv[i])) {
            minishell_error("unset", "not a valid identifier");
            ret_status = SHELL_EXIT_FAILURE;
            continue;
        }
        minishell_unsetenv(argv[i]);
    }
    return ret_status;
}

int builtin_jobs(shell_state_t *state, int argc, char **argv) {
    (void)argc;
    (void)argv;
    if (state) {
        jobs_check(state);
        jobs_list(&state->jobs);
    }
    return SHELL_EXIT_SUCCESS;
}
