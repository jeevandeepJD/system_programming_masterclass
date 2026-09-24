#!/usr/bin/env python3
"""Encode and decode the tiny 16-bit Cedar-16 teaching ISA.

Examples:
  python3 day-006-fictional-isa.py table
  python3 day-006-fictional-isa.py encode add r1 r2 r3
  python3 day-006-fictional-isa.py encode addi r1 r2 -5
  python3 day-006-fictional-isa.py decode 0x1298
  python3 day-006-fictional-isa.py repl

Encoding formats (bit 15 is the most-significant bit):

  R:  [ opcode:4 | rd:3  | rs1:3 | rs2:3 | 000:3 ]
  I:  [ opcode:4 | rd:3  | rs1:3 | signed immediate:6 ]
  B:  [ opcode:4 | rs1:3 | rs2:3 | signed PC offset:6 ]
  J:  [ opcode:4 | signed PC offset:12 ]

LOAD uses I format as "ld rd, [rs1 + imm]".
STORE uses I-shaped fields as "st rd, [rs1 + imm]", where rd is the source.
Offsets are measured in instructions, not bytes. This is a deliberate design
choice for the exercise, not a claim about x86-64 or RISC-V.
"""

from __future__ import annotations

import argparse
import shlex
import sys
from dataclasses import dataclass
from typing import Sequence


@dataclass(frozen=True)
class Instruction:
    opcode: int
    form: str
    operands: tuple[str, ...]
    description: str


INSTRUCTIONS: dict[str, Instruction] = {
    "nop": Instruction(0x0, "N", (), "no operation"),
    "add": Instruction(0x1, "R", ("rd", "rs1", "rs2"), "rd = rs1 + rs2"),
    "sub": Instruction(0x2, "R", ("rd", "rs1", "rs2"), "rd = rs1 - rs2"),
    "and": Instruction(0x3, "R", ("rd", "rs1", "rs2"), "rd = rs1 & rs2"),
    "or": Instruction(0x4, "R", ("rd", "rs1", "rs2"), "rd = rs1 | rs2"),
    "addi": Instruction(0x5, "I", ("rd", "rs1", "imm6"), "rd = rs1 + imm6"),
    "ld": Instruction(0x6, "I", ("rd", "base", "off6"), "rd = memory[base + off6]"),
    "st": Instruction(0x7, "I", ("src", "base", "off6"), "memory[base + off6] = src"),
    "beq": Instruction(0x8, "B", ("rs1", "rs2", "off6"), "branch if rs1 == rs2"),
    "jmp": Instruction(0x9, "J", ("off12",), "unconditional PC-relative jump"),
    "halt": Instruction(0xF, "N", (), "stop the fictional CPU"),
}

BY_OPCODE = {instruction.opcode: (name, instruction)
             for name, instruction in INSTRUCTIONS.items()}


class ISAError(ValueError):
    """A useful command-line error rather than a Python traceback."""


def parse_register(text: str) -> int:
    lowered = text.lower()
    if not lowered.startswith("r"):
        raise ISAError(f"expected register r0..r7, got {text!r}")
    try:
        number = int(lowered[1:], 10)
    except ValueError as error:
        raise ISAError(f"expected register r0..r7, got {text!r}") from error
    if not 0 <= number <= 7:
        raise ISAError(f"register out of range: {text!r}; Cedar-16 has r0..r7")
    return number


def parse_integer(text: str) -> int:
    try:
        return int(text, 0)
    except ValueError as error:
        raise ISAError(f"expected an integer (decimal, 0xhex, or 0bbinary), got {text!r}") from error


def encode_signed(value: int, width: int, label: str) -> int:
    minimum = -(1 << (width - 1))
    maximum = (1 << (width - 1)) - 1
    if not minimum <= value <= maximum:
        raise ISAError(f"{label} {value} does not fit signed {width} bits "
                       f"({minimum}..{maximum})")
    return value & ((1 << width) - 1)


def sign_extend(value: int, width: int) -> int:
    sign_bit = 1 << (width - 1)
    return (value ^ sign_bit) - sign_bit


def encode(tokens: Sequence[str]) -> int:
    if not tokens:
        raise ISAError("missing mnemonic")
    mnemonic = tokens[0].lower()
    instruction = INSTRUCTIONS.get(mnemonic)
    if instruction is None:
        raise ISAError(f"unknown mnemonic {mnemonic!r}; use 'table' to list instructions")

    operands = list(tokens[1:])
    expected = len(instruction.operands)
    if len(operands) != expected:
        syntax = " ".join((mnemonic, *instruction.operands))
        raise ISAError(f"{mnemonic} expects {expected} operand(s): {syntax}")

    word = instruction.opcode << 12
    if instruction.form == "N":
        return word
    if instruction.form == "R":
        rd, rs1, rs2 = map(parse_register, operands)
        return word | (rd << 9) | (rs1 << 6) | (rs2 << 3)
    if instruction.form == "I":
        rd_or_src = parse_register(operands[0])
        rs1_or_base = parse_register(operands[1])
        immediate = encode_signed(parse_integer(operands[2]), 6, "immediate")
        return word | (rd_or_src << 9) | (rs1_or_base << 6) | immediate
    if instruction.form == "B":
        rs1 = parse_register(operands[0])
        rs2 = parse_register(operands[1])
        offset = encode_signed(parse_integer(operands[2]), 6, "branch offset")
        return word | (rs1 << 9) | (rs2 << 6) | offset
    if instruction.form == "J":
        offset = encode_signed(parse_integer(operands[0]), 12, "jump offset")
        return word | offset
    raise AssertionError(f"unhandled format {instruction.form}")


