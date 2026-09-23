# Source Materials Catalog

This directory catalogs the learning material selected from:

- `/home/jd/Downloads/Books-20260916T113723Z-1-001.zip`
- relevant standalone PDFs in `/home/jd/Downloads/`
- additional EPUBs placed in the repository root and reviewed on
  22 September 2026

The imported files live under `source-materials/library/`. That directory is
local-only and Git-ignored by default because it contains third-party books
and imported code. This catalog may be committed safely.

These are references, not a cover-to-cover reading queue. Daily lessons
should select only the pages or examples needed for the current tracker topic.

## Books

### Foundations, architecture, C, and toolchain

| Local filename | Best use in curriculum |
|---|---|
| `books/01-foundations-and-architecture/code-hidden-language-computer-hardware-software-charles-petzold.pdf` | Weeks 1–5: physical state, binary, gates, storage, and the path to a CPU |
| `books/01-foundations-and-architecture/introduction-to-computing-systems-patt-patel-2e.pdf` | Weeks 1–10: bits, logic, LC-3-style architecture, ISA, assembly, and OS bridge |
| `books/01-foundations-and-architecture/computer-systems-programmers-perspective-bryant-ohallaron-3e-global.pdf` | Weeks 6–33: machine representation, assembly, linking, memory hierarchy, processes, virtual memory, concurrency |

### Operating systems and Linux interfaces

| Local filename | Best use in curriculum |
|---|---|
| `books/02-os-and-linux/operating-systems-three-easy-pieces-arpaci-dusseau.pdf` | Weeks 22–33: virtualization, processes, scheduling, memory, concurrency, persistence |
| `books/02-os-and-linux/linux-programming-interface-michael-kerrisk.pdf` | Weeks 24–37: processes, signals, file descriptors, IPC, threads, sockets, and Linux APIs |
| `books/02-os-and-linux/hands-on-system-programming-with-linux-kaiwan-billimoria-2018.pdf` | Stages 5, 7, 9, and 10: Linux architecture, virtual memory, allocation/debugging, credentials/capabilities, process execution and creation, signals, timers, pthreads, scheduling, and advanced file I/O |

### Kernel and driver development

| Local filename | Best use in curriculum |
|---|---|
| `books/03-kernel-and-drivers/linux-device-drivers-development-john-madieu-2017.pdf` | Weeks 38–47: modules, kernel APIs, device model, character devices, interrupts, and drivers |
| `books/03-kernel-and-drivers/linux-driver-development-embedded-processors-alberto-liberal-2018.pdf` | Weeks 45–47 and later hardware labs: embedded Linux driver workflow |
| `books/03-kernel-and-drivers/mastering-linux-device-driver-development-john-madieu-2020.pdf` | Weeks 41–47: synchronization, device model, MMIO, DMA, and advanced driver work |
| `books/03-kernel-and-drivers/linux-kernel-programming-kaiwan-billimoria-2e-2024.pdf` | Preferred modern kernel-programming companion for Stages 11–12: building Linux 6.x, modules, process/task internals, memory allocation, scheduling, synchronization, RCU, and barriers |
| `books/03-kernel-and-drivers/linux-kernel-programming-part2-char-drivers-synchronization-kaiwan-billimoria-2021.pdf` | Stages 11 and 13: misc/character drivers, user-kernel interfaces, procfs/sysfs/debugfs/netlink/ioctl, MMIO, interrupts, timers, workqueues, and locking |
| `books/03-kernel-and-drivers/linux-kernel-debugging-kaiwan-billimoria-2022.pdf` | Stages 11–13: printk/dynamic debug, kprobes, KASAN/UBSAN, SLUB debug, kmemleak, Oops analysis, lock debugging, ftrace, perf, KGDB, kdump/crash, static analysis, and coverage |

