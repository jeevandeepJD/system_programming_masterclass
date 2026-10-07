#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t handled;
static int signal_pipe[2] = {-1, -1};

static void count_handler(int signo) {
    (void)signo;
    handled = 1;
}

static void pipe_handler(int signo) {
    unsigned char byte = (unsigned char)signo;
    int saved = errno;
    if (signal_pipe[1] >= 0) (void)write(signal_pipe[1], &byte, 1);
    errno = saved;
}

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void install(int signo, void (*handler)(int)) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handler;
    if (sigemptyset(&action.sa_mask) != 0) fail("sigemptyset");
    action.sa_flags = SA_RESTART;
    if (sigaction(signo, &action, NULL) != 0) fail("sigaction");
}

static void mode_pending(void) {
    sigset_t block;
    if (sigemptyset(&block) != 0 || sigaddset(&block, SIGUSR1) != 0) fail("signal set");
    install(SIGUSR1, count_handler);
    if (sigprocmask(SIG_BLOCK, &block, NULL) != 0) fail("sigprocmask block");
    if (raise(SIGUSR1) != 0) fail("raise");
    sigset_t pending;
    if (sigpending(&pending) != 0) fail("sigpending");
    int is_pending = sigismember(&pending, SIGUSR1);
    if (is_pending != 1 || handled != 0) fail("pending verification");
    puts("blocked_pending=yes handled_before_unblock=0");
    if (sigprocmask(SIG_UNBLOCK, &block, NULL) != 0) fail("sigprocmask unblock");
    if (handled != 1) fail("pending delivery");
    puts("handled_after_unblock=1");
}

static void mode_self_pipe(void) {
    if (pipe(signal_pipe) != 0) fail("pipe");
    int flags = fcntl(signal_pipe[1], F_GETFL);
    if (flags < 0 || fcntl(signal_pipe[1], F_SETFL, flags | O_NONBLOCK) != 0) fail("fcntl");
    install(SIGUSR2, pipe_handler);
    if (raise(SIGUSR2) != 0) fail("raise");
    unsigned char byte = 0;
    ssize_t n;
    do {
        n = read(signal_pipe[0], &byte, 1);
    } while (n < 0 && errno == EINTR);
    if (n != 1 || byte != (unsigned char)SIGUSR2) fail("self-pipe read");
    close(signal_pipe[0]);
    close(signal_pipe[1]);
    signal_pipe[0] = -1;
    signal_pipe[1] = -1;
    printf("self_pipe_byte=%u async_handler_used=write\n", (unsigned int)byte);
}

static void mode_failures(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = count_handler;
    if (sigemptyset(&action.sa_mask) != 0) fail("sigemptyset");
    errno = 0;
    if (sigaction(SIGKILL, &action, NULL) != -1 || errno != EINVAL) {
        fail("SIGKILL handler rejection");
    }
    errno = 0;
    if (kill((pid_t)999999999, 0) != -1 || errno != ESRCH) fail("missing pid probe");
    puts("sigaction_SIGKILL=EINVAL");
    puts("kill_missing_pid=ESRCH");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s pending|self-pipe|failures\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], "pending") == 0) mode_pending();
    else if (strcmp(argv[1], "self-pipe") == 0) mode_self_pipe();
    else if (strcmp(argv[1], "failures") == 0) mode_failures();
    else {
        fprintf(stderr, "unknown mode: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
