# Day 4 — Integrating and Running a Tiny RV32I Core

**Target time:** approximately 2–3 hours

- Integrate fetch, decode, execute, memory, and writeback: 40–50 minutes
- Predict and run the hand-encoded program: 50–65 minutes
- Inspect state, branches, and invariants: 35–45 minutes
- Extend, evaluate omissions, and bridge outward: 30–40 minutes

> Can you account for every architectural state change in a complete loop,
> while keeping the model's convenient assumptions separate from real CPU
> behavior?

## Why this day exists

Yesterday's decoder produced control signals but did not execute a program.
Today we connect those signals to a small state machine:

```text
architectural state before edge
  → fetch instruction at PC
  → decode fields and controls
  → read registers and form immediate
  → compute ALU, memory, branch, and writeback candidates
  → commit enabled state at one abstract edge
  → architectural state after edge
```

The runnable model makes PC, instruction, control, registers, memory, and
next-state candidates visible. It executes a hand-encoded RV32I-subset
program that loops over four signed words, sums them, stores the result, and
loads it back.

This is a microarchitecture model, not merely an instruction interpreter,
because it exposes datapath candidates and an explicit commit boundary. It
is still not cycle-accurate for any real processor.

By the end, you should be able to:

- predict the next PC and enabled writes for each supported instruction;
- separate combinational candidates from committed state;
- explain why `x0` remains zero;
- trace little-endian `LW` and `SW`;
- use invariants and final state as evidence;
- name what the model omits;
- relate this loop to QEMU and to a physical pipelined CPU without claiming
  they work identically.

---

## 1. The implemented contract

The model supports only:

```text
ADD
ADDI
LW
SW
BEQ
BNE
```

It has:

- 32 integer registers, each masked to 32 bits;
- `x0` hardwired to zero;
- a byte-addressed, little-endian data-memory dictionary;
- a list of 32-bit instruction words;
- a byte-addressed PC;
- one architectural instruction committed per model cycle;
- alignment checks for `LW` and `SW`;
- a cycle limit to catch accidental infinite loops.

Execution stops when the PC points just beyond the program list. That is a
test-harness convention, not a RISC-V halt instruction.

The distinction matters. Bare RV32I does not promise that “falling off the
end” powers down a processor. Real software exits through an environment
call, device protocol, firmware convention, debugger, or platform-specific
mechanism.

---

## 2. State versus candidates

Before an edge, the model has architectural state:

```text
PC
x0..x31
data-memory bytes
cycle counter (model instrumentation, not an ISA register)
```

Given the current instruction, its combinational phase calculates:

```text
decoded fields and control
rs1 and rs2 values
selected ALU operand B
ALU result
optional loaded memory value
optional writeback value
branch decision
next PC
```

No architectural state changes during this calculation. The `clock` method
then commits only enabled writes and the selected next PC.

For:

```asm
addi x3, x3, -1
```

the visible sequence is:

```text
before:    x3=4, PC=0x18
candidate: ALU=3, writeback=3, nextPC=0x1c
edge:      x3←3, PC←0x1c
after:     later instructions can observe x3=3
```

The Python statements execute sequentially because software must simulate
the model somehow. The represented hardware relationships are combinational
settling followed by simultaneous-looking state capture.

### `x0` is an invariant

The decoder may identify `rd=0`, and the ALU may calculate a result. The
commit path discards the register write when `rd == 0`, then reasserts:

```text
registers[0] = 0
```

The invariant is stronger than “this demo does not write x0.” It says every
modeled cycle ends with `x0 == 0`.

---

## 3. The hand-encoded program

The challenge contains these literal words:

```text
PC    word        assembly                 purpose
00    00000093    addi x1,x0,0             sum = 0
04    10000113    addi x2,x0,256           pointer = 0x100
08    00400193    addi x3,x0,4             count = 4
0c    00012203    lw   x4,0(x2)            load current element
10    004080b3    add  x1,x1,x4            add to sum
14    00410113    addi x2,x2,4             advance pointer
18    fff18193    addi x3,x3,-1            decrement count
1c    fe0198e3    bne  x3,x0,-16           repeat at PC 0x0c
20    00112023    sw   x1,0(x2)            store sum at 0x110
24    00012283    lw   x5,0(x2)            load it back
```

Initial data memory contains:

```text
address  signed word
0x100       7
0x104      -2
0x108      13
0x10c       5
```

The sum is 23. The loop advances `x2` four times, so after the final
not-taken branch:

```text
x2 = 0x110
```

That is where `SW` writes 23 and `LW` reads it into `x5`.

### Predict dynamic instruction count

Three setup instructions execute once. Five loop instructions execute four
times. Two final memory instructions execute once:

```text
3 + (5 × 4) + 2 = 25 model cycles
```

`BNE` is taken three times and not taken once. A wrong prediction here often
reveals an off-by-one error about whether decrement happens before the
condition is tested.

---

## 4. One loop iteration through the datapath

Assume the first iteration begins:

```text
PC=0x0c, x1=0, x2=0x100, x3=4
memory word at 0x100 = 7
```

### `LW`