def decode(word: int) -> str:
    if not 0 <= word <= 0xFFFF:
        raise ISAError(f"instruction word 0x{word:x} does not fit 16 bits")

    opcode = (word >> 12) & 0xF
    entry = BY_OPCODE.get(opcode)
    if entry is None:
        return f".word 0x{word:04x}  ; invalid/reserved opcode 0x{opcode:x}"

    mnemonic, instruction = entry
    if instruction.form == "N":
        if word & 0x0FFF:
            return f".word 0x{word:04x}  ; non-canonical {mnemonic} encoding"
        return mnemonic
    if instruction.form == "R":
        rd = (word >> 9) & 0x7
        rs1 = (word >> 6) & 0x7
        rs2 = (word >> 3) & 0x7
        reserved = word & 0x7
        suffix = "" if reserved == 0 else "  ; reserved low bits are nonzero"
        return f"{mnemonic} r{rd} r{rs1} r{rs2}{suffix}"
    if instruction.form == "I":
        rd_or_src = (word >> 9) & 0x7
        rs1_or_base = (word >> 6) & 0x7
        immediate = sign_extend(word & 0x3F, 6)
        return f"{mnemonic} r{rd_or_src} r{rs1_or_base} {immediate}"
    if instruction.form == "B":
        rs1 = (word >> 9) & 0x7
        rs2 = (word >> 6) & 0x7
        offset = sign_extend(word & 0x3F, 6)
        return f"{mnemonic} r{rs1} r{rs2} {offset}"
    if instruction.form == "J":
        offset = sign_extend(word & 0x0FFF, 12)
        return f"{mnemonic} {offset}"
    raise AssertionError(f"unhandled format {instruction.form}")


def format_word(word: int) -> str:
    bits = f"{word:016b}"
    return f"0x{word:04x}  {bits[:4]} {bits[4:7]} {bits[7:10]} {bits[10:]}"


def print_table() -> None:
    print("Cedar-16 instruction set")
    print("opcode  format  syntax                    meaning")
    for mnemonic, instruction in INSTRUCTIONS.items():
        syntax = " ".join((mnemonic, *instruction.operands))
        print(f"  0x{instruction.opcode:x}      {instruction.form}     "
              f"{syntax:<25} {instruction.description}")


def execute_command(tokens: Sequence[str]) -> bool:
    if not tokens:
        return True
    command = tokens[0].lower()
    if command in {"quit", "exit"}:
        return False
    if command in {"help", "?"}:
        print("Commands: table | encode MNEMONIC OPERANDS... | decode WORD | quit")
        return True
    if command == "table":
        print_table()
        return True
    if command == "encode":
        word = encode(tokens[1:])
        print(format_word(word))
        print(f"decode check: {decode(word)}")
        return True
    if command == "decode":
        if len(tokens) != 2:
            raise ISAError("decode expects one 16-bit word")
        word = parse_integer(tokens[1])
        print(format_word(word))
        print(decode(word))
        return True
    raise ISAError(f"unknown command {command!r}; try 'help'")


def repl() -> int:
    print("Cedar-16 encoder/decoder. Type 'help' or 'quit'.")
    while True:
        try:
            line = input("cedar16> ")
        except EOFError:
            print()
            return 0
        try:
            if not execute_command(shlex.split(line)):
                return 0
        except (ISAError, ValueError) as error:
            print(f"error: {error}", file=sys.stderr)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Cedar-16 ISA encoder/decoder")
    subparsers = parser.add_subparsers(dest="command")
    subparsers.add_parser("table", help="show the instruction set")
    encode_parser = subparsers.add_parser("encode", help="encode one instruction")
    encode_parser.add_argument("instruction", nargs=argparse.REMAINDER)
    decode_parser = subparsers.add_parser("decode", help="decode one 16-bit word")
    decode_parser.add_argument("word")
    subparsers.add_parser("repl", help="start the interactive prompt")
    return parser


def main() -> int:
    parser = build_parser()
    arguments = parser.parse_args()
    if arguments.command is None or arguments.command == "repl":
        return repl()
    try:
        if arguments.command == "table":
            print_table()
        elif arguments.command == "encode":
            word = encode(arguments.instruction)
            print(format_word(word))
            print(f"decode check: {decode(word)}")
        elif arguments.command == "decode":
            word = parse_integer(arguments.word)
            print(format_word(word))
            print(decode(word))
    except ISAError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
