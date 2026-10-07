# Day 4 — Condition Variables, Predicate Loops, and Bounded Queues

**Target time:** approximately 3 hours

> A condition variable is a notification mechanism. The shared predicate,
> checked under its mutex, decides whether a thread may proceed.

## 1. Exercise the queue

Run `challenges/day-004-condition-queue.sh`. Predict 2000 consumed items, a
sum of 2001000, an empty final queue, and the marker
`condition_waits=while_predicate_loops`. Do not predict which producer or
consumer runs first, how often either condition variable waits, or whether a
spurious wakeup occurs.

The canonical queue has capacity eight, two producers, and two consumers.
Small capacity forces coordination without requiring stress. Producers insert
positive values; after both producers join, the parent inserts one `-1`
sentinel per consumer. Every consumer returns a heap-allocated partial result,
which the parent joins, combines, and frees.

The final count and arithmetic sum jointly detect many missing, duplicate, or
corrupted-item failures. They do not prove FIFO order per producer or exercise
every schedule.

## 2. State predicates before operations

The queue's state is `items`, `head`, `tail`, and `count`, all protected by
`queue.lock`. Two predicates control progress:

```text
can_put := count < CAPACITY
can_get := count > 0
```

`put` locks, waits while `can_put` is false, writes at `tail`, advances the
index modulo capacity, increments `count`, signals `not_empty`, and unlocks.
`get` locks, waits while `can_get` is false, reads at `head`, advances,
decrements, signals `not_full`, and unlocks.

The structural invariant is `0 <= count <= CAPACITY`, with head and tail always
valid indices. Mutex exclusion keeps each transition atomic relative to other
queue operations. Condition variables avoid busy-waiting when no legal
transition is currently available.

## 3. Understand the atomic wait transition

`pthread_cond_wait(&condition, &mutex)` requires the caller to hold the mutex.
Conceptually it releases the mutex and blocks as one coordinated operation,
then reacquires the mutex before returning. That coordination prevents the
classic lost-wakeup gap where a waiter checks a false predicate, another
thread makes it true and signals, and only then the waiter starts sleeping.

The mutex—not the condition variable—protects the predicate. Every code path
that changes `count` does so under `queue.lock`. The waiter checks the
predicate under that same lock. The signal occurs while the state transition
is still protected in this implementation, and the awakened waiter cannot
inspect the new state until it reacquires the mutex.

A signal with no current waiter is not stored as a future permit. Condition
variables are not counting semaphores. The queue state persists; notifications
only prompt waiters to check that state again.

## 4. Always use a predicate loop

The wait must be inside `while`, not `if`. POSIX permits spurious wakeups, so a
wait can return without the program-specific predicate becoming true. Even
without a spurious wakeup, another awakened or already-running thread may
acquire the mutex first and consume the only item or slot. By the time this
thread reacquires the lock, its predicate can be false again.

The correct pattern is:

```text
lock
while predicate is false:
    wait(condition, lock)
perform transition
possibly notify
unlock
```

This pattern makes wakeup causes irrelevant to correctness. A wakeup is an
invitation to re-evaluate state. Code that attempts to distinguish a “real”
signal from a spurious one usually models the condition variable incorrectly.

Timeouts add another possible return, but timeout and predicate transition can
race. After a timed wait, re-check the predicate under the mutex and interpret
the API's return carefully. Use the clock configured for the condition
variable, and expect wall-clock changes if it uses realtime.

## 5. Choose signal or broadcast from the transition

`pthread_cond_signal` wakes at least one eligible waiter; `broadcast` wakes all.
In this queue, one insertion creates at least one unit of consumer progress,
and one removal creates one unit of producer progress, so signaling one is a
reasonable policy. Correctness still depends on the predicate loop.

Broadcast is useful when one transition may enable many waiters or when
different predicates share a condition variable and all must re-check. It can
also create a thundering herd: many threads wake, contend for the mutex, and
most return to sleep. Signal can be insufficient in protocols where multiple
units become available or where the selected waiter cannot make the progress
another waiter could.

Notification placement is a protocol choice. Updating the predicate must
happen under the mutex. Signaling while holding it often makes the relationship
obvious; signaling after unlock can be correct for carefully designed
protocols but requires proof that lifetime and predicate transitions cannot
race with waiter setup or object destruction.

## 6. Prove termination and ownership

After producers join, no more positive values will be created. The parent then
enqueues exactly one sentinel per consumer. Queue FIFO behavior ensures all
previously queued work precedes subsequently inserted sentinels. Each consumer
stops after one sentinel, so every consumer can terminate without a shared
“done” race.

This shutdown technique assumes `-1` is outside the valid data domain. In a
general queue, use a tagged item, a protected closed flag, reference counts, or
an explicit close operation. If producers can fail, the parent must still
decide how to close and wake blocked consumers. A missing sentinel can leave a
join blocked forever; too many can terminate consumers before all work under a
different ordering protocol.

Each consumer allocates its result and returns ownership through `join`.
Allocation failure returns `NULL`; the parent detects it. One limitation is
that the other threads have already completed by then, so this demo reports
failure rather than implementing coordinated early cancellation.

## 7. Debug queue hangs systematically

For a hang, inspect all threads and classify each: running, waiting for the
mutex, waiting on `not_empty`, waiting on `not_full`, or blocked in join. Record
`count`, `head`, `tail`, producer completion, number of sentinels inserted, and
the owner of the mutex. Do not mutate shared variables in the debugger before
capturing evidence.

For corruption, verify every state access holds the intended mutex. Then check
index arithmetic, item domain, duplicate ownership, consumer result lifetime,
and shutdown transitions. Sanitizers can detect invalid memory but do not prove
the queue protocol. Helgrind/DRD or TSan may help where available, with their
respective runtime and modeling limits.

A single successful queue run establishes the checked count, sum, and final
state for that artifact and execution. It does not establish fairness, maximum
throughput, absence of starvation, or correctness for multiple producers that
terminate unexpectedly.

## Practical challenge

**Purpose:** derive a bounded producer-consumer queue from protected predicates,
atomic wait transitions, re-check loops, and explicit shutdown ownership.

**Prediction:** all 2000 values are consumed exactly in aggregate, the expected
sum is observed, and queue count returns to zero.

**Evidence:** checked count/sum, source audit of every queue-state access,
predicate transition table, and an ownership account for sentinels and result
allocations.

**Limits:** no guaranteed wake order, fairness, observed spurious wakeup,
failure-injection recovery, cancellation safety, throughput conclusion, or
proof over every schedule.
