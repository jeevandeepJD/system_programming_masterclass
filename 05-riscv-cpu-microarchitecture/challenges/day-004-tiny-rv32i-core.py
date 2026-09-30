#!/usr/bin/env python3
"""Runnable, cycle/state-visible single-cycle RV32I-subset core.

The program words are hand encoded.  Each model cycle performs one complete
instruction state transition; this is an educational timing abstraction, not
a timing-accurate model of a physical core.

Run:
    python3 05-riscv-cpu-microarchitecture/challenges/day-004-tiny-rv32i-core.py
    python3 05-riscv-cpu-microarchitecture/challenges/day-004-tiny-rv32i-core.py --step
    python3 05-riscv-cpu-microarchitecture/challenges/day-004-tiny-rv32i-core.py --selftest
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass


MASK32 = 0xFFFF_FFFF


def bits(value: int, high: int, low: int) -> int:
    return (value >> low) & ((1 << (high - low + 1)) - 1)


def sext(value: int, width: int) -> int:
    sign = 1 << (width - 1)
    return (value & (sign - 1)) - (value & sign)


def signed32(value: int) -> int:
    return sext(value & MASK32, 32)


@dataclass(frozen=True)
class Control:
    name: str
    alu_op: str
    alu_b_sel: str = "RS2"
    wb_sel: str = "-"
    pc_sel: str = "PC+4"
    reg_write: bool = False
    mem_read: bool = False
    mem_write: bool = False
    branch: str = "-"


@dataclass(frozen=True)
class Decoded:
    word: int
    rd: int
    rs1: int
    rs2: int
    imm: int
    control: Control


@dataclass(frozen=True)
class Signals:
    decoded: Decoded
    rs1_value: int
    rs2_value: int
    alu_b: int
    alu_result: int
    memory_value: int | None
    writeback_value: int | None
    branch_taken: bool
    next_pc: int


class IllegalInstruction(RuntimeError):
    pass


def decode(word: int) -> Decoded:
    opcode = bits(word, 6, 0)
    rd, funct3 = bits(word, 11, 7), bits(word, 14, 12)
    rs1, rs2 = bits(word, 19, 15), bits(word, 24, 20)
    funct7 = bits(word, 31, 25)

    if opcode == 0x33 and (funct3, funct7) == (0, 0):
        return Decoded(
            word, rd, rs1, rs2, 0,
            Control("ADD", "ADD", wb_sel="ALU", reg_write=True),
        )
    if opcode == 0x13 and funct3 == 0:
        return Decoded(
            word, rd, rs1, rs2, sext(bits(word, 31, 20), 12),
            Control(
                "ADDI", "ADD", alu_b_sel="IMM", wb_sel="ALU",
                reg_write=True,
            ),
        )
    if opcode == 0x03 and funct3 == 0b010:
        return Decoded(
            word, rd, rs1, rs2, sext(bits(word, 31, 20), 12),
            Control(
                "LW", "ADD", alu_b_sel="IMM", wb_sel="MEM",
                reg_write=True, mem_read=True,
            ),
        )
    if opcode == 0x23 and funct3 == 0b010:
        raw = (bits(word, 31, 25) << 5) | bits(word, 11, 7)
        return Decoded(
            word, rd, rs1, rs2, sext(raw, 12),
            Control("SW", "ADD", alu_b_sel="IMM", mem_write=True),
        )
    if opcode == 0x63 and funct3 in (0, 1):
        raw = (
            (bits(word, 31, 31) << 12)
            | (bits(word, 7, 7) << 11)
            | (bits(word, 30, 25) << 5)
            | (bits(word, 11, 8) << 1)
        )
        name = "BEQ" if funct3 == 0 else "BNE"
        return Decoded(
            word, rd, rs1, rs2, sext(raw, 13),
            Control(name, "SUB", pc_sel="BRANCH", branch=name),
        )
    raise IllegalInstruction(
        f"word 0x{word:08x}: unsupported opcode/funct encoding"
    )


class TinyRV32I:
    def __init__(self, program: list[int]) -> None:
        self.program = program
        self.regs = [0] * 32
        self.memory: dict[int, int] = {}
        self.pc = 0
        self.cycle = 0

    def load_word(self, address: int) -> int:
        if address & 3:
            raise RuntimeError(f"misaligned LW at 0x{address:08x}")
        return sum(self.memory.get(address + i, 0) << (8 * i) for i in range(4))

    def store_word(self, address: int, value: int) -> None:
        if address & 3:
            raise RuntimeError(f"misaligned SW at 0x{address:08x}")
        for i in range(4):
            self.memory[address + i] = (value >> (8 * i)) & 0xFF

    def fetch(self) -> int | None:
        index = self.pc // 4
        if self.pc & 3 or not 0 <= index < len(self.program):
            return None
        return self.program[index]

    def combinational(self, word: int) -> Signals:
        d = decode(word)
        a, rs2_value = self.regs[d.rs1], self.regs[d.rs2]
        alu_b = d.imm & MASK32 if d.control.alu_b_sel == "IMM" else rs2_value
        if d.control.alu_op == "ADD":
            alu_result = (a + alu_b) & MASK32
        else:
            alu_result = (a - alu_b) & MASK32

        branch_taken = (
            (d.control.branch == "BEQ" and a == rs2_value)
            or (d.control.branch == "BNE" and a != rs2_value)
        )
        next_pc = (
            (self.pc + d.imm) & MASK32
            if branch_taken else (self.pc + 4) & MASK32
        )
        memory_value = self.load_word(alu_result) if d.control.mem_read else None
        writeback = (
            memory_value if d.control.wb_sel == "MEM"
            else alu_result if d.control.wb_sel == "ALU"
            else None
        )
        return Signals(
            d, a, rs2_value, alu_b, alu_result, memory_value, writeback,
            branch_taken, next_pc,
        )

    def clock(self, s: Signals) -> list[str]:
        """Commit all enabled architectural changes at one abstract edge."""
        changes: list[str] = []
        c, d = s.decoded.control, s.decoded

        if c.mem_write:
            old = self.load_word(s.alu_result)
            self.store_word(s.alu_result, s.rs2_value)
            changes.append(
                f"mem[0x{s.alu_result:08x}] "
                f"0x{old:08x}->0x{s.rs2_value:08x}"
            )
        if c.reg_write and d.rd != 0:
            assert s.writeback_value is not None
            old = self.regs[d.rd]
            self.regs[d.rd] = s.writeback_value & MASK32
            changes.append(
                f"x{d.rd} 0x{old:08x}->0x{self.regs[d.rd]:08x}"
            )

        old_pc = self.pc
        self.pc = s.next_pc
        self.regs[0] = 0
        self.cycle += 1
        changes.append(f"pc 0x{old_pc:08x}->0x{self.pc:08x}")
        return changes


# Hand-encoded RV32I words.  No assembler or encoder constructs this list.
# The loop sums data[0..3], stores 23 at 0x110, then loads it into x5.
PROGRAM = [
    0x00000093,  # 00: addi x1,x0,0       sum = 0
    0x10000113,  # 04: addi x2,x0,256     pointer = 0x100
    0x00400193,  # 08: addi x3,x0,4       count = 4
    0x00012203,  # 0c: lw   x4,0(x2)
    0x004080B3,  # 10: add  x1,x1,x4
    0x00410113,  # 14: addi x2,x2,4
    0xFFF18193,  # 18: addi x3,x3,-1
    0xFE0198E3,  # 1c: bne  x3,x0,-16     back to 0x0c
    0x00112023,  # 20: sw   x1,0(x2)      mem[0x110] = 23
    0x00012283,  # 24: lw   x5,0(x2)      x5 = 23
]


def control_text(c: Control) -> str:
    return (
        f"{c.name:<4} ALU={c.alu_op} B={c.alu_b_sel:<3} WB={c.wb_sel:<3} "
        f"PC={c.pc_sel:<6} RegW={int(c.reg_write)} "
        f"MemR={int(c.mem_read)} MemW={int(c.mem_write)}"
    )


def run(cpu: TinyRV32I, step: bool, max_cycles: int, trace: bool = True) -> None:
    while (word := cpu.fetch()) is not None:
        if cpu.cycle >= max_cycles:
            raise RuntimeError(f"cycle limit {max_cycles} reached")
        s = cpu.combinational(word)
        d, c = s.decoded, s.decoded.control
        if trace:
            print(
                f"\nCycle {cpu.cycle + 1:02d}  PC=0x{cpu.pc:08x} "
                f"instruction=0x{word:08x}  {c.name}"
            )
            print(
                f"  CONTROL {control_text(c)}  rd=x{d.rd} rs1=x{d.rs1} "
                f"rs2=x{d.rs2} imm={d.imm}"
            )
            print(
                f"  COMB    A=0x{s.rs1_value:08x} B=0x{s.alu_b:08x} "
                f"ALU=0x{s.alu_result:08x} branch={int(s.branch_taken)} "
                f"nextPC=0x{s.next_pc:08x}"
                + (
                    f" mem_data=0x{s.memory_value:08x}"
                    if s.memory_value is not None else ""
                )
            )
            print(
                f"  BEFORE  x1(sum)={signed32(cpu.regs[1]):3d} "
                f"x2(ptr)=0x{cpu.regs[2]:03x} x3(count)={cpu.regs[3]} "
                f"x4(data)={signed32(cpu.regs[4]):3d} x5(result)={cpu.regs[5]}"
            )
        if step:
            answer = input("  Predict edge changes; Enter=commit, q=quit: ")
            if answer.strip().lower() == "q":
                return
        changes = cpu.clock(s)
        if trace:
            print(f"  EDGE    {', '.join(changes)}")


def new_demo_cpu() -> TinyRV32I:
    cpu = TinyRV32I(PROGRAM)
    for index, value in enumerate((7, -2, 13, 5)):
        cpu.store_word(0x100 + index * 4, value & MASK32)
    return cpu


def selftest() -> None:
    cpu = new_demo_cpu()
    run(cpu, step=False, max_cycles=64, trace=False)
    assert cpu.pc == 0x28
    assert cpu.regs[0] == 0
    assert signed32(cpu.regs[1]) == 23
    assert cpu.regs[3] == 0
    assert cpu.regs[5] == 23
    assert cpu.load_word(0x110) == 23
    assert [cpu.memory[0x110 + i] for i in range(4)] == [23, 0, 0, 0]
    print("PASS: sum loop produced 23")
    print("PASS: SW/LW round trip at 0x110")
    print("PASS: x0 remained zero and execution stopped at PC 0x28")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--step", action="store_true")
    parser.add_argument("--max-cycles", type=int, default=64)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.max_cycles <= 0:
        parser.error("--max-cycles must be positive")
    if args.selftest:
        selftest()
        return

    print("Program: sum [7, -2, 13, 5], store result, load it into x5")
    print("Predict: loop cycles, taken/not-taken branches, final PC and state.")
    cpu = new_demo_cpu()
    run(cpu, args.step, args.max_cycles)
    print(
        f"\nFINAL cycles={cpu.cycle} PC=0x{cpu.pc:08x} "
        f"x1={signed32(cpu.regs[1])} x5={signed32(cpu.regs[5])} "
        f"mem[0x110]={signed32(cpu.load_word(0x110))}"
    )

    # TODO extension:
    # Add SLT (R-type funct3=010, funct7=0000000).  First predict its control
    # line values and signed comparison result.  Add one hand-encoded SLT to a
    # separate program and extend selftest.  Keep unknown encodings illegal.


if __name__ == "__main__":
    main()
