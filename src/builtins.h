#ifndef MINISHELL_BUILTINS_H
#define MINISHELL_BUILTINS_H

#include <stdbool.h>
#include "parser.h"
#include "shell.h"

/* Returns true if the command name matches a shell built-in */
bool is_builtin(const char *cmd_name);

/* Execute a built-in command in the shell process.
 * Returns the exit status (0 for success, non-zero for error).
 */
int execute_builtin(shell_state_t *state, command_t *cmd);

/* Individual built-in implementations */
int builtin_echo(shell_state_t *state, int argc, char **argv);
int builtin_cd(shell_state_t *state, int argc, char **argv);
int builtin_pwd(shell_state_t *state, int argc, char **argv);
int builtin_exit(shell_state_t *state, int argc, char **argv);
int builtin_help(shell_state_t *state, int argc, char **argv);
int builtin_history(shell_state_t *state, int argc, char **argv);
int builtin_export(shell_state_t *state, int argc, char **argv);
int builtin_unset(shell_state_t *state, int argc, char **argv);
int builtin_jobs(shell_state_t *state, int argc, char **argv);

#endif /* MINISHELL_BUILTINS_H */
