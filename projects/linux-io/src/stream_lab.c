#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

enum { TRANSFER = 262144, PIPE_CAP = 65536 };

static pid_t child_pid = -1;
static volatile sig_atomic_t signal_note;

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void on_signal(int sig) {
    if (sig == SIGUSR1) {
        signal_note = 1;
    } else if (sig == SIGALRM && signal_note == 0) {
        signal_note = 2;
    }
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

static int past_ms(const struct timespec *start, long limit_ms) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 1;
    }
    long long sec = (long long)now.tv_sec - (long long)start->tv_sec;
    long long nsec = (long long)now.tv_nsec - (long long)start->tv_nsec;
    return sec * 1000LL + nsec / 1000000LL >= (long long)limit_ms;
}

static int parent_in_read(pid_t pid) {
    char path[64];
    int wrote = snprintf(path, sizeof(path), "/proc/%ld/syscall", (long)pid);
    if (wrote < 0 || (size_t)wrote >= sizeof(path)) {
        return 0;
    }
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return 0;
    }
    char buf[128];
    if (fgets(buf, (int)sizeof(buf), fp) == NULL) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    char *end = buf;
    long num = strtol(buf, &end, 10);
    if (end == buf) {
        return 0;
    }
    return num == (long)SYS_read;
}

static void observe_short_write(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        fail("pipe");
    }
    if (fcntl(fds[1], F_SETPIPE_SZ, PIPE_CAP) < 0) {
        fail("F_SETPIPE_SZ");
    }
    int cap = fcntl(fds[1], F_GETPIPE_SZ);
    if (cap <= 0 || cap > 1024 * 1024) {
        fail("F_GETPIPE_SZ");
    }
    if (set_nonblock(fds[1]) != 0) {
        fail("O_NONBLOCK");
    }
    size_t request = (size_t)cap + 4096U;
    unsigned char *buf = calloc(request, 1U);
    if (buf == NULL) {
        fail("calloc");
    }
    ssize_t n = write(fds[1], buf, request);
    int saved = errno;
    free(buf);
    if (close(fds[0]) != 0 || close(fds[1]) != 0) {
        fail("close");
    }
    if (n < 0) {
        errno = saved;
        fail("short write");
    }
    if ((size_t)n == 0U || (size_t)n >= request) {
        fail("expected a short pipe write");
    }
    printf("short_write=%lld requested=%zu\n", (long long)n, request);
    puts("short_write_lt_request=yes");
}

static void observe_eof(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        fail("pipe");
    }
    if (close(fds[1]) != 0) {
        fail("close writer");
    }
    unsigned char byte = 0;
    ssize_t n = read(fds[0], &byte, 1);
    if (close(fds[0]) != 0) {
        fail("close");
    }
    if (n != 0) {
        fail("expected EOF");
    }
    puts("eof=0");
}

static void observe_eagain(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        fail("pipe");
    }
    if (set_nonblock(fds[0]) != 0) {
        fail("O_NONBLOCK");
    }
    unsigned char byte = 0;
    ssize_t n = read(fds[0], &byte, 1);
    int saved = errno;
    if (close(fds[0]) != 0 || close(fds[1]) != 0) {
        fail("close");
    }
    if (n >= 0 || (saved != EAGAIN && saved != EWOULDBLOCK)) {
        errno = saved;
        fail("expected EAGAIN");
    }
    puts("nonblock_read=EAGAIN");
}

