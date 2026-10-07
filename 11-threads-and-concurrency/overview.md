# Week 11 — Threads and Concurrency

**Format:** seven daily sections collected into one weekly module

**Suggested workload:** approximately 21 focused hours, adjustable to mastery

> A thread is a private register set and a private stack in a shared address
> space. Everything else this week studies — races, mutexes, condition
> waits, atomics, deadlocks, cache lines, and debuggers — is a consequence
> of that split.

The lab is the canonical
[Linux system-programming project](../projects/linux-system-programming/).
This week does not copy or patch it. Each day's wrapper resolves that
project, builds through its Makefile, and prints what the run is allowed to
mean. Kernel excerpts in Days 4–6 were read from tree `c9b2385d6793`, whose
`Makefile` says Linux 5.10.134. The host that runs the lab may be a
different kernel; `uname` in the reports is that host.

## Why these seven days belong together

Shared memory is one virtual address space, translated by one set of page
tables, backed by cache lines the coherence protocol moves as wholes. Each
thread still has its own registers, its own stack, and its own place in the
scheduler. The C abstract machine then decides which of those concurrent
accesses have a meaning at all.

```text
pthread / clone
  shared mappings, private registers, private stacks, TLS
        |
        v
C abstract machine
  conflicting non-atomic accesses with no happens-before are undefined
        |
        v
mutex
  a userspace atomic on the fast path, a futex when the task must sleep
        |
        v
condition variable
  check the predicate under the mutex, release and sleep as one step,
  recheck on every wake
        |
        v
C11 atomics and memory orders
  modification order, release/acquire, seq_cst
  compiler reorderings, then CPU reorderings (x86-64 TSO, AArch64, RISC-V)
        |
        v
liveness and the cache
  wait-for cycles; false sharing on one coherence line
        |
        v
tools and three systems
  GDB, Memcheck, ASan/UBSan, TSan
  ToyOS tasks, a teaching-OS sleep, Linux tasks plus the C model
```

## Daily roadmap

### Day 1 — Threads, processes, `clone`, stacks, and TLS

[Day 1](sections/day-001-threads-processes-clone-stacks-tls.md) separates
the POSIX thread from a new process: shared address space, private registers,
stack, signal mask, `errno`, and TLS. Linux libc creates that shape with a
`clone`-family call, and the join is both a lifetime edge and a happens-before
edge. Wrapper:
[challenges/day-001-thread-model.sh](challenges/day-001-thread-model.sh).

### Day 2 — Data races and the C abstract machine

[Day 2](sections/day-002-data-races-and-the-c-abstract-machine.md) defines a
data race as conflicting actions, at least one of them non-atomic, with no
happens-before between them. A plausible `counter++` total is still undefined
behavior. The intentional race runs only through the Day 2 and Day 7 wrappers.
Wrapper:
[challenges/day-002-data-races.sh](challenges/day-002-data-races.sh).

### Day 3 — Mutexes, ownership, lock order, and the futex boundary

[Day 3](sections/day-003-mutexes-ownership-lock-order-futex.md) ties a mutex
to a named invariant and to the unlock/lock happens-before edge. Heap ownership
moves at the join. Two locks need one global order. An uncontended lock can
stay in userspace; contention may enter the kernel through `futex`. Wrapper:
[challenges/day-003-mutex-ownership.sh](challenges/day-003-mutex-ownership.sh).

### Day 4 — Condition variables and bounded queues

[Day 4](sections/day-004-condition-variables-and-bounded-queues.md) starts
from the gap a mutex cannot close. The waiter must check `count` while
holding the mutex, then release the mutex and become a published sleeper as
one operation, then re-acquire and check again. A signal that finds an empty
wait list stores nothing. Spurious returns and stolen wakeups are why the
lab's `put` and `get` use `while`. Empty and full are the two predicates of
an eight-slot ring. glibc implements `pthread_cond_t` in userspace and
blocks with `futex`; the lesson quotes `wait_event`, `set_current_state`,
and `futex_wait` from tree `c9b2385d6793` only. Wrapper:
[challenges/day-004-condition-queue.sh](challenges/day-004-condition-queue.sh).

### Day 5 — C11 atomics and memory ordering

