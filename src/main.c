#define _POSIX_C_SOURCE 200809L

#include "shell.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *progname) {
    printf("Usage: %s [OPTIONS] [SCRIPT_FILE]\n", progname);
    printf("MiniShell - Complete Unix Command-Line Shell in C\n\n");
    printf("Options:\n");
    printf("  -c <command>   Execute the specified command string and exit.\n");
    printf("  -h, --help     Display this help message and exit.\n");
    printf("  -v, --version  Display version information and exit.\n");
}

static void print_version(void) {
    printf("MiniShell version 1.0.0 (C11, POSIX / Cross-Platform)\n");
    printf("Developed autonomously by Antigravity.\n");
}

int main(int argc, char **argv) {
    shell_state_t state;
    shell_init(&state, argc, argv);

    int exit_status = SHELL_EXIT_SUCCESS;

    if (argc >= 2) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            shell_cleanup(&state);
            return SHELL_EXIT_SUCCESS;
        } else if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
            print_version();
            shell_cleanup(&state);
            return SHELL_EXIT_SUCCESS;
        } else if (strcmp(argv[1], "-c") == 0) {
            if (argc < 3) {
                minishell_error("-c", "option requires an argument");
                shell_cleanup(&state);
                return SHELL_EXIT_SYNTAX_ERROR;
            }
            exit_status = shell_run_command_string(&state, argv[2]);
            shell_cleanup(&state);
            return exit_status;
        } else if (argv[1][0] == '-') {
            minishell_error(argv[1], "invalid option");
            print_usage(argv[0]);
            shell_cleanup(&state);
            return SHELL_EXIT_SYNTAX_ERROR;
        } else {
            /* Execute script file */
            FILE *fp = fopen(argv[1], "r");
            if (!fp) {
                minishell_perror(argv[1]);
                shell_cleanup(&state);
                return SHELL_EXIT_FAILURE;
            }
            state.is_interactive = false;
            exit_status = shell_run_script(&state, fp);
            fclose(fp);
            shell_cleanup(&state);
            return exit_status;
        }
    }

    /* No arguments provided */
    if (state.is_interactive) {
        exit_status = shell_run_interactive(&state);
    } else {
        exit_status = shell_run_script(&state, stdin);
    }

    shell_cleanup(&state);
    return exit_status;
}
