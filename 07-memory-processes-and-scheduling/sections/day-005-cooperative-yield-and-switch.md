# Day 5 — Cooperative Yield, Context Switch, and Register Ownership

**Target time:** approximately 3 hours

> A context switch does not move a running task. It changes which saved stack
> and continuation the CPU treats as current.

## 1. Build the CPU mental image

At any instant one CPU has one active `RSP` and instruction pointer. Other
tasks are data structures containing enough state to become active later.
ToyOS's `yielding_worker` voluntarily calls the scheduler four times; that C
call is its yield operation even though there is no public function named
`yield`.

Before a switch, identify:

```text
CPU owner: current task
live state: registers plus its active stack
saved next task: context fields plus inactive stack
policy state: current, task states, fixed tasks[] order
```

## 2. Select before switching

`schedule` in `projects/toyos/src/tasks.c` picks task 0 initially, otherwise
the next array slot. It avoids a blocked task and returns if no other runnable
task exists. It then marks the old task runnable, selects an old-context
destination, marks the new task running, updates `current`, and calls
`context_switch`.

Updating `current` before assembly transfers control ensures the new task's
trampoline and later code see the correct owner. It also means an interrupt
at an unsafe point could observe partially transitioned scheduler state.
ToyOS has no explicit interrupt-disable critical section around this path.

## 3. Let the ABI reduce saved state

`projects/toyos/src/switch.S` saves `R15`, `R14`, `R13`, `R12`, `RBX`, `RBP`,
`RSP`, and a local resume `RIP`. These general registers are callee-saved by
the System V AMD64 ABI. Caller-saved registers need not survive a normal C
call to `context_switch`; the compiler already treats them as volatile.

The architecture does not define this set—the ABI does. SIMD/FPU state is
absent, and kernel compilation uses `-mgeneral-regs-only`. Future code that
uses extended state must add a policy for ownership and save/restore.

## 4. Follow the unusual return

Assembly stores the old `RSP` and the address of label `1`, loads the new
callee-saved registers and `RSP`, then jumps to the new `RIP`.

For a new task, that target is `task_trampoline`. For a resumed task, it is
label `1`, whose `ret` consumes the return address already present on that
task's saved stack. Thus one invocation of `context_switch` may return much
later, on the same task's stack, after several switches.

This is why stack ownership is central. A saved context whose stack storage
was freed or overwritten is not a stale value—it is a future control-flow
failure.

## 5. Separate mechanism from policy

Assembly implements mechanism: save old continuation, restore new
continuation. C implements policy: which task should run and which states
change. Keeping that boundary narrow makes it possible to audit offsets and
ABI obligations.

The simple round-robin array is deterministic but not a general run queue.
There is no priority, starvation protection, CPU affinity, or load balancing.

## 6. Diagnose context corruption

Typical signatures include:

- bad saved `RSP`: fault during restore or `ret`;
- bad saved `RIP`: jump into unmapped or non-code memory;
- offset mismatch: registers appear to rotate into one another;
- wrong `current`: one task mutates another task's state;
- stack overflow: scheduler data corrupts before the crash is observed.

In GDB, inspect both context pointers at entry, disassemble `context_switch`,
and verify the destination addresses against `nm` from the exact ELF. C
debugging still matters: enum state and pointer ownership bugs can perfectly
feed assembly the wrong but well-aligned address.

## 7. Practical challenge

Use `challenges/day-005-cooperative-switch.sh` from the repository root.

**Purpose:** prove agreement among ABI expectations, C context order,
assembly offsets, and the selected cooperative test.

**Predict:** identify every preserved register and narrate first-entry versus
resume behavior.

**Evidence:** the wrapper inspects flags/layout/disassembly and checks the
cooperative marker from the canonical profile. It cannot prove correctness
under arbitrary interrupt timing.

## Mastery prompts

Explain why `RAX` is not in `struct context`, trace the two different meanings
of saved `RIP`, and identify the smallest region that would need interrupt
exclusion before calling this scheduler robust.
