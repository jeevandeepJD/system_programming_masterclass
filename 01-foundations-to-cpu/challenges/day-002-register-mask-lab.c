/*
 * Day 2 challenge A — implement bit and field operations.
 *
 * Build:
 *   gcc -std=c17 -Wall -Wextra -Wconversion -O0 -g \
 *     day-002-register-mask-lab.c -o /tmp/register-mask-lab
 *
 * Do not change the tests. Replace only the TODO function bodies.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

enum {
    START_BIT = 0,
    IRQ_EN_BIT = 1,
    MODE_SHIFT = 3,
    ERROR_BIT = 6,
    READY_BIT = 7,
};

#define MODE_MASK UINT8_C(0x38) /* bits 5:3 */

static uint8_t set_bit(uint8_t value, unsigned bit)
{
    /* TODO: set only the requested bit. */
    (void)bit;
    return value;
}

static uint8_t clear_bit(uint8_t value, unsigned bit)
{
    /* TODO: clear only the requested bit. */
    (void)bit;
    return value;
}

static uint8_t toggle_bit(uint8_t value, unsigned bit)
{
    /* TODO: invert only the requested bit. */
    (void)bit;
    return value;
}

static bool test_bit(uint8_t value, unsigned bit)
{
    /* TODO: return true exactly when the requested bit is set. */
    (void)value;
    (void)bit;
    return false;
}

static uint8_t get_mode(uint8_t reg)
{
    /* TODO: extract bits 5:3 as a value in the range 0..7. */
    (void)reg;
    return 0;
}

static uint8_t replace_mode(uint8_t reg, uint8_t mode)
{
    /*
     * TODO:
     *   1. preserve READY, ERROR, IRQ_EN, START, and reserved bit 2;
     *   2. clear the old MODE field;
     *   3. place only the low three bits of mode into bits 5:3.
     */
    (void)mode;
    return reg;
}

static unsigned failures;

static void expect_u8(const char *name, uint8_t actual, uint8_t expected)
{
    if (actual == expected) {
        printf("PASS  %-24s 0x%02x\n", name, actual);
        return;
    }

    printf("FAIL  %-24s got 0x%02x, expected 0x%02x\n",
           name, actual, expected);
    ++failures;
}

static void expect_bool(const char *name, bool actual, bool expected)
{
    if (actual == expected) {
        printf("PASS  %-24s %s\n", name, actual ? "true" : "false");
        return;
    }

    printf("FAIL  %-24s got %s, expected %s\n",
           name,
           actual ? "true" : "false",
           expected ? "true" : "false");
    ++failures;
}

int main(void)
{
    const uint8_t initial = UINT8_C(0xad); /* 1010 1101 */

    puts("Day 2 register-mask lab");
    puts("==========================");
    printf("initial register: 0x%02x (10101101)\n\n", initial);

    expect_u8("set IRQ_EN", set_bit(initial, IRQ_EN_BIT), UINT8_C(0xaf));
    expect_u8("clear START", clear_bit(initial, START_BIT), UINT8_C(0xac));
    expect_u8("toggle ERROR", toggle_bit(initial, ERROR_BIT), UINT8_C(0xed));
    expect_bool("READY is set", test_bit(initial, READY_BIT), true);
    expect_bool("ERROR is clear", test_bit(initial, ERROR_BIT), false);
    expect_u8("extract MODE", get_mode(initial), UINT8_C(5));
    expect_u8("replace MODE with 2",
              replace_mode(initial, UINT8_C(2)),
              UINT8_C(0x95));
    expect_u8("oversized MODE is masked",
              replace_mode(initial, UINT8_C(0x0f)),
              UINT8_C(0xbd));

    if (failures == 0) {
        puts("\nAll tests pass. Now explain every mask on paper.");
        return 0;
    }

    printf("\n%u test(s) still failing. Draw the operation before editing.\n",
           failures);
    return 1;
}
