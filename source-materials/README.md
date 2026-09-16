# Source Materials Catalog

This directory catalogs the learning material selected from:

- `/home/jd/Downloads/Books-20260916T113723Z-1-001.zip`
- relevant standalone PDFs in `/home/jd/Downloads/`

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

### Kernel and driver development

| Local filename | Best use in curriculum |
|---|---|
| `books/03-kernel-and-drivers/linux-device-drivers-development-john-madieu-2017.pdf` | Weeks 38–47: modules, kernel APIs, device model, character devices, interrupts, and drivers |
| `books/03-kernel-and-drivers/linux-driver-development-embedded-processors-alberto-liberal-2018.pdf` | Weeks 45–47 and later hardware labs: embedded Linux driver workflow |
| `books/03-kernel-and-drivers/mastering-linux-device-driver-development-john-madieu-2020.pdf` | Weeks 41–47: synchronization, device model, MMIO, DMA, and advanced driver work |

> Kernel books age quickly. Use them for concepts and lab ideas, then verify
> every API against the documentation and source for the kernel being built.

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

## Selection decisions

Included:

- resources aligned with the authoritative 66-week tracker,
- foundational computing and architecture books,
- OS/Linux programming references,
- kernel/driver references,
- source-code labs that can support hands-on daily work.

Excluded:

- personal/religious/fiction/finance notes unrelated to the curriculum,
- CompTIA Security+ notes because this course is not a certification track,
- duplicate copies of books,
- the unusually large duplicate CS:APP PDF,
- generated binaries, nested repository metadata, swap files, and crash dumps.

## How daily lessons should use this library

1. Start from the current tracker objective.
2. Choose at most one primary reading and one supplemental reference.
3. Cite the exact chapter/page range in the daily lesson.
4. Prefer a short targeted reading followed by an experiment.
5. Treat imported example code as something to inspect, build, break, and
   explain—not as an answer to copy blindly.
