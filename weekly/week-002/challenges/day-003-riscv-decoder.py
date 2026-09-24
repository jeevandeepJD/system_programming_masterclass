#!/usr/bin/env python3
"""
Day 3 — RV64I decoder and trace laboratory.

No RISC-V compiler, assembler, emulator, or third-party package is required.
This program decodes a useful teaching subset of the base integer ISA and
executes enough of it to make register, immediate, branch, and load/store
behavior visible.

Try:
  python3 day-003-riscv-decoder.py demo
  python3 day-003-riscv-decoder.py decode 0xfff50513 0x00b50633
  python3 day-003-riscv-decoder.py fields 0x00b50633
  python3 day-003-riscv-decoder.py trace sum
  python3 day-003-riscv-decoder.py exercise
  python3 day-003-riscv-decoder.py exercise --answers
  python3 day-003-riscv-decoder.py selftest

Scope:
  R-type integer ALU operations
  I-type immediate ALU operations, loads, and JALR
  S-type stores
  B-type conditional branches
  U-type LUI/AUIPC
  J-type JAL

The simulator is deliberately not a complete RISC-V implementation. It does
not model privilege, CSRs, traps, atomics, compressed instructions, floating
point, or the memory-ordering details needed by a production emulator.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from typing import Dict, List, MutableMapping, Sequence


MASK64 = (1 << 64) - 1

ABI_NAMES = (
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0/fp", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
)


class DecodeError(ValueError):
    """The 32-bit word is not in the subset understood by this laboratory."""


def bits(value: int, high: int, low: int) -> int:
    """Return inclusive bit slice value[high:low]."""
    width = high - low + 1
    return (value >> low) & ((1 << width) - 1)


def sign_extend(value: int, width: int) -> int:
    """Interpret value as a width-bit two's-complement integer."""
    sign = 1 << (width - 1)
    return (value & (sign - 1)) - (value & sign)


def signed64(value: int) -> int:
    return sign_extend(value & MASK64, 64)


def reg_name(number: int, abi: bool = True) -> str:
    return ABI_NAMES[number] if abi else f"x{number}"


def parse_word(text: str) -> int:
    value = int(text, 0)
    if not 0 <= value <= 0xFFFFFFFF:
        raise argparse.ArgumentTypeError("instruction must fit in 32 bits")
    return value


@dataclass(frozen=True)
class Instruction:
    word: int
    name: str
    fmt: str
    rd: int | None = None
    rs1: int | None = None
    rs2: int | None = None
    imm: int | None = None
    width: int | None = None
    unsigned_load: bool = False

    def assembly(self, abi: bool = True) -> str:
        r = lambda n: reg_name(n, abi)
        if self.fmt == "R":
            return f"{self.name} {r(self.rd)}, {r(self.rs1)}, {r(self.rs2)}"
        if self.name in {"lb", "lh", "lw", "ld", "lbu", "lhu", "lwu"}:
            return f"{self.name} {r(self.rd)}, {self.imm}({r(self.rs1)})"
        if self.name in {"sb", "sh", "sw", "sd"}:
            return f"{self.name} {r(self.rs2)}, {self.imm}({r(self.rs1)})"
        if self.name in {"beq", "bne", "blt", "bge", "bltu", "bgeu"}:
            sign = "+" if self.imm is not None and self.imm >= 0 else ""
            return f"{self.name} {r(self.rs1)}, {r(self.rs2)}, {sign}{self.imm}"
        if self.name == "jalr":
            return f"jalr {r(self.rd)}, {self.imm}({r(self.rs1)})"
        if self.name == "jal":
            sign = "+" if self.imm is not None and self.imm >= 0 else ""
            return f"jal {r(self.rd)}, {sign}{self.imm}"
        if self.name in {"lui", "auipc"}:
            return f"{self.name} {r(self.rd)}, 0x{self.imm >> 12:x}"
        return f"{self.name} {r(self.rd)}, {r(self.rs1)}, {self.imm}"


R_OPS = {
    (0x0, 0x00): "add",
    (0x0, 0x20): "sub",
    (0x1, 0x00): "sll",
    (0x2, 0x00): "slt",
    (0x3, 0x00): "sltu",
    (0x4, 0x00): "xor",
    (0x5, 0x00): "srl",
    (0x5, 0x20): "sra",
    (0x6, 0x00): "or",
    (0x7, 0x00): "and",
}

I_OPS = {
    0x0: "addi",
    0x2: "slti",
    0x3: "sltiu",
    0x4: "xori",
    0x6: "ori",
    0x7: "andi",
}

