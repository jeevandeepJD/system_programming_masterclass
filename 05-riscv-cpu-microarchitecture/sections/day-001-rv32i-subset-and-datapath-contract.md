# Day 1 — An RV32I Subset and Its Datapath Contract

**Target time:** approximately 2–3 hours

- Choose the architectural contract: 35–45 minutes
- Decode the required formats: 40–50 minutes
- Derive datapath and control requirements: 45–55 minutes
- Run and explain the Python trace lab: 30–40 minutes

> A program says `add x5, x6, x7`. What must be true after that instruction
> retires, and which parts of the machine are free to differ?

## Why this day exists

Systems software depends on an architectural machine: registers, addresses,
instruction effects, and precisely defined control flow. A CPU designer must
build some implementation that preserves that contract. Those are related
objects, but they are not the same object.

```text
official RISC-V ISA
        ↓ choose a documented teaching subset
architectural state transition
        ↓ derive required data movement
datapath + control contract
        ↓ choose one implementation
single-cycle teaching CPU
```

Today stops at that implementation contract. We will not begin with HDL and
hope a CPU emerges. First we decide exactly which instructions exist, what
state they can change, which values each datapath must produce, and what this
small design deliberately does not implement.

This is necessary CPU-design depth for systems programming. It is not an HDL
specialization. The payoff is being able to look at a faulting instruction,
kernel register frame, emulator trace, or disassembly and ask: which
architectural transition was requested, and what machinery had to realize it?

By the end, you should be able to:

- distinguish official RV32I behavior from our subset and implementation;
- name the architectural state visible to this model;
- decode the R, I, S, B, U, and J formats used here;
- derive register-file, ALU, immediate, memory, and next-PC requirements;
- state the assumptions that make a single-cycle diagram possible;
- identify deferred instructions and exception behavior without pretending
  they do not exist;
- trace instruction bits into control decisions and an architectural update.

---

## 1. Three contracts, not one

Keep these statements separate:

1. **The RISC-V unprivileged ISA specification** defines instruction
   encodings and architectural behavior.
2. **Our pedagogical subset** chooses a finite set of those instructions and
   narrows the execution environment.
3. **Our single-cycle implementation** chooses one way to realize the subset.

For example, the official ISA says what `ADD` computes. It does not require
one physical adder, one clock cycle, a single-cycle datapath, combinational
memory, or any particular SystemVerilog module boundary.

```text
ISA-visible fact:
    ADD writes (rs1 + rs2) modulo 2^32 to rd

implementation choices:
    ripple or carry-lookahead adder?
    shared address/ALU hardware or separate adders?
    one cycle or several?
    pipelined?
    physical register renaming?
```

All conforming implementations must produce the required architectural
result. Their internal paths, timing, and speculative state may differ.

### Why RV32I?

`RV32I` has 32-bit integer registers and a 32-bit base integer address space.
It gives us regular 32-bit instruction forms, explicit register arithmetic,
loads/stores, and PC-relative control flow without requiring multiplication,
floating point, vectors, atomics, or compressed instructions.

That does not make this exact teaching CPU a practical Linux platform. A real
RISC-V Linux system needs privilege, exceptions, interrupts, address
translation, atomics and other platform facilities beyond this subset.
RV32I is the clean ISA boundary on which to learn the datapath.

---

## 2. The chosen subset

The following list is the complete instruction contract for the CPU we will
derive:

```text
register ALU     ADD SUB AND OR XOR SLT SLTU
immediate ALU    ADDI ANDI ORI XORI SLTI SLTIU
memory           LW SW
branches         BEQ BNE BLT BGE BLTU BGEU
upper immediate  LUI AUIPC
control transfer JAL JALR
```

This is not a newly invented ISA. Every listed instruction is an official
RV32I instruction. The selection is ours.

### Why these instructions?

The subset is small enough to trace but broad enough to expose the important
datapath choices:

- `ADD`/`ADDI` need ordinary arithmetic and occur constantly in compiled code.
- `SUB` and comparisons force us to distinguish arithmetic results from
  branch decisions.
