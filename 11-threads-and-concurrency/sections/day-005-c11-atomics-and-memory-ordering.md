# Day 5 — C11 Atomics and Memory Ordering

**Target time:** approximately 3 hours

> Atomicity says a single object moves from one value to the next with no
> torn intermediate. Ordering says which *other* accesses in this thread
> become visible to a thread that observes that value. The C memory orders
> exist because those are different promises, and because the compiler and
> the CPU can each rearrange work the source text wrote in one sequence.

The lab's relaxed counter and the lab's relaxed litmus use the same header,
`<stdatomic.h>`, and they establish different facts.
[challenges/day-005-atomics-memory-order.sh](../challenges/day-005-atomics-memory-order.sh)
checks that the counter equals 800000, runs the litmus, and writes one
compiler's assembly for this host. Kernel barrier headers from commit
`adc218676eef25575469234709c2d87185ca223a` (Linux 6.12) show how *that kernel* lowers its own
release, acquire, and full barriers on x86, AArch64, and RISC-V. They are
evidence about the kernel. They are not a dump of what `memory_order_release`
compiles to in the lab.

## 1. Syntax needed for this source

A C11 atomic expression names an atomic object, an operation, and a memory
order. The order is an argument of that expression. A plain store, and an
inline-assembly statement, are different syntax and different promises.

```c
#include <stdatomic.h>

atomic_int flag;

void publish(void)
{
    atomic_store_explicit(&flag, 1, memory_order_release);
}
```

`memory_order_release` is not implied by the semicolon, by `volatile`, or
by a compiler barrier you did not write. The sections below say what this
particular order promises, and what the 6.12 headers emit for their own
barriers.

## 2. Linux 0.12 first: cli on one uniprocessor

The early kernel excludes the timer, and every other interrupt, by clearing
the processor interrupt flag. `include/asm/system.h` line 17 (Program
14-17) is the whole macro:

```c
#define cli() __asm__ ("cli"::)     /* disable interrupt */
#define sti() __asm__ ("sti"::)     /* enable interrupt */
```

`kernel/sched.c` uses that pair around the floppy-motor update (line 233
`cli()`, later `sti()`). While `cli` is in effect on this CPU, `do_timer`
cannot enter, so `schedule` cannot switch tasks out of the interrupted
kernel path. Trace it as a historical uniprocessor critical section: one
core, the interrupt flag, then the ordinary stores that follow.

What carries forward is only the motive for a critical section.

Linux 0.12 cannot teach portable C11 atomics or the ordering of a weakly
ordered SMP machine. There is no second CPU whose loads could pass this
core's stores, and there is no `<stdatomic.h>`. The C memory model remains
the authority for the lab. The shared
[Linux 0.12 reading map](../../references/EARLY_LINUX_012_MAP.md)
(Week 11) records that 0.12 predates that model.

## 3. Evolution: why the early design changed

`cli` stops interrupts on one CPU. It does not order another CPU's loads
against this CPU's stores, and it is not a C memory order. Once more than
one CPU can write the same line, exclusion by the interrupt flag is the
wrong tool for publication. The kernel's SMP path uses atomic operations and
explicit barriers. This tree states that model in
`tools/memory-model/linux-kernel.cat` and lowers it in the architecture
headers behind `smp_store_release`, `smp_load_acquire`, and `smp_mb`.

That model is for kernel C: `READ_ONCE`, `WRITE_ONCE`, and the `smp_*`
macros. Portable C11 is a separate contract in `<stdatomic.h>`. A
`barrier()` plus `WRITE_ONCE` on x86-64, an `stlr` on AArch64, and a `fence`
on RISC-V are this commit's kernel mappings
(`adc218676eef25575469234709c2d87185ca223a`). They are evidence about the
kernel. They are not what `memory_order_release` in the lab must compile to.

ToyOS is one x86-64 core as well. Its timer interrupt is a scheduling event,
the one Week 7 and [Day
7](day-007-debuggers-sanitizers-and-system-comparison.md) already describe.
An interrupt flag there is not a C memory order.

## 4. Why the queue needed a mutex and the counter might not

