# Day 2 — Data Races and the C Abstract Machine

**Target time:** approximately 3 hours

> Four threads execute `unsafe_counter++`. The machine is coherent, the
> compiler is conforming, and the printed total can still be anything —
> including the total you hoped for. Where did the program lose its meaning?

Day 1 left two tasks in one `mm_struct`. The same virtual address is the
same object, and Week 2's caches will move that line between cores until
they agree on the bytes. Today's question is older than the cache protocol.
It is the question [Week 2's abstract machine](../../02-assembly-memory-and-c/sections/day-007-c-execution-model-and-data-types.md)
already posed for a single thread: C constrains observable behavior, and
where the program has undefined behavior the implementation has no
obligation to preserve the story the source seems to tell.

The lab object is `unsafe_counter` in
[`thread_lab.c`](../../projects/linux-system-programming/src/thread_lab.c).
Four threads run this loop, `ITERATIONS` equal to 200000:

```c
for (int i = 0; i < ITERATIONS; ++i) unsafe_counter++;
```

Linux 6.12 wording quoted below is from commit `adc218676eef25575469234709c2d87185ca223a`. The listing in
section 2 is Linux 0.12 and is not that commit. The assembly is
from this host's GCC 16.2.1 20260819 (Red Hat 16.2.1-2), which is also the
compiler that produced `projects/linux-system-programming/build/thread_lab`
under the project's `CFLAGS ?= -O2`.

---

## 1. Syntax needed for this source

`counter++` on a plain integer is a C expression. In the abstract machine it
is a read of the object, an addition, and a write back. The source text does
not make those steps one access. A store through a pointer,
`current->state = 0`, is likewise one ordinary assignment.

```c
unsigned long counter;

void bump(void)
{
    counter++;                 /* read, add, write */
}
```

Nothing in that expression is an atomic operation, a memory order, or a
lock. The 6.12 and C11 sections below name the extra syntax (`_Atomic`,
`memory_order`, `data_race`) when the program actually needs it.

## 2. Linux 0.12 first: one CPU, and the timer can interleave

Linux 0.12 runs on one CPU. Another task does not execute beside this one.
The timer still cuts in. `do_timer` (`kernel/sched.c` lines 364–367, printed
p. 365) counts down the current slice and, when the interrupted code was
user mode (`cpl != 0`), calls the scheduler:

```c
if ((--current->counter)>0) return;
current->counter=0;
if (!cpl) return;          /* still inside the kernel: do not switch */
schedule();
```

`schedule` itself stores task state with ordinary assignments, including
`(*p)->state = TASK_RUNNING` when a sleeping task's timer or signal says it
may run again (lines 128–131 and 137–139). Trace the interleaving as: task
A is in user code, the timer interrupt runs `do_timer`, `schedule` may
`switch_to` task B, and a later tick can switch back. A's stores and B's
stores occupy the same memory in program order only along each task's own
instructions. Across the switch they are neighbors in time.

That is why an ordering question shows up even before a second core exists.
What carries forward is the question, not a rule for answering it.

Linux 0.12 cannot teach C11 atomics, data-race undefined behavior, or an
SMP memory model. It predates those contracts. The C abstract machine
remains the authority for the lab. The shared
[Linux 0.12 reading map](../../references/EARLY_LINUX_012_MAP.md)
(Week 11) states the same limit.

## 3. Evolution: why the early design changed

On the uniprocessor in section 2, task A and task B never execute at once.
The timer is the interleaving: `schedule` switches, and the two tasks'
stores are neighbors only across that switch. A second core removes that
assumption. Both tasks can execute, and a cache-coherence protocol moves the
line so a later load observes some store. Coherence agrees on bytes. It does
not create a happens-before edge the language never had.

The compiler is the other actor. Where the abstract machine says a data race
cannot occur, a conforming compiler may collapse a loop, keep a value in a
register, or delete a store. A lost update and a printed total of 800000 are
one fact: both are executions the standard no longer constrains.

C11 names the rule. Conflicting accesses, at least one non-atomic, with no
happens-before between them, are a data race, and a data race is undefined
behavior. Kernel C keeps that discipline for unmarked shared accesses.
`data_race` in `include/linux/compiler.h` (lines 191–215 of commit
`adc218676eef25575469234709c2d87185ca223a`) is a hint to tooling, and the
access stays non-atomic. KCSAN (`include/linux/kcsan.h`) is the checker that
hint talks to.

ToyOS at the scheduler milestone is still one core and a timer tick, with no
C memory model. Week 7 records that switch. [Day
7](day-007-debuggers-sanitizers-and-system-comparison.md) places ToyOS
beside these hosted rules. Neither is the authority for `counter++`.

## 4. Why a lost update and a perfect total are one fact

`counter++` on a plain `uint64_t` is a read, a modify, and a write of a
non-atomic object. In the abstract machine those are separate accesses.
Two threads performing them on the same object, with at least one write,
and with no happens-before edge between the accesses, execute a **data
race**. A data race is undefined behavior. The standard then imposes no
requirements on that execution. The final number, the number of memory
operations left in the loop, and the text a later `printf` shows are all
outside what the standard still specifies.

Hardware can still finish every instruction. Coherence can still make every
core's later load see some agreed value. A particular schedule can still
print `800000`, which is `4 * 200000`. That printout is a property of one
run of one binary. It is the abstract machine declining to give the run a
portable meaning. The challenge script therefore treats the racy total as
unconstrained and labels the mode `data_race=undefined_behavior`.

The mutex mode of the same program is a different program. Every increment
happens while `counter_lock` is held, unlock synchronizes with a later
lock, and the checked result is exactly 800000. Day 3 builds that edge.
Today the point is to see the edge's absence in both the language and the
machine.

---

## 5. Conflicting accesses and happens-before

Two accesses **conflict** when they target the same memory location and at
least one of them modifies the object. A load conflicts with a store. Two
stores conflict. Two pure loads of the same object are concurrent reads.

**Happens-before** is how the language orders accesses across threads. It
is assembled from:

- **sequenced-before** inside one thread (the ordinary left-to-right story
  of one thread's statements, with the evaluation-order exceptions Week 2
  already listed);
- **synchronizes-with**, the edges supplied by thread creation, thread
  join, mutex unlock/lock pairs, and atomic release/acquire operations.

If A happens-before B, the abstract machine requires B's view of the object
to reflect A's side effects. If two conflicting accesses have no
happens-before edge either way, the execution has a data race.

```text
parent: unsafe_counter = 0
   │
   │  pthread_create          (synchronizes-with thread start)
   ▼
worker 0                         worker 1
load / modify / store  ·······  load / modify / store
        no happens-before edge between these accesses
   │
   │  pthread_join            (synchronizes-with thread termination)
   ▼
parent: printf the counter
```

The vertical edges are real. The parent's store of 0 happens-before every
worker's first access, and each worker's termination happens-before the
parent's `printf`. The horizontal edge is the one the program never
builds. Join orders the printf after the workers finish. The workers'
accesses to `unsafe_counter` stay unordered with respect to each other,
and those are the conflicting accesses.

Wall-clock overlap is a different question. A scheduler that runs one
worker to completion before the other has ordered that run. The abstract
machine's promise is the set of edges the program built, and a lucky
schedule is not one of those edges.

---

## 6. The increment in registers

[Week 2](../../02-assembly-memory-and-c/sections/day-001-x86-64-assembly-foundations.md)
spells a memory increment as a load into a register, an `add`, and a
store. At `-O0`, GCC 16.2.1 emits that shape for a file-scope `uint64_t`.
The rip-relative operand is the relocation for the counter; this object
file was not linked, so the displacement prints as zero:

```text
mov    0x0(%rip),%rax
add    $0x1,%rax
mov    %rax,0x0(%rip)
```

In the Week 2 spelling those are `movq`, `addq $1, %rax`, and `movq`.
The surrounding `-O0` loop keeps the counter in memory on every iteration
and the induction variable in a stack slot:

```text
movl   $0x0, -0x4(%rbp)          # i = 0
mov    counter(%rip), %rax       # load
add    $0x1, %rax                # add in the register
mov    %rax, counter(%rip)       # store
addl   $0x1, -0x4(%rbp)          # i++
cmpl   $0x30d3f, -0x4(%rbp)      # i <= 199999
jle    ...                       # back to the load
```

`0x30d3f` is 199999, the last value of `i` for which the body runs.
`%rax` is private to the core that executes the three memory-touching
instructions. The load copies the line's bytes into that register. The
add updates the register. The store writes the register back. Between the
load and the store the counter in memory is stale relative to `%rax`, and
another core is free to own the line.

Take a starting value of 41 and two cores:

```text
Core 0                              Core 1
mov  counter(%rip), %rax   # 41     mov  counter(%rip), %rax   # 41
add  $1, %rax              # 42     add  $1, %rax              # 42
mov  %rax, counter(%rip)   # 42     mov  %rax, counter(%rip)   # 42
```

The source asked for 43. Memory holds 42. Each `add` was a correct
register operation on a private copy of 41. The same shape with a general
starting value `N` stores `N+1` twice: two source-level increments become
one surviving store. Nothing in the three-instruction sequence is a single
memory operation. The `add` never touches the line.

x86-64 will perform an aligned 8-byte load or store as one architectural
access. The read-modify-write is three accesses unless the instruction
stream uses a locked read-modify-write. The `-O0` sequence has no `lock`
prefix. Day 5's `atomic_fetch_add` is what asks the compiler to emit one.

The prefix is an x86-64 fact. AArch64 compilers emit an atomic add as
`LDADD` or as an exclusive `LDXR`/`STXR` pair, depending on the core and
the code generator. RISC-V uses an atomic memory operation such as
`amoadd.d`. Those are the instructions a C11 atomic asks for. Plain
`counter++` asks for none of them: each ISA implements it with an ordinary
load, an add in a register, and an ordinary store, which is the sequence
above. This lesson quotes the encoding this host emitted.

---

## 7. Why the cache can look finished while the language is not

[Week 2's cache lesson](../../02-assembly-memory-and-c/sections/day-005-cache-organization.md)
already has the hardware half. A core that wants to write a line takes
exclusive ownership; other copies are invalidated or downgraded. After both
stores in the diagram above, some core holds the line and every later
coherent load sees `N+1`. The protocol did its job. It delivered a single
agreed value for a pair of stores that both carried the same bits.

```text
counter lives on one 64-byte line

Core 0 load     obtains a readable copy, fills %rax with N
Core 1 load     obtains a readable copy, fills %rax with N
Core 0 store    takes exclusive ownership, line becomes N+1
Core 1 store    takes exclusive ownership, line becomes N+1 again
```

The line never held `N+2`, because `N+2` existed only as a sum of two
private registers that were never combined. Coherence orders ownership of
the line. The load, the register add, and the store stay three operations.

This is how a non-atomic increment can appear to work. Run the workers
with no overlap and each load sees the previous store: the total matches
the arithmetic. Run them overlapped and some stores replace other stores
of the same computed value: the total comes out low. Both runs used a
coherent memory system. The C rule does not consult that outcome. The
accesses conflicted, and no happens-before edge ordered them, so the
execution has no required meaning either way. A later compiler, a
different schedule, or ThreadSanitizer is entitled to treat the same
source differently.

False sharing, from the same Week 2 page, is the complementary layout
fact: two different objects on one line bounce the line even though they
do not conflict in C. Today's bug is true sharing of one object. The line
size explains the traffic. The data-race rule explains the meaning.

---

## 8. The compiler may assume the race does not exist

The as-if rule lets the compiler transform a program when the required
observable behavior is preserved. For a program with a data race, there
is no required behavior on that execution, so the compiler may transform
the code under the assumption that the race does not occur. In practice
that assumption is "this thread is the only one performing conflicting
accesses on this object for the duration of the loop."

The project's Makefile sets `CFLAGS ?= -O2`. The built `thread_lab` on
this host contains, at `increment_unsafe`:

```text
400a00:  addq   $0x30d40, 0x2685(%rip)    # unsafe_counter
400a0b:  xor    %eax, %eax
400a0d:  ret
```

`0x30d40` is 200000, the constant `ITERATIONS`. The 200000 separate
read-modify-writes of the source have become one add of 200000 to memory.
That is a correct compilation of a single-threaded loop: the counter ends
200000 higher, and no conforming program can observe the intermediate
values from another thread. It is a correct compilation of the racy
program only because the racy program has no requirements. If another
thread's increments had to be respected one by one, adding 200000 in one
step would drop them on the floor whenever they landed in the middle of
the loop.

The same compiler, same version, asked for three nearby shapes:

```text
global uint64_t, -O2
    addq   $0x30d40, counter(%rip)          one unlocked add of 200000

volatile uint64_t, -O2
    loop: addq $1, counter(%rip)            a memory RMW per iteration
          no lock prefix

atomic_fetch_add_explicit(..., memory_order_relaxed), -O2
    loop: lock addq $1, counter(%rip)       an atomic RMW per iteration
```

`volatile` keeps the accesses in the instruction stream. Each `addq $1` to
memory is still a non-atomic read-modify-write: the prefix that would make
it atomic on x86-64 is `lock`, and it is absent. Two cores can still
interleave inside that instruction. The abstract machine still sees
conflicting non-atomic accesses. `volatile` is the qualifier for objects
whose accesses are observable as accesses — device registers, and the
narrow signal patterns the standard actually defines. It is a different
contract from inter-thread happens-before.

The atomic loop keeps one atomic read-modify-write per source increment
(`lock addq` on this ISA; GCC unrolled it by two). `memory_order_relaxed`
is enough for the narrow claim that increments are preserved in one
modification order. Each operation is one step of that order. Other payload
data needs a release/acquire protocol, which day 5 separates from this
counter. The final load in the lab happens after every worker has been
joined, so thread termination supplies the phase edge around the relaxed
adds.

The `-O2` collapse is one compiler's choice. Another version can keep the
three-instruction loop. Both are licensed by the same rule. Reading the
binary tells you which choice this build made. That choice stays a fact
about this build. The C source's meaning is the rule that licensed it.

Even the collapsed `addq $0x30d40, mem` has no `lock` prefix. Four threads
each adding 200000 with an unlocked memory read-modify-write can still
lose an entire chunk of 200000 when two of those instructions interleave.
A racy total below 800000 on this binary is that machine-level loss, or a
further consequence of undefined behavior. A racy total of exactly 800000
is a schedule in which those unlocked adds happened not to collide. The
source line is the same line in both cases.

---

## 9. The interleaving picture is already the smaller claim

Section 6 is what the `-O0` instructions do when two cores overlap: each
core adds 1 to a private register, and one of the stores survives. It is
tempting to stop there, call the bug a lost update, and treat a total of
800000 as proof the schedule was kind. That picture is real, and it is
smaller than the permission the compiler used.

The `-O2` function never performs 200000 loads and 200000 stores. The
steps an interleaving story wants to shuffle have already been deleted,
before any second thread is scheduled. A model that only shuffles
source-level `++` operations cannot derive `addq $0x30d40`. Deriving it
requires the premise "no other thread conflicts," which is the premise a
data race withholds.

The layers sit on top of each other, and each one has an artifact:

```text
language    conflicting non-atomic accesses, no happens-before
            → this execution has no requirements

compiler    may delete, combine, or reorder under that permission
            → this binary's worker is one unlocked add of 200000

machine     that addq is still a non-atomic read-modify-write
            → two cores can each read M and each write M+200000
            → the line ends on one of those stores

coherence   the line has one value afterward
            → later loads agree
            → agreement is a hardware fact, and the C execution
              still has no requirements
```

The totals this binary usually prints come from the machine layer: overlapping
unlocked adds of 200000 can drop a chunk of that size. Undefined behavior is
wider than that menu. The collapse of the loop sits outside the source-level
interleaving model. A sanitizer report, a surprising total, or a later
compiler that deletes still more of the function are inside the same
permission.

The repair restores a requirement, and it is specific. `increment_mutex`
performs each update of `mutex_counter` while `counter_lock` is held, so
the critical sections are ordered by unlock-then-lock. The relaxed atomic
makes each increment one step of a single modification order, and the joins
order the final load after those steps. Day 3 and day 5 are those repairs.
A kind schedule leaves the missing edge missing. A coherent line leaves it
missing too.

---

## 10. The kernel's own name for an intentional race

The Linux tree you are reading is itself C, and it has the same rule.
`include/linux/compiler.h` spells a narrow escape hatch for diagnostics.
The comment is the kernel's, and it matches the split above: the macro is
a hint to tooling, and it leaves ordinary code generation alone.

```c
/* include/linux/compiler.h :: data_race :: adc218676eef25575469234709c2d87185ca223a
 * lines 191–215
 */
/**
 * data_race - mark an expression as containing intentional data races
 *
 * This data_race() macro is useful for situations in which data races
 * should be forgiven.  One example is diagnostic code that accesses
 * shared variables but is not a part of the core synchronization design.
 * For example, if accesses to a given variable are protected by a lock,
 * except for diagnostic code, then the accesses under the lock should
 * be plain C-language accesses and those in the diagnostic code should
 * use data_race().  This way, KCSAN will complain if buggy lockless
 * accesses to that variable are introduced, even if the buggy accesses
 * are protected by READ_ONCE() or WRITE_ONCE().
 *
 * This macro *does not* affect normal code generation, but is a hint
 * to tooling that data races here are to be ignored.  If the access must
 * be atomic *and* KCSAN should ignore the access, use both data_race()
 * and READ_ONCE(), for example, data_race(READ_ONCE(x)).
 */
#define data_race(expr)							\
({									\
	__kcsan_disable_current();					\
	__auto_type __v = (expr);					\
	__kcsan_enable_current();					\
	__v;								\
})
```

`__kcsan_disable_current` tells the kernel's concurrency sanitizer to
ignore the expression. The generated instructions for `expr` stay whatever
the compiler would have emitted. `copy_process` in the same tree uses the
macro on a limit check, `data_race(nr_threads >= max_threads)` at
`kernel/fork.c` line 2256: a racy read of a counter, marked on purpose,
and not a read-modify-write of user data.

`unsafe_counter++` carries no such mark, and it modifies the object. The
abstract-machine rule for an unmarked conflicting pair is the one in
section 5. The kernel macro is evidence that the people who compile this
tree treat "data race" as a language and tooling category, separate from
"the CPU completed a load and a store."

---

## 11. Execution trace: four workers, one object

`mode_counter("race")` zeroes `unsafe_counter`, starts four threads in
`increment_unsafe`, joins them, and prints the value. The binary under
discussion is the `-O2` build, so each worker's loop is the single
`addq $0x30d40`.

```text
parent
  unsafe_counter = 0                          sequenced-before create
  pthread_create × 4                          synchronizes-with each start

worker 0                worker 1              worker 2              worker 3
addq $200000, mem       addq $200000, mem     addq $200000, mem     addq $200000, mem
  │                       │                     │                     │
  └── each addq is an unlocked read-modify-write of the same line
      private ALU work sits between the load half and the store half
      coherence will agree on some final bytes
      no happens-before edge connects any pair of these four accesses

parent
  pthread_join × 4                            synchronizes-with each exit
  printf(race_counter=… expected=800000 data_race=undefined_behavior)
```

One concrete interleaving of two of those `addq` instructions, counter
currently 0:

```text
Core 0 reads 0, computes 200000
Core 1 reads 0, computes 200000
Core 0 writes 200000
Core 1 writes 200000
line holds 200000; 200000 of source-level increments are gone
```

A different interleaving writes 200000, then 400000, then 600000, then
800000, and the printf shows the expected number. The source, the
conflict, and the missing edge are unchanged.

ThreadSanitizer, when the challenge can run it, instruments the accesses
and looks for a conflicting pair with no happens-before edge. A report
names that pair for the schedule the instrumented binary actually ran.
The challenge classifies three other outcomes on purpose: the
instrumented program failed to build, the runtime could not execute, or
the run finished without a recognized report. Those are facts about the
tool on this host. The race is a fact about the source.

---

## 12. Mastery: Explain · Draw · Observe · Build

Run [`../challenges/day-002-data-races.sh`](../challenges/day-002-data-races.sh).
Predict `mutex_counter=800000`. Leave the racy total unpredicted. Predict
that ThreadSanitizer will either report the race or be classified as
unavailable or inconclusive. Record which of those the script actually
printed.

### Explain

- A data race is two conflicting accesses to one non-atomic object, at
  least one of them a write, with no happens-before edge between them.
- `unsafe_counter++` is a load, an add in a register, and a store.
  `%rax` on one core is invisible to the other core's `%rax`.
- Coherence makes the cores agree on the line. Agreement on `N+1` after
  two increments is a coherent machine doing what it was asked. The
  language assigns that execution no requirements.
- This host's `-O2` build of `increment_unsafe` is `addq $0x30d40` with
  no `lock` prefix, because the compiler may assume the race does not
  exist. `volatile` keeps per-iteration accesses and still has no `lock`.
  `lock addq` is what the relaxed atomic became.
- `pthread_join` orders the printf with thread termination. The missing
  edge is between the workers.

### Draw

1. The happens-before sketch from section 5, with the four workers on the
   horizontal line and create/join as the only vertical edges.
2. Two cores executing `mov` / `add` / `mov` against one line, and the
   same two cores executing unlocked `addq $0x30d40`. Mark where the value
   lives in a register and where it lives in the line.
3. The three instruction shapes: plain `-O2`, `volatile`, and
   `lock addq`.

### Observe

From the challenge, keep:

- the mutex line, which is allowed to be exact;
- the race line, including whatever total this run produced, next to the
  sentence "this execution has undefined behavior";
- the ThreadSanitizer classification and, if a report was produced, the
  two stacks it names;
- `cc --version` and the project's `CFLAGS`.

Then disassemble the function the challenge just ran:

```bash
objdump -d --disassemble=increment_unsafe \
  ../../projects/linux-system-programming/build/thread_lab
```

Record whether you still see one `addq` of `0x30d40` or a load/add/store
loop. Rebuild a throwaway copy at `-O0` if you want the three-instruction
shape beside it. The addresses and the compiler version belong in the
note. A different machine may choose a different shape; the data-race
rule is what both shapes are allowed to rely on.

### Build

Write a five-line ledger for `unsafe_counter` in the race mode:

```text
access              thread        conflicts with          happens-before?
store of 0          parent        later worker accesses   yes: create
load/add/store      worker i      worker j, i != j        no
printf's load       parent        worker stores           yes: join
```

Then answer, in one sentence each: why a printed total of 800000 leaves
the ledger's middle row in place, and which instruction in the `-O2`
binary is still an unlocked read-modify-write.

### Why? notebook

1. Why is "the threads ran at the same wall-clock time" the wrong test
   for a data race?
2. Why can both cores execute a correct `add` and still lose an increment?
3. Why does a coherent cache line at `N+1` after two increments satisfy
   the hardware and leave the C program without a meaning?
4. Why is `addq $0x30d40, mem` a legal compilation of a 200000-iteration
   `++` on a non-atomic object?
5. Why does that same `addq` still have a machine-level race with a
   second core's `addq`?
6. Why does `volatile` keep the loop and still leave a data race?
7. Why is `pthread_join` enough to order the printf and not enough to
   order the workers with each other?
8. Why is a ThreadSanitizer skip a fact about the tool, and the race a
   fact about the source?

---

## References used selectively

- C abstract machine and undefined behavior, as used in
  [Week 2, day 7](../../02-assembly-memory-and-c/sections/day-007-c-execution-model-and-data-types.md).
- Linux 6.12 commit `adc218676eef25575469234709c2d87185ca223a`, `include/linux/compiler.h` `data_race`, and
  the call at `kernel/fork.c` line 2256.
- Host observation: GCC 16.2.1, `increment_unsafe` in the `-O2`
  `thread_lab` binary, and the `-O0` / `volatile` / relaxed-atomic shapes
  compiled beside it.

**Next:** the mutex mode puts a happens-before edge between those workers.
Day 3 names the invariant that edge protects, and the futex that lets the
uncontended lock stay in user mode.
