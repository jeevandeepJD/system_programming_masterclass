# Day 6 — Liveness, False Sharing, and Scalability

**Target time:** approximately 3 hours

> A protocol can keep its invariant on every step and still fail to finish.
> Deadlock is a cycle of threads each waiting for a lock another thread in
> the cycle holds. False sharing is a different failure of progress: the
> invariant holds, the atomics are race-free, and the cores still spend the
> interval moving one cache line back and forth.

Today's wrapper runs both phenomena and keeps their evidence different.
[challenges/day-006-liveness-scalability.sh](../challenges/day-006-liveness-scalability.sh)
expects the timed cycle detector to exit and the globally ordered mode to
finish. It prints three timing samples of the adjacent and padded counters
and requires no winner. A measurement from those samples describes one
machine, one compiler, and one layout.

## 1. Syntax needed for this source

Two members written next to each other in a `struct` occupy adjacent
offsets of one object. That is a layout fact about the abstract machine's
object, not a statement about cache lines. A loop that waits until some
other task changes a variable is ordinary control flow.

```c
struct pair {
    int left;
    int right;          /* next offset after left, plus padding the ABI inserts */
};

void wait_until(int *ready)
{
    while (*ready == 0)
        ;               /* this loop does not by itself make *ready change */
}
```

The adjacency becomes a coherence question only when two cores write those
two objects and a cache treats the line as one unit. That hardware is not
part of the C syntax.

## 2. Linux 0.12 first: a task that is not runnable is not switched to

`schedule` selects only a task whose `state` is `TASK_RUNNING` and whose
remaining slice is the best so far (`kernel/sched.c` lines 152–153, printed
p. 358):

```c
if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
        c = (*p)->counter, next = i;
```

A task left in `TASK_UNINTERRUPTIBLE` or `TASK_INTERRUPTIBLE` is skipped.
`wake_up` is what stores `0` and makes it selectable again. Trace a cycle
on that rule: task A is asleep until B calls `wake_up` on A's queue, and
B is asleep until A calls `wake_up` on B's queue. Neither store to `state`
ever runs. `schedule` keeps choosing some other runnable task, or idle
task 0, which the file comments say must not itself sleep. The wait-cycle
reasoning carries forward: progress needs a participant that is still
runnable and that performs the wake. Day 3's global lock order is the
modern way this week breaks that cycle. The early kernel has no mutex
graph to draw.

False sharing does not carry. Linux 0.12 is a uniprocessor kernel. One core
does not bounce a line through a coherence protocol against a second core,
and this tree has no cache-line alignment macro for SMP.

The shared
[Linux 0.12 reading map](../../references/EARLY_LINUX_012_MAP.md)
(Week 11) stops this kernel short of SMP. The following sections stay with
the wait-for cycle in the lab, then with false sharing on one coherence line.

## 3. Evolution: why the early design changed

The wait-cycle reasoning survives the later kernel. A task that is not
runnable is not selected, and two tasks that each wait for the other's wake
make no progress. A mutex deadlock is that cycle drawn on lock ownership.
Day 3's global lock order is one way to refuse the cycle. A second CPU does
not remove it.

SMP adds a different failure of progress. Two cores can write different
objects that share one cache line, and the coherence protocol bounces that
line. False sharing is that bounce. The kernel separates hot data with
`____cacheline_aligned_in_smp` (`include/linux/cache.h` lines 70–76, commit
`adc218676eef25575469234709c2d87185ca223a`): under `CONFIG_SMP` the object
is aligned to `SMP_CACHE_BYTES`, and on a uniprocessor build the macro is
empty. A per-CPU object goes further. `DEFINE_PER_CPU_SHARED_ALIGNED`
(`include/linux/percpu-defs.h` lines 142–144) gives each CPU its own copy
and applies that alignment so one CPU's data does not share a line with
another's. The comment at lines 127–137 says why.

ToyOS matches the 0.12 machine on this point: Week 7's two tasks share one
core, and its `schedule` walks a two-slot array. That is not an SMP
scalability result. [Day
7](day-007-debuggers-sanitizers-and-system-comparison.md) keeps ToyOS on
that single-core reading.

