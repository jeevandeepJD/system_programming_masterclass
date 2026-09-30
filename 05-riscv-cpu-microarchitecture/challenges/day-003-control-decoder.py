#!/usr/bin/env python3
"""Control decoder lab for a small RV32I implementation subset.

This is an executable hardware-design table, not a Python tutorial.  It
separates instruction-field extraction, immediate generation, and control
decoding.  Unsupported or reserved encodings produce safe, disabled control.

Run:
    python3 05-riscv-cpu-microarchitecture/challenges/day-003-control-decoder.py
    python3 05-riscv-cpu-microarchitecture/challenges/day-003-control-decoder.py --word 0x004080b3
    python3 05-riscv-cpu-microarchitecture/challenges/day-003-control-decoder.py --selftest
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass


MASK32 = 0xFFFF_FFFF


def bits(value: int, high: int, low: int) -> int:
    return (value >> low) & ((1 << (high - low + 1)) - 1)


def sign_extend(value: int, width: int) -> int:
    sign = 1 << (width - 1)
    return (value & (sign - 1)) - (value & sign)


@dataclass(frozen=True)
class Fields:
    opcode: int
    rd: int
    funct3: int
    rs1: int
    rs2: int
    funct7: int


@dataclass(frozen=True)
class Control:
    name: str = "ILLEGAL"
    imm_kind: str = "-"
    alu_op: str = "ADD"
    alu_a_sel: str = "RS1"
    alu_b_sel: str = "RS2"
    wb_sel: str = "-"
    pc_sel: str = "PC+4"
    reg_write: bool = False
    mem_read: bool = False
    mem_write: bool = False
    branch: str = "-"
    illegal: bool = True


def fields(word: int) -> Fields:
    return Fields(
        opcode=bits(word, 6, 0),
        rd=bits(word, 11, 7),
        funct3=bits(word, 14, 12),
        rs1=bits(word, 19, 15),
        rs2=bits(word, 24, 20),
        funct7=bits(word, 31, 25),
    )


def immediate(word: int, kind: str) -> int | None:
    """The immediate generator is wiring/sign extension selected by format."""
    if kind == "I":
        return sign_extend(bits(word, 31, 20), 12)
    if kind == "S":
        raw = (bits(word, 31, 25) << 5) | bits(word, 11, 7)
        return sign_extend(raw, 12)
    if kind == "B":
        raw = (
            (bits(word, 31, 31) << 12)
            | (bits(word, 7, 7) << 11)
            | (bits(word, 30, 25) << 5)
            | (bits(word, 11, 8) << 1)
        )
        return sign_extend(raw, 13)
    if kind == "U":
        return word & 0xFFFFF000
    if kind == "J":
        raw = (
            (bits(word, 31, 31) << 20)
            | (bits(word, 19, 12) << 12)
            | (bits(word, 20, 20) << 11)
            | (bits(word, 30, 21) << 1)
        )
        return sign_extend(raw, 21)
    return None


def decode(word: int) -> Control:
    """Produce control lines; the default is deliberately side-effect-free."""
    f = fields(word)

    if f.opcode == 0x33:  # OP
        operation = {
            (0b000, 0b0000000): ("ADD", "ADD"),
            (0b000, 0b0100000): ("SUB", "SUB"),
            (0b111, 0b0000000): ("AND", "AND"),
            (0b110, 0b0000000): ("OR", "OR"),
        }.get((f.funct3, f.funct7))
        if operation:
            name, alu_op = operation
            return Control(
                name=name, alu_op=alu_op, wb_sel="ALU",
                reg_write=True, illegal=False
            )

    elif f.opcode == 0x13 and f.funct3 == 0b000:  # ADDI only
        return Control(
            name="ADDI", imm_kind="I", alu_op="ADD", alu_b_sel="IMM",
            wb_sel="ALU", reg_write=True, illegal=False
        )

    elif f.opcode == 0x03 and f.funct3 == 0b010:  # LW only
        return Control(
            name="LW", imm_kind="I", alu_op="ADD", alu_b_sel="IMM",
            wb_sel="MEM", reg_write=True, mem_read=True, illegal=False
        )

    elif f.opcode == 0x23 and f.funct3 == 0b010:  # SW only
        return Control(
            name="SW", imm_kind="S", alu_op="ADD", alu_b_sel="IMM",
            mem_write=True, illegal=False
        )

    elif f.opcode == 0x63 and f.funct3 in (0b000, 0b001):  # BEQ/BNE
        name = "BEQ" if f.funct3 == 0 else "BNE"
        return Control(
            name=name, imm_kind="B", alu_op="SUB", pc_sel="BRANCH",
            branch=name, illegal=False
        )

    # No write enable survives an unrecognized opcode/funct combination.
    return Control()


def bool01(value: bool) -> int:
    return int(value)


def describe(word: int) -> str:
    f = fields(word)
    c = decode(word)
    imm = immediate(word, c.imm_kind)
    return (
        f"0x{word:08x}  op={f.opcode:07b} rd=x{f.rd:<2} rs1=x{f.rs1:<2} "
        f"rs2=x{f.rs2:<2} f3={f.funct3:03b} f7={f.funct7:07b}\n"
        f"  {c.name:<7} imm={str(imm):>5} ALU={c.alu_op:<3} "
        f"A={c.alu_a_sel:<3} B={c.alu_b_sel:<3} WB={c.wb_sel:<3} "
        f"PC={c.pc_sel:<6} RegW={bool01(c.reg_write)} "
        f"MemR={bool01(c.mem_read)} MemW={bool01(c.mem_write)} "
        f"branch={c.branch:<3} illegal={bool01(c.illegal)}"
    )


SAMPLES = (
    0x004080B3,  # add x1,x1,x4
    0x404080B3,  # sub x1,x1,x4
    0xFFF18193,  # addi x3,x3,-1
    0x00012203,  # lw x4,0(x2)
    0x00112023,  # sw x1,0(x2)
    0xFE0198E3,  # bne x3,x0,-16
    0x00013203,  # illegal here: LD-like funct3 in RV32I LOAD family
    0x024080B3,  # illegal: unsupported/reserved ADD-family funct7
    0x00000000,  # illegal major opcode
)


def selftest() -> None:
    expected = ("ADD", "SUB", "ADDI", "LW", "SW", "BNE")
    assert tuple(decode(word).name for word in SAMPLES[:6]) == expected
    assert immediate(SAMPLES[2], "I") == -1
    assert immediate(SAMPLES[5], "B") == -16
    assert fields(SAMPLES[0]).rs1 == 1 and fields(SAMPLES[0]).rs2 == 4

    for word in SAMPLES[6:]:
        c = decode(word)
        assert c.illegal
        assert not (c.reg_write or c.mem_read or c.mem_write)

    print("PASS: legal subset decodes and immediates")
    print("PASS: illegal encodings disable architectural writes")


def parse_word(text: str) -> int:
    value = int(text, 0)
    if not 0 <= value <= MASK32:
        raise argparse.ArgumentTypeError("word must fit in 32 bits")
    return value


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--word", type=parse_word, action="append")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        selftest()
        return

    for word in args.word or SAMPLES:
        print(describe(word))


if __name__ == "__main__":
    main()
