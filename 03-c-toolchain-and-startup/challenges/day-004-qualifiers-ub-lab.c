/*
 * Day 4 qualifiers and undefined-behavior lab.
 *
 * Safe/default mode is defined and warning-clean. The overflow,
 * restrict-overlap, and lifetime modes intentionally violate C rules and
 * must be selected explicitly under sanitizers.
 */
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int sum_plain(const int *values, size_t count)
{
    int sum = 0;

    for (size_t i = 0U; i < count; ++i) {
        sum += values[i];
    }
    return sum;
}

static void add_restrict(size_t count, int *restrict output,
                         const int *restrict left,
                         const int *restrict right)
{
    for (size_t i = 0U; i < count; ++i) {
        output[i] = left[i] + right[i];
    }
}

static int safe_mode(void)
{
    const int left[] = {1, 2, 3, 4};
    const int right[] = {10, 20, 30, 40};
    int output[4] = {0, 0, 0, 0};
    volatile unsigned int observed_register = 0U;

    add_restrict(4U, output, left, right);
    observed_register = 7U;

    printf("const input sum=%d\n", sum_plain(left, 4U));
    printf("restrict contract honored; output=%d,%d,%d,%d\n", output[0],
           output[1], output[2], output[3]);
    printf("volatile access observed value=%u\n", observed_register);
    puts("volatile forced an abstract-machine access; it did not synchronize threads");
    return EXIT_SUCCESS;
}

static int overflow_mode(void)
{
    volatile int maximum = INT_MAX;
    int result = maximum + 1;

    printf("invalid signed-overflow result=%d\n", result);
    return EXIT_SUCCESS;
}

static int restrict_overlap_mode(void)
{
    int values[] = {1, 2, 3, 4};
    const int right[] = {10, 20, 30, 40};
    volatile int select_overlap = 1;
    int *alias = select_overlap ? values : NULL;

    puts("calling a restrict-qualified function with overlapping access paths");
    add_restrict(4U, values, alias, right);
    printf("result after contract violation=%d,%d,%d,%d\n", values[0],
           values[1], values[2], values[3]);
    return EXIT_SUCCESS;
}

static int lifetime_mode(void)
{
    void (*release)(void *) = free;
    int *stale = malloc(sizeof *stale);

    if (stale == NULL) {
        return EXIT_FAILURE;
    }
    *stale = 41;
    release(stale);
    puts("reading after allocated-object lifetime ended");
    printf("invalid value=%d\n", *stale);
    return EXIT_SUCCESS;
}

static void usage(const char *program)
{
    fprintf(stderr,
            "usage: %s [safe|overflow|restrict-overlap|lifetime]\n",
            program);
}

int main(int argc, char **argv)
{
    const char *mode = argc == 1 ? "safe" : argv[1];

    if (argc > 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (strcmp(mode, "safe") == 0) {
        return safe_mode();
    }
    if (strcmp(mode, "overflow") == 0) {
        return overflow_mode();
    }
    if (strcmp(mode, "restrict-overlap") == 0) {
        return restrict_overlap_mode();
    }
    if (strcmp(mode, "lifetime") == 0) {
        return lifetime_mode();
    }

    usage(argv[0]);
    return EXIT_FAILURE;
}
