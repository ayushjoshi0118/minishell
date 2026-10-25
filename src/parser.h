#ifndef MINISHELL_PARSER_H
#define MINISHELL_PARSER_H

#include <stdbool.h>
#include "util.h"

typedef enum {
    REDIR_IN,       /* <  */
    REDIR_OUT,      /* >  */
    REDIR_APPEND,   /* >> */
    REDIR_ERR       /* 2> */
} redir_type_t;

typedef struct redirection {
    redir_type_t type;
    char *filename;
    struct redirection *next;
} redirection_t;

typedef struct command {
    char **argv;        /* NULL-terminated array of arguments */
    int argc;
    redirection_t *redirs;
} command_t;

typedef struct pipeline {
    command_t **commands;
    int count;
    bool background;
} pipeline_t;

/* Forward declaration of shell_state_t to avoid circular dependency */
struct shell_state;

/* Parser API */
pipeline_t *parse_command_line(const char *line, struct shell_state *state);
void pipeline_free(pipeline_t *pipe);
void command_free(command_t *cmd);
void redirection_free(redirection_t *redir);

#endif /* MINISHELL_PARSER_H */
