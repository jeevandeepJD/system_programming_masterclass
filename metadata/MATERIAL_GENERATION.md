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
- Installed QEMU system-emulator documentation and `qemu-system-x86_64`
  command help
- Installed GRUB2 manuals/tool help, including `grub-file`,
  `grub-mkrescue`, and the Multiboot2 boot contract used by ToyOS
- Installed GDB documentation for QEMU remote debugging
- The repository's [`projects/toyos/grub.cfg`](../projects/toyos/grub.cfg),
  linker scripts, Makefile, serial logs, ELF/map artifacts, profile-specific
  GDB scripts, and checked QEMU harness as reproducible evidence for the
  prepared Week 6–9 path
- The [Week 8 overview](../08-user-mode-syscalls-and-ipc/overview.md) and
  [Week 9 overview](../09-filesystems-and-virtual-devices/overview.md) as the
  scope boundaries for their source packages: Week 8 uses educational
  `int 0x80`, not `SYSCALL`/`SYSRET`; Week 9 implements ramfs/VFS and QEMU EDU
  PCI/MMIO, not a block/page-cache/journaling or IRQ/DMA stack
- Limine protocol/documentation only if a later milestone deliberately
  evaluates it as an alternative boot path; do not describe the current
  GRUB2/Multiboot2 implementation as Limine-based
- The [Week 10 overview](../10-linux-processes-and-signals/overview.md),
  [Week 11 overview](../11-threads-and-concurrency/overview.md), and shared
  [Linux system-programming lab](../projects/linux-system-programming/) for
  bounded process, signal, pthread, synchronization, and memory-ordering
  evidence
- Installed Linux man-pages and local libc/pthread documentation are the
  first references for these offline-first labs. `strace`, GDB, Valgrind,
  ASan/UBSan, and ThreadSanitizer are optional observation tools, not package
  prerequisites; ThreadSanitizer is unavailable on the prepared host/runtime.

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
- ToyOS Week 8's one-process syscall/pipe model is not a complete process,
  IPC, or signal implementation.
- ToyOS Week 9's ramfs and polled EDU MMIO path are not evidence of a real
  storage stack, durable filesystem, interrupt-driven driver, or DMA support.
- Linux system-programming timing, scheduling, PID, trace, and relaxed-memory
  observations vary by run and architecture. Deterministic tests check
  invariants, not every interleaving.
- An unavailable or skipped ThreadSanitizer run is not evidence that the
  intentional Week 11 race is safe; the C data race remains undefined
  behavior.
- Upstream review and real debugging evidence remain required outcomes.