## 4. Safety and liveness answer different questions

Safety for the mutex counter on [Day 3](day-003-mutexes-ownership-lock-order-futex.md)
is a statement about states: every time a thread reads or writes
`mutex_counter`, it holds `counter_lock`, so the counter moves by one
published increment at a time. Liveness is a statement about futures: a
thread that needs the lock eventually acquires it, does the increment, and
releases it. The ordered two-lock mode in today's lab is a small liveness
policy. The detector mode is a small demonstration that the opposite policy
waits in a cycle, with a timeout so the process still terminates.

The C abstract machine will not detect either outcome for you. A deadlock
of well-locked mutexes is defined behavior that makes no progress. A
data-race-free atomic program that false-shares is defined behavior that
makes progress slowly. [Day 2](day-002-data-races-and-the-c-abstract-machine.md)
removes programs from the model when non-atomic accesses conflict. It does
not rank the defined programs by how long they take, and it does not require
a scheduler to be fair.

Hardware sits under both. A thread blocked in a futex wait owns no core:
its userspace registers were saved at the syscall boundary, its stack stays
mapped in the shared address space, and `schedule` runs somebody else. A
thread spinning on an atomic in userspace owns a core and a cache line. The
deadlock lab uses the first shape, bounded by a timeout. The false-sharing
lab uses the second. Mixing them up produces the wrong repair: adding a
timeout does not separate two counters onto two lines, and padding a struct
does not break a lock cycle.

## 5. Deadlock is a wait-for cycle

Draw the waits. The order of the source lines only suggests which edges can appear.

```text
Thread A holds lock 0 and waits for lock 1
Thread B holds lock 1 and waits for lock 0

        owns              waits for
   A ---------- lock 0
   |                        |
   |                        v
   +---- waits for ---- lock 1 ---------- B
```

Each arrow is a thread that will not release what it holds until it acquires
what it waits for. The graph has a cycle. No thread in the cycle can run the
unlock that the next thread needs. That is deadlock.

Four ingredients show up together in this kind of cycle, and naming them
makes the repair obvious:

1. The locks are exclusive. One owner at a time.
2. A thread holds one lock while it waits for another. That is hold-and-wait.
3. Nothing steals a lock from an owner. The owner runs the unlock, or the
   wait gives up.
4. The waits form a cycle.

The lab's ordered mode leaves 1–3 in place and removes 4. Every path that
takes both locks takes `locks[0]` before `locks[1]`. A thread that holds
`locks[1]` does not go on to wait for `locks[0]`, because it already acquired
`locks[0]` earlier in the same path and will release `locks[1]` first. The
wait-for graph cannot contain the edge "holds 1, waits for 0."

The detector mode builds the cycle on purpose and then refuses to wait
forever. `pthread_mutex_timedlock` returns `ETIMEDOUT` when the deadline
passes. The thread unlocks the mutex it still holds. The process joins both
threads and exits. `test_hung=no` is the claim the program is willing to
print: this run ended. It is not a claim that a non-timed version of the
same lock order would have ended.

## 6. Execution trace: the timed cycle

`deadlock_lab detect` creates two threads with `struct attempt` values
`{first: 0}` and `{first: 1}`. Each thread locks `locks[first]`, meets the
other at a rendezvous, then timed-locks `locks[1 - first]` with a deadline
100 milliseconds after `CLOCK_REALTIME`. The rendezvous is a third mutex,
`gate_lock`, and a condition variable. Its only job is to make both "first"
acquisitions visible before either "second" attempt. Without it, one thread
might acquire both resource locks before the other thread starts, and the
run would show no cycle.

Start with `locks[0]` and `locks[1]` free, `gate_count` 0, both threads
runnable.

