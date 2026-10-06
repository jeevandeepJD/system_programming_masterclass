# Day 6 — PIT/PIC Interrupt-Driven Preemption

**Target time:** approximately 3 hours

> Preemption means the current instruction stream did not volunteer. The
> interrupt path must preserve its claim on every register and stack byte
> before policy lends the CPU elsewhere.

## 1. Follow IRQ0 end to end

For milestone 7, `pic_and_pit_init` in `projects/toyos/src/arch.c` remaps the
8259 PIC so hardware IRQ0 arrives at IDT vector 32, unmasks only the timer,
and programs PIT channel 0. `arch_init` installs `irq0_stub` at vector 32.

The path is:

```text
PIT tick → PIC IRQ0 → IDT vector 32 → irq0_stub → isr_common
→ interrupt_frame on current stack → interrupt_handler
→ ticks++ → PIC EOI → scheduler_tick → schedule
```

The divisor 11932 against the traditional 1,193,182 Hz PIT input is roughly
100 Hz. Emulation and host scheduling mean this is not a precision timing
measurement.

## 2. Account for interrupt context

`isr_common` in `projects/toyos/src/isr.S` saves all general-purpose
registers represented by `interrupt_frame`, calls C on the interrupted task's
current stack, then restores and executes `iretq`.

On the timer path, C may call `context_switch`, which changes stacks. The old
task's stack retains its interrupt frame and nested C frames. When that task
is scheduled again, `context_switch` returns through those frames, the
handler returns, assembly restores the interrupt frame, and `iretq` resumes
the interrupted instruction stream.

That compact design works only if every saved stack remains owned and intact.

## 3. Understand preemption evidence

`preempted_worker` never voluntarily schedules. It spins until the shared
tick count advances by three. Task 1 can run only because timer interrupt
handling invokes the scheduler. Therefore both the task output and
`SELFTEST: PIT/PIC preemption PASS` are meaningful evidence for the selected
path.

`volatile` on `task_done` prevents certain compiler caching, but it is not a
lock or an inter-CPU memory-ordering protocol. `ticks` is changed in interrupt
context and read in task context; this single-core build relies on aligned
64-bit operations and compiler-visible accesses.

## 4. Audit interrupt safety

Interrupt context cannot safely call arbitrary code. A production design
must classify operations that may block, allocate, acquire contended locks,
or recurse into interrupt-dependent services. ToyOS serial output polls, the
heap has no lock, and the scheduler has no explicit critical section.

The handler sends PIC end-of-interrupt before scheduling. That releases the
controller while CPU interrupts remain disabled from interrupt-gate entry
until the task trampoline or eventual `iretq` enables/restores them. Nested
timing and state updates deserve explicit design before adding devices or
multiple CPUs.

## 5. Contrast modern interrupt controllers

The 8259 PIC and PIT are useful legacy teaching devices. Modern x86 systems
normally use a local APIC timer, I/O APIC, MSI/MSI-X, and often x2APIC.
Interrupt routing, per-CPU timers, acknowledgment, priorities, and SMP
coordination differ.

Do not translate ToyOS port writes directly into a Linux explanation. Current
Linux landmarks include `arch/x86/kernel/apic/`, `arch/x86/entry/`, and
`kernel/sched/`, but names and paths require revision-specific inspection.

## 6. Locking assumptions are part of correctness

ToyOS assumes one CPU. No other core can concurrently mutate `current`,
states, the frame bitmap, or heap cursor. Interrupts are the only asynchronous
writer, and task initialization completes before they are enabled in the
first trampoline.

Adding a second CPU would require per-CPU current-task state, run-queue locks
or ownership partitioning, TLB shootdowns, interrupt routing, and memory
ordering. “Add a spinlock” is not a sufficient architecture.

## 7. Practical challenge

Use `challenges/day-006-preemption.sh` from the repository root.

**Purpose:** connect PIT/PIC setup, IRQ frame construction, EOI, tick update,
and an involuntary scheduling observation.

**Predict:** state the first vector, approximate frequency, register owner,
and why task 1 can run while task 0 never yields.

**Evidence:** the wrapper displays the exact source chain and checks task,
tick, and preemption markers in the canonical run. It does not validate APIC,
SMP, latency, fairness, or physical timer accuracy.

## Mastery prompts

Draw the interrupted task's stack at `scheduler_tick`, explain how it remains
resumable across another task's execution, and list three operations that
would need an interrupt-context safety contract before use in this handler.
