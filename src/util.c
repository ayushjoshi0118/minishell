#define _POSIX_C_SOURCE 200809L

#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define SB_INITIAL_CAPACITY 64

void sb_init(string_builder_t *sb) {
    if (!sb) return;
    sb->capacity = SB_INITIAL_CAPACITY;
    sb->length = 0;
    sb->data = (char *)safe_malloc(sb->capacity);
    sb->data[0] = '\0';
}

void sb_append_char(string_builder_t *sb, char c) {
    if (!sb) return;
    if (sb->length + 2 > sb->capacity) {
        sb->capacity *= 2;
        sb->data = (char *)safe_realloc(sb->data, sb->capacity);
    }
    sb->data[sb->length++] = c;
    sb->data[sb->length] = '\0';
}

void sb_append_len(string_builder_t *sb, const char *str, size_t len) {
    if (!sb || !str || len == 0) return;
    if (sb->length + len + 1 > sb->capacity) {
        while (sb->length + len + 1 > sb->capacity) {
            sb->capacity *= 2;
        }
        sb->data = (char *)safe_realloc(sb->data, sb->capacity);
    }
    memcpy(sb->data + sb->length, str, len);
    sb->length += len;
    sb->data[sb->length] = '\0';
}

void sb_append_str(string_builder_t *sb, const char *str) {
    if (!str) return;
    sb_append_len(sb, str, strlen(str));
}

char *sb_detach(string_builder_t *sb) {
    if (!sb) return NULL;
    char *res = sb->data;
    sb->data = NULL;
    sb->length = 0;
    sb->capacity = 0;
    return res;
}

void sb_free(string_builder_t *sb) {
    if (!sb) return;
    if (sb->data) {
        free(sb->data);
        sb->data = NULL;
    }
    sb->length = 0;
    sb->capacity = 0;
}

void *safe_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "minishell: fatal: out of memory (%zu bytes requested)\n", size);
        exit(SHELL_EXIT_FAILURE);
    }
    return ptr;
}

void *safe_realloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fprintf(stderr, "minishell: fatal: out of memory (%zu bytes requested)\n", size);
        exit(SHELL_EXIT_FAILURE);
    }
    return new_ptr;
}

void *safe_calloc(size_t nmemb, size_t size) {
    void *ptr = calloc(nmemb, size);
    if (!ptr && nmemb > 0 && size > 0) {
        fprintf(stderr, "minishell: fatal: out of memory (%zu bytes requested)\n", nmemb * size);
        exit(SHELL_EXIT_FAILURE);
    }
    return ptr;
}

char *safe_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *dup = (char *)safe_malloc(len);
    memcpy(dup, s, len);
    return dup;
}

char *trim_whitespace(char *str) {
    if (!str) return NULL;
    while (*str && is_whitespace_char(*str)) {
        str++;
    }
    if (*str == '\0') {
        return str;
    }
    char *end = str + strlen(str) - 1;
    while (end > str && is_whitespace_char(*end)) {
        *end = '\0';
        end--;
    }
    return str;
}

bool is_whitespace_char(char c) {
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

bool is_quote_char(char c) {
    return (c == '\'' || c == '"');
}

void minishell_error(const char *cmd, const char *msg) {
    if (cmd && *cmd) {
        fprintf(stderr, "minishell: %s: %s\n", cmd, msg);
    } else {
        fprintf(stderr, "minishell: %s\n", msg);
    }
}

void minishell_perror(const char *cmd) {
    minishell_error(cmd, strerror(errno));
}

void minishell_syntax_error(const char *token) {
    if (token) {
        fprintf(stderr, "minishell: syntax error near unexpected token '%s'\n", token);
    } else {
        fprintf(stderr, "minishell: syntax error\n");
    }
}

const char *minishell_normalize_path(const char *path, char *buf, size_t buf_size) {
    if (!path || !buf || buf_size == 0) return path;

#if defined(_WIN32)
    /* Translate MSYS2 / POSIX paths on Windows to valid Windows paths */
    if (strncmp(path, "/tmp", 4) == 0 && (path[4] == '/' || path[4] == '\0')) {
        const char *temp = getenv("TEMP");
        if (!temp) temp = getenv("TMP");
        if (!temp) temp = "C:\\Windows\\Temp";
        snprintf(buf, buf_size, "%s%s", temp, path + 4);
        return buf;
    }

    if (path[0] == '/' && isalpha((unsigned char)path[1]) && (path[2] == '/' || path[2] == '\0')) {
        /* /c/... -> C:/... */
        snprintf(buf, buf_size, "%c:%s", toupper((unsigned char)path[1]), path + 2);
        return buf;
    }

    if (strncmp(path, "/usr/bin/", 9) == 0) {
        snprintf(buf, buf_size, "C:\\Program Files\\Git\\usr\\bin\\%s", path + 9);
        return buf;
    }

    if (strncmp(path, "/bin/", 5) == 0) {
        snprintf(buf, buf_size, "C:\\Program Files\\Git\\usr\\bin\\%s", path + 5);
        return buf;
    }
#endif

    return path;
}

int minishell_setenv(const char *name, const char *value) {
    if (!name || *name == '\0') return -1;
#if defined(_WIN32)
    /* Windows _putenv_s */
    return _putenv_s(name, value ? value : "");
#else
    /* POSIX setenv */
    return setenv(name, value ? value : "", 1);
#endif
}

int minishell_unsetenv(const char *name) {
    if (!name || *name == '\0') return -1;
#if defined(_WIN32)
    /* On Windows, setting NAME= with _putenv removes it */
    char buf[512];
    snprintf(buf, sizeof(buf), "%s=", name);
    return _putenv(buf);
#else
    /* POSIX unsetenv */
    return unsetenv(name);
#endif
}