- bitwise operations implement masks used in kernels and drivers.
- signed and unsigned comparisons show that bits carry no permanent type.
- `LW`/`SW` create the register–address–memory boundary.
- branches select between `pc+4` and `pc+imm`.
- `LUI` and `AUIPC` help construct constants and PC-relative addresses.
- `JAL` and `JALR` expose calls, returns, link values, and indirect control.

`JALR` computes `(rs1 + sign_extend(imm)) & ~1` as its target. Clearing bit
zero is official behavior, not a convenience invented by our datapath.

### Explicitly outside the subset

```text
byte/halfword loads and stores
shifts
FENCE and memory-ordering machinery
ECALL and EBREAK
CSRs, privilege levels, traps, and trap return
interrupts
M multiplication/division
A atomics
F/D floating point
V vectors
C compressed instructions
```

These omissions matter. Without `ECALL`, privilege, and trap state, this CPU
cannot enter a kernel through the normal system-call path. Without atomics
and a memory-ordering implementation it cannot support realistic
multi-hart kernel synchronization. We defer those mechanisms; we do not
redefine them as no-ops.

---

## 3. Architectural state

For this subset, the programmer-visible state is:

```text
pc               address of the current instruction
x0..x31          32 integer registers, each 32 bits
memory bytes      instruction and data contents addressed by the program
```

`x0` always reads as zero. An attempted write to `x0` has no architectural
effect. The other registers have no guaranteed ISA-defined reset values.

The architectural transition for one successfully completed instruction can
be written:

```text
(old pc, old registers, old memory)
                ↓ instruction semantics
(new pc, new registers, new memory)
```

Most instructions change one integer register and the PC. A store changes
memory and the PC. A branch changes only the PC. A write with `rd=x0` changes
no retained integer register.

### Architectural state is not every internal bit

An implementation may have:

- an instruction register;
- control flops;
- pipeline registers;
- cache tags;
- branch-predictor tables;
- physical registers;
- outstanding memory-request state.

Those are microarchitectural state. Software does not get to demand their
particular layout merely because they exist. Conversely, a context switch or
virtual CPU must preserve the architectural state relevant to the enabled
ISA and privilege extensions.

### A precise observation boundary

Our teaching model treats an instruction as completing at the active clock
edge:

```text
before edge   old architectural state drives combinational paths
at edge       register file, PC, or data memory accepts selected updates
after edge    the next instruction observes the new architectural state
```

This makes the state transition visible. It is not a claim that the official
ISA defines an instruction as one clock cycle.

---

## 4. Six formats supply the needed fields

All instructions in this teaching subset are 32 bits. Because the `C`
extension is absent, valid instruction addresses are four-byte aligned for
this implementation.

Common field positions are:

```text
opcode  instruction[6:0]
rd      instruction[11:7]
funct3  instruction[14:12]
rs1     instruction[19:15]
rs2     instruction[24:20]
funct7  instruction[31:25]
```

The format determines which fields have meaning.

### R-type — register ALU

```text
31       25 24    20 19    15 14  12 11     7 6       0
+----------+--------+--------+------+---------+---------+
| funct7   | rs2    | rs1    |funct3| rd      | opcode  |
+----------+--------+--------+------+---------+---------+
```

`ADD`, `SUB`, `AND`, `OR`, `XOR`, `SLT`, and `SLTU` read two registers and
write one. `opcode`, `funct3`, and sometimes `funct7` select the operation.

### I-type — immediate ALU, load, and JALR

```text
31                20 19    15 14  12 11     7 6       0
+-------------------+--------+------+---------+---------+
| imm[11:0]         | rs1    |funct3| rd      | opcode  |
+-------------------+--------+------+---------+---------+
```

The 12-bit immediate is sign-extended for this subset. The second ALU input
comes from that immediate for `ADDI`, `LW`, and `JALR`, even though their
final destinations differ.

### S-type — store

```text
31       25 24    20 19    15 14  12 11      7 6       0
+----------+--------+--------+------+----------+---------+
|imm[11:5] | rs2    | rs1    |funct3| imm[4:0] | opcode  |
+----------+--------+--------+------+----------+---------+
```

`SW` reads `rs1` as the base and `rs2` as the data. It has no `rd`, so those
five bit positions help carry the address displacement.