[Day 4](day-004-condition-variables-and-bounded-queues.md) protected four
fields that move together: a slot, `head` or `tail`, `count`, and the
decision to signal. No single atomic store can publish "the slot is written
and `count` has been incremented" as one modification of one object. The
mutex made the whole transition one critical section, and its unlock
synchronized with the next lock.

A counter that only ever goes up by one is a different shape. Each step
reads one integer and writes that integer plus one. If those
read-modify-write steps are atomic operations on one atomic object, the C
memory model gives them a single modification order. The lab's
`atomic_fetch_add_explicit(..., memory_order_relaxed)` is that step. Four
threads perform it 200000 times each. The parent joins them and then loads
the counter. The checked result is 800000.

The mutex version of the same counter is still correct, and [Day 3](day-003-mutexes-ownership-lock-order-futex.md)
uses it to show a critical section. The atomic version is correct for a
narrower contract: one object, one modification order, no second piece of
payload published by the increment. Choosing relaxed is part of stating that
narrower contract. Relaxed does not say "this increment is unordered with
respect to itself." It says "do not create a synchronizes-with edge to any
other memory."

[Day 2](day-002-data-races-and-the-c-abstract-machine.md) is the reason an
ordinary `uint64_t` increment is not a substitute. Conflicting non-atomic
accesses with no happens-before edge are a data race, and a data race is
undefined behavior. Declaring the object `_Atomic`, or accessing it through
the `atomic_*` functions, makes the conflicting accesses atomic. The program
stays inside the memory model. What the model then guarantees depends on the
memory order written at each access.

## 5. Modification order

For each atomic object the model supplies one total order of all the
modifications of that object. Every thread that looks at the object agrees
on that order. It is called the modification order. It is per object. The
modification order of `x` and the modification order of `y` are two orders.
Nothing in the relaxed fragment of the model pastes them into one timeline.

A read-modify-write such as `atomic_fetch_add` reads the value immediately
preceding its own write in that order and computes the new value from it.
If the counter starts at 0 and eight hundred thousand relaxed fetch-adds
each add 1, the modification order is a chain:

```text
0 → 1 → 2 → … → 800000
     ^    ^
     each arrow is one atomic_fetch_add in the modification order
     the thread that performs it is whichever thread the implementation
     serialized at that step
```

The numeric result does not record which thread won which step. It records
that every step saw the previous value and wrote the next one, so no
increment was lost and no increment was applied twice. That is atomicity of
one object. It is the whole of what `memory_order_relaxed` contributes to
this counter.

The parent observes 800000 because of a second fact sitting next to the
modification order. Each worker's fetch-adds are sequenced before the worker
returns. Thread termination synchronizes with `pthread_join`. The parent's
load is sequenced after the joins. Happens-before therefore runs from the
increments to the load, and the load reads the value those increments
produced. A relaxed load in a thread that had no such edge could observe an
earlier value in the same modification order. The lab does not do that. It
joins, then loads.

Coherence of a single object is easy to picture and easy to over-read. Cores
and caches also keep a coherent picture of a cache line, which [Day 6](day-006-liveness-false-sharing-and-scalability.md)
connects to Week 2. Coherence of a line is a hardware property. Modification
order is a rule of the C abstract machine. A program can have a coherent
machine and still have undefined behavior if it races on a non-atomic
object. A program can have a well-defined relaxed counter and still allow
surprising interleavings of *other* objects. The litmus in section 10 is that
second situation.

## 6. What `memory_order_relaxed` promises

`memory_order_relaxed` is the order that adds atomicity and participation in
the object's modification order, and stops there. A relaxed store and a
relaxed load of the same object will agree with that order: a load sees some
value that belongs in it, and the usual coherence rules stop a single thread
from observing the order backwards. There is no synchronizes-with edge
between the store and the load merely because both were relaxed.

Consequences that the lab relies on:

- The fetch-add counter needs no extra ordering because it publishes nothing
  beside the counter.
- The compiler may reorder a relaxed atomic with neighboring ordinary
  accesses when the memory model allows it. Sequenced-before inside one
  thread still governs that thread's own uses of the values it computed.
  It does not, by itself, publish those ordinary accesses to another thread.
- The CPU may reorder the instructions the compiler emitted, within the
  architecture's memory model. Relaxed is a request that the compiler emit
  the cheapest mapping that still implements an atomic access.

