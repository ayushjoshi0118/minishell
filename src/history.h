#ifndef MINISHELL_HISTORY_H
#define MINISHELL_HISTORY_H

#include <stdbool.h>
#include <stdio.h>

#define HISTORY_DEFAULT_CAPACITY 1000

typedef struct history {
    char **entries;
    int count;
    int capacity;
} history_t;

/* Initialize history subsystem */
history_t *history_init(int capacity);

/* Add a line to history (with duplicate suppression and whitespace check) */
void history_add(history_t *hist, const char *line);

/* Get number of entries */
int history_count(const history_t *hist);

/* Get entry at index (0-based) */
const char *history_get(const history_t *hist, int index);

/* Print all history entries with 1-based indexing */
void history_print(const history_t *hist);

/* Clear all history */
void history_clear(history_t *hist);

/* Free history memory */
void history_free(history_t *hist);

/* Persistence: load/save to file */
void history_load(history_t *hist, const char *filepath);
void history_save(const history_t *hist, const char *filepath);

/* Default history file path (~/.minishell_history) */
char *history_get_default_filepath(void);

/* Portable line reading function (fallback or readline wrapper) */
char *minishell_read_line(FILE *stream, const char *prompt, bool is_interactive);

#endif /* MINISHELL_HISTORY_H */
