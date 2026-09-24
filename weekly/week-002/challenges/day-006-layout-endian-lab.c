/*
 * Day 6: observe C layout and byte order without undefined behavior.
 * memcpy is used for byte-wise reconstruction; no misaligned typed pointer
 * is ever dereferenced.
 */
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct mixed {
    char marker;
    uint32_t count;
    uint16_t code;
    double value;
};

struct reordered {
    double value;
    uint32_t count;
    uint16_t code;
    char marker;
};

static void print_bytes(const void *object, size_t size)
{
    const unsigned char *bytes = object;
    for (size_t i = 0; i < size; ++i)
        printf("%s%02x", i ? " " : "", bytes[i]);
    putchar('\n');
}

int main(void)
{
    struct mixed a = {'A', UINT32_C(0x11223344), UINT16_C(0x5566), 1.5};
    struct reordered b = {1.5, UINT32_C(0x11223344), UINT16_C(0x5566), 'A'};

    puts("struct mixed");
    printf("  sizeof=%zu alignof=%zu\n", sizeof a, _Alignof(struct mixed));
    printf("  offsets marker=%zu count=%zu code=%zu value=%zu\n",
           offsetof(struct mixed, marker), offsetof(struct mixed, count),
           offsetof(struct mixed, code), offsetof(struct mixed, value));
    print_bytes(&a, sizeof a);

    puts("struct reordered");
    printf("  sizeof=%zu alignof=%zu\n", sizeof b, _Alignof(struct reordered));
    printf("  offsets value=%zu count=%zu code=%zu marker=%zu\n",
           offsetof(struct reordered, value), offsetof(struct reordered, count),
           offsetof(struct reordered, code), offsetof(struct reordered, marker));
    print_bytes(&b, sizeof b);

    uint32_t word = UINT32_C(0x01020304);
    unsigned char representation[sizeof word];
    memcpy(representation, &word, sizeof word);
    printf("uint32_t 0x%08" PRIx32 " in memory: ", word);
    print_bytes(representation, sizeof representation);
    if (representation[0] == 0x04)
        puts("observed little-endian byte order");
    else if (representation[0] == 0x01)
        puts("observed big-endian byte order");
    else
        puts("observed a byte order outside this lab's two common cases");

    unsigned char wire[4] = {0x12, 0x34, 0x56, 0x78};
    uint32_t decoded = ((uint32_t)wire[0] << 24) |
                       ((uint32_t)wire[1] << 16) |
                       ((uint32_t)wire[2] << 8) |
                       (uint32_t)wire[3];
    printf("network-order bytes decode explicitly to 0x%08" PRIx32 "\n",
           decoded);

    unsigned char unaligned[sizeof word + 1];
    memcpy(unaligned + 1, &word, sizeof word);
    uint32_t safe_copy;
    memcpy(&safe_copy, unaligned + 1, sizeof safe_copy);
    printf("misaligned byte address copied safely with memcpy: 0x%08" PRIx32
           "\n", safe_copy);
    puts("Do not replace memcpy with *(uint32_t *)(unaligned + 1): that can");
    puts("violate alignment and effective-type rules even where the CPU tolerates it.");
    return 0;
}
