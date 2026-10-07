# Week 11 — Threads and Concurrency

**Format:** seven hands-on-first lessons using the canonical
[`projects/linux-system-programming`](../projects/linux-system-programming/) lab

**Suggested workload:** approximately 21 focused hours, adjustable to mastery

> Predict an invariant, run a bounded experiment, identify the happens-before
> edges and object lifetimes, and stop the claim where the evidence stops.

The project is an offline C17/POSIX threads lab. This week does not copy or
patch it. Every wrapper resolves the canonical project, builds through its
existing Makefile, labels expected failures and optional-tool skips, and uses:

```text
purpose → prediction → evidence → explanation → limits
```

Run wrappers from this week directory. Output ordering, thread IDs, timings,
and relaxed-memory observations are intentionally not treated as stable.

## Daily roadmap

1. [Threads, processes, stacks, and TLS](sections/day-001-threads-processes-clone-stacks-tls.md)
   — pthread/process relationships, clone intuition, shared address spaces,
   per-thread stacks, TLS, joins, and lifetime.
2. [Data races and the C abstract machine](sections/day-002-data-races-and-the-c-abstract-machine.md)
   — conflicting actions, undefined behavior, failed increments, compiler
   freedom, and why plausible output proves nothing.
3. [Mutexes, ownership, ordering, and futexes](sections/day-003-mutexes-ownership-lock-order-futex.md)
   — protected invariants, ownership transfer, global lock order, and the
   userspace/kernel boundary.
4. [Condition variables and bounded queues](sections/day-004-condition-variables-and-bounded-queues.md)
   — predicates, while loops, spurious wakeups, lost-wakeup avoidance, and a
   producer-consumer proof.
5. [C11 atomics and memory orders](sections/day-005-c11-atomics-and-memory-ordering.md)
   — modification order, release/acquire, seq_cst, compiler versus CPU
   ordering, and cautious x86-64/AArch64/RISC-V mappings.
6. [Liveness, false sharing, and measurement](sections/day-006-liveness-false-sharing-and-scalability.md)
   — deadlock, starvation, cache-line interference, scalability, and what a
   microbenchmark cannot establish.
7. [Integrated debugging report](sections/day-007-debuggers-sanitizers-and-system-comparison.md)
   — GDB, Valgrind, sanitizers, intentional failures, evidence quality, and
   ToyOS/xv6/Linux comparison.

Each day has one executable wrapper and a generated four-question HTML theory
check. [`quizzes.json`](quizzes.json) is their source. The consolidated
[`week-11-material.pdf`](week-11-material.pdf) preserves the overview and all
seven lessons for offline reading.

## Safety and evidence discipline

The intentional data race has undefined behavior and is excluded from the
ordinary deterministic suite. Run it only through Day 2 or Day 7's bounded
wrapper. A ThreadSanitizer build does not imply its runtime can execute on this
host: compile failure, runtime incompatibility, and a completed run that did
not report the race are reported separately. Absence of a report is not proof
of absence.

The deadlock exercise uses timed locking and terminates. It demonstrates a
wait-for cycle without leaving a hung process. The false-sharing and relaxed
litmus runs are measurements, not correctness tests. In particular, an x86-64
observation cannot prove portable C behavior; the C abstract machine defines
portability, while ISA manuals and compiler output explain one implementation.

## Completion checkpoint

Your report should answer:

1. Which state is process-wide, which is per-thread, and who owns each object?
2. Which operations conflict, and what happens-before edge—if any—orders them?
3. Why can `counter++` fail at both the machine and language levels?
4. What invariant does each mutex protect, and what global lock order holds?
5. Why must condition waits re-check a predicate while holding its mutex?
6. Which atomic ordering is sufficient for the stated protocol, and why?
7. Which claims came from program assertions, timing, debugger, dynamic tool,
   compiler output, ISA documentation, or source?
8. What remains unproved because of scheduling, optimization, architecture,
   tool availability, runtime policy, sampling, or workload choice?

Do not auto-fill mastery, confidence, hours, or completion.
