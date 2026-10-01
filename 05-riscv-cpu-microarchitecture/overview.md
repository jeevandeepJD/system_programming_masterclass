# Week 5 — Designing a Simple RISC-V CPU

**Format:** seven daily sections collected into one weekly mastery module

**Suggested workload:** approximately 21 focused hours, adjustable to mastery

> The ISA defines the machine software may rely on. A microarchitecture must
> make that machine appear while real values move through finite, timed
> hardware.

This module turns the Week 4 hardware foundation into a small RISC-V CPU
model. It begins with an explicit RV32I subset and architectural state,
derives the ALU, register file, PC, decoder, and single-cycle paths, executes
a hand-encoded loop, then introduces pipelining, hazards, caches, and address
translation. The purpose is systems understanding: the learner should be
able to connect compiler output, saved trap state, page faults, PMU evidence,
QEMU/KVM behavior, and eventually ToyOS to concrete CPU mechanisms.

This is not an HDL-specialization module and does not claim to implement a
production RISC-V platform. “Week” names a mastery package, not a calendar
deadline. Repeat a trace or split a lab across sessions whenever the
architectural result, implementation mechanism, or ownership of a decision
is unclear.

## The responsibility map

Keep four layers separate throughout the module:

- **Architectural:** ISA-visible registers, instruction effects, addresses,
  ordering guarantees, and exceptions.
- **Microarchitectural:** datapaths, controls, pipeline registers, forwarding,
  predictors, caches, TLBs, stalls, fills, and replacement choices.
- **OS policy:** mappings, page-table entries, permissions, fault handling,
  scheduling, and process isolation.
- **Physical:** gates, wires, clocked state, voltage ranges, capacitance,
  delay, energy, and transistor switching.

The layers form a causal chain, but they do not own the same decisions.

```text
C and compiler
  → RISC-V architectural instructions
  → datapath and control
  → pipelined implementation
  → cache/TLB/MMU interactions
  → OS-configured translation and trap response
  → gates and physical switching
```

## Daily roadmap and exact files

### Day 1 — An RV32I Subset and Its Datapath Contract

Choose a documented RV32I teaching subset, separate official ISA behavior
from implementation choices, decode the required formats, name architectural
state, and derive the datapath/control contract.

Lesson:

```text
05-riscv-cpu-microarchitecture/sections/day-001-rv32i-subset-and-datapath-contract.md
```

Challenge:

```text
05-riscv-cpu-microarchitecture/challenges/day-001-rv32i-subset-trace.py
```

**Evidence:** decode R/I/S/B/U/J examples, trace one complete architectural
transition, reject unsupported words safely, and label every claim as ISA or
teaching-implementation behavior. Keep the model's instruction/data request
ports distinct from the SRAM arrays, cache hierarchy, memory controller, and
DRAM banks that may implement those requests.

### Day 2 — ALU, Register File, and Program Counter

Make modulo arithmetic, signed/unsigned comparison, two-read/one-write
register state, the `x0` invariant, and next-PC selection precise. Observe
combinational values separately from edge-triggered state.

Lesson:

```text
05-riscv-cpu-microarchitecture/sections/day-002-alu-register-file-and-program-counter.md
```

Challenges:

```text
05-riscv-cpu-microarchitecture/challenges/day-002-alu-register-file.sv
05-riscv-cpu-microarchitecture/challenges/day-002-alu-register-file-tb.sv
05-riscv-cpu-microarchitecture/challenges/day-002-run-alu-register-file.sh
```

**Evidence:** preserve clean lint/simulation output, compare `SLT` with
`SLTU`, demonstrate that writes to `x0` have no effect, and explain
current-PC state versus next-PC candidates. Treat the RTL register array as
one implementation model: physical register files may use custom cells,
banking, replication, or synthesized flops, and their ports need not map
one-for-one to the source-level array.

### Day 3 — Decoder, Control, and Single-Cycle Paths

Turn instruction bits into immediate construction, legality checks, mux
selections, ALU operation, write enables, memory requests, and next-PC
control. Derive why the single-cycle load path can determine the clock.

Lesson:

```text
05-riscv-cpu-microarchitecture/sections/day-003-decoder-control-and-single-cycle-paths.md
```

Challenge:

```text
05-riscv-cpu-microarchitecture/challenges/day-003-control-decoder.py
```

**Evidence:** inspect legal arithmetic/load/store/branch controls, reconstruct
a negative branch immediate, prove illegal encodings disable architectural
writes, and draw the load critical path. Carry the request forward to the
stallable cache and lower-memory path rather than assuming every physical
memory responds combinationally.

### Day 4 — Integrating and Running a Tiny RV32I Core

