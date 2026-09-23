# Week 1 — From Physical State to CPU Control

**Format:** seven daily sections collected into one weekly module

**Suggested workload:** approximately 21 focused hours, adjustable to mastery

> This week reconstructs the path from a physical state that can represent
> one bit to a processor that can fetch instructions, branch, cross privilege
> boundaries, and enter an operating-system handler.

## Why these seven days belong together

The modern CPU is easy to memorize as a block diagram and difficult to
understand as a causal system. This week builds it in dependency order:

```text
physical state and encoding
    ↓
signed values and bit-level transformations
    ↓
Boolean gates
    ↓
arithmetic, selection, and storage
    ↓
minimal stored-program CPU
    ↓
instruction-set contract and machine code
    ↓
control flow, exceptions, and privilege
```

Every section answers two questions:

1. What problem forced this abstraction to exist?
2. Where does it physically or operationally exist in a running computer?

## Daily roadmap

### Day 1 — Foundations: From State to Meaning

Physical state, voltage ranges, noise margin, binary patterns, positional
notation, encodings, information, and the observation that identical bytes
can be numbers, text, pixels, or instructions.

### Day 2 — Signed Numbers and Bitwise Reasoning

Two's complement, modular arithmetic, masks, packed register fields,
carry/overflow, extension, C promotion preview, and the bridge toward an ALU.

### Day 3 — Boolean Logic and Physical Gates

Boole, Shannon, relays, vacuum tubes, transistors, truth tables, six common
gates, De Morgan's laws, universal gates, combinational logic, and a
half-adder preview.

### Day 4 — Adders, Selection, and Memory

Half/full adders, ripple carry, muxes, decoders, encoders, feedback, latches,
flip-flops, clocks, registers, and a full physical-to-logical trace of
`3 + 5 = 8`.

### Day 5 — Building a Minimal CPU

Architectural state, PC, IR, registers, ALU, instruction/data memory,
datapath versus control, a tiny ISA, fetch/decode/execute, and cycle-visible
simulation.

### Day 6 — Instruction Sets and Machine Code

ISA versus microarchitecture, opcodes, operands, immediates, addressing,
load/store design, careful RISC/CISC comparison, x86-64 and RISC-V examples,
ELF disassembly, and fictional ISA design.

### Day 7 — Control Flow, Exceptions, and Privilege

Flags, branches, loops, calls, user/kernel privilege, synchronous exceptions,
asynchronous interrupts, vectoring, saved context, return from exception,
Unix signals, and system-call entry.

## Weekly evidence

Do not treat reading the combined PDF as completion. Collect evidence:

- **Explain:** reconstruct each day's central causal chain without notes.
- **Draw:** produce the physical-state-to-CPU diagram and the exception-flow
  diagram.
- **Observe:** run Linux disassembly, tracing, simulator, and safe exception
  experiments.
- **Build:** complete the register-mask lab, digital-circuit predictions,
  tiny CPU extensions, fictional ISA exercise, and machine-code lab.

## Weekly mastery checkpoint

Starting with the physical voltage state of a bit, explain how a CPU can:

1. represent and add `3 + 5`;
2. preserve the result;
3. fetch and decode an instruction requesting the operation;
4. choose the next instruction through control flow;
5. stop ordinary user execution and safely enter an operating-system
   handler.

If any arrow requires “the computer just knows,” return to the corresponding
daily section and make that mechanism explicit.
