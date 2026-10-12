#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <unistd.h>

static char workdir[] = "/tmp/week12-debug.XXXXXX";
static int workdir_live;

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void cleanup(void) {
    if (!workdir_live) {
        return;
    }
    char path[128];
    int wrote = snprintf(path, sizeof(path), "%s/payload", workdir);
    if (wrote > 0 && (size_t)wrote < sizeof(path)) {
        unlink(path);
    }
    rmdir(workdir);
    workdir_live = 0;
}

static void observe_proc_fd(int fd, const char *path) {
    char linkpath[64];
    int wrote = snprintf(linkpath, sizeof(linkpath), "/proc/self/fd/%d", fd);
    if (wrote < 0 || (size_t)wrote >= sizeof(linkpath)) {
        fail("fd path");
    }
    char target[256];
    ssize_t n = readlink(linkpath, target, sizeof(target) - 1U);
    if (n < 0) {
        fail("readlink");
    }
    target[n] = '\0';
    printf("proc_fd_target=%s\n", target);
    if (strstr(target, "payload") == NULL) {
        fail("readlink target");
    }
    puts("proc_fd_matches=yes");

    char needle[32];
    wrote = snprintf(needle, sizeof(needle), "%d", fd);
    if (wrote < 0 || (size_t)wrote >= sizeof(needle)) {
        fail("fd name");
    }
    DIR *dir = opendir("/proc/self/fd");
    if (dir == NULL) {
        fail("opendir /proc/self/fd");
    }
    int found = 0;
    const struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, needle) == 0) {
            found = 1;
        }
    }
    if (closedir(dir) != 0) {
        fail("closedir");
    }
    if (!found) {
        fail("fd not listed");
    }
    puts("proc_self_fd_listed=yes");
    if (strstr(target, path) == NULL && strstr(target, "payload") == NULL) {
        fail("path evidence");
    }
}

static void observe_ebadf(int fd) {
    if (close(fd) != 0) {
        fail("close");
    }
    unsigned char byte = 0;
    ssize_t n = read(fd, &byte, 1);
    if (n >= 0 || errno != EBADF) {
        fail("expected EBADF");
    }
    puts("deliberate_ebadf=yes");
}

static void observe_epipe(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) != 0) {
        fail("sigaction SIGPIPE");
    }
    int fds[2];
    if (pipe(fds) != 0) {
        fail("pipe");
    }
    if (close(fds[0]) != 0) {
        fail("close reader");
    }
    ssize_t n = write(fds[1], "x", 1);
    int saved = errno;
    if (close(fds[1]) != 0) {
        fail("close writer");
    }
    if (n >= 0 || saved != EPIPE) {
        errno = saved;
        fail("expected EPIPE");
    }
    puts("deliberate_epipe=yes");
}

static void observe_epoll_failures(const char *path) {
    int ep = epoll_create1(EPOLL_CLOEXEC);
    if (ep < 0) {
        fail("epoll_create1");
    }
    int reg = open(path, O_RDONLY | O_CLOEXEC);
    if (reg < 0) {
        fail("open regular");
    }
    int stale = dup(reg);
    if (stale < 0) {
        fail("dup");
    }
    if (close(stale) != 0) {
        fail("close stale");
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = (uint32_t)EPOLLIN;
    if (epoll_ctl(ep, EPOLL_CTL_ADD, stale, &ev) == 0 || errno != EBADF) {
        fail("expected epoll EBADF");
    }
    puts("epoll_bad_fd=EBADF");

    if (epoll_ctl(ep, EPOLL_CTL_ADD, reg, &ev) == 0 || errno != EPERM) {
        fail("expected epoll EPERM");
    }
    puts("epoll_regular_file=EPERM");
    if (close(reg) != 0 || close(ep) != 0) {
        fail("close");
    }
    if (epoll_wait(ep, &ev, 1, 0) == 0 || errno != EBADF) {
        fail("expected epoll_wait EBADF");
    }
    puts("epoll_wait_closed=EBADF");
}

static void mode_demo(void) {
    if (mkdtemp(workdir) == NULL) {
        fail("mkdtemp");
    }
    workdir_live = 1;
    char path[128];
    int wrote = snprintf(path, sizeof(path), "%s/payload", workdir);
    if (wrote < 0 || (size_t)wrote >= sizeof(path)) {
        fail("path");
    }
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, (mode_t)0600);
    if (fd < 0) {
        fail("open");
    }
    if (write(fd, "evidence", 8) != 8) {
        fail("write");
    }
    observe_proc_fd(fd, path);
    observe_ebadf(fd);
    observe_epipe();
    observe_epoll_failures(path);
    puts("limit=errno_is_the_evidence");
    puts("descendants=none");
    cleanup();
    struct stat st;
    if (stat(workdir, &st) == 0) {
        fail("temp directory remains");
    }
    puts("temp_cleanup=done");
}

int main(int argc, char **argv) {
    if (atexit(cleanup) != 0) {
        fail("atexit");
    }
    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: debug_lab demo\n");
        return 2;
    }
    mode_demo();
    return 0;
}
