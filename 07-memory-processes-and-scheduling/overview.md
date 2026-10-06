# Week 7 — Memory, Kernel Tasks, and Scheduling

**Format:** seven daily sections collected into one mastery package

**Suggested workload:** approximately 21 focused hours, adjustable to mastery

> Keep one CPU mental image: memory decides what an address means; scheduling
> decides which saved instruction stream owns the CPU next.

This package studies the existing
[`projects/toyos`](../projects/toyos/) Week 7 milestone. The practical work
invokes or inspects that canonical project; it does not fork the kernel. Every
challenge follows:

```text
purpose → prediction → evidence → explanation → limits
```

ToyOS schedules two **ring-0 kernel tasks**, not user processes. They share
one address space and privilege level. Ring 3, syscalls, user ELF loading,
per-process page tables, and process isolation are Week 8 topics.

## Daily roadmap

### Day 1 — Boot memory map and physical frames

Parse Multiboot2's variable-size memory-map tag and audit the deliberately
bounded first-fit bitmap allocator.

- Lesson: [day-001-memory-map-and-frame-allocator.md](../07-memory-processes-and-scheduling/sections/day-001-memory-map-and-frame-allocator.md)
- Challenge: `challenges/day-001-memory-frames.sh`
- Evidence: normal allocator marker plus deterministic double-free injection,
  checked marker/status, exact fault-profile symbols, and source invariants.

### Day 2 — Page tables, TLBs, and page faults

Walk x86-64's four-level translation, ToyOS's mapper, `invlpg`, and the
existing page-fault diagnosis; compare AArch64 and RISC-V mechanisms.

- Lesson: [day-002-page-tables-mapper-and-faults.md](../07-memory-processes-and-scheduling/sections/day-002-page-tables-mapper-and-faults.md)
- Challenge: `challenges/day-002-mapper-fault.sh`
- Evidence: mapper source/disassembly and checked CR2/error-code output.

### Day 3 — Kernel heap and ownership

Explain the bump heap's alignment and exhaustion checks, then make lifetime
and ownership claims explicit.

- Lesson: [day-003-kernel-heap-and-ownership.md](../07-memory-processes-and-scheduling/sections/day-003-kernel-heap-and-ownership.md)
- Challenge: `challenges/day-003-kernel-heap.sh`
- Evidence: mapped heap construction, allocator checks, and task allocations.

### Day 4 — Kernel tasks and saved contexts

Trace task records, dedicated stacks, trampoline entry, and saved contexts.

- Lesson: [day-004-kernel-tasks-stacks-contexts.md](../07-memory-processes-and-scheduling/sections/day-004-kernel-tasks-stacks-contexts.md)
- Challenge: `challenges/day-004-task-context.sh`
- Evidence: C/assembly layout agreement, task-start markers, and deterministic
  corrupted-stack-alignment rejection before the first context switch.

### Day 5 — Cooperative yield and ABI ownership

Follow a voluntary switch and account for callee-saved registers, stack
ownership, return addresses, and scheduler state.

- Lesson: [day-005-cooperative-yield-and-switch.md](../07-memory-processes-and-scheduling/sections/day-005-cooperative-yield-and-switch.md)
- Challenge: `challenges/day-005-cooperative-switch.sh`
- Evidence: switch disassembly, ABI register set, and cooperative marker.

### Day 6 — Timer preemption and interrupt safety

Follow PIT IRQ0 through the remapped PIC into scheduling, and contrast the
legacy lab path with APIC/x2APIC systems.

- Lesson: [day-006-pit-pic-preemption.md](../07-memory-processes-and-scheduling/sections/day-006-pit-pic-preemption.md)
- Challenge: `challenges/day-006-preemption.sh`
- Evidence: IRQ source path, tick growth, EOI ordering, and preemption marker.

### Day 7 — Trace, diagnose, and compare

Build a scheduler report, diagnose injected allocator/page/context failures,
and compare ToyOS with xv6 and Linux before Week 8.

- Lesson: [day-007-scheduler-report-and-comparison.md](../07-memory-processes-and-scheduling/sections/day-007-scheduler-report-and-comparison.md)
- Challenge: `challenges/day-007-scheduler-report.sh`
- Evidence: one temporary report with exact artifacts, tests, source contracts,
  all three injected fault profiles, interpretations, comparisons, and limits.

## Integrated checkpoint

Narrate both chains without skipping ownership:

```text
Multiboot map → free physical frame → page-table ownership → virtual mapping
→ heap object → task and stack → runnable → running → IRQ/yield
→ saved context → run queue choice → restored context
```

For every arrow, identify the architecture rule, ABI rule, ToyOS policy,
mutable owner, and evidence. State the single-core/no-locking assumption.

## Reference discipline

Use current architecture manuals for normative details. Linux source
landmarks include `arch/x86/entry/`, `arch/x86/mm/`, `mm/`, and
`kernel/sched/`; inspect and record a revision before making line-level
claims. Current xv6-riscv concepts commonly live around `kernel/kalloc.c`,
`kernel/vm.c`, `kernel/proc.c`, `kernel/swtch.S`, and `kernel/trap.c`, but
verify the tree you cite. Local comparison material is available at
[`references/ARCHITECTURE_COMPARISON.md`](../references/ARCHITECTURE_COMPARISON.md).

Do not auto-fill mastery, confidence, hours, or completion.
