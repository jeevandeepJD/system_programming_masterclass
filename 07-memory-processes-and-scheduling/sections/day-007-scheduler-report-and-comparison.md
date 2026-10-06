# Day 7 — Scheduler Trace, Fault Diagnosis, and Systems Comparison

**Target time:** approximately 3 hours

> A scheduler report should make every ownership transfer reproducible and
> every uncertainty visible. “It printed PASS” is the beginning, not the end.

## 1. Trace state and CPU ownership

Build a timeline with one row per meaningful event:

```text
event | task 0 state | task 1 state | current | active RSP owner | reason
```

Start after `tasks_init`: both tasks are runnable and `current` is null.
`tasks_start` chooses task 0. Timer IRQ0 interrupts its busy loop, stores the
interrupt continuation on task 0's stack, and switches to task 1. Task 1
voluntarily yields; later each task returns, becomes blocked, and the final
task exits QEMU after both test conditions hold.

Do not imply a transition not represented in code. ToyOS has no real waiting
queue: `TASK_BLOCKED` means finished.

## 2. Diagnose by violated invariant

The canonical project exposes three deterministic Week 7 profiles:

**Allocator double-free (`EXPECT_FAULT=4`):** `memory_init` allocates and
frees `probe`, then injects a second `frame_free(probe)`.
`frame_is_used` observes that its bitmap bit is already clear, so the handler
emits `EXPECTED-FAULT: allocator PASS` and exits through QEMU code `0x13`.
The host harness requires status 39. Inspect the exact
`build/week7-allocator/toyos.elf`; do not describe this as allocator
exhaustion, because that is not what was injected.

**Unmapped page write (`EXPECT_FAULT=2`):** the store to `0x70000000`
produces vector 14 with CR2 equal to that address and
`not-present write supervisor`. The handler exits through code `0x11`, and
the host expects status 35. Correlate saved RIP with
`build/week7-page/toyos.elf`.

**Corrupted context (`EXPECT_FAULT=5`):** `tasks_init` adds one to task 0's
saved `rsp`. Before the first switch, `validate_context` rejects its lost
8-mod-16 alignment, emits `EXPECTED-FAULT: context PASS`, and exits through
code `0x14`; the host expects status 41. Inspect
`build/week7-context/toyos.elf`. Because rejection precedes
`context_switch`, no invalid stack is loaded.

The three controls exercise different detection boundaries:

```text
allocator metadata invariant → frame_free
address translation failure  → x86 #PF entry and handler
saved continuation invariant → scheduler validation
```

The wrappers invoke build profiles; they do not patch or restore canonical
source.

## 3. Write the scheduler report

Include:

1. artifact identity and tool versions;
2. predictions for normal, allocator, page, and context profiles, including
   markers and host statuses 33, 39, 35, and 41;
3. exact source excerpts for allocation, mapping, task creation, switching,
   IRQ dispatch, and state transitions;
4. ELF hash and ordered symbols;
5. complete Week 7 test output;
6. the state/CPU/stack ownership trace;
7. interpretation of all three injected failures and next observations;
8. comparison and explicit limits.

Separate “source states,” “ELF contains,” “QEMU emitted,” and “we infer.”

## 4. Compare ToyOS with xv6-riscv

Useful current-tree landmarks commonly include:

- `kernel/kalloc.c`: physical-page allocation;
- `kernel/vm.c`: page-table construction and walking;
- `kernel/proc.c`: process state, allocation, and scheduling;
- `kernel/swtch.S`: saved kernel context;
- `kernel/trap.c`: timer/trap policy.

xv6 has processes, per-process page tables, user mode, syscalls, sleep/wakeup,
and a richer lifecycle. Its RISC-V `swtch` and trap CSRs are conceptual
counterparts, not x86 frame equivalents. Record the exact xv6 revision.

## 5. Compare ToyOS with Linux

Linux separates architecture entry and MMU code under `arch/x86/` from
generic memory management in `mm/` and scheduler policy in `kernel/sched/`.
It has per-CPU run queues, synchronization, priorities/classes, process and
thread abstractions, user/kernel transitions, demand paging, and extensive
fault recovery.

ToyOS's fixed two-element array can teach runnable/running ownership, but it
does not model Linux fairness or SMP. Use current source navigation before
naming functions; current paths are landmarks, not timeless contracts.

## 6. Preview processes and threads for Week 8

A process adds an isolation and resource-ownership boundary: user virtual
address space, credentials/capabilities, files, signals, and lifecycle
policy. A thread is an independently schedulable execution context that may
share a process's address space and resources.

Week 7 kernel tasks already demonstrate stacks, saved execution contexts, and
scheduling. Week 8 must add ring-3 entry, syscall and return ABI, user ELF
loading, per-process page tables, validation of user pointers, and safe fault
policy. Reusing the word “task” does not make those properties appear.

## 7. Practical challenge

Use `challenges/day-007-scheduler-report.sh` from the repository root.

**Purpose:** generate one temporary evidence report without editing canonical
source or claiming unperformed live-debug observations.

**Predict:** write all four host statuses and exact markers, the page-fault
tuple, the allocator bit transition, and the context alignment failure.

**Evidence:** the wrapper records source contracts, exact ELF identity,
ordered symbols from every fault ELF, full Week 7 regression output covering
all three injected failures, a static state trace, interpretations,
comparisons, and limits. Add your causal explanation; generated text is not
itself mastery.

## Integrated mastery

Explain the complete chain from Multiboot available range to the final QEMU
exit. At each boundary identify physical ownership, virtual mapping, C object
lifetime, stack owner, register owner, interrupt state, and locking
assumption. Then name the first Week 8 invariant that invalid user input will
test.
