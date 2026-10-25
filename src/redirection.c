#define _POSIX_C_SOURCE 200809L

#include "redirection.h"
#include "util.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN32)
#include <io.h>
#include <sys/stat.h>
#define CREAT_MODE (_S_IREAD | _S_IWRITE)
#else
#include <sys/stat.h>
#define CREAT_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#endif

void saved_fds_init(saved_fds_t *saved) {
    if (!saved) return;
    saved->saved_stdin = -1;
    saved->saved_stdout = -1;
    saved->saved_stderr = -1;
    saved->has_saved_stdin = false;
    saved->has_saved_stdout = false;
    saved->has_saved_stderr = false;
}

int redirection_apply(redirection_t *redirs, saved_fds_t *saved) {
    redirection_t *curr = redirs;

    while (curr) {
        int target_fd = -1;
        int flags = 0;
        int mode = CREAT_MODE;

        switch (curr->type) {
            case REDIR_IN:
                target_fd = STDIN_FILENO;
                flags = O_RDONLY | O_BINARY;
                break;
            case REDIR_OUT:
                target_fd = STDOUT_FILENO;
                flags = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
                break;
            case REDIR_APPEND:
                target_fd = STDOUT_FILENO;
                flags = O_WRONLY | O_CREAT | O_APPEND | O_BINARY;
                break;
            case REDIR_ERR:
                target_fd = STDERR_FILENO;
                flags = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
                break;
        }

        char path_buf[1024];
        const char *norm_path = minishell_normalize_path(curr->filename, path_buf, sizeof(path_buf));

        int fd = open(norm_path, flags, mode);
        if (fd < 0) {
            minishell_perror(curr->filename);
            return -1;
        }

        /* If caller requested saving parent FDs (for built-in commands) */
        if (saved) {
            if (target_fd == STDIN_FILENO && !saved->has_saved_stdin) {
                saved->saved_stdin = dup(STDIN_FILENO);
                saved->has_saved_stdin = true;
            } else if (target_fd == STDOUT_FILENO && !saved->has_saved_stdout) {
                saved->saved_stdout = dup(STDOUT_FILENO);
                saved->has_saved_stdout = true;
            } else if (target_fd == STDERR_FILENO && !saved->has_saved_stderr) {
                saved->saved_stderr = dup(STDERR_FILENO);
                saved->has_saved_stderr = true;
            }
        }

        if (dup2(fd, target_fd) < 0) {
            minishell_perror("dup2");
            close(fd);
            return -1;
        }

        close(fd);
        curr = curr->next;
    }

    return 0;
}

void redirection_restore(saved_fds_t *saved) {
    if (!saved) return;

    /* Flush C standard I/O buffers before restoring file descriptors */
    fflush(stdout);
    fflush(stderr);

    if (saved->has_saved_stdin && saved->saved_stdin >= 0) {
        dup2(saved->saved_stdin, STDIN_FILENO);
        close(saved->saved_stdin);
        saved->saved_stdin = -1;
        saved->has_saved_stdin = false;
    }
    if (saved->has_saved_stdout && saved->saved_stdout >= 0) {
        dup2(saved->saved_stdout, STDOUT_FILENO);
        close(saved->saved_stdout);
        saved->saved_stdout = -1;
        saved->has_saved_stdout = false;
    }
    if (saved->has_saved_stderr && saved->saved_stderr >= 0) {
        dup2(saved->saved_stderr, STDERR_FILENO);
        close(saved->saved_stderr);
        saved->saved_stderr = -1;
        saved->has_saved_stderr = false;
    }
}
