/*
 * Day 7: defined experiments for types, conversions, representation, and
 * compiler output. Build at -O0 and -O2, run, then inspect assembly.
 */
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int mixed_compare(int signed_value, unsigned int unsigned_value)
{
    return signed_value < unsigned_value;
}

static uint32_t wrap_add(uint32_t value)
{
    return value + UINT32_C(1);
}

static int promoted_sum(unsigned char a, unsigned char b)
{
    return a + b;
}

static void show_object_bytes(const char *name, const void *object, size_t size)
{
    const unsigned char *bytes = object;
    printf("%-12s", name);
    for (size_t i = 0; i < size; ++i)
        printf(" %02x", bytes[i]);
    putchar('\n');
}

int main(void)
{
    printf("CHAR_BIT=%d\n", CHAR_BIT);
    printf("sizeof char/short/int/long/long long = %zu/%zu/%zu/%zu/%zu\n",
           sizeof(char), sizeof(short), sizeof(int), sizeof(long),
           sizeof(long long));
    printf("INT_MIN=%d INT_MAX=%d UINT_MAX=%u\n", INT_MIN, INT_MAX, UINT_MAX);
    printf("float/double: radix=%d FLT_MANT_DIG=%d DBL_MANT_DIG=%d\n",
           FLT_RADIX, FLT_MANT_DIG, DBL_MANT_DIG);

    int negative = -1;
    unsigned int one = 1;
    printf("-1 < 1u is %s (usual arithmetic conversions apply)\n",
           mixed_compare(negative, one) ? "true" : "false");
    printf("UINT32_MAX + 1 = %" PRIu32 " (defined modulo 2^32)\n",
           wrap_add(UINT32_MAX));
    printf("(unsigned char)250 + (unsigned char)10 = %d (promoted first)\n",
           promoted_sum(250, 10));

    uint32_t integer = UINT32_C(0x11223344);
    float real = 1.0f;
    show_object_bytes("uint32_t", &integer, sizeof integer);
    show_object_bytes("float 1.0", &real, sizeof real);

    uint32_t float_bits = 0;
    _Static_assert(sizeof float_bits == sizeof real,
                   "this lab expects 32-bit float and uint32_t");
    memcpy(&float_bits, &real, sizeof float_bits);
    printf("float bytes copied into uint32_t: 0x%08" PRIx32 "\n", float_bits);

    puts("Now compare compiler output for mixed_compare, wrap_add, and");
    puts("promoted_sum at -O0 and -O2. C specifies results, not one instruction list.");
    return 0;
}
