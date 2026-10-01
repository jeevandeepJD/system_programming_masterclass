# Day 2 — ALU, Register File, and Program Counter

**Target time:** approximately 2–3 hours

- ALU results and comparisons: 35–45 minutes
- Register-file read/write contract and `x0`: 40–50 minutes
- Program-counter state and next-PC logic: 35–45 minutes
- Build, simulate, and explain one small RTL lab: 40–50 minutes

> If a register file returns operands continuously but accepts a result only
> at a clock edge, what exactly happens during one instruction cycle?

## Why this day exists

Yesterday derived boxes from the RV32I subset. Today makes three of them
precise:

```text
register file supplies operands
        ↓
ALU returns a 32-bit result and comparisons
        ↓
writeback becomes architectural state at an edge

PC register supplies current instruction address
        ↓
next-PC logic chooses sequential or redirected address
        ↓
PC becomes new architectural state at an edge
```

The important lesson is not SystemVerilog syntax. It is the boundary between
combinational values that respond during a cycle and state that changes at a
clock edge.

By the end, you should be able to:

- specify modulo-32-bit ALU behavior;
- distinguish signed from unsigned less-than on identical input bits;
- explain why RV32I uses comparison results and branch conditions rather than
  a general architectural flags register;
- explain two combinational register-file reads and one synchronous write as
  a design convention, not an ISA mandate;
- enforce `x0` on reads and writes;
- separate the current PC register from combinational next-PC candidates;
- validate a small ALU/register-file RTL model with Icarus and Verilator;
- state what the lab proves and what remains for a complete CPU.

---

## 1. The ALU transforms bit patterns

For the Day 1 subset, the ALU needs:

```text
ADD   a + b modulo 2^32
SUB   a - b modulo 2^32
AND   bitwise a & b
OR    bitwise a | b
XOR   bitwise a ^ b
SLT   signed(a) < signed(b)  ? 1 : 0
SLTU  unsigned(a) < unsigned(b) ? 1 : 0
```

The ALU receives 32-bit patterns. The selected operation determines their
interpretation.

```text
a = 0xffffffff
b = 0x00000001

ADD   → 0x00000000
SLT   → 0x00000001    because signed -1 < 1
SLTU  → 0x00000000    because 4294967295 < 1 is false
```

No register carries a permanent signed/unsigned tag. The control signal
selecting `SLT` or `SLTU` gives the comparator its meaning.

### Modulo arithmetic is the RV32I result

`ADD`, `ADDI`, and `SUB` ignore arithmetic overflow in the sense that no
integer-overflow exception is raised. The low 32 bits are written:

```text
0xffffffff + 1 = 0x00000000   modulo 2^32
0x00000000 - 1 = 0xffffffff   modulo 2^32
```

Signed C overflow is still undefined behavior at the language level. That
does not contradict the hardware operation. A compiler may reason that a
well-defined C execution never overflows, while the emitted RV32I `ADD`
always has a modulo result if executed.

### One ALU result, several consumers

The same 32-bit output can serve:

- integer writeback for arithmetic;
- effective address generation for `LW`/`SW`;
- target calculation for `JALR`;
- part of branch-target or `AUIPC` calculation if the implementation shares
  hardware.

Sharing is an implementation choice. A single-cycle design often adds
dedicated PC adders so data arithmetic and next-PC calculation can happen in
parallel.

---

## 2. Compare outputs are not architectural flags

The lab ALU returns:

```text
eq            a == b
lt_signed     signed(a) < signed(b)
lt_unsigned   unsigned(a) < unsigned(b)
```

A branch-control block can derive:

```text
BEQ   eq
BNE   !eq
BLT   lt_signed
BGE   !lt_signed
BLTU  lt_unsigned
BGEU  !lt_unsigned
```

`SLT` and `SLTU` instead turn a comparison into an integer value:

```text
comparison true   result = 0x00000001
comparison false  result = 0x00000000
```

Those are related uses of compare logic, but their destinations differ.

### Why not a flags register?

Base RV32I has no x86-style general integer condition-code register updated
by ordinary arithmetic. `BEQ` and `BLT` name their source registers directly.
That means the architectural dependency is:

```text
branch ← rs1, rs2
```

rather than:

```text
compare/subtract → architectural flags → later branch
```

Our combinational `eq` and less-than outputs are internal datapath signals.
They do not become hidden ISA-visible state merely because the RTL names
them.

This distinction matters in a context switch. A kernel must preserve
architectural state. It does not save every combinational wire from the
branch comparator.

### Carry and overflow

An implementation may internally compute carry or signed overflow. Base
RV32I `ADD` does not write a carry flag or trap on signed overflow.
Multiword arithmetic can derive carry with ordinary instructions, for
example by comparing the sum with an operand using `SLTU`.

