# Day 7 — Integrated Fault-Debug Report and Systems Comparison

**Target time:** approximately 3 hours

> A fault report is useful only when another person can reproduce its evidence,
> distinguish fact from inference, and see where the model stops.

Today combines the Week 6 artifacts into one bounded report. The goal is not
more kernel code. It is a defensible chain from C source to architectural
fault state, exception entry, diagnostics, and symbolized location.

---

## 1. Start with an evidence question

Use the deliberate page fault as the main case:

> Why did the Week 6 page-fault profile stop, and what evidence identifies the
> faulting access and handler path?

Keep the assertion and divide profiles as controls. The assertion reaches
panic without IDT exception entry. Divide uses exception vector 0 without a
hardware error code. Together they help distinguish mechanisms.

## 2. Report structure

A strong report has seven parts:

1. **Artifact identity:** repository path, profile, ELF path, and tool versions
   relevant to interpretation.
2. **Prediction:** vector, address, access class, frame shape, marker, status.
3. **Static evidence:** source excerpts, linker map/symbols, disassembly, and
   exact debug ELF.
4. **Dynamic evidence:** serial lines, expected markers, QEMU statuses.
5. **Causal explanation:** each transition labelled C, ABI, architecture, or
   ToyOS implementation.
6. **Comparison:** xv6 concept and Linux source landmarks, with revision
   caveats.
7. **Limits:** untested paths and claims the evidence cannot support.

Do not paste an unexplained command transcript. Every command needs a purpose,
prediction, and interpretation.

## 3. Reconstruct the complete ToyOS path

```text
kernel_main chooses EXPECT_FAULT == 2
  → volatile store targets 0x70000000
  → compiler emits a memory-write instruction
  → current page tables have no translation
  → x86 raises #PF, records CR2 and error code
  → IDT vector 14 selects IST1 and isr14
  → stub adds vector; common code saves registers
  → RDI points to uniform interrupt_frame
  → C handler prints vector/error/RIP and decodes CR2
  → expected-fault policy emits PASS and exits QEMU
  → host harness checks status and exact marker
```

Qualify two links carefully. C source alone does not prove the emitted
instruction; inspect the fault ELF. Source and static page-table construction
support the mapping prediction; the dynamic page-fault output confirms the
tested result under QEMU.

## 4. Compare with xv6 at the concept level

xv6 is valuable because its small trap path exposes the same categories:
architecture entry, assembly save area, C dispatcher, cause decode, and
kernel policy. But the commonly taught xv6-riscv tree is RISC-V, not x86-64.

Conceptual correspondence:

```text
ToyOS IDT gate/stub     ↔ RISC-V stvec trampoline/vector entry
interrupt_frame         ↔ trapframe/register save area
vector + CR2 + error    ↔ scause + stval + sepc
interrupt_handler       ↔ trap dispatcher
panic/expected exit     ↔ kernel policy after cause decode
```

This is not field-for-field equivalence. RISC-V records trap metadata in CSRs
and software saves registers. It has no x86 IDT, hardware error-code stack
word, or IST.

If a local xv6 source tree is present, record its exact path and revision
before citing a function. Otherwise use the current upstream xv6-riscv source
and name the revision. Never claim that a remembered filename proves current
behavior.

## 5. Compare with Linux without pretending ToyOS is Linux

Linux's x86 exception path has analogous layers at much greater scale:

- architecture entry assembly under `arch/x86/entry/`;
- x86 trap/fault handling under `arch/x86/kernel/` and `arch/x86/mm/`;
- generic memory-fault policy in `mm/`;
- diagnostic symbolization using kernel symbols and debug information.

Exact files and helper names change. Inspect a current source revision before
making a line-level claim. Begin at the architecture entry, identify the
`pt_regs`-style saved state, follow page-fault dispatch into memory-management
policy, and note where user faults, kernel faults, fixups, signals, and panic
paths diverge.

ToyOS has none of Linux's process address spaces, demand paging, exception
tables, signals, locking, per-CPU entry stacks, or production recovery policy.
Its expected page fault exits the VM.

## 6. Architecture comparison at the diagnosis boundary

The evidence tuple generalizes:

```text
cause + fault address/detail + saved PC + saved registers + exact symbols
```

Its realization differs:

- x86-64: vector/error, `CR2`, saved `RIP`, IDT/IST;
- AArch64: `ESR_ELx`, `FAR_ELx`, `ELR_ELx`, vector-table slot;
- RISC-V: `scause`, `stval`, `sepc`, `stvec`.

The C-level handler structure, naming, and policy are OS implementation. The
calling bridge from assembly is an internal kernel ABI layered on the machine
architecture.

## 7. Practical challenge

Use `challenges/day-007-fault-report.sh` from the repository root.

**Purpose:** generate a reproducible evidence bundle from the canonical Week
6 project without changing it.

**Predict:** write one expected marker and host status for normal, divide,
page, and assertion profiles. Predict the page-fault address and decode.

**Run:** the wrapper records tool versions, selected source contracts, ordered
symbols, disassembly around fault injection, and the complete existing Week 6
test suite into a temporary report file. It prints the report path and key
summary lines.

**Evidence:** retain the generated report if you want a learning artifact;
it is intentionally placed under `/tmp`, outside the repository. Add your
interpretation separately. Generated output is evidence, not the explanation.

## 8. Separate observation from inference

Use verbs precisely:

- **source states** a selected IST index;
- **ELF contains** a symbol at an observed address;
- **objdump decodes** bytes as an instruction;
- **QEMU emitted** serial bytes and a host status;
- **GDB observed** a register in a specific stopped VM;
- **we infer** the causal link from these compatible facts.

Avoid “proved the kernel is correct.” A finite set of tests supports only the
tested properties under recorded conditions.

## 9. Current manuals and optional local material

Normative details belong to current architecture and ABI documents:

- Intel 64 and IA-32 SDM or AMD64 Architecture Programmer's Manual;
- System V AMD64 ABI;
- current Multiboot2 and ELF documentation;
- Arm A-profile architecture reference for AArch64 comparison;
- RISC-V privileged specification for xv6/RISC-V comparison.

Use [`references/ARCHITECTURE_COMPARISON.md`](../../references/ARCHITECTURE_COMPARISON.md)
as a local map. Optional source and lab material under `references/library/`
can offer examples, but record provenance and do not let examples override
current manuals or source.

---

## Integrated mastery: Explain · Draw · Observe · Build

### Explain

Without notes, narrate the page-fault chain from C expression to checked host
status. At every boundary say architecture, ABI, or implementation.

### Draw

Draw three side-by-side exception paths for ToyOS x86-64, xv6-riscv, and
Linux x86-64. Use solid arrows for observed Week 6 evidence and dashed arrows
for conceptual comparison. Do not draw identical frame layouts.

### Observe

Audit the generated report:

- Does it identify the exact ELF?
- Does it preserve all four expected outcomes?
- Does it include enough source and symbol context?
- Are inferred claims visibly separated?
- Are tool versions and limitations recorded?

### Build

Write a two-page incident note answering:

1. What happened?
2. Which instruction and address were involved?
3. How did the machine enter the handler?
4. Which state was hardware-created versus software-created?
5. Why was the result expected?
6. What would a production kernel need beyond ToyOS?
7. Which claim remains least certain, and what next observation would narrow
   it?

## Final bridge

Week 6 has established boot and exception observability. Week 7 can build
memory allocation, mappings, interrupts, and kernel-task scheduling on that
base. Ring 3, user processes, syscalls, and isolated address spaces are not
present yet.

Do not mark progress automatically. Preserve evidence first, then assess
mastery yourself.
