#ifndef MINISHELL_UTIL_H
#define MINISHELL_UTIL_H

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

/* Shell standard exit codes matching POSIX standard */
#define SHELL_EXIT_SUCCESS          0
#define SHELL_EXIT_FAILURE          1
#define SHELL_EXIT_SYNTAX_ERROR      2
#define SHELL_EXIT_CMD_CANNOT_EXEC  126
#define SHELL_EXIT_CMD_NOT_FOUND    127

/* Standard file descriptor numbers if not defined */
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

/* Dynamic String Builder */
typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} string_builder_t;

/* String builder functions */
void sb_init(string_builder_t *sb);
void sb_append_char(string_builder_t *sb, char c);
void sb_append_str(string_builder_t *sb, const char *str);
void sb_append_len(string_builder_t *sb, const char *str, size_t len);
char *sb_detach(string_builder_t *sb);
void sb_free(string_builder_t *sb);

/* Safe memory allocation routines (exit on fatal OOM) */
void *safe_malloc(size_t size);
void *safe_realloc(void *ptr, size_t size);
void *safe_calloc(size_t nmemb, size_t size);
char *safe_strdup(const char *s);

/* String utility routines */
char *trim_whitespace(char *str);
bool is_whitespace_char(char c);
bool is_quote_char(char c);

/* Error reporting routines */
void minishell_error(const char *cmd, const char *msg);
void minishell_perror(const char *cmd);
void minishell_syntax_error(const char *token);

/* Cross-platform path and environment helpers */
const char *minishell_normalize_path(const char *path, char *buf, size_t buf_size);
int minishell_setenv(const char *name, const char *value);
int minishell_unsetenv(const char *name);

#endif /* MINISHELL_UTIL_H */
