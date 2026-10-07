#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void checked_write(int fd, const void *data, size_t size) {
    const char *p = data;
    while (size > 0U) {
        ssize_t n = write(fd, p, size);
        if (n < 0) {
            if (errno == EINTR) continue;
            _exit(120);
        }
        p += (size_t)n;
        size -= (size_t)n;
    }
}

static void checked_read(int fd, void *data, size_t size) {
    char *p = data;
    while (size > 0U) {
        ssize_t n = read(fd, p, size);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) fail("read");
        p += (size_t)n;
        size -= (size_t)n;
    }
}

static void sleep_ms(long milliseconds) {
    struct timespec delay = {.tv_sec = milliseconds / 1000,
                             .tv_nsec = (milliseconds % 1000) * 1000000L};
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
}

static void mode_status(void) {
    pid_t child = fork();
    if (child < 0) fail("fork");
    if (child == 0) {
        execl("/bin/sh", "sh", "-c", "exit 7", (char *)NULL);
        _exit(127);
    }
    int status = 0;
    if (waitpid(child, &status, 0) != child) fail("waitpid");
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 7) fail("exit status");
    printf("exec_exit=7 child=%ld parent=%ld\n", (long)child, (long)getpid());

    child = fork();
    if (child < 0) fail("fork");
    if (child == 0) {
        for (;;) pause();
    }
    if (kill(child, SIGTERM) != 0) fail("kill");
    if (waitpid(child, &status, 0) != child) fail("waitpid");
    if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGTERM) fail("signal status");
    puts("signal_exit=SIGTERM");
}

static void mode_tree(void) {
    int fds[2];
    if (pipe(fds) != 0) fail("pipe");
    for (int i = 0; i < 2; ++i) {
        pid_t child = fork();
        if (child < 0) fail("fork");
        if (child == 0) {
            close(fds[0]);
            long ids[3] = {(long)i, (long)getpid(), (long)getppid()};
            checked_write(fds[1], ids, sizeof(ids));
            close(fds[1]);
            _exit(0);
        }
    }
    close(fds[1]);
    for (int i = 0; i < 2; ++i) {
        long ids[3];
        checked_read(fds[0], ids, sizeof(ids));
        printf("node=%ld pid=%ld ppid=%ld root=%ld\n",
               ids[0], ids[1], ids[2], (long)getpid());
    }
    close(fds[0]);
    while (wait(NULL) > 0) {}
    if (errno != ECHILD) fail("wait");
}

static char proc_state(pid_t pid) {
    char path[64];
    if (snprintf(path, sizeof(path), "/proc/%ld/stat", (long)pid) < 0) fail("snprintf");
    FILE *fp = fopen(path, "r");
    if (fp == NULL) fail("fopen /proc/pid/stat");
    long parsed_pid = 0;
    char comm[256];
    char state = '?';
    if (fscanf(fp, "%ld %255s %c", &parsed_pid, comm, &state) != 3) fail("fscanf");
    if (fclose(fp) != 0) fail("fclose");
    return state;
}

static void mode_zombie(void) {
    pid_t child = fork();
    if (child < 0) fail("fork");
    if (child == 0) _exit(23);
    char state = '?';
    for (int i = 0; i < 100; ++i) {
        state = proc_state(child);
        if (state == 'Z') break;
        sleep_ms(1);
    }
    if (state != 'Z') fail("zombie not observed");
    printf("zombie_observed pid=%ld state=Z\n", (long)child);
    int status = 0;
    if (waitpid(child, &status, 0) != child) fail("waitpid");
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 23) fail("zombie status");
    puts("zombie_reaped=yes");
}

struct report {
    long before;
    long after;
    long pid;
};

static void mode_orphan(void) {
    if (prctl(PR_SET_CHILD_SUBREAPER, 1) != 0) fail("prctl subreaper");
    int fds[2];
    int ready[2];
    if (pipe(fds) != 0 || pipe(ready) != 0) fail("pipe");
    pid_t middle = fork();
    if (middle < 0) fail("fork");
    if (middle == 0) {
        close(fds[0]);
        pid_t grandchild = fork();
        if (grandchild < 0) _exit(121);
        if (grandchild == 0) {
            close(ready[0]);
            struct report r = {.before = (long)getppid(), .after = 0, .pid = (long)getpid()};
            const unsigned char captured = 1;
            checked_write(ready[1], &captured, sizeof(captured));
            close(ready[1]);
            for (int i = 0; i < 200 && getppid() == (pid_t)r.before; ++i) sleep_ms(1);
            r.after = (long)getppid();
            checked_write(fds[1], &r, sizeof(r));
            close(fds[1]);
            _exit(r.after == (long)getppid() ? 0 : 122);
        }
        close(ready[1]);
        unsigned char captured = 0;
        checked_read(ready[0], &captured, sizeof(captured));
        close(ready[0]);
        close(fds[1]);
        _exit(captured == 1 ? 0 : 123);
    }
    close(fds[1]);
    close(ready[0]);
    close(ready[1]);
    int status = 0;
    if (waitpid(middle, &status, 0) != middle) fail("wait middle");
    struct report r;
    checked_read(fds[0], &r, sizeof(r));
    close(fds[0]);
    pid_t adopted = waitpid(-1, &status, 0);
    if (adopted != (pid_t)r.pid || r.before != (long)middle ||
        r.after != (long)getpid() || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fail("reparent verification");
    }
    printf("reparented pid=%ld from=%ld to_subreaper=%ld reaped=yes\n",
           r.pid, r.before, r.after);
}

static void mode_failures(void) {
    pid_t child = fork();
    if (child < 0) fail("fork");
    if (child == 0) {
        execl("/definitely/not/a/program", "missing", (char *)NULL);
        if (errno == ENOENT) _exit(127);
        _exit(126);
    }
    int status = 0;
    if (waitpid(child, &status, 0) != child) fail("waitpid");
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 127) fail("exec failure");
    errno = 0;
    if (waitpid(child, &status, 0) != -1 || errno != ECHILD) fail("double wait");
    puts("exec_failure=ENOENT mapped_exit=127");
    puts("wait_failure=ECHILD");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s status|tree|zombie|orphan|failures\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], "status") == 0) mode_status();
    else if (strcmp(argv[1], "tree") == 0) mode_tree();
    else if (strcmp(argv[1], "zombie") == 0) mode_zombie();
    else if (strcmp(argv[1], "orphan") == 0) mode_orphan();
    else if (strcmp(argv[1], "failures") == 0) mode_failures();
    else {
        fprintf(stderr, "unknown mode: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