LOAD_OPS = {
    0x0: ("lb", 1, False),
    0x1: ("lh", 2, False),
    0x2: ("lw", 4, False),
    0x3: ("ld", 8, False),
    0x4: ("lbu", 1, True),
    0x5: ("lhu", 2, True),
    0x6: ("lwu", 4, True),
}

STORE_OPS = {
    0x0: ("sb", 1),
    0x1: ("sh", 2),
    0x2: ("sw", 4),
    0x3: ("sd", 8),
}

BRANCH_OPS = {
    0x0: "beq",
    0x1: "bne",
    0x4: "blt",
    0x5: "bge",
    0x6: "bltu",
    0x7: "bgeu",
}


def decode(word: int) -> Instruction:
    """Decode one 32-bit instruction from the supported RV64I subset."""
    opcode = bits(word, 6, 0)
    rd = bits(word, 11, 7)
    funct3 = bits(word, 14, 12)
    rs1 = bits(word, 19, 15)
    rs2 = bits(word, 24, 20)
    funct7 = bits(word, 31, 25)

    if opcode == 0x33:
        name = R_OPS.get((funct3, funct7))
        if name is None:
            raise DecodeError(
                f"unsupported R-type funct3=0b{funct3:03b}, funct7=0b{funct7:07b}"
            )
        return Instruction(word, name, "R", rd=rd, rs1=rs1, rs2=rs2)

    if opcode == 0x13:
        if funct3 == 0x1:
            if bits(word, 31, 26) != 0:
                raise DecodeError("unsupported SLLI upper immediate bits")
            return Instruction(word, "slli", "I", rd=rd, rs1=rs1,
                               imm=bits(word, 25, 20))
        if funct3 == 0x5:
            upper = bits(word, 31, 26)
            if upper == 0x00:
                name = "srli"
            elif upper == 0x10:
                name = "srai"
            else:
                raise DecodeError("unsupported right-shift immediate encoding")
            return Instruction(word, name, "I", rd=rd, rs1=rs1,
                               imm=bits(word, 25, 20))
        name = I_OPS.get(funct3)
        if name is None:
            raise DecodeError(f"unsupported OP-IMM funct3=0b{funct3:03b}")
        return Instruction(word, name, "I", rd=rd, rs1=rs1,
                           imm=sign_extend(bits(word, 31, 20), 12))

    if opcode == 0x03:
        load = LOAD_OPS.get(funct3)
        if load is None:
            raise DecodeError(f"unsupported load funct3=0b{funct3:03b}")
        name, width, unsigned_load = load
        return Instruction(word, name, "I", rd=rd, rs1=rs1,
                           imm=sign_extend(bits(word, 31, 20), 12),
                           width=width, unsigned_load=unsigned_load)

    if opcode == 0x23:
        store = STORE_OPS.get(funct3)
        if store is None:
            raise DecodeError(f"unsupported store funct3=0b{funct3:03b}")
        immediate = (bits(word, 31, 25) << 5) | bits(word, 11, 7)
        name, width = store
        return Instruction(word, name, "S", rs1=rs1, rs2=rs2,
                           imm=sign_extend(immediate, 12), width=width)

    if opcode == 0x63:
        name = BRANCH_OPS.get(funct3)
        if name is None:
            raise DecodeError(f"unsupported branch funct3=0b{funct3:03b}")
        immediate = (
            (bits(word, 31, 31) << 12)
            | (bits(word, 7, 7) << 11)
            | (bits(word, 30, 25) << 5)
            | (bits(word, 11, 8) << 1)
        )
        return Instruction(word, name, "B", rs1=rs1, rs2=rs2,
                           imm=sign_extend(immediate, 13))

    if opcode == 0x37:
        return Instruction(word, "lui", "U", rd=rd, imm=word & 0xFFFFF000)

    if opcode == 0x17:
        return Instruction(word, "auipc", "U", rd=rd, imm=word & 0xFFFFF000)

    if opcode == 0x6F:
        immediate = (
            (bits(word, 31, 31) << 20)
            | (bits(word, 19, 12) << 12)
            | (bits(word, 20, 20) << 11)
            | (bits(word, 30, 21) << 1)
        )
        return Instruction(word, "jal", "J", rd=rd,
                           imm=sign_extend(immediate, 21))

    if opcode == 0x67:
        if funct3 != 0:
            raise DecodeError("JALR requires funct3=000")
        return Instruction(word, "jalr", "I", rd=rd, rs1=rs1,
                           imm=sign_extend(bits(word, 31, 20), 12))

    raise DecodeError(f"unsupported opcode 0b{opcode:07b} (0x{opcode:02x})")


