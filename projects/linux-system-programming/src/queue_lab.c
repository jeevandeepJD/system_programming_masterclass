#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

enum { CAPACITY = 8, ITEMS_PER_PRODUCER = 1000, PRODUCERS = 2, CONSUMERS = 2 };

struct queue {
    int items[CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
};

static struct queue queue = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .not_empty = PTHREAD_COND_INITIALIZER,
    .not_full = PTHREAD_COND_INITIALIZER
};

static void check(int error, const char *what) {
    if (error != 0) {
        fprintf(stderr, "%s: pthread error %d\n", what, error);
        exit(EXIT_FAILURE);
    }
}

static void put(int value) {
    check(pthread_mutex_lock(&queue.lock), "lock");
    while (queue.count == CAPACITY) {
        check(pthread_cond_wait(&queue.not_full, &queue.lock), "wait not_full");
    }
    queue.items[queue.tail] = value;
    queue.tail = (queue.tail + 1U) % CAPACITY;
    queue.count++;
    check(pthread_cond_signal(&queue.not_empty), "signal not_empty");
    check(pthread_mutex_unlock(&queue.lock), "unlock");
}

static int get(void) {
    check(pthread_mutex_lock(&queue.lock), "lock");
    while (queue.count == 0U) {
        check(pthread_cond_wait(&queue.not_empty, &queue.lock), "wait not_empty");
    }
    int value = queue.items[queue.head];
    queue.head = (queue.head + 1U) % CAPACITY;
    queue.count--;
    check(pthread_cond_signal(&queue.not_full), "signal not_full");
    check(pthread_mutex_unlock(&queue.lock), "unlock");
    return value;
}

static void *producer(void *arg) {
    long id = *(long *)arg;
    for (int i = 1; i <= ITEMS_PER_PRODUCER; ++i) {
        put((int)(id * ITEMS_PER_PRODUCER) + i);
    }
    return NULL;
}

struct result {
    long long sum;
    int count;
};

static void *consumer(void *arg) {
    (void)arg;
    struct result *result = calloc(1U, sizeof(*result));
    if (result == NULL) return NULL;
    for (;;) {
        int value = get();
        if (value == -1) break;
        result->sum += value;
        result->count++;
    }
    return result;
}

int main(void) {
    pthread_t producers[PRODUCERS];
    pthread_t consumers[CONSUMERS];
    long ids[PRODUCERS] = {0, 1};
    for (int i = 0; i < CONSUMERS; ++i) {
        check(pthread_create(&consumers[i], NULL, consumer, NULL), "create consumer");
    }
    for (int i = 0; i < PRODUCERS; ++i) {
        check(pthread_create(&producers[i], NULL, producer, &ids[i]), "create producer");
    }
    for (int i = 0; i < PRODUCERS; ++i) check(pthread_join(producers[i], NULL), "join producer");
    for (int i = 0; i < CONSUMERS; ++i) put(-1);

    long long sum = 0;
    int count = 0;
    for (int i = 0; i < CONSUMERS; ++i) {
        void *value = NULL;
        check(pthread_join(consumers[i], &value), "join consumer");
        struct result *result = value;
        if (result == NULL) {
            fputs("consumer allocation failed\n", stderr);
            exit(EXIT_FAILURE);
        }
        sum += result->sum;
        count += result->count;
        free(result);
    }
    long long n = (long long)PRODUCERS * ITEMS_PER_PRODUCER;
    long long expected = n * (n + 1LL) / 2LL;
    if (count != PRODUCERS * ITEMS_PER_PRODUCER || sum != expected || queue.count != 0U) {
        fputs("queue verification failed\n", stderr);
        return EXIT_FAILURE;
    }
    printf("bounded_queue=ok capacity=%d items=%d sum=%lld\n", CAPACITY, count, sum);
    puts("condition_waits=while_predicate_loops");
    return EXIT_SUCCESS;
}
