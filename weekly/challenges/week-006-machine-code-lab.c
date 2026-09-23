/*
 * Week 6 machine-code lab
 *
 * Build both versions with warnings enabled:
 *
 *   gcc   -std=c17 -Wall -Wextra -Wpedantic -O2 -g \
 *     week-006-machine-code-lab.c -o /tmp/week6-gcc
 *   clang -std=c17 -Wall -Wextra -Wpedantic -O2 -g \
 *     week-006-machine-code-lab.c -o /tmp/week6-clang
 *
 * Observe the executable and its machine instructions:
 *
 *   readelf -h /tmp/week6-gcc
 *   readelf -S /tmp/week6-gcc
 *   readelf -sW /tmp/week6-gcc
 *   objdump -d -M intel /tmp/week6-gcc
 *   objdump -d -M intel --disassemble=weighted_sum /tmp/week6-gcc
 *   objdump -d -M intel --disassemble=count_above /tmp/week6-gcc
 *   objdump -s -j .text /tmp/week6-gcc
 *
 * Then rebuild with -O0 and compare. Instruction selection, register choice,
 * and layout may differ by compiler/version/flags even though all outputs
 * obey the same C implementation requirements and target ISA/ABI.
 */

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

/*
 * Arithmetic plus indexed memory loads. Keeping this out of line makes the
 * function easy to locate in objdump output.
 */
NOINLINE int64_t weighted_sum(const int32_t *values, size_t count,
                              int32_t scale, int32_t bias)
{
    int64_t total = 0;

    for (size_t index = 0; index < count; ++index) {
        total += (int64_t)values[index] * scale + bias;
    }
    return total;
}

/*
 * A memory load, comparison, conditional update, and loop back-edge. At -O2,
 * a compiler may implement the source-level "if" with a branch, conditional
 * move, set instruction, vector operation, or another equivalent sequence.
 */
NOINLINE size_t count_above(const int32_t *values, size_t count,
                            int32_t threshold)
{
    size_t matches = 0;

    for (size_t index = 0; index < count; ++index) {
        if (values[index] > threshold) {
            ++matches;
        }
    }
    return matches;
}

/*
 * A switch supplies several control-flow targets. Do not assume it becomes a
 * jump table: that decision belongs to the compiler and depends on the target
 * and optimization choices.
 */
NOINLINE uint32_t choose_operation(uint32_t selector, uint32_t left,
                                   uint32_t right)
{
    switch (selector & UINT32_C(3)) {
    case 0:
        return left + right;
    case 1:
        return left ^ right;
    case 2:
        return (left << 3) | (right >> 2);
    default:
        return left - right;
    }
}

int main(void)
{
    static const int32_t samples[] = {
        12, -7, 31, 5, 18, -2, 44, 9, 23, 0, -11, 27
    };
    const size_t count = sizeof(samples) / sizeof(samples[0]);

    int64_t total = weighted_sum(samples, count, 3, -4);
    size_t matches = count_above(samples, count, 10);
    uint32_t control = choose_operation((uint32_t)matches,
                                        (uint32_t)total,
                                        UINT32_C(0x13579bdf));

    printf("weighted_sum = %" PRId64 "\n", total);
    printf("values above 10 = %zu\n", matches);
    printf("control result = 0x%08" PRIx32 "\n", control);
    return 0;
}
