#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile uint64_t sink;

static double now_seconds(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1.0e-9;
}

static uint64_t rng_next(uint64_t *state)
{
    uint64_t x = *state;

    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return x * UINT64_C(2685821657736338717);
}

static void shuffle(size_t *a, size_t n)
{
    uint64_t state = UINT64_C(0x6a09e667f3bcc909);

    for (size_t i = n; i > 1; --i) {
        size_t j = (size_t)(rng_next(&state) % i);
        size_t tmp = a[i - 1];
        a[i - 1] = a[j];
        a[j] = tmp;
    }
}

static size_t parse_max_mib(int argc, char **argv)
{
    char *end = NULL;
    unsigned long value;

    if (argc == 1) {
        return 64;
    }
    if (argc != 2) {
        fprintf(stderr, "usage: %s [maximum-working-set-MiB]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    errno = 0;
    value = strtoul(argv[1], &end, 10);
    if (errno != 0 || end == argv[1] || *end != '\0' || value < 1 ||
        value > 1024) {
        fprintf(stderr, "maximum-working-set-MiB must be 1..1024\n");
        exit(EXIT_FAILURE);
    }
    return (size_t)value;
}

static uint64_t sequential_scan(const uint64_t *data, size_t n, size_t passes)
{
    uint64_t sum = 0;

    for (size_t pass = 0; pass < passes; ++pass) {
        for (size_t i = 0; i < n; ++i) {
            sum += data[i];
        }
    }
    return sum;
}

static size_t pointer_chase(const size_t *next, size_t index, size_t steps)
{
    for (size_t i = 0; i < steps; ++i) {
        index = next[index];
    }
    return index;
}

int main(int argc, char **argv)
{
    const size_t max_mib = parse_max_mib(argc, argv);
    const size_t max_bytes = max_mib * 1024U * 1024U;
    const size_t max_n = max_bytes / sizeof(uint64_t);
    uint64_t *data = malloc(max_n * sizeof(*data));
    size_t *order = malloc(max_n * sizeof(*order));
    size_t *next = malloc(max_n * sizeof(*next));

    if (data == NULL || order == NULL || next == NULL) {
        fprintf(stderr, "allocation failed for a %zu MiB working set\n", max_mib);
        free(next);
        free(order);
        free(data);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < max_n; ++i) {
        data[i] = (uint64_t)i * UINT64_C(0x9e3779b97f4a7c15);
        order[i] = i;
    }

    puts("mode,bytes,accesses,seconds,ns_per_access,checksum");

    for (size_t bytes = 4U * 1024U; bytes <= max_bytes; bytes *= 2U) {
        const size_t n = bytes / sizeof(*data);
        size_t seq_passes = (256U * 1024U * 1024U) / bytes;
        const size_t chase_steps = n < 8000000U ? 8000000U : n;
        double start;
        double elapsed;
        uint64_t sum;
        size_t index;

        if (seq_passes < 1) {
            seq_passes = 1;
        }

        /* Warm-up is not a promise of a noise-free or entirely resident run. */
        sink ^= sequential_scan(data, n, 1);
        start = now_seconds();
        sum = sequential_scan(data, n, seq_passes);
        elapsed = now_seconds() - start;
        sink ^= sum;
        printf("sequential,%zu,%zu,%.9f,%.3f,%" PRIu64 "\n", bytes,
               n * seq_passes, elapsed,
               elapsed * 1.0e9 / (double)(n * seq_passes), sum);

        shuffle(order, n);
        for (size_t i = 0; i + 1 < n; ++i) {
            next[order[i]] = order[i + 1];
        }
        next[order[n - 1]] = order[0];

        index = pointer_chase(next, order[0], n);
        start = now_seconds();
        index = pointer_chase(next, index, chase_steps);
        elapsed = now_seconds() - start;
        sink ^= (uint64_t)index;
        printf("random-chase,%zu,%zu,%.9f,%.3f,%zu\n", bytes, chase_steps,
               elapsed, elapsed * 1.0e9 / (double)chase_steps, index);

        if (bytes > max_bytes / 2U) {
            break;
        }
    }

    free(next);
    free(order);
    free(data);
    return sink == UINT64_MAX ? EXIT_FAILURE : EXIT_SUCCESS;
}