```text
step  who   holds                         waits                gate_count
----  ----  ----------------------------  -------------------  ----------
 1    T0    locks[0]                      —                    0
 2    T1    locks[1]                      —                    0
 3    T0    locks[0], then gate_lock      —                    1
 4    T0    locks[0]                      gate_cond            1
      (cond_wait released gate_lock)
 5    T1    locks[1], gate_lock           —                    2
      T1 broadcasts, unlocks gate_lock
 6    T0    locks[0], reacquires gate     —                    2
      T0 sees gate_count == 2, unlocks gate_lock
 7    T1    locks[1]                      —                    2
      T1 leaves the rendezvous
 8    T0    locks[0]                      locks[1] (timed)     2
 9    T1    locks[1]                      locks[0] (timed)     2
10    one or both timedlocks return ETIMEDOUT
11    each thread unlocks the lock it still holds
12    parent joins both and prints test_hung=no
```

Steps 1 and 2 are the hold. Steps 8 and 9 are the cycle. Between them the
gate does something that looks like a wait and is not the deadlock. At step
4, T0 holds `locks[0]` and waits on `gate_cond`. That wait is a [Day 4](day-004-condition-variables-and-bounded-queues.md)
wait: `pthread_cond_wait` releases `gate_lock`, so T1 can acquire it, raise
`gate_count` to 2, and broadcast. The predicate `gate_count == 2` becomes
true, both threads recheck it under `gate_lock`, and both release `gate_lock`
before they touch the second resource lock. The gate's job is to publish
"both first locks are held." Once that predicate is true, the gate is no
longer in the wait-for graph.

At step 8, T0's `pthread_mutex_timedlock` on `locks[1]` finds T1's ownership
and blocks. glibc's usual implementation waits through a futex, the same
kernel sleep sketched on Day 4, with a timeout attached. T0's userspace
registers are saved. The core is free to run T1. At step 9, T1 tries
`locks[0]`, which T0 still owns, and blocks the same way. Now neither thread
is running the unlock the other needs. The cycle is real, and it is inside
the kernel's wait queues rather than in a userspace spin.

The deadline each thread computed is `CLOCK_REALTIME` plus
100000000 nanoseconds, with the nanosecond field normalized if it overflows.
When that time is reached, the futex wait returns and `pthread_mutex_timedlock`
returns `ETIMEDOUT`. The lab treats success as "at least one of the two
results is `ETIMEDOUT`." Both may time out. The printed integers are those
pthread return codes. After the return, a thread that happened to acquire
the second lock unlocks it, and every thread unlocks its first lock. The
parent joins. The process is gone. `deadlock_cycle_detected=yes` together
with `test_hung=no` means the cycle was observed through a bounded failure
and the cleanup ran.

What the trace does not include is a rollback. The lab's critical sections
contain no payload, so there is nothing to undo. A timed lock in a program
that had already published a field under the first mutex would return with
that field still published. The timeout ends the wait. Repair of partial
work is a separate protocol, and this source does not have one.

## 7. One global order removes the cycle

`deadlock_lab ordered` starts two threads that both run the same sequence:
lock `locks[0]`, lock `locks[1]`, unlock `locks[1]`, unlock `locks[0]`. There
is no rendezvous and no timed call. One thread gets `locks[0]` first. The
other blocks on `locks[0]` holding nothing. The owner takes `locks[1]`,
releases it, releases `locks[0]`, and the waiter then repeats the same
order. The wait-for graph is a single edge from the waiter to `locks[0]`,
and it disappears when the owner unlocks.

```text
wrong for a second path:          right for every path:

  sometimes 0 then 1                always 0 then 1
  sometimes 1 then 0
        \                          lock 0 -----> lock 1 -----> unlock 1
         \                                            |
          +-- cycle appears               unlock 0 --+
              when the two orders meet
```

The rule has to be global to be a rule. "Usually 0 before 1" leaves one
error path, one callback, or one logging helper that takes them backward,
and that path is enough to rebuild the cycle. The lab is small enough that
both call sites are visible in `mode_ordered` and `opposite_order`. A larger
program writes the order down and holds to it on the failure returns as well
as on the happy path. [Day 3](day-003-mutexes-ownership-lock-order-futex.md)
stated that obligation; today the detector shows the cycle the obligation
exists to prevent.

