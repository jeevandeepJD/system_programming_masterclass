#!/usr/bin/env python3
"""Week 1 Day 3 coding challenge: derive gates and universal NAND logic.

Complete the TODO functions, then run:

    python3 weekly/week-001/challenges/day-003-boolean-functions-lab.py

Inputs are restricted to integer bits 0 and 1.
"""


def gate_not(a: int) -> int:
    # TODO
    raise NotImplementedError


def gate_and(a: int, b: int) -> int:
    # TODO
    raise NotImplementedError


def gate_or(a: int, b: int) -> int:
    # TODO
    raise NotImplementedError


def gate_xor(a: int, b: int) -> int:
    # TODO: derive from NOT, AND, and OR.
    raise NotImplementedError


def gate_nand(a: int, b: int) -> int:
    # TODO
    raise NotImplementedError


def nand_not(a: int) -> int:
    # TODO: use gate_nand only.
    raise NotImplementedError


def nand_and(a: int, b: int) -> int:
    # TODO: use gate_nand only.
    raise NotImplementedError


def nand_or(a: int, b: int) -> int:
    # TODO: use gate_nand only and apply De Morgan's law.
    raise NotImplementedError


def expected_rows(function_name: str) -> tuple[int, int, int, int]:
    rows = {
        "and": (0, 0, 0, 1),
        "or": (0, 1, 1, 1),
        "xor": (0, 1, 1, 0),
        "nand": (1, 1, 1, 0),
    }
    return rows[function_name]


def truth_table(function) -> tuple[int, int, int, int]:
    return tuple(function(a, b) for a, b in ((0, 0), (0, 1), (1, 0), (1, 1)))


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
    failures += check(
        "NOT", lambda: (gate_not(0), gate_not(1)), (1, 0)
    )
    failures += check(
        "AND", lambda: truth_table(gate_and), expected_rows("and")
    )
    failures += check("OR", lambda: truth_table(gate_or), expected_rows("or"))
    failures += check(
        "XOR", lambda: truth_table(gate_xor), expected_rows("xor")
    )
    failures += check(
        "NAND", lambda: truth_table(gate_nand), expected_rows("nand")
    )
    failures += check(
        "NOT from NAND", lambda: (nand_not(0), nand_not(1)), (1, 0)
    )
    failures += check(
        "AND from NAND", lambda: truth_table(nand_and), expected_rows("and")
    )
    failures += check(
        "OR from NAND", lambda: truth_table(nand_or), expected_rows("or")
    )

    print()
    if failures:
        print(f"{failures} check(s) remain. Derive each truth-table row first.")
        return 1
    print("All checks pass. Explain why matching every row proves equivalence.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
