#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void close_or_fail(int fd) {
    if (close(fd) != 0) {
        fail("close");
    }
}

int main(int argc, char **argv) {
    int event;
    int timer;
    struct pollfd ready;
    struct itimerspec spec;
    uint64_t one = 1;
    uint64_t got = 0;
    uint64_t expirations = 0;
    int empty;
    int filled;
    int waited;

    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: %s demo\n", argv[0]);
        return EXIT_FAILURE;
    }

    event = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (event < 0) {
        fail("eventfd");
    }
    memset(&ready, 0, sizeof ready);
    ready.fd = event;
    ready.events = POLLIN;
    empty = poll(&ready, 1, 0);
    if (empty != 0) {
        fprintf(stderr, "empty eventfd poll returned %d\n", empty);
        return EXIT_FAILURE;
    }
    if (write(event, &one, sizeof one) != (ssize_t)sizeof one) {
        fail("eventfd write");
    }
    ready.revents = 0;
    filled = poll(&ready, 1, 0);
    if (filled != 1) {
        fprintf(stderr, "ready eventfd poll returned %d\n", filled);
        return EXIT_FAILURE;
    }
    if (read(event, &got, sizeof got) != (ssize_t)sizeof got || got != 1U) {
        fprintf(stderr, "eventfd value mismatch\n");
        return EXIT_FAILURE;
    }
    close_or_fail(event);

    timer = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (timer < 0) {
        fail("timerfd_create");
    }
    memset(&spec, 0, sizeof spec);
    spec.it_value.tv_nsec = 20000000L;
    if (timerfd_settime(timer, 0, &spec, NULL) != 0) {
        fail("timerfd_settime");
    }
    memset(&ready, 0, sizeof ready);
    ready.fd = timer;
    ready.events = POLLIN;
    waited = poll(&ready, 1, 500);
    if (waited != 1) {
        fprintf(stderr, "timerfd poll returned %d\n", waited);
        return EXIT_FAILURE;
    }
    if (read(timer, &expirations, sizeof expirations) != (ssize_t)sizeof expirations ||
        expirations == 0U) {
        fprintf(stderr, "timerfd produced no expiration\n");
        return EXIT_FAILURE;
    }
    close_or_fail(timer);

    puts("analog=userspace_poll");
    puts("not_an_irq_handler=yes");
    puts("not_a_softirq=yes");
    puts("not_a_workqueue=yes");
    puts("eventfd_empty_poll=0");
    puts("eventfd_ready_poll=1");
    puts("eventfd_value=1");
    puts("timerfd_poll=ready");
    puts("descendants=none");
    return EXIT_SUCCESS;
}
