# ToyOS Weeks 6–7

ToyOS is one freestanding x86-64 kernel with two stable build profiles.
`CONFIG_MILESTONE=6` keeps the bootstrap/exception milestone runnable while
`CONFIG_MILESTONE=7` compiles the memory and scheduling milestone from the
same sources. Everything builds offline with the host GNU toolchain, GRUB2,
xorriso, QEMU, and GDB.

## Quick start

From `projects/toyos/`:

```bash
make week6 week7
make test-week6
make test-week7
make run-week6
make run-week7
```

The run targets terminate through QEMU's `isa-debug-exit` device. A successful
kernel exit value of `0x10` appears to the host as status 33. Tests enforce
both that status and a serial log marker, and apply a 12-second timeout.

Expected-failure exercises build separate images and check decoded logs:

```bash
make expected-fault-week6-divide
make expected-fault-week6-page
make expected-fault-week6-assert
make expected-fault-week7-page
make expected-fault-week7-allocator
make expected-fault-week7-context
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

These are **kernel tasks**, not user processes. They share one address space
and ring 0. There is no process isolation, ring 3, syscall ABI, user ELF
loader, or per-process page table yet.

## Debugging

In one terminal, start a paused VM:

```bash
make debug-week6
# or: make debug-week7
```

In another terminal:

```bash
make gdb-week6
# or: make gdb-week7
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
- `src/kernel.c`: profile orchestration and self-tests
- `scripts/check-qemu.sh`: deterministic serial/status regression harness
- `gdb/*.gdb`: symbol-aware debug entry points

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
