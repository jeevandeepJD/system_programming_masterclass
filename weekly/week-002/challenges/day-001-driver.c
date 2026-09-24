/*
 * Build command is in the Day 1 lesson. Predict every line before running.
 */
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

long asm_sum_to(long n);
long asm_scale_add(long value, long scale, long bias);
size_t asm_count_byte(const unsigned char *data, size_t count,
                      unsigned char needle);

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

NOINLINE long c_sum_to(long n)
{
    long sum = 0;
    for (long i = 0; i < n; ++i) {
        sum += i;
    }
    return sum;
}

int main(void)
{
    static const unsigned char bytes[] = {
        0x2a, 0x10, 0x2a, 0xff, 0x00, 0x2a, 0x2b, 0x2a
    };
    const size_t count = sizeof(bytes) / sizeof(bytes[0]);

    printf("asm_sum_to(4) = %ld\n", asm_sum_to(4));
    printf("c_sum_to(4) = %ld\n", c_sum_to(4));
    printf("asm_scale_add(10, 3, -2) = %ld\n",
           asm_scale_add(10, 3, -2));
    printf("asm_count_byte(..., 0x2a) = %zu\n",
           asm_count_byte(bytes, count, UINT8_C(0x2a)));
    return 0;
}
