#ifndef MINISHELL_EXECUTOR_H
#define MINISHELL_EXECUTOR_H

#include "parser.h"
#include "shell.h"

/* Execute a parsed pipeline (single command or multi-stage pipe).
 * Returns the exit status of the pipeline (or the last command in pipeline).
 */
int execute_pipeline(shell_state_t *state, pipeline_t *pipe);

#endif /* MINISHELL_EXECUTOR_H */
