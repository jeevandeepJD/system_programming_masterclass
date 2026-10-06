# Day 6 — TSS, IST, Page Faults, and GDB Diagnosis

**Target time:** approximately 3 hours

> If the current stack contributed to the fault, should the fault handler
> trust it?

The x86-64 Task State Segment and Interrupt Stack Table let selected IDT gates
switch to a known stack. ToyOS uses IST entry 1 for NMI, double fault, and page
fault, then exposes enough state to diagnose a deliberate unmapped write.

---

## 1. Runtime GDT and TSS

`arch.c` creates a five-quadword GDT: null, kernel code, kernel data, and a
two-slot 64-bit TSS descriptor. Unlike the bootstrap GDT, this table supports
loading `TR` with selector `0x18`.

The TSS stores:

- privilege stack pointers `rsp[0..2]`;
- seven IST pointers;
- an I/O-map offset.

ToyOS sets `rsp[0]` and `ist[0]` to the top of one aligned 16 KiB
`interrupt_stack`. It sets `iomap_base` beyond the TSS body and executes
`ltr`.

This does not implement hardware task switching. In 64-bit mode, ToyOS uses
the TSS for stack-selection metadata.

## 2. What IST buys—and what this implementation shares

An IDT gate's low IST bits select one of the TSS IST pointers. ToyOS selects
IST index 1 for vectors 2, 8, and 14:

```text
NMI, double fault, page fault → interrupt_stack
```

A known stack can preserve diagnostic ability if the interrupted `RSP` is
unusable. However, this implementation shares one IST stack among three
vectors and is single-core. It does not provide per-CPU stacks, guard pages,
or robust nesting policy.

Double fault is vector 8. It is not simply “two exceptions happened.” Exact
classification depends on combinations of exceptions during delivery; use
the current architecture manual's exception-combination rules. A failure
during double-fault delivery can lead to shutdown/triple-fault behavior,
which ToyOS does not test here.

## 3. Deliberate page fault

The expected page profile writes to linear address `0x70000000`. Bootstrap
page tables identity map only the first 1 GiB, ending at `0x3fffffff`.
Therefore the chosen address lies outside the initial mapping.

The C expression uses a `volatile` lvalue so the compiler must emit the
observable access under its abstract-machine rules. The invalid dereference
would be undefined behavior in general C reasoning; this controlled kernel
test relies on inspected code generation and the configured x86 translation
state to provoke vector 14.

## 4. Decoding page-fault evidence

On vector 14, ToyOS reads `CR2` for the faulting linear address and decodes
low error-code bits:

- bit 0: protection violation versus non-present;
- bit 1: write versus read;
- bit 2: user versus supervisor;
- bit 3: reserved paging-structure bit;
- bit 4: instruction fetch.

For this test, predict:

```text
address=0x0000000070000000
not-present write supervisor
```

Treat additional error bits and feature-dependent semantics according to the
current Intel or AMD manual. ToyOS prints only this bounded subset.

`CR2` supplies the address; saved `RIP` identifies the instruction context;
the error code classifies the attempted access. None alone is a complete
diagnosis.

## 5. Symbolized static diagnosis

The linked ELF retains debug information and symbols because the build uses
`-g3`. Even without a live VM, use:

```text
symbol address/range → decoded instruction → source line
```

`nm -n`, `objdump -dS`, and `addr2line` can correlate the injected path.
Optimized code complicates one-to-one source mapping: functions can inline,
statements can combine, and one source line can span several instructions.
Report the observed mapping rather than claiming every instruction uniquely
belongs to one C statement.

## 6. Live GDB workflow

The project provides two coordinated targets:

1. `make debug-fault-week6-page` builds the fault image and starts QEMU paused
   with a GDB stub on TCP port 1234.
2. `make gdb-fault` loads `build/week6-page/toyos.elf`, connects, breaks at
   `interrupt_handler`, and continues.

At the breakpoint, useful questions are:

- What are `frame->vector`, `frame->error`, and `frame->rip`?
- Does `$cr2` equal the injected address?
- What instruction is at `frame->rip`?
- Which source line and symbol contain it?
- Which stack contains `frame`, and is it within `interrupt_stack`?

The challenge wrapper does not start a persistent debug server. It prints a
purpose-labelled command guide and performs deterministic static checks. Use
the two-terminal live workflow only when you can supervise and stop QEMU.

## 7. Practical challenge

Use `challenges/day-006-page-fault-diagnosis.sh` from the repository root.

**Purpose:** produce repeatable page-fault evidence and prepare a symbol-aware
live diagnosis without leaving a paused VM behind.

**Predict:** write the vector, CR2 value, decoded low error bits, expected
host status, and likely source function before running.

**Run:** the wrapper invokes the canonical expected-page regression, locates
relevant symbols/disassembly in the exact fault ELF, verifies GDB script
paths, and prints the supervised two-terminal procedure.

**Evidence:** preserve the exception line, page-fault line, expected marker,
symbol addresses, and source/disassembly correlation. If you perform the
optional GDB session, record the ELF build path and exact frame values.

## 8. Architecture lens at fault reporting

At this real boundary, all three architectures report fault context
differently:

- x86-64 uses vector 14, an error code, `CR2`, IDT entry, and optional IST.
- AArch64 reports syndrome in `ESR_ELx`, fault address in `FAR_ELx`, and return
  state in `ELR_ELx`/`SPSR_ELx`; vector choice depends on origin and stack.
- RISC-V reports cause in `scause`, address/detail in `stval`, and return PC
  in `sepc`, entering through `stvec`.

The shared debugging method is cause + address + saved PC + symbolized binary.
The registers, encodings, and stack mechanisms are architectural.

---

## Mastery: Explain · Observe · Build

### Explain

- Why can IST preserve a diagnostic path after stack corruption?
- Why is sharing one IST stack a limitation?
- Why are CR2, error code, and RIP complementary?
- Why must GDB load the exact ELF used by QEMU?

### Observe

Annotate the page-fault serial line field by field. For each field, state its
architectural source and the ToyOS code that formats it.

### Build

Write a fault worksheet containing:

```text
image/profile | vector | error | address | saved PC | symbol/source
prediction | observation | interpretation | evidence limit
```

Complete it from actual output. Do not invent a live-debug value if you ran
only the static challenge.

## Evidence limits

The test establishes one non-present supervisor write under QEMU TCG. It does
not validate protection faults, user faults, execute faults, nested faults,
real hardware, or robust double-fault recovery.
