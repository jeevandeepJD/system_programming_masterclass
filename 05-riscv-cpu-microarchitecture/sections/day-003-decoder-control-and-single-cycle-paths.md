# Day 3 — Decoder, Control, and Single-Cycle Paths

**Target time:** approximately 2–3 hours

- Fields, immediates, and decode: 40–50 minutes
- Control signals and datapath steering: 45–55 minutes
- Critical-path reasoning: 25–35 minutes
- Decoder laboratory and evidence: 45–55 minutes

> An instruction word does not “tell the ALU what to do.” Which concrete
> wires must change before that word can update a register, memory, or the PC?

## Why this day exists

Systems programmers usually meet an instruction as assembly text or a
disassembler line. A processor receives bits. Between those bits and an
architectural state change sit field extraction, legality checks, an
immediate generator, control logic, muxes, and write enables.

That machinery explains practical facts:

- why a malformed or unsupported encoding must not silently store data;
- why adding an ISA operation changes decode and datapath requirements;
- why a simple-looking load can set the clock period of a single-cycle core;
- why QEMU's decode code resembles, but is not timed like, hardware decode;
- why “the CPU executes `lw`” is shorthand for coordinated state movement.

Today uses a deliberately small RV32I subset:

```text
ADD SUB AND OR | ADDI | LW SW | BEQ BNE
```

This is not a Python lesson and not a complete RISC-V implementation.

By the end, you should be able to trace:

```text
instruction fields
  → legality and operation decode
  → immediate construction
  → ALU operation and mux selections
  → register/memory enables
  → next-PC selection
```

---

## 1. Control and datapath are roles, not separate universes

The **datapath** carries and transforms instruction data:

```text
PC → instruction memory → register-file operands → ALU
                                      ↘ data memory ↗
                                      → writeback mux → register file
```

The **control** interprets the instruction and steers that machinery:

```text
ALU operation
ALU operand-B select
writeback select
next-PC select
register write enable
memory read/write enables
branch condition kind
illegal-instruction indication
```

This partition is useful for reasoning, but it is not a physical wall.
`funct3` wires may run into ALU-control gates; register addresses go directly
to the register file; branch comparison can live in the ALU or separate
logic. A synthesized implementation may optimize across source-level module
boundaries.

Think of control signals as a protocol inside the core. They must be
consistent. Selecting memory writeback while not reading memory is suspicious;
asserting both memory write and register write for this subset is almost
certainly a decoder bug.

---

## 2. Stable fields begin the decode

For the ordinary 32-bit base formats:

```text
opcode  instruction[6:0]
rd      instruction[11:7]
funct3  instruction[14:12]
rs1     instruction[19:15]
rs2     instruction[24:20]
funct7  instruction[31:25]
```

The physical wires can expose all six slices in parallel. Their *meaning*
depends on the format. In `SW`, bits 11:7 are immediate bits, not a
destination register. In `ADDI`, bits 24:20 belong to the immediate even
though an unconditional field extractor can label that slice `rs2`.

That distinction is important when reading traces:

> Extraction says where bits are. Decode says whether those bits have a role.

The seven-bit major opcode first identifies a family. `funct3` and sometimes
`funct7` then select an operation within that family. Checking only the major
opcode is insufficient. For example, not every `LOAD`-family `funct3` is
`LW`, and not every `OP`-family `funct7` is `ADD`.

### A two-level view

Many textbook designs describe:

```text
main decode(opcode) → broad controls + ALUOp category
ALU decode(ALUOp, funct3, funct7) → exact ALU function
```

That division avoids one giant truth table and reflects reusable operation
families. The lab returns the final control bundle directly, but its lookup
still performs both conceptual levels. Source organization is not a timing
claim; synthesis may flatten or restructure the logic.

### Architecture comparison — instruction boundaries and decode work

