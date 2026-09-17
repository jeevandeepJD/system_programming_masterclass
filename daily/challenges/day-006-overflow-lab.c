/*
 * Day 6 challenge — predict every line before running.
 *
 * Normal build:
 *   gcc -std=c17 -Wall -Wextra -O0 -g \
 *     day-006-overflow-lab.c -o /tmp/overflow-lab
 *
 * Instrumented build:
 *   gcc -std=c17 -Wall -Wextra -O1 -g \
 *     -fsanitize=undefined -fno-sanitize-recover=undefined \
 *     day-006-overflow-lab.c -o /tmp/overflow-lab-ubsan
 *
 * The final section intentionally performs signed overflow so UBSan can
 * detect it. Its ordinary-build output must not be treated as portable C.
 */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    puts("Day 6 overflow and extension lab");
    puts("================================\n");

    uint8_t u8_max = UINT8_MAX;
    uint8_t u8_wrapped = (uint8_t)(u8_max + UINT8_C(1));

    printf("1. uint8_t stored result:  %3u + 1 -> %3u\n",
           (unsigned)u8_max, (unsigned)u8_wrapped);
    printf("2. promoted expression:    %3u + 1 -> %3d\n",
           (unsigned)u8_max, u8_max + UINT8_C(1));

    uint8_t raw = UINT8_C(0xf6);
    uint32_t zero_extended = raw;
    int32_t sign_extended = (int8_t)raw;

    printf("3. raw pattern:             0x%02x\n", (unsigned)raw);
    printf("4. zero-extended to 32-bit: 0x%08x = %u\n",
           zero_extended, zero_extended);
    printf("5. sign-extended to 32-bit: 0x%08x = %d\n",
           (uint32_t)sign_extended, sign_extended);

    int checked_result = 0;
    bool overflowed = __builtin_add_overflow(INT_MAX, 1, &checked_result);
    printf("6. checked INT_MAX + 1:     overflow=%s, low bits=0x%x\n",
           overflowed ? "true" : "false",
           (unsigned)checked_result);

    puts("\n7. The next expression intentionally violates the C signed-range rule.");
    puts("   A UBSan build should stop here with a runtime error.");

    volatile int maximum = INT_MAX;
    volatile int one = 1;
    int undefined_result = maximum + one; /* intentional signed overflow */

    printf("   ordinary build happened to produce: %d\n", undefined_result);
    puts("   This observed value is not a portable language guarantee.");

    return 0;
}
