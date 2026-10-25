#define _POSIX_C_SOURCE 200809L

#include "history.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(USE_READLINE)
#include <readline/readline.h>
#include <readline/history.h>
#endif

history_t *history_init(int capacity) {
    if (capacity <= 0) {
        capacity = HISTORY_DEFAULT_CAPACITY;
    }
    history_t *hist = (history_t *)safe_malloc(sizeof(history_t));
    hist->capacity = capacity;
    hist->count = 0;
    hist->entries = (char **)safe_calloc(capacity, sizeof(char *));
    return hist;
}

void history_add(history_t *hist, const char *line) {
    if (!hist || !line) return;

    /* Skip lines that are only whitespace */
    const char *p = line;
    while (*p && is_whitespace_char(*p)) {
        p++;
    }
    if (*p == '\0') return;

    /* Duplicate suppression: do not store if identical to the previous entry */
    if (hist->count > 0 && strcmp(hist->entries[hist->count - 1], line) == 0) {
        return;
    }

    /* If history is full, drop the oldest entry (shift left) */
    if (hist->count >= hist->capacity) {
        free(hist->entries[0]);
        for (int i = 1; i < hist->count; i++) {
            hist->entries[i - 1] = hist->entries[i];
        }
        hist->count--;
    }

    hist->entries[hist->count++] = safe_strdup(line);

#if defined(USE_READLINE)
    add_history(line);
#endif
}

int history_count(const history_t *hist) {
    return hist ? hist->count : 0;
}

const char *history_get(const history_t *hist, int index) {
    if (!hist || index < 0 || index >= hist->count) return NULL;
    return hist->entries[index];
}

void history_print(const history_t *hist) {
    if (!hist) return;
    for (int i = 0; i < hist->count; i++) {
        printf("%5d  %s\n", i + 1, hist->entries[i]);
    }
}

void history_clear(history_t *hist) {
    if (!hist) return;
    for (int i = 0; i < hist->count; i++) {
        if (hist->entries[i]) {
            free(hist->entries[i]);
            hist->entries[i] = NULL;
        }
    }
    hist->count = 0;
}

void history_free(history_t *hist) {
    if (!hist) return;
    history_clear(hist);
    free(hist->entries);
    free(hist);
}

char *history_get_default_filepath(void) {
    const char *home = getenv("HOME");
#if defined(_WIN32)
    if (!home) {
        home = getenv("USERPROFILE");
    }
#endif
    if (!home) return NULL;

    size_t len = strlen(home) + 32;
    char *path = (char *)safe_malloc(len);
    snprintf(path, len, "%s/.minishell_history", home);
    return path;
}

void history_load(history_t *hist, const char *filepath) {
    if (!hist || !filepath) return;

    FILE *fp = fopen(filepath, "r");
    if (!fp) return;

    char *line = NULL;
    size_t cap = 0;
    string_builder_t sb;
    sb_init(&sb);

    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '\n') {
            char *entry = sb_detach(&sb);
            if (entry) {
                history_add(hist, entry);
                free(entry);
            }
            sb_init(&sb);
        } else if (c != '\r') {
            sb_append_char(&sb, (char)c);
        }
    }

    char *entry = sb_detach(&sb);
    if (entry && *entry) {
        history_add(hist, entry);
    }
    if (entry) free(entry);
    sb_free(&sb);

    (void)line;
    (void)cap;
    fclose(fp);
}

void history_save(const history_t *hist, const char *filepath) {
    if (!hist || !filepath) return;

    FILE *fp = fopen(filepath, "w");
    if (!fp) return;

    for (int i = 0; i < hist->count; i++) {
        fprintf(fp, "%s\n", hist->entries[i]);
    }

    fclose(fp);
}

char *minishell_read_line(FILE *stream, const char *prompt, bool is_interactive) {
#if defined(USE_READLINE)
    if (is_interactive && stream == stdin) {
        char *line = readline(prompt ? prompt : "");
        return line;
    }
#endif

    if (is_interactive && prompt && *prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }

    string_builder_t sb;
    sb_init(&sb);
    bool any_read = false;

    int c;
    while ((c = fgetc(stream)) != EOF) {
        any_read = true;
        if (c == '\n') {
            break;
        } else if (c == '\r') {
            /* Check if next character is \n (CRLF) */
            int next_c = fgetc(stream);
            if (next_c != '\n' && next_c != EOF) {
                ungetc(next_c, stream);
            }
            break;
        } else {
            sb_append_char(&sb, (char)c);
        }
    }

    if (!any_read && c == EOF) {
        sb_free(&sb);
        return NULL;
    }

    return sb_detach(&sb);
}
