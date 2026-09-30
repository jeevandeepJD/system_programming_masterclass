#!/usr/bin/env python3
"""Run and summarize the Day 3 static-CMOS ngspice experiment.

From the repository root:

    python3 04-cpu-and-chip-design/challenges/day-003-cmos-networks.py

Only Python's standard library is required. The script checks for ngspice,
runs it in batch mode in a temporary directory, and reports settled truth
values plus the inverter's analog transition region.
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path
import shutil
import subprocess
import tempfile


VDD = 1.8
SAMPLE_TIMES_NS = (1.0, 3.0, 5.0, 7.0)


def read_wrdata(path: Path) -> tuple[list[str], list[list[float]]]:
    """Read ngspice wrdata output with one shared scale column."""
    lines = [line.split() for line in path.read_text(encoding="utf-8").splitlines()]
    if len(lines) < 2:
        raise RuntimeError(f"{path.name}: no simulation data")
    header = lines[0]
    try:
        rows = [[float(item) for item in line] for line in lines[1:] if line]
    except ValueError as error:
        raise RuntimeError(f"{path.name}: unexpected ngspice output") from error
    return header, rows


def nearest(rows: list[list[float]], target: float) -> list[float]:
    return min(rows, key=lambda row: abs(row[0] - target))


def logic(value: float) -> int | None:
    if value <= 0.3 * VDD:
        return 0
    if value >= 0.7 * VDD:
        return 1
    return None


def crossing(rows: list[list[float]], column: int, level: float) -> float:
    return min(rows, key=lambda row: abs(row[column] - level))[0]


def summarize(work: Path) -> None:
    _, transient = read_wrdata(work / "day-003-truth-waveforms.dat")
    _, transfer = read_wrdata(work / "day-003-inverter-transfer.dat")

    expected = {
        (0, 0): (1, 1),
        (1, 0): (1, 0),
        (0, 1): (1, 0),
        (1, 1): (0, 0),
    }
    failures = 0
    print("Settled static-CMOS observations")
    print(" A B | NAND NOR | analog outputs (V)")
    print("-----+----------+-------------------")
    for time_ns in SAMPLE_TIMES_NS:
        row = nearest(transient, time_ns * 1e-9)
        a, b = logic(row[1]), logic(row[2])
        nand, nor = logic(row[3]), logic(row[4])
        valid_inputs = isinstance(a, int) and isinstance(b, int)
        passed = valid_inputs and (nand, nor) == expected[(a, b)]
        failures += not passed
        print(
            f" {a} {b} |   {nand}    {nor} |"
            f" NAND={row[3]:.4f}, NOR={row[4]:.4f}"
            f"  {'PASS' if passed else 'CHECK'}"
        )

    switching_input = crossing(transfer, 2, VDD / 2)
    below = nearest(transfer, switching_input - 0.01)
    above = nearest(transfer, switching_input + 0.01)
    gain = (above[2] - below[2]) / (above[1] - below[1])
    low_in = transfer[0]
    high_in = transfer[-1]

    print("\nInverter restoration evidence")
    print(f" Vin=0 V   -> Vout={low_in[2]:.6f} V")
    print(f" Vin=VDD   -> Vout={high_in[2]:.6f} V")
    print(f" Vout≈VDD/2 near Vin={switching_input:.3f} V")
    print(f" local slope near transition ≈ {gain:.1f} V/V")
    print(
        "A steep negative slope means a modest input movement near the boundary "
        "becomes a much larger output movement toward a rail."
    )

    finite = all(math.isfinite(value) for row in transient + transfer for value in row)
    if failures or not finite:
        raise RuntimeError("simulation did not satisfy the expected checks")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--keep",
        type=Path,
        metavar="DIR",
        help="copy ngspice data and log into DIR after the run",
    )
    args = parser.parse_args()

    executable = shutil.which("ngspice")
    if executable is None:
        raise SystemExit("ngspice is required (this lab was validated with ngspice 47)")

    circuit = Path(__file__).with_suffix(".cir").resolve()
    with tempfile.TemporaryDirectory(prefix="day-003-cmos-") as temporary:
        work = Path(temporary)
        completed = subprocess.run(
            [executable, "-b", "-o", str(work / "ngspice.log"), str(circuit)],
            cwd=work,
            text=True,
            capture_output=True,
            check=False,
        )
        if completed.returncode != 0:
            log = (work / "ngspice.log").read_text(encoding="utf-8", errors="replace")
            raise SystemExit(f"ngspice failed:\n{completed.stderr}\n{log}")
        summarize(work)
        if args.keep:
            args.keep.mkdir(parents=True, exist_ok=True)
            for name in (
                "day-003-truth-waveforms.dat",
                "day-003-inverter-transfer.dat",
                "ngspice.log",
            ):
                shutil.copy2(work / name, args.keep / name)
            print(f"\nKept raw evidence in {args.keep}")

    print("\nExplain: why do the NAND and NOR require complementary networks?")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
