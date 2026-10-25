#define _POSIX_C_SOURCE 200809L

#include "parser.h"
#include "shell.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    TOK_WORD,
    TOK_PIPE,
    TOK_REDIR_IN,
    TOK_REDIR_OUT,
    TOK_REDIR_APPEND,
    TOK_REDIR_ERR,
    TOK_BG,
    TOK_EOF,
    TOK_ERROR
} token_type_t;

typedef struct token {
    token_type_t type;
    char *value;
    struct token *next;
} token_t;

static void token_free_list(token_t *head) {
    while (head) {
        token_t *next = head->next;
        if (head->value) {
            free(head->value);
        }
        free(head);
        head = next;
    }
}

static token_t *token_create(token_type_t type, char *value) {
    token_t *tok = (token_t *)safe_malloc(sizeof(token_t));
    tok->type = type;
    tok->value = value;
    tok->next = NULL;
    return tok;
}

static void expand_variable(const char **p_ptr, string_builder_t *sb, struct shell_state *state) {
    const char *p = *p_ptr;
    /* Skip the '$' */
    p++;

    if (*p == '?') {
        /* $? - last exit status */
        char buf[32];
        int status = state ? state->last_exit_status : 0;
        snprintf(buf, sizeof(buf), "%d", status);
        sb_append_str(sb, buf);
        p++;
    } else if (*p == '$') {
        /* $$ - process ID */
        char buf[32];
        long pid = state ? state->shell_pid : 0;
        snprintf(buf, sizeof(buf), "%ld", pid);
        sb_append_str(sb, buf);
        p++;
    } else if (*p == '{') {
        /* ${VAR} */
        p++; /* skip '{' */
        const char *start = p;
        while (*p && *p != '}') {
            p++;
        }
        if (*p == '}') {
            size_t len = p - start;
            char var_name[256];
            if (len < sizeof(var_name)) {
                memcpy(var_name, start, len);
                var_name[len] = '\0';
                char *val = getenv(var_name);
                if (val) {
                    sb_append_str(sb, val);
                }
            }
            p++; /* skip '}' */
        } else {
            /* Missing closing '}', append literal */
            sb_append_str(sb, "${");
        }
    } else if (isalpha((unsigned char)*p) || *p == '_') {
        /* $VAR */
        const char *start = p;
        while (isalnum((unsigned char)*p) || *p == '_') {
            p++;
        }
        size_t len = p - start;
        char var_name[256];
        if (len < sizeof(var_name)) {
            memcpy(var_name, start, len);
            var_name[len] = '\0';
            char *val = getenv(var_name);
            if (val) {
                sb_append_str(sb, val);
            }
        }
    } else {
        /* Just a lone '$' */
        sb_append_char(sb, '$');
    }

    *p_ptr = p;
}

