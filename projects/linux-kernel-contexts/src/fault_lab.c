#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t saw_usr1;

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void on_usr1(int signo) {
    (void)signo;
    saw_usr1 = 1;
}

static int reap(pid_t pid) {
    int status = 0;
    pid_t got;

    do {
        got = waitpid(pid, &status, 0);
    } while (got < 0 && errno == EINTR);
    if (got != pid) {
        fail("waitpid");
    }
    return status;
}

static void fault_child(void) {
    long page = sysconf(_SC_PAGESIZE);
    void *mem;
    volatile unsigned char *pointer;

    if (page <= 0) {
        _exit(2);
    }
    mem = mmap(NULL, (size_t)page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        _exit(3);
    }
    pointer = mem;
    *pointer = 1U;
    _exit(4);
}

static void signal_child(void) {
    struct sigaction action;

    memset(&action, 0, sizeof action);
    action.sa_handler = on_usr1;
    if (sigaction(SIGUSR1, &action, NULL) != 0) {
        _exit(5);
    }
    if (raise(SIGUSR1) != 0) {
        _exit(6);
    }
    _exit(saw_usr1 == 1 ? 0 : 7);
}

int main(int argc, char **argv) {
    pid_t fault_pid;
    pid_t signal_pid;
    int fault_status;
    int signal_status;

    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: %s demo\n", argv[0]);
        return EXIT_FAILURE;
    }

    alarm(3U);

    fault_pid = fork();
    if (fault_pid < 0) {
        fail("fork fault");
    }
    if (fault_pid == 0) {
        fault_child();
    }

    signal_pid = fork();
    if (signal_pid < 0) {
        fail("fork signal");
    }
    if (signal_pid == 0) {
        signal_child();
    }

    fault_status = reap(fault_pid);
    signal_status = reap(signal_pid);
    alarm(0U);

    if (!WIFSIGNALED(fault_status) || WTERMSIG(fault_status) != SIGSEGV) {
        fprintf(stderr, "expected child SIGSEGV\n");
        return EXIT_FAILURE;
    }
    if (!WIFEXITED(signal_status) || WEXITSTATUS(signal_status) != 0) {
        fprintf(stderr, "expected caught SIGUSR1\n");
        return EXIT_FAILURE;
    }

    puts("child_sigsegv=yes");
    puts("child_sigusr1_caught=yes");
    puts("children_reaped=yes");
    puts("fault_is_userspace_observation=yes");
    puts("irq_claim=none");
    return EXIT_SUCCESS;
}
