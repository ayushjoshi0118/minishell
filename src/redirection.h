#ifndef MINISHELL_REDIRECTION_H
#define MINISHELL_REDIRECTION_H

#include "parser.h"
#include <stdbool.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

typedef struct {
    int saved_stdin;
    int saved_stdout;
    int saved_stderr;
    bool has_saved_stdin;
    bool has_saved_stdout;
    bool has_saved_stderr;
} saved_fds_t;

/* Initialize saved file descriptors structure */
void saved_fds_init(saved_fds_t *saved);

/* Apply all redirections in the linked list.
 * If saved is not NULL, saves original FDs for later restoration (for builtins).
 * Returns 0 on success, -1 on error.
 */
int redirection_apply(redirection_t *redirs, saved_fds_t *saved);

/* Restore saved file descriptors back to standard streams */
void redirection_restore(saved_fds_t *saved);

#endif /* MINISHELL_REDIRECTION_H */
