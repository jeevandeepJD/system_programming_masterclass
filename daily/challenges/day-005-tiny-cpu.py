#!/usr/bin/env python3
"""A cycle-visible simulator for the Tiny-8 teaching CPU.

Run:
    python3 daily/challenges/day-005-tiny-cpu.py
    python3 daily/challenges/day-005-tiny-cpu.py --step

This models state transitions, not transistor propagation time.  The
``combinational`` method calculates the values that would settle between
clock edges; ``clock`` commits only enabled state changes.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from typing import Optional


FETCH = "FETCH"
EXECUTE = "EXECUTE"


@dataclass(frozen=True)
class Control:
    """Control-unit outputs for the current interval."""

    name: str
    ir_load: bool = False
    pc_increment: bool = False
    reg_write: bool = False
    mem_write: bool = False
    halt: bool = False
    alu_op: str = "IDLE"
    rd: Optional[int] = None
    rs: Optional[int] = None
    immediate: Optional[int] = None
    address: Optional[int] = None


@dataclass(frozen=True)
class Signals:
    """Combinational values that settle before the next clock edge."""

    control: Control
    instruction_byte: Optional[int] = None
    next_pc: Optional[int] = None
    alu_a: Optional[int] = None
    alu_b: Optional[int] = None
    alu_out: Optional[int] = None
    memory_write_data: Optional[int] = None


def decode(ir: int, phase: str) -> Control:
    """Combinational control decoder: inputs change, outputs settle."""
    if phase == FETCH:
        return Control(
            name="FETCH",
            ir_load=True,
            pc_increment=True,
        )

    opcode = (ir >> 6) & 0b11

    if opcode == 0b00:  # 00 dd iiii
        return Control(
            name="LDI",
            reg_write=True,
            alu_op="PASS_IMMEDIATE",
            rd=(ir >> 4) & 0b11,
            immediate=ir & 0x0F,
        )

    if opcode == 0b01:  # 01 dd ss 00
        if ir & 0b11:
            raise ValueError(f"invalid ADD reserved bits in 0x{ir:02x}")
        return Control(
            name="ADD",
            reg_write=True,
            alu_op="ADD",
            rd=(ir >> 4) & 0b11,
            rs=(ir >> 2) & 0b11,
        )

    if opcode == 0b10:  # 10 ss aaaa
        return Control(
            name="STORE",
            mem_write=True,
            alu_op="PASS_REGISTER",
            rs=(ir >> 4) & 0b11,
            address=ir & 0x0F,
        )

    if ir != 0xC0:  # 11 000000
        raise ValueError(f"invalid HALT reserved bits in 0x{ir:02x}")
    return Control(name="HALT", halt=True)


class TinyCPU:
    """A two-cycle (FETCH/EXECUTE) implementation of the Tiny-8 ISA."""

    def __init__(self, program: list[int]) -> None:
        if not program:
            raise ValueError("program must contain at least one byte")
        if any(not 0 <= byte <= 0xFF for byte in program):
            raise ValueError("instruction bytes must fit in eight bits")

        self.instruction_memory = list(program)
        self.data_memory = [0] * 16
        self.registers = [0] * 4
        self.pc = 0
        self.ir = 0
        self.phase = FETCH
        self.halted = False
        self.cycle = 0

    def combinational(self) -> Signals:
        """Calculate candidate values; do not mutate CPU state."""
        control = decode(self.ir, self.phase)

        if self.phase == FETCH:
            if self.pc >= len(self.instruction_memory):
                raise IndexError(
                    f"PC {self.pc} is outside instruction memory "
                    f"(size {len(self.instruction_memory)})"
                )
            return Signals(
                control=control,
                instruction_byte=self.instruction_memory[self.pc],
                next_pc=(self.pc + 1) & 0xFF,
            )

        if control.name == "LDI":
            assert control.immediate is not None
            return Signals(
                control=control,
                alu_a=control.immediate,
                alu_b=0,
                alu_out=control.immediate,
            )

        if control.name == "ADD":
            assert control.rd is not None and control.rs is not None
            a = self.registers[control.rd]
            b = self.registers[control.rs]
            return Signals(
                control=control,
                alu_a=a,
                alu_b=b,
                alu_out=(a + b) & 0xFF,
            )

        if control.name == "STORE":
            assert control.rs is not None
            value = self.registers[control.rs]
            return Signals(
                control=control,
                alu_a=value,
                alu_b=0,
                alu_out=value,
                memory_write_data=value,
            )

        return Signals(control=control)  # HALT has no ALU work.

    def clock(self, signals: Signals) -> list[str]:
        """Commit enabled state changes at one clock edge."""
        if self.halted:
            return []

        changes: list[str] = []
        control = signals.control

        if control.ir_load:
            assert signals.instruction_byte is not None
            old_ir = self.ir
            self.ir = signals.instruction_byte
            changes.append(f"IR 0x{old_ir:02X}->0x{self.ir:02X}")

        if control.pc_increment:
            assert signals.next_pc is not None
            old_pc = self.pc
            self.pc = signals.next_pc
            changes.append(f"PC {old_pc}->{self.pc}")

        if control.reg_write:
            assert control.rd is not None and signals.alu_out is not None
            old_value = self.registers[control.rd]
            self.registers[control.rd] = signals.alu_out
            changes.append(
                f"R{control.rd} {old_value}->{self.registers[control.rd]}"
            )

        if control.mem_write:
            assert control.address is not None
            assert signals.memory_write_data is not None
            old_value = self.data_memory[control.address]
            self.data_memory[control.address] = signals.memory_write_data
            changes.append(
                f"MEM[{control.address}] "
                f"{old_value}->{self.data_memory[control.address]}"
            )

        if control.halt:
            self.halted = True
            changes.append("halted 0->1")
        else:
            old_phase = self.phase
            self.phase = EXECUTE if self.phase == FETCH else FETCH
            changes.append(f"phase {old_phase}->{self.phase}")

        self.cycle += 1
        return changes

    def state_text(self) -> str:
        regs = " ".join(
            f"R{index}={value:3d}/0x{value:02X}"
            for index, value in enumerate(self.registers)
        )
        return (
            f"PC={self.pc:02d} IR=0x{self.ir:02X} "
            f"phase={self.phase:<7} halted={int(self.halted)} | {regs} | "
            f"MEM[0..3]={self.data_memory[:4]}"
        )


def control_text(control: Control) -> str:
    fields = [
        f"op={control.name}",
        f"IR_load={int(control.ir_load)}",
        f"PC_inc={int(control.pc_increment)}",
        f"Reg_write={int(control.reg_write)}",
        f"Mem_write={int(control.mem_write)}",
        f"Halt={int(control.halt)}",
        f"ALU={control.alu_op}",
    ]
    if control.rd is not None:
        fields.append(f"rd=R{control.rd}")
    if control.rs is not None:
        fields.append(f"rs=R{control.rs}")
    if control.immediate is not None:
        fields.append(f"imm={control.immediate}")
    if control.address is not None:
        fields.append(f"addr={control.address}")
    return " ".join(fields)


def signals_text(signals: Signals) -> str:
    values: list[str] = []
    if signals.instruction_byte is not None:
        values.append(f"instruction_memory[PC]=0x{signals.instruction_byte:02X}")
    if signals.next_pc is not None:
        values.append(f"candidate_PC={signals.next_pc}")
    if signals.alu_out is not None:
        values.append(
            f"ALU_A={signals.alu_a} ALU_B={signals.alu_b} "
            f"ALU_out={signals.alu_out}"
        )
    if signals.memory_write_data is not None:
        values.append(f"memory_write_data={signals.memory_write_data}")
    return " | ".join(values) if values else "no datapath value is written"


def run(cpu: TinyCPU, step: bool, max_cycles: int) -> None:
    while not cpu.halted:
        if cpu.cycle >= max_cycles:
            raise RuntimeError(f"stopped after {max_cycles} cycles without HALT")

        signals = cpu.combinational()
        print(f"\nCycle {cpu.cycle + 1} — before edge")
        print(f"  STATE   {cpu.state_text()}")
        print(f"  CONTROL {control_text(signals.control)}")
        print(f"  COMB    {signals_text(signals)}")

        if step:
            answer = input(
                "  Predict which state changes at the edge; "
                "press Enter to commit (q to quit): "
            ).strip().lower()
            if answer == "q":
                print("Stopped before the edge; displayed candidates were not committed.")
                return

        changes = cpu.clock(signals)
        print(f"  EDGE    {', '.join(changes) if changes else 'no state change'}")
        print(f"  AFTER   {cpu.state_text()}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Trace the two-cycle Tiny-8 CPU."
    )
    parser.add_argument(
        "--step",
        action="store_true",
        help="pause before each clock edge for a prediction",
    )
    parser.add_argument(
        "--max-cycles",
        type=int,
        default=32,
        help="safety limit for simulation cycles (default: 32)",
    )
    args = parser.parse_args()

    # 00 dd iiii   LDI Rd, imm4
    # 01 dd ss 00  ADD Rd, Rs
    # 10 ss aaaa   STORE Rs, addr4
    # 11 000000    HALT
    program = [
        0x03,  # LDI R0, 3
        0x15,  # LDI R1, 5
        0x44,  # ADD R0, R1
        0x80,  # STORE R0, 0
        0xC0,  # HALT
    ]

    print("Tiny-8 program: 03 15 44 80 C0")
    print("Prediction prompts (answer before reading the final trace):")
    print("  1. After the first FETCH edge, which two state values change?")
    print("  2. During ADD, when does ALU_out become 8?")
    print("  3. At which edge does R0 become 8?")
    print("  4. What are final PC, IR, R0, R1, and MEM[0]?")

    cpu = TinyCPU(program)
    run(cpu, step=args.step, max_cycles=args.max_cycles)

    expected = (cpu.registers[0], cpu.registers[1], cpu.data_memory[0])
    if cpu.halted:
        assert expected == (8, 5, 8), f"unexpected final values: {expected}"
        print("\nResult verified: R0=8 and MEM[0]=8.")

    # TODO extension (intentionally left for the learner):
    # Add a Z (zero) flag that is captured when ADD executes.  Display both
    # the combinational zero test and the edge at which Z becomes state.
    #
    # Deeper option: design a conditional branch.  The four two-bit opcodes
    # are already occupied, so document how you revise the encoding instead
    # of silently inventing a fifth opcode.


if __name__ == "__main__":
    main()