[Day 5](sections/day-005-c11-atomics-and-memory-ordering.md) separates
modification order from synchronization. Relaxed fetch-adds on one counter
have a single order, so 800000 increments finish at 800000 after the joins.
Release and acquire publish a payload. Seq_cst adds one total order, which
is why both-zero is outside a seq_cst store-buffering litmus and inside the
relaxed litmus the lab runs. The compiler reorders under the C model; the
CPU reorders under the ISA. In the excerpt tree, x86-64
`__smp_store_release` is a compiler barrier plus a plain store, AArch64
emits `stlr`/`ldar`, and RISC-V emits `fence`. A litmus count on x86-64
does not prove the C program. Wrapper:
[challenges/day-005-atomics-memory-order.sh](challenges/day-005-atomics-memory-order.sh).

### Day 6 — Liveness, false sharing, and scalability

[Day 6](sections/day-006-liveness-false-sharing-and-scalability.md) draws
deadlock as a wait-for cycle and breaks it with one global lock order. The
detector forces the cycle, waits with `pthread_mutex_timedlock`, and exits;
the process does not hang. False sharing is Week 2's cache line: two cores
writing different atomic objects on one line bounce that line through the
coherence protocol (the MESI picture of exclusive ownership). The kernel
header `include/linux/cache.h` in the excerpt tree defines
`____cacheline_aligned_in_smp` for the same reason. Adding cores helps
private lines and adds contenders on a shared written line. The three timing
samples are one machine, one compiler, one layout. Wrapper:
[challenges/day-006-liveness-scalability.sh](challenges/day-006-liveness-scalability.sh).

### Day 7 — Debuggers, sanitizers, and three systems

[Day 7](sections/day-007-debuggers-sanitizers-and-system-comparison.md)
puts GDB, Valgrind Memcheck, ASan/UBSan, and ThreadSanitizer on different
layers: architectural state at a stop, heap definedness, compiler-inserted
shadow checks, and conflicting accesses missing happens-before. ToyOS at its
scheduler milestone runs two ring-0 tasks, cooperatively and from the timer,
with `TASK_BLOCKED` meaning finished and with no C memory model. A small
teaching OS adds `sleep`/`wakeup` on a channel, which you quote only after
pinning a revision. Linux adds tasks, futex, and the C model the lab is
written in. Wrapper:
[challenges/day-007-debug-report.sh](challenges/day-007-debug-report.sh).

## Safety and evidence

The intentional data race has undefined behavior. Run it only through the
Day 2 or Day 7 wrapper. A ThreadSanitizer build can fail to link or to
start when `libtsan` is missing or the runtime cannot map itself. A skip is
not a pass. A finished TSan run that does not report the race is not proof
the accesses were ordered; the wrapper records that outcome as a skip.

The deadlock exercise uses a timed lock, then unlocks, joins, and exits. It
demonstrates a wait-for cycle without leaving a hung process.

False-sharing timings and the relaxed litmus are measurements. They are not
correctness tests. The deterministic suite checks that those programs print
their caveat lines. An x86-64 `both_zero` count, including zero, describes
that run. Portable meaning stays with the C memory model, the cited ISA
manuals, and the compiler output you actually have.

## Completion checkpoint

Finishing the wrappers does not complete mastery. Answer from the source and
the logs, with hashes and versions, and leave a question blank when the run
did not speak to it:

1. Which state is process-wide, which is per-thread, and who owns each object?
2. Which operations conflict, and what happens-before edge orders them?
3. Why can `counter++` fail in the abstract machine even when a run prints
   the expected total?
4. What invariant does each mutex protect, and what global lock order holds?
5. Why does a condition wait recheck its predicate while holding the mutex?
6. Which memory order matches the protocol, and which kernel mapping
   (`barrier` plus store, `stlr`/`ldar`, or `fence`) did you actually read?
7. Which claim came from an assertion, a timer, a debugger, a sanitizer, a
   compiler listing, an ISA manual, or the excerpt tree?
8. What is still open because of scheduling, optimization, architecture,
   a missing `libtsan`, or a two-thread sample on one machine?

Do not auto-fill mastery, confidence, hours, or completion. The
[quizzes](quizzes.json) and [week-11-material.pdf](week-11-material.pdf)
sit beside this overview and do not grade the report.