Do not add architectural flags because an adder can conveniently produce
them. Hardware capability does not silently extend the ISA contract.

---

## 3. The register-file interface

The subset needs at most two source operands and one destination per
instruction. A natural register-file interface is:

```text
raddr1[4:0] ──▶ read port 1 ──▶ rdata1[31:0]
raddr2[4:0] ──▶ read port 2 ──▶ rdata2[31:0]

write_enable
waddr[4:0] ───▶ write port
wdata[31:0] ──▶ accepted on rising edge
```

Five address bits select one of 32 architectural names.

### Our timing convention

The lab chooses:

- **combinational reads** — changing `raddr1` or `raddr2` changes the
  corresponding read data after combinational delay;
- **synchronous write** — when `write_enable` is true, `wdata` is accepted at
  the rising edge for `waddr`;
- **two read ports, one write port**;
- **no reset of x1–x31**.

This convention fits the single-cycle path:

```text
PC/instruction bits settle
 → rs1/rs2 addresses select register values
 → ALU and memory paths settle
 → result is written at the closing edge
```

It is not mandated by RV32I. An FPGA-oriented design might use block RAM with
synchronous reads. A high-performance CPU may use banked, replicated,
multiported, renamed, or bypassed structures. Any implementation must
preserve architectural behavior.

### RTL array, physical cells, and ports

`logic [31:0] registers [31:0]` describes indexed state and the lab's access
behavior; it does not declare a literal floorplan of 1,024 independent
flip-flops. A synthesis flow for a small target may implement the array with
flip-flops and muxes. An FPGA tool may infer distributed RAM or block RAM.
An ASIC may use a custom or generated register-file macro whose dense storage
cells, wordlines, bitlines, read sensing, and write drivers differ from both.

Ports are physical resources, not just function arguments. Two simultaneous
reads and one write may require a true multiported cell, duplicated read
structures, banking, time-multiplexing, or bypass logic. Those choices affect
area, delay, energy, and collision behavior while preserving the same
architectural register semantics. Therefore say “this RTL models a
two-read/one-write register file,” not “all register files are flip-flop
arrays” or “every RTL port becomes one identical physical port.”

### Why there is no bulk reset

RV32I does not promise that x1–x31 become zero at reset. Clearing every entry
in a small RTL array can also imply reset hardware that a physical register
file would not use.

The testbench therefore writes a register before expecting a known value.
In a real boot flow, reset establishes the PC and platform-required control
state; firmware initializes the software state it needs.

---

## 4. `x0` needs two enforcement points

Architecturally:

```text
read x0       always returns 0
write x0      has no effect
```

The lab implements both:

```systemverilog
assign rdata1 = (raddr1 == 5'd0) ? 32'd0 : registers[raddr1];

always_ff @(posedge clk)
    if (write_enable && waddr != 5'd0)
        registers[waddr] <= wdata;
```

Suppressing writes alone is insufficient if the physical entry for index
zero is unknown and reads expose it. Forcing reads to zero alone hides an
unnecessary write but allows useless state changes and can complicate
verification or power reasoning. The contract is clearest when both paths
express the invariant.

An implementation may omit physical storage for x0 entirely. The ISA
requires behavior, not 32 identical storage rows.

### Same-address read and write

Suppose x5 contains `7`, while the rising edge writes `9` to x5.

Under the lab's abstract RTL model:

```text
before edge                 rdata for x5 is 7
at edge                     write accepts 9
after nonblocking update    rdata for x5 becomes 9
```

That is useful for this model, but actual memory macros can be read-first,
write-first, no-change, or device-specific for a same-address collision.
Pipeline bypass logic may be required when a consumer needs a value before
the register file naturally presents it.

In the single-cycle CPU, the next instruction is selected after the PC
updates and sees the newly written register value during the next cycle.
There is no overlap between instructions yet.

---

## 5. The PC is a register, not the entire control-flow mechanism

The **program counter register** stores the address of the current
instruction. Combinational logic computes candidates:

```text
sequential    pc + 4
branch        pc + branch_imm
JAL           pc + jal_imm
JALR          (rs1 + i_imm) & ~1
```

A next-PC selector chooses one candidate:

```text
next_pc = branch_taken ? branch_target : pc_plus_4
```

with higher-level decode choices for `JAL` and `JALR`.

At the rising edge:

```text
pc <= next_pc;
```

Do not say that the PC “increments itself.” An adder creates a candidate
`pc+4`; control selects it; the PC register captures it.

### Current PC and link value

For `JAL` and `JALR`, two values are needed:

```text
next PC        control-flow target
rd writeback   old pc + 4
```

