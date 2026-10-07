# Day 4 — Condition Variables and Bounded Queues

**Target time:** approximately 3 hours

> A mutex can make a predicate check true of the protected state. It cannot,
> by itself, put a thread to sleep until that predicate becomes true. A
> condition variable exists to close that gap.

Run the day's experiment before the kernel excerpts, and keep the two layers
apart. The lab speaks POSIX through glibc. The excerpts later in this lesson
are Linux 5.10.134 at tree `c9b2385d6793`, and they show how the kernel waits.
glibc's `pthread_cond_t` is a userspace object. This lesson quotes kernel
code only.

## 1. Why exclusion is the wrong tool for waiting

[Day 3](day-003-mutexes-ownership-lock-order-futex.md) gave a mutex a job:
name an invariant, and hold the mutex across every read and write that must
appear atomic with respect to other threads. For the queue in today's lab the
invariant is small and strict. `queue.count` is the number of live slots in
a ring of eight, `head` names the oldest slot, and `tail` names the next free
slot. While `queue.lock` is held, those fields move together. A producer that
sees `count == 8` knows the ring is full *as of that critical section*. A
consumer that sees `count == 0` knows the ring is empty as of that same
section.

The next question is what the thread should do when the predicate it needs
is false.

Spinning while still holding the mutex deadlocks the protocol. The producer
is the only thread allowed to increment `count`, and it can increment `count`
only while it holds `queue.lock`. A consumer that keeps the lock and rereads
`count` has reserved the core's registers for a load that cannot change. The
cache line that holds `count` stays hot in that core, and the producer cannot
perform the store that would make the load return nonzero.

Dropping the mutex and spinning in userspace fixes the exclusion problem and
creates two new ones. The waiter burns a hardware thread: its loop occupies
an instruction pointer, a stack, and a share of the memory hierarchy, which
matters as soon as the machine has more waiters than idle cores. And there is
a gap between "I observed false" and "I am asleep." In that gap the producer
can fill the queue and issue a notification that nobody is waiting to hear.

Sleeping without a protocol is the same gap, moved into the kernel. A thread
that decides to block has to publish "I am a waiter" and release the mutex as
one transition. If those are separate steps, a wakeup can land after the
decision and before the publication. The waiter then enters the scheduler
with a predicate that is already true and a notification that has already
been consumed by an empty wait list.

That is why a condition variable is a different object from a mutex. The
mutex protects the predicate. The condition variable is the list of threads
that have committed to sleep until a later critical section asks them to look
again. POSIX packages the commit as `pthread_cond_wait`: the caller holds the
mutex; the call releases it and blocks; the call returns only after the mutex
has been reacquired.

```text
mutex answers:  "Is the protected state consistent while I look?"
condvar answers: "Wake me when someone else may have changed it."
predicate answers: "May I proceed on this look?"
```

The three answers stay separate. A signal is a hint that the predicate might
now hold. The bytes of `count` are the predicate. The mutex is what makes a
read of those bytes agree with the writer that last updated them.

## 2. Where the queue lives, in the machine

The lab queue is one object in the process address space. Both producers and
both consumers use the same virtual addresses for `items`, `head`, `tail`,
`count`, the mutex, and the two condition variables. Sharing the mapping is
what [Day 1](day-001-threads-processes-clone-stacks-tls.md) established:
`pthread_create` on Linux does not give the new thread a private heap. Virtual
memory still translates each thread's loads and stores through the same page
tables, so a store to `count` by the producer is a store to the physical page
the consumer will load.

Each thread keeps a private stack. `put` and `get` push a return address and
spill caller-saved registers on the calling thread's stack. When a consumer
blocks inside `pthread_cond_wait`, that frame remains mapped. The kernel saves
the userspace registers at the syscall boundary, switches to another task's
register state, and later restores this consumer so `pthread_cond_wait` can
return into the `while` that called it. Sleep is a scheduling event. It is
not an unmap, and it is not a destruction of the C automatic objects in the
waiter.

The C abstract machine sees a narrower fact. Every read and write of `count`,
`head`, `tail`, and `items` in the lab happens while `queue.lock` is held, and
the unlock that ends one critical section synchronizes with the lock that
begins the next. Those accesses are therefore ordered by happens-before.
[Day 2](day-002-data-races-and-the-c-abstract-machine.md) is why that sentence
matters: the same bytes, read and written with no such edge, would be a data
race and the program would have undefined behavior. The condition variable
does not relax that rule. It adds a place to sleep between critical sections.

