# Day 3 — Mutexes, Ownership, Lock Ordering, and the Futex Boundary

**Target time:** approximately 3 hours

> A mutex is useful because it protects a named invariant, not because calls to
> `lock` and `unlock` happen to surround some code.

## 1. Check two protocols

Run `challenges/day-003-mutex-ownership.sh`. Predict an exact mutex count of
800000, one ownership transfer producing 42, and completion of the globally
ordered two-lock mode. The wrapper also identifies source lines; your task is
to explain why those lines define complete protocols.

In the counter mode, every read-modify-write of `mutex_counter` occurs while
`counter_lock` is held. In the ownership mode, the parent allocates and
initializes an object, passes its pointer at thread creation, refrains from
accessing it while the worker may mutate it, joins, validates the returned
pointer, and frees exactly once. These solve different problems:
synchronization for a repeatedly shared object versus temporal transfer of
exclusive access.

## 2. Name the protected invariant

Before adding a mutex, write the invariant in a sentence. For the counter:
“while `counter_lock` is held, `mutex_counter` equals the number of completed
protected increments.” For a linked list it might include reachability, link
consistency, and size. For a bank transfer it likely spans two balances and a
ledger entry.

Then list every location that reads or writes the protected fields. Locking
only writers is insufficient when an unsynchronized reader conflicts with
them. Using different mutexes for the same invariant is insufficient unless a
documented partition proves the accesses concern disjoint state.

The mutex should be initialized before any user can reach it and destroyed
only after no current or future operation can lock, unlock, or wait with it.
Destroying an in-use mutex is a lifetime/protocol error. Static initialization
works for the canonical process-lifetime mutexes; dynamic objects need explicit
construction and teardown paths.

## 3. Understand ownership rules

A normal POSIX mutex has an owner while locked. The successful locker must
unlock it; unlocking from another thread is not a portable handoff mechanism
and can be undefined depending on mutex type. Error-checking mutexes can turn
some mistakes into diagnostics, but they do not replace a correct protocol.
Recursive mutexes can conceal accidental reentrancy and make invariants harder
to audit.

Object ownership is broader than mutex ownership. “Owner” should answer who
may mutate an object, who must release it, and how that responsibility moves.
The canonical heap object is created by the parent, exclusively used by the
worker between creation and join, and freed by the parent after join. No mutex
is needed because there are no concurrent conflicting accesses.

A common failure is freeing task memory after calling `pthread_create`, on the
assumption that the worker already copied it. Creation only starts a possible
execution; it does not promise the worker has dereferenced the argument before
the creator proceeds. Transfer must keep the allocation alive until an
acknowledgment, join, queue protocol, or worker-owned free.

## 4. Derive happens-before from lock operations

Within one thread, initialization is sequenced before unlock. A mutex unlock
synchronizes with a later successful lock of that mutex, so protected writes
become visible to the next critical section. Mutual exclusion and ordering
together let programmers reason in critical-section units instead of raw
instruction interleavings.

The relation is tied to the same synchronization object and its valid
lifetime. Unlocking mutex A does not automatically publish data to a thread
that locks unrelated mutex B. Merely observing that a lock was uncontended
does not remove its ordering semantics.

Keep critical sections large enough to preserve the invariant and small
enough to avoid unnecessary contention. Splitting one logical update across
two lock/unlock pairs can expose an invalid intermediate state. Conversely,
performing blocking I/O or expensive callbacks while holding a widely used
mutex can damage liveness and introduce unknown lock ordering.

## 5. Establish a global lock order

Multiple locks create a new obligation. If one path acquires A then B while
another acquires B then A, both can hold one and wait forever for the other.
The canonical ordered mode imposes lock 0 before lock 1 on every path. A rank
system generalizes this: acquire in increasing rank, release normally in
reverse, and never call code with unknown locking behavior while holding a
ranked lock.

The policy must cover error paths and callbacks. “Usually A before B” does not
prevent a rare deadlock. Sometimes try-lock, timed locking, lock coupling, or
coarser locking is appropriate, but each needs an explicit recovery rule. A
timeout detects lack of progress; it does not make partially completed work
transactional.

Document whether a function requires a lock, acquires it internally, or must
be called lock-free. Assertions or lock-order tooling can make the convention
executable. Avoid exposing raw protected pointers beyond the lock's scope.

## 6. Locate the userspace/kernel boundary

On Linux, pthread mutex implementations commonly use an atomic userspace fast
path when the mutex is uncontended. Contention may invoke `futex` operations
so the kernel can block and wake tasks. This avoids a system call for every
successful lock. Exact word layouts, adaptive spinning, robust-list behavior,
and futex op selection are libc/version/architecture details.

“Mutexes are futexes” is too broad. POSIX specifies mutex behavior; libc may
combine atomics, spinning, and Linux futex syscalls to implement it. The futex
kernel interface coordinates waiting on a userspace value but does not own the
high-level protected invariant. An uncontended trace may contain no futex
syscall at all, while a contended run may still vary with timing.

If `strace -f -e futex` shows calls, that proves selected syscall-boundary
events for that run. It does not reveal every lock acquisition, cache transfer,
or scheduler decision. If it shows none, it does not prove mutex calls were
optimized away; they may have remained on the fast path.

## 7. Handle failures without corrupting ownership

Pthread routines generally return an error number directly rather than setting
`errno`. The canonical `check` helper prints the returned value and exits.
Production code often needs structured unwinding: release locks already held,
join created threads, free only still-owned objects, and preserve the first
useful diagnostic.

Cancellation complicates cleanup because a thread can terminate at a
cancellation point while owning resources. Cleanup handlers, restricted
cancellation regions, or a no-cancellation policy around critical sections may
be necessary. Robust mutexes add owner-death recovery semantics, but the next
owner must repair state and mark consistency; they do not guarantee the
interrupted invariant remained valid.

Debug lock problems by recording the invariant, owner, acquisition stack,
rank, wait target, and lifetime. A debugger snapshot can show a current wait,
but the source protocol explains whether it is legal.

## Practical challenge

**Purpose:** connect mutex exclusion and synchronization to a named invariant,
explicit heap ownership, global lock ordering, and Linux's optional futex slow
path.

**Prediction:** protected count, ownership result, and ordered locking pass;
any futex trace is environment- and schedule-dependent.

**Evidence:** checked program markers, source audit of every protected access,
ownership timeline, lock-order graph, and narrowly scoped optional syscall
trace.

**Limits:** no claim about mutex representation, fairness, uncontended syscall
count, robust recovery, priority inversion policy, or all production failure
paths.
