# Day 3 — Mutexes, Ownership, Lock Order, and the Futex

**Target time:** approximately 3 hours

> `pthread_mutex_lock` returned, and `mutex_counter` went up by one. What
> invariant was true while the lock was held, who was allowed to touch the
> object, and why did that lock usually finish without entering the kernel?

Day 2 left four threads conflicting on one non-atomic object, with no
happens-before edge between them. The repair in
[`thread_lab.c`](../../projects/linux-system-programming/src/thread_lab.c)
is a mutex around each increment. The lock is there because an invariant
needs to stay true across a read-modify-write, and because the unlock has
to synchronize with the next locker. The variable is one field inside that
invariant. Locking "the counter" is a shorthand for locking the rule the
counter is part of.

Kernel excerpts are from tree `c9b2385d6793`, `kernel/futex.c`. The
assembly and the futex counts are from one host build of `thread_lab`
(GCC 16.2.1, the project's `-O2`) and one `strace` of its `mutex` mode.
Counts move with the schedule. The challenge records a fresh trace.

---

## 1. Why a lock is a statement about an invariant

Write the invariant before choosing the mutex. For this lab it is:

> `mutex_counter` equals the number of increments whose critical sections
> have completed.

Four threads and 200000 iterations make that number 800000, which is the
line the mutex mode is allowed to require. Every read-modify-write of
`mutex_counter` in `increment_mutex` happens between `pthread_mutex_lock`
and `pthread_mutex_unlock` on `counter_lock`. The critical section in the
built binary is a single increment of that object:

```text
mov    $0x30d40, %ebx                 # 200000
mov    $counter_lock, %edi
call   pthread_mutex_lock
incq   mutex_counter(%rip)
mov    $counter_lock, %edi
call   pthread_mutex_unlock
dec    %ebx
jne    ...                            # back to the lock
```

`incq` is still a read-modify-write. It is safe here because the mutex
protocol guarantees that only the holding thread executes it, and that
the holder's view of the object includes every increment whose unlock
has already synchronized with this lock. The instruction did not become
atomic by sitting in a function. The surrounding calls supply the
exclusion and the happens-before edge from day 2.

A linked list would have a larger invariant: reachability, link
consistency, and the size field, all true at the moment the lock is
released. A transfer between two balances would span both balances and
the ledger line. In each case the mutex is the name of that whole
statement. Taking the lock, updating one field, and dropping the lock
before the statement is true again publishes a broken intermediate state
to the next locker. Holding the lock across work that is outside the
invariant — a blocking read, an unbounded callback — keeps the statement
true and stalls every other thread that needs it.

The mutex object itself has a lifetime. `counter_lock` is initialized
with `PTHREAD_MUTEX_INITIALIZER` for the life of the process. A mutex
that lives inside a heap structure has to be initialized before any
thread can lock it, and destroyed only after every lock, unlock, and
wait that names it has finished.

---

## 2. Two kinds of ownership

**Mutex ownership.** A normal POSIX mutex has one owner while it is
locked: the thread whose `pthread_mutex_lock` succeeded. That thread is
the thread that calls `pthread_mutex_unlock`. The lab's
`increment_mutex` locks and unlocks on the same thread, every iteration.
Handing the locked mutex to another thread and unlocking it there is
outside the contract of a normal mutex. An error-checking mutex can
return a diagnostic for some of those mistakes. The diagnostic is a
report. The protocol is still "the locker unlocks."

**Object ownership.** This answers a different question: which thread may
read or write the object, and which thread releases it. The ownership
mode in the same file uses no mutex:

```text
parent: malloc, item->value = 21
parent: pthread_create(consume_owned, item)     # synchronizes-with start
child:  item->value *= 2, return item           # only the child writes
parent: pthread_join                            # synchronizes-with exit
parent: read 42, free(item)                     # only the parent frees
```

Create and join are the edges from day 1. During the child's execution
the parent does not touch `item`. After join the child is gone. The
intervals of access do not overlap, so there is no conflicting pair that
needs a mutex. The heap allocation stays alive across the whole interval
because the parent frees it only after join. Freeing it immediately
after `pthread_create` would end the object's lifetime while the child
might still be in `consume_owned`. Creation starts the child; it does
not promise the child has already read the pointer.

The two words collide on purpose, and they stay separate:

```text
mutex owner     the thread inside pthread_mutex_lock / unlock
object owner    the thread the protocol currently allows to touch the bytes
```

The counter's object is shared for the whole run, so its protocol is
"the mutex owner may touch it, and only then." The heap item's object
owner moves from parent to child at create and back at join, so its
protocol needs no lock.

---

## 3. Where the happens-before edge comes from

Inside one thread, the store to `mutex_counter` is sequenced before the
unlock. A mutex unlock **synchronizes with** a later successful lock of
that same mutex. Anything sequenced before the unlock happens-before
anything sequenced after that later lock.

```text
thread A                         thread B
lock(counter_lock)
  incq mutex_counter             .
unlock(counter_lock)  ─────────► lock(counter_lock)
                                 incq sees A's increment
                                 unlock(counter_lock)
```

The arrow is the unlock and lock of `counter_lock`. An unlock of a
different mutex leaves `mutex_counter` unpublished for a thread whose next
step is to lock `counter_lock`. An uncontended lock still has the edge: the
synchronizes-with relation is part of the operation's meaning, whether
or not a waiter was asleep.

Day 2's race mode has the create and join edges and lacks this middle
arrow. The mutex mode adds the arrow on every iteration. That is why the
checked total is a requirement of the program and the racy total is not.

---

## 4. A global order when one thread needs two locks

One lock has no deadlock cycle. Two locks do, as soon as one path
acquires A then B and another acquires B then A. Each holds the lock the
other is waiting for.

[`deadlock_lab.c`](../../projects/linux-system-programming/src/deadlock_lab.c)
has a path that refuses that cycle. `ordered` always takes `locks[0]`
and then `locks[1]`:

```c
pthread_mutex_lock(&locks[0]);
pthread_mutex_lock(&locks[1]);
pthread_mutex_unlock(&locks[1]);
pthread_mutex_unlock(&locks[0]);
```

Every caller that needs both locks uses that order, including the second
thread the `ordered` mode creates. A rank is the same rule written for
more than two locks: acquire in increasing rank, release in reverse, and
call out to unknown code only when you can name the locks it might take.

```text
legal                         the cycle day 6 studies
thread A    thread B          thread A         thread B
lock 0      lock 0            lock 0           lock 1
lock 1      lock 1            lock 1 (waits)   lock 0 (waits)
unlock 1    unlock 1
unlock 0    unlock 0
```

The challenge runs the left-hand shape and expects
`lock_order=0_then_1 completed=yes`. Day 6 runs the right-hand shape with
a timed lock so the wait ends and the process exits. A timeout reports
that the lock stayed unacquired. Work already performed under the lock
that was acquired remains in place. The order has to hold on the error
path too: a function that takes lock 0, fails, and then takes lock 1
while another thread does the opposite has rebuilt the cycle on the
failure branch.

Document, for each function, whether it assumes a lock is already held,
takes the lock itself, or must be called with no locks held. The lab's
`ordered` takes both internally and releases both before it returns.

---

## 5. Why the fast path stays in user mode

[Week 8](../../08-user-mode-syscalls-and-ipc/sections/day-004-int80-syscall-abi.md)
measured what a syscall costs: a privilege transition, a register packet,
kernel entry, the work, and the return. A mutex taken on every increment
of a hot counter would pay that cost 200000 times per thread if each
lock and unlock entered the kernel. The user/kernel boundary you already
timed would dominate the increment.

The pthread mutex on Linux avoids that for the common case. The mutex
word lives in user memory, in the shared `mm` from day 1. The uncontended
path is a user-mode atomic update of that word — a compare-and-swap from
"free" to "held," or the unlock's store back to "free." `pthread_mutex_lock`
returns to `incq` without a syscall. The kernel is entered when a thread
must sleep because the word is already held, or when an unlock must wake
a thread that went to sleep. That sleep/wake operation is a **futex**.

The split, as a design:

```text
lock, word is free
  atomic compare-and-swap in user mode
  return to the critical section          # no syscall

lock, word is held
  mark "waiters" in the word
  futex_wait                              # syscall, sleep

unlock, no waiters
  atomic store of "free"
  return                                  # no syscall

unlock, waiters were marked
  store "free"
  futex_wake                              # syscall, wake one
```

The integers in the word are a libc encoding. In the comment below, the
kernel waits and wakes on a user address. The invariant about
`mutex_counter` stays in the libc that owns the word.

One run of this host's `thread_lab mutex` printed `mutex_counter=800000`.
`strace -f -e trace=futex` on that run recorded 315 `FUTEX_WAIT_PRIVATE`,
555 `FUTEX_WAKE_PRIVATE`, and 2 `FUTEX_WAIT_BITSET` — 872 futex entries
against 800000 lock calls and 800000 unlock calls. The waits that named
the mutex used the address `increment_mutex` passes to
`pthread_mutex_lock` (`0x4030a8` in that binary) and an expected value of
2, which is glibc's "held, and someone may be waiting" encoding on this
lib, not a kernel constant. One of those waits returned `EAGAIN`: the
kernel re-read the word and found it had already changed, which is the
check the comment in the next section exists to require. Another run will
print different counts. The stable fact is the ratio: hundreds of sleeps
beside hundreds of thousands of critical sections. The sections that found
the word free never appeared in the trace.

---

## 6. What the kernel promises about wait and wake

`kernel/futex.c` opens with the contract. The waiter samples the user
word, then asks the kernel to sleep only if the word is still that
value. The waker updates the word, then asks the kernel to wake. Both
sides compute a hash bucket and take its lock.

```c
/* kernel/futex.c :: file comment :: tree c9b2385d6793
 * lines 47–64
 */
/*
 * READ this before attempting to hack on futexes!
 *
 * Basic futex operation and ordering guarantees
 * =============================================
 *
 * The waiter reads the futex value in user space and calls
 * futex_wait(). This function computes the hash bucket and acquires
 * the hash bucket lock. After that it reads the futex user space value
 * again and verifies that the data has not changed. If it has not changed
 * it enqueues itself into the hash bucket, releases the hash bucket lock
 * and schedules.
 *
 * The waker side modifies the user space value of the futex and calls
 * futex_wake(). This function computes the hash bucket and acquires the
 * hash bucket lock. Then it looks for waiters on that futex in the hash
 * bucket and wakes them.
 *
```

The comment then shows the bug those steps exist to close. A waiter can
read the old value, a waker can update the word and observe an empty
queue, and the waiter can then enqueue and sleep with nobody left to wake
it. The repair is a pair of barriers and a waiter count, so that either
the waiter sees the new value and refuses to sleep, or the waker sees the
waiter and wakes it:

```c
/* kernel/futex.c :: file comment :: tree c9b2385d6793
 * lines 91–121
 */
 * The correct serialization ensures that a waiter either observes
 * the changed user space value before blocking or is woken by a
 * concurrent waker:
 *
 * CPU 0                                 CPU 1
 * val = *futex;
 * sys_futex(WAIT, futex, val);
 *   futex_wait(futex, val);
 *
 *   waiters++; (a)
 *   smp_mb(); (A) <-- paired with -.
 *                                  |
 *   lock(hash_bucket(futex));      |
 *                                  |
 *   uval = *futex;                 |
 *                                  |        *futex = newval;
 *                                  |        sys_futex(WAKE, futex);
 *                                  |          futex_wake(futex);
 *                                  |
 *                                  `--------> smp_mb(); (B)
 *   if (uval == val)
 *     queue();
 *     unlock(hash_bucket(futex));
 *     schedule();                         if (waiters)
 *                                           lock(hash_bucket(futex));
 *   else                                    wake_waiters(futex);
 *     waiters--; (b)                        unlock(hash_bucket(futex));
 *
 * Where (A) orders the waiters increment and the futex value read through
 * atomic operations (see hb_waiters_inc) and where (B) orders the write
 * to futex and the waiters read (see hb_waiters_pending()).
```

Barrier (B) is the `smp_mb` in `hb_waiters_pending` (lines 341–348), which
the wake path calls before it decides the queue is empty. Barrier (A) is
the matching count increment on the wait path. Together they are the
kernel's statement of the lost-wakeup problem you will meet again with
condition variables on day 4: the sleep decision and the wake decision
have to share an order, or a wakeup is consumed by nobody.

`EAGAIN` from the run in section 5 is this re-read succeeding in the
useful direction. The waiter reached the kernel, the word had already
moved, and `futex_wait` returned instead of sleeping. The userspace lock
path tries the atomic update again.

---

## 7. One hash bucket for many user addresses

Wait queues live in a fixed table of buckets. `hash_futex` maps a key
derived from the user address into that table:

```c
/* kernel/futex.c :: hash_futex :: tree c9b2385d6793
 * lines 354–367
 */
/**
 * hash_futex - Return the hash bucket in the global hash
 * @key:	Pointer to the futex key for which the hash is calculated
 *
 * We hash on the keys returned from get_futex_key (see below) and return the
 * corresponding hash bucket in the global hash.
 */
static struct futex_hash_bucket *hash_futex(union futex_key *key)
{
	u32 hash = jhash2((u32 *)key, offsetof(typeof(*key), both.offset) / 4,
			  key->both.offset);

	return &futex_queues[hash & (futex_hashsize - 1)];
}
```

Waiters for different user addresses can share a bucket. `match_futex`
compares the full key so a wake only unblocks waiters for the address it
was asked about. The bucket lock is kernel serialization for the queue.
It is a different object from `counter_lock`, which is the user word the
critical section is built on.

`futex_wait` is the sleep side. It rejects an empty bitset, prepares an
optional timeout, and calls `futex_wait_setup` to re-read the user word
and enqueue under the bucket lock. Only then does it schedule:

```c
/* kernel/futex.c :: futex_wait :: tree c9b2385d6793
 * lines 2684–2709
 */
static int futex_wait(u32 __user *uaddr, unsigned int flags, u32 val,
		      ktime_t *abs_time, u32 bitset)
{
	struct hrtimer_sleeper timeout, *to;
	struct restart_block *restart;
	struct futex_hash_bucket *hb;
	struct futex_q q = futex_q_init;
	int ret;

	if (!bitset)
		return -EINVAL;
	q.bitset = bitset;

	to = futex_setup_timer(abs_time, &timeout, flags,
			       current->timer_slack_ns);
retry:
	/*
	 * Prepare to wait on uaddr. On success, holds hb lock and increments
	 * q.key refs.
	 */
	ret = futex_wait_setup(uaddr, val, flags, &q, &hb);
	if (ret)
		goto out;

	/* queue_me and wait for wakeup, timeout, or a signal. */
	futex_wait_queue_me(hb, &q, to);
```

`uaddr` is the user virtual address of the word. `val` is the value the
caller sampled. `flags` carries whether the futex is process-private.
`FUTEX_WAIT_PRIVATE` in the trace is that private form: both threads
share an `mm`, so the key does not have to be a shared-mapping key.

`futex_wake` is the other side. It builds the key, hashes it, and skips
the bucket lock when the waiter count says the bucket is empty — the fast
path of the wake, still a syscall, but a short one:

```c
/* kernel/futex.c :: futex_wake :: tree c9b2385d6793
 * lines 1586–1611
 */
/*
 * Wake up waiters matching bitset queued on this futex (uaddr).
 */
static int
futex_wake(u32 __user *uaddr, unsigned int flags, int nr_wake, u32 bitset)
{
	struct futex_hash_bucket *hb;
	struct futex_q *this, *next;
	union futex_key key = FUTEX_KEY_INIT;
	int ret;
	DEFINE_WAKE_Q(wake_q);

	if (!bitset)
		return -EINVAL;

	ret = get_futex_key(uaddr, flags & FLAGS_SHARED, &key, FUTEX_READ);
	if (unlikely(ret != 0))
		return ret;

	hb = hash_futex(&key);

	/* Make sure we really have tasks to wakeup */
	if (!hb_waiters_pending(hb))
		return ret;

	spin_lock(&hb->lock);
```

The loop that follows walks `hb->chain` with `match_futex`, calls
`mark_wake_futex` for up to `nr_wake` matches, drops the bucket lock, and
`wake_up_q`s them. A pthread unlock that knows a waiter exists calls this
with `nr_wake` of 1. The woken thread returns to user mode and tries the
atomic lock of the word again. Between the wake and that successful
compare-and-swap, another thread may have taken the word; the woken
thread then waits again. The futex wake is permission to retry, paired
with the user-word protocol libc maintains.

---

## 8. Execution trace: one critical section, then a sleep

One iteration of `increment_mutex` on thread A that finds the word free,
overlapping a thread B that finds it held. The word values here are the
design in section 5; the trace's expected value of 2 is this libc's
"waiters" state, and a different libc can use a different encoding.

```text
thread A, user                         thread B, user                    kernel
word == free
CAS free → held
  pthread_mutex_lock returns
incq mutex_counter
                                       CAS fails (word is held)
                                       store "waiters"
                                       syscall futex_wait(word, val)
                                                                         hash_futex(key)
                                                                         re-read *word
                                                                         val still matches
                                                                         enqueue on bucket
                                                                         schedule B out
store free
word says waiters → syscall futex_wake(word, 1)
                                                                         hash_futex
                                                                         hb_waiters_pending
                                                                         bucket lock, match_futex
                                                                         wake B
                                       futex_wait returns
                                       CAS free → held
                                       incq mutex_counter
                                       unlock
```

A's `incq` and B's `incq` are in different critical sections. A's unlock
synchronizes with B's later successful lock, so B's `incq` happens after
A's. The `incq` instructions themselves are ordinary. The edge is the
mutex.

The iterations where B's CAS succeeds never appear as a `futex` line.
That absence is the point of the design: the syscall exists for the
sleep, and the uncontended path never enters the kernel. A trace full of
`FUTEX_WAKE_PRIVATE` is evidence about the contended tail of that run.
The line count beside 800000 lock calls describes the slow path. One
logical wait can show up as an unfinished line and a resumed line under
`strace -f`, and an unlock with no sleeper never calls `futex` at all. The challenge's limit line
says the same thing. Use the trace to see the boundary. Use the source
to see the invariant.

---

## 9. Mastery: Explain · Draw · Observe · Build

Run [`../challenges/day-003-mutex-ownership.sh`](../challenges/day-003-mutex-ownership.sh).
Predict `mutex_counter=800000`, `joined_value=42` with `freed_by=parent`,
and `lock_order=0_then_1 completed=yes`. If `strace` exists, the script
keeps a futex trace. Predict that the trace is much smaller than 1.6
million lines, and leave the exact count unpredicted.

### Explain

- The lock protects an invariant. For the lab, `mutex_counter` equals the
  number of completed critical sections, and the `incq` is one step inside
  that statement.
- Mutex ownership is "the thread that locked unlocks." Object ownership
  is "who may touch the bytes, and when." The heap transfer uses create
  and join because those intervals do not overlap.
- Unlock of a mutex synchronizes with a later lock of that same mutex.
- `ordered` takes `locks[0]` before `locks[1]` on every path. That order
  is what day 6's opposite-order cycle lacks.
- The uncontended lock is a user-mode atomic on a word in the shared
  `mm`. `futex_wait` and `futex_wake` are the sleep and the wakeup.
  `hash_futex` places both on a bucket in `futex_queues`. The kernel
  re-reads the user word under the bucket lock so a wake and a wait
  cannot miss each other.

### Draw

1. The invariant as a sentence, and every source line that reads or
   writes `mutex_counter`, each marked "lock held" or "lock not required
   because this thread is the only one alive in this interval."
2. The object-ownership timeline for `item` from `malloc` to `free`.
3. Two threads and two locks in index order, beside the cycle where each
   holds the lock the other waits for.
4. The uncontended CAS and the contended `futex_wait` / `futex_wake`
   path, with the user/kernel boundary drawn on the syscall only.

### Observe

From the challenge output, keep:

- `mutex_counter=800000 expected=800000`;
- `ownership=heap_transfer joined_value=42 freed_by=parent`;
- `lock_order=0_then_1 completed=yes`;
- if a trace was captured, the path the script prints, the operation
  names (`FUTEX_WAIT_PRIVATE`, `FUTEX_WAKE_PRIVATE`, anything else), and
  whether any wait returned `EAGAIN`.

Then:

```bash
objdump -d --disassemble=increment_mutex \
  ../../projects/linux-system-programming/build/thread_lab
```

Confirm the `incq` of `mutex_counter` sits between the lock call and the
unlock call. The futex lines, if any, are the slow path inside those
calls. A trace with zero futex lines on a quiet machine is still
compatible with 800000 critical sections that all took the fast path.

### Build

Write the invariant of `counter_lock` in one sentence, and the ownership
rule of `item` in one sentence. Then, from the trace or from the source
of `ordered`, list the lock ranks:

```text
rank 0    locks[0]     acquired first by every path that needs both
rank 1    locks[1]     acquired second, released first
```

State what would make a third lock legal to take while rank 1 is held
(a higher rank, on every path), and what day 6 will demonstrate by
taking them in opposite orders.

### Why? notebook

1. Why is "I locked around the variable" weaker than "this lock protects
   this invariant"?
2. Why does the ownership mode free `item` after join and use no mutex?
3. Why does unlocking mutex A fail to publish a write that the other
   thread observes by locking mutex B?
4. Why can an uncontended lock still be the happens-before edge?
5. Why does a syscall on every lock throw away the user/kernel boundary
   Week 8 measured?
6. Why does `futex_wait` re-read the user word after it has taken the
   hash-bucket lock?
7. Why can two unrelated user addresses share a bucket, and how does a
   wake still find the right waiters?
8. Why is a futex count of a few hundred, next to 800000 locks, evidence
   about the slow path and silent about each fast-path acquisition?

---

## References used selectively

- Linux tree `c9b2385d6793`, `kernel/futex.c`: the wait/wake comment at
  lines 47–122, `hash_futex`, `futex_wait`, `futex_wake`,
  `hb_waiters_pending`.
- [Day 2's missing edge](day-002-data-races-and-the-c-abstract-machine.md)
  and [Week 8's syscall boundary](../../08-user-mode-syscalls-and-ipc/sections/day-004-int80-syscall-abi.md).

**Next:** day 4 waits for a condition with the same lost-wakeup shape the
futex comment just drew, and it re-checks the predicate under the mutex
after every wake.