Connect fetch, decode, register reads, ALU/address work, memory, writeback,
and next-PC selection around an explicit commit boundary. Run a hand-encoded
loop that sums signed words, stores the result, and loads it back.

Lesson:

```text
05-riscv-cpu-microarchitecture/sections/day-004-integrating-and-running-a-tiny-rv32i-core.md
```

Challenge:

```text
05-riscv-cpu-microarchitecture/challenges/day-004-tiny-rv32i-core.py
```

**Evidence:** predict all 25 model cycles, annotate one loop iteration as
state → candidates → commit, verify final registers and memory, and explain
why model errors are not yet architectural traps.

### Day 5 — Pipelining and Throughput

Split the long combinational path into IF, ID, EX, MEM, and WB stages.
Distinguish latency from throughput, account for register overhead and stage
imbalance, and carry identity/control/validity with each instruction.

Lesson:

```text
05-riscv-cpu-microarchitecture/sections/day-005-pipelining-and-throughput.md
```

Challenge:

```text
05-riscv-cpu-microarchitecture/challenges/day-005-pipelining-and-throughput.py
```

**Evidence:** derive the clock period and `k + N - 1` cycle count, inspect an
ideal pipeline trace, calculate finite-batch speedup, and separate pipeline
state from architectural retirement.

### Day 6 — Hazards, Forwarding, Stalls, and Branches

Classify structural, data, and control hazards. Use forwarding when a result
exists, stall when it does not, and flush wrong-path work after branch
resolution while preserving precise architectural state.

Lesson:

```text
05-riscv-cpu-microarchitecture/sections/day-006-hazards-forwarding-stalls-and-branches.md
```

Challenge:

```text
05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py
```

**Evidence:** preserve traces for an ALU bypass, load-use interlock,
no-forwarding wait, unified-memory structural conflict, branch misprediction,
and correct prediction; explain the architectural error each mechanism
prevents.

### Day 7 — Cache, TLB, MMU, and End-to-End Task Trace

Connect instruction and data requests to caches, miss backpressure, address
translation, a TLB, page-table walks, permissions, and precise faults. Keep
memory ordering and coherence at overview depth, then trace a C `sum` from
compiler-selected RISC-V instructions down to gates and transistor switching.

Lesson:

```text
05-riscv-cpu-microarchitecture/sections/day-007-cache-tlb-mmu-and-end-to-end-task-trace.md
```

Challenge:

```text
05-riscv-cpu-microarchitecture/challenges/day-007-cache-tlb-trace.py
```

**Evidence:** annotate TLB and cache hit/miss paths, observe a page-table walk,
prove that a read-only PTE blocks a store even when the line is cached,
distinguish absent mapping from denied permission, and assign every decision
to architecture, microarchitecture, OS policy, or physics. Follow one miss
below the cache through SRAM tag/data lookup, line fill, a memory controller,
and a bounded DRAM row operation before tracing the response back to the
waiting load.

## Adjustable workload guide

The nominal budget is approximately 21 focused hours:

```text
Day 1   3 h   RV32I subset, architectural state, formats, datapath contract
Day 2   3 h   ALU, comparison, register file, x0, PC state
Day 3   3 h   decode, legality, controls, single-cycle paths
Day 4   3 h   integrated core and complete loop execution
Day 5   3 h   pipeline timing, latency, throughput, retirement
Day 6   3 h   hazards, forwarding, stalls, prediction, flush
Day 7   3 h   caches, TLB/MMU, faults, end-to-end trace
```

This is an estimate, not a completion rule. Spend longer where a prediction
and trace disagree. Familiar syntax may be reviewed quickly, but do not skip
the explanation of architectural effects, timing assumptions, or model
limits.

## Integrated weekly evidence

### Explain

Without notes:

1. separate official ISA behavior, the chosen subset, and implementation;
2. derive the data and control path for `ADD`, `LW`, `SW`, a branch, and an
   indirect control transfer;
3. explain why combinational candidates are not committed state;
4. distinguish latency, throughput, clock period, CPI, and retirement;
5. explain when forwarding works and why a load-use pair can still stall;
6. explain how prediction and flushing preserve the selected instruction
   stream;
7. distinguish cache miss, TLB miss, invalid translation, and permission
   fault;
8. trace one result down through gates to physical charge movement.

### Draw

Produce one connected drawing with:

- PC, instruction fields, immediate generation, register file, ALU, memory,
  writeback, and next-PC paths;
- IF/ID, ID/EX, EX/MEM, and MEM/WB state with valid/kill metadata;
- forwarding sources, load-use detection, and branch redirect;
- instruction/data cache paths and miss backpressure;
- virtual page number, page offset, TLB, page-table walker, permission check,
  physical address, and fault path;
