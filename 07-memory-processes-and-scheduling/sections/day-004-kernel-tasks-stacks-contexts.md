# Day 4 — Kernel Tasks, Dedicated Stacks, and Saved Contexts

**Target time:** approximately 3 hours

> A task is not a function call. It is a resumable ownership bundle whose
> saved stack and registers let one instruction stream temporarily stop.

## 1. Name the abstraction accurately

`projects/toyos/src/tasks.c` defines two ring-0 kernel tasks. They share the
same page tables, global variables, kernel code, devices, and privilege.
They are not processes and do not provide isolation. Calling them processes
would hide exactly what Week 8 must add.

Each `kernel_task` contains a saved `context`, state, identifier, entry
function, and stack pointer. The static `tasks[2]` array is a tiny fixed run
queue; `current` identifies the task whose stack and execution stream own the
CPU.

## 2. Give every task a private kernel stack

`create_task` obtains an 8192-byte stack from the bump heap and computes an
aligned top because x86-64 stacks grow downward. It subtracts eight bytes and
places a zero sentinel so entry through a jump has a stack shape compatible
with the function ABI's expectation that a call supplied a return address.

The initial saved `RSP` points there and initial `RIP` names
`task_trampoline`. The context's other fields are zero from the heap's
freshly zeroed physical pages, an assumption worth documenting rather than
silently depending on forever.

No guard page catches stack overflow. An overflow can corrupt the adjacent
heap object or scheduler metadata, producing symptoms far from the cause.

Before selecting a runnable task, `validate_context` now checks that its
record and stack exist, its state is runnable, saved `RSP` lies inside the
dedicated 8192-byte stack with the expected low bits, and saved `RIP` lies
inside the linked kernel image. These are boundary checks, not proof that
every byte of the saved continuation is trustworthy.

## 3. Distinguish two saved-state structures

`struct context` in `projects/toyos/include/toyos.h` stores the callee-saved
registers, stack pointer, and resume address needed at a cooperative C call
boundary. `struct interrupt_frame` stores a broader interrupted machine
state assembled by hardware and `projects/toyos/src/isr.S`.

They are not interchangeable:

```text
context: scheduler continuation at an ABI boundary
interrupt_frame: asynchronous exception/IRQ return state
```

A production preemptive scheduler usually coordinates these layers
explicitly. ToyOS calls its compact context switch while already executing on
the interrupted task's stack.

## 4. Enter through a trampoline

The switch jumps to `task_trampoline`, not directly to the task entry.
The trampoline enables interrupts, verifies `current`, invokes the selected
entry, and funnels a normal return into `task_finish`. This centralizes task
exit policy.

`task_finish` marks the current task blocked, reports completion, and either
schedules another task or exits after both finish and checks pass. “Blocked”
here means permanently finished; there is no wait channel or wakeup path.

## 5. Track the state machine

The implemented transitions are:

```text
created → RUNNABLE → RUNNING
RUNNING → RUNNABLE during a switch
RUNNING → BLOCKED on completion
```

There is no separate ready-list node, sleep queue, priority, fairness
accounting, or idle task. If the other task is blocked, `schedule` may return
without switching. State and queue membership are updated without locks.
That relies on one CPU and carefully constrained interrupt behavior.

## 6. Debug task-start failures

If the first task never prints, inspect its saved `rip`, `rsp`, stack bounds,
and alignment before investigating task logic. If a completed task resumes,
check state selection and context ownership. If a return faults, inspect the
sentinel and trampoline contract.

Use `build/week7/toyos.elf` with `gdb/week7.gdb`. A breakpoint at
`context_switch` can inspect old context in `RDI` and new context in `RSI`
under the System V AMD64 ABI.

The deterministic `expected-fault-week7-context` profile corrupts task 0's
saved stack alignment by adding one to `context.rsp` after task creation.
The first `schedule` calls `validate_context` before mutating `current` or
jumping into assembly. The validator emits `EXPECTED-FAULT: context PASS`
and QEMU code `0x14`; the host harness expects status 41. This establishes
early rejection of that selected corruption, not safe handling of arbitrary
stack or instruction-pointer damage.

## 7. Practical challenge

Use `challenges/day-004-task-context.sh` from the repository root.

**Purpose:** connect C layouts, initial stack construction, assembly offsets,
and runtime task identity.

**Predict:** list context offsets and the first task's initial `RIP`; explain
why its valid `RSP` ends in hexadecimal 8, why adding one violates the
contract, and why rejection occurs before `context_switch`.

**Evidence:** the wrapper compiles an offset probe, compares switch assembly,
checks both task-start markers, runs `expected-fault-week7-context`, and
inspects its exact ELF. It does not establish stack-overflow safety,
arbitrary-context recovery, or user-process isolation.

## Mastery prompts

Draw both task stacks while task 1 runs, mark where task 0's resumable state
lives, then trace the injected misalignment from `tasks_init` to
`context_is_valid` and the checked host status.