Try-lock and timed-lock are escape hatches with their own postcondition. The
detector uses a timeout as a sensor. A program that uses `timedlock` as a
protocol has to say what it does with `ETIMEDOUT`: retry the whole ordered
sequence from the beginning, drop the work, or take a coarser lock. Returning
into the caller while still holding `locks[0]` and pretending the second
update happened would break the invariant the two locks were protecting.
The detector unlocks and exits, which matches a sensor and would be incomplete
as a transaction.

## 8. Starvation is a different graph

Starvation is a thread that stays runnable, or stays blocked on a lock that
other threads keep acquiring, for as long as you care to watch, while the
system as a whole continues. There is no cycle. There is a schedule that
never picks the unlucky thread.

POSIX mutexes do not promise FIFO ownership. A scheduler does not promise
that a runnable thread runs within a bound this lab could check. The ordered
mode happens to give each of two threads one turn and then joins them, so a
passing run has no room to display starvation. A passing run is compatible
with a mutex implementation that, under a different loop, would hand
`locks[0]` to the same thread every time.

The detector's timeout is also the wrong instrument for starvation. A
timeout fires because time passed, including when the lock was busy for a
good reason and the holder was making progress. `ETIMEDOUT` in the detector
is informative because the rendezvous has already established that the other
thread holds the contested lock and is itself waiting. The cycle is the
explanation. The clock only bounded the wait. On a loaded machine the same clock can expire for
a lock whose owner is merely slow. The lab does not try to tell those apart
with a second experiment; it forces the cycle first.

Fairness, priority inheritance, and robust mutexes after a dead owner are
real pthread features and they are outside this wrapper. The wrapper's limit
line says so: no fairness proof, no starvation proof.

## 9. False sharing starts from the cache line

Week 2's [cache organization](../../02-assembly-memory-and-c/sections/day-005-cache-organization.md)
fixed the unit the memory hierarchy actually moves. A core that needs one
address fills a whole line, commonly 64 bytes on current x86-64 parts, with
the neighboring bytes that share that line's base. The line is the unit of
coherence as well as the unit of fill. Two cores may each hold a copy.
A write needs a writable exclusive copy, and the other copies are invalidated
or downgraded. Week 2's name for the resulting traffic, when the two cores
are writing *different* objects that happen to sit on one line, is false
sharing.

The lab builds that situation with atomics so the C program stays defined.

```text
struct adjacent {
    atomic_uint_fast64_t value[2];   /* two objects, one likely line */
};

struct padded_counter {
    _Alignas(64) atomic_uint_fast64_t value;
    unsigned char padding[64];
};
```

`mode_false_sharing` runs two threads. Each thread calls
`atomic_fetch_add_explicit` 200000 times on its own counter, with
`memory_order_relaxed`. One run uses `&near.value[0]` and `&near.value[1]`.
The next uses `&far[0].value` and `&far[1].value`. [Day 5](day-005-c11-atomics-and-memory-ordering.md)
already said why the final values would be exact if the lab checked them:
each counter has its own modification order. The lab does not check them.
It prints wall-clock seconds and the caveat
`timings_are_noisy_and_not_a_correctness_test`.

The addresses are virtual. The cache, on the machines Week 2 described,
tags physical lines; L1 is often virtually indexed and physically tagged,
and the exact split is microarchitectural. Inside one page, a normal
anonymous mapping is contiguous in physical memory, so a virtual gap of one
or more line sizes is a physical gap of the same size. `_Alignas(64)` asks
the compiler to place `value` on a 64-byte boundary. The trailing
`padding[64]` plus the struct's alignment make `sizeof(struct padded_counter)`
a multiple of 64, so `far[0]` and `far[1]` land in different 64-byte blocks.
`near.value[0]` and `near.value[1]` are adjacent `atomic_uint_fast64_t`
objects, typically 8 bytes each, and those 16 bytes share a 64-byte line
whenever the array itself is not split across a line boundary by a perverse
placement. The wrapper never prints the addresses, the sizes, or
`getconf LEVEL1_DCACHE_LINESIZE`. A timing gap is consistent with that layout
on a machine whose line size is 64. It is not a measurement of the line size,
and a line size of 128 would change which of these gaps is enough.

