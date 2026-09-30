#!/usr/bin/env python3
"""Visual five-stage hazard simulator and prediction exercises.

No third-party packages are required.

    python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py trace data
    python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py trace branch --prediction not-taken
    python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py trace branch --prediction taken
    python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py trace branch --prediction one-bit
    python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py exercise
    python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py exercise --answers
    python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py selftest

The simulator models timing and instruction identity, not data values. Branch
outcomes are attached to the teaching program. A branch resolves in EX.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, replace
from typing import Sequence


@dataclass(frozen=True)
class Instruction:
    pc: int
    text: str
    kind: str
    rd: str | None = None
    sources: tuple[str, ...] = ()
    target: int | None = None
    taken: bool = False
    predicted_taken: bool = False

    @property
    def short(self) -> str:
        return f"{self.pc}:{self.text}"


def instruction(
    pc: int,
    text: str,
    kind: str,
    rd: str | None = None,
    sources: tuple[str, ...] = (),
    target: int | None = None,
    taken: bool = False,
) -> Instruction:
    return Instruction(pc, text, kind, rd, sources, target, taken)


PROGRAMS: dict[str, tuple[Instruction, ...]] = {
    "data": (
        instruction(0, "add x5,x1,x2", "alu", "x5", ("x1", "x2")),
        instruction(1, "sub x6,x5,x3", "alu", "x6", ("x5", "x3")),
        instruction(2, "lw x7,0(x6)", "load", "x7", ("x6",)),
        instruction(3, "add x8,x7,x5", "alu", "x8", ("x7", "x5")),
        instruction(4, "sw x8,8(x1)", "store", None, ("x8", "x1")),
    ),
    "branch": (
        instruction(0, "add x5,x1,x2", "alu", "x5", ("x1", "x2")),
        instruction(1, "beq x5,x0,+3", "branch", None, ("x5", "x0"), target=4, taken=True),
        instruction(2, "add x6,x6,x1", "alu", "x6", ("x6", "x1")),
        instruction(3, "sw x6,0(x2)", "store", None, ("x6", "x2")),
        instruction(4, "sub x9,x9,x3", "alu", "x9", ("x9", "x3")),
        instruction(5, "and x10,x9,x4", "alu", "x10", ("x9", "x4")),
    ),
    "structural": (
        instruction(0, "lw x5,0(x1)", "load", "x5", ("x1",)),
        instruction(1, "add x6,x2,x3", "alu", "x6", ("x2", "x3")),
        instruction(2, "sw x6,0(x4)", "store", None, ("x6", "x4")),
        instruction(3, "xor x7,x8,x9", "alu", "x7", ("x8", "x9")),
        instruction(4, "or x10,x11,x12", "alu", "x10", ("x11", "x12")),
    ),
}


class Predictor:
    def __init__(self, mode: str) -> None:
        self.mode = mode
        self.one_bit: dict[int, bool] = {}

    def predict(self, insn: Instruction) -> bool:
        if self.mode == "not-taken":
            return False
        if self.mode == "taken":
            return True
        if self.mode == "perfect":
            return insn.taken
        return self.one_bit.get(insn.pc, False)

    def update(self, insn: Instruction) -> None:
        if self.mode == "one-bit":
            self.one_bit[insn.pc] = insn.taken


@dataclass
class Result:
    rows: list[tuple[int, tuple[str, ...], str]]
    retired: int
    cycles: int
    stalls: int
    flushes: int
    structural_stalls: int


def writes(insn: Instruction | None) -> bool:
    return insn is not None and insn.rd is not None and insn.rd != "x0"


def depends(consumer: Instruction | None, producer: Instruction | None) -> bool:
    return (
        consumer is not None
        and writes(producer)
        and producer.rd in consumer.sources
    )


def simulate(
    program: tuple[Instruction, ...],
    prediction: str = "not-taken",
    forwarding: bool = True,
    unified_memory: bool = False,
    max_cycles: int = 100,
) -> Result:
    by_pc = {insn.pc: insn for insn in program}
    predictor = Predictor(prediction)
    stages: list[Instruction | None] = [None] * 5  # IF, ID, EX, MEM, WB
    pc = 0
    retired = stalls = flushes = structural_stalls = 0
    rows: list[tuple[int, tuple[str, ...], str]] = []

    for cycle in range(1, max_cycles + 1):
        events: list[str] = []
        if stages[4] is not None:
            retired += 1
            events.append(f"retire {stages[4].pc}")

        redirect: int | None = None
        ex = stages[2]
        if ex is not None and ex.kind == "branch":
            actual_next = ex.target if ex.taken else ex.pc + 1
            predicted_next = ex.target if ex.predicted_taken else ex.pc + 1
            predictor.update(ex)
            if actual_next != predicted_next:
                redirect = actual_next
                wrong = sum(item is not None for item in stages[:2])
                flushes += wrong
                events.append(f"mispredict: flush {wrong}, redirect {actual_next}")
            else:
                events.append("branch prediction correct")

        data_stall = False
        if redirect is None and stages[1] is not None:
            if forwarding:
                data_stall = stages[2] is not None and stages[2].kind == "load" and depends(stages[1], stages[2])
                if data_stall:
                    events.append(f"load-use stall on {stages[2].rd}")
            else:
                blockers = [stage for stage in stages[2:4] if depends(stages[1], stage)]
                data_stall = bool(blockers)
                if data_stall:
                    events.append(f"RAW stall waiting for {blockers[0].rd}")

        rows.append(
            (
                cycle,
                tuple(item.short if item is not None else "." for item in stages),
                "; ".join(events) or "-",
            )
        )

        old_if, old_id, old_ex, old_mem, _old_wb = stages
        next_wb = old_mem
        next_mem = old_ex

        if redirect is not None:
            next_ex = None
            next_id = None
            next_if = None
            pc = redirect
        elif data_stall:
            stalls += 1
            next_ex = None
            next_id = old_id
            next_if = old_if
        else:
            next_ex = old_id
            next_id = old_if
            next_if = None

        memory_busy = unified_memory and old_mem is not None and old_mem.kind in {"load", "store"}
        can_fetch = redirect is not None or (not data_stall and next_if is None)
        if can_fetch and memory_busy:
            structural_stalls += 1
            rows[-1] = (cycle, rows[-1][1], (rows[-1][2] + "; " if rows[-1][2] != "-" else "") + "IF structural stall")
        elif can_fetch and pc in by_pc:
            fetched = by_pc[pc]
            if fetched.kind == "branch":
                predicted = predictor.predict(fetched)
                fetched = replace(fetched, predicted_taken=predicted)
                pc = fetched.target if predicted else fetched.pc + 1
            else:
                pc += 1
            next_if = fetched

        stages = [next_if, next_id, next_ex, next_mem, next_wb]
        if all(stage is None for stage in stages) and pc not in by_pc:
            return Result(rows, retired, cycle, stalls, flushes, structural_stalls)

    raise RuntimeError(f"simulation did not drain in {max_cycles} cycles")


def print_result(result: Result, title: str) -> None:
    print(title)
    print("Cycle | IF                 | ID                 | EX                 | MEM                | WB                 | event")
    print("-" * 128)
    for cycle, cells, event in result.rows:
        print(f"{cycle:5} | " + " | ".join(f"{cell:<18}" for cell in cells) + f" | {event}")
    cpi = result.cycles / result.retired if result.retired else float("inf")
    print(
        f"\nretired={result.retired} cycles={result.cycles} CPI={cpi:.3f} "
        f"data-stalls={result.stalls} flushed-instructions={result.flushes} "
        f"structural-stalls={result.structural_stalls}"
    )
    print("Rows show stage occupancy at the start of each cycle; '.' is a bubble.")


EXERCISES = (
    (
        "A",
        "add x5,x1,x2; sub x6,x5,x3 — classify the dependence and response.",
        "RAW on x5. Full forwarding supplies the value to the consumer EX stage; no stall is needed.",
    ),
    (
        "B",
        "lw x7,0(x6); add x8,x7,x5 — predict the classic five-stage response.",
        "RAW load-use hazard. The load value is too late for the immediately following EX use: stall one cycle, then forward.",
    ),
    (
        "C",
        "add x5,... followed by another write to x5, with no intervening read: RAW, WAR, or WAW?",
        "WAW name dependence. This simple in-order, single-issue pipeline preserves write order, so it needs no special stall.",
    ),
    (
        "D",
        "An older instruction reads x5; a younger instruction writes x5. Is WAR a hazard here?",
        "It is a WAR name dependence, but reads occur in ID before the younger in-order write reaches WB, so this pipeline cannot violate it.",
    ),
    (
        "E",
        "A taken branch resolves in EX after predict-not-taken. Which younger work is wrong-path?",
        "The instructions in IF and ID are younger and must be flushed; the branch and all older instructions remain.",
    ),
    (
        "F",
        "Why can CPI exceed 1 even though the ideal pipeline retires one instruction each cycle?",
        "Fill/drain, data stalls, structural conflicts, branch recovery, cache misses, and other events insert cycles without useful retirement.",
    ),
)


def show_exercises(answers: bool) -> None:
    print("Predict first. Use the trace commands as evidence, then reveal answers.\n")
    for label, prompt, answer in EXERCISES:
        print(f"{label}. {prompt}")
        if answers:
            print(f"   ANSWER: {answer}")
        print()


def selftest() -> None:
    data = simulate(PROGRAMS["data"])
    assert data.retired == len(PROGRAMS["data"])
    assert data.stalls == 1
    no_forwarding = simulate(PROGRAMS["data"], forwarding=False)
    assert no_forwarding.stalls > data.stalls

    wrong = simulate(PROGRAMS["branch"], prediction="not-taken")
    perfect = simulate(PROGRAMS["branch"], prediction="perfect")
    assert wrong.flushes == 2
    assert perfect.flushes == 0
    assert wrong.retired == 4  # branch target skips PCs 2 and 3
    assert perfect.cycles < wrong.cycles

    split = simulate(PROGRAMS["structural"], unified_memory=False)
    unified = simulate(PROGRAMS["structural"], unified_memory=True)
    assert split.structural_stalls == 0
    assert unified.structural_stalls > 0
    print("PASS: forwarding leaves exactly one load-use stall")
    print("PASS: no-forwarding mode adds RAW stalls")
    print("PASS: misprediction flushes two younger instructions")
    print("PASS: perfect prediction avoids branch flush")
    print("PASS: unified memory exposes an IF/MEM structural conflict")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command")
    trace = subparsers.add_parser("trace", help="draw a cycle-by-cycle pipeline trace")
    trace.add_argument("program", choices=tuple(PROGRAMS), nargs="?", default="data")
    trace.add_argument(
        "--prediction",
        choices=("not-taken", "taken", "one-bit", "perfect"),
        default="not-taken",
    )
    trace.add_argument("--no-forwarding", action="store_true")
    trace.add_argument("--unified-memory", action="store_true")
    exercise = subparsers.add_parser("exercise", help="show hazard prediction prompts")
    exercise.add_argument("--answers", action="store_true")
    subparsers.add_parser("selftest", help="validate pipeline invariants")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.command is None:
        args = parser.parse_args(["trace", "data"])
    command = args.command
    if command == "exercise":
        show_exercises(args.answers)
    elif command == "selftest":
        selftest()
    else:
        result = simulate(
            PROGRAMS[args.program],
            prediction=args.prediction,
            forwarding=not args.no_forwarding,
            unified_memory=args.unified_memory,
        )
        print_result(
            result,
            f"Program={args.program} prediction={args.prediction} "
            f"forwarding={'off' if args.no_forwarding else 'on'} "
            f"memory={'unified' if args.unified_memory else 'split'}",
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
