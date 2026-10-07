# Week 8 — User Mode, Syscalls, and IPC

**Format:** seven daily sections collected into one mastery package

**Suggested workload:** approximately 21 focused hours, adjustable to mastery

> Keep three layers separate: architecture moves privilege and checks pages;
> an ABI assigns meaning to registers and frames; kernel policy decides what a
> valid request may do.

This package studies the existing
[`projects/toyos`](../projects/toyos/) Week 8 milestone. Every practical
challenge inspects or runs that canonical project; no challenge forks or
patches the kernel. Each wrapper follows:

```text
purpose → prediction → evidence → explanation → limits
```

ToyOS Week 8 has exactly one ring-3 process. It owns a PML4, user mappings, a
four-page user stack, a 16 KiB kernel stack, PID 1, and a small descriptor
convention. It does not implement fork/exec, teardown, demand paging,
blocking, shared-memory mappings, or signals. Its 64-byte global pipe is the
bounded IPC mechanism available in the current project.

## Daily roadmap

### Day 1 — Ring 3 entry and the architecture lens

Audit user descriptors, TSS `RSP0`, the DPL3 gate, and the crafted `iretq`
frame. Distinguish CPU privilege rules from ToyOS choices.

- Lesson: [day-001-ring3-gdt-tss-iret.md](sections/day-001-ring3-gdt-tss-iret.md)
- Challenge: `challenges/day-001-ring3-entry.sh`

### Day 2 — User address spaces and U/S permissions

Follow the process-owned PML4, copied kernel mappings, user propagation
through all page-table levels, CR3 activation, and the isolation limits.

- Lesson: [day-002-user-address-spaces.md](sections/day-002-user-address-spaces.md)
- Challenge: `challenges/day-002-address-space.sh`

### Day 3 — Embedded ELF loading

Validate the ELF64 header and program-header table, load `PT_LOAD` segments,
map the user stack, and identify validation and rollback limits.

- Lesson: [day-003-embedded-elf-loader.md](sections/day-003-embedded-elf-loader.md)
- Challenge: `challenges/day-003-elf-loader.sh`

### Day 4 — `int 0x80` syscall ABI

Trace the DPL3 interrupt gate, saved frame, dispatch registers, result in
`RAX`, and `iretq` return. Explain explicitly why this is not a
`SYSCALL`/`SYSRET` implementation.

- Lesson: [day-004-int80-syscall-abi.md](sections/day-004-int80-syscall-abi.md)
- Challenge: `challenges/day-004-syscall-path.sh`

### Day 5 — Safe user copying

Walk and validate user translations page by page, enforce the 4096-byte copy
bound, and inspect the deterministic invalid-pointer profile.

- Lesson: [day-005-safe-user-copying.md](sections/day-005-safe-user-copying.md)
- Challenge: `challenges/day-005-user-pointer.sh`

### Day 6 — Process, descriptors, and bounded IPC

Account for process and descriptor state, the 64-byte pipe, and the distinction
between implemented pipe IPC and conceptual shared-memory/signal boundaries.

- Lesson: [day-006-process-fd-ipc.md](sections/day-006-process-fd-ipc.md)
- Challenge: `challenges/day-006-pipe-ipc.sh`

### Day 7 — Integrated trace, diagnosis, and comparison

Build an evidence report from user instruction through syscall and return,
interpret both Week 8 injected profiles, and compare ToyOS with xv6 and Linux.

- Lesson: [day-007-trace-debug-comparison.md](sections/day-007-trace-debug-comparison.md)
- Challenge: `challenges/day-007-user-kernel-report.sh`

## Integrated checkpoint

Narrate this chain without skipping an owner:

```text
embedded ELF bytes → validated PT_LOAD → physical frames → user PTEs
→ CR3 activation → TSS RSP0 → iretq to CPL3 → int 0x80
→ hardware stack transition → saved interrupt_frame → syscall policy
→ checked copy / pipe state → RAX result → iretq to user
```

At every arrow name:

1. the architecture rule, such as CPL/DPL/RPL, page U/S, or interrupt entry;
2. the ABI rule, such as syscall numbers and register arguments;
3. the ToyOS policy, such as the 256-byte I/O and 64-byte pipe bounds;
4. the mutable owner and its lifetime;
5. the exact source, ELF, or QEMU evidence supporting the claim.

## Debugging discipline

Kernel debugging starts with artifact identity. Use the exact
`build/week8/toyos.elf` or fault-profile ELF that produced the observation.
The canonical `gdb/week8.gdb` stops at `kernel_main`, `process_init`, and
`syscall_dispatch`. Useful follow-up observations include the saved frame,
`CR3`, `CR2`, current `RSP`, `CS`, `SS`, the IDT gate, and page-table entries.
Do not claim a GDB observation merely because a command is suggested.

The normal profile exits through QEMU with host status 33 and marker
`WEEK8: PASS`. The bad-user-pointer profile checks marker
`EXPECTED-FAULT: user-pointer PASS` and host status 43. The unknown-syscall
profile checks marker `EXPECTED-FAULT: syscall PASS` and host status 45.
These are controlled rejection tests, not crashes.

## Reference discipline

Current xv6-riscv comparison landmarks commonly include `kernel/proc.c`,
`kernel/vm.c`, `kernel/exec.c`, `kernel/trampoline.S`, `kernel/trap.c`,
`kernel/syscall.c`, `kernel/pipe.c`, and `kernel/signal`-equivalent absence;
verify the revision you cite. Linux landmarks include `arch/x86/entry/`,
`arch/x86/mm/`, `fs/`, `ipc/`, `kernel/signal.c`, and `mm/`; inspect a current
tree before making line-level claims.

Do not auto-fill mastery, confidence, hours, or completion.
