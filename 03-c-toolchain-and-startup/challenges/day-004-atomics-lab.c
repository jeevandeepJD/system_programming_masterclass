/*
 * Day 4 atomics lab: defined communication between POSIX threads.
 *
 * Build:
 *   cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -g -pthread \
 *      day-004-atomics-lab.c -o /tmp/week3-day4-atomics
 */
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

enum { WORKERS = 4, ITERATIONS = 100000 };

struct message {
    int payload;
    atomic_bool ready;
};

static atomic_uint counter;

static void *count_worker(void *unused)
{
    (void)unused;
    for (unsigned int i = 0U; i < ITERATIONS; ++i) {
        atomic_fetch_add_explicit(&counter, 1U, memory_order_relaxed);
    }
    return NULL;
}

static void *producer(void *argument)
{
    struct message *message = argument;

    message->payload = 2026;
    atomic_store_explicit(&message->ready, 1, memory_order_release);
    return NULL;
}

static void *consumer(void *argument)
{
    struct message *message = argument;

    while (!atomic_load_explicit(&message->ready, memory_order_acquire)) {
        /* The exercise favors clarity; production code should avoid hot spinning. */
    }
    printf("acquire observed published payload=%d\n", message->payload);
    return NULL;
}

static int join_created(pthread_t *threads, size_t count)
{
    int failed = 0;

    for (size_t i = 0U; i < count; ++i) {
        if (pthread_join(threads[i], NULL) != 0) {
            failed = 1;
        }
    }
    return failed;
}

int main(void)
{
    pthread_t workers[WORKERS];
    pthread_t publishing_threads[2];
    struct message message;
    size_t created = 0U;

    message.payload = 0;
    atomic_init(&message.ready, 0);
    atomic_init(&counter, 0U);
    for (; created < WORKERS; ++created) {
        if (pthread_create(&workers[created], NULL, count_worker, NULL) != 0) {
            fputs("could not create counter worker\n", stderr);
            (void)join_created(workers, created);
            return EXIT_FAILURE;
        }
    }
    if (join_created(workers, WORKERS) != 0) {
        fputs("could not join counter worker\n", stderr);
        return EXIT_FAILURE;
    }

    printf("relaxed atomic counter=%u expected=%u\n",
           atomic_load_explicit(&counter, memory_order_relaxed),
           (unsigned int)(WORKERS * ITERATIONS));
    printf("atomic_uint lock-free on this run: %s\n",
           atomic_is_lock_free(&counter) ? "yes" : "no");

    if (pthread_create(&publishing_threads[0], NULL, consumer, &message) != 0) {
        fputs("could not create consumer\n", stderr);
        return EXIT_FAILURE;
    }
    if (pthread_create(&publishing_threads[1], NULL, producer, &message) != 0) {
        fputs("could not create producer\n", stderr);
        atomic_store_explicit(&message.ready, 1, memory_order_release);
        (void)pthread_join(publishing_threads[0], NULL);
        return EXIT_FAILURE;
    }
    if (join_created(publishing_threads, 2U) != 0) {
        fputs("could not join publishing threads\n", stderr);
        return EXIT_FAILURE;
    }

    puts("relaxed preserved the counter; release/acquire published the payload");
    return EXIT_SUCCESS;
}