static void observe_eintr(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) != 0 || sigaction(SIGALRM, &sa, NULL) != 0) {
        fail("sigaction");
    }
    signal_note = 0;

    int fds[2];
    if (pipe(fds) != 0) {
        fail("pipe");
    }
    pid_t born = fork();
    if (born < 0) {
        fail("fork");
    }
    if (born == 0) {
        close(fds[0]);
        close(fds[1]);
        pid_t parent = getppid();
        struct timespec start;
        if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
            _exit(5);
        }
        for (;;) {
            if (getppid() != parent) {
                _exit(4);
            }
            if (parent_in_read(parent)) {
                if (kill(parent, SIGUSR1) != 0) {
                    _exit(3);
                }
                _exit(0);
            }
            if (past_ms(&start, 2000L)) {
                _exit(2);
            }
            struct timespec pause = {.tv_sec = 0, .tv_nsec = 1000000L};
            nanosleep(&pause, NULL);
        }
    }
    child_pid = born;
    /* Keep the write end open so read blocks instead of returning EOF. */
    alarm(4);
    unsigned char byte = 0;
    ssize_t n = read(fds[0], &byte, 1);
    int saved = errno;
    alarm(0);
    if (close(fds[0]) != 0 || close(fds[1]) != 0) {
        fail("close");
    }
    int status = 0;
    if (waitpid(child_pid, &status, 0) != child_pid) {
        fail("waitpid");
    }
    child_pid = -1;
    if (n < 0 && saved == EINTR && signal_note == 1 && WIFEXITED(status) &&
        WEXITSTATUS(status) == 0) {
        puts("eintr=yes");
        return;
    }
    fprintf(stderr, "eintr observation failed n=%lld errno=%d note=%d status=%d\n",
            (long long)n, saved, (int)signal_note, status);
    fail("eintr");
}

static int write_retry(int fd, const unsigned char *data, size_t size, int *saw_partial) {
    unsigned timeouts = 0;
    while (size > 0U) {
        ssize_t n = write(fd, data, size);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                *saw_partial = 1;
                struct pollfd pfd = {.fd = fd, .events = (short)POLLOUT, .revents = 0};
                int ready = poll(&pfd, 1, 1000);
                if (ready < 0 && errno == EINTR) {
                    continue;
                }
                if (ready < 0) {
                    return -1;
                }
                if (ready == 0) {
                    timeouts++;
                    if (timeouts > 5U) {
                        return -1;
                    }
                }
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        if ((size_t)n < size) {
            *saw_partial = 1;
        }
        data += (size_t)n;
        size -= (size_t)n;
        timeouts = 0U;
    }
    return 0;
}

static int read_full(int fd, unsigned char *data, size_t size) {
    while (size > 0U) {
        ssize_t n = read(fd, data, size);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return -1;
        }
        data += (size_t)n;
        size -= (size_t)n;
    }
    return 0;
}

static void observe_transfer(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        fail("pipe");
    }
    if (fcntl(fds[1], F_SETPIPE_SZ, PIPE_CAP) < 0) {
        fail("F_SETPIPE_SZ");
    }
    pid_t born = fork();
    if (born < 0) {
        fail("fork");
    }
    if (born == 0) {
        if (close(fds[1]) != 0) {
            _exit(3);
        }
        unsigned char *buf = calloc((size_t)TRANSFER, 1U);
        if (buf == NULL) {
            _exit(4);
        }
        int rc = read_full(fds[0], buf, (size_t)TRANSFER);
        free(buf);
        if (close(fds[0]) != 0) {
            _exit(5);
        }
        _exit(rc == 0 ? 0 : 2);
    }
    child_pid = born;
    if (close(fds[0]) != 0) {
        fail("close read end");
    }
    if (set_nonblock(fds[1]) != 0) {
        fail("O_NONBLOCK");
    }
    unsigned char *buf = calloc((size_t)TRANSFER, 1U);
    if (buf == NULL) {
        fail("calloc");
    }
    memset(buf, 0x5a, (size_t)TRANSFER);
    int saw_partial = 0;
    int rc = write_retry(fds[1], buf, (size_t)TRANSFER, &saw_partial);
    free(buf);
    if (close(fds[1]) != 0) {
        fail("close write end");
    }
    int status = 0;
    if (waitpid(child_pid, &status, 0) != child_pid) {
        fail("waitpid");
    }
    child_pid = -1;
    if (rc != 0 || !saw_partial || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fail("robust transfer");
    }
    printf("robust_transfer=%d\n", TRANSFER);
    puts("robust_saw_short_or_eagain=yes");
}

static void mode_demo(void) {
    observe_short_write();
    observe_eof();
    observe_eagain();
    observe_eintr();
    observe_transfer();
    puts("limit=positive_short_count_is_not_an_error");
    puts("children_reaped=yes");
    puts("temp_files=none");
}

int main(int argc, char **argv) {
    if (atexit(reap_for_cleanup) != 0) {
        fail("atexit");
    }
    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: stream_lab demo\n");
        return 2;
    }
    mode_demo();
    return 0;
}