> Kernel books age quickly. Use them for concepts and lab ideas, then verify
> every API against the documentation and source for the kernel being built.
> The 2024 second edition is the strongest local starting point for general
> kernel work, but it targets the Linux 6.1 era; this VM is newer, so APIs,
> Kconfig options, tools, and examples still require upstream verification.
> The 2021–2022 driver/debugging volumes are valuable technique catalogs but
> are even more likely to contain version-specific details.
>
> These four entries were converted from EPUB to searchable A4 PDFs with
> page numbers using Calibre 9.13.0. The original EPUB files are retained
> beside the PDFs as source copies.

### Optional language reference

| Local filename | Best use in curriculum |
|---|---|
| `books/04-optional-languages/systems-programming-with-rust-ken-youens-clark-2022.pdf` | Optional side track after strong C foundations; not part of the critical path |

## Notes

| Local filename | Use |
|---|---|
| `notes/linux-kernel-programming-2e-reading-notes.docx` | Supplemental notes for kernel modules and internals; verify against current kernel source |

## Lab collections

| Directory | Contents and likely tracker use |
|---|---|
| `labs/hands-on-system-programming-with-linux/` | C examples and assignments for memory, processes, signals, threads, scheduling, capabilities, file I/O, and `mmap` |
| `labs/ipc-experiments/` | Pipes, shared memory, message queues, signals, pthread synchronization, and custom allocator experiments |
| `labs/linux-debugging-and-kernel-tracing-labs/` | GDB, sanitizers, kernel debugging, KASAN, kmemleak, kprobes, crash/kdump, DAMON, ioctl, perf, and flame graphs |
| `labs/kernel-training-exercises/` | Small process, signal, pthread, and kernel-training exercises |
| `labs/linux-system-programming-exercises/` | Earlier personal Linux system-programming exercises and assignments |

Only source and reference artifacts were imported. Nested Git repositories,
editor swap files, object files, shared libraries, executables, core dumps,
and other generated binaries were deliberately removed.

## Instructor-led training material

The KaiwanTECH PDFs under `library/training/kaiwantech/` came from an
instructor-led kernel/system-programming course. They are useful because they
are concise, diagram-heavy companions to the larger books. They are
references, not replacements for current kernel documentation and source.

### Linux kernel internals modules (2024)

| Module | Local filename | Curriculum use |
|---:|---|---|
| 01 | `training/kaiwantech/kernel-internals-2024/01-linux-system-architecture.pdf` | Stages 7, 10, and 11: Unix/Linux architecture, user/kernel boundary, system calls, execution contexts |
| 02 | `training/kaiwantech/kernel-internals-2024/02-kernel-source-tree.pdf` | Stage 11: navigating subsystems, architecture trees, headers, documentation, Kconfig/Kbuild locations |
| 03 | `training/kaiwantech/kernel-internals-2024/03-modern-kernel-build.pdf` | Stage 11: configuring, building, installing, and booting a modern kernel |
| 04 | `training/kaiwantech/kernel-internals-2024/04-practical-loadable-kernel-modules.pdf` | Stages 11 and 13: module lifecycle, build flow, parameters, symbols, logging, and practical LKM workflow |
| 06 | `training/kaiwantech/kernel-internals-2024/06-kernel-architecture-and-task-structure.pdf` | Stage 11: process/interrupt context, kernel architecture, `task_struct`, process state |
| 08 | `training/kaiwantech/kernel-internals-2024/08-linux-cpu-scheduler.pdf` | Stages 7 and 11: scheduler concepts, policies, run queues, priorities, affinity, and kernel implementation |
| 09 | `training/kaiwantech/kernel-internals-2024/09-kernel-synchronization-locking.pdf` | Stages 9 and 11: races, atomicity, mutexes, spinlocks, interrupt interaction, and locking rules |

Modules 05 and 07 were not included in the supplied material. Their absence
is preserved in the numbering instead of silently renumbering the course.

These decks carry 2024 copyright markings, but exact APIs and implementation
details must still be checked against the target kernel.

### Memory-management series (legacy/conceptual)