The current instruction's PC must remain available while both are computed.
If `rd=x0`, the jump still occurs and the link write is discarded.

### Reset PC is a platform choice

A complete RTL CPU needs some way to enter a known fetch location, commonly
a reset input and parameterized reset vector. The unprivileged ISA does not
declare one universal physical reset address for every RISC-V platform.

This Day 2 lab does not instantiate a PC register because its only sequential
lab state is the register file. The PC contract is specified and traced here;
it will join decode and memory in the complete CPU. Keeping the lab bounded
prevents it from becoming a partial, misleading processor.

### Architecture comparison — flags, registers, and PC

The three ISAs expose different **architectural** dependencies:

- RV32I has 32 integer registers, with `x0` fixed at zero, and conditional
  branches compare registers directly. `JAL`/`JALR` can write a link to the
  selected `rd`; the ABI conventionally uses `ra`.
- AArch64 has 31 general-purpose integer registers plus an encoding that means
  the zero register or stack pointer by context. `PC` is not a
  general-purpose register. Some arithmetic forms update `NZCV`, `B.cond`
  consumes those flags, while instructions such as `CBZ` and `TBZ` test a
  register without first setting flags; `BL` writes link register `x30`.
- x86-64 has 16 general-purpose registers. `RIP` and `RFLAGS` are
  architectural state, although `RIP` is not an ordinary integer register.
  Many arithmetic instructions update flags that later conditional branches
  consume.

This changes compiler-visible dependence chains and context state, but it
does not mandate a physical flags register, register-file port count, or PC
pipeline. Renaming, bypassing, and next-PC hardware are
**microarchitectural**; ABI register roles and OS save/restore policy are
separate contracts. See
[Architecture Comparison for Systems Programmers](../../references/ARCHITECTURE_COMPARISON.md).

---

## 6. Read the small RTL lab

Files:

```text
05-riscv-cpu-microarchitecture/challenges/day-002-alu-register-file.sv
05-riscv-cpu-microarchitecture/challenges/day-002-alu-register-file-tb.sv
05-riscv-cpu-microarchitecture/challenges/day-002-run-alu-register-file.sh
```

The design file contains exactly two modules:

```text
rv32_alu       combinational value and compare outputs
rv32_regfile   two combinational reads, one rising-edge write
```

It is not an instruction decoder or CPU.

### ALU structure

The ALU computes compare outputs continuously and selects one 32-bit result:

```systemverilog
eq          = (a == b);
lt_signed   = ($signed(a) < $signed(b));
lt_unsigned = (a < b);
```

`$signed` changes how the comparison interprets the vector. It does not add a
sign bit or mutate `a`.

The result `case` has a default. Complete assignment avoids accidental
storage in combinational logic.

### Register-file structure

Read ports are continuous assignments. The write uses `always_ff` and a
nonblocking assignment. This directly expresses the chosen timing contract:

```text
read selection     combinational
retained update    rising-edge state change
```

There is no testbench delay, print, or assertion inside the design modules.
Those simulation-only activities remain in the testbench.

---

## 7. Predict, run, inspect

From the repository root:

```bash
bash 05-riscv-cpu-microarchitecture/challenges/day-002-run-alu-register-file.sh
```

The script:

1. checks the design and testbench with Verilator lint;
2. compiles them as SystemVerilog with Icarus Verilog;
3. runs assertions with `vvp`;
4. uses a temporary build directory, leaving source directories clean.

Expected final result:

```text
PASS: ALU and register-file checks completed
```

### Predict before running

Write answers first:

1. What are ADD, SLT, and SLTU results for `a=0xffffffff`, `b=1`?
2. Does writing `0xdeadbeef` to x0 affect either read port?
3. If x5 holds `7`, what does its combinational read show before and after an
   edge writing `9`?
4. Does the ALU retain its previous result when `op` changes?
5. Do the compare outputs become architectural state?
6. Does a passing test establish maximum clock frequency?

### Controlled break

Make one temporary change at a time:

- remove `waddr != 0` from the write condition;
- remove the read-side x0 selection;
- change signed less-than to an unsigned comparison;
- delete one ALU `case` assignment and the default.

Predict which assertion or lint warning should expose each defect. Restore
the source and rerun. A defect not caught by the current tests is evidence
that the testbench needs another observation—not that the defect is safe.

---

## 8. What the tools establish

A clean Verilator run means the selected lint configuration reported no
warning or error. A passing Icarus run means the simulated model satisfied
the testbench assertions for those cases.

Neither proves:

- every 32-bit operand pair;
- decoder correctness;
- physical register-file implementation;
- setup, hold, clock-to-Q, or maximum frequency;
- same-address behavior of a future SRAM macro;
- exception behavior;
- a complete RV32I processor;
- equivalence to a synthesized netlist.

