# Day 6 — Deadlock, Starvation, False Sharing, and Scalability

**Target time:** approximately 3 hours

> Correct final values establish safety properties. They do not establish that
> every thread progresses promptly or that added cores improve throughput.

## 1. Run bounded liveness and timing experiments

Run `challenges/day-006-liveness-scalability.sh`. Predict that the detector
reports a lock cycle without hanging, the ordered mode completes, and both
false-sharing timings are nonnegative. Do not predict which timed lock returns
`ETIMEDOUT`, a fixed adjacent/padded ratio, or any speedup.

The deadlock detector deliberately synchronizes two workers after each has
acquired a different mutex. Each then attempts the other mutex with a deadline.
At least one attempt must time out, after which locks are released and threads
join. This reproduces the wait-for cycle while bounding the test.

The false-sharing mode places two atomic counters first adjacent and then in
separately aligned/padded structures. It records elapsed time for one run of
each. The program itself labels the timing as noisy and not a correctness test.

## 2. Distinguish safety from liveness

Safety means “nothing bad happens”: no data race, no broken invariant, no
double free. Liveness means “something good eventually happens”: an operation
completes, a waiter is served, the system continues to make progress. Mutexes
can establish safety while a bad acquisition graph destroys liveness.

A deadlock is a state in which participants wait in a cycle that cannot resolve
without external intervention. The classic necessary conditions are mutual
exclusion, hold-and-wait, no forced preemption of resources, and circular wait.
Breaking any one can prevent this form of deadlock. Global lock ordering
directly prevents circular wait.

Not every hang is deadlock. A thread can wait for a condition that no producer
will ever make true, block on I/O, spin on a stale protocol, miss shutdown, or
wait for a thread that failed before signaling. Build a wait-for graph from
actual owners and dependencies before labeling the symptom.

## 3. Treat timeouts as detection, not repair

`pthread_mutex_timedlock` bounds one wait. A timeout says the mutex was not
acquired by the deadline; in the controlled experiment, synchronized opposite
ownership makes that evidence of the intended cycle. In general, a timeout can
also result from preemption, overload, priority inversion, or an overly short
deadline.

After timeout, software must restore or abandon partial state coherently. The
canonical workers have not changed shared application data while holding only
their first lock, so they can release it. A transaction that updates one object
before timing out on another needs rollback, idempotent retry, versioning, or a
different design. “Retry forever” can turn deadlock avoidance into livelock.

Use a monotonic clock where the API allows it for duration-based deadlines.
The canonical mutex timed lock uses `CLOCK_REALTIME` because that is the
portable default for this API form; realtime adjustment is therefore a stated
measurement limitation.

## 4. Recognize starvation and fairness limits

Starvation occurs when one thread repeatedly fails to make progress while
others do. There need not be a cycle. An unfair lock, priority policy, constant
arrival of competing work, or reader-preference design can starve a participant
while aggregate throughput remains high.

POSIX mutexes generally should not be assumed to provide FIFO admission unless
a specific protocol says so. A successful thousand-iteration test cannot prove
starvation freedom. Measure per-thread wait distributions and maximum waits,
not only total throughput, and identify scheduler policy, CPU placement, and
load.

Priority inversion occurs when a high-priority thread waits for a lock held by
a low-priority thread while medium-priority work delays that owner. Priority
inheritance mutex protocols may help in real-time designs, but availability,
permissions, scheduler configuration, and transitive cases matter. Ordinary
desktop timing does not validate a real-time bound.

## 5. Explain false sharing

Coherent caches typically move ownership at cache-line granularity. Two
threads updating distinct atomic objects on the same line do not have a C data
race, yet each write can invalidate the other's cached copy. The line
“ping-pongs,” adding coherence traffic and latency. This is false sharing:
independent language objects interfere because of physical layout.

Padding or alignment can place hot writable fields on different lines. The
canonical `_Alignas(64)` and padding are an experiment, not a universal cache
line declaration. The atomic type size, structure layout, allocator placement,
compiler, CPU line size, and adjacent objects must be checked. Over-padding can
waste cache and memory bandwidth.

False sharing differs from true sharing, where threads intentionally update the
same synchronization object or data. It also differs from a data race. Relaxed
atomics make each benchmark update defined while preserving the coherence cost;
they do not eliminate contention.

## 6. Measure scalability responsibly

Speedup is `T1 / Tp`; efficiency is speedup divided by worker count. Amdahl's
law reminds us that serial work, synchronization, and overhead cap speedup.
Contention can make additional threads slower. Work partitioning, NUMA
placement, memory bandwidth, allocator behavior, and operating-system noise
all influence results.

A useful benchmark includes warmup where justified, repeated samples,
distribution summaries, controlled input, correctness checks, and environment
metadata. Pinning threads can reduce migration but also create an artificial
placement; report it. CPU frequency scaling, simultaneous multithreading,
background load, thermal limits, and virtualization must be considered.

The canonical benchmark is deliberately too small for a scalability claim. It
runs adjacent first and padded second, so order effects are confounded. It
does not pin CPUs, inspect line addresses, repeat trials, or report hardware
counters. Its value is generating a hypothesis and teaching measurement limits.

## 7. Improve designs before micro-optimizing

Reduce shared writes through per-thread counters combined after join, sharded
locks, batching, immutable snapshots, or ownership transfer. Such changes can
remove contention and false sharing together. They also change memory usage,
latency, complexity, and consistency semantics.

Lock-free algorithms can improve some workloads but introduce atomic hot spots,
retry storms, ABA concerns, and difficult reclamation. A lock-free progress
guarantee is system-wide, not necessarily fair to each thread. Wait-free
algorithms provide a bounded per-operation progress guarantee but are harder
and may have larger constants.

Optimize only after preserving an executable correctness oracle. Compare
throughput and tail latency at multiple thread counts against a single-thread
baseline. If results differ, use profilers and hardware counters where
available, labeling sampled evidence and event semantics. Avoid attributing a
timing ratio to false sharing without confirming layout and coherence events.

## Practical challenge

**Purpose:** observe a bounded wait-for cycle, verify global ordering, measure
a false-sharing hypothesis, and separate correctness, liveness, and performance
claims.

**Prediction:** timed detection and ordered locking terminate; adjacent and
padded runs produce variable timings with no required winner.

**Evidence:** lock graph, timeout return codes, completion markers, binary and
environment identity, object addresses/layout if collected, and repeated
timing distributions for any performance report.

**Limits:** one run does not prove fairness, starvation freedom, cache-line
causality, realtime bounds, or scalability; the benchmark lacks affinity,
counter evidence, randomized order, and statistical power.