### B-type — conditional branch

```text
imm[12]    = instruction[31]
imm[10:5]  = instruction[30:25]
imm[4:1]   = instruction[11:8]
imm[11]    = instruction[7]
imm[0]     = 0
```

The reconstructed 13-bit value is sign-extended and added to the current
instruction's PC. Both source registers feed comparison logic. There is no
`rd`.

### U-type — upper immediate

```text
31                                      12 11     7 6:0
+-----------------------------------------+---------+------+
| imm[31:12]                              | rd      |opcode|
+-----------------------------------------+---------+------+
```

`LUI` writes `imm[31:12] << 12`. `AUIPC` adds that same 32-bit immediate to
the current PC.

### J-type — JAL

```text
imm[20]    = instruction[31]
imm[10:1]  = instruction[30:21]
imm[11]    = instruction[20]
imm[19:12] = instruction[19:12]
imm[0]     = 0
```

`JAL` writes `pc+4` to `rd` and selects `pc+imm` as the next PC. It therefore
needs two results in the same instruction: a link value and a control-flow
target.

### Pause and derive

For each format, mark:

1. which registers are read;
2. whether a register is written;
3. how the immediate is reconstructed;
4. what can become the next PC;
5. whether memory is read or written.

That table is already most of a decoder specification.

---

## 5. Derive the datapath from required values

Do not memorize a canonical CPU picture. Begin with values the instructions
need:

```text
current PC
instruction bits
rs1 value
rs2 value
decoded immediate
ALU result
memory load data
pc + 4
pc + immediate
rs1 + immediate, with bit 0 cleared for JALR target
```

A minimal block view is:

```text
                         ┌──────── immediate generator
instruction ── decoder ──┤
     │                   └──────── control
     ├─ rs1/rs2/rd
     ▼
register file ── operands ──▶ ALU / compare ──▶ data-memory address
     ▲                           │                       │
     │                           │                       └─ load data
     └──────── writeback mux ◀───┴──────────────────────────
                  ▲
                  └──────── pc+4 / upper immediate

PC register ──▶ instruction memory
    │
    ├─▶ pc+4
    └─▶ pc+imm
          │
next-PC mux ◀──── branch decision / JAL / JALR
```

The drawing can use separate adders for `pc+4` and branch targets while the
main ALU computes a data result. Sharing one adder would require multiple
phases or a longer combinational arrangement. Separate adders are a
single-cycle implementation choice, not an ISA rule.

### Control contract

One useful controller interface is:

```text
alu_op          ADD/SUB/AND/OR/XOR/SLT/SLTU/pass
alu_src_imm     choose rs2 or decoded immediate
imm_kind        I/S/B/U/J reconstruction
reg_write       permit rd update
writeback_sel   ALU / load data / pc+4 / U-immediate / pc+U-immediate
mem_read        request LW
mem_write       request SW
branch_kind     none/EQ/NE/LT/GE/LTU/GEU
next_pc_sel     sequential/branch/JAL/JALR
```

Exact signal names are implementation choices. The contract matters:
illegal encodings must not accidentally write a register or memory.

### Instruction-to-path table

| Family | Register reads | ALU/compare use | Writeback | Next PC |
|---|---|---|---|---|
| R ALU | `rs1`, `rs2` | selected operation | ALU → `rd` | `pc+4` |
| I ALU | `rs1` | `rs1 op imm` | ALU → `rd` | `pc+4` |
| `LW` | `rs1` | address = `rs1+imm` | memory → `rd` | `pc+4` |
| `SW` | `rs1`, `rs2` | address = `rs1+imm` | memory write | `pc+4` |
| branch | `rs1`, `rs2` | selected compare | none | `pc+imm` or `pc+4` |
| `LUI` | none | upper immediate | immediate → `rd` | `pc+4` |
| `AUIPC` | none | `pc+upper_imm` | result → `rd` | `pc+4` |
| `JAL` | none | target = `pc+imm` | `pc+4` → `rd` | target |
| `JALR` | `rs1` | target = `(rs1+imm)&~1` | `pc+4` → `rd` | target |