`memory_order_relaxed` is the wrong order when a store is the publication of
some other bytes. The flag example in section 8 is the standard shape. A
relaxed store to `flag` after a plain store to `data` does not make a
relaxed load of `flag` synchronize with the writer. A reader that sees
`flag == 1` and then reads `data` has no happens-before edge to the writer's
store of `data`. If `data` is non-atomic, that is a data race. If both are
atomic and relaxed, the program is defined and the load of `data` may still
return the old value.

## 7. The compiler reorders, and then the CPU reorders

Two different machines sit between the source text and the wires.

```text
C abstract machine
  sequenced-before inside one thread
  synchronizes-with from atomics, mutexes, thread create/join
  happens-before, modification order, data-race freedom
        |
        |  the compiler must preserve this, and may rearrange
        |  anything the model leaves free
        v
assembly for one ISA
  registers, loads, stores, locked RMWs, fences
        |
        |  the CPU preserves the ISA memory model, and may use
        |  a store buffer, out-of-order execution, and a coherent cache
        v
one core's retirement, then the coherence protocol
```

The first arrow is why a data race is so severe. Once the program leaves the
model, the compiler's obligation to preserve the interleaving you imagined
is gone. The second arrow is why a defined program can still show an outcome
that the source lines, read top to bottom, do not suggest. Store buffers are
the everyday example on x86-64: a store retires into a buffer before it
becomes visible to other cores, and a later load of a different address can
complete from the cache while that store is still buffered.

Kernel code separates the same two arrows with two different tools, both
present in this tree.

`include/linux/compiler.h` defines the usual `barrier()` as an empty
`asm volatile` with a `"memory"` clobber. That instruction emits no CPU
fence. It tells the compiler that memory has been touched, so earlier loads
and stores in the compiler's view stay earlier, and later ones stay later.
`include/asm-generic/rwonce.h` defines `WRITE_ONCE` as a store through a
volatile-qualified pointer of the object's type, after a compile-time size
check. Volatile stops the compiler from deleting the store, merging it with
a neighbor, or splitting it into a torn pair of smaller stores. It is still
a compiler constraint. The CPU's memory model applies to whatever store
instruction the compiler actually emits.

A CPU fence is an instruction in the architecture header: `mfence`, a
`lock` prefix, `dmb`, `stlr`, `ldar`, `fence`. Those names show up in
section 11. They are how this kernel asks a particular CPU to order its own
accesses. They do not appear in the C source of the lab, and a C programmer
asks for them by writing a memory order, then checks the assembly to see
what this compiler did for this target.

Keeping the arrows apart is the point of the day's limit. An x86-64 process
that prints one outcome has exercised the second arrow on one machine. The
first arrow, the C abstract machine, is what makes the same source mean
something on another compiler and another ISA. A litmus test that prints the
expected order on x86-64 does not prove the C program.

## 8. Release and acquire

`memory_order_release` on a store and `memory_order_acquire` on a load are
the orders that publish a payload. The pattern is:

```text
writer                              reader
------                              ------
write payload bytes
atomic_store(&flag, 1, release)     if (atomic_load(&flag, acquire) == 1)
                                      read payload bytes
```

If the acquire load reads the value the release store wrote, or a later
value in that object's modification order from the release sequence the
standard defines, the release synchronizes with the acquire. Happens-before
then orders the writer's earlier accesses before the reader's later accesses.
The payload stores are visible to the payload loads. The flag is the
message. The payload rides on the edge the message creates.

Several limits belong in the same picture.

The edge exists when the load reads that write. A reader that still sees
the old flag has not synchronized, and it stays out of the payload. Spinning
on the flag with acquire until the value appears is the usual way to wait
for the edge. The queue in Day 4 does not use this pattern: it waits with a
mutex and a condition variable, and the mutex unlock/lock pair is itself a
release/acquire edge.

A release store orders *earlier* accesses in that thread against the store.
It does not order later accesses in the writer against the store. An acquire
load orders *later* accesses in the reader against the load. It does not
reach backward into the reader's earlier accesses. An atomic
read-modify-write that both publishes and observes uses
`memory_order_acq_rel` when it must do both jobs.

Release/acquire is pairwise. Thread A releasing `flag1` and thread B
acquiring `flag2` creates no edge. The litmus in section 10 is two relaxed
operations on two different objects, so it has no release/acquire pair at
all.