```text
decode:      I immediate 0, ALU ADD, B=IMM, MemRead, WB=MEM, RegWrite
registers:   A=x2=0x100
ALU:         0x100+0=0x100
memory:      bytes at 0x100..0x103 → 0x00000007
edge:        x4←7, PC←0x10
```

### `ADD`

```text
decode:      ALU ADD, B=RS2, WB=ALU, RegWrite
registers:   A=x1=0, B=x4=7
candidate:   7
edge:        x1←7, PC←0x14
```

### Pointer and count updates

The two `ADDI` instructions produce:

```text
x2←0x104
x3←3
```

### `BNE`

```text
comparison:  x3=3 differs from x0=0
target:      branch PC 0x1c + (-16) = 0x0c
edge:        PC←0x0c
```

The final iteration instead compares zero with zero and chooses `PC+4`,
which is `0x20`.

---

## 5. Loads, stores, and signed interpretation

Memory is byte-addressed and little-endian. The initial `-2` word is stored
as the 32-bit pattern:

```text
0xfffffffe
```

At increasing addresses:

```text
fe ff ff ff
```

`LW` reconstructs the same 32-bit pattern. The register contains bits, not a
persistent signed type. The trace displays selected values through a signed
helper so humans see `-2`; addition still wraps modulo \(2^{32}\).

For the second iteration:

```text
0x00000007 + 0xfffffffe = 0x00000005  modulo 2^32
```

No overflow flag or trap exists in base `ADD`; the low 32 bits are the
architectural result.

`SW` writes the low 32 bits as four bytes. `LW` at the same aligned address
reconstructs them, giving a useful end-to-end memory invariant:

```text
store_word(address, v); load_word(address) == v & 0xffffffff
```

---

## 6. Laboratory — predict, run, and explain

Use:

```text
05-riscv-cpu-microarchitecture/challenges/day-004-tiny-rv32i-core.py
```

First run the noninteractive checks:

```bash
python3 05-riscv-cpu-microarchitecture/challenges/day-004-tiny-rv32i-core.py \
  --selftest
```

Then predict:

1. Which cycle first writes `x4`?
2. What are `x1`, `x2`, and `x3` immediately before each `BNE`?
3. Which three branches are taken?
4. At what PC does the final store execute?
5. Which address does it use?
6. What is the final PC?
7. How many model cycles execute?

Run the full trace:

```bash
python3 05-riscv-cpu-microarchitecture/challenges/day-004-tiny-rv32i-core.py
```

For the first iteration, annotate each line:

```text
STATE → CONTROL → COMB candidates → EDGE changes
```

Then use prediction pauses:

```bash
python3 05-riscv-cpu-microarchitecture/challenges/day-004-tiny-rv32i-core.py \
  --step
```

At every pause, say the exact PC, register, or memory changes before
committing. “The ADD executes” is not specific enough.

### Fault observation

Make a temporary copy and change the pointer initialization from 256 to 258.
Predict the first failing instruction and whether any later state commits.
The model reports a misaligned `LW`.

This is a host-language exception used to stop the lab. It is **not** a
modeled RISC-V trap: there is no `mepc`, `mcause`, privilege state, trap
vector, or restart. Restore the original word after observing the result.

### TODO extension

The source leaves one bounded extension:

```text
SLT rd,rs1,rs2
opcode=0110011, funct3=010, funct7=0000000
```

Before editing:

- decide whether a new mux is needed;
- specify `ALUOp`, `ALUBSel`, `WBSel`, and enables;
- predict signed results for `-2 < 7` and `7 < -2`;
- hand-encode one test instruction;
- add an independent self-test;
- retain rejection of nearby unsupported encodings.

The key implementation detail is signed comparison of 32-bit patterns. Do
not accidentally use Python's nonnegative stored integers as though they
already carried RV32 signed meaning.

---

## 7. What this model omits

The trace is intentionally honest about a small contract. It omits:

- all RV32I instructions except the six named operations;
- instruction bytes and an instruction-memory bus;
- compressed or other extensions;
- privilege modes, CSRs, traps, interrupts, and environment calls;
- virtual memory, protection, an MMU, page tables, and TLBs;
- caches and variable memory latency;
- memory-mapped devices and bus protocols;
- atomic operations and the RISC-V memory model;
- pipelines, hazards, bypassing, stalls, and branch prediction;
- superscalar issue, out-of-order execution, speculation, and retirement;
- realistic SRAM timing, ports, clock-to-Q, setup/hold, and physical delay;
- reset, debug, power, and multicore coherence.

It also treats a dictionary read as combinational data memory. A physical
single-cycle implementation cannot assume arbitrary system memory responds
within one short clock period.

The trace therefore proves only:

> For this program and modeled initial state, the implemented transition
> rules produce the asserted final state.

It does not prove RV32I compliance, synthesizability, timing closure, or
behavior of an existing CPU.

---

## 8. Bridge to QEMU

QEMU can run RISC-V software on a non-RISC-V host. At a high level it also:

```text
fetch guest instruction bytes
→ decode guest instruction semantics
→ update guest-visible state
```

