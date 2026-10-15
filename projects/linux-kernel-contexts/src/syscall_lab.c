#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    int fds[2];
    char out = 'k';
    char in = 0;
    pid_t libc_pid;
    long raw;

    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: %s demo\n", argv[0]);
        return EXIT_FAILURE;
    }

    libc_pid = getpid();
    raw = syscall(SYS_getpid);
    if (raw < 0) {
        fail("syscall getpid");
    }
    printf("libc_getpid=%ld\n", (long)libc_pid);
    printf("syscall_getpid=%ld\n", raw);
    if (raw != (long)libc_pid) {
        fprintf(stderr, "pid mismatch\n");
        return EXIT_FAILURE;
    }
    puts("getpid_matches=yes");

    if (pipe(fds) != 0) {
        fail("pipe");
    }
    if (write(fds[1], &out, 1) != 1) {
        fail("write");
    }
    if (read(fds[0], &in, 1) != 1) {
        fail("read");
    }
    if (in != 'k') {
        fprintf(stderr, "pipe byte mismatch\n");
        return EXIT_FAILURE;
    }
    if (close(fds[0]) != 0 || close(fds[1]) != 0) {
        fail("close");
    }

    puts("pipe_byte=k");
    puts("syscall_completed=yes");
    puts("userspace_syscall=yes");
    puts("entry_path_claim=none");
    puts("descendants=none");
    return EXIT_SUCCESS;
}
