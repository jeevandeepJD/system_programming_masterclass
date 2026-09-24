/*
 * Day 3 arena lab: a small, defined bump allocator.
 *
 * Build:
 *   cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -g \
 *      day-003-arena-lab.c -o /tmp/week3-day3-arena
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct arena {
    unsigned char *base;
    size_t capacity;
    size_t used;
};

static int arena_init(struct arena *arena, size_t capacity)
{
    arena->base = malloc(capacity);
    arena->capacity = arena->base != NULL ? capacity : 0U;
    arena->used = 0U;
    return arena->base != NULL;
}

static void arena_destroy(struct arena *arena)
{
    free(arena->base);
    arena->base = NULL;
    arena->capacity = 0U;
    arena->used = 0U;
}

static void arena_reset(struct arena *arena)
{
    arena->used = 0U;
}

static void *arena_alloc(struct arena *arena, size_t size, size_t alignment)
{
    size_t remainder;
    size_t padding;
    size_t start;

    if (size == 0U || alignment == 0U ||
        alignment > _Alignof(max_align_t) ||
        (alignment & (alignment - 1U)) != 0U) {
        return NULL;
    }

    remainder = arena->used & (alignment - 1U);
    padding = remainder == 0U ? 0U : alignment - remainder;
    if (padding > arena->capacity - arena->used) {
        return NULL;
    }

    start = arena->used + padding;
    if (size > arena->capacity - start) {
        return NULL;
    }

    arena->used = start + size;
    return arena->base + start;
}

static size_t offset_of(const struct arena *arena, const void *pointer)
{
    return (size_t)((const unsigned char *)pointer - arena->base);
}

int main(void)
{
    struct arena arena;
    uint32_t *number;
    double *values;
    unsigned char *bytes;
    void *failed;
    size_t before_failure;

    if (!arena_init(&arena, 64U)) {
        fputs("arena allocation failed\n", stderr);
        return EXIT_FAILURE;
    }

    number = arena_alloc(&arena, sizeof *number, _Alignof(uint32_t));
    values = arena_alloc(&arena, 3U * sizeof *values, _Alignof(double));
    bytes = arena_alloc(&arena, 7U, _Alignof(max_align_t));
    if (number == NULL || values == NULL || bytes == NULL) {
        fputs("unexpected arena exhaustion\n", stderr);
        arena_destroy(&arena);
        return EXIT_FAILURE;
    }

    *number = UINT32_C(42);
    values[0] = 1.25;
    values[1] = 2.50;
    values[2] = 5.00;
    for (size_t i = 0U; i < 7U; ++i) {
        bytes[i] = (unsigned char)(i + 1U);
    }

    printf("uint32 offset=%zu value=%u\n", offset_of(&arena, number),
           (unsigned int)*number);
    printf("double[3] offset=%zu values=%.2f,%.2f,%.2f\n",
           offset_of(&arena, values), values[0], values[1], values[2]);
    printf("bytes offset=%zu last=%u used=%zu/%zu\n",
           offset_of(&arena, bytes), (unsigned int)bytes[6], arena.used,
           arena.capacity);

    before_failure = arena.used;
    failed = arena_alloc(&arena, 128U, _Alignof(max_align_t));
    printf("oversized allocation=%s state_unchanged=%s\n",
           failed == NULL ? "rejected" : "accepted",
           arena.used == before_failure ? "yes" : "no");

    arena_reset(&arena);
    printf("after reset used=%zu (all earlier arena pointers are retired)\n",
           arena.used);
    arena_destroy(&arena);
    return EXIT_SUCCESS;
}
