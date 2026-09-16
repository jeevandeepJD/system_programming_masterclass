#!/usr/bin/env python3
"""Week 1 self-check: verify your by-hand decimal/binary/hex conversions.

Do the conversions on paper FIRST. Only run this afterward to check
yourself -- the value of this exercise is in doing the conversion
algorithm manually, not in reading the answer.
"""

VALUES = [13, 47, 128, 255, 9, 200, 64, 1, 17, 99,
          250, 6, 33, 111, 222, 88, 150, 3, 77, 240,
          500, 1024, 4095, 0, 170]

def min_bits(n: int) -> int:
    """Minimum bits needed to represent n in unsigned binary (at least 8)."""
    return max(8, n.bit_length())

print(f"{'decimal':>8} | {'binary':>16} | {'hex':>6}")
print("-" * 38)
for v in VALUES:
    width = min_bits(v)
    b = format(v, f'0{width}b')
    h = format(v, '#04x')
    print(f"{v:>8} | {b:>16} | {h:>6}")

print()
print("Bonus answers:")
print(f"  0xFF   = {0xFF} decimal")
print(f"  0b1111 = {0b1111} decimal")