But QEMU's Tiny Code Generator translates blocks of guest instructions into
an intermediate representation and host code, caches translated blocks, and
maintains a much broader architectural/platform model. It is not arranging
guest mux selects in silicon and does not make one host action equal one
guest CPU cycle.

The lab and QEMU share an architectural obligation: given defined starting
state and an instruction, produce the specified visible result or exception.
They differ in scope, optimization, timing purpose, and implementation.

A useful future experiment is:

```bash
qemu-system-riscv32 --version
qemu-riscv32 --version
```

if those tools already exist. Do not infer absence of RISC-V support merely
because one binary is not installed; system emulation and Linux user-mode
emulation are separate builds and use cases.

---

## 9. Bridge to a real CPU

A simple pipelined implementation could spread today's dependencies across:

```text
IF   fetch instruction and form sequential PC
ID   decode, generate immediate, read registers
EX   ALU, address calculation, branch decision
MEM  data-memory access
WB   register writeback
```

That can shorten the combinational work per clock period, but introduces
overlap. The loop then creates hazards:

- `ADD` needs the value loaded by the preceding `LW`;
- `BNE` needs the count written by the preceding `ADDI`;
- a taken `BNE` redirects younger fetched instructions;
- `SW` needs the final `x1` sum and `x2` address.

Forwarding can satisfy some dependencies; stalls handle values not ready in
time; flushing or redirect logic handles wrong-path fetch. An out-of-order
core may execute many internal operations concurrently and speculatively,
then retire results in architectural order.

Despite those differences, correct implementations must make committed state
look as though this instruction stream obeyed the ISA. That is why the
simple trace remains useful to systems programmers: it is a reference-level
story of architectural dependence, not a performance model.

---

## 10. Evidence and mastery

Preserve:

- all three `PASS` lines;
- your predicted and observed dynamic instruction count;
- one taken and the final not-taken branch trace;
- one `LW` showing address and loaded value;
- the `SW` edge showing four-byte state at `0x110`;
- final PC, `x1`, `x3`, `x5`, and memory word;
- one sentence explaining why end-of-list is not an ISA halt;
- one sentence explaining why one model cycle is not one real CPU cycle.

Explain without notes:

1. Which values are state before a cycle?
2. Which values are combinational candidates?
3. Why does the edge update PC even for a store?
4. Why does the loop execute exactly four loads?
5. Why does loading `0xfffffffe` and adding it produce subtraction-like
   behavior without a signed register type?
6. Why does the final `LW` add confidence beyond inspecting the stored bytes?
7. What would a real misaligned-access exception need to preserve?
8. Which loop dependencies become hazards in a pipeline?

### Mastery checkpoint

For the dynamic instruction `BNE` in the third loop iteration, state:

- PC and instruction word;
- decoded register fields and immediate;
- source values;
- control outputs;
- subtraction/comparison result;
- target candidate and sequential candidate;
- selected next PC;
- all architectural state changes at the edge.

Then explain how QEMU and a five-stage core could both preserve that
architectural result while doing very different work internally.

---

## Why? notebook

1. Why does a runnable state-transition model reveal more than a decoder
   table?
2. Why are combinational values printed before edge changes?
3. Why must `x0` be enforced at commit even if ordinary programs rarely
   target it?
4. Why is the branch target based on the branch instruction's PC?
5. Why does the pointer end at `0x110` rather than `0x10c`?
6. Why is `-2` stored as bytes `fe ff ff ff` on this little-endian model?
7. Why does a cycle limit provide useful validation evidence?
8. Why is a Python alignment error not a RISC-V architectural trap?
9. Why can a pipelined CPU complete the same program with overlapping work?
10. Why can QEMU preserve ISA behavior without reproducing hardware control
    wires or cycle timing?
11. Why does final state alone not prove every intermediate control decision
    was correct?
12. Why does passing this program not establish ISA compliance?

---

## References used selectively

- RISC-V International, *RV32I Base Integer Instruction Set*, for integer
  register state, instruction encodings, loads/stores, branches, and
  architectural behavior:
  <https://docs.riscv.org/reference/isa/unpriv/rv32.html>
- RISC-V International, *The RISC-V Instruction Set Manual, Volume II:
  Privileged Architecture*, for exceptions, CSRs, and trap behavior omitted
  from the model:
  <https://docs.riscv.org/reference/isa/priv/priv-index.html>
- RISC-V International, *RVWMO Memory Consistency Model*, for the multicore
  ordering contract not represented by a single-thread dictionary:
  <https://docs.riscv.org/reference/isa/unpriv/rvwmo.html>
- QEMU documentation, “Translator Internals” and Tiny Code Generator
  documentation, for block translation and the distinction from a
  cycle-accurate hardware model:
  <https://www.qemu.org/docs/master/devel/tcg.html>
- QEMU documentation, RISC-V system emulator target:
  <https://www.qemu.org/docs/master/system/target-riscv.html>
- David Money Harris and Sarah L. Harris, *Digital Design and Computer
  Architecture: RISC-V Edition*, processor and pipelining chapters, for the
  single-cycle-to-pipeline bridge.

**Next bridge:** pipeline this architectural work, then confront data,
control, and structural hazards without changing the software-visible result.
