#!/usr/bin/env python3
"""Explore five-stage pipeline timing, fill/drain, latency, and throughput.

Standard-library only. Run from the repository root:

    python3 05-riscv-cpu-microarchitecture/challenges/day-005-pipelining-and-throughput.py demo
    python3 05-riscv-cpu-microarchitecture/challenges/day-005-pipelining-and-throughput.py trace --instructions 8
    python3 05-riscv-cpu-microarchitecture/challenges/day-005-pipelining-and-throughput.py compare
    python3 05-riscv-cpu-microarchitecture/challenges/day-005-pipelining-and-throughput.py predict
    python3 05-riscv-cpu-microarchitecture/challenges/day-005-pipelining-and-throughput.py predict --answers
    python3 05-riscv-cpu-microarchitecture/challenges/day-005-pipelining-and-throughput.py selftest

This is a timing/accounting model, not an ISA emulator or circuit simulator.
It assumes independent instructions and therefore introduces no hazards.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from typing import Sequence


STAGES = ("IF", "ID", "EX", "MEM", "WB")


@dataclass(frozen=True)
class Timing:
    """Combinational stage delays plus per-stage register overhead, in ps."""

    delays: tuple[float, ...]
    register_overhead: float

    @property
    def period(self) -> float:
        return max(self.delays) + self.register_overhead

    @property
    def latency(self) -> float:
        return len(self.delays) * self.period

    def cycles_for(self, instructions: int) -> int:
        if instructions < 1:
            raise ValueError("instruction count must be positive")
        return len(self.delays) + instructions - 1

    def time_for(self, instructions: int) -> float:
        return self.cycles_for(instructions) * self.period


def pipeline_grid(instructions: int) -> str:
    if instructions < 1:
        raise ValueError("instruction count must be positive")
    cycles = instructions + len(STAGES) - 1
    labels = [f"I{index + 1}" for index in range(instructions)]
    width = max(4, len(str(cycles)) + 1)
    lines = [" " * 8 + "".join(f"C{cycle:<{width - 1}}" for cycle in range(1, cycles + 1))]
    for index, label in enumerate(labels):
        cells = []
        for cycle in range(cycles):
            stage_index = cycle - index
            cell = STAGES[stage_index] if 0 <= stage_index < len(STAGES) else "."
            cells.append(f"{cell:<{width}}")
        lines.append(f"{label:<8}" + "".join(cells))
    return "\n".join(lines)


def print_trace(instructions: int) -> None:
    print("Ideal in-order five-stage trace (no hazards)\n")
    print(pipeline_grid(instructions))
    cycles = instructions + len(STAGES) - 1
    print(
        f"\n{instructions} instructions take {cycles} cycles: "
        f"{len(STAGES) - 1} fill/drain cycles plus {instructions} retirement slots."
    )
    print("After fill, one instruction reaches WB per cycle; each instruction still spans 5 cycles.")


def print_comparison() -> None:
    # One deliberately simple unpipelined path and one uneven five-way split.
    unpipelined_logic = 1000.0
    unpipelined_overhead = 60.0
    pipeline = Timing((180.0, 220.0, 250.0, 210.0, 140.0), 60.0)
    single_period = unpipelined_logic + unpipelined_overhead
    count = 100
    single_time = count * single_period
    pipelined_time = pipeline.time_for(count)

    print("Critical-path split")
    print(f"  unpipelined logic:               {unpipelined_logic:7.1f} ps")
    print(f"  one register overhead:           {unpipelined_overhead:7.1f} ps")
    print(f"  unpipelined period:              {single_period:7.1f} ps")
    print(f"  stage logic delays:              {pipeline.delays}")
    print(f"  pipelined period=max(stage)+ovh: {pipeline.period:7.1f} ps")
    print(f"  one-instruction pipeline latency:{pipeline.latency:7.1f} ps")
    print()
    print(f"For {count} independent instructions")
    print(f"  unpipelined: {count} cycles × {single_period:.1f} ps = {single_time:.1f} ps")
    print(
        f"  pipelined:   {pipeline.cycles_for(count)} cycles × "
        f"{pipeline.period:.1f} ps = {pipelined_time:.1f} ps"
    )
    print(f"  batch speedup in this model: {single_time / pipelined_time:.2f}×")
    print(
        "\nThe five-way split is not 5× faster: the slowest stage sets the period, "
        "every stage pays register overhead, and a finite batch must fill and drain."
    )


QUESTIONS = (
    (
        "A",
        "Five stages have logic delays 180, 220, 250, 210, 140 ps and each "
        "pipeline boundary costs 60 ps. What period is required?",
        "310 ps: max(180, 220, 250, 210, 140) + 60.",
    ),
    (
        "B",
        "At 310 ps per cycle, what are ideal latency and steady-state throughput?",
        "Latency is 5 × 310 = 1550 ps; throughput after fill is 1/310 ps per instruction.",
    ),
    (
        "C",
        "How many cycles do 12 independent instructions need in a five-stage pipeline?",
        "5 + 12 - 1 = 16 cycles.",
    ),
    (
        "D",
        "If EX grows from 250 to 330 ps, which quantities change?",
        "Period becomes 390 ps, so both time latency and throughput worsen; cycle latency stays five.",
    ),
    (
        "E",
        "Can an instruction in EX update an architectural register immediately?",
        "No. In this model its result and metadata advance to later pipeline registers; retirement/write-back is later.",
    ),
)


def show_predictions(answers: bool) -> None:
    print("Cover the answers, write a prediction, then rerun with --answers.\n")
    for label, question, answer in QUESTIONS:
        print(f"{label}. {question}")
        if answers:
            print(f"   ANSWER: {answer}")
        print()


def selftest() -> None:
    timing = Timing((180, 220, 250, 210, 140), 60)
    assert timing.period == 310
    assert timing.latency == 1550
    assert timing.cycles_for(1) == 5
    assert timing.cycles_for(12) == 16
    assert pipeline_grid(2).count("IF") == 2
    assert pipeline_grid(2).count("WB") == 2
    print("PASS: timing equations")
    print("PASS: fill/drain cycle counts")
    print("PASS: every instruction visits IF and WB")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command")
    trace = subparsers.add_parser("trace", help="draw an ideal five-stage trace")
    trace.add_argument("--instructions", type=int, default=6)
    subparsers.add_parser("compare", help="compare latency and batch throughput")
    predict = subparsers.add_parser("predict", help="show prediction exercises")
    predict.add_argument("--answers", action="store_true")
    subparsers.add_parser("demo", help="run the trace and timing comparison")
    subparsers.add_parser("selftest", help="check model invariants")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    command = args.command or "demo"
    try:
        if command == "trace":
            print_trace(args.instructions)
        elif command == "compare":
            print_comparison()
        elif command == "predict":
            show_predictions(args.answers)
        elif command == "selftest":
            selftest()
        else:
            print_trace(6)
            print("\n" + "=" * 72 + "\n")
            print_comparison()
    except ValueError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