static token_t *tokenize(const char *input, struct shell_state *state) {
    const char *p = input;
    token_t *head = NULL;
    token_t *tail = NULL;

    while (*p) {
        /* Skip whitespace */
        while (*p && is_whitespace_char(*p)) {
            p++;
        }
        if (!*p) break;

        /* Check for 2> */
        if (*p == '2' && *(p + 1) == '>') {
            token_t *tok = token_create(TOK_REDIR_ERR, safe_strdup("2>"));
            if (!head) head = tok; else tail->next = tok;
            tail = tok;
            p += 2;
            continue;
        }

        /* Check for >> */
        if (*p == '>' && *(p + 1) == '>') {
            token_t *tok = token_create(TOK_REDIR_APPEND, safe_strdup(">>"));
            if (!head) head = tok; else tail->next = tok;
            tail = tok;
            p += 2;
            continue;
        }

        /* Check for single-character operators */
        if (*p == '>') {
            token_t *tok = token_create(TOK_REDIR_OUT, safe_strdup(">"));
            if (!head) head = tok; else tail->next = tok;
            tail = tok;
            p++;
            continue;
        }
        if (*p == '<') {
            token_t *tok = token_create(TOK_REDIR_IN, safe_strdup("<"));
            if (!head) head = tok; else tail->next = tok;
            tail = tok;
            p++;
            continue;
        }
        if (*p == '|') {
            token_t *tok = token_create(TOK_PIPE, safe_strdup("|"));
            if (!head) head = tok; else tail->next = tok;
            tail = tok;
            p++;
            continue;
        }
        if (*p == '&') {
            token_t *tok = token_create(TOK_BG, safe_strdup("&"));
            if (!head) head = tok; else tail->next = tok;
            tail = tok;
            p++;
            continue;
        }

        /* Parse a word argument */
        string_builder_t sb;
        sb_init(&sb);
        bool in_word = true;

        while (*p && in_word) {
            if (*p == '\'') {
                /* Single quotes: preserve all characters literally until next single quote */
                p++; /* skip opening quote */
                const char *quote_start = p;
                while (*p && *p != '\'') {
                    sb_append_char(&sb, *p);
                    p++;
                }
                if (*p != '\'') {
                    minishell_error(NULL, "syntax error: unmatched single quote `'`");
                    sb_free(&sb);
                    token_free_list(head);
                    if (state) state->last_exit_status = SHELL_EXIT_SYNTAX_ERROR;
                    return NULL;
                }
                p++; /* skip closing quote */
                (void)quote_start;
            } else if (*p == '"') {
                /* Double quotes: preserve spaces, handle escapes and variable expansion */
                p++; /* skip opening quote */
                while (*p && *p != '"') {
                    if (*p == '\\') {
                        p++;
                        if (*p == '"' || *p == '\\' || *p == '$' || *p == '`') {
                            sb_append_char(&sb, *p);
                            p++;
                        } else if (*p == '\0') {
                            /* Backslash at end of string */
                            sb_append_char(&sb, '\\');
                        } else {
                            sb_append_char(&sb, '\\');
                            sb_append_char(&sb, *p);
                            p++;
                        }
                    } else if (*p == '$') {
                        expand_variable(&p, &sb, state);
                    } else {
                        sb_append_char(&sb, *p);
                        p++;
                    }
                }
                if (*p != '"') {
                    minishell_error(NULL, "syntax error: unmatched double quote `\"`");
                    sb_free(&sb);
                    token_free_list(head);
                    if (state) state->last_exit_status = SHELL_EXIT_SYNTAX_ERROR;
                    return NULL;
                }
                p++; /* skip closing quote */
            } else if (*p == '\\') {
                /* Escaped character outside quotes */
                p++;
                if (*p) {
                    sb_append_char(&sb, *p);
                    p++;
                }
            } else if (*p == '$') {
                /* Variable expansion outside quotes */
                expand_variable(&p, &sb, state);
            } else if (is_whitespace_char(*p) || *p == '|' || *p == '<' || *p == '>' || *p == '&') {
                /* Delimiter reached */
                in_word = false;
            } else {
                sb_append_char(&sb, *p);
                p++;
            }
        }

        char *val = sb_detach(&sb);
        token_t *tok = token_create(TOK_WORD, val);
        if (!head) head = tok; else tail->next = tok;
        tail = tok;
    }

    /* Append EOF token */
    token_t *eof_tok = token_create(TOK_EOF, NULL);
    if (!head) head = eof_tok; else tail->next = eof_tok;

    return head;
}

void redirection_free(redirection_t *redir) {
    while (redir) {
        redirection_t *next = redir->next;
        if (redir->filename) {
            free(redir->filename);
        }
        free(redir);
        redir = next;
    }
}

void command_free(command_t *cmd) {
    if (!cmd) return;
    if (cmd->argv) {
        for (int i = 0; i < cmd->argc; i++) {
            if (cmd->argv[i]) {
                free(cmd->argv[i]);
            }
        }
        free(cmd->argv);
    }
    redirection_free(cmd->redirs);
    free(cmd);
}

void pipeline_free(pipeline_t *pipe) {
    if (!pipe) return;
    if (pipe->commands) {
        for (int i = 0; i < pipe->count; i++) {
            command_free(pipe->commands[i]);
        }
        free(pipe->commands);
    }
    free(pipe);
}

static void cmd_add_arg(command_t *cmd, const char *arg) {
    cmd->argv = (char **)safe_realloc(cmd->argv, sizeof(char *) * (cmd->argc + 2));
    cmd->argv[cmd->argc] = safe_strdup(arg);
    cmd->argc++;
    cmd->argv[cmd->argc] = NULL;
}

static void cmd_add_redir(command_t *cmd, redir_type_t type, const char *filename) {
    redirection_t *redir = (redirection_t *)safe_malloc(sizeof(redirection_t));
    redir->type = type;
    redir->filename = safe_strdup(filename);
    redir->next = NULL;

    if (!cmd->redirs) {
        cmd->redirs = redir;
    } else {
        redirection_t *curr = cmd->redirs;
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = redir;
    }
}

