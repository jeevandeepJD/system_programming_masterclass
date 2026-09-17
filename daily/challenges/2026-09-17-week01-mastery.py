#!/usr/bin/env python3
"""Week 1 mastery challenge: conversions plus prediction-based reasoning."""

from __future__ import annotations

import random


def normalized(value: str) -> str:
    return value.strip().lower().replace("_", "").replace(" ", "")


def ask(prompt: str, expected: set[str], explanation: str) -> bool:
    answer = normalized(input(f"\n{prompt}\n> "))
    if answer in expected:
        print(f"✓ Correct. {explanation}")
        return True
    print("✗ Not yet.")
    print(f"  Explanation: {explanation}")
    return False


def main() -> None:
    print("Week 1 mastery challenge")
    print("========================")
    print("Predict first. Do not use a calculator.\n")

    score = 0
    value = random.choice([19, 37, 70, 90, 165, 182, 225])
    binary = f"{value:08b}"
    hexadecimal = f"{value:02x}"

    score += ask(
        f"Write decimal {value} as exactly eight binary digits.",
        {binary, f"0b{binary}"},
        f"{value} = {binary}₂.",
    )
    score += ask(
        f"Write binary {binary} in hexadecimal.",
        {hexadecimal, f"0x{hexadecimal}"},
        f"Group it as {binary[:4]} {binary[4:]}; the nibbles map to 0x{hexadecimal.upper()}.",
    )
    score += ask(
        "How many distinct patterns can five independent bits produce?",
        {"32", "2^5", "2**5"},
        "Each new independent bit doubles the previous set, so 2⁵ = 32.",
    )
    score += ask(
        "With guaranteed LOW ≤ 0.8 V and HIGH ≥ 2.0 V, classify 1.4 V "
        "as low, high, or unspecified.",
        {"unspecified", "unknown", "invalid", "forbidden", "indeterminate"},
        "1.4 V lies between the guaranteed ranges, so the specification promises no logical value.",
    )
    score += ask(
        "The byte 01000110 appears as F in a terminal. Did the stored bits "
        "change? Answer yes or no.",
        {"no", "n"},
        "The bits stayed fixed; the terminal applied a character encoding.",
    )

    print(f"\nObjective score: {score}/5")
    print("\nNow complete the part a script cannot grade:")
    print("1. Draw: physical state → logical bit → pattern → interpretation → meaning.")
    print("2. Explain aloud why binary is physically practical.")
    print("3. Explain how 01000110 can be 70, 'F', an instruction byte, or pixels.")
    print("4. Record any weak arrow or uncertain phrase in your Why? notebook.")
    print("\nDo not mark Week 1 complete based only on the numeric score.")


if __name__ == "__main__":
    main()