| Part | Local filename | Main topics |
|---:|---|---|
| 1 | `training/kaiwantech/memory-management-legacy/01-virtual-memory-and-paging.pdf` | Virtual-memory motivation, paging, translation, page tables, allocator overview |
| 2 | `training/kaiwantech/memory-management-legacy/02-kernel-and-process-memory-segments.pdf` | Process mappings, user/kernel split, code/data/heap/stack and kernel segments |
| 3 | `training/kaiwantech/memory-management-legacy/03-physical-memory-and-kernel-segment.pdf` | Physical-memory organization and kernel virtual-address-space concepts |
| 4 | `training/kaiwantech/memory-management-legacy/04-page-cache-watermarks-oom-and-vmas.pdf` | Page cache introduction, watermarks, OOM behavior, and VMAs |

Parts of this series are marked 2000–2015. Use them for diagrams, historical
context, and stable concepts only. Do **not** rely on their structures,
algorithms, function names, tunables, or source paths without checking the
current kernel.

### Containers and resource control

| Local filename | Use |
|---|---|
| `training/kaiwantech/containers/cgroups-training-slides.pdf` | Stage 17: cgroup motivation, hierarchy, controllers, process grouping, and resource management |
| `training/kaiwantech/containers/cgroups-v2-quick-notes.pdf` | Current-oriented cgroup v2 quick reference centered on `/sys/fs/cgroup`; verify controller and delegation details against current kernel/systemd docs |

### Debugging, workflow, and reports

| Local filename | Use |
|---|---|
| `training/kaiwantech/debugging/programmers-checklist-seven-rules.pdf` | Cross-cutting code-review checklist: API errors, warnings, input validation, assertions, logging, and cleanup |
| `training/kaiwantech/debugging/kdump-crash-quick-notes.pdf` | Stages 11–13: introductory kdump/kexec/crash setup and panic-dump workflow; distribution configuration is likely dated |
| `training/kaiwantech/tooling/better-makefile.pdf` | Stages 5 and 11: reusable userspace/LKM Makefile workflow, warnings, analysis, debug and sanitizer targets |
| `training/kaiwantech/tooling/git-basics.pdf` | Supporting workflow reference; useful but not part of the systems-concept critical path |
| `training/kaiwantech/reports/linux-kernel-report-oss-europe-2023-jonathan-corbet.pdf` | Historical snapshot of kernel-development statistics, community, process, and concerns in 2023—not current-state authority |

## Selection decisions

Included:

- resources aligned with the authoritative 66-week tracker,
- foundational computing and architecture books,
- OS/Linux programming references,
- kernel/driver references,
- source-code labs that can support hands-on daily work.
- instructor-led Linux architecture, kernel build/module, scheduler,
  synchronization, memory-management, cgroup, debugging, and workflow
  material aligned to later curriculum stages.

New-material assessment:

- **Hands-On System Programming with Linux (2018):** broad and practical;
  overlaps the existing lab collection from the same book and adds the
  missing explanatory narrative.
- **Linux Kernel Programming, 2nd ed. (2024):** highly relevant and the most
  current local kernel reference; use as the primary companion for kernel
  build, modules, internals, memory, scheduler, and synchronization.
- **Linux Kernel Programming Part 2 (2021):** directly matches the explicit
  driver track, especially character drivers, user-kernel interfaces, MMIO,
  interrupts, workqueues, and synchronization.
- **Linux Kernel Debugging (2022):** directly supports the tracing/debugging
  labs and adds structured coverage of sanitizers, probes, Oops/panic
  analysis, ftrace, KGDB, and kdump/crash.

Excluded:

- personal/religious/fiction/finance notes unrelated to the curriculum,
- CompTIA Security+ notes because this course is not a certification track,
- duplicate copies of books,
- the unusually large duplicate CS:APP PDF,
- three new root-level PDFs whose SHA-256 hashes exactly matched the existing
  CS:APP, OSTEP, and TLPI library copies,
- generated binaries, nested repository metadata, swap files, and crash dumps.

## How daily lessons should use this library

1. Start from the current tracker objective.
2. Choose at most one primary reading and one supplemental reference.
3. Cite the exact chapter/page range in the daily lesson.
4. Prefer a short targeted reading followed by an experiment.
5. Treat imported example code as something to inspect, build, break, and
   explain—not as an answer to copy blindly.
