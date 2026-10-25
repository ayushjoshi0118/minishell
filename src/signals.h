#ifndef MINISHELL_SIGNALS_H
#define MINISHELL_SIGNALS_H

#include <stdbool.h>
#include <signal.h>

extern volatile sig_atomic_t g_signal_received;

/* Setup signal handling for the interactive shell parent process */
void signals_setup_parent(void);

/* Setup signal handling for child processes before exec */
void signals_setup_child(void);

/* Check and reset signal flag */
int signals_check_and_reset(void);

/* Ignore signals during non-interactive operations */
void signals_ignore_all(void);

#endif /* MINISHELL_SIGNALS_H */