On the uncontended fast path a pthread mutex is a userspace atomic on a word
in that shared object. On the slow path glibc asks the kernel to block the
task with a futex operation. Either way, the POSIX rule the lab relies on is
the same: `pthread_cond_wait` returns with the mutex owned by the caller, and
the predicate is examined again under that ownership.

## 3. The wait protocol

Write the protocol as four steps that share one mutex. The names below match
`put` and `get` in the lab.

1. Acquire the mutex that protects the predicate.
2. While the predicate is false, wait on the condition variable, passing that
   same mutex. The wait releases the mutex and sleeps as one operation, then
   reacquires the mutex before it returns.
3. With the mutex held and the predicate true, perform the state transition.
4. If the transition may have made some other predicate true, signal the
   condition variable that names it, then release the mutex.

Step 2 is a loop. The call returns holding the mutex, which means the waiter
may evaluate the predicate again in the same critical section that will do
the transition. An `if` around the wait evaluates the predicate once. The
`while` in the lab evaluates it every time the wait returns.

```text
lock(M)
while predicate is false:
    cond_wait(CV, M)          # unlock M, sleep, lock M, return
transition protected state
signal(the other CV)          # still holding M, in this lab
unlock(M)
```

The atomicity that matters is the pair "release M" and "become a published
waiter." POSIX assigns that pair to `pthread_cond_wait`. A program that
unlocks by hand and only then calls the wait has opened the gap itself.

Signaling while still holding the mutex, as `put` and `get` do, keeps the
state transition and the notification inside one critical section. The woken
thread cannot observe the new `count` until it acquires the mutex, and by
then the transition has already happened. Signaling after the unlock is a
different schedule with the same logical outcome when the waiter uses the
loop above: either the waiter has not yet called `cond_wait`, in which case
it will lock, see the updated predicate, and skip the wait, or it is already
inside `cond_wait`, in which case it is on the wait list and the signal finds
it. The lab takes the simpler side of that choice and signals before unlock.

A condition variable stores waiters. A semaphore stores permits.
`pthread_cond_signal` with
an empty wait list returns having woken nobody, and the next thread to wait
does not inherit that call. A semaphore can do the opposite: a `post` with no
waiter increments a count that a later `wait` consumes. Treating a condvar
as a semaphore is how notifications get lost. The queue does not need a
stored permit. `count` itself is the memory of how many items are present.

## 4. Execution trace: signal before anybody waits

This is the lost-wakeup trace. The mutex is free, `count` is 0, and the
condition variable's waiter list is empty. Thread S signals without holding
the mutex. Thread W waits afterwards. No second signal arrives.

```text
time  mutex owner   count   waiter list    what runs
----  ------------  ------  -------------  ---------------------------
 1    nobody        0       empty          S calls pthread_cond_signal
 2    nobody        0       empty          signal finds no waiter; returns
 3    W             0       empty          W locks and reads count == 0
 4    (inside wait) 0       W              W enters pthread_cond_wait,
                                           releases the mutex, sleeps
 5    nobody        0       W              S has already returned and
                                           does not signal again
```

Walk the machine, not only the API.

At time 1, S's call enters glibc. The userspace waiter list for this
condition variable is empty, so glibc has no task to hand to a futex wake.
The call returns to S. Nothing in the condvar retains the fact that a signal
occurred. S never wrote `count`. The predicate is still false, and the
notification has already vanished.

At time 3, W acquires the mutex. That acquisition is a userspace atomic, and
on this schedule it is uncontended, so it may complete without a futex
syscall. W's load of `count` happens while W owns the mutex. The load returns
0. The predicate W needs, `count > 0`, is false.

At time 4, W calls `pthread_cond_wait`. This is the moment the protocol would
have been safe had the signal still been in the future. W publishes itself on
the waiter list and releases the mutex, then enters the kernel to block. The
core that was running W saves W's userspace registers and the kernel picks
some other task. W's stack still holds the frame that will return into
`get`. The virtual page that holds `count` stays mapped.

At time 5 the scheduler has a sleeping task whose wake condition will be a
later `pthread_cond_signal` or an equivalent glibc wake. S has none left to
send. W stays off the run queue. The queue is empty and a consumer is asleep,
which is a consistent state, and it is also a stuck state: the consumer is
waiting for a notification that already happened to an empty list.

