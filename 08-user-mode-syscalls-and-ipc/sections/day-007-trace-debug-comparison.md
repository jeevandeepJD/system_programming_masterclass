# Day 7 — User→Kernel→User Trace, Fault Report, and Comparison

**Target time:** approximately 3 hours

> A trustworthy trace distinguishes bytes in source, instructions in an ELF,
> events observed in QEMU, and conclusions inferred from their agreement.

## 1. Build one integrated timeline

Begin at `_user_start` in the exact `build/week8/user.elf`. For the first
write syscall, record:

```text
user RIP prepares RAX=1, RDI=1, RSI=&hello, RDX=length
int 0x80 selects IDT[128]
CPU checks DPL3, loads TSS RSP0, saves user return state
stub adds vector/error and saves GPRs
interrupt_handler recognizes vector 128
syscall_dispatch bounds length and walks user mappings
bytes move into kernel stack buffer and then COM1
saved frame RAX becomes byte count
stub restores registers and iretq restores CPL3 continuation
```

At each event add columns for CPL, active CR3, live stack owner, saved-frame
owner, buffer owner, interrupt state, and evidence type. This prevents a
common mistake: saying the CPU “calls the C syscall function.” The CPU enters
an assembly gate; software constructs the C boundary.

Repeat the trace for `pipe`, pipe `write`, pipe `read`, and `exit`. Note that
`exit` never returns and QEMU termination substitutes for process teardown.

## 2. Record artifact identity and dynamic contracts

An evidence report should include:

1. tool versions;
2. SHA-256 for kernel ELF, user ELF, and both fault-profile kernel ELFs;
3. ELF class, machine, and entry points;
4. ordered symbols for entry, loading, copying, dispatch, and return;
5. the complete `make test-week8` output;
6. source excerpts for each claimed invariant;
7. a trace with ownership and privilege;
8. interpretations and limits.

The normal profile is accepted only when the harness observes host status 33
and `WEEK8: PASS`. Because `isa-debug-exit` maps a guest byte to an odd host
status, do not call 33 the kernel's exit code; the kernel writes `0x10`.

Likewise, the user-pointer profile writes `0x15` and appears as 43; the
unknown-syscall profile writes `0x16` and appears as 45.

## 3. Diagnose by detection boundary

**Invalid user pointer (`EXPECT_FAULT=6`).** After activating the process
root, the kernel asks `copy_from_user` to read one byte at `0x1234`.
`user_translate` finds no complete present/user path, returns false, and the
profile checks that result. It emits `EXPECTED-FAULT: user-pointer PASS`.
This is software rejection before user entry, not vector 14.

**Unknown syscall (`EXPECT_FAULT=7`).** Before user entry, the kernel creates
a synthetic frame with `RAX=UINT64_MAX`, calls `syscall_dispatch`, and asserts
that the signed return is `-38`. It emits
`EXPECTED-FAULT: syscall PASS`. This tests dispatch policy directly, not the
IDT/TSS path.

**Unexpected user exception.** There is no selected Week 8 profile that turns
a user fault into process termination or a signal. The generic handler prints
the exception and panics. Therefore a debugging report should propose CR2,
error code, saved RIP/CS/RSP/SS, CR3, and exact ELF as next evidence, while
stating that recovery policy is absent.

These boundaries are intentionally different:

```text
user-pointer validity → page-table software walker
syscall-number validity → dispatcher default case
architectural memory fault → exception handler
```

## 4. Use GDB without inventing observations

The canonical `gdb/week8.gdb` loads `build/week8/toyos.elf`, connects to
QEMU's `:1234` stub, and stops at `kernel_main`, `process_init`, and
`syscall_dispatch`. Before launching, write predictions for:

- `init_process.root` and hardware `CR3`;
- aligned TSS `RSP0` and live kernel `RSP`;
- first frame's syscall registers;
- saved ring-3 selectors;
- saved RIP immediately after `int $0x80`;
- the page-table chain for `RSI`;
- returned `RAX`.

Interactive output belongs in a report only if actually observed. The
generated wrapper supplies source, artifact, and automated QEMU evidence; it
does not pretend to be a live debugging transcript.

