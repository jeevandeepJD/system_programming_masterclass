# Day 7 — GDB, Valgrind, Sanitizers, and System Comparison

**Target time:** approximately 3 hours

> A concurrency debugging report must identify the artifact, preserve the
> failure, connect evidence to a protocol, and state what each tool cannot see.

## 1. Generate the evidence bundle

Run `challenges/day-007-debug-report.sh`. It creates a fresh temporary report,
runs the canonical deterministic Week 11 suite, captures identity, executes
intentional bounded failures, and tries optional GDB, Valgrind,
AddressSanitizer/UBSan, and ThreadSanitizer checks.

Predict `WEEK11 PASS`. Predict that the deadlock detector terminates and that
the data-race mode remains outside deterministic correctness testing. Do not
predict optional-tool availability. A tool binary can exist while its runtime
is unusable; this is especially relevant for TSan.

Keep the full diagnostics when a tool cannot run. “SKIP: TSan runtime
unavailable or no report” is more accurate than claiming the race passed.
Likewise, a debugger blocked by ptrace policy has produced no stopped-state
evidence.

## 2. Establish reproducible identity

Record UTC time, `uname -a`, architecture, compiler and libc versions, exact
canonical project path, source and binary hashes, build flags, optimization,
and invocation. For optional tools record their versions and exit statuses.
A report against a different binary cannot explain the failure.

Concurrency failures are schedule-sensitive. Preserve standard output and
error, iteration counts, environment variables, CPU count/placement if known,
and whether instrumentation was active. Avoid “fixing” the reproduction with
logging before saving the original evidence; I/O can perturb timing.

If source changes, make a new identity section. Line-level debugger and
sanitizer stacks are meaningful only against matching debug information and
binaries.

## 3. Use GDB for state, not proof of absence

In GDB, `info threads`, `thread apply all bt`, mutex/condition state where
debug symbols permit, and selected shared variables can capture a stopped
snapshot. For a hang, classify every thread's current wait and construct a
wait-for graph. For lifetime errors, inspect the allocation owner and frames
that could still hold pointers.

Stopping changes scheduling. A snapshot can establish that thread A currently
waits in a lock path while B owns or waits elsewhere; it cannot by itself prove
the complete historical cause. Mutex internals are libc-private and version
dependent, so do not hard-code field interpretations without matching sources.

Breakpoints serialize execution and may hide races. Watchpoints are limited
hardware resources and observe selected accesses. GDB's scheduler-locking
controls can help isolate a transition but create an execution unlike normal
operation. Record every debugger intervention.

## 4. Use Valgrind and sanitizers according to their models

Valgrind Memcheck can detect invalid reads/writes, use of uninitialized values,
and leaks in executed code, though it is slow and can significantly alter
thread timing. Valgrind's thread tools, where installed, have separate
happens-before models and options; “Valgrind clean” without naming the tool is
ambiguous.

AddressSanitizer uses compiler instrumentation and shadow memory for many
spatial and temporal errors. LeakSanitizer integration varies. UBSan detects
selected undefined operations but is not generally a data-race detector.
ThreadSanitizer instruments memory and synchronization accesses to report
dynamic races.

For TSan distinguish:

1. compiler rejects the instrumentation;
2. binary builds but runtime fails before meaningful execution;
3. run completes without reporting the targeted race;
4. report identifies conflicting accesses and exits by configured status.

Only the fourth is positive detection evidence. The third is inconclusive, not
a correctness result. Any dynamic tool can miss unexecuted paths and can
perturb the schedule.

## 5. Preserve intentional failures

The canonical `thread_lab race` is intentionally undefined and must never be
folded into a suite that treats exit zero or a numeric total as correctness.
Its purpose is to feed a race detector and illustrate why output cannot
validate racy C.

The `deadlock_lab detect` mode intentionally creates opposite acquisition
order, but timed lock bounds the failure. It checks that at least one attempt
times out and then releases both first locks. This is an expected detector,
not permission to deploy timeout-based partial transactions.