Two repairs are easy to confuse.

If S, still at time 1, had held the mutex, set `count` to 1, and signaled,
the signal would still find an empty list and would still be discarded. W's
later lock-and-check would then observe `count == 1` and would skip the wait.
The predicate, updated under the mutex, is what W observes. The early signal
is redundant once the predicate is true at the check. The bug in the trace is
the combination of a signal that matches no waiter and a predicate that is
still false when W finally waits.

If W had unlocked after seeing 0, S had then locked, set `count` to 1,
signaled, and unlocked, and W had later waited *without looking again*, the
same stuck state would appear for a different reason. The signal in the gap
matched no waiter, and W slept over a predicate that had become true. The
`while` loop closes that variant: after W reacquires the mutex it loads
`count`, sees 1, and takes the item. `pthread_cond_wait` closes the narrower
gap inside a single call, where the unlock and the publication of the waiter
are one operation. The loop closes every other reason the call might return
with the predicate still false, and it also closes the case where the
predicate became true and was consumed again before this thread ran.

Calling `pthread_cond_signal` without the mutex is legal POSIX when the
program still updates the predicate under the mutex and every waiter rechecks
in a loop. The trace above is the case that loses: the signal is early, the
predicate is not true at the eventual check, and nobody signals again.

## 5. Spurious wakeups and stolen wakeups

A wait can also return when a signal did happen, and the waiter must still
recheck.

POSIX allows `pthread_cond_wait` to return when no thread has signaled or
broadcast. That return is a spurious wakeup. The mutex is held again. The
predicate may still be false. An implementation can produce this from a
kernel wakeup that was not the logical event, from an internal race between
the userspace word and the futex queue, or from a path that must leave the
sleep for a reason the application did not name. Application code treats the
return as "look at the predicate," because that is the only fact the
application owns.

A stolen wakeup is a real signal whose resource is gone by the time this
thread looks. Consider two consumers and one item, which is the shape of the
lab whenever the ring holds a single slot and both consumers are in `get`.

```text
count == 0. C1 and C2 are inside cond_wait on not_empty.
P locks, writes one item, count = 1, signals not_empty, unlocks.
The signal wakes C1.
C2 is also runnable: it was between loop iterations, or an earlier
broadcast woke it, or it has just arrived at the lock.
C2 acquires queue.lock first, observes count == 1, takes the item,
sets count = 0, signals not_full, unlocks.
C1 acquires queue.lock, observes count == 0, and waits again.
```

C1 was woken for an item that C2 now owns. The signal was honest at the time
P issued it. Honesty at signal time is not a reservation. `pthread_cond_signal`
wakes at least one waiter; it does not transfer ownership of a queue slot, and
it does not hand the mutex to a chosen thread as a Hoare-style monitor would.
The lab's semantics are Mesa-style: the signaler continues, the waiter
reacquires, and the predicate is re-evaluated under the lock. The `while` is
the entire handling of both the spurious return and the stolen item.

`pthread_cond_broadcast` wakes every waiter, which makes the stolen-wakeup
schedule ordinary rather than exotic. The lab uses `pthread_cond_signal`
because each successful `put` creates one item and each successful `get`
creates one free slot. One new right-to-proceed is enough to wake one thread,
and the loop covers the case where that thread is not the one that consumes
the right. Broadcast would also be correct here. It would schedule more
threads back onto the mutex, most of which would observe a false predicate
and sleep again.

## 6. Empty and full are the two predicates

The lab ring has `CAPACITY` 8. Two producers each insert 1000 positive
values. Two consumers remove values until they see a sentinel. The fields
under `queue.lock` are:

```text
items[8]   the slots
head       index of the oldest item
tail       index of the next store
count      number of occupied slots, 0 .. 8

can_get    count > 0      waited on not_empty
can_put    count < 8      waited on not_full
```

`put` locks, waits while `count == 8`, writes `items[tail]`, advances `tail`
modulo 8, increments `count`, signals `not_empty`, and unlocks. `get` locks,
waits while `count == 0`, reads `items[head]`, advances `head`, decrements
`count`, signals `not_full`, and unlocks. Both transitions preserve
`0 <= count <= 8` and keep `head` and `tail` inside the ring. The occupied
range is the `count` slots beginning at `head`, wrapping at 8. That sentence
is the invariant the mutex protects.

