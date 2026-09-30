#!/usr/bin/env python3
"""Sweep a small CMOS inverter's load, then sample three illustrative corners.

Run from the repository root:

    python3 04-cpu-and-chip-design/challenges/day-004-delay-power.py

The script uses only Python's standard library and ngspice. Its "fan-out"
labels map one receiver input to 4 fF so the trend is easy to discuss; they
are not measurements of a real standard-cell library.
"""

from __future__ import annotations

import csv
import io
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


MEASURE = re.compile(
    r"^\s*(tphl|tplh|tfall|trise|qsupply)\s*=\s*"
    r"([-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:e[-+]?\d+)?)",
    re.IGNORECASE | re.MULTILINE,
)


def set_parameter(netlist: str, name: str, value: str) -> str:
    pattern = re.compile(rf"(?m)^(\.param\s+{re.escape(name)}=).*$")
    updated, count = pattern.subn(rf"\g<1>{value}", netlist)
    if count != 1:
        raise RuntimeError(f"could not set {name}")
    return updated


def simulate(
    executable: str,
    template: str,
    work: Path,
    label: str,
    *,
    load_ff: float,
    vdd: float = 1.8,
    nscale: float = 1.0,
    pscale: float = 1.0,
    temperature: float = 25.0,
) -> dict[str, float]:
    netlist = template
    parameters = {
        "CLOAD": f"{load_ff}f",
        "VDDVAL": str(vdd),
        "KNSCALE": str(nscale),
        "KPSCALE": str(pscale),
        "TEMPERATURE": str(temperature),
    }
    for name, value in parameters.items():
        netlist = set_parameter(netlist, name, value)

    circuit = work / f"{label}.cir"
    log = work / f"{label}.log"
    circuit.write_text(netlist, encoding="utf-8")
    completed = subprocess.run(
        [executable, "-b", "-o", str(log), str(circuit)],
        cwd=work,
        text=True,
        capture_output=True,
        check=False,
    )
    log_text = log.read_text(encoding="utf-8", errors="replace")
    if completed.returncode != 0:
        raise RuntimeError(f"ngspice failed for {label}:\n{completed.stderr}\n{log_text}")
    values = {name.lower(): float(value) for name, value in MEASURE.findall(log_text)}
    missing = {"tphl", "tplh", "tfall", "trise", "qsupply"} - values.keys()
    if missing:
        raise RuntimeError(f"{label}: missing measurements {sorted(missing)}\n{log_text}")

    values["load_ff"] = load_ff
    values["vdd"] = vdd
    values["energy_fj"] = -values["qsupply"] * vdd * 1e15
    values["average_power_uw"] = values["energy_fj"] * 1e-15 / 8e-9 * 1e6
    return values


def print_load_sweep(rows: list[dict[str, float]]) -> None:
    print("Load/fan-out sweep (nominal teaching model)")
    print(" fanout  load(fF)  tpHL(ps)  tpLH(ps)  rise(ps)  fall(ps)  energy(fJ)")
    for row in rows:
        print(
            f" {row['load_ff'] / 4:6.1f}  {row['load_ff']:8.1f}"
            f"  {row['tphl'] * 1e12:8.2f}  {row['tplh'] * 1e12:8.2f}"
            f"  {row['trise'] * 1e12:8.2f}  {row['tfall'] * 1e12:8.2f}"
            f"  {row['energy_fj']:10.2f}"
        )
    print(
        "\nMore receiver capacitance takes more charge. Delay and transition time "
        "rise; supply energy per toggle also rises."
    )


def print_corners(rows: list[tuple[str, dict[str, float]]]) -> None:
    print("\nIllustrative PVT sensitivity at fan-out 4")
    print(" corner             VDD  temp(C)  average delay(ps)")
    for name, row in rows:
        average = (row["tphl"] + row["tplh"]) * 0.5e12
        print(f" {name:18} {row['vdd']:4.2f}  {row['temp']:7.1f}  {average:17.2f}")
    print(
        "These are sensitivity experiments, not sign-off corners. Real PVT "
        "analysis uses characterized foundry models and cell libraries."
    )


def csv_text(rows: list[dict[str, float]]) -> str:
    output = io.StringIO()
    names = (
        "fanout",
        "load_ff",
        "tphl_ps",
        "tplh_ps",
        "trise_ps",
        "tfall_ps",
        "energy_fj",
        "average_power_uw",
    )
    writer = csv.DictWriter(output, fieldnames=names)
    writer.writeheader()
    for row in rows:
        writer.writerow(
            {
                "fanout": row["load_ff"] / 4,
                "load_ff": row["load_ff"],
                "tphl_ps": row["tphl"] * 1e12,
                "tplh_ps": row["tplh"] * 1e12,
                "trise_ps": row["trise"] * 1e12,
                "tfall_ps": row["tfall"] * 1e12,
                "energy_fj": row["energy_fj"],
                "average_power_uw": row["average_power_uw"],
            }
        )
    return output.getvalue()


def main() -> int:
    executable = shutil.which("ngspice")
    if executable is None:
        raise SystemExit("ngspice is required (this lab was validated with ngspice 47)")

    source = Path(__file__).with_suffix(".cir")
    template = source.read_text(encoding="utf-8")
    with tempfile.TemporaryDirectory(prefix="day-004-delay-") as temporary:
        work = Path(temporary)
        loads = [
            simulate(executable, template, work, f"load-{load}", load_ff=load)
            for load in (4.0, 8.0, 16.0, 32.0)
        ]
        print_load_sweep(loads)

        corner_specs = (
            ("slow/low/hot", 1.62, 0.75, 85.0),
            ("typical", 1.80, 1.00, 25.0),
            ("fast/high/cool", 1.98, 1.25, 0.0),
        )
        corners: list[tuple[str, dict[str, float]]] = []
        for index, (name, vdd, scale, temperature) in enumerate(corner_specs):
            row = simulate(
                executable,
                template,
                work,
                f"corner-{index}",
                load_ff=16.0,
                vdd=vdd,
                nscale=scale,
                pscale=scale,
                temperature=temperature,
            )
            row["temp"] = temperature
            corners.append((name, row))
        print_corners(corners)

        destination = Path("/tmp/day-004-delay-power.csv")
        destination.write_text(csv_text(loads), encoding="utf-8")
        print(f"\nWrote {destination}")

    print(
        "\nCPU bridge: a heavily loaded node or slow corner can lengthen a "
        "combinational path; the clock period must still cover the worst valid path."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
