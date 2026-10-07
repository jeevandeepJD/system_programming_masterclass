# ToyOS Weeks 6–9

ToyOS is one freestanding x86-64 kernel with four stable build profiles.
Weeks 6 and 7 preserve bootstrap, exception, memory, and kernel-task behavior.
Week 8 adds a ring-3 process and syscall/FD model. Week 9 adds a ramfs/VFS and
a real QEMU EDU PCI/MMIO path.

## Quick start

From `projects/toyos/`:

```bash
make week6 week7 week8 week9
make test-all
make run-week8
make run-week9
```

The run targets terminate through QEMU's `isa-debug-exit` device. A successful
kernel exit value of `0x10` appears to the host as status 33. Tests enforce
both that status and a serial log marker, and apply a 12-second timeout.
Week 9 alone adds `-device edu`; earlier QEMU profiles are unchanged.

Expected-failure exercises build separate images and check decoded logs:

```bash
make expected-fault-week6-divide
make expected-fault-week6-page
make expected-fault-week6-assert
make expected-fault-week7-page
make expected-fault-week7-allocator
make expected-fault-week7-context
make expected-fault-week8-user-pointer
make expected-fault-week8-syscall
make expected-fault-week9-filesystem
make expected-fault-week9-device
```

## What each profile demonstrates

Week 6 has a Multiboot2 header consumed by GRUB2, a 32-bit entry, initial
identity-mapped 2 MiB pages, transition to long mode, a 64-bit GDT, a TSS with
one IST stack, COM1 logging, panic/assert, a 256-entry IDT, and assembly
exception stubs. The stubs normalize vectors with and without hardware error
codes into one `interrupt_frame`. Page faults print CR2 and decode
present/write/user/reserved/instruction-fetch flags.

Week 7 additionally parses the Multiboot2 memory-map tag, bounds a bitmap
page-frame allocator to the first 128 MiB, creates 4 KiB mappings, maps a
small bump heap, and schedules two ring-0 tasks with dedicated kernel stacks.
One task exercises cooperative switching; the other requires PIT IRQ0
preemption through the remapped legacy PIC. Deterministic negative profiles
verify that `frame_free` rejects a double-free and that the scheduler rejects
a task context whose stack range/alignment or instruction pointer is invalid
before executing the low-level switch.

Week 8 creates a process-owned PML4, maps ELF load segments and a stack with
U/S permissions, loads an embedded ELF64 image, updates TSS `RSP0`, and enters
CPL3 with `iretq`. User pointers are translated and copied page-by-page with a
4096-byte per-call bound. User mode demonstrates `write`, `getpid`, `yield`,
`pipe`, `read`, and `exit`; the pipe is a bounded 64-byte kernel buffer.

The syscall ABI deliberately uses a DPL3 `int 0x80` interrupt gate, not
`SYSCALL/SYSRET`. The gate provides the validated hardware TSS stack transition
already used by this compact kernel. `SYSCALL` does not switch stacks itself;
a safe implementation needs per-CPU entry state (normally `swapgs`), careful
canonical-address and flags validation, and an `iretq` fallback. Claiming a
direct `SYSRET` path without those invariants would be unsafe, so the interrupt
gate is the explicit last-resort teaching ABI.

Week 9 provides a tiny VFS over ramfs: inode lookup by absolute path, open-file
offsets, and descriptor-based `open`, `read`, and `write` from ring 3. It also
enumerates PCI bus 0 through configuration mechanism #1, finds QEMU EDU
vendor/device `1234:11e8`, maps BAR0, verifies the device ID, and runs the
MMIO liveness inversion test.

## Debugging

In one terminal, start a paused VM:

```bash
make debug-week6
# or: make debug-week7 / debug-week8 / debug-week9
```

In another terminal:

```bash
make gdb-week6
# or: make gdb-week7 / gdb-week8 / gdb-week9
```

For the injected page fault, use `make debug-fault-week6-page` in the first
terminal and `make gdb-fault` in the second.

The scripts in `gdb/` load symbols, connect to QEMU's `:1234` stub, and set
milestone-specific breakpoints. `gdb/fault.gdb` is a starting point for a
page-fault image. Build artifacts include `toyos.elf` and `toyos.map`, so
`info registers`, `x/i $rip`, `p/x *frame`, and `list *frame->rip` can connect
the architectural frame to source.

## Source map

- `src/boot.S`: Multiboot2 header, 32-bit bootstrap, page tables, long mode
- `src/isr.S`: exception/IRQ stubs and normalized saved-register frame
- `src/arch.c`: serial, GDT/TSS/IST, IDT, PIC/PIT, panic, fault decoding
- `src/memory.c`: memory-map parser, frame allocator, mapper, kernel heap
- `src/tasks.c`, `src/switch.S`: kernel-task contexts and scheduling
- `src/process.c`, `src/user_entry.S`: private address space, ELF loading,
  bounded user copies, syscall/FD state, and CPL3 transition
- `src/user_program.S`, `user.ld`, `src/user_image.S`: embedded ELF64 program
- `src/vfs.c`: ramfs inodes, paths, open files, and descriptor I/O
- `src/edu.c`: PCI configuration discovery and checked EDU BAR/MMIO access
- `src/kernel.c`: profile orchestration and self-tests
- `scripts/check-qemu.sh`: deterministic serial/status regression harness
- `gdb/*.gdb`: symbol-aware debug entry points

## Exact regression commands

```bash
make clean && make test-all
make clean && make CC=clang test-all
```

`test-all` runs normal profiles and every fault image: Week 6
divide/page/assert, Week 7 page/allocator/context, Week 8
invalid-user-pointer/unknown-syscall, and Week 9
missing-path/invalid-device-register. Every VM requires its profile-specific
serial marker and `isa-debug-exit` status.

## Deliberate limits

The bootstrap identity maps only the first 1 GiB and assumes GRUB places the
kernel and Multiboot information there. The allocator intentionally ignores
RAM above 128 MiB. It is single-core and has no locking. The heap cannot free.
The allocator bitmap catches double-free but does not distinguish an allocated
frame from a reserved frame, so it is not a complete ownership model.
The mapper does not split existing 2 MiB huge pages. IRQ handling uses the
educational 8259 PIC and PIT rather than APIC/x2APIC. Scheduler switching from
interrupt context is intentionally compact and is not a production kernel
design.

There is one user process and no fork/exec, reclaim, demand paging, user page
fault recovery, SMP, or security hardening. ELF segments are writable because
this teaching mapper does not yet enforce W^X/NX. File and pipe capacity,
descriptor count, path length, and each user-copy operation are fixed and
bounded. Signals, asynchronous delivery, blocking behavior, permissions,
directories, close/seek, and full POSIX semantics are simplified or absent.

The EDU profile intentionally validates deterministic PCI discovery, BAR
mapping, checked register access, and MMIO behavior only. IRQ delivery was not
added because legacy-PIC routing of PCI INTx is machine-dependent in this
minimal platform. DMA was not enabled because a correct test also requires
pinning/ownership and device-visible physical-buffer bookkeeping that this
allocator does not provide. No IRQ or DMA success is claimed.