`memory_order_consume` was specified to order a load with later uses that
depend on the loaded value, at a lower cost than acquire on some weakly
ordered machines. In practice major compilers have strengthened it to
acquire for years. The lab does not use it, and new code in this course
should write acquire when a dependency is the intent.

## 9. Sequential consistency

`memory_order_seq_cst` places the operation in one total order, shared by
every sequentially consistent operation in the program, consistent with the
modification order of each object and with happens-before. A seq_cst store
also has release semantics, and a seq_cst load also has acquire semantics.
The extra promise is the single total order.

That promise is what makes the store-buffering outcome illegal for a
seq_cst version of the litmus. Label the operations:

```text
Thread 0:  Sx  store 1 to x, seq_cst
           Ly  load y, seq_cst
Thread 1:  Sy  store 1 to y, seq_cst
           Lx  load x, seq_cst
```

Sequenced-before puts `Sx` before `Ly` in thread 0 and `Sy` before `Lx` in
thread 1, and the seq_cst total order respects that. Suppose both loads
read 0, and the only stores in the round are the two stores of 1. Then `Ly`
read 0, so `Sy` is not before `Ly` in the order that would have supplied the
value 1. Likewise `Sx` is not before `Lx`. One consistent reading of the
total order is the cycle:

```text
Sx  before  Ly     (sequenced-before, both seq_cst)
Ly  before  Sy     (Ly observed the initial 0, so Sy is later)
Sy  before  Lx     (sequenced-before, both seq_cst)
Lx  before  Sx     (Lx observed the initial 0, so Sx is later)
```

A total order cannot contain a cycle. Both-zero is outside the seq_cst
model. The lab does not compile this variant. The argument above is a
reason to respect the order parameter, and it becomes a claim about a binary
only after you inspect that binary's operations and the compiler's mapping.
Seq_cst is also the default for the convenience functions that do not take
an explicit order. The lab's checked paths pass an explicit order so the
source states the promise in place.

Seq_cst is sufficient for a great many publication patterns and stricter
than release/acquire. The counter does not need it. A single-flag message
does not need it. The total order costs fences or locked instructions on
machines where ordinary stores are cheap, which is visible in the headers
in section 11: a full barrier is a different macro from a store-release.

## 10. Execution trace: the relaxed store-buffering litmus

`litmus_lab` runs 100000 rounds. `x` and `y` are `atomic_int`, set to 0 by
the parent at the start of each round with relaxed stores. Two workers and
the parent rendezvous on a pthread barrier. Worker 0 then does a relaxed
store of 1 to `x` and a relaxed load of `y`. Worker 1 does a relaxed store
of 1 to `y` and a relaxed load of `x`. A second barrier joins the round.
The parent counts rounds in which both loads returned 0. The workers write
those observations into separate slots of a plain `int observed[2]`. The
finish barrier synchronizes those plain stores with the parent's reads, so
the slots are not a data race. `x` and `y` are atomic, so the cross-thread
accesses on them are not a data race either. The order on every atomic
access in the round is relaxed.

Here is one execution the relaxed model allows. It is also the execution a
store buffer produces on x86-64.

```text
initial modification orders:  x: 0    y: 0

Thread 0                         Thread 1
--------                         --------
store x = 1
  enters store buffer
  not yet visible to thread 1
                                 store y = 1
                                   enters store buffer
                                   not yet visible to thread 0
load y
  cache still holds 0
  observed[0] = 0
                                 load x
                                   cache still holds 0
                                   observed[1] = 0
store buffer drains: x becomes 1, y becomes 1
parent's finish barrier, then both_zero increments
```

Nothing in this execution tears a store or invents a third value. Each
object's modification order is still `0` then `1`. Each load read a real
value from that order, the earlier one. There is no release store and no
acquire load, so neither load synchronizes with the other thread's store.
The pthread barriers order the *rounds*: the zeroing happens-before the
workers start, and the workers' writes to `observed` happen-before the
parent reads them. Inside the round they do not paste `x`'s modification
order onto `y`'s.

