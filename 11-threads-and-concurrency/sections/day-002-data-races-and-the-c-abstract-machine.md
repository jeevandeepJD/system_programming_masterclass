# Day 2 — Data Races and the C Abstract Machine

**Target time:** approximately 3 hours

> A bad final count is evidence of a bug. A correct final count is not evidence
> that racy C was valid.

## 1. Observe the contrast safely

Run `challenges/day-002-data-races.sh`. It executes the checked mutex counter,
then attempts a separately compiled ThreadSanitizer build of the intentional
`race` mode. Predict that the mutex result is exactly 800000. Do not predict a
particular racy total or promise that ThreadSanitizer runs.

The wrapper distinguishes three TSan outcomes: the compiler cannot build the
instrumented program; the instrumented runtime cannot execute correctly in
this environment; or a completed instrumented run reports the intentional
race. Toolchain combinations can fail because of address-space layout,
unsupported libc/runtime integration, container restrictions, or sanitizer
runtime defects. An explicit skip is accurate evidence. It is not a pass for
the program.

The ordinary race run is allowed to print any count, including the expected
one. Because the program has undefined behavior, that number cannot validate
the source. The bounded invocation is pedagogical evidence only.

## 2. Define a C data race

In the C abstract machine, a data race exists when different threads perform
conflicting actions on the same memory location, at least one action is not
atomic, and the actions are not ordered by happens-before. Two actions
conflict when one modifies an object and the other reads or modifies the same
object (with details for overlapping storage).

The key words are not “simultaneous in wall-clock time.” Happens-before is a
language-level ordering assembled from sequenced-before relations within a
thread and synchronizes-with relations supplied by operations such as mutex
unlock/lock pairs, thread creation/joining, and suitable atomic release/acquire
communication. If there is no required ordering, a lucky scheduler does not
repair the source.

A data race gives undefined behavior. This is stronger than “one increment may
be lost.” The implementation is no longer constrained to preserve the
intuitive interleaving model for that execution. Debugging must therefore
identify the missing ordering, not merely reproduce a numeric discrepancy.

## 3. Expand `counter++`

At source level, `counter++` is a read-modify-write expression, but an ordinary
integer object does not make that operation atomic. A conceptual expansion is:

```text
temporary = counter
temporary = temporary + 1
counter = temporary
```

Two workers can read the same old value and both store the same incremented
value, apparently losing one update. Machine code may use a load, arithmetic
instruction, and store, but the exact sequence depends on optimization and
architecture. Even one machine instruction would not by itself establish C
atomic semantics: alignment, compiler assumptions, ISA guarantees, and the C
memory model all matter.

The canonical `unsafe_counter` is written by four threads without mutex or
atomic operations. Those accesses conflict and lack happens-before ordering.
The compiler may assume that a conforming program has no data race and
transform code accordingly. Explaining only cache coherence or instruction
interleaving misses this source-level fact.

## 4. Separate compiler ordering from CPU ordering

The compiler translates according to the C abstract machine. It may keep a
value in a register, combine operations, eliminate loads, reorder independent
work, or exploit impossible-under-defined-C assumptions. The CPU then executes
instructions subject to its architecture's ordering and cache-coherence rules.
Both layers affect observations, but they answer different questions.

`volatile` does not bridge the gap. It requests observable accesses useful for
objects such as memory-mapped device registers or signal-related patterns
within their proper rules; it does not make an increment indivisible and does
not create inter-thread happens-before. Replacing the counter with `volatile`
still leaves conflicting non-atomic accesses.

Compiler barriers, CPU fences, atomic operations, and mutexes are not
interchangeable. A compiler barrier may restrict code motion without emitting
a hardware instruction. An ISA fence orders specified machine-memory effects
but does not retroactively make a non-atomic C race defined. C atomics provide
language semantics that the implementation realizes with instructions,
barriers, or library calls.

## 5. Read sanitizer evidence correctly

ThreadSanitizer instruments memory accesses and synchronization, maintaining a
runtime model of happens-before. A useful report identifies two conflicting
accesses, their thread creation sites, and associated stacks. That is much
stronger diagnostic evidence than a bad counter total because it points to the
protocol defect.

TSan is dynamic. It examines paths and schedules reached during that
instrumented run. It can miss unexecuted races, unsupported synchronization,
uninstrumented code, or races hidden by changed timing. Instrumentation also
changes layout and scheduling. A clean report is not a proof of race freedom.

AddressSanitizer detects classes of invalid memory use; UndefinedBehavior
Sanitizer checks selected operations; Valgrind tools use other instrumentation
models. None should be described as a universal “C correctness checker.”
Sanitizers may be mutually incompatible in one binary, and a sanitizer's own
runtime availability must be tested, not inferred from successful compilation.

## 6. Repair according to the invariant

The mutex mode wraps every increment in lock/unlock. The protected invariant is
that `mutex_counter` equals the number of completed critical-section updates.
Unlock by one thread and subsequent lock by another establish synchronization,
while mutual exclusion prevents overlapping updates.

The atomic mode uses `atomic_fetch_add_explicit(..., memory_order_relaxed)`.
Relaxed is sufficient for the narrow invariant that no increments are lost:
each operation is an indivisible read-modify-write in one modification order.
It does not publish unrelated payload data. The final load occurs after all
workers are joined, so thread completion supplies the needed phase boundary.

Choosing between a mutex and an atomic is not “slow versus fast.” A mutex can
protect a multi-object invariant and compose a critical section. An atomic
counter protects only its atomic operation unless a larger protocol explicitly
connects other state through memory ordering.

## 7. Diagnose a race from source

For each shared object, list all accesses, mark writes, and identify the
ordering edge between every potentially concurrent conflicting pair. A verbal
claim such as “the producer goes first” is insufficient unless enforced by
creation ordering, a mutex/condition protocol, atomic communication, or
another documented primitive.

Check object lifetime too. A use-after-free can look like a race, and a race
can permit a free to occur while another access is active. Ownership and
happens-before are two separate obligations: the object must exist, and the
access must be legal.

Do not fix symptoms with sleeps. Delays alter probability but establish no C
synchronization. Do not fix a counter by narrowing the test until it usually
passes. State the invariant, select a primitive that provides the required
ordering and atomicity, and verify both normal and failure paths.

## Practical challenge

**Purpose:** explain the intentional counter race using the C abstract machine,
then contrast a real synchronization repair with dynamic diagnostic evidence.

**Prediction:** the mutex mode is exact; the race output is unconstrained; TSan
either reports a race or is classified accurately as unavailable/inconclusive.

**Evidence:** source access inventory, happens-before graph, checked mutex
result, compiler command, sanitizer diagnostic and exit status.

**Limits:** no exhaustive schedule exploration, portability proof from emitted
instructions, claim that `volatile` synchronizes, or claim that a clean or
unavailable TSan run proves race freedom.
