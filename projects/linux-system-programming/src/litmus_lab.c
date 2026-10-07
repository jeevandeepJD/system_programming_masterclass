#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

enum { ROUNDS = 100000 };

static atomic_int x;
static atomic_int y;
static int observed[2];
static pthread_barrier_t start_barrier;
static pthread_barrier_t finish_barrier;

static void check(int error, const char *what) {
    if (error != 0) {
        fprintf(stderr, "%s: pthread error %d\n", what, error);
        exit(EXIT_FAILURE);
    }
}

static void barrier_wait(pthread_barrier_t *barrier) {
    int result = pthread_barrier_wait(barrier);
    if (result != 0 && result != PTHREAD_BARRIER_SERIAL_THREAD) check(result, "barrier");
}

static void *worker(void *arg) {
    long id = *(long *)arg;
    for (int round = 0; round < ROUNDS; ++round) {
        barrier_wait(&start_barrier);
        if (id == 0) {
            atomic_store_explicit(&x, 1, memory_order_relaxed);
            observed[0] = atomic_load_explicit(&y, memory_order_relaxed);
        } else {
            atomic_store_explicit(&y, 1, memory_order_relaxed);
            observed[1] = atomic_load_explicit(&x, memory_order_relaxed);
        }
        barrier_wait(&finish_barrier);
    }
    return NULL;
}

int main(void) {
    check(pthread_barrier_init(&start_barrier, NULL, 3U), "start barrier init");
    check(pthread_barrier_init(&finish_barrier, NULL, 3U), "finish barrier init");
    pthread_t threads[2];
    long ids[2] = {0, 1};
    for (int i = 0; i < 2; ++i) check(pthread_create(&threads[i], NULL, worker, &ids[i]), "create");
    unsigned long both_zero = 0;
    for (int round = 0; round < ROUNDS; ++round) {
        atomic_store_explicit(&x, 0, memory_order_relaxed);
        atomic_store_explicit(&y, 0, memory_order_relaxed);
        barrier_wait(&start_barrier);
        barrier_wait(&finish_barrier);
        if (observed[0] == 0 && observed[1] == 0) both_zero++;
    }
    for (int i = 0; i < 2; ++i) check(pthread_join(threads[i], NULL), "join");
    check(pthread_barrier_destroy(&start_barrier), "start barrier destroy");
    check(pthread_barrier_destroy(&finish_barrier), "finish barrier destroy");
    printf("store_buffering rounds=%d both_zero=%lu order=relaxed\n", ROUNDS, both_zero);
    puts("caveat=x86_observation_is_not_C_language_proof");
    return EXIT_SUCCESS;
}