This lab can index one aligned 32-bit RV32I word before decoding it. With the
RISC-V `C` extension, the front end must also distinguish 16-bit and 32-bit
instruction lengths. AArch64 A64 keeps a fixed 32-bit width. x86-64 must find
boundaries in a variable-length byte stream and decode prefixes, opcode maps,
addressing forms, and possible memory operands.

These are **ISA** parsing obligations. They do not prescribe one decoder
stage or imply that one family must execute faster. Implementations of all
three may use parallel decoders, queues, caches of decoded work, instruction
fusion, or internal operations unlike the architectural instruction. The
safe-enable rule remains universal: however decode is organized
**microarchitecturally**, an illegal or faulting instruction must not acquire
unintended architectural effects. See
[Architecture Comparison for Systems Programmers](../../references/ARCHITECTURE_COMPARISON.md).

---

## 3. The immediate generator is format-dependent wiring

RV32I does not place every immediate in one contiguous location. One
combinational block can build several candidates and select one according to
the decoded format.

```text
I: sext(inst[31:20])

S: sext({inst[31:25], inst[11:7]})

B: sext({inst[31], inst[7], inst[30:25], inst[11:8], 1'b0})
```

The B immediate's low bit is zero because branch displacements are encoded
in two-byte units. For the base-only core here, valid instruction addresses
are four-byte aligned, but the encoding was designed to coexist with
16-bit instructions.

For:

```asm
bne x3, x0, -16
```

the lab word is `0xfe0198e3`. The immediate generator reconstructs `-16`;
the branch target is:

```text
current branch PC + (-16)
```

It is not `PC+4-16`, and it is not an absolute address.

Sign extension is wiring in hardware: upper output bits replicate the encoded
sign bit. Calling it “free” is too strong—it still contributes routing and
fanout—but it need not be a sequential arithmetic procedure.

---

## 4. A compact control contract

Use these conceptual signals:

```text
ALUOp      ADD, SUB, AND, OR
ALUBSel    RS2 or IMM
WBSel      ALU, MEM, or unused
PCSel      PC+4 or branch candidate
RegWrite   permit register-file state update
MemRead    request data-memory read
MemWrite   permit data-memory state update
Branch     BEQ, BNE, or none
Illegal    encoding is not implemented/legal here
```

For the lesson subset:

```text
ADD   ALU=ADD  B=RS2 WB=ALU RegWrite=1
ADDI  ALU=ADD  B=IMM WB=ALU RegWrite=1
LW    ALU=ADD  B=IMM WB=MEM RegWrite=1 MemRead=1
SW    ALU=ADD  B=IMM        MemWrite=1
BEQ   ALU=SUB  B=RS2 PC=branch-if-equal
```

`LW` and `SW` use the ALU as an address adder:

```text
effective address = register[rs1] + sign-extended immediate
```

The same datapath resource used by `ADD` can therefore serve address
generation. Sharing saves hardware, but muxes and longer paths have costs.

### Mux selects answer “which candidate?”

A mux does not move data in software sequence. Its candidates exist as
combinational signals, and select inputs determine which reaches the output.

For `LW`:

```text
ALU A mux:       rs1 value
ALU B mux:       immediate
writeback mux:   data-memory value
next-PC mux:     PC+4
```

For a taken `BNE`:

```text
ALU inputs:      rs1 and rs2 for comparison
writeback mux:   irrelevant; RegWrite=0
next-PC mux:     PC + branch immediate
```

An irrelevant select should not be confused with permission. Even if the
writeback mux happens to show a value during `SW`, `RegWrite=0` prevents a
register update.

### Enables are the final safety boundary

At the active clock edge, architectural state changes only where an enable
permits it:

```text
if RegWrite: registers[rd] ← writeback value, except x0
if MemWrite: memory[address] ← store data
PC ← selected next PC
```

This is why illegal decode must fail closed. A useful safe default is:

```text
Illegal=1, RegWrite=0, MemRead=0, MemWrite=0
```

