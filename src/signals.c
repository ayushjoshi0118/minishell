#define _POSIX_C_SOURCE 200809L

#include "signals.h"
#include <stdio.h>
#include <unistd.h>

#if defined(_WIN32)
#include <windows.h>
#endif

volatile sig_atomic_t g_signal_received = 0;

#if defined(_WIN32)
static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            g_signal_received = SIGINT;
            /* Write newline using low-level write */
            write(STDERR_FILENO, "\n", 1);
            return TRUE; /* Handled - do not terminate shell */
        default:
            return FALSE;
    }
}
#else
static void sigint_handler(int sig) {
    g_signal_received = sig;
    const char msg[] = "\n";
    (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
}
#endif

void signals_setup_parent(void) {
    g_signal_received = 0;
#if defined(_WIN32)
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* Interrupt system calls so getline/read returns */
    sigaction(SIGINT, &sa, NULL);

    /* Ignore SIGQUIT and SIGTSTP in interactive parent shell */
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
#endif
}

void signals_setup_child(void) {
#if !defined(_WIN32)
    /* Restore default signal actions in child process */
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
#endif
}

int signals_check_and_reset(void) {
    int sig = g_signal_received;
    g_signal_received = 0;
    return sig;
}

void signals_ignore_all(void) {
#if defined(_WIN32)
    SetConsoleCtrlHandler(NULL, FALSE);
#else
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
#endif
}