def field_diagram(word: int) -> str:
    """Render raw common fields; decode() determines which ones have meaning."""
    opcode = bits(word, 6, 0)
    lines = [
        f"word    0x{word:08x}  {word:032b}",
        f"bits    31........25 24...20 19...15 14..12 11...7 6.....0",
        f"common  {bits(word,31,25):07b}     {bits(word,24,20):05b}   "
        f"{bits(word,19,15):05b}   {bits(word,14,12):03b}   "
        f"{bits(word,11,7):05b}  {opcode:07b}",
    ]
    try:
        insn = decode(word)
        lines.append(f"format  {insn.fmt}-type")
        lines.append(f"decode  {insn.assembly(abi=False)}")
        lines.append(f"ABI     {insn.assembly(abi=True)}")
        if insn.imm is not None:
            lines.append(f"imm     {insn.imm} (0x{insn.imm & MASK64:x} as 64-bit)")
    except DecodeError as error:
        lines.append(f"decode  ERROR: {error}")
    return "\n".join(lines)


def require_signed(value: int, width: int, what: str) -> int:
    low = -(1 << (width - 1))
    high = (1 << (width - 1)) - 1
    if not low <= value <= high:
        raise ValueError(f"{what} {value} does not fit signed {width} bits")
    return value & ((1 << width) - 1)


def encode_r(name: str, rd: int, rs1: int, rs2: int) -> int:
    matches = [(key, op) for key, op in R_OPS.items() if op == name]
    if not matches:
        raise ValueError(f"unknown R operation {name}")
    (funct3, funct7), _ = matches[0]
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | 0x33


