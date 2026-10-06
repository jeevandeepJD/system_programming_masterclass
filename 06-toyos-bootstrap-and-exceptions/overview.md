# Week 6 — ToyOS Bootstrap and Exceptions

**Format:** seven daily sections collected into one mastery package

**Suggested workload:** approximately 21 focused hours, adjustable to mastery

> A kernel starts with fewer guarantees than an ordinary C program. Make each
> guarantee explicit, then preserve enough evidence to debug when one fails.

This week follows the existing freestanding x86-64 kernel in
[`projects/toyos`](../projects/toyos/). You will trace GRUB's Multiboot2 handoff,
the 32-bit bootstrap, long-mode entry, serial diagnostics, exception entry,
TSS/IST stack selection, and symbol-aware fault diagnosis. The package does
not duplicate the kernel: each practical challenge invokes or inspects that
canonical project.

The recurring discipline is:

```text
purpose → prediction → run or inspect → preserve evidence → explain limits
```

“Week” names a coherent mastery module, not a deadline. Do not treat a boot
banner as proof of every preceding claim. Tie each conclusion to source,
ELF metadata, serial output, QEMU status, or debugger state.

## Responsibility map

- **C language:** object representations, pointer and integer operations,
  `volatile`, undefined behavior, and the freestanding implementation.
- **ABI/toolchain:** argument registers, stack alignment, object format,
  relocations, symbol names, and compiler options.
- **x86-64 architecture:** control registers, page-table interpretation,
  descriptor tables, exception vectors, error codes, and `iretq`.
- **ToyOS implementation:** chosen mappings, stacks, logging, frame layout,
  fault policy, tests, and QEMU exit protocol.

An architecture specifies mechanisms; an ABI lets separately built pieces
cooperate; this kernel chooses one implementation and policy.

## Daily roadmap

### Day 1 — Freestanding C and the project contract

Read the build flags and kernel entry contract. Explain what `-ffreestanding`,
`-nostdlib`, `-mno-red-zone`, and the project header do—and do not—guarantee.

- Lesson: [day-001-freestanding-c-and-project-contract.md](sections/day-001-freestanding-c-and-project-contract.md)
- Challenge: `challenges/day-001-project-contract.sh`
- Evidence: compiler/linker flags, undefined-symbol inspection, and a clean
  Week 6 build without changing canonical source.

### Day 2 — ELF, linker, Multiboot2, and GRUB

Follow the image from linker script to Multiboot2 validation and ISO layout.

- Lesson: [day-002-elf-linker-multiboot2-and-grub.md](sections/day-002-elf-linker-multiboot2-and-grub.md)
- Challenge: `challenges/day-002-elf-boot-contract.sh`
- Evidence: ELF header/sections/program headers/symbols, Multiboot2 recognition,
  and the GRUB configuration actually packaged in the ISO.

### Day 3 — From 32-bit entry to 64-bit C

Trace initial page tables, PAE, EFER.LME, paging, GDT selection, far jump,
stack setup, and the System V AMD64 argument handoff.

- Lesson: [day-003-long-mode-pages-gdt-and-stack.md](sections/day-003-long-mode-pages-gdt-and-stack.md)
- Challenge: `challenges/day-003-long-mode-trace.sh`
- Evidence: annotated bootstrap disassembly and the normal serial regression.

### Day 4 — Serial, panic, and assertions

Treat COM1 as the early diagnostic channel and separate an assertion policy
from the architecture's exception machinery.

- Lesson: [day-004-serial-panic-and-assert.md](sections/day-004-serial-panic-and-assert.md)
- Challenge: `challenges/day-004-serial-panic-assert.sh`
- Evidence: normal and expected-assert serial markers plus host exit status.

### Day 5 — IDT, stubs, and one saved frame

Connect an IDT gate to assembly stubs, normalize hardware error-code
differences, and verify the C structure against push order.

- Lesson: [day-005-idt-stubs-and-uniform-frame.md](sections/day-005-idt-stubs-and-uniform-frame.md)
- Challenge: `challenges/day-005-exception-frame.sh`
- Evidence: breakpoint self-test, divide-error test, and frame-layout
  inspection.

### Day 6 — TSS, IST, page faults, and GDB

Explain why a known-good exception stack matters, decode `CR2` and the page
fault error code, and correlate saved `RIP` with symbols and source.

- Lesson: [day-006-tss-ist-page-fault-and-gdb.md](sections/day-006-tss-ist-page-fault-and-gdb.md)
- Challenge: `challenges/day-006-page-fault-diagnosis.sh`
- Evidence: expected page-fault regression, static symbolization, and a
  generated command guide for an optional live GDB session.

### Day 7 — Integrated fault-debug report

Produce one evidence-bounded report and compare ToyOS's exception path with
xv6 concepts and current Linux source landmarks.

- Lesson: [day-007-integrated-fault-debug-report.md](sections/day-007-integrated-fault-debug-report.md)
- Challenge: `challenges/day-007-fault-report.sh`
- Evidence: a reproducible report containing build identity, relevant source
  excerpts, ELF symbols, all Week 6 test markers, and explicit limits.

## Integrated mastery checkpoint

Starting at the deliberate unmapped write in
`projects/toyos/src/kernel.c`, explain:

```text
C access
  → compiler-selected faulting instruction
  → page translation failure
  → vector 14 gate and IST selection
  → assembly register/error-code normalization
  → struct interrupt_frame
  → CR2 and error-bit decoding
  → serial evidence and expected QEMU exit
  → symbol/source correlation in GDB
```

For each arrow, identify architecture, ABI, or ToyOS implementation. Then
state what the evidence does not prove.

## References and version discipline

The repository's
[architecture comparison](../references/ARCHITECTURE_COMPARISON.md) is a
local orientation aid. Optional local source trees under `references/library/`
are examples, not normative specifications. For exact exception, paging, and
descriptor semantics, consult the current Intel 64 and IA-32 Software
Developer's Manual or AMD64 Architecture Programmer's Manual. For comparison,
use current Arm A-profile and RISC-V privileged architecture manuals. For
Linux claims, record the source revision you inspected; paths and details
change.

Do not auto-fill confidence, completion, hours, or mastery. Those remain the
learner's evidence-based assessment.
