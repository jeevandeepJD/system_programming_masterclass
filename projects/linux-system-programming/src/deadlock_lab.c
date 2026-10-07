#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t locks[2] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};
static pthread_mutex_t gate_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_cond = PTHREAD_COND_INITIALIZER;
static int gate_count;

static void check(int error, const char *what) {
    if (error != 0) {
        fprintf(stderr, "%s: pthread error %d\n", what, error);
        exit(EXIT_FAILURE);
    }
}

static void rendezvous(void) {
    check(pthread_mutex_lock(&gate_lock), "gate lock");
    gate_count++;
    if (gate_count == 2) check(pthread_cond_broadcast(&gate_cond), "gate broadcast");
    while (gate_count < 2) check(pthread_cond_wait(&gate_cond, &gate_lock), "gate wait");
    check(pthread_mutex_unlock(&gate_lock), "gate unlock");
}

struct attempt {
    int first;
    int result;
};

static void *opposite_order(void *arg) {
    struct attempt *attempt = arg;
    int second = 1 - attempt->first;
    check(pthread_mutex_lock(&locks[attempt->first]), "first lock");
    rendezvous();
    struct timespec deadline;
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    deadline.tv_nsec += 100000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }
    attempt->result = pthread_mutex_timedlock(&locks[second], &deadline);
    if (attempt->result == 0) check(pthread_mutex_unlock(&locks[second]), "second unlock");
    check(pthread_mutex_unlock(&locks[attempt->first]), "first unlock");
    return NULL;
}

static void mode_detect(void) {
    pthread_t threads[2];
    struct attempt attempts[2] = {{0, 0}, {1, 0}};
    for (int i = 0; i < 2; ++i) {
        check(pthread_create(&threads[i], NULL, opposite_order, &attempts[i]), "create");
    }
    for (int i = 0; i < 2; ++i) check(pthread_join(threads[i], NULL), "join");
    if (attempts[0].result != ETIMEDOUT && attempts[1].result != ETIMEDOUT) {
        fputs("expected at least one timed-out lock\n", stderr);
        exit(EXIT_FAILURE);
    }
    printf("deadlock_cycle_detected=yes results=%d,%d test_hung=no\n",
           attempts[0].result, attempts[1].result);
}

static void *ordered(void *arg) {
    (void)arg;
    check(pthread_mutex_lock(&locks[0]), "lower lock");
    check(pthread_mutex_lock(&locks[1]), "higher lock");
    check(pthread_mutex_unlock(&locks[1]), "higher unlock");
    check(pthread_mutex_unlock(&locks[0]), "lower unlock");
    return NULL;
}

static void mode_ordered(void) {
    pthread_t threads[2];
    for (int i = 0; i < 2; ++i) check(pthread_create(&threads[i], NULL, ordered, NULL), "create");
    for (int i = 0; i < 2; ++i) check(pthread_join(threads[i], NULL), "join");
    puts("lock_order=0_then_1 completed=yes");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s detect|ordered\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], "detect") == 0) mode_detect();
    else if (strcmp(argv[1], "ordered") == 0) mode_ordered();
    else {
        fprintf(stderr, "unknown mode: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
