#!/usr/bin/env python3
"""Plot CSV emitted by day-004-memory-wall-lab.c."""

import csv
import sys

try:
    import matplotlib.pyplot as plt
except ImportError:
    raise SystemExit("matplotlib is required: sudo dnf install python3-matplotlib")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} RESULTS.csv")

    series: dict[str, tuple[list[float], list[float]]] = {}
    with open(sys.argv[1], newline="", encoding="utf-8") as source:
        for row in csv.DictReader(source):
            xs, ys = series.setdefault(row["mode"], ([], []))
            xs.append(int(row["bytes"]) / 1024)
            ys.append(float(row["ns_per_access"]))

    if not series:
        raise SystemExit("no benchmark rows found")

    for mode, (xs, ys) in series.items():
        plt.plot(xs, ys, marker="o", label=mode)
    plt.xscale("log", base=2)
    plt.yscale("log")
    plt.xlabel("working-set size (KiB)")
    plt.ylabel("elapsed time per measured access (ns, log scale)")
    plt.title("Memory access cost versus working-set size")
    plt.grid(True, which="both", alpha=0.25)
    plt.legend()
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()
