# How Masterclass Materials Are Generated

## Purpose

The notes are synthesized teaching material, not a replacement for primary
documentation. Each module is built from an approved curriculum scope,
current specifications/source, targeted books, and experiments that are run
on the practice VM.

The goal is a natural first-principles explanation with reproducible evidence:

```text
question → source research → derive the model → predict
         → build/run → observe → correct → explain → connect
```

## Source hierarchy

Work offline-first: begin with the organized local library, repository
references, installed manuals/man pages, local source trees, and experiments.
Internet access is a targeted fallback for missing material and
version-sensitive verification—not the default research step.

When sources disagree, use this order:

1. Current official specification, standard, or upstream source
2. Current official project/kernel documentation
3. Upstream mailing-list discussion and commit history
4. Maintainer-quality technical references such as LWN
5. Selected textbooks
6. Older training slides or notes, used only for stable concepts/diagrams
7. Generated explanation, which must be verified against the layers above

Version-sensitive kernel APIs, structures, Kconfig symbols, source paths, and
tool commands are always checked against the current target environment.
If offline sources cannot establish a current, authoritative answer, consult
the relevant upstream specification, documentation, source, or history rather
than lowering the lesson's quality.

## Generation workflow

### 1. Define scope

- Start from `metadata/COURSE_ROADMAP.md`.
- Read the corresponding objectives in `metadata/curriculum-tracker.docx`.
- State what the module must teach and what remains deliberately deferred.
- Keep chip/HDL, cloud, or distributed breadth only when it strengthens the
  kernel/system-programming objective.

### 2. Build the causal explanation

- Begin with the engineering problem.
- Derive the abstraction rather than presenting terminology first.
- Separate specification, encoding, physical implementation, runtime state,
  compiler behavior, kernel policy, and user-space interpretation.
- Connect the topic to CPU, Linux, hardware, and later infrastructure where
  technically relevant.

### 3. Add evidence

Each major topic should include the appropriate subset of:

- prediction exercises;
- interactive MCQs;
- C/assembly/Python/SystemVerilog/ngspice labs;
- GDB, sanitizer, Valgrind, strace, perf, ftrace, eBPF, or QEMU evidence;
- diagrams and execution traces;
- deliberately injected failures;
- comparison with xv6/Linux/upstream source.

Bare commands are not considered exercises. Every challenge explains what it
teaches, what to predict or implement, how to run it, and what evidence to
record.

### 4. Validate

- Compile with strict warnings where appropriate.
- Run safe/default paths and self-tests.
- Validate expected-failure or sanitizer modes separately.
- Check HTML JavaScript syntax and hide answers until submission.
- Build consolidated PDFs and verify relative challenge links.
- Run `git diff --check` and IDE diagnostics.
- Do not mark learner mastery automatically because generated artifacts pass.

### 5. Publish

- Keep learner-facing PDFs at numbered topic-folder roots.
- Keep editable sections and challenges beside their PDF.
- Keep metadata, references, tools, and archive content separate.
- Commit coherent milestones; preserve experimental structures on backup
  branches when necessary.

## Primary references

### Computing and architecture

- Charles Petzold, *Code*
- Patt and Patel, *Introduction to Computing Systems*
- Bryant and O'Hallaron, *Computer Systems: A Programmer's Perspective*
- Intel 64 and IA-32 Software Developer's Manuals
- AMD64 Architecture Programmer's Manuals
- Arm Architecture Reference Manual for A-profile architecture
- Arm AAPCS64
- RISC-V ISA specifications
- RISC-V ELF psABI
- System V AMD64 ABI
- [`references/ARCHITECTURE_COMPARISON.md`](../references/ARCHITECTURE_COMPARISON.md)
  as the course's reusable comparison map; primary manuals remain authoritative

### C and toolchain

- ISO C standard drafts and WG14 defect discussions where needed
- GCC and Clang documentation
- GNU binutils, ELF, linker, and dynamic-loader documentation
- *The Linux Programming Interface*

### Operating systems

- *Operating Systems: Three Easy Pieces*
- xv6 source and book
- Linux man-pages
- QEMU and GDB documentation
- Limine boot protocol/documentation for the guided ToyOS bootstrap

### Linux kernel

- Current Linux source and `Documentation/`
- `MAINTAINERS`, lore.kernel.org, subsystem trees, and relevant commits
- LWN kernel articles
- Linux Kernel Programming, 2nd edition, as a companion—not authority
- Kernel selftests, KUnit, LTP, kvm-unit-tests, and subsystem tests

### Hardware/RTL tools

- ngspice documentation
- SystemVerilog language/tool documentation
- Icarus Verilog, Verilator, GTKWave, and Yosys documentation
- NAND2Tetris only as a simplified conceptual supplement

## Important limitations

- Generated lessons can contain mistakes; experiments and primary sources are
  the correction mechanism.
- Simplified CPU, cache, TLB, allocator, and scheduler models demonstrate
  causality but do not represent production complexity.
- Simulation is not silicon measurement.
- Passing a quiz demonstrates recall, not implementation mastery.
- A working toy kernel does not demonstrate production-kernel competence.
- Upstream review and real debugging evidence remain required outcomes.
