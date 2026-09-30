#!/usr/bin/env python3
"""Encode, decode, and trace the documented Day 1 RV32I teaching subset.

This is an architectural state-transition model, not a cycle-accurate CPU.
Unsupported or misaligned operations stop with a lab diagnostic; that is not
an implementation of the RISC-V exception mechanism.

Try:
  python3 day-001-rv32i-subset-trace.py demo
  python3 day-001-rv32i-subset-trace.py decode 0x007302b3 0xfe0318e3
  python3 day-001-rv32i-subset-trace.py trace --limit 5
  python3 day-001-rv32i-subset-trace.py trace
  python3 day-001-rv32i-subset-trace.py selftest
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from typing import Callable, Sequence


MASK32 = 0xFFFF_FFFF

R_OPS = {
    (0b000, 0b0000000): "add",
    (0b000, 0b0100000): "sub",
    (0b111, 0b0000000): "and",
    (0b110, 0b0000000): "or",
    (0b100, 0b0000000): "xor",
    (0b010, 0b0000000): "slt",
    (0b011, 0b0000000): "sltu",
}
I_OPS = {
    0b000: "addi",
    0b111: "andi",
    0b110: "ori",
    0b100: "xori",
    0b010: "slti",
    0b011: "sltiu",
}
BRANCH_OPS = {
    0b000: "beq",
    0b001: "bne",
    0b100: "blt",
    0b101: "bge",
    0b110: "bltu",
    0b111: "bgeu",
}


class LabError(ValueError):
    """Input lies outside the documented laboratory preconditions."""


def bits(value: int, high: int, low: int) -> int:
    return (value >> low) & ((1 << (high - low + 1)) - 1)


def sign_extend(value: int, width: int) -> int:
    sign = 1 << (width - 1)
    return (value & (sign - 1)) - (value & sign)


def signed32(value: int) -> int:
    return sign_extend(value & MASK32, 32)


def signed_field(value: int, width: int, name: str) -> int:
    lower = -(1 << (width - 1))
    upper = (1 << (width - 1)) - 1
    if not lower <= value <= upper:
        raise LabError(f"{name} {value} does not fit signed {width} bits")
    return value & ((1 << width) - 1)


def register(number: int) -> int:
    if not 0 <= number < 32:
        raise LabError(f"register x{number} is outside x0..x31")
    return number


@dataclass(frozen=True)
class Instruction:
    word: int
    name: str
    fmt: str
    rd: int | None = None
    rs1: int | None = None
    rs2: int | None = None
    imm: int | None = None

    def assembly(self) -> str:
        if self.fmt == "R":
            return f"{self.name} x{self.rd}, x{self.rs1}, x{self.rs2}"
        if self.name == "lw":
            return f"lw x{self.rd}, {self.imm}(x{self.rs1})"
        if self.name == "sw":
            return f"sw x{self.rs2}, {self.imm}(x{self.rs1})"
        if self.name in BRANCH_OPS.values():
            return f"{self.name} x{self.rs1}, x{self.rs2}, {self.imm:+}"
        if self.name in {"lui", "auipc"}:
            return f"{self.name} x{self.rd}, 0x{self.imm >> 12:x}"
        if self.name == "jal":
            return f"jal x{self.rd}, {self.imm:+}"
        if self.name == "jalr":
            return f"jalr x{self.rd}, {self.imm}(x{self.rs1})"
        return f"{self.name} x{self.rd}, x{self.rs1}, {self.imm}"

    def path(self) -> str:
        if self.fmt == "R":
            return f"RF[x{self.rs1}],RF[x{self.rs2}]→ALU.{self.name.upper()}→x{self.rd}"
        if self.name in I_OPS.values():
            return f"RF[x{self.rs1}],imm→ALU.{self.name.upper()}→x{self.rd}"
        if self.name == "lw":
            return f"RF[x{self.rs1}]+imm→DMEM.read→x{self.rd}"
        if self.name == "sw":
            return f"RF[x{self.rs1}]+imm→DMEM.write(RF[x{self.rs2}])"
        if self.name in BRANCH_OPS.values():
            return f"RF[x{self.rs1}],RF[x{self.rs2}]→CMP.{self.name.upper()}→PC mux"
        if self.name == "lui":
            return f"U-imm→x{self.rd}"
        if self.name == "auipc":
            return f"PC+U-imm→x{self.rd}"
        if self.name == "jal":
            return f"PC+4→x{self.rd}; PC+J-imm→PC"
        return f"PC+4→x{self.rd}; (RF[x{self.rs1}]+imm)&~1→PC"


def decode(word: int) -> Instruction:
    if not 0 <= word <= MASK32:
        raise LabError("instruction must fit in 32 bits")
    opcode = bits(word, 6, 0)
    rd = bits(word, 11, 7)
    funct3 = bits(word, 14, 12)
    rs1 = bits(word, 19, 15)
    rs2 = bits(word, 24, 20)
    funct7 = bits(word, 31, 25)

    if opcode == 0x33:
        name = R_OPS.get((funct3, funct7))
        if name is None:
            raise LabError("unsupported R-type funct3/funct7")
        return Instruction(word, name, "R", rd, rs1, rs2)
    if opcode == 0x13:
        name = I_OPS.get(funct3)
        if name is None:
            raise LabError("unsupported OP-IMM funct3")
        immediate = sign_extend(bits(word, 31, 20), 12)
        return Instruction(word, name, "I", rd, rs1, imm=immediate)
    if opcode == 0x03 and funct3 == 0b010:
        immediate = sign_extend(bits(word, 31, 20), 12)
        return Instruction(word, "lw", "I", rd, rs1, imm=immediate)
    if opcode == 0x23 and funct3 == 0b010:
        immediate = (bits(word, 31, 25) << 5) | bits(word, 11, 7)
        return Instruction(
            word, "sw", "S", rs1=rs1, rs2=rs2,
            imm=sign_extend(immediate, 12),
        )
    if opcode == 0x63:
        name = BRANCH_OPS.get(funct3)
        if name is None:
            raise LabError("unsupported BRANCH funct3")
        immediate = (
            (bits(word, 31, 31) << 12)
            | (bits(word, 7, 7) << 11)
            | (bits(word, 30, 25) << 5)
            | (bits(word, 11, 8) << 1)
        )
        return Instruction(
            word, name, "B", rs1=rs1, rs2=rs2,
            imm=sign_extend(immediate, 13),
        )
    if opcode in {0x37, 0x17}:
        name = "lui" if opcode == 0x37 else "auipc"
        return Instruction(word, name, "U", rd=rd, imm=word & 0xFFFFF000)
    if opcode == 0x6F:
        immediate = (
            (bits(word, 31, 31) << 20)
            | (bits(word, 19, 12) << 12)
            | (bits(word, 20, 20) << 11)
            | (bits(word, 30, 21) << 1)
        )
        return Instruction(
            word, "jal", "J", rd=rd,
            imm=sign_extend(immediate, 21),
        )
    if opcode == 0x67 and funct3 == 0:
        immediate = sign_extend(bits(word, 31, 20), 12)
        return Instruction(word, "jalr", "I", rd, rs1, imm=immediate)
    raise LabError(f"unsupported instruction encoding 0x{word:08x}")


def encode_r(name: str, rd: int, rs1: int, rs2: int) -> int:
    matches = [fields for fields, operation in R_OPS.items() if operation == name]
    if not matches:
        raise LabError(f"unknown R operation {name}")
    funct3, funct7 = matches[0]
    return (
        (funct7 << 25) | (register(rs2) << 20) | (register(rs1) << 15)
        | (funct3 << 12) | (register(rd) << 7) | 0x33
    )


def encode_i(name: str, rd: int, rs1: int, immediate: int) -> int:
    matches = [field for field, operation in I_OPS.items() if operation == name]
    if not matches:
        raise LabError(f"unknown immediate operation {name}")
    immediate = signed_field(immediate, 12, "immediate")
    return (
        (immediate << 20) | (register(rs1) << 15) | (matches[0] << 12)
        | (register(rd) << 7) | 0x13
    )


def encode_lw(rd: int, rs1: int, immediate: int) -> int:
    immediate = signed_field(immediate, 12, "load offset")
    return (
        (immediate << 20) | (register(rs1) << 15) | (0b010 << 12)
        | (register(rd) << 7) | 0x03
    )


def encode_sw(rs2: int, rs1: int, immediate: int) -> int:
    immediate = signed_field(immediate, 12, "store offset")
    return (
        (bits(immediate, 11, 5) << 25) | (register(rs2) << 20)
        | (register(rs1) << 15) | (0b010 << 12)
        | (bits(immediate, 4, 0) << 7) | 0x23
    )


def encode_branch(name: str, rs1: int, rs2: int, immediate: int) -> int:
    matches = [field for field, operation in BRANCH_OPS.items() if operation == name]
    if not matches:
        raise LabError(f"unknown branch {name}")
    if immediate & 1:
        raise LabError("branch offset must be even")
    immediate = signed_field(immediate, 13, "branch offset")
    return (
        (bits(immediate, 12, 12) << 31)
        | (bits(immediate, 10, 5) << 25)
        | (register(rs2) << 20) | (register(rs1) << 15)
        | (matches[0] << 12) | (bits(immediate, 4, 1) << 8)
        | (bits(immediate, 11, 11) << 7) | 0x63
    )


def encode_u(name: str, rd: int, upper20: int) -> int:
    if not 0 <= upper20 < (1 << 20):
        raise LabError("upper immediate must fit 20 bits")
    opcode = {"lui": 0x37, "auipc": 0x17}.get(name)
    if opcode is None:
        raise LabError(f"unknown U operation {name}")
    return (upper20 << 12) | (register(rd) << 7) | opcode


def encode_jal(rd: int, immediate: int) -> int:
    if immediate & 1:
        raise LabError("JAL offset must be even")
    immediate = signed_field(immediate, 21, "JAL offset")
    return (
        (bits(immediate, 20, 20) << 31)
        | (bits(immediate, 10, 1) << 21)
        | (bits(immediate, 11, 11) << 20)
        | (bits(immediate, 19, 12) << 12)
        | (register(rd) << 7) | 0x6F
    )


def encode_jalr(rd: int, rs1: int, immediate: int) -> int:
    immediate = signed_field(immediate, 12, "JALR offset")
    return (
        (immediate << 20) | (register(rs1) << 15)
        | (register(rd) << 7) | 0x67
    )


class Machine:
    def __init__(self) -> None:
        self.pc = 0
        self.regs = [0] * 32
        self.memory: dict[int, int] = {}

    def write_register(self, number: int | None, value: int) -> None:
        assert number is not None
        if number != 0:
            self.regs[number] = value & MASK32
        self.regs[0] = 0

    def load_word(self, address: int) -> int:
        if address & 3:
            raise LabError(f"misaligned LW address 0x{address:08x}")
        return sum(self.memory.get(address + index, 0) << (8 * index)
                   for index in range(4))

    def store_word(self, address: int, value: int) -> None:
        if address & 3:
            raise LabError(f"misaligned SW address 0x{address:08x}")
        for index in range(4):
            self.memory[address + index] = (value >> (8 * index)) & 0xFF

    def step(self, instruction: Instruction) -> str:
        old_pc = self.pc
        a = self.regs[instruction.rs1] if instruction.rs1 is not None else 0
        b = self.regs[instruction.rs2] if instruction.rs2 is not None else 0
        immediate = instruction.imm or 0
        next_pc = (old_pc + 4) & MASK32
        note = f"PC 0x{old_pc:08x}→0x{next_pc:08x}"
        name = instruction.name

        operations: dict[str, Callable[[], int]] = {
            "add": lambda: a + b,
            "sub": lambda: a - b,
            "and": lambda: a & b,
            "or": lambda: a | b,
            "xor": lambda: a ^ b,
            "slt": lambda: int(signed32(a) < signed32(b)),
            "sltu": lambda: int(a < b),
            "addi": lambda: a + immediate,
            "andi": lambda: a & (immediate & MASK32),
            "ori": lambda: a | (immediate & MASK32),
            "xori": lambda: a ^ (immediate & MASK32),
            "slti": lambda: int(signed32(a) < immediate),
            "sltiu": lambda: int(a < (immediate & MASK32)),
        }
        if name in operations:
            value = operations[name]()
            self.write_register(instruction.rd, value)
            note += f"; x{instruction.rd}=0x{value & MASK32:08x}"
        elif name == "lw":
            address = (a + immediate) & MASK32
            value = self.load_word(address)
            self.write_register(instruction.rd, value)
            note += f"; load[0x{address:08x}]→x{instruction.rd}=0x{value:08x}"
        elif name == "sw":
            address = (a + immediate) & MASK32
            self.store_word(address, b)
            note += f"; x{instruction.rs2}→store[0x{address:08x}]"
        elif name in BRANCH_OPS.values():
            conditions = {
                "beq": a == b,
                "bne": a != b,
                "blt": signed32(a) < signed32(b),
                "bge": signed32(a) >= signed32(b),
                "bltu": a < b,
                "bgeu": a >= b,
            }
            taken = conditions[name]
            if taken:
                next_pc = (old_pc + immediate) & MASK32
            note = f"branch {'taken' if taken else 'not-taken'}; PC→0x{next_pc:08x}"
        elif name == "lui":
            self.write_register(instruction.rd, immediate)
            note += f"; x{instruction.rd}=0x{immediate:08x}"
        elif name == "auipc":
            value = old_pc + immediate
            self.write_register(instruction.rd, value)
            note += f"; x{instruction.rd}=0x{value & MASK32:08x}"
        elif name == "jal":
            self.write_register(instruction.rd, old_pc + 4)
            next_pc = (old_pc + immediate) & MASK32
            note = f"x{instruction.rd}=0x{old_pc + 4:08x}; PC→0x{next_pc:08x}"
        elif name == "jalr":
            self.write_register(instruction.rd, old_pc + 4)
            next_pc = (a + immediate) & MASK32 & ~1
            note = f"x{instruction.rd}=0x{old_pc + 4:08x}; PC→0x{next_pc:08x}"
        else:
            raise AssertionError(name)

        self.pc = next_pc
        self.regs[0] = 0
        return note


def sum_program() -> tuple[dict[int, int], Machine]:
    words = [
        encode_i("addi", 5, 0, 0x100),     # pointer
        encode_i("addi", 6, 0, 4),         # count
        encode_i("addi", 7, 0, 0),         # sum
        encode_lw(28, 5, 0),               # loop: load word
        encode_r("add", 7, 7, 28),         # sum += word
        encode_i("addi", 5, 5, 4),         # pointer += 4
        encode_i("addi", 6, 6, -1),        # count -= 1
        encode_branch("bne", 6, 0, -16),   # loop while count != 0
        encode_sw(7, 0, 0),                # memory[0] = sum
    ]
    machine = Machine()
    for index, value in enumerate((7, -2, 13, 5)):
        machine.store_word(0x100 + 4 * index, value & MASK32)
    return {4 * index: word for index, word in enumerate(words)}, machine


def run_trace(limit: int) -> None:
    program, machine = sum_program()
    print("Predict the path and next PC before reading each right-hand side.")
    for step_number in range(limit):
        word = program.get(machine.pc)
        if word is None:
            print(f"STOP: no instruction at PC 0x{machine.pc:08x}")
            break
        instruction = decode(word)
        old_pc = machine.pc
        result = machine.step(instruction)
        print(f"{step_number:02d} {old_pc:04x}: {word:08x}  "
              f"{instruction.assembly():<24} | {instruction.path()}")
        print(f"   {result}")
    else:
        print(f"STOP: step limit {limit} reached")
    stored_sum = machine.load_word(0)
    print(f"Final x7={signed32(machine.regs[7])}; memory[0]={signed32(stored_sum)}")


def selftest() -> None:
    words = [
        encode_r("add", 5, 6, 7),
        encode_r("sub", 31, 0, 1),
        encode_i("sltiu", 8, 9, -1),
        encode_lw(10, 2, -16),
        encode_sw(10, 2, 20),
        encode_branch("blt", 5, 6, -4096),
        encode_branch("bgeu", 5, 6, 4094),
        encode_u("lui", 3, 0xABCDE),
        encode_u("auipc", 4, 0x12345),
        encode_jal(1, -1_048_576),
        encode_jal(1, 1_048_574),
        encode_jalr(0, 1, 0),
    ]
    for word in words:
        assert decode(word).word == word

    assert decode(encode_branch("bne", 1, 2, -16)).imm == -16
    assert decode(encode_sw(3, 4, -2048)).imm == -2048
    assert decode(encode_jal(1, 1_048_574)).imm == 1_048_574

    program, machine = sum_program()
    for _ in range(100):
        word = program.get(machine.pc)
        if word is None:
            break
        machine.step(decode(word))
    assert signed32(machine.regs[7]) == 23
    assert machine.load_word(0) == 23
    machine.write_register(0, 0xDEADBEEF)
    assert machine.regs[0] == 0

    print(f"PASS: {len(words)} representative encode/decode checks")
    print("PASS: B/S/J immediate reconstruction")
    print("PASS: loop sum is 23 in x7 and memory[0]")
    print("PASS: x0 discards writes")


def parse_word(text: str) -> int:
    value = int(text, 0)
    if not 0 <= value <= MASK32:
        raise argparse.ArgumentTypeError("word must fit in 32 bits")
    return value


def demo() -> None:
    program, _ = sum_program()
    print("Day 1 RV32I subset — encoded loop\n")
    for pc, word in program.items():
        instruction = decode(word)
        print(f"{pc:04x}: {word:08x}  {instruction.assembly():<24} "
              f"[{instruction.fmt}]")
    print("\nUse `trace --limit 5`, predict the next state, then run `trace`.")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command")
    subparsers.add_parser("demo")
    decode_parser = subparsers.add_parser("decode")
    decode_parser.add_argument("words", nargs="+", type=parse_word)
    trace_parser = subparsers.add_parser("trace")
    trace_parser.add_argument("--limit", type=int, default=100)
    subparsers.add_parser("selftest")
    args = parser.parse_args(argv)

    try:
        if args.command in {None, "demo"}:
            demo()
        elif args.command == "decode":
            for word in args.words:
                instruction = decode(word)
                print(f"0x{word:08x}  {instruction.assembly()}")
                print(f"  format={instruction.fmt}; path={instruction.path()}")
        elif args.command == "trace":
            if args.limit <= 0:
                parser.error("--limit must be positive")
            run_trace(args.limit)
        else:
            selftest()
    except LabError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