If the breakpoint cannot decode a frame, first verify artifact/profile
identity and structure offsets. If execution faults before dispatch, inspect
the gate, TSS, stack mapping, and selector frame rather than changing syscall
policy. Debug from the earliest violated invariant.

## 5. Compare ToyOS with xv6-riscv

xv6-riscv has multiple processes, per-process page tables, trapframes,
trampoline code, a scheduler, `fork`, `exec`, `wait`, descriptor tables,
reference-counted files, and pipes with synchronization and sleep/wakeup.
Its syscall instruction is `ecall`; supervisor trap CSRs and RISC-V page
tables replace x86 IDT/TSS/`iretq` mechanisms.

Useful current-tree landmarks commonly include:

- `kernel/proc.c` for process lifecycle;
- `kernel/vm.c` and `kernel/exec.c` for address spaces and loading;
- `kernel/trampoline.S` and `kernel/trap.c` for user/kernel transitions;
- `kernel/syscall.c` and `kernel/sysfile.c` for dispatch;
- `kernel/pipe.c`, `kernel/file.c`, and `kernel/fdtable`-equivalent logic for
  descriptors and pipe lifetime.

Verify names against the revision used. xv6's simplicity is still materially
richer than ToyOS's one static process.

## 6. Compare ToyOS with Linux

Linux separates x86 entry mechanism under `arch/x86/entry/` from generic
syscall, VFS, memory-management, IPC, and signal policy. It supports both
architecture fast paths and carefully validated fallback return paths,
per-CPU entry state, speculation mitigations, preemption, tracing, seccomp,
credentials, namespaces, and compatibility ABIs.

Its process address spaces use VMAs, demand paging, copy-on-write, ASLR,
permission enforcement, teardown, and SMP TLB synchronization. User copying
uses architecture-aware helpers and recoverable fault machinery rather than
ToyOS's identity-mapped physical-pointer loop.

Linux descriptor state distinguishes descriptor-table entries from shared
open-file descriptions and underlying objects. Pipes have lifecycle,
blocking/nonblocking behavior, wakeups, polling, and capacity rules.
Shared-memory families manage named or anonymous objects and mapping
references. Signals maintain pending state, masks, dispositions, user frames,
and validated return.

The comparison should not become a feature checklist detached from the
trace. For each richer mechanism, name the invariant it addresses:
per-CPU stack ownership, concurrent mapping changes, endpoint lifetime,
partial-copy recovery, or asynchronous user-return modification.

## 7. State ToyOS conclusions narrowly

Week 8 demonstrates:

- a process-owned paging root with shared supervisor kernel reachability;
- user mappings whose U/S bit propagates through the walk;
- bounded parsing/loading of the canonical embedded ELF;
- a four-page user stack and distinct kernel entry stack;
- ring-3 entry and interrupt-gate syscall round trips;
- page-wise bounded user copies;
- fixed descriptor policy and a bounded global pipe;
- controlled rejection of one bad pointer and one unknown syscall.

It does not demonstrate hostile ELF hardening, W^X, multiple processes,
scheduling, blocking IPC, shared memory, signals, resource reclamation,
concurrency, SMP, or production fast-syscall entry.

That final limit paragraph is part of the technical result. It tells the next
engineer which assumptions can safely be reused and which must be redesigned.

## Practical challenge

Use `challenges/day-007-user-kernel-report.sh` from the repository root.

**Purpose:** generate one temporary Week 8 report from exact artifacts,
complete regression output, source contracts, integrated trace, comparison,
and limits.

**Predict:** normal/user-pointer/syscall host statuses are 33/43/45 with their
exact markers. Explain why each fault profile stops before ring 3.

**Evidence:** the wrapper hashes all artifacts, runs `test-week8`, inspects
both ELFs, records the trace and detection boundaries, and names comparison
landmarks. Add your own causal explanation; generated text is not mastery.

## Integrated mastery

Explain the first serial write from ELF byte to COM1 byte. For every boundary,
identify architecture, ABI, policy, ownership/lifetime, and evidence. Then
choose one absent feature—second process, shared memory, or signals—and list
the invariants that must be added before it can be safe.
