# Week 2 — Assembly, Memory Hierarchy, and the C Abstract Machine

**Format:** seven daily sections collected into one weekly module

**Suggested workload:** approximately 21 focused hours, adjustable to mastery

> Last week built upward from physical state to CPU control. This week begins
> at the software-visible machine: read its instructions, follow a function
> call, compare two ISAs, discover why memory speed depends on access pattern,
> and finally explain how C's typed abstract machine becomes operations on
> that hardware.

“Week” names a mastery module, not a seven-day deadline. A difficult lab may
take two sessions; a familiar section may take less. The completion criterion
is integrated evidence, not elapsed calendar time.

## Why these seven days belong together

These topics can look like separate courses—assembly, caches, data layout,
and C—but a running program crosses all of them:

```text
C types, objects, and expressions
        ↓ compiler preserves required behavior
ISA instructions and ABI contracts
        ↓ name registers, operations, calls, and addresses
virtual and physical memory paths
        ↓ hierarchy exploits temporal and spatial locality
cache lines, sets, placement, and coherence
        ↓ concrete bytes have alignment and byte order
CPU executes generated operations on representations
```

The causal order matters. Assembly makes architectural state visible. Calling
conventions show that interoperability depends on contracts beyond the ISA.
RISC-V comparison separates universal machine problems from x86-64's
particular solutions. The memory hierarchy then explains why two correct
instruction sequences can have very different costs. Alignment and byte
order make object representation concrete. C's abstract machine finally
explains which of those details the language specifies, which the
implementation chooses, and which an optimizer may transform away.

## Daily roadmap and exact files

### Day 1 — x86-64 Assembly Foundations

Read registers, subregisters, memory operands, address calculations, flags,
arithmetic, and branches. Hand-trace state and use GDB to step one instruction
at a time.

Lesson:

```text
weekly/week-002/lessons/day-001-x86-64-assembly-foundations.md
```

Challenges:

```text
weekly/week-002/challenges/day-001-routines.s
weekly/week-002/challenges/day-001-driver.c
```

**Evidence:** three small routines, one instruction trace with registers and
flags, and an annotated compiler-generated loop.

### Day 2 — Stack Frames and Calling Conventions

Follow `call` and `ret`, draw activation records, apply the System V AMD64
ABI, distinguish caller- and callee-saved registers, and connect recursion to
finite user and kernel stacks.

Lesson:

```text
weekly/week-002/lessons/day-002-stack-frames-and-calling-conventions.md
```

Challenges:

```text
weekly/week-002/challenges/day-002-stack-frames-lab.c
weekly/week-002/challenges/day-002-abi-probe.s
```

**Evidence:** a labelled stack frame, GDB backtrace/frame inspection, a
detected ABI violation, and a measured stack-exhaustion experiment.

### Day 3 — RISC-V Assembly and ISA Comparison

Read a load/store ISA, compare register and instruction conventions with
x86-64, and distinguish architectural simplicity from the performance and
compatibility of a full implementation.

Lesson:

```text
weekly/week-002/lessons/day-003-risc-v-assembly-and-isa-comparison.md
```

Challenge:

```text
weekly/week-002/challenges/day-003-riscv-decoder.py
```

**Evidence:** the same small computation explained in x86-64 and RISC-V,
decoded instruction fields, and a concise comparison grounded in actual
operations rather than “RISC good, CISC bad.”

### Day 4 — RAM, Locality, and the Memory Wall

Explain why registers, caches, DRAM, and storage form a hierarchy; separate
latency from bandwidth; and measure sequential, strided, and random access
without treating one benchmark as a universal hardware truth.

Lesson:

```text
weekly/week-002/lessons/day-004-ram-locality-and-memory-wall.md
```

Challenges:

```text
weekly/week-002/challenges/day-004-memory-wall-lab.c
weekly/week-002/challenges/day-004-plot.py
```

**Evidence:** reproducible benchmark parameters and output, a runtime versus
working-set plot, and a written explanation of where locality and the memory
wall appear.

### Day 5 — Cache Organization

Calculate tag, set, and offset; distinguish lines, sets, and ways; classify
misses; understand write policies; and observe false sharing and performance
counter limitations.

Lesson:

```text
weekly/week-002/lessons/day-005-cache-organization.md
```

Challenge:

```text
weekly/week-002/challenges/day-005-cache-lab.c
```

**Evidence:** one hand-derived mapping checked by code, compact-versus-padded
measurements, and `perf` evidence or the exact environmental reason PMU
counters were unavailable.

### Day 6 — Alignment, Endianness, and NUMA

Inspect alignment and structure padding, distinguish object layout from wire
format, reconstruct byte order safely, and relate memory-controller topology
and NUMA placement to access cost.

Lesson:

```text
weekly/week-002/lessons/day-006-alignment-endianness-and-numa.md
```

Challenges:

```text
weekly/week-002/challenges/day-006-layout-endian-lab.c
weekly/week-002/challenges/day-006-topology-probe.sh
```

**Evidence:** predicted and observed structure offsets, a safe byte-order
decode, host or VM topology observations, and an explanation of how layout
affects correctness, ABI compatibility, and performance.

### Day 7 — C's Execution Model and Data Types