False-sharing and relaxed-litmus modes are intentional non-assertions. Their
variable values are evidence to classify, not thresholds to force. A report
that marks a particular ratio or `both_zero` count as required has converted
an observation exercise into a flaky test.

## 6. Debug from invariant to root cause

For a wrong result, list the involved objects and every access. For each
conflicting pair, locate its happens-before edge. For a crash, verify object
lifetime first: allocation, publication, owners, final access, destruction.
For a hang, build a wait-for graph and inspect shutdown predicates.

Minimize while preserving the defect. Keep the same optimization and
instrumentation status during comparison. Then add one tool at a time:
sanitizer for dynamic access evidence, debugger for a stopped snapshot,
syscall trace for futex/blocking boundaries, compiler output for lowering, and
profiler/counters for performance hypotheses.

End every conclusion with scope. “TSan reported concurrent writes to
`unsafe_counter` in this instrumented run” is defensible. “The CPU reordered
the increment” is not established by that report. “Padded was faster once”
does not establish false sharing as the cause.

## 7. Compare ToyOS, xv6, and Linux carefully

Pin exact revisions before comparison. “ToyOS” is course-specific; inspect
whether its current milestone even has multiple execution contexts, shared
address spaces, blocking locks, or a scheduler. Absence is a valid result, not
a defect. Its simplest lock may disable interrupts on one CPU, which does not
automatically generalize to SMP.

xv6 variants differ. The common teaching kernels have a compact process table,
scheduler, sleep/wakeup mechanism, spinlocks, and often per-process address
spaces; lab branches may add kernel threads, copy-on-write, or other features.
Read the chosen RISC-V revision's `proc.c`, lock implementation, sleep/wakeup
code, and memory subsystem. Do not attribute a lab extension to base xv6.

Linux presents pthreads through libc over kernel tasks, supports resource
sharing through clone-family interfaces, futex-backed blocking primitives,
rich scheduling, TLS ABI support, sanitizers in userspace toolchains, ptrace,
procfs, and performance tooling. Complexity expands possible evidence and
lifetime obligations; it does not make teaching kernels “wrong.”

Use a question-based comparison:

- What is the schedulable entity?
- Can execution contexts share one user address space?
- How is a blocked waiter represented and awakened?
- Which primitive supplies mutual exclusion on one CPU and on SMP?
- Where are stack and TLS state established?
- What establishes lifetime after exit/join?
- Which observations come from symbols, serial output, debugger, procfs,
  tracing, or instrumentation?

Separate interface guarantees from implementation landmarks in every system.

## 8. Write the final report

Include identity, reproduction, ownership/lifetime, happens-before graph,
counter race, mutex invariant, queue predicates, atomic ordering, liveness
graph, performance caveats, tool results, intentional failures, and
three-system comparison. For every tool, name the captured artifact and
diagnostic, then state its blind spots.

A strong causal conclusion looks like: “Two instrumented worker stacks report
conflicting non-atomic writes to `unsafe_counter`; source inspection finds no
synchronizes-with edge between increments. The C execution therefore has a
data race and undefined behavior. The report covers this instrumented run and
does not prove TSan observes every schedule.”

Conclude with cleanup and non-claims. All deterministic modes completed, the
deadlock exercise was bounded, every created thread was joined, returned heap
objects were freed, and temporary report paths were printed. No x86
observation was used as portability proof.

## Practical challenge

**Purpose:** produce a reproducible concurrency debugging report joining source
protocols, runtime assertions, debugger/tool evidence, intentional failures,
and revision-aware system comparison.

**Prediction:** deterministic Week 11 tests pass; optional tools either produce
scoped evidence or an explicit diagnostic/skip; intentional race output is
never accepted as correctness.

**Evidence:** versions, hashes, commands, exit statuses, complete logs, stack
traces, happens-before and wait-for graphs, source landmarks, and limits.

**Limits:** no exhaustive schedule proof, guaranteed ptrace/sanitizer runtime,
kernel rebuild, stress campaign, fairness proof, portable claim from one ISA,
or comparison without pinned ToyOS/xv6/Linux revisions.
