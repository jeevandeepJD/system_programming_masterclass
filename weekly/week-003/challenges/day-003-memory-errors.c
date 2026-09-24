/*
 * Day 3 memory diagnostics fixture.
 *
 * The default and "safe" modes are defined. Invalid behavior runs only when
 * its mode is explicitly selected; use ASan/UBSan or Valgrind.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int safe_mode(void)
{
    int *values = malloc(4U * sizeof *values);

    if (values == NULL) {
        fputs("allocation failed\n", stderr);
        return EXIT_FAILURE;
    }
    for (size_t i = 0U; i < 4U; ++i) {
        values[i] = (int)(i * i);
    }
    printf("safe checksum=%d\n",
           values[0] + values[1] + values[2] + values[3]);
    free(values);
    return EXIT_SUCCESS;
}

static int leak_mode(void)
{
    int *lost = malloc(32U * sizeof *lost);

    if (lost == NULL) {
        return EXIT_FAILURE;
    }
    lost[0] = 17;
    printf("allocated value=%d; intentionally omitting free\n", lost[0]);
    return EXIT_SUCCESS;
}

static int use_after_free_mode(void)
{
    void (*release)(void *) = free;
    int *stale = malloc(sizeof *stale);

    if (stale == NULL) {
        return EXIT_FAILURE;
    }
    *stale = 23;
    release(stale);
    puts("about to read through a pointer whose allocation lifetime ended");
    printf("invalid value=%d\n", *stale);
    return EXIT_SUCCESS;
}

static int double_free_mode(void)
{
    void (*release)(void *) = free;
    int *owner = malloc(sizeof *owner);

    if (owner == NULL) {
        return EXIT_FAILURE;
    }
    *owner = 29;
    release(owner);
    puts("about to free the same allocation a second time");
    release(owner);
    return EXIT_SUCCESS;
}

static void usage(const char *program)
{
    fprintf(stderr, "usage: %s [safe|leak|uaf|double-free]\n", program);
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
    if (strcmp(mode, "leak") == 0) {
        return leak_mode();
    }
    if (strcmp(mode, "uaf") == 0) {
        return use_after_free_mode();
    }
    if (strcmp(mode, "double-free") == 0) {
        return double_free_mode();
    }

    usage(argv[0]);
    return EXIT_FAILURE;
}