A picture of a ring that holds three items, with two producers asleep because
the picture is about to fill, helps separate the pointers from the count:

```text
index:   0     1     2     3     4     5     6     7
       [ 41 ][ 42 ][ 43 ][    ][    ][    ][    ][    ]
         ^head             ^tail
         count == 3

after five more successful puts, count == 8, tail == head:
a put must wait on not_full
a get waits on not_empty only when count == 0
```

When `count` is 0, `head == tail` and the ring is empty. When `count` is 8,
`head == tail` and the ring is full. The pointers alone do not say which.
`count` is part of the predicate, which is why every wait expression in the
lab reads `count` and not a comparison of `head` and `tail`.

Producers 0 and 1 pass `long` ids 0 and 1. Producer `id` stores
`id * 1000 + i` for `i` from 1 to 1000, so the values are 1..1000 and
1001..2000. The parent checks that the consumers together removed
`n = 2000` values whose sum is `n * (n + 1) / 2 = 2001000`, and that `count`
is 0 at the end. The ring is FIFO for the queue as a whole: each `put`
appends and each `get` removes the oldest slot, so one producer's values stay
in order relative to each other, interleaved with the other producer's values
according to who won the mutex. The checked sum detects many dropped,
duplicated, and corrupted payloads. It does not record that interleaving, and
the wrapper says the wake order is unspecified.

Capacity 8 is small on purpose. Producers must sometimes observe `can_put`
false and enter `pthread_cond_wait` on `not_full`. Consumers must sometimes
observe an empty ring, including at startup if they run before any `put`. A
huge buffer could let all 2000 inserts finish before a consumer ran, and the
wait path would be unexercised even though the final sum would match.

The signals match the predicates they publish. A `put` that increments
`count` from 0 to 1 makes `can_get` true, so it signals `not_empty`. A `get`
that decrements `count` from 8 to 7 makes `can_put` true, so it signals
`not_full`. Signaling the other way would wake a thread whose predicate is
still false; the loop would send it back to sleep, and the thread that could
make progress would stay on the other wait list until some later signal.

## 7. Shutdown and who owns the result

After both producers have been joined, the parent calls `put(-1)` twice, once
per consumer. The joins matter. They are a happens-before edge from each
producer's last `put` to the parent's sentinels, so the sentinels enter the
ring after every positive value that producer was going to write. Consumers
treat `-1` as "stop," and they do not add it into the sum or the count.

Each consumer `calloc`s a `struct result`, fills `sum` and `count`, and
returns the pointer from its thread function. The parent `pthread_join`s that
pointer, folds it into the totals, and `free`s it. Ownership is the same
pattern as [Day 3](day-003-mutexes-ownership-lock-order-futex.md): the worker
allocates, the join publishes the pointer to the parent, and the parent is
the only thread that frees it. The queue mutex is not involved in that free.
By the time the parent reads the result, the consumer has returned and the
join has synchronized with that return.

The sentinel path can block. If the ring is full of positive values, `put(-1)`
waits on `not_full` until a consumer takes something. That is ordinary `can_put`.
It is also why the sentinels are inserted only after the producers have
finished: a sentinel occupying a slot cannot be overtaken by a later positive
`put` from a producer that is still running, because those producers have
already been joined.

## 8. What this kernel tree does when a task waits

glibc implements `pthread_cond_wait`, `pthread_cond_signal`, and the mutex
operations. On Linux the blocking path is a `futex` system call. The call
crosses the privilege boundary: userspace `RIP` enters the kernel through the
architecture's syscall instruction, the kernel sees a task that wants to
sleep on a userspace word, and `schedule` runs something else on that core.
The excerpts below are that kernel mechanism at tree `c9b2385d6793`. They are
not a line-by-line listing of glibc, and this lesson does not pretend to have
read the nptl sources.

The kernel's own "sleep until a C expression is true" macro is `wait_event`
in `include/linux/wait.h`. It checks the expression, and if the expression is
false it goes to `__wait_event`, whose body (the `___wait_event` macro just
above this excerpt) is a loop: prepare to wait, test the condition, and only
then call `schedule()`.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: include/linux/wait.h
 * lines: 318–336
 * function: wait_event
 */