Use C's abstract machine to distinguish translation from execution and
objects from representations. Reason about integer types, rank, promotions,
conversions, character bytes, floating-point properties, expression
evaluation, and the first boundaries among scope, storage duration, and
lifetime.

Lesson:

```text
weekly/week-002/lessons/day-007-c-execution-model-and-data-types.md
```

Challenge:

```text
weekly/week-002/challenges/day-007-c-machine-model-lab.c
```

**Evidence:** predicted and observed type/conversion results, raw object-byte
observations with appropriate scope, annotated `-O0`/`-O2` assembly, and an
explanation of how type changes operation width, extension, or comparison.

## Workload guide

The nominal budget is about 21 focused hours:

```text
Day 1   3 h   assembly reading, routines, GDB, compiler loop
Day 2   3 h   calls, ABI, frames, unwinding, stack experiment
Day 3   3 h   RISC-V model, decoding, cross-ISA comparison
Day 4   3 h   hierarchy/locality model, benchmark, plot
Day 5   3 h   cache geometry, false sharing, counters
Day 6   3 h   layout, endianness, topology/NUMA
Day 7   3 h   C machine model, conversions, assembly comparison
```

This is a planning estimate, not a pace requirement. Adjust it:

- split GDB or benchmarking work across sessions if observation quality
  starts dropping;
- repeat a lab when prediction and result disagree;
- shorten familiar exposition, but still produce mastery evidence;
- preserve failed `perf`, topology, or cross-toolchain attempts when the
  failure accurately documents an environmental limitation;
- do not mark a day complete merely because its lesson was read.

## Integrated weekly evidence

Keep each artifact small enough to review, but connect it to the same running
model.

### Explain

Without notes, tell this story:

1. an ISA defines software-visible state and operations;
2. an ABI adds calling and interoperability contracts;
3. x86-64 and RISC-V solve the same machine problems with different visible
   instruction styles;
4. loads and stores encounter a hierarchy whose cost depends on locality;
5. caches locate lines through tag, set, and offset and coordinate ownership
   at line granularity;
6. alignment, padding, and byte order shape stored representations;
7. C types constrain values and observable behavior while leaving many
   machine-level choices to the implementation and compiler.

### Draw

Produce one connected diagram containing:

- a C expression with operand types;
- compiler lowering to a plausible instruction sequence;
- ABI argument/result locations;
- register and memory state;
- virtual address to cache-line path;
- line offset/set/tag;
- object bytes, alignment boundary, and byte order;
- labels distinguishing language rule, ABI contract, ISA rule,
  microarchitectural choice, and OS policy.

Also include a separate stack-frame drawing with return address, saved
registers, arguments, locals, `rsp`, and optional `rbp`.

### Observe

Preserve a compact evidence bundle:

- one GDB instruction trace and one frame/backtrace inspection;
- one x86-64/RISC-V comparison from source or decoded instructions;
- memory-wall benchmark data and plot;
- one cache mapping and repeated false-sharing measurements;
- structure offsets, object bytes, and observed endianness;
- topology/NUMA output with environment caveats;
- C limits/conversion output plus annotated unoptimized and optimized
  assembly;
- compiler, kernel, CPU/VM, and relevant tool versions.

An unavailable PMU, cross-compiler, or NUMA interface is valid evidence only
when the exact failure and environment are recorded. It is not evidence for
the hardware behavior the tool could not observe.

### Build

Complete or extend the challenge work so that the final code demonstrates:

- one correct hand-written assembly routine and its C caller;
- one mechanically explained function call across the ABI;
- one decoded or modified RISC-V instruction;
- one reproducible memory-access experiment;
- one cache-address calculation or false-sharing variation;
- one safe layout/byte-order conversion;
- two C functions where a type change causes an explainable generated-code
  difference.

Extensions should preserve a prediction before execution and a written
explanation afterwards.

## Integrated weekly mastery checkpoint

Start with this C function:

```c
long sum_positive(const int *values, size_t count);
```

Then, without relying on “the compiler handles it,” explain the complete
route:

1. What do the parameter and result types require on this C implementation?
2. Where do the two arguments and return value travel under the System V
   AMD64 ABI?
3. Give a plausible x86-64 loop and identify its registers, load width,
   signed comparison, branch, and address calculation.
4. Give the equivalent RISC-V shape and explain its explicit load/store
   behavior.
5. Draw one function activation and the return path.
6. Explain which accesses have spatial or temporal locality.
7. For a stated cache geometry, split one array-element address into tag, set,
   and offset.
8. Explain how element alignment and host byte order affect representation
   without changing the mathematical sum.
9. Identify at least three places where another correct compiler could emit
   different instructions.
10. Explain why none of this supports “one C line equals one instruction.”

Mastery is demonstrated when the explanation crosses every boundary
explicitly:

```text
C semantics → compiler choice → ABI → ISA → addresses/layout
            → cache/hierarchy → observed execution
```

If an arrow is justified only by “the machine knows,” return to that day's
lesson and identify the missing contract or mechanism.

## End-of-module reflection

Record, in your own words:

- strongest understanding;
- remaining confusion;
- connection to Linux, kernel, virtualization, or current work;
- one result that contradicted your prediction;
- next revision action;
- whether to advance, repeat one lab, or extend the module.

Do not auto-fill confidence, completion, hours, or mastery. Those remain the
learner's evidence-based assessment.