On x86-64 the picture is close to the hardware. The architecture allows a
load to pass an older store to a different address, which is what a store
buffer does. The compiler can emit ordinary stores and loads for these
relaxed atomics and the CPU can produce both-zero. On AArch64 and RISC-V the
architecture allows still more reorderings of ordinary loads and stores, so
the same source can produce both-zero for additional reasons. A run whose
`both_zero` count is 0 is a report of this process, this compiler, this
schedule, and these 100000 rounds. The printf in the lab records the count
and then prints `caveat=x86_observation_is_not_C_language_proof`. Believe
that line. The C abstract machine allows both-zero for `memory_order_relaxed`
whether or not this CPU exhibited it today.

A seq_cst compilation of the same shape forbids the cycle in section 9. If
you build that variant yourself, read the assembly before you interpret a
zero count. A compiler that lowered seq_cst to the same plain stores as
relaxed would not have implemented the model. The day's wrapper does not
build that variant; it prints the relaxed counter's function so you can see
a real lowering of a relaxed read-modify-write.

## 11. Architecture lens: three headers, three mappings

The kernel macros `smp_store_release`, `smp_load_acquire`, and `smp_mb` are
the kernel's names for release stores, acquire loads, and full barriers
between kernel memory accesses. `include/asm-generic/barrier.h` supplies a
default. Each architecture header may define the `__smp_*` forms first and
then include the generic file, which is what x86, AArch64, and RISC-V do in
this tree. The generic default is a full barrier around a `WRITE_ONCE` or
`READ_ONCE`:

```c
/* include/asm-generic/barrier.h :: __smp_store_release, __smp_load_acquire :: adc218676eef25575469234709c2d87185ca223a
 * lines 138–155
 */
#ifndef __smp_store_release
#define __smp_store_release(p, v)					\
do {									\
	compiletime_assert_atomic_type(*p);				\
	__smp_mb();							\
	WRITE_ONCE(*p, v);						\
} while (0)
#endif

#ifndef __smp_load_acquire
#define __smp_load_acquire(p)						\
({									\
	__unqual_scalar_typeof(*p) ___p1 = READ_ONCE(*p);		\
	compiletime_assert_atomic_type(*p);				\
	__smp_mb();							\
	(typeof(*p))___p1;						\
})
#endif
```

An architecture that does not override these pays for a full `__smp_mb()`
on every release store and every acquire load. The three architectures
below do override them, and they do not override them the same way. Read
the overrides as kernel engineering for a stated ISA, then keep C11 as a
separate layer that a C compiler lowers on its own.

### x86-64

`arch/x86/include/asm/barrier.h` in this tree:

```c
/* arch/x86/include/asm/barrier.h :: __smp_mb, __smp_store_release, __smp_load_acquire :: adc218676eef25575469234709c2d87185ca223a
 * lines 53–72
 */
#define __smp_mb()	asm volatile("lock; addl $0,-4(%%" _ASM_SP ")" ::: "memory", "cc")

#define __smp_rmb()	dma_rmb()
#define __smp_wmb()	barrier()
#define __smp_store_mb(var, value) do { (void)xchg(&var, value); } while (0)

#define __smp_store_release(p, v)					\
do {									\
	compiletime_assert_atomic_type(*p);				\
	barrier();							\
	WRITE_ONCE(*p, v);						\
} while (0)

#define __smp_load_acquire(p)						\
({									\
	typeof(*p) ___p1 = READ_ONCE(*p);				\
	compiletime_assert_atomic_type(*p);				\
	barrier();							\
	___p1;								\
})
```

`__smp_mb()` is one macro. `_ASM_SP` (`arch/x86/include/asm/asm.h` line 49,
via `__ASM_REG(sp)` at line 32) expands to `rsp` in a 64-bit build and `esp`
in a 32-bit build, so the locked `addl` targets that kernel's stack pointer.
The `lock` prefix makes that instruction a full barrier for ordinary memory,
and the `"memory"` clobber is also a compiler barrier. `__smp_store_release`
is not that instruction. It is `barrier()` followed by `WRITE_ONCE`: a
compiler barrier, then a plain store. `__smp_load_acquire` is `READ_ONCE`
followed by `barrier()`: a plain load, then a compiler barrier. The same
header defines `__dma_rmb()` as `barrier()` (line 50). `__smp_rmb()` calls
`dma_rmb()`, and `include/asm-generic/barrier.h` lines 45–46 wrap that helper
with a KCSAN annotation. The CPU half of that read barrier is the compiler
barrier. `__smp_wmb()` is `barrier()` directly.

