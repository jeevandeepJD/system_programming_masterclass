#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

struct rule {
    const char *name;
    int may_sleep;
};

static const struct rule rules[] = {
    {"hardirq", 0},
    {"softirq", 0},
    {"wq_bh", 0},
    {"worker_thread", 1},
    {"process", 1},
};

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static long elapsed_ms(const struct timespec *start, const struct timespec *end) {
    long sec = (long)end->tv_sec - (long)start->tv_sec;
    long nsec = (long)end->tv_nsec - (long)start->tv_nsec;

    if (nsec < 0) {
        sec--;
        nsec += 1000000000L;
    }
    return sec * 1000L + nsec / 1000000L;
}

static void print_rules(void) {
    size_t i;

    for (i = 0; i < sizeof rules / sizeof rules[0]; i++) {
        printf("rule_%s_may_sleep=%s\n", rules[i].name, rules[i].may_sleep ? "yes" : "no");
    }
}

int main(int argc, char **argv) {
    int event;
    int timer;
    struct pollfd wait;
    struct itimerspec spec;
    struct timespec start;
    struct timespec end;
    uint64_t expirations = 0;
    int immediate;
    int blocked;
    long waited_ms;

    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: %s demo\n", argv[0]);
        return EXIT_FAILURE;
    }

    print_rules();

    event = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (event < 0) {
        fail("eventfd");
    }
    memset(&wait, 0, sizeof wait);
    wait.fd = event;
    wait.events = POLLIN;
    immediate = poll(&wait, 1, 0);
    if (close(event) != 0) {
        fail("close eventfd");
    }
    if (immediate != 0) {
        fprintf(stderr, "zero timeout poll returned %d\n", immediate);
        return EXIT_FAILURE;
    }

    timer = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (timer < 0) {
        fail("timerfd_create");
    }
    memset(&spec, 0, sizeof spec);
    spec.it_value.tv_nsec = 20000000L;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        fail("clock_gettime start");
    }
    if (timerfd_settime(timer, 0, &spec, NULL) != 0) {
        fail("timerfd_settime");
    }
    memset(&wait, 0, sizeof wait);
    wait.fd = timer;
    wait.events = POLLIN;
    blocked = poll(&wait, 1, 500);
    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        fail("clock_gettime end");
    }
    if (blocked == 1) {
        if (read(timer, &expirations, sizeof expirations) != (ssize_t)sizeof expirations) {
            fail("timerfd read");
        }
    }
    if (close(timer) != 0) {
        fail("close timerfd");
    }
    waited_ms = elapsed_ms(&start, &end);
    if (blocked != 1 || expirations == 0U || waited_ms < 10L) {
        fprintf(stderr, "blocking poll did not wait for timerfd\n");
        return EXIT_FAILURE;
    }

    puts("zero_poll_immediate=yes");
    puts("blocking_poll_waited=yes");
    puts("userspace_poll_is_not_kernel_context=yes");
    puts("kernel_preempt_claim=none");
    puts("scheduler_dive=deferred_to_week_15");
    puts("descendants=none");
    return EXIT_SUCCESS;
}
