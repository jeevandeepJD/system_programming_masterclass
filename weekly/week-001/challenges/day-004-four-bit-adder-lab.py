#!/usr/bin/env python3
"""Week 1 Day 4 coding challenge: build addition from one-bit stages.

Complete the TODO functions, then run:

    python3 weekly/week-001/challenges/day-004-four-bit-adder-lab.py

Do not use Python's + operator inside half_adder, full_adder, or add4.
"""


def half_adder(a: int, b: int) -> tuple[int, int]:
    """Return (sum_bit, carry_bit)."""
    # TODO: derive from XOR and AND.
    raise NotImplementedError


def full_adder(a: int, b: int, carry_in: int) -> tuple[int, int]:
    """Return (sum_bit, carry_out)."""
    # TODO: combine two half adders and an OR operation.
    raise NotImplementedError


def add4(a: int, b: int) -> tuple[int, int, list[int]]:
    """Return (low_four_bits, carry_out, carry_trace) for values 0..15."""
    # TODO:
    # 1. process bits from least-significant to most-significant;
    # 2. ripple each carry into the next full-adder;
    # 3. collect result bits and carries.
    raise NotImplementedError


def check(label: str, function, expected) -> int:
    try:
        actual = function()
    except NotImplementedError:
        print(f"TODO  {label}")
        return 1
    if actual == expected:
        print(f"PASS  {label}: {actual}")
        return 0
    print(f"FAIL  {label}: got {actual}, expected {expected}")
    return 1


def main() -> int:
    failures = 0
    failures += check("half 0+0", lambda: half_adder(0, 0), (0, 0))
    failures += check("half 1+1", lambda: half_adder(1, 1), (0, 1))
    failures += check("full 1+1+1", lambda: full_adder(1, 1, 1), (1, 1))
    failures += check(
        "3+5", lambda: add4(3, 5), (8, 0, [0, 1, 1, 1, 0])
    )
    failures += check(
        "15+1", lambda: add4(15, 1), (0, 1, [0, 1, 1, 1, 1])
    )

    print()
    if failures:
        print(f"{failures} check(s) remain. Trace the carry chain on paper.")
        return 1
    print("All checks pass. Explain why the carry path limits ripple-adder speed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