The header does not contain the words "total store order." The mapping is
still the one people summarize that way, and the summary should stay tied
to the manuals rather than to a sentence this file does not have. For
write-back memory, the Intel and AMD x86-64 models keep stores in program
order with other stores, keep loads in program order with other loads, and
keep a store from passing an older load. They allow a load to pass an older
store to a different address. That last allowance is the store buffer in
section 10.

Under those rules a store-release needs the compiler to keep the payload
accesses before the flag store, and the CPU already keeps earlier stores
and earlier loads before that store. A load-acquire needs the compiler to
keep later accesses after the load, and the CPU already keeps later loads
and later stores after that load. A compiler barrier plus a plain access is
therefore the kernel's x86 mapping of release and acquire. A full barrier,
the locked add, remains the mapping of `__smp_mb()`, which is what seq_cst
and a few kernel protocols still call. This is a statement about the header
you just read plus the published x86-64 model. It is not a promise that a
given C compiler emits the same sequence for `memory_order_release`. Check
the `.s` file when that is the claim.

### AArch64

AArch64's ordinary loads and stores are weakly ordered. This tree does not
implement store-release as a plain store. `arch/arm64/include/asm/barrier.h`
emits a release store instruction. The same file sets `__smp_mb()` to
`dmb(ish)`, `__smp_rmb()` to `dmb(ishld)`, and `__smp_wmb()` to `dmb(ishst)`:
inner-shareable data memory barriers, full, load, and store.

```c
/* arch/arm64/include/asm/barrier.h :: __smp_mb, __smp_store_release :: adc218676eef25575469234709c2d87185ca223a
 * lines 123–160
 */
#define __smp_mb()	dmb(ish)
#define __smp_rmb()	dmb(ishld)
#define __smp_wmb()	dmb(ishst)

#define __smp_store_release(p, v)					\
do {									\
	typeof(p) __p = (p);						\
	union { __unqual_scalar_typeof(*p) __val; char __c[1]; } __u =	\
		{ .__val = (__force __unqual_scalar_typeof(*p)) (v) };	\
	compiletime_assert_atomic_type(*p);				\
	kasan_check_write(__p, sizeof(*p));				\
	switch (sizeof(*p)) {						\
	case 1:								\
		asm volatile ("stlrb %w1, %0"				\
				: "=Q" (*__p)				\
				: "rZ" (*(__u8 *)__u.__c)		\
				: "memory");				\
		break;							\
	case 2:								\
		asm volatile ("stlrh %w1, %0"				\
				: "=Q" (*__p)				\
				: "rZ" (*(__u16 *)__u.__c)		\
				: "memory");				\
		break;							\
	case 4:								\
		asm volatile ("stlr %w1, %0"				\
				: "=Q" (*__p)				\
				: "rZ" (*(__u32 *)__u.__c)		\
				: "memory");				\
		break;							\
	case 8:								\
		asm volatile ("stlr %x1, %0"				\
				: "=Q" (*__p)				\
				: "rZ" (*(__u64 *)__u.__c)		\
				: "memory");				\
		break;							\
	}								\
} while (0)
```

`stlrb`, `stlrh`, and `stlr` are AArch64 store-release instructions for 8,
16, 32, and 64 bits. The matching acquire loads are in `__smp_load_acquire`
immediately below, lines 162–191 of the same file: `ldarb`, `ldarh`, and
`ldar`, again selected by size, each with a `"memory"` clobber. Those
mnemonics are the architecture's acquire loads. This lesson stops at naming
the instructions the header emits. The Arm Architecture Reference Manual is
the definition of how `STLR` and `LDAR` order against other accesses in an
inner-shareable domain; the header is the evidence that this kernel uses
them for `smp_store_release` and `smp_load_acquire`.

The contrast with x86 is the contrast the macros encode. x86's
`__smp_store_release` is a compiler barrier and `WRITE_ONCE`. AArch64's is
an explicit `stlr` family instruction. Both macros are called
store-release in kernel code. The instructions inside them are the ISA
showing through.

### RISC-V

