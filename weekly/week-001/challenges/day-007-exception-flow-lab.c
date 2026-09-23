#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

enum action {
    ACTION_NORMAL,
    ACTION_SIGILL,
    ACTION_SIGSEGV,
    ACTION_SIGFPE,
    ACTION_ALL
};

static void signal_handler(int signo)
{
    static const char ill_message[] =
        "child handler: caught SIGILL; exiting safely\n";
    static const char segv_message[] =
        "child handler: caught SIGSEGV; exiting safely\n";
    static const char fpe_message[] =
        "child handler: caught SIGFPE; exiting safely\n";
    static const char other_message[] =
        "child handler: caught an unexpected signal; exiting safely\n";

    switch (signo) {
    case SIGILL:
        (void)write(STDERR_FILENO, ill_message, sizeof(ill_message) - 1U);
        break;
    case SIGSEGV:
        (void)write(STDERR_FILENO, segv_message, sizeof(segv_message) - 1U);
        break;
    case SIGFPE:
        (void)write(STDERR_FILENO, fpe_message, sizeof(fpe_message) - 1U);
        break;
    default:
        (void)write(STDERR_FILENO, other_message,
                    sizeof(other_message) - 1U);
        break;
    }

    _exit(128 + signo);
}

static int install_handlers(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    if (sigemptyset(&action.sa_mask) == -1) {
        return -1;
    }

    if (sigaction(SIGILL, &action, NULL) == -1 ||
        sigaction(SIGSEGV, &action, NULL) == -1 ||
        sigaction(SIGFPE, &action, NULL) == -1) {
        return -1;
    }

    return 0;
}

static void run_normal_loop(void)
{
    int sum = 0;

    puts("normal loop:");
    for (int i = 0; i < 5; ++i) {
        sum += i;
        printf("  i=%d sum=%d\n", i, sum);
    }
}

static void trigger_in_child(const char *name, int signo)
{
    pid_t child;
    int status;

    if (fflush(NULL) == EOF) {
        perror("fflush");
        exit(EXIT_FAILURE);
    }

    child = fork();
    if (child == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (child == 0) {
        dprintf(STDOUT_FILENO,
                "child %ld: requesting %s with raise(%d)\n",
                (long)getpid(), name, signo);
        if (raise(signo) != 0) {
            dprintf(STDERR_FILENO, "raise: %s\n", strerror(errno));
            _exit(EXIT_FAILURE);
        }
        _exit(EXIT_FAILURE);
    }

    do {
        if (waitpid(child, &status, 0) == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        break;
    } while (1);

    if (WIFEXITED(status)) {
        printf("parent: child %ld exited with status %d; parent continues\n",
               (long)child, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("parent: child %ld terminated by signal %d; parent continues\n",
               (long)child, WTERMSIG(status));
    } else {
        printf("parent: child %ld changed state unexpectedly; parent continues\n",
               (long)child);
    }
}

static enum action parse_action(const char *text)
{
    if (strcmp(text, "normal") == 0) {
        return ACTION_NORMAL;
    }
    if (strcmp(text, "ill") == 0) {
        return ACTION_SIGILL;
    }
    if (strcmp(text, "segv") == 0) {
        return ACTION_SIGSEGV;
    }
    if (strcmp(text, "fpe") == 0) {
        return ACTION_SIGFPE;
    }
    if (strcmp(text, "all") == 0) {
        return ACTION_ALL;
    }

    fprintf(stderr, "unknown action: %s\n", text);
    fprintf(stderr, "usage: exception-flow-lab {normal|ill|segv|fpe|all}\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    enum action action;

    if (argc != 2) {
        fprintf(stderr,
                "usage: %s {normal|ill|segv|fpe|all}\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    if (install_handlers() == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    action = parse_action(argv[1]);

    if (action == ACTION_NORMAL || action == ACTION_ALL) {
        run_normal_loop();
    }
    if (action == ACTION_SIGILL || action == ACTION_ALL) {
        trigger_in_child("SIGILL", SIGILL);
    }
    if (action == ACTION_SIGSEGV || action == ACTION_ALL) {
        trigger_in_child("SIGSEGV", SIGSEGV);
    }
    if (action == ACTION_SIGFPE || action == ACTION_ALL) {
        trigger_in_child("SIGFPE", SIGFPE);
    }

    puts("parent: lab completed without crashing the session");
    return EXIT_SUCCESS;
}