def encode_i(name: str, rd: int, rs1: int, immediate: int) -> int:
    if name in {"slli", "srli", "srai"}:
        if not 0 <= immediate <= 63:
            raise ValueError("RV64 shift amount must be in 0..63")
        funct3 = 0x1 if name == "slli" else 0x5
        upper = 0x10 if name == "srai" else 0x00
        return (upper << 26) | (immediate << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | 0x13
    matches = [funct3 for funct3, op in I_OPS.items() if op == name]
    if not matches:
        raise ValueError(f"unknown I operation {name}")
    imm = require_signed(immediate, 12, "immediate")
    return (imm << 20) | (rs1 << 15) | (matches[0] << 12) | (rd << 7) | 0x13


def encode_load(name: str, rd: int, rs1: int, immediate: int) -> int:
    matches = [funct3 for funct3, entry in LOAD_OPS.items() if entry[0] == name]
    if not matches:
        raise ValueError(f"unknown load {name}")
    imm = require_signed(immediate, 12, "load offset")
    return (imm << 20) | (rs1 << 15) | (matches[0] << 12) | (rd << 7) | 0x03


def encode_store(name: str, rs2: int, rs1: int, immediate: int) -> int:
    matches = [funct3 for funct3, entry in STORE_OPS.items() if entry[0] == name]
    if not matches:
        raise ValueError(f"unknown store {name}")
    imm = require_signed(immediate, 12, "store offset")
    return (
        (bits(imm, 11, 5) << 25)
        | (rs2 << 20)
        | (rs1 << 15)
        | (matches[0] << 12)
        | (bits(imm, 4, 0) << 7)
        | 0x23
    )


def encode_branch(name: str, rs1: int, rs2: int, immediate: int) -> int:
    matches = [funct3 for funct3, op in BRANCH_OPS.items() if op == name]
    if not matches:
        raise ValueError(f"unknown branch {name}")
    if immediate & 1:
        raise ValueError("branch offset must be a multiple of 2")
    imm = require_signed(immediate, 13, "branch offset")
    return (
        (bits(imm, 12, 12) << 31)
        | (bits(imm, 10, 5) << 25)
        | (rs2 << 20)
        | (rs1 << 15)
        | (matches[0] << 12)
        | (bits(imm, 4, 1) << 8)
        | (bits(imm, 11, 11) << 7)
        | 0x63
    )


def encode_u(name: str, rd: int, upper20: int) -> int:
    if not 0 <= upper20 < (1 << 20):
        raise ValueError("upper immediate must fit 20 bits")
    opcode = {"lui": 0x37, "auipc": 0x17}.get(name)
    if opcode is None:
        raise ValueError(f"unknown U operation {name}")
    return (upper20 << 12) | (rd << 7) | opcode


def encode_jal(rd: int, immediate: int) -> int:
    if immediate & 1:
        raise ValueError("JAL offset must be a multiple of 2")
    imm = require_signed(immediate, 21, "JAL offset")
    return (
        (bits(imm, 20, 20) << 31)
        | (bits(imm, 10, 1) << 21)
        | (bits(imm, 11, 11) << 20)
        | (bits(imm, 19, 12) << 12)
        | (rd << 7)
        | 0x6F
    )


class Machine:
    """Small architectural-state model, not a cycle-accurate CPU."""

    def __init__(self) -> None:
        self.regs: List[int] = [0] * 32
        self.pc = 0
        self.memory: MutableMapping[int, int] = {}

    def write_reg(self, number: int, value: int) -> None:
        if number != 0:
            self.regs[number] = value & MASK64
        self.regs[0] = 0

    def load(self, address: int, width: int, unsigned: bool) -> int:
        value = sum(self.memory.get(address + i, 0) << (8 * i) for i in range(width))
        return value if unsigned else sign_extend(value, width * 8)

    def store(self, address: int, width: int, value: int) -> None:
        for i in range(width):
            self.memory[address + i] = (value >> (8 * i)) & 0xFF

    def changed_registers(self, before: Sequence[int]) -> str:
        changes = []
        for number, (old, new) in enumerate(zip(before, self.regs)):
            if old != new:
                changes.append(
                    f"{reg_name(number)}:0x{old:016x}→0x{new:016x}"
                )
        return ", ".join(changes) if changes else "(no register write)"

    def step(self, insn: Instruction) -> str:
        before_pc = self.pc
        next_pc = (self.pc + 4) & MASK64
        memory_note = ""
        a = self.regs[insn.rs1] if insn.rs1 is not None else 0
        b = self.regs[insn.rs2] if insn.rs2 is not None else 0
        name = insn.name

        if insn.fmt == "R":
            shamt = b & 0x3F
            results = {
                "add": lambda: a + b,
                "sub": lambda: a - b,
                "sll": lambda: a << shamt,
                "slt": lambda: int(signed64(a) < signed64(b)),
                "sltu": lambda: int(a < b),
                "xor": lambda: a ^ b,
                "srl": lambda: a >> shamt,
                "sra": lambda: signed64(a) >> shamt,
                "or": lambda: a | b,
                "and": lambda: a & b,
            }
            self.write_reg(insn.rd, results[name]())
        elif name in I_OPS.values() or name in {"slli", "srli", "srai"}:
            imm = insn.imm
            if name == "addi":
                result = a + imm
            elif name == "slti":
                result = int(signed64(a) < imm)
            elif name == "sltiu":
                result = int(a < (imm & MASK64))
            elif name == "xori":
                result = a ^ (imm & MASK64)
            elif name == "ori":
                result = a | (imm & MASK64)
            elif name == "andi":
                result = a & (imm & MASK64)
            elif name == "slli":
                result = a << imm
            elif name == "srli":
                result = a >> imm
            else:
                result = signed64(a) >> imm
            self.write_reg(insn.rd, result)
        elif name in {"lb", "lh", "lw", "ld", "lbu", "lhu", "lwu"}:
            address = (a + insn.imm) & MASK64
            value = self.load(address, insn.width, insn.unsigned_load)
            self.write_reg(insn.rd, value)
            memory_note = f" load[{address:#x}]={value & MASK64:#x}"
        elif name in {"sb", "sh", "sw", "sd"}:
            address = (a + insn.imm) & MASK64
            self.store(address, insn.width, b)
            memory_note = f" store[{address:#x}]={b & ((1 << (8 * insn.width))-1):#x}"
        elif name in BRANCH_OPS.values():
            taken = {
                "beq": a == b,
                "bne": a != b,
                "blt": signed64(a) < signed64(b),
                "bge": signed64(a) >= signed64(b),
                "bltu": a < b,
                "bgeu": a >= b,
            }[name]
            if taken:
                next_pc = (self.pc + insn.imm) & MASK64
            memory_note = f" branch={'taken' if taken else 'not-taken'}"
        elif name == "lui":
            self.write_reg(insn.rd, sign_extend(insn.imm, 32))
        elif name == "auipc":
            self.write_reg(insn.rd, self.pc + sign_extend(insn.imm, 32))
        elif name == "jal":
            self.write_reg(insn.rd, self.pc + 4)
            next_pc = (self.pc + insn.imm) & MASK64
        elif name == "jalr":
            target = (a + insn.imm) & ~1
            self.write_reg(insn.rd, self.pc + 4)
            next_pc = target & MASK64
        else:
            raise NotImplementedError(name)

        self.pc = next_pc
        self.regs[0] = 0
        return f"pc:{before_pc:#06x}→{self.pc:#06x}{memory_note}"


def sum_program() -> tuple[Dict[int, int], Machine]:
    """Return a hand-encoded loop that sums four signed 64-bit elements."""
    words = [
        encode_i("addi", 5, 0, 0),       # t0 = index = 0
        encode_i("addi", 6, 0, 0),       # t1 = sum = 0
        encode_i("slli", 7, 5, 3),       # t2 = index * 8
        encode_r("add", 28, 10, 7),      # t3 = base + byte offset
        encode_load("ld", 29, 28, 0),    # t4 = array[index]
        encode_r("add", 6, 6, 29),       # sum += t4
        encode_i("addi", 5, 5, 1),       # index++
        encode_branch("blt", 5, 11, -20),# while index < count
        encode_i("addi", 10, 6, 0),      # a0 = sum
    ]
    machine = Machine()
    machine.regs[10] = 0x100
    machine.regs[11] = 4
    for index, value in enumerate((7, -2, 13, 5)):
        machine.store(0x100 + index * 8, 8, value & MASK64)
    return {index * 4: word for index, word in enumerate(words)}, machine


def branch_program() -> tuple[Dict[int, int], Machine]:
    """Small trace that distinguishes signed BLT from unsigned BLTU."""
    words = [
        encode_i("addi", 5, 0, -1),       # t0 = -1 / UINT64_MAX
        encode_i("addi", 6, 0, 1),        # t1 = 1
        encode_branch("blt", 5, 6, 8),    # signed: taken
        encode_i("addi", 10, 0, 99),      # skipped
        encode_branch("bltu", 5, 6, 8),   # unsigned: not taken
        encode_i("addi", 10, 0, 7),       # executed
        encode_i("addi", 11, 0, 8),
    ]
    return {index * 4: word for index, word in enumerate(words)}, Machine()


def run_trace(name: str, limit: int) -> None:
    if name == "sum":
        program, machine = sum_program()
        expectation = "Expected final a0: 23"
    else:
        program, machine = branch_program()
        expectation = "Expected final a0: 7, a1: 8"

    print(f"Trace: {name} — {expectation}")
    print("Predict each next PC and changed register before reading the right side.\n")
    for step_number in range(limit):
        if machine.pc not in program:
            print(f"STOP: no instruction mapped at pc={machine.pc:#x}")
            break
        word = program[machine.pc]
        insn = decode(word)
        before = machine.regs.copy()
        old_pc = machine.pc
        note = machine.step(insn)
        print(
            f"{step_number:02d}  {old_pc:04x}: {word:08x}  "
            f"{insn.assembly():<27} | {note}; {machine.changed_registers(before)}"
        )
    else:
        print(f"STOP: step limit {limit} reached")
    print(f"\nFinal: a0={signed64(machine.regs[10])} ({machine.regs[10]:#x}), "
          f"a1={signed64(machine.regs[11])} ({machine.regs[11]:#x}), "
          f"x0={machine.regs[0]}")


EXERCISES = (
    ("A", encode_i("addi", 10, 10, -1),
     "Decode it. Which source and destination are the same? How is -1 represented?"),
    ("B", encode_store("sd", 11, 2, 24),
     "Decode it. Which field is split, and why is rd not a destination here?"),
    ("C", encode_branch("bne", 5, 0, -12),
     "Decode it. Add the immediate to PC=0x40 and state the taken target."),
    ("D", encode_u("lui", 6, 0xABCDE),
     "Decode it. What 64-bit value is written after RV64 sign extension?"),
    ("E", encode_jal(1, 16),
     "Decode it at PC=0x100. What goes to ra, and what is the next PC?"),
)


def show_exercises(answers: bool) -> None:
    print("Decode exercises — cover the answer lines before predicting.\n")
    for label, word, question in EXERCISES:
        print(f"{label}. 0x{word:08x}  ({word:032b})")
        print(f"   {question}")
        if answers:
            insn = decode(word)
            print(f"   ANSWER: {insn.assembly(abi=False)}  |  {insn.assembly()}")
        print()

    print("Trace exercises:")
    print("  1. Run `trace branch`; explain why BLT is taken but BLTU is not.")
    print("  2. Run `trace sum --limit 8`, predict state, then raise the limit.")
    print("  3. For the sum loop, list exactly which instructions access memory.")
    print("  4. Change array values in sum_program(), predict a0, and rerun.")
    print("  5. Change the write to x0 in the experiment below; explain the result.")


def selftest() -> None:
    samples = [
        encode_r("add", 10, 11, 12),
        encode_r("sub", 5, 6, 7),
        encode_i("addi", 10, 10, -2048),
        encode_i("srai", 5, 6, 63),
        encode_load("ld", 9, 2, -16),
        encode_store("sd", 9, 2, 24),
        encode_branch("beq", 5, 6, -4096),
        encode_branch("bgeu", 5, 6, 4094),
        encode_u("lui", 3, 0xFFFFF),
        encode_u("auipc", 3, 0x12345),
        encode_jal(1, -1048576),
        encode_jal(1, 1048574),
    ]
    for word in samples:
        insn = decode(word)
        assert 0 <= word <= 0xFFFFFFFF
        assert insn.word == word

    program, machine = sum_program()
    for _ in range(100):
        if machine.pc not in program:
            break
        machine.step(decode(program[machine.pc]))
    assert signed64(machine.regs[10]) == 23
    assert machine.regs[0] == 0

    machine.write_reg(0, 0xDEADBEEF)
    assert machine.regs[0] == 0

    # Verify immediate reconstruction at important negative and positive edges.
    assert decode(encode_branch("bne", 1, 2, -4096)).imm == -4096
    assert decode(encode_branch("bne", 1, 2, 4094)).imm == 4094
    assert decode(encode_jal(1, -1048576)).imm == -1048576
    assert decode(encode_jal(1, 1048574)).imm == 1048574
    assert decode(encode_store("sd", 9, 2, -2048)).imm == -2048

    print(f"PASS: decoded {len(samples)} representative instructions")
    print("PASS: sum trace returned 23")
    print("PASS: x0 discarded a write")
    print("PASS: B/J/S immediate edge reconstruction")


def demo() -> None:
    print("RV64I decoder laboratory\n")
    words = [
        encode_i("addi", 10, 10, -1),
        encode_r("add", 12, 10, 11),
        encode_load("ld", 5, 2, 16),
        encode_store("sd", 5, 2, 24),
        encode_branch("bne", 5, 0, -12),
        encode_jal(1, 16),
    ]
    for word in words:
        print(f"0x{word:08x}  {decode(word).assembly():<26}  [{decode(word).fmt}]")
    print("\nUse `fields WORD` to expose bits, `trace sum` to execute, and")
    print("`exercise` for prediction prompts. Run `selftest` after edits.")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Decode and trace a teaching subset of RV64I without external tools."
    )
    sub = parser.add_subparsers(dest="command")

    decode_parser = sub.add_parser("decode", help="decode one or more 32-bit words")
    decode_parser.add_argument("words", nargs="+", type=parse_word)
    decode_parser.add_argument("--x-registers", action="store_true",
                               help="show xN names instead of ABI names")

    fields_parser = sub.add_parser("fields", help="show raw fields and reconstructed immediate")
    fields_parser.add_argument("word", type=parse_word)

    trace_parser = sub.add_parser("trace", help="trace an included program")
    trace_parser.add_argument("program", choices=("sum", "branch"), nargs="?", default="sum")
    trace_parser.add_argument("--limit", type=int, default=100)

    exercise_parser = sub.add_parser("exercise", help="show decode and trace exercises")
    exercise_parser.add_argument("--answers", action="store_true")

    sub.add_parser("demo", help="show representative decodes")
    sub.add_parser("selftest", help="validate decoder and simulator invariants")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    command = args.command or "demo"

    try:
        if command == "decode":
            for word in args.words:
                try:
                    insn = decode(word)
                    print(f"0x{word:08x}  {insn.assembly(abi=not args.x_registers)}")
                except DecodeError as error:
                    print(f"0x{word:08x}  ERROR: {error}")
        elif command == "fields":
            print(field_diagram(args.word))
        elif command == "trace":
            if args.limit <= 0:
                parser.error("--limit must be positive")
            run_trace(args.program, args.limit)
        elif command == "exercise":
            show_exercises(args.answers)
        elif command == "selftest":
            selftest()
        else:
            demo()
    except (DecodeError, ValueError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