RISC-V's base memory model is weak, and this tree inserts a `fence`
instruction rather than a plain store. The fence text lives in
`arch/riscv/include/asm/fence.h`, and `arch/riscv/include/asm/barrier.h`
calls it:

```c
/* arch/riscv/include/asm/fence.h :: RISCV_FENCE :: adc218676eef25575469234709c2d87185ca223a
 * lines 5–7
 */
#define RISCV_FENCE_ASM(p, s)		"\tfence " #p "," #s "\n"
#define RISCV_FENCE(p, s) \
	({ __asm__ __volatile__ (RISCV_FENCE_ASM(p, s) : : : "memory"); })
```

```c
/* arch/riscv/include/asm/barrier.h :: __smp_mb, __smp_store_release, __smp_load_acquire, smp_mb__after_spinlock :: adc218676eef25575469234709c2d87185ca223a
 * lines 22–72
 */
/* These barriers need to enforce ordering on both devices or memory. */
#define __mb()		RISCV_FENCE(iorw, iorw)
#define __rmb()		RISCV_FENCE(ir, ir)
#define __wmb()		RISCV_FENCE(ow, ow)

/* These barriers do not need to enforce ordering on devices, just memory. */
#define __smp_mb()	RISCV_FENCE(rw, rw)
#define __smp_rmb()	RISCV_FENCE(r, r)
#define __smp_wmb()	RISCV_FENCE(w, w)

/*
 * This is a very specific barrier: it's currently only used in two places in
 * the kernel, both in the scheduler.  See include/linux/spinlock.h for the two
 * orderings it guarantees, but the "critical section is RCsc" guarantee
 * mandates a barrier on RISC-V.  The sequence looks like:
 *
 *    lr.aq lock
 *    sc    lock <= LOCKED
 *    smp_mb__after_spinlock()
 *    // critical section
 *    lr    lock
 *    sc.rl lock <= UNLOCKED
 *
 * The AQ/RL pair provides a RCpc critical section, but there's not really any
 * way we can take advantage of that here because the ordering is only enforced
 * on that one lock.  Thus, we're just doing a full fence.
 *
 * Since we allow writeX to be called from preemptive regions we need at least
 * an "o" in the predecessor set to ensure device writes are visible before the
 * task is marked as available for scheduling on a new hart.  While I don't see
 * any concrete reason we need a full IO fence, it seems safer to just upgrade
 * this in order to avoid any IO crossing a scheduling boundary.  In both
 * instances the scheduler pairs this with an mb(), so nothing is necessary on
 * the new hart.
 */
#define smp_mb__after_spinlock()	RISCV_FENCE(iorw, iorw)

#define __smp_store_release(p, v)					\
do {									\
	compiletime_assert_atomic_type(*p);				\
	RISCV_FENCE(rw, w);						\
	WRITE_ONCE(*p, v);						\
} while (0)

#define __smp_load_acquire(p)						\
({									\
	typeof(*p) ___p1 = READ_ONCE(*p);				\
	compiletime_assert_atomic_type(*p);				\
	RISCV_FENCE(r, rw);						\
	___p1;								\
})
```

`fence pred,succ` orders the predecessor set against the successor set.
`RISCV_FENCE(rw, w)` before the store orders earlier reads and writes before
later writes, which is this tree's release fence. `RISCV_FENCE(r, rw)` after
the load orders the load, a read, before later reads and writes, which is
this tree's acquire fence. `__smp_mb()` is `RISCV_FENCE(rw, rw)`. `__mb()`
is `RISCV_FENCE(iorw, iorw)` and also orders I/O; the generic header wraps
`__mb()` as `mb()`. The `"memory"` clobber on `RISCV_FENCE` is the compiler
half.

The comment on `smp_mb__after_spinlock` in that excerpt (lines 32–57) sits
above `__smp_store_release`. It says the kernel's own lock sequence uses
acquire/release bits on the atomic instructions and that those bits give an
RCpc critical section on that one lock. The comment's conclusion in this
tree is to emit `RISCV_FENCE(iorw, iorw)` after the lock anyway. The detail
that matters here is the caution already present in the source: a RISC-V
acquire/release pair in this kernel is not treated as a drop-in
sequential-consistency fence. When you map a C11 order onto RISC-V, cite
the compiler output and the ISA manual for that order, and expect a `fence`
or an `aq`/`rl` bit where x86 had a plain `mov`.