pipeline_t *parse_command_line(const char *line, struct shell_state *state) {
    if (!line) return NULL;

    token_t *tokens = tokenize(line, state);
    if (!tokens) {
        return NULL; /* Syntax error during tokenization */
    }

    if (tokens->type == TOK_EOF) {
        token_free_list(tokens);
        return NULL; /* Empty line */
    }

    /* Check for leading pipe */
    if (tokens->type == TOK_PIPE) {
        minishell_syntax_error("|");
        if (state) state->last_exit_status = SHELL_EXIT_SYNTAX_ERROR;
        token_free_list(tokens);
        return NULL;
    }

    pipeline_t *pipe = (pipeline_t *)safe_calloc(1, sizeof(pipeline_t));
    pipe->commands = NULL;
    pipe->count = 0;
    pipe->background = false;

    token_t *curr = tokens;
    while (curr && curr->type != TOK_EOF) {
        command_t *cmd = (command_t *)safe_calloc(1, sizeof(command_t));
        cmd->argv = NULL;
        cmd->argc = 0;
        cmd->redirs = NULL;

        while (curr && curr->type != TOK_PIPE && curr->type != TOK_EOF && curr->type != TOK_BG) {
            if (curr->type == TOK_WORD) {
                cmd_add_arg(cmd, curr->value);
                curr = curr->next;
            } else if (curr->type == TOK_REDIR_IN || curr->type == TOK_REDIR_OUT ||
                       curr->type == TOK_REDIR_APPEND || curr->type == TOK_REDIR_ERR) {
                redir_type_t rtype = REDIR_IN;
                if (curr->type == TOK_REDIR_OUT) rtype = REDIR_OUT;
                else if (curr->type == TOK_REDIR_APPEND) rtype = REDIR_APPEND;
                else if (curr->type == TOK_REDIR_ERR) rtype = REDIR_ERR;

                token_t *op_token = curr;
                curr = curr->next;
                if (!curr || curr->type != TOK_WORD) {
                    const char *bad_tok = (curr && curr->value) ? curr->value : "newline";
                    minishell_syntax_error(bad_tok);
                    (void)op_token;
                    command_free(cmd);
                    pipeline_free(pipe);
                    token_free_list(tokens);
                    if (state) state->last_exit_status = SHELL_EXIT_SYNTAX_ERROR;
                    return NULL;
                }
                cmd_add_redir(cmd, rtype, curr->value);
                curr = curr->next;
            } else {
                /* Unexpected token */
                minishell_syntax_error(curr->value);
                command_free(cmd);
                pipeline_free(pipe);
                token_free_list(tokens);
                if (state) state->last_exit_status = SHELL_EXIT_SYNTAX_ERROR;
                return NULL;
            }
        }

        /* Validate command: must have at least one argument or redirection */
        if (cmd->argc == 0 && cmd->redirs == NULL) {
            minishell_syntax_error(curr ? (curr->value ? curr->value : "|") : "newline");
            command_free(cmd);
            pipeline_free(pipe);
            token_free_list(tokens);
            if (state) state->last_exit_status = SHELL_EXIT_SYNTAX_ERROR;
            return NULL;
        }

        /* Append command to pipeline */
        pipe->commands = (command_t **)safe_realloc(pipe->commands, sizeof(command_t *) * (pipe->count + 1));
        pipe->commands[pipe->count++] = cmd;

        if (curr && curr->type == TOK_PIPE) {
            curr = curr->next;
            /* If pipe is immediately followed by EOF or another pipe */
            if (!curr || curr->type == TOK_EOF || curr->type == TOK_PIPE) {
                minishell_syntax_error(curr && curr->value ? curr->value : "|");
                pipeline_free(pipe);
                token_free_list(tokens);
                if (state) state->last_exit_status = SHELL_EXIT_SYNTAX_ERROR;
                return NULL;
            }
        } else if (curr && curr->type == TOK_BG) {
            pipe->background = true;
            curr = curr->next;
            if (curr && curr->type != TOK_EOF) {
                minishell_syntax_error(curr->value ? curr->value : "&");
                pipeline_free(pipe);
                token_free_list(tokens);
                if (state) state->last_exit_status = SHELL_EXIT_SYNTAX_ERROR;
                return NULL;
            }
            break;
        }
    }

    token_free_list(tokens);
    return pipe;
}