/**
 * wait_event - sleep until a condition gets true
 * @wq_head: the waitqueue to wait on
 * @condition: a C expression for the event to wait for
 *
 * The process is put to sleep (TASK_UNINTERRUPTIBLE) until the
 * @condition evaluates to true. The @condition is checked each time
 * the waitqueue @wq_head is woken up.
 *
 * wake_up() has to be called after changing any variable that could
 * change the result of the wait condition.
 */
#define wait_event(wq_head, condition)						\
do {										\
	might_sleep();								\
	if (condition)								\
		break;								\
	__wait_event(wq_head, condition);					\
} while (0)
```

The comment states the same split as the lab. Something must change the
variables that the condition reads, and `wake_up` must be called after that
change. The macro rechecks. `might_sleep()` is a kernel annotation that this
path can block; it is not a userspace condvar.

The order between "I am about to sleep" and "I looked at the condition" is
the subject of the comment on `set_current_state` in `include/linux/sched.h`.
`prepare_to_wait_event` in `kernel/sched/wait.c` calls `set_current_state`
while holding the wait-queue lock, before the waiter returns to the loop and
evaluates the condition. The comment explains why that state store carries a
memory barrier, and why a colliding wakeup store is repaired by going around
the loop again.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: include/linux/sched.h
 * lines: 160–193
 * function: set_current_state
 * The comment continues briefly after this excerpt and closes at line 196.
 */
/*
 * set_current_state() includes a barrier so that the write of current->state
 * is correctly serialised wrt the caller's subsequent test of whether to
 * actually sleep:
 *
 *   for (;;) {
 *	set_current_state(TASK_UNINTERRUPTIBLE);
 *	if (CONDITION)
 *	   break;
 *
 *	schedule();
 *   }
 *   __set_current_state(TASK_RUNNING);
 *
 * If the caller does not need such serialisation (because, for instance, the
 * CONDITION test and condition change and wakeup are under the same lock) then
 * use __set_current_state().
 *
 * The above is typically ordered against the wakeup, which does:
 *
 *   CONDITION = 1;
 *   wake_up_state(p, TASK_UNINTERRUPTIBLE);
 *
 * where wake_up_state()/try_to_wake_up() executes a full memory barrier before
 * accessing p->state.
 *
 * Wakeup will do: if (@state & p->state) p->state = TASK_RUNNING, that is,
 * once it observes the TASK_UNINTERRUPTIBLE store the waking CPU can issue a
 * TASK_RUNNING store which can collide with __set_current_state(TASK_RUNNING).
 *
 * However, with slightly different timing the wakeup TASK_RUNNING store can
 * also collide with the TASK_UNINTERRUPTIBLE store. Losing that store is not
 * a problem either because that will result in one extra go around the loop
 * and our @cond test will save the day.
```

Read the parenthetical in the middle with the lab in mind. When the predicate
test, the predicate update, and the wakeup are all done under one lock, the
lock supplies the order and the kernel can use the barrier-free store. A
pthread mutex is that lock for `count`. The futex path still has to publish
the waiter and recheck the userspace word, because the kernel does not hold
`queue.lock` across the sleep. The comment's last sentences are the kernel's
version of "the loop saves you": a wakeup store and a sleep store can pass
each other, the task may run an extra iteration, and the condition expression
is what decides whether to sleep again. [Day 5](day-005-c11-atomics-and-memory-ordering.md)
names the barriers this comment is pointing at.

The futex syscall documents the same race at the userspace boundary.
`futex_wait_setup` in `kernel/futex.c` locks the hash bucket, then reads the
userspace word, and only then is the waiter eligible to be queued.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: kernel/futex.c
 * lines: 2635–2652
 * function: futex_wait_setup
 */
	/*
	 * Access the page AFTER the hash-bucket is locked.
	 * Order is important:
	 *
	 *   Userspace waiter: val = var; if (cond(val)) futex_wait(&var, val);
	 *   Userspace waker:  if (cond(var)) { var = new; futex_wake(&var); }
	 *
	 * The basic logical guarantee of a futex is that it blocks ONLY
	 * if cond(var) is known to be true at the time of blocking, for
	 * any cond.  If we locked the hash-bucket after testing *uaddr, that
	 * would open a race condition where we could block indefinitely with
	 * cond(var) false, which would violate the guarantee.
	 *
	 * On the other hand, we insert q and release the hash-bucket only
	 * after testing *uaddr.  This guarantees that futex_wait() will NOT
	 * absorb a wakeup if *uaddr does not match the desired values
	 * while the syscall executes.
	 */