A real RISC-V core normally raises an illegal-instruction exception rather
than merely stopping. The teaching decoder has no privilege or trap
machinery, so it reports illegality while proving that write enables remain
low.

---

## 5. One instruction through the complete path

Trace:

```asm
lw x4, 0(x2)
```

1. The PC addresses instruction memory.
2. `0x00012203` arrives as the instruction word.
3. `opcode=0000011` identifies `LOAD`; `funct3=010` selects `LW`.
4. `rs1=2`, `rd=4`, and the I immediate is zero.
5. The register file reads `x2`.
6. `ALUBSel=IMM` chooses zero as the ALU's second operand.
7. `ALUOp=ADD` forms the effective address.
8. `MemRead=1` requests the word at that address.
9. `WBSel=MEM` chooses loaded data.
10. `RegWrite=1` allows `x4` to capture it at the edge.
11. The normal next-PC selection chooses `PC+4`.

All combinational steps must settle early enough for the destination
register's setup requirement. The numbered explanation is causal, not a list
of separate clock cycles.

---

## 6. The single-cycle critical path

“Single-cycle” means each instruction completes its architectural state
transition in one clock period. It does **not** mean the clock period is one
nanosecond, every instruction has equal physical delay, or memory responds
instantly.

A simplified load path is:

```text
PC register clock-to-Q
 → instruction-memory access
 → decode + register-file read + immediate selection
 → ALU address calculation
 → data-memory access
 → writeback mux
 → destination-register setup
```

The minimum clock period must cover the worst enabled register-to-register
path, plus clocking uncertainty. The load often dominates a textbook
single-cycle design because it crosses two modeled memories and the ALU.
An `ADD` does not need the data-memory access, but it still waits for the same
global clock edge.

A branch has its own competition:

```text
PC → instruction decode → register compare → taken decision ┐
PC → immediate generation → target addition                ├→ next-PC mux
PC → PC+4                                                  ┘
```

The select and selected candidate must both arrive in time.

Realistic memories complicate the picture. SRAMs may be synchronous;
caches can miss; external memory can take hundreds of cycles. A practical
core therefore uses multicycle control, pipelines, caches, handshakes, or
some combination. The single-cycle model is valuable because it exposes
dependencies, not because it is a competitive implementation template.

Carry this path forward in two steps:

- [Day 5](day-005-pipelining-and-throughput.md) places fetch and data-memory
  work in pipeline stages, making latency and stage boundaries explicit.
- [Day 7](day-007-cache-tlb-mmu-and-end-to-end-task-trace.md) replaces the
  assumed combinational response with request/response backpressure, cache
  hit or miss handling, line fill, and a lower-memory return path.

In that realistic timing model, decode still requests `LW`, but `RegWrite`
is permission for eventual completion, not permission to capture nonexistent
data at the next edge. The core must retain the load's destination and
control state while the memory hierarchy responds, suppress writeback on a
fault, and commit only when valid data returns. Exact cycle counts are
microarchitectural; the load result or exception remains architectural.

---

## 7. Laboratory — interrogate the decoder

Use:

```text
05-riscv-cpu-microarchitecture/challenges/day-003-control-decoder.py
```

It requires only Python 3's standard library.

Run the checks:

```bash
python3 05-riscv-cpu-microarchitecture/challenges/day-003-control-decoder.py \
  --selftest
```

Then print the included legal and illegal cases:

```bash
python3 05-riscv-cpu-microarchitecture/challenges/day-003-control-decoder.py
```

### Predict before reading output

For each word, write:

1. opcode family;
2. meaningful register fields;
3. immediate kind and value;
4. ALU operation and operand selections;
5. writeback source;
6. register and memory enables;
7. next-PC source;
8. whether the complete encoding is legal in this subset.

Inspect one branch alone:

```bash
python3 05-riscv-cpu-microarchitecture/challenges/day-003-control-decoder.py \
  --word 0xfe0198e3
```

Explain why its printed `rd` slice is not a destination and why `RegW=0`.