```text
one 64-byte line, adjacent case

| value[0] (thread 0 writes) | value[1] (thread 1 writes) | 48 bytes unused |
+----------------------------+----------------------------+-----------------+
  both fetch-adds are RMWs of this same coherence unit

padded case

| far[0].value | pad ........ | far[1].value | pad ........ |
+--------------+--------------+--------------+--------------+
  line A, thread 0 only        line B, thread 1 only
```

The unused bytes in the adjacent line are the expensive part. The hardware
has no field in the line that says "thread 0 promised to touch only the
first eight bytes." Coherence grants and revokes the line.

## 10. The MESI picture Week 2 already implied

Week 2's lesson did not need the four state names to make the traffic real.
The names are the compact picture of what it did say: a writable copy is
exclusive, other copies lose write permission, and a later reader has to
fetch the line again. MESI is that picture.

```text
state        this cache's copy                         a write from here
-----------  ----------------------------------------  -------------------------
Modified     only copy, dirty relative to memory      already exclusive; write hits
Exclusive    only copy, clean                          write hits, state becomes Modified
Shared       other caches may also hold a clean copy  must invalidate the others first
Invalid      no copy                                   must obtain the line first
```

A false-sharing bounce between two cores, each doing a relaxed fetch-add to
its own half of one line, is a walk around those states:

```text
Core 0                         Core 1                      the line
------                         ------                      --------
fetch-add value[0]
  line arrives Exclusive
  RMW writes, line is Modified
                               fetch-add value[1]
                                 needs the line writable
                                 Core 0's copy → Invalid
                                 Core 1's copy → Modified
fetch-add value[0] again
  Core 1's copy → Invalid
  Core 0's copy → Modified
```

Each step's arithmetic is a handful of cycles once the operand is in a
register. The miss that fills the register waits on the interconnect, on
the other core's cache, and possibly on a snoop filter or a last-level
cache. The C abstract machine does not contain those cycles. The ISA
guarantees that the locked or atomic read-modify-write observes a single
modification order for that object. How long the line took to arrive is a
property of this die, this topology, and this placement.

Real cores extend MESI. Intel parts are often described with a Forward state
(MESIF). AMD parts are often described with an Owned state (MOESI). A
directory or a snoop filter can answer "who has the line?" without a bus
broadcast. None of that changes the lab's qualitative fact: the coherence
granule is the line, so two writers on one line invalidate each other, and
two writers on two lines do not. Naming MESI here is naming Week 2's
exclusive-ownership rule. It is not a claim that the CPU under this
particular run implements exactly four states and no directory.

The fetch-add also touches the core's own machinery. The atomic value has
to be in a register for the add. On x86-64 a `lock`-prefixed add holds the
line in a writable state for the instruction and is a full barrier as a side
effect of the `lock` prefix, which [Day 5](day-005-c11-atomics-and-memory-ordering.md)
already separated from the C order `memory_order_relaxed`. The false-sharing
cost is paid on top of that atomicity cost. A single thread doing both
counters would pay the atomicity cost and would not bounce the line between
cores. The lab always uses two threads, so it does not isolate those two
costs. The three samples remain a comparison of two layouts under one
harness, not a decomposition into "cycles of `lock add`" versus "cycles of
snoop."

## 11. How this kernel asks for a private line

The kernel fights the same placement problem with alignment attributes.
Commit `adc218676eef25575469234709c2d87185ca223a`, `include/linux/cache.h`, states the intent for read-mostly
data and then defines the SMP alignment macros. `____cacheline_aligned_in_smp`
is the one to remember: under `CONFIG_SMP` it is an alignment to
`SMP_CACHE_BYTES`; on a uniprocessor build the macro is empty, because there
is no second cache to bounce a line with.