- one selected datapath bit realized by gates and transistor-controlled
  charging/discharging.

Mark architectural, microarchitectural, OS-policy, and physical elements
using distinct labels.

### Observe

Preserve a compact evidence bundle:

- Day 1 decode/state trace and unsupported-word diagnostic;
- Day 2 RTL lint/simulation result and signed/unsigned comparison;
- Day 3 legal/illegal control bundles;
- Day 4 complete loop trace and final state;
- Day 5 timing derivation and ideal pipeline trace;
- Day 6 forwarding, stall, structural-conflict, and flush traces;
- Day 7 cache/TLB trace and all self-test PASS lines;
- one statement per lab describing what its model cannot prove.

### Correlate existing evidence end to end

Do not create a new lab. Reuse the existing artifacts as a cross-layer
workflow:

1. Start with the Week 3
   [source-to-object evidence](../03-c-toolchain-and-startup/sections/day-005-source-to-object-file.md)
   and [ELF loading evidence](../03-c-toolchain-and-startup/sections/day-007-elf-loading-and-main.md).
   `readelf` proves container metadata for the inspected file; `objdump`
   proves how that tool decodes that file's bytes for its declared machine.
   Host-architecture output is not RISC-V CPU evidence.
2. Select one operation in the C source and correlate it with the
   compiler-selected instructions only when source locations, symbols, and
   the actual disassembly support the match. Then use the Week 5 CPU and
   cache traces to demonstrate the chosen teaching model's architectural
   updates, stalls, hits, misses, and fills.
3. Reuse the Week 4
   [RTL and waveform evidence](../04-cpu-and-chip-design/sections/day-006-systemverilog-rtl-and-waveforms.md)
   and [Yosys synthesis evidence](../04-cpu-and-chip-design/sections/day-007-synthesis-netlists-and-physical-design-overview.md).
   Simulation proves tested RTL behavior; synthesis proves the selected RTL
   was transformed into a netlist under the recorded script and cell model.
   Neither proves that the Week 5 Python core is that RTL.
4. Reuse the Week 4
   [CMOS/ngspice evidence](../04-cpu-and-chip-design/sections/day-003-cmos-nand-nor-and-gate-networks.md)
   and [delay/power evidence](../04-cpu-and-chip-design/sections/day-004-delay-capacitance-fanout-and-power.md)
   to observe voltage, current, delay, and charging in the simulated circuit.
   That proves behavior of the stated device models and stimulus, not the
   transistor implementation or energy of a fabricated RISC-V core.

The result is an evidence-backed causal chain, not a claim that these
separate teaching artifacts form one formally equivalent implementation.

### Build

Complete one bounded extension in each model, or build one integrated
explanation that:

1. begins with a 32-bit instruction and old architectural state;
2. derives operands, controls, and candidate results;
3. commits only enabled effects;
4. follows the same dependencies through a pipeline;
5. inserts a justified bypass, stall, or flush;
6. waits safely for cache/TLB misses;
7. reports a denied or absent mapping without committing the access;
8. ends with layer-labelled physical switching.

Use the recurring loop:

```text
predict → run → inspect → explain the responsible layer
        → change one condition → rerun → compare
```

## Integrated mastery checkpoint

Trace this task end to end:

```c
int sum(const int *a, int n) {
    int s = 0;
    for (int i = 0; i < n; ++i)
        s += a[i];
    return s;
}
```

A complete answer must connect:

```text
C abstract-machine behavior
  → compiler-selected RISC-V instructions
  → architectural PC/register/memory effects
  → fetch, decode, execute, memory, and writeback paths
  → dependencies, forwarding, stalls, branch redirect, and retirement
  → instruction/data cache hits, misses, fills, and backpressure
  → TLB hit or hardware page-table walk
  → OS-created mapping and permission data
  → precise fault and OS response when an access cannot complete
  → mux/decoder/adder/register gates
  → transistor conduction, capacitance, delay, and switching energy
```

For every arrow, identify whether it is an architectural requirement, a
microarchitectural choice, OS policy, or a physical mechanism. The
explanation must remain correct when a load hits, misses, waits for
translation, or faults.

## Bridge to ToyOS

The CPU provides mechanisms: architectural state, privilege transitions,
precise traps, interrupt delivery, translation, permissions, and memory
access. ToyOS will provide policy: boot-time setup, physical-page allocation,
page-table construction, mappings, trap handling, task state, scheduling,
user mode, and system calls.

```text
simple CPU model
  → architectural traps and privilege
  → ToyOS boot and exception entry
  → physical-page allocation and virtual memory
  → interrupts, tasks, user mode, and system calls
```

Do not auto-fill confidence, completion, hours, or mastery. Those remain the
learner's evidence-based assessment.
