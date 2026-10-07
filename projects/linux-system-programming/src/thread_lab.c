#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { THREADS = 4, ITERATIONS = 200000 };

static uint64_t unsafe_counter;
static uint64_t mutex_counter;
static atomic_uint_fast64_t atomic_counter;
static pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;

static void check(int error, const char *what) {
    if (error != 0) {
        fprintf(stderr, "%s: pthread error %d\n", what, error);
        exit(EXIT_FAILURE);
    }
}

static void *return_id(void *arg) {
    return arg;
}

static void *increment_unsafe(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; ++i) unsafe_counter++;
    return NULL;
}

static void *increment_mutex(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; ++i) {
        check(pthread_mutex_lock(&counter_lock), "mutex lock");
        mutex_counter++;
        check(pthread_mutex_unlock(&counter_lock), "mutex unlock");
    }
    return NULL;
}

static void *increment_atomic(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; ++i) {
        (void)atomic_fetch_add_explicit(&atomic_counter, 1U, memory_order_relaxed);
    }
    return NULL;
}

static void run_workers(void *(*worker)(void *)) {
    pthread_t threads[THREADS];
    for (int i = 0; i < THREADS; ++i) check(pthread_create(&threads[i], NULL, worker, NULL), "create");
    for (int i = 0; i < THREADS; ++i) check(pthread_join(threads[i], NULL), "join");
}

static void mode_basic(void) {
    pthread_t thread;
    int token = 42;
    void *result = NULL;
    check(pthread_create(&thread, NULL, return_id, &token), "create");
    check(pthread_join(thread, &result), "join");
    if (result != &token) {
        fputs("join result mismatch\n", stderr);
        exit(EXIT_FAILURE);
    }
    puts("pthread_create_join=ok result=42");
}

static void mode_counter(const char *mode) {
    uint64_t expected = (uint64_t)THREADS * (uint64_t)ITERATIONS;
    if (strcmp(mode, "race") == 0) {
        unsafe_counter = 0;
        run_workers(increment_unsafe);
        printf("race_counter=%llu expected=%llu data_race=undefined_behavior\n",
               (unsigned long long)unsafe_counter, (unsigned long long)expected);
    } else if (strcmp(mode, "mutex") == 0) {
        mutex_counter = 0;
        run_workers(increment_mutex);
        if (mutex_counter != expected) exit(EXIT_FAILURE);
        printf("mutex_counter=%llu expected=%llu\n",
               (unsigned long long)mutex_counter, (unsigned long long)expected);
    } else {
        atomic_store_explicit(&atomic_counter, 0U, memory_order_relaxed);
        run_workers(increment_atomic);
        uint64_t value = atomic_load_explicit(&atomic_counter, memory_order_relaxed);
        if (value != expected) exit(EXIT_FAILURE);
        printf("atomic_counter=%llu expected=%llu order=relaxed\n",
               (unsigned long long)value, (unsigned long long)expected);
    }
}

struct owned {
    int value;
};

static void *consume_owned(void *arg) {
    struct owned *item = arg;
    item->value *= 2;
    return item;
}

static void mode_ownership(void) {
    struct owned *item = malloc(sizeof(*item));
    if (item == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    item->value = 21;
    pthread_t thread;
    check(pthread_create(&thread, NULL, consume_owned, item), "create");
    void *result = NULL;
    check(pthread_join(thread, &result), "join");
    item = result;
    if (item == NULL || item->value != 42) exit(EXIT_FAILURE);
    printf("ownership=heap_transfer joined_value=%d freed_by=parent\n", item->value);
    free(item);
}

struct adjacent {
    atomic_uint_fast64_t value[2];
};

struct padded_counter {
    _Alignas(64) atomic_uint_fast64_t value;
    unsigned char padding[64];
};

struct bench_arg {
    atomic_uint_fast64_t *counter;
};

static void *bench_worker(void *arg) {
    struct bench_arg *bench = arg;
    for (int i = 0; i < ITERATIONS; ++i) {
        (void)atomic_fetch_add_explicit(bench->counter, 1U, memory_order_relaxed);
    }
    return NULL;
}

static double elapsed(const struct timespec *start, const struct timespec *end) {
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

static double bench_pair(atomic_uint_fast64_t *a, atomic_uint_fast64_t *b) {
    pthread_t threads[2];
    struct bench_arg args[2] = {{a}, {b}};
    struct timespec start;
    struct timespec end;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < 2; ++i) check(pthread_create(&threads[i], NULL, bench_worker, &args[i]), "create");
    for (int i = 0; i < 2; ++i) check(pthread_join(threads[i], NULL), "join");
    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return elapsed(&start, &end);
}

static void mode_false_sharing(void) {
    struct adjacent near = {0};
    struct padded_counter far[2] = {0};
    double near_time = bench_pair(&near.value[0], &near.value[1]);
    double far_time = bench_pair(&far[0].value, &far[1].value);
    printf("adjacent_seconds=%.6f padded_seconds=%.6f\n", near_time, far_time);
    puts("caveat=timings_are_noisy_and_not_a_correctness_test");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s basic|race|mutex|atomic|ownership|false-sharing\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], "basic") == 0) mode_basic();
    else if (strcmp(argv[1], "race") == 0) mode_counter("race");
    else if (strcmp(argv[1], "mutex") == 0) mode_counter("mutex");
    else if (strcmp(argv[1], "atomic") == 0) mode_counter("atomic");
    else if (strcmp(argv[1], "ownership") == 0) mode_ownership();
    else if (strcmp(argv[1], "false-sharing") == 0) mode_false_sharing();
    else {
        fprintf(stderr, "unknown mode: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
