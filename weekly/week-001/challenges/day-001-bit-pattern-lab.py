#!/usr/bin/env python3
"""Week 1 Day 1 coding challenge: representation without hidden magic.

Complete the TODO functions, then run:

    python3 weekly/week-001/challenges/day-001-bit-pattern-lab.py

For the conversion functions, derive the result with place values and bit
operations rather than calling bin(value) or int(bits, 2).
"""


def bits_needed(state_count: int) -> int:
    """Return the minimum bits needed to distinguish state_count states."""
    # TODO: repeatedly double the number of available patterns.
    raise NotImplementedError


def encode_unsigned(value: int, width: int) -> str:
    """Encode value as exactly width binary digits."""
    # TODO: inspect powers of two from most-significant to least-significant.
    raise NotImplementedError


def decode_unsigned(bits: str) -> int:
    """Decode a binary string using positional place values."""
    # TODO: accumulate one bit at a time; reject characters other than 0/1.
    raise NotImplementedError


def pixel_row(bits: str) -> str:
    """Interpret 0 as '.' and 1 as '#' without changing the bit pattern."""
    # TODO: this is a different interpretation, not a numeric conversion.
    raise NotImplementedError


def check(label: str, actual, expected) -> int:
    if actual == expected:
        print(f"PASS  {label}: {actual!r}")
        return 0
    print(f"FAIL  {label}: got {actual!r}, expected {expected!r}")
    return 1


def attempt(label: str, function, expected) -> int:
    try:
        return check(label, function(), expected)
    except NotImplementedError:
        print(f"TODO  {label}")
        return 1
    except (TypeError, ValueError) as error:
        print(f"FAIL  {label}: unexpected {type(error).__name__}: {error}")
        return 1


def main() -> int:
    failures = 0
    failures += attempt("20 states need 5 bits", lambda: bits_needed(20), 5)
    failures += attempt("1 state needs 0 bits", lambda: bits_needed(1), 0)
    failures += attempt(
        "70 encoded in 8 bits", lambda: encode_unsigned(70, 8), "01000110"
    )
    failures += attempt(
        "01000110 decoded", lambda: decode_unsigned("01000110"), 70
    )
    failures += attempt(
        "same pattern as pixels",
        lambda: pixel_row("01000110"),
        ".#...##.",
    )

    print()
    if failures:
        print(f"{failures} check(s) remain. Draw the place values before editing.")
        return 1
    print("All checks pass. Explain why 01000110 still has no inherent meaning.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