The table is an implementation review tool. If `SW` accidentally asserts
`reg_write`, or a branch uses the wrong immediate kind, the architectural
transition is wrong even if the circuit is electrically valid.

---

## 6. The single-cycle timing fiction—useful and explicit

Our first CPU assumes one long cycle:

```text
PC output
 → instruction memory
 → decode and register-file read
 → immediate/ALU/compare
 → optional data-memory read
 → writeback selection
 → register-file and PC capture
```

To make that model executable, we assume:

- instruction memory supplies a word combinationally from the current PC;
- data memory supplies `LW` data combinationally;
- register-file reads are combinational;
- register-file writes occur at the active edge;
- `SW` memory update occurs at the active edge;
- the PC updates at the active edge;
- all combinational paths settle before that edge;
- instruction and data access are shown as distinct ports/memories;
- every implemented instruction completes in one cycle.

These assumptions are pedagogical, not ordinary high-performance memory
interfaces. Real SRAMs, caches, buses, and DRAM have clocked protocols and
variable latency. A single-cycle CPU's period must accommodate its longest
path, often a load:

```text
PC → instruction fetch → decode/register read
   → address ALU → data memory → writeback mux → register setup
```

Even a simple `ADD` waits for that same global period. The inefficiency
motivates multicycle and pipelined designs later.

### Harvard-looking ports do not redefine software memory

Separate instruction and data interfaces avoid two accesses competing for
one port in the same cycle. This is a microarchitectural convenience.
Whether the programmer sees one address space, whether code is writable, and
how caches remain coherent are separate platform questions.

---

## 7. Exceptions are deferred, not erased

Official RISC-V systems define exception behavior in conjunction with the ISA,
privileged architecture, and execution environment. This teaching CPU does
not yet contain trap CSRs, privilege levels, a trap vector, or restart logic.

We therefore impose a lab precondition:

```text
fetched instruction is in the documented subset
PC is four-byte aligned
LW/SW effective address is four-byte aligned
instruction and data addresses are mapped by the lab
```

Outside that precondition, the Python model reports an error and stops. That
is **lab behavior**, not the architectural definition of an illegal
instruction or access fault.

Deferred cases include:

- illegal or unsupported instruction encoding;
- instruction-address misalignment;
- load/store address misalignment;
- instruction or data access fault;
- environment call and breakpoint;
- interrupts.

A later CPU must suppress inappropriate partial updates, record a cause and
faulting PC, select a handler, and support the required return semantics. A
kernel relies on that precision. Silently treating an unsupported opcode as
`NOP` would corrupt the software-visible contract.

---

## 8. Python decode-and-trace laboratory

File:

```text
05-riscv-cpu-microarchitecture/challenges/day-001-rv32i-subset-trace.py
```

No cross-compiler or third-party package is required.

From the repository root:

```bash
python3 05-riscv-cpu-microarchitecture/challenges/day-001-rv32i-subset-trace.py demo
python3 05-riscv-cpu-microarchitecture/challenges/day-001-rv32i-subset-trace.py trace
python3 05-riscv-cpu-microarchitecture/challenges/day-001-rv32i-subset-trace.py selftest
```

The trace prints, for each instruction:

```text
instruction word → decoded fields → control/path summary
                 → old PC/register values → architectural update
```

### Predict before running

The included loop sums four 32-bit words.

1. Which instruction family first reads data memory?
2. Which instruction writes data memory?
3. Which path produces the address used by `LW`?
4. Which comparison makes the loop branch?
5. Which value is fed back to the PC when the branch is taken?
6. Does the loop ever alter `x0`?

Then run only five steps:

```bash
python3 05-riscv-cpu-microarchitecture/challenges/day-001-rv32i-subset-trace.py trace --limit 5
```

Predict the sixth line before increasing the limit.

### What the lab proves

It demonstrates that the selected encodings can be decoded and that the
software model performs the documented state transitions for tested cases.
It helps review immediate reconstruction and datapath selection.

It does not prove:

- cycle-accurate hardware behavior;
- RTL correctness;
- electrical timing;
- official compliance beyond the tested subset;
- exception, privilege, or memory-ordering behavior;
- that a physical CPU uses the same internal operations.

