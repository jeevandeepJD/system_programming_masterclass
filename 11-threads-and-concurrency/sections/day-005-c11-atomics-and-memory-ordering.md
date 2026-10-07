# Day 5 — C11 Atomics, Memory Orders, and Architecture Mappings

**Target time:** approximately 3 hours

> The portable claim comes from the C memory model. Assembly and hardware
> observations explain an implementation; they do not redefine the language.

## 1. Run two deliberately different atomic experiments

Run `challenges/day-005-atomics-memory-order.sh`. Predict an exact atomic
counter of 800000 because each relaxed fetch-add participates in the atomic
object's modification order. Predict only that the litmus completes 100000
rounds and prints a nonnegative `both_zero` count. Do not predict that
both-zero must or must not appear on this host.

The wrapper may also request compiler assembly for the canonical source. Treat
it as evidence for that compiler, flags, source, and target only. Optimizers
can transform surrounding code, and atomics that look similar in C can map
differently depending on width, alignment, target extensions, or whether an
operation is lock-free.

Most importantly, neither seeing nor failing to see an outcome on x86-64 proves
portable C behavior. The litmus source itself uses relaxed atomics and is
defined; the C model determines its allowed executions.

## 2. Begin with atomicity and modification order

An atomic object is accessed through atomic operations. Each atomic object has
a single modification order for its writes and read-modify-write operations.
`atomic_fetch_add` is indivisible with respect to competing atomic operations
on that object, so four workers cannot lose increments as the non-atomic mode
can.

`memory_order_relaxed` supplies atomicity and modification-order coherence but
no release/acquire synchronization for unrelated ordinary objects. It is ideal
for independent statistics where only the count matters. It is not sufficient
to publish a pointer plus initialized payload merely because the pointer store
is atomic.

After joining all workers, the parent loads the counter. Join orders completed
thread actions before the joiner proceeds. The demo's exact result therefore
comes from atomic increments plus the completion phase, not from relaxed
ordering magically synchronizing every object.

## 3. Build a release/acquire publication

Suppose a producer fills ordinary payload fields, then performs a release store
of `ready = true`. A consumer performs an acquire load that reads that store
(or its release sequence) and then reads the payload. The release synchronizes
with the acquire, creating happens-before from payload initialization to
payload consumption.

The acquire must actually observe the appropriate release relationship. An
acquire load returning `false` does not import the producer's writes. A relaxed
load of `ready` preserves atomicity but does not establish this publication
edge. The protocol normally loops or otherwise waits until the expected value
is acquired.

Release prevents prior relevant actions from moving after publication in the
abstract semantics; acquire prevents subsequent relevant actions from moving
before consumption. They are directional. Using release on a load or acquire
on a plain store is not a meaningful substitute, and the API restricts which
orders each operation accepts.

## 4. Place stronger orders carefully

`memory_order_seq_cst` adds all sequentially consistent operations to one
global order consistent with required per-thread order. It simplifies many
arguments but does not turn a collection of operations into one transaction.
Ordinary data still needs a valid synchronization protocol, and invariants
spanning several atomics can expose intermediate states.

Acquire-release read-modify-write operations combine both directions.
`memory_order_acq_rel` can consume prior publication and publish preceding
updates while atomically changing one object. A failed compare-exchange does
not perform the write, so it has a separately specified failure order that
cannot include release semantics.

`memory_order_consume` has historically been difficult to implement as its
dependency-based intent suggested; portable teaching and production guidance
commonly uses acquire instead. Always check the language/toolchain version
rather than assuming an informal dependency rule.

Fences can connect carefully structured atomic operations, but they are easier
to misuse than release/acquire on the communicating object. A fence does not
make nearby non-atomic races legal by proximity.

## 5. Analyze the store-buffering litmus

The canonical litmus has atomic `x` and `y`, initially zero each round. Thread
0 stores 1 to `x` relaxed and loads `y` relaxed. Thread 1 stores 1 to `y`
relaxed and loads `x` relaxed. Barriers separate rounds but do not impose a
cross-thread order between the two atomic operations inside a round.

Both loads may read zero under relaxed C semantics. Each atomic still has a
coherent modification order; there is simply no release/acquire edge requiring
either load to observe the other thread's store. The barrier after the
operations lets the controller safely inspect the result array, whose slots
are written by separate workers and not reused until phase completion.

Changing operations to sequential consistency alters the allowed global-order
argument: both-zero cannot be placed consistently with each store preceding
its following load and both loads preceding the other stores. Validate such
claims against the exact source; compiler transformations and barrier
semantics matter.

## 6. Map cautiously to x86-64, AArch64, and RISC-V

On x86-64's commonly described TSO model, ordinary aligned loads/stores already
provide relatively strong ordering, but store-to-load relaxation through store
buffers matters to the litmus. A compiler may emit plain instructions for
relaxed and release/acquire operations and use locked operations or fences
where stronger guarantees require them. Exact lowering depends on the
operation and compiler.

AArch64 has a weaker ordering model and dedicated acquire/release forms such as
`LDAR` and `STLR`; relaxed operations can use ordinary `LDR`/`STR`, while
read-modify-write may use exclusive loops or LSE atomics. Barriers such as
`DMB` can implement stronger order. Architectural extensions and target flags
change the sequence.

RISC-V's base weak memory model uses ordering bits on atomic instructions and
fences; implementations and compiler mappings depend on the A extension,
profiles, and evolving architecture/toolchain support. Loads/stores plus
`fence` or acquire/release annotations on AMOs may realize C orders.

These are mapping landmarks, not a lookup table. For each claim record compiler
version, target triple, optimization, ISA extensions, ABI, source, and emitted
instructions. Never transfer an observed x86 result into a portable guarantee.

## 7. Separate compiler and hardware evidence

Compiler explorers and `-S` output show selected lowering. They cannot show
runtime cache state or all legal compiler choices. ISA manuals define machine
ordering, not C object lifetime or data-race rules. Hardware counters and
litmus tools measure or explore architecture behavior, but a user-space test
has finite samples and environmental noise.

Start a correctness proof in C: objects, operations, modification orders,
sequenced-before, synchronizes-with, and happens-before. Then inspect compiler
output to understand costs or diagnose a suspected lowering issue. Finally use
architecture documentation and measurements to explain the machine behavior.
Do not reverse that hierarchy.

Atomics also do not solve reclamation automatically. A lock-free pointer can
be atomically updated while another thread dereferences freed storage. Hazard
pointers, epochs, reference counting, or another lifetime scheme is required.
Lock-free means some operation makes system-wide progress; it does not mean
wait-free, fair, simple, or faster.

## Practical challenge

**Purpose:** distinguish atomicity from ordering, derive release/acquire
happens-before, interpret a relaxed litmus, and compare representative ISA
mappings without weakening the portable argument.

**Prediction:** the relaxed counter is exact; litmus count varies; assembly
depends on compiler and host target.

**Evidence:** C source protocol, checked counters, finite litmus observations,
compiler identity/options, emitted assembly, and cited ISA rules.

**Limits:** no portability claim from x86 observation, no exhaustive hardware
outcome search, no promise of lock freedom, no universal instruction mapping,
and no automatic solution to multi-object invariants or reclamation.
