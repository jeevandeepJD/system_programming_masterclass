#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

enum { PIPE_CAP = 65536 };

static pid_t child_pid = -1;

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void reap_for_cleanup(void) {
    if (child_pid <= 0) {
        return;
    }
    if (waitpid(child_pid, NULL, WNOHANG) == 0) {
        kill(child_pid, SIGKILL);
        waitpid(child_pid, NULL, 0);
    }
    child_pid = -1;
}

static int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void close_pair(int fds[2]) {
    if (close(fds[0]) != 0 || close(fds[1]) != 0) {
        fail("close");
    }
}

static void observe_shared_write_end(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        fail("pipe");
    }
    int extra = dup(fds[1]);
    if (extra < 0) {
        fail("dup");
    }
    if (write(fds[1], "ab", 2) != 2 || write(extra, "cd", 2) != 2) {
        fail("dup writes");
    }
    char got[4];
    if (read(fds[0], got, sizeof(got)) != 4 || memcmp(got, "abcd", 4) != 0) {
        fail("shared pipe stream");
    }
    puts("dup_write_end_shares_pipe=yes");
    if (close(extra) != 0) {
        fail("close");
    }
    close_pair(fds);
}

static void observe_writer_eof(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        fail("pipe");
    }
    int extra = dup(fds[1]);
    if (extra < 0) {
        fail("dup");
    }
    if (set_nonblock(fds[0]) != 0) {
        fail("O_NONBLOCK");
    }
    if (close(fds[1]) != 0) {
        fail("close one writer");
    }
    unsigned char byte = 0;
    ssize_t n = read(fds[0], &byte, 1);
    int saved = errno;
    if (n >= 0 || (saved != EAGAIN && saved != EWOULDBLOCK)) {
        errno = saved;
        fail("expected no EOF while a writer remains");
    }
    puts("writer_remaining_not_eof=EAGAIN");
    if (close(extra) != 0) {
        fail("close last writer");
    }
    n = read(fds[0], &byte, 1);
    if (n != 0) {
        fail("expected EOF");
    }
    puts("all_writers_closed_eof=0");
    if (close(fds[0]) != 0) {
        fail("close");
    }
}

static void observe_dup2(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        fail("pipe");
    }
    pid_t born = fork();
    if (born < 0) {
        fail("fork");
    }
    if (born == 0) {
        if (close(fds[0]) != 0) {
            _exit(2);
        }
        if (dup2(fds[1], STDOUT_FILENO) < 0) {
            _exit(3);
        }
        if (fds[1] != STDOUT_FILENO && close(fds[1]) != 0) {
            _exit(4);
        }
        const char msg[] = "redir-ok";
        if (write(STDOUT_FILENO, msg, sizeof(msg) - 1U) != (ssize_t)(sizeof(msg) - 1U)) {
            _exit(5);
        }
        _exit(0);
    }
    child_pid = born;
    if (close(fds[1]) != 0) {
        fail("close parent writer");
    }
    char got[8];
    ssize_t n = read(fds[0], got, sizeof(got));
    if (close(fds[0]) != 0) {
        fail("close");
    }
    int status = 0;
    if (waitpid(child_pid, &status, 0) != child_pid) {
        fail("waitpid");
    }
    child_pid = -1;
    if (n != 8 || memcmp(got, "redir-ok", 8) != 0 || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {
        fail("dup2 stdout");
    }
    puts("dup2_stdout=redir-ok");
}

static void observe_backpressure_and_pipe_buf(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        fail("pipe");
    }
    if (fcntl(fds[1], F_SETPIPE_SZ, PIPE_CAP) < 0) {
        fail("F_SETPIPE_SZ");
    }
    int cap = fcntl(fds[1], F_GETPIPE_SZ);
    if (cap != PIPE_CAP) {
        fail("unexpected pipe capacity");
    }
    if (set_nonblock(fds[1]) != 0) {
        fail("O_NONBLOCK");
    }
    char chunk[PIPE_BUF];
    memset(chunk, 'B', sizeof(chunk));
    size_t filled = 0;
    for (;;) {
        ssize_t n = write(fds[1], chunk, sizeof(chunk));
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        if (n != (ssize_t)sizeof(chunk)) {
            fail("unexpected short atomic fill");
        }
        filled += (size_t)n;
        if (filled > 16U * 1024U * 1024U) {
            fail("pipe fill exceeded bound");
        }
    }
    if (filled != (size_t)cap) {
        fail("pipe capacity mismatch");
    }
    printf("backpressure=EAGAIN filled=%zu\n", filled);

    ssize_t blocked = write(fds[1], chunk, sizeof(chunk));
    if (blocked != -1 || errno != EAGAIN) {
        fail("PIPE_BUF write on a full pipe");
    }
    printf("pipe_buf=%d\n", PIPE_BUF);
    puts("pipe_buf_atomic_eagain=yes");
    close_pair(fds);

    if (pipe(fds) != 0) {
        fail("pipe");
    }
    if (set_nonblock(fds[1]) != 0) {
        fail("O_NONBLOCK");
    }
    ssize_t full = write(fds[1], chunk, sizeof(chunk));
    if (full != (ssize_t)sizeof(chunk)) {
        fail("PIPE_BUF write on an empty pipe");
    }
    puts("pipe_buf_atomic_full=yes");
    close_pair(fds);
}

static void mode_demo(void) {
    observe_shared_write_end();
    observe_writer_eof();
    observe_dup2();
    observe_backpressure_and_pipe_buf();
    puts("limit=no_socket_no_network");
    puts("children_reaped=yes");
    puts("temp_files=none");
}

int main(int argc, char **argv) {
    if (atexit(reap_for_cleanup) != 0) {
        fail("atexit");
    }
    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: pipe_lab demo\n");
        return 2;
    }
    mode_demo();
    return 0;
}