### Illegal-encoding experiment

Compare:

```bash
python3 05-riscv-cpu-microarchitecture/challenges/day-003-control-decoder.py \
  --word 0x004080b3 \
  --word 0x024080b3
```

The words share the OP major opcode and several fields, but the second has an
unsupported `funct7`. Record the write enables for both. This is direct
evidence that decoding the major opcode alone is unsafe.

### One bounded extension

Add `XOR`:

```text
opcode=0110011 funct3=100 funct7=0000000
```

Before editing:

- predict every control output;
- hand-encode or obtain one 32-bit test word;
- add a positive self-test;
- retain a nearby illegal `funct7` test.

Do not add execution behavior today. The task is to extend the control
contract without broadening the lab into language practice.

---

## 8. Evidence and mastery

Preserve:

- the two `PASS` lines from `--selftest`;
- one legal R-type decode;
- one negative immediate reconstruction;
- one load and one store control bundle;
- one branch target immediate;
- two illegal encodings with all architectural write enables low;
- a hand-drawn load critical path.

Explain without notes:

1. Why can fields be physically extracted before their roles are known?
2. Why does `SW` use bits 11:7 without writing `rd`?
3. Why do `LW` and `SW` select an immediate at the ALU B mux?
4. Why are ALU selection and write enable distinct controls?
5. Why must legality include `funct` fields, not only the major opcode?
6. Why can a load determine the clock period for an `ADD`?
7. Which path decides a branch target, and which path decides whether to use
   it?

### Mastery checkpoint

Starting with `0x00012203`, trace every value and control decision from the
instruction word to the edge that updates `x4`. Then identify the additional
mechanism required if the access is misaligned, unmapped, or denied.

The missing mechanism is an exception path. The tiny decoder does not model
one; a production core must suppress ordinary completion, record a cause and
faulting PC, and redirect to a trap handler according to its privilege
architecture.

---

## Why? notebook

1. Why are register fields kept in stable positions across several formats?
2. Why are immediate bits rearranged instead of shifting an instruction word
   procedurally?
3. Why does a branch need both a comparison result and a target candidate?
4. Why can a mux output toggle even when its destination write enable is low?
5. Why is “default all writes off” a valuable illegal-decode policy?
6. Why is a Python lookup table evidence about logic behavior but not delay?
7. Why does hardware decode happen concurrently with other combinational
   work even when a diagram is read left to right?
8. Why does a single-cycle core make fast instructions wait for the longest
   path?
9. Why does adding an instruction affect verification even if no new datapath
   unit is needed?
10. Why would QEMU need legality and immediate rules but not this core's clock
    period?

---

## References used selectively

- RISC-V International, *The RISC-V Instruction Set Manual, Volume I:
  Unprivileged Architecture*, “RV32I Base Integer Instruction Set,” for base
  formats, immediates, integer operations, loads/stores, branches, and illegal
  instruction behavior:
  <https://docs.riscv.org/reference/isa/unpriv/rv32.html>
- RISC-V International, unprivileged ISA specification index and ratified
  extension status:
  <https://docs.riscv.org/reference/isa/unpriv/unpriv-index.html>
- RISC-V International, *The RISC-V Instruction Set Manual, Volume II:
  Privileged Architecture*, for the real exception/trap mechanisms omitted
  by the lab:
  <https://docs.riscv.org/reference/isa/priv/priv-index.html>
- David Money Harris and Sarah L. Harris, *Digital Design and Computer
  Architecture: RISC-V Edition*, single-cycle processor and control chapters,
  for the standard datapath/control decomposition and timing argument.
- QEMU documentation, system emulation and TCG overview, for the later bridge
  from architectural decode to software translation:
  <https://www.qemu.org/docs/master/devel/tcg.html>

**Next bridge:** connect this decoder to PC state, registers, memory, and an
edge-by-edge trace so a hand-encoded loop becomes a complete, observable
architectural execution.