```c
/* include/linux/cache.h :: __read_mostly, ____cacheline_aligned_in_smp :: adc218676eef25575469234709c2d87185ca223a
 * lines 42–76
 */
/*
 * __read_mostly is used to keep rarely changing variables out of frequently
 * updated cachelines. Its use should be reserved for data that is used
 * frequently in hot paths. Performance traces can help decide when to use
 * this. You want __read_mostly data to be tightly packed, so that in the
 * best case multiple frequently read variables for a hot path will be next
 * to each other in order to reduce the number of cachelines needed to
 * execute a critical path. We should be mindful and selective of its use.
 * ie: if you're going to use it please supply a *good* justification in your
 * commit log
 */
#ifndef __read_mostly
#define __read_mostly
#endif

/*
 * __ro_after_init is used to mark things that are read-only after init (i.e.
 * after mark_rodata_ro() has been called). These are effectively read-only,
 * but may get written to during init, so can't live in .rodata (via "const").
 */
#ifndef __ro_after_init
#define __ro_after_init __section(".data..ro_after_init")
#endif

#ifndef ____cacheline_aligned
#define ____cacheline_aligned __attribute__((__aligned__(SMP_CACHE_BYTES)))
#endif

#ifndef ____cacheline_aligned_in_smp
#ifdef CONFIG_SMP
#define ____cacheline_aligned_in_smp ____cacheline_aligned
#else
#define ____cacheline_aligned_in_smp
#endif /* CONFIG_SMP */
#endif
```

`SMP_CACHE_BYTES` defaults in the same header (lines 12–14) to
`L1_CACHE_BYTES` when an architecture has not overridden it. That is a compile-time constant of the
kernel build, not a value the userspace lab queries. The comment on
`__read_mostly` is the dual of false sharing: pack variables that are read
together onto as few lines as possible, and keep them off the lines that
writers hammer. Alignment to a cache line does the other job, giving a hot
counter a line of its own. The comment also says to justify the attribute.
Padding is not free. A line reserved for one counter is a line the rest of
the working set cannot use, and a structure that used to fit in one cache
fill now takes two.

The lab's `_Alignas(64)` is a userspace request of the same shape, with the
line size written as a literal. It is not the kernel macro, and it is not
consulted from `SMP_CACHE_BYTES`. If you compare them, compare the idea —
put independently written objects on different coherence granules — and keep
the two source texts in their own projects.

Per-CPU data is the other placement tool in this tree. The comment in
`include/linux/percpu-defs.h` says a CPU's own copy should not share a line
with another CPU's copy, and the macro applies `____cacheline_aligned_in_smp`:

```c
/* include/linux/percpu-defs.h :: DEFINE_PER_CPU_SHARED_ALIGNED :: adc218676eef25575469234709c2d87185ca223a
 * lines 127–144
 */
/*
 * Declaration/definition used for per-CPU variables that must be cacheline
 * aligned under SMP conditions so that, whilst a particular instance of the
 * data corresponds to a particular CPU, inefficiencies due to direct access by
 * other CPUs are reduced by preventing the data from unnecessarily spanning
 * cachelines.
 *
 * An example of this would be statistical data, where each CPU's set of data
 * is updated by that CPU alone, but the data from across all CPUs is collated
 * by a CPU processing a read from a proc file.
 */
#define DECLARE_PER_CPU_SHARED_ALIGNED(type, name)			\
	DECLARE_PER_CPU_SECTION(type, name, PER_CPU_SHARED_ALIGNED_SECTION) \
	____cacheline_aligned_in_smp

#define DEFINE_PER_CPU_SHARED_ALIGNED(type, name)			\
	DEFINE_PER_CPU_SECTION(type, name, PER_CPU_SHARED_ALIGNED_SECTION) \
	____cacheline_aligned_in_smp
```

## 12. Adding cores helps independent work

Scalability is the shape of the work, drawn against the number of cores that
can run it.