### What these three mappings do not decide

They do not decide the assembly of `increment_atomic` in the lab. That
function is a C11 `atomic_fetch_add_explicit` with `memory_order_relaxed`
on an `atomic_uint_fast64_t`. A relaxed read-modify-write still has to be
atomic. On x86-64 the usual instruction is a `lock`-prefixed add or `xadd`,
because atomicity of a read-modify-write is a locked bus operation even when
the memory order is relaxed. Confirm the opcode in the file the wrapper
writes. A locked add is a full barrier on x86 as a *consequence of the
instruction the CPU requires for atomicity*. That hardware consequence is
stronger than the C order the source requested. Other threads still must not
rely on it for portable publication. On AArch64 the same C function may be
an exclusive loop or an LSE atomic, depending on the target flags, and the
relaxed form need not be `stlr`/`ldar`. On RISC-V it may be an AMO with or
without `aq`/`rl`. This tree's kernel headers show the kernel's choices.
The wrapper's `.s` file shows one C compiler's choice for one function on
the host you actually run.

## 12. How to read the evidence without promoting it

The wrapper's prediction line is the right size. The atomic count is exact:
800000. The litmus prints `both_zero` and does not require a particular
value. The assembly path is best-effort; a failure to generate it is a skip
recorded in `assembly.log`, and the counter result stands on its own.

When the `.s` file exists, find `increment_atomic` and name the instruction
that updates memory, including any `lock` prefix or fence. Record
`cc --version` and `uname -m` from the wrapper's output beside that
instruction. The pair is "this compiler, this target, this function." It
becomes a portability claim only after the same source is lowered for
another target, and even then the claim is about those compilers, not about
every future one.

Leave these as open, because the lab does not close them:

- every interleaving the abstract machine allows;
- the seq_cst lowering, which the wrapper does not generate;
- glibc's internal atomics inside a mutex or a condvar;
- the kernel's `smp_store_release` as if it were the C library.

[Day 4](day-004-condition-variables-and-bounded-queues.md) already showed
`set_current_state` storing task state with `smp_store_mb`, and
`futex_wait_setup` depending on the hash-bucket lock to keep a wake from
passing the waiter's publication. Those are kernel clients of the barriers
in this section. The queue lab remains a mutex program. It does not become
a lock-free program because the kernel uses atomics on the sleep path.

## 13. Mastery: Explain · Draw · Observe · Build

### Explain

- What a modification order is, and why eight hundred thousand relaxed
  fetch-adds on one counter have a determined final value.
- Which extra edge the parent's `pthread_join` supplies before the load.
- What a release store synchronizes with, and which payload accesses ride
  on that edge.
- Why the seq_cst both-zero cycle cannot be a total order, and why the
  relaxed litmus may print both-zero anyway.
- Which instruction this tree's x86 header uses for `__smp_store_release`,
  which instruction the AArch64 header uses, and which fence the RISC-V
  header places before the store. Say, in the same answer, that those are
  kernel macros.

### Draw

Draw the two arrows of section 7 for the litmus: source operations, then
the instructions you think this host emits, then the store buffers and the
loads that return 0. Draw the modification orders of `x` and `y` as two
separate chains. Draw the seq_cst cycle underneath and mark it as a
forbidden total order. The relaxed binary is allowed to print both-zero.

### Observe

Run
[challenges/day-005-atomics-memory-order.sh](../challenges/day-005-atomics-memory-order.sh).
Keep the exact counter line, the litmus line including `both_zero` and the
caveat, the compiler version, `uname -m`, and either the `increment_atomic`
assembly or the skip line. If `both_zero` is 0, write the sentence from
section 10 about what that count measures.

### Build

From the printed assembly and the three headers, make a three-row note:
x86-64, AArch64, RISC-V. Each row names the kernel store-release sequence
quoted above and states whether a CPU fence instruction is present in that
macro. Add a fourth row for the lab's relaxed fetch-add, filled only with
what the wrapper's assembly file shows on this host. The note is done when
a reader can see that a plain x86 store-release in the kernel header, an
`stlr` on AArch64, a RISC-V `fence rw,w`, and a locked counter increment in
a C binary are four different artifacts.