The testbench checks representative arithmetic boundaries, signed/unsigned
comparison, dual reads, synchronous writes, x0, and the before/after-edge
behavior. Those are focused claims.

---

## 9. Systems relevance

These small blocks appear in larger systems questions:

### Context switching

The kernel saves architectural registers, not ALU compare wires. `x0` need
not be saved as a mutable value because its architectural value is known.

### Trap restart

The saved exception PC identifies the instruction that must be diagnosed or
restarted according to the trap's semantics. The next-PC decision therefore
cannot be treated as incidental bookkeeping.

### Emulation and virtualization

An emulator can keep `regs[32]` in host memory and force index zero to zero.
Hardware may omit x0 storage. Both implement the same architectural rule by
different mechanisms.

### Compiler code generation

The absence of flags shapes sequences. Unsigned carry can become an `SLTU`
result; branches compare registers directly. The compiler targets ISA
behavior, not our particular ALU module.

### Debugging

When a trace says the PC jumped incorrectly, separate candidates:

```text
bad immediate decode?
bad signed/unsigned compare?
wrong branch control?
wrong target addition?
wrong PC capture?
stale operand from register file?
```

The block contract turns “the CPU is wrong” into testable boundaries.

---

## 10. Mastery evidence

### Explain

Without notes:

- explain why addition wraps without an RV32I overflow trap;
- compare SLT, SLTU, and branch compare outputs;
- explain why internal comparison wires are not architectural flags;
- describe combinational reads and synchronous writes over one cycle;
- enforce x0 independently on read and write paths;
- explain why x1–x31 are not bulk-reset in this lab;
- distinguish current PC, next-PC candidates, selection, and capture.

### Draw

Draw:

```text
instruction rs1/rs2 fields
      ↓
two-read register file
      ↓ a,b
ALU result + eq/lt_signed/lt_unsigned
      ↓
writeback selection → synchronous write

PC register → pc+4 / pc+imm / (rs1+imm)&~1
      ▲                    ↓
      └──────── next-PC selection
```

Mark combinational paths in one color and edge-triggered state in another.

### Observe

Preserve:

- warning-clean Verilator output;
- warning-clean Icarus compile output;
- the testbench PASS line;
- one signed-versus-unsigned comparison prediction;
- one x0 write-discard observation;
- one before-edge/after-edge same-register observation.

### Build

Add one test—not one new hardware feature—that checks:

```text
0x80000000 < 0x00000000
```

under both signed and unsigned interpretation. Predict all three compare
outputs and the `SLT`/`SLTU` result values before running.

Then sketch, without implementing, a PC module with:

```text
clk, reset, next_pc, current_pc
```

State whether reset is synchronous or asynchronous and choose a reset vector.
Label both as implementation/platform choices.

### Mastery checkpoint

> Trace one `SLT`, one `BLTU`, and one `JALR` from register-file read through
> combinational values to the edge where architectural state changes. Name
> every value that is merely internal and every value that remains visible to
> the next instruction.

---

## Why? notebook

1. Why do ADD and SUB not need signed and unsigned variants?
2. Why do SLT and SLTU need distinct interpretations?
3. Why can branch comparison wires exist without an architectural flags
   register?
4. Why are combinational register-file reads convenient for a single-cycle
   CPU?
5. Why might an FPGA implementation choose a different read convention?
6. Why must x0 behavior be correct even if physical row zero does not exist?
7. Why can a same-address read/write policy require bypass logic later?
8. Why is resetting every register neither required by RV32I nor always cheap?
9. Why is `pc+4` a combinational candidate rather than an automatic property
   of the PC?
10. Why can a passing RTL simulation not establish a safe clock period?

---

## References used selectively

- RISC-V International, *The RISC-V Instruction Set Manual, Volume I:
  Unprivileged Architecture*, RV32I programmer's model, integer computational
  instructions, control transfer, and `x0` behavior, version 20240411:
  <https://docs.riscv.org/reference/isa/unpriv/rv32.html>
- IEEE Std 1800-2023, *SystemVerilog*, for `always_comb`, `always_ff`,
  nonblocking assignment, packed vectors, and signed casts:
  <https://standards.ieee.org/ieee/1800/7743/>
- Icarus Verilog documentation:
  <https://steveicarus.github.io/iverilog/>
- Verilator guide:
  <https://verilator.org/guide/latest/>
- David Money Harris and Sarah L. Harris, *Digital Design and Computer
  Architecture: RISC-V Edition*, processor datapath and register-file
  sections, for the teaching implementation pattern.

**Next bridge:** connect these blocks to immediate generation, instruction
decode, memories, writeback, and next-PC control, then test complete
instruction-level state transitions before introducing pipelining.