```text
independent counters, private lines
  core 0 writes line A
  core 1 writes line B
  core 2 writes line C
  each RMW completes from the local cache after the first fill
  more cores, more completed increments, until bandwidth or the
  serial fraction of the program runs out

one shared line
  every writer invalidates every other writer's copy
  the line occupies the interconnect in proportion to the writes
  another core is another contender for the same granule
```

Amdahl's observation fits the first shape when you can name the serial
fraction. If a portion `s` of the runtime must happen on one core, and the
rest is parallel, then even an infinite number of cores leaves you with that
portion `s`. The shared-line program is harsher than a plain serial fraction:
the contended writes actively slow down as contenders are added, because each
write waits for exclusive ownership of a line that the new cores are also
taking. Adding cores helps the private-line work. Adding cores increases the
coherence traffic of the shared-line work.

The lab never varies the core count. It runs two threads, twice, and times
each pair with `CLOCK_MONOTONIC` around `pthread_create`, the increments, and
`pthread_join`. Thread creation and join are inside the number you read.
The scheduler may place both threads on one core, on two cores, or migrate
them mid-run. The wrapper asks `getconf _NPROCESSORS_ONLN` and `uname` so
the report has an identity, and it does not pin threads with `taskset`.
Three samples are what the script prints. They will not agree with each
other down to the microsecond, and the script does not require
`padded_seconds` to be smaller than `adjacent_seconds`.

A microbenchmark measures one machine, one compiler, one layout, one
iteration count, and one timer placement. A result that matches the MESI
story on this host is support for the story here. A result that does not
is a fact about this run: one core, a noisy timer, a line size other than
64, a hypervisor that multiplexes the two threads onto one physical core,
or a `lock`-prefixed instruction whose local cost dominates the bounce.
Record the numbers and the identity. Keep the correctness claim where it
already is, in the atomic modification order and the deadlock markers, not
in which sample was faster.

Virtual memory and the scheduler belong in that record. Migration moves a
thread onto a core whose private cache does not hold the line, so a
correctly padded counter still misses after a migration. A page that the
kernel places far away in a NUMA machine pays a remote latency even when the
line is uncontended. Week 2's hierarchy — TLB, private caches, shared last
level, memory controller — is the path each fetch-add walks. Today's lab
times the whole path plus thread lifetime, and it stops there.

## 13. Mastery: Explain · Draw · Observe · Build

### Explain

- Draw, in words, the wait-for cycle at step 8 of the trace, and say why the
  gate's condition wait is a finished rendezvous by then.
- Why `pthread_mutex_timedlock` lets the process print `test_hung=no`, and
  what state a program would still have to repair if the first lock had
  protected a real update.
- Why the ordered mode's graph has no cycle as long as every path takes
  index 0 before index 1.
- Why two relaxed atomics on one line produce coherence traffic, and why
  that traffic is not a C data race.
- What `____cacheline_aligned_in_smp` expands to in an SMP build of this
  tree, and why the userspace lab's `_Alignas(64)` is a related idea with a
  hard-coded size.

### Draw

Draw the two-lock cycle with the gate already released. Draw a 64-byte line
containing `value[0]` and `value[1]`, then the same bytes after padding, and
mark MESI state on each line for three successive fetch-adds. Use Week 2's
vocabulary for anything you are unsure the four letters capture: exclusive
ownership, invalidation, line rather than object.

### Observe

Run
[challenges/day-006-liveness-scalability.sh](../challenges/day-006-liveness-scalability.sh).
Keep `uname`, the online processor count, both deadlock markers, the three
timing samples, and the source the wrapper prints for the cycle and the
ordered path. Write one sentence on whether the three padded samples agree
with each other. That sentence is part of the evidence.

### Build

Write two short accounts next to the wrapper output. The first is the
wait-for graph of `detect` at the moment both timed locks are in flight, and
the graph of `ordered` when one thread holds both locks. The second is a
statement of what the three samples are allowed to decide: one host, one
compiler, one layout, timers that include create and join, no affinity, no
hardware counters, no required winner. End with the scale picture from
section 12: private lines gain from more cores; a shared written line spends
those cores on coherence.