```

That comment is the lost-wakeup trace translated into a hash-bucket lock. If
the kernel tested `*uaddr` and only afterwards locked the bucket, a waker
could change the word and issue `futex_wake` in between. The wake would find
no queued waiter. The waiter would then queue and block on a word that no
longer matches. Locking first, testing second, and queueing only if the value
still matches is the kernel closing the gap that `pthread_cond_wait` closes
for the mutex. The userspace sketches in the comment are the contract a
library uses. They are still kernel commentary, not glibc source.

After `futex_wait_queue_me` returns, `futex_wait` may observe a wakeup that
was not the event the caller asked for. The function retries.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: kernel/futex.c
 * lines: 2708–2725
 * function: futex_wait
 */
	/* queue_me and wait for wakeup, timeout, or a signal. */
	futex_wait_queue_me(hb, &q, to);

	/* If we were woken (and unqueued), we succeeded, whatever. */
	ret = 0;
	/* unqueue_me() drops q.key ref */
	if (!unqueue_me(&q))
		goto out;
	ret = -ETIMEDOUT;
	if (to && !to->task)
		goto out;

	/*
	 * We expect signal_pending(current), but we might be the
	 * victim of a spurious wakeup as well.
	 */
	if (!signal_pending(current))
		goto retry;
```

The comment names a spurious wakeup in the kernel's own wait path. The retry
goes back through `futex_wait_setup`, which reads the userspace word again.
That is the same shape as the lab's `while`: leave the sleep, look at the
predicate under the protocol's lock, and sleep again if it is still false.
glibc has to do an analogous recheck of the pthread predicate. The lines
above are the kernel doing it for a futex word.

One more boundary is worth keeping visible. An uncontended `put` on a
non-full queue may perform the mutex lock and unlock entirely in userspace
and never enter `futex`. A trace that shows no `futex` lines has shown that
those particular calls took the fast path. A trace that shows `futex` lines
has shown that those calls blocked or woke. [Day 3](day-003-mutexes-ownership-lock-order-futex.md)
already treats syscall traces as evidence of the slow path, with a scope
limited to the calls that happened.

## 9. Mastery: Explain · Draw · Observe · Build

The exercise is
[challenges/day-004-condition-queue.sh](../challenges/day-004-condition-queue.sh).
It builds the canonical lab and runs `queue_lab`, then prints the `put`/`get`
transitions and the shutdown path. Predict 2000 items, sum 2001000, a final
`count` of 0, and the marker `condition_waits=while_predicate_loops`. Leave
wake order, the number of waits, and whether any return was spurious as
unspecified.

### Explain

- Why a consumer that holds `queue.lock` and spins on `count == 0` prevents
  the store that would make the predicate true.
- What `pthread_cond_wait` does with the mutex on entry and on return, and
  why the publication of the waiter has to be in the same step as the unlock.
- Why the lost-wakeup trace in section 4 stays asleep, and which fact would
  have to change (a later signal, or a predicate that is already true at the
  check) for the consumer to proceed.
- Why a stolen wakeup still has a correct signal in its past, and why the
  `while` is what makes that schedule safe.
- Which predicate `not_empty` serves, which predicate `not_full` serves, and
  why `head == tail` is ambiguous unless `count` is known.

### Draw

Draw the ring of eight with `head`, `tail`, and `count` for empty, full, and
three occupied slots. Under it, draw two wait lists, `not_full` and
`not_empty`, and mark which predicate each list is sleeping on. Then draw the
five-step lost-wakeup trace with columns for mutex owner, `count`, and waiter
list.

### Observe

Run the wrapper from the week directory. Record the checked line, the
compiler and kernel identity the rest of the week is using, and the source
the wrapper prints for the wait, the transition, the signal, the sentinels,
the joins, and the frees. In your notes, point at the `while` and write the
predicate it reloads.

### Build

Using the printed lab source as the specimen, write a one-page account of a
single item's life: which thread stores it, which mutex edge publishes that
store, which signal is a hint, which thread's `while` observes `count > 0`,
and which `free` runs only after `pthread_join`. Include the sentence the
wrapper prints as its limit: wake order, forced spurious wakeups,
cancellation, producer-failure recovery, and a schedule proof are outside
what this run establishes.