---

## 9. Systems and kernel relevance

This small datapath explains several larger mechanisms:

- A saved kernel trap frame names architectural registers and a PC because
  those are the state software must restore.
- A disassembler reconstructs fields much like our decoder, although it
  covers many more extensions and aliases.
- QEMU can model architectural transitions without copying a target CPU's
  physical datapath.
- KVM exposes virtual architectural state while host hardware supplies a very
  different microarchitecture.
- A page fault cannot simply write a load's `rd` and then report failure; the
  architecture needs a restartable, precise boundary.
- `JALR` underlies indirect calls and returns, so target formation becomes
  relevant to control-flow integrity and exploit analysis.
- `LW`/`SW` address generation is where virtual addressing, alignment,
  permissions, caches, and device mappings later attach.

The single-cycle machine is not realistic enough to run a kernel. It is
simple enough to expose exactly where those mechanisms must connect.

---

## 10. Mastery evidence

### Explain

Without notes:

- separate official ISA, subset, and implementation;
- justify every instruction family in the subset;
- name architectural state and give one microarchitectural counterexample;
- explain why `x0` needs both read and write behavior;
- derive each possible register writeback source;
- explain why the single-cycle load path tends to set the clock period;
- state what happens in this lab and what must eventually happen in hardware
  for an unsupported instruction.

### Draw

Draw the complete Day 1 datapath. Label:

```text
PC register
instruction memory
field extraction and decoder
immediate generator
two-read/one-write register file
ALU and signed/unsigned compare
data memory
writeback mux
pc+4 and pc+immediate paths
next-PC selection
```

Use two colors: one for data values and one for control selections. Trace
`ADD`, `LW`, `SW`, `BLT`, and `JALR` across the drawing.

### Observe

Preserve:

- one decoded R instruction;
- one negative B immediate reconstructed from scattered bits;
- the complete loop trace;
- one deliberately unsupported word and the diagnostic;
- one sentence distinguishing model behavior from official exception
  behavior.

### Build

Change the loop's four data words, predict the final sum, and rerun. Then add
one `XOR` instruction to the program using the provided encoder. Before
running, write its expected instruction fields and architectural update.

### Mastery checkpoint

> Starting with one 32-bit instruction word and old architectural state,
> derive every operand, candidate result, control choice, and state update
> needed to execute it—then identify which claims came from the ISA and which
> came from our single-cycle implementation.

---

## Why? notebook

1. Why does an ISA define results but usually not cycle count?
2. Why is a subset not permission to alter an included instruction?
3. Why can `SW` reuse bit positions occupied by `rd` in an R instruction?
4. Why are B- and J-immediate bits rearranged?
5. Why does `JAL` need both `pc+4` and `pc+imm`?
6. Why does `JALR` clear target bit zero?
7. Why do signed and unsigned comparisons need the same register bits?
8. Why can separate instruction/data ports coexist with one software address
   space?
9. Why is “halt on bad opcode” not an implementation of an illegal-instruction
   exception?
10. Why does precise exception handling matter to a restartable page fault?

---

## References used selectively

- RISC-V International, *The RISC-V Instruction Set Manual, Volume I:
  Unprivileged Architecture*, RV32I base integer chapter, version 20240411:
  <https://docs.riscv.org/reference/isa/unpriv/rv32.html>
- RISC-V International, unprivileged ISA index and current ratified
  specifications:
  <https://docs.riscv.org/reference/isa/unpriv/unpriv-index.html>
- RISC-V International, *The RISC-V Instruction Set Manual, Volume II:
  Privileged Architecture*, for the exception/privilege mechanisms explicitly
  deferred here:
  <https://docs.riscv.org/reference/isa/priv/priv-index.html>
- David Money Harris and Sarah L. Harris, *Digital Design and Computer
  Architecture: RISC-V Edition*, single-cycle processor chapters, for the
  pedagogical datapath decomposition.

**Next bridge:** Day 2 makes three boxes concrete: an ALU that returns values
and comparisons, a two-read/one-write register file that enforces `x0`, and a
program-counter register whose next value comes from explicit control-flow
logic.
