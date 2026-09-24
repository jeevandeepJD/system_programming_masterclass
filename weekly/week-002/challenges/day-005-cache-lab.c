/*
 * Day 5 cache lab: safe userspace experiments for mapping, locality, and
 * false sharing.  No result is a hardware guarantee; repeat measurements.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { DEFAULT_MIB = 64, DEFAULT_ROUNDS = 8 };

struct compact_counters {
    _Atomic uint64_t a;
    _Atomic uint64_t b;
};

struct padded_counter {
    _Alignas(64) _Atomic uint64_t value;
    unsigned char padding[64 - sizeof(_Atomic uint64_t)];
};

struct padded_counters {
    struct padded_counter a;
    struct padded_counter b;
};

struct worker {
    _Atomic uint64_t *counter;
    uint64_t iterations;
};

static double seconds_now(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static void *increment(void *argument)
{
    struct worker *work = argument;
    for (uint64_t i = 0; i < work->iterations; ++i)
        atomic_fetch_add_explicit(work->counter, 1, memory_order_relaxed);
    return NULL;
}

static void run_pair(const char *label, _Atomic uint64_t *a,
                     _Atomic uint64_t *b, uint64_t iterations)
{
    pthread_t first, second;
    struct worker work[2] = {{a, iterations}, {b, iterations}};
    atomic_store(a, 0);
    atomic_store(b, 0);
    double start = seconds_now();
    if (pthread_create(&first, NULL, increment, &work[0]) != 0 ||
        pthread_create(&second, NULL, increment, &work[1]) != 0) {
        fputs("pthread_create failed\n", stderr);
        exit(EXIT_FAILURE);
    }
    if (pthread_join(first, NULL) != 0 || pthread_join(second, NULL) != 0) {
        fputs("pthread_join failed\n", stderr);
        exit(EXIT_FAILURE);
    }
    double elapsed = seconds_now() - start;
    printf("%-9s  offsets=%td bytes  total=%" PRIu64 "  %.6f s\n",
           label, (unsigned char *)b - (unsigned char *)a,
           atomic_load(a) + atomic_load(b), elapsed);
}

static int false_sharing(uint64_t iterations)
{
    struct compact_counters compact = {0};
    struct padded_counters padded = {0};
    puts("Prediction: which pair should cause more cache-line ownership traffic?");
    run_pair("compact", &compact.a, &compact.b, iterations);
    run_pair("padded", &padded.a.value, &padded.b.value, iterations);
    puts("Repeat this test. Scheduling, vCPUs, frequency, and VM topology can");
    puts("hide or reverse small timing differences; timings alone prove nothing.");
    return 0;
}

static int locality(size_t mib, unsigned rounds)
{
    if (mib == 0 || mib > SIZE_MAX / (1024u * 1024u)) {
        fputs("invalid MiB value\n", stderr);
        return 2;
    }
    size_t bytes = mib * 1024u * 1024u;
    unsigned char *data = aligned_alloc(64, (bytes + 63u) & ~(size_t)63u);
    if (data == NULL) {
        fprintf(stderr, "allocation failed: %s\n", strerror(errno));
        return 1;
    }
    memset(data, 1, bytes);
    volatile uint64_t sum = 0;
    const size_t strides[] = {1, 16, 64, 256, 4096};
    printf("buffer=%zu MiB, rounds=%u\n", mib, rounds);
    for (size_t s = 0; s < sizeof(strides) / sizeof(strides[0]); ++s) {
        size_t stride = strides[s];
        double start = seconds_now();
        size_t visits = 0;
        for (unsigned r = 0; r < rounds; ++r)
            for (size_t i = 0; i < bytes; i += stride) {
                sum += data[i];
                ++visits;
            }
        double elapsed = seconds_now() - start;
        printf("stride=%4zu bytes  visits=%10zu  %8.3f ns/visit\n",
               stride, visits, elapsed * 1e9 / (double)visits);
    }
    printf("checksum=%" PRIu64 "\n", sum);
    free(data);
    return 0;
}

static int mapping(int argc, char **argv)
{
    if (argc != 6) {
        fprintf(stderr,
                "usage: %s map ADDRESS LINE_SIZE SETS WAYS\n", argv[0]);
        return 2;
    }
    char *end = NULL;
    uint64_t address = strtoull(argv[2], &end, 0);
    if (*argv[2] == '\0' || *end != '\0') {
        fputs("ADDRESS must be an integer (hex accepted)\n", stderr);
        return 2;
    }
    uint64_t line = strtoull(argv[3], &end, 0);
    if (*end != '\0' || line == 0) return 2;
    uint64_t sets = strtoull(argv[4], &end, 0);
    if (*end != '\0' || sets == 0) return 2;
    uint64_t ways = strtoull(argv[5], &end, 0);
    if (*end != '\0' || ways == 0) return 2;
    uint64_t offset = address % line;
    uint64_t line_number = address / line;
    uint64_t set = line_number % sets;
    uint64_t tag = line_number / sets;
    printf("address=%#" PRIx64 " line=%" PRIu64 " sets=%" PRIu64
           " ways=%" PRIu64 "\n", address, line, sets, ways);
    printf("tag=%#" PRIx64 " set=%" PRIu64 " offset=%" PRIu64 "\n",
           tag, set, offset);
    puts("Note: ways affect placement choices/replacement, not these field values.");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s map ... | locality [MiB] [rounds] | false-sharing [iterations]\n",
                argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "map") == 0) return mapping(argc, argv);
    if (strcmp(argv[1], "locality") == 0) {
        size_t mib = argc > 2 ? (size_t)strtoull(argv[2], NULL, 0) : DEFAULT_MIB;
        unsigned rounds = argc > 3 ? (unsigned)strtoul(argv[3], NULL, 0) : DEFAULT_ROUNDS;
        return locality(mib, rounds);
    }
    if (strcmp(argv[1], "false-sharing") == 0) {
        uint64_t n = argc > 2 ? strtoull(argv[2], NULL, 0) : 20000000u;
        return false_sharing(n);
    }
    fputs("unknown mode\n", stderr);
    return 2;
}
