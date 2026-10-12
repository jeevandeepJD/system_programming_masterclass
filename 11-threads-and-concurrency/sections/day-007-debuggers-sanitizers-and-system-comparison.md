# Day 7 — Debuggers, Sanitizers, and Three Systems

**Target time:** approximately 3 hours

> Each tool stops the program at a different layer and answers a different
> question. A report is finished when it says which layer produced each fact,
> which binary those facts belong to, and which questions that layer leaves
> open.

The day's wrapper builds that report for you and leaves the interpretation
as the work.
[challenges/day-007-debug-report.sh](../challenges/day-007-debug-report.sh)
writes a temporary file, runs the deterministic Week 11 suite, then runs the
intentional race, the timed deadlock, the false-sharing samples, and the
litmus, then tries GDB, Valgrind Memcheck, ASan/UBSan, and ThreadSanitizer.
Predict `WEEK11 PASS` from the suite. Predict that the deadlock detector
exits. Treat every optional tool as "evidence, or an accurate skip."

## 1. Syntax needed for this source

`printk` takes a pointer to a format string and a variable argument list.
A function pointer is a separate form: the timer list stores a `void
(*)(void)` and the tick calls through it. GDB is about to show you both.

```c
int note(const char *fmt, ...);          /* fmt points at the format text */

void run(void (*fn)(void))
{
    fn();                                /* the call target is data */
}
```

The format pointer is not the console device, and the function pointer is
not a task record. Each is an address the debugger can print.

## 2. Linux 0.12 first: printk, then the whole tree under Bochs and GDB

The kernel's own printer is `printk` (`kernel/printk.c` lines 21–31,
printed p. 439). It formats into a static buffer and hands that buffer to
the console:

```c
int printk(const char *fmt, ...)
{
        va_list args;
        int i;

        va_start(args, fmt);
        i=vsprintf(buf,fmt,args);
        va_end(args);
        console_print(buf);
        return i;
}
```

`do_timer` in the same kernel calls a timer through a function pointer,
`fn = next_timer->fn; (fn)();` (`kernel/sched.c` lines 354–359). Under a
debugger that is a direct call to whatever address the list held.

Chapter 17 of the same annotated book (printed pp. 1094–1095, "Debugging
Kernel Code with GDB and Bochs") treats the 0.1x tree as one image. Bochs
is built with its gdbstub, the kernel is rebuilt with `-g`, and GDB on the
host speaks to that stub. The book states that the 0.12 steps match the
0.11 steps it writes down. Trace that workflow as complete-tree debugging:
every `task_struct`, `schedule`, `sleep_on`, and `printk` is in the image
you can stop, and `printk` is the trace the kernel emits while it runs.

What carries forward is the habit of naming the layer a fact came from.
ToyOS is the first system this day already compares: two ring-0 tasks,
cooperative `schedule` and the timer path, `TASK_BLOCKED` meaning finished.
That reconstruction is section 11 below, in
[projects/toyos/src/tasks.c](../../projects/toyos/src/tasks.c). The
teaching-OS `sleep`/`wakeup` is section 12, quoted only after you pin a
revision.

Linux 0.12 cannot teach Valgrind Memcheck, ASan, UBSan, or ThreadSanitizer,
and it cannot teach a data race in hosted multithreaded C. Those tools
instrument a different language implementation than this freestanding
uniprocessor kernel. The shared
[Linux 0.12 → Linux 5.10 map](../../references/EARLY_LINUX_012_MAP.md)
(Week 11) keeps 0.12 as the small map. The following sections are the
modern tools on the lab binary, then ToyOS and Linux 5.10 side by side.
The 5.10 excerpts already in this week stay at tree `c9b2385d6793`.

## 3. Why the layers have to stay apart

A concurrent C program is several machines at once.

```text
C abstract machine
  objects, lifetimes, happens-before, data-race rules
        |
        | compiler may instrument (ASan, UBSan, TSan)
        v
userspace binary
  registers, instructions, heap allocator
        |
        | Valgrind translates this binary as it runs
        | GDB stops it and reads architectural state
        v
kernel tasks
  clone, stacks, futex sleep, ptrace, the scheduler
        |
        v
caches, store buffers, the coherence protocol
```

A fact established on one arrow does not automatically establish the next.
GDB can show that two threads exist and that one of them is inside
`pthread_cond_wait`. That is a snapshot of tasks and registers. It does not
say whether the `while` predicate in the source is the right one. ThreadSanitizer
can report a conflicting pair of accesses. That is a fact about an
instrumented schedule. It does not print the cache line those addresses
share, and a run that prints nothing has not surveyed the abstract machine.
Valgrind Memcheck can say the queue's result objects were freed. That is a
fact about the allocator's bookkeeping on that run. It is silent about
modification order.

The intentional data race belongs only on the paths that name it. The
deterministic suite, `scripts/test-week11.sh` in the lab, runs the checked
modes and prints `INFO race mode is intentionally excluded`. Day 2's wrapper
and today's wrapper are the places that invoke `thread_lab race`. That mode
increments a plain `uint64_t` from four threads. [Day 2](day-002-data-races-and-the-c-abstract-machine.md)
already classified it: conflicting non-atomic accesses, no happens-before,
undefined behavior. The wrapper runs it as a bounded specimen. A printed
count, including a count that equals 800000, is part of the specimen. It is
not a test that the source is defined.

## 4. Identity before interpretation

The report starts with a UTC timestamp, `uname -a`, the first line of
`cc --version`, and SHA-256 hashes of `thread_lab`, `queue_lab`,
`deadlock_lab`, and `litmus_lab`. Those hashes name the binaries the later
sections execute. A sanitizer rebuild is a different binary, compiled with
different flags, and the report keeps it in its own section so a stack trace
from that binary is not matched against the hashed uninstrumented file.

Concurrency bugs move when the binary changes. A log line added to "see the
race" changes timing, register allocation, and sometimes whether the compiler
is still willing to keep a value in a register. Capture the report from the
binary you intend to talk about, and if you edit the source, say so and hash
again. Optimization level matters for the same reason: the ASan build in the
wrapper uses `-O1`, the ordinary lab build uses the Makefile's flags, and
Day 5's assembly sample uses `-O2`. Three translations, three identities.

The kernel that executes the lab is whatever `uname` prints. The kernel
*text* quoted on Days 4, 5, and 6 was read from tree `c9b2385d6793`, whose
top-level `Makefile` says version 5.10.134. Those are allowed to differ. A
futex path you traced with `strace` on the host is the running kernel's
path. A comment quoted from `kernel/futex.c` is the tree's comment. Version
discipline is writing both names down.

## 5. Execution trace: one report, top to bottom

What follows is the trace of the wrapper, in the order the sections appear.
It records evidence. The interleaving of the four worker threads is a
separate record, and this wrapper does not claim to have captured it.

```text
section            program / tool              what a careful reader may conclude
-----------------  --------------------------  ------------------------------------
identity           uname, cc, sha256           which host, compiler, and binaries
deterministic      make test-week11            the checked modes printed their
                                               markers, including WEEK11 PASS,
                                               and the race mode was not among them
race               thread_lab race             the UB specimen ran and printed
                                               some count; the count is not a spec
deadlock           deadlock_lab detect         the timed cycle returned; test_hung=no
false-sharing      thread_lab false-sharing    one pair of wall-clock samples
litmus             litmus_lab                  both_zero for this many rounds,
                                               plus the x86-is-not-C-proof caveat
gdb                gdb batch, mode basic       threads and a completed join,
                                               or a skip naming ptrace/symbols
valgrind           memcheck, queue_lab         leak check of that run, or a skip
asan-ubsan         instrumented ownership      that path's shadow checks, or a skip
tsan               instrumented race           a data-race report, a runtime skip,
                                               or a skip that withholds any pass
landmarks          three printed lines         a reminder to pin ToyOS, a teaching
                                               OS revision, and the Linux tree
limits             one printed sentence        no exhaustive schedule, no fairness
                                               proof, no portability from one ISA
```

Read the trace as a stack of scopes. `WEEK11 PASS` means the patterns in
`test-week11.sh` matched: create/join, the mutex counter 800000, the atomic
counter 800000, ownership 42, the queue's while-predicate marker, the
deadlock detector's `test_hung=no`, the ordered lock mode, the false-sharing
caveat string, and the litmus caveat string. Several of those patterns are
checks that a caveat was printed, which is a check that the program admitted
its limit. They are not checks that the padded counter was faster or that
`both_zero` had a required value.

## 6. GDB reads architectural state at a stop

The wrapper runs, when `gdb` is on `PATH`:

```text
gdb -batch -ex 'break mode_basic' -ex 'run basic' -ex 'info threads'
    -ex 'continue'
```

`mode_basic` creates one thread, joins it, and checks that the pointer came
back. The breakpoint stops the process at that function. `info threads`
lists the threads GDB can see at the stop. `continue` lets the process run
to exit, and the wrapper looks for `pthread_create_join=ok` in the log.

Underneath, GDB uses `ptrace`. The kernel marks the task traced, arranges a
stop at the breakpoint (a patched instruction, or a hardware debug register,
depending on the breakpoint), and hands GDB the saved registers. At the stop
you can ask for a backtrace per thread, the instruction pointer, the stack
pointer, and, with debug symbols and a cooperative glibc, the mutex owner
words and condition-wait frames. Those are the architectural and ABI state
of a paused process. They are the same registers Week 2 taught you to track,
frozen at one moment.

The pause is part of the data. A thread that was runnable is now stopped.
Wakeups that would have raced it do not race it while the process is
stopped. A deadlock investigation uses this on purpose: stop everyone, record
for each thread whether it is running, blocked in a futex, or sitting in
userspace, and draw the wait-for graph from Day 6. A single stop that shows
no cycle shows that the cycle was not present at that stop. Other schedules
remain possible. The wrapper's batch session is narrower still. It confirms
that GDB could stop `mode_basic`, list threads, and reach the success line.
It does not walk a deadlocked process. The deadlock evidence in the same
report is the timed-lock return code. This breakpoint does not capture a
deadlocked backtrace.

If `gdb` is missing, or the batch command fails, the wrapper prints a skip
and keeps the diagnostic log. A ptrace policy that rejects the tracer is a
property of the environment. The skip records that no stop-state evidence
was collected. The create/join result from the uninstrumented binary is a
different section and still stands.

## 7. Valgrind Memcheck follows heap ownership

When `valgrind` is installed, the wrapper runs Memcheck on `queue_lab` with
`--leak-check=full`, `--errors-for-leak-kinds=all`, and an error exit code
of 91. Memcheck does not run the binary on the raw CPU. It translates basic
blocks as the process executes and keeps shadow state beside the program's
memory: which bits have defined values, and which heap blocks the allocator
handed out and accepted back.

That is the right tool for the queue's result objects. Each consumer
`calloc`s a `struct result`, returns it through `pthread_join`, and the
parent `free`s it. A clean Memcheck run supports "on this execution, those
blocks were allocated, used, and released, and no leak Memcheck tracks was
left behind." The queue's ring itself is a static object with static
mutexes and condition variables; Memcheck is not being asked to prove the
ring invariant. The ring invariant is the source protocol from Day 4, checked
by the sum and the final `count`.

Memcheck's shadow state is not a happens-before relation. Valgrind's race
detectors are Helgrind and DRD, and the wrapper does not invoke them. A
clean Memcheck log of `queue_lab` is evidence about allocation lifetime on
that run. The data-race specimen is a different program section, and Memcheck
is not the tool the wrapper points at it.

If Valgrind is absent, the skip is the evidence. If it exits 91, the wrapper
keeps the log as `FAIL-EVIDENCE` and does not relabel that as success. Read
the log against the source before changing anything: a leak in the result
path would be a lifetime bug, and a complaint inside the C library's thread
startup would be a tool-and-runtime interaction to record as such.

## 8. ASan and UBSan instrument the translation

The wrapper compiles `thread_lab.c` with `-fsanitize=address,undefined`,
`-fno-omit-frame-pointer`, and `-O1`, then runs the `ownership` mode with
`ASAN_OPTIONS=detect_leaks=1`.

AddressSanitizer is a compiler pass plus a runtime. The pass inserts checks
around loads and stores and asks the allocator for redzones around heap
objects. The runtime keeps a shadow map from addresses to "accessible /
redzone / freed." A load or store whose shadow says the address is
poisoned aborts with a stack trace. Leak detection at exit walks the heap
for blocks with no remaining pointer. The ownership mode is a single
allocation, a handoff through `pthread_create`, a write by the worker, a
join, and a free by the parent. A clean run supports that path under this
instrumentation. It does not run `race`, `false-sharing`, or the queue.

UndefinedBehaviorSanitizer, enabled by the same `-fsanitize=undefined`,
inserts the checks the compiler version knows how to insert: typical sets
cover null references, out-of-bounds shifts, signed overflow in the
patterns the pass handles, and a list that changes with the compiler. A
data race is undefined behavior in the C abstract machine and is not what
this UBSan build is being asked to catch. The race specimen is reserved for
the TSan section. UBSan's clean ownership run means those inserted checks
did not fire on that path.

A failed compilation prints `SKIP: ASan/UBSan instrumentation unavailable`
and keeps `asan-build.log`. A runtime failure is `FAIL-EVIDENCE` with the
log retained. Both are report material. A skip means the layer was not
observed. The uninstrumented ownership check inside `WEEK11 PASS` is the
other layer, the plain binary's assertions, and it remains.

## 9. ThreadSanitizer instruments conflicting accesses

The TSan section compiles `thread_lab.c` with `-fsanitize=thread -O1` and,
if the binary links, runs `thread_lab race` with `TSAN_OPTIONS=exitcode=66`.

The compiler inserts calls on memory accesses and on synchronization
operations the runtime understands: thread create and join, mutex
operations, atomic operations with their memory orders. The runtime records
which thread touched which location and which happens-before edges it has
seen. When it observes a pair of conflicting accesses that those edges do
not order, it reports a data race. That is a direct instrument of the Day 2
definition, for the schedules and the synchronization kinds this runtime
models.

The wrapper then sorts the outcome, and the sort is the lesson:

```text
build failed
  SKIP: TSan instrumentation unavailable

process ran, status 66, and the log contains "data race"
  PASS: ThreadSanitizer detected the intentional race

log matches a fatal runtime / unexpected memory mapping
  SKIP: TSan built but the runtime is unavailable or incompatible

anything else, including a process that exits 0 with no report
  SKIP: no recognized race report (with the status)
  the wrapper's own words: not proof of safety
```

This host may lack `libtsan`. The compiler can also succeed at the link and
then fail at process start because the runtime's memory layout does not fit
the process, which is the "unexpected memory mapping" branch. A skip on
either branch records that the instrumented execution did not produce a
race report. It does not convert the racy source into a defined program,
and it does not satisfy the deterministic suite. The suite already passed
or failed on its own, with TSan out of that recipe. Absence of a report is
not proof that the accesses were ordered.

## 10. One stopped consumer, and the sentence it supports

Picture a GDB stop of `queue_lab` while a consumer sits in `get`. The
wrapper does not perform this stop — its breakpoint is `mode_basic` — but
the picture is the one you will want the moment a real wait looks stuck.

`info threads` lists the tasks in the thread group. `thread apply all bt`
walks each stack. A blocked consumer's frames should include `get`, then
`pthread_cond_wait`, then the libc frame that entered the `futex` syscall
if the call has crossed into the kernel. The instruction pointer is in libc
or in the kernel entry path. The stack pointer addresses that thread's
stack, which remains mapped; Day 4's point about the `while` frame surviving
the sleep is exactly this stack. Printing `queue.count` reads one stable
word because every thread of the process is stopped. A snapshot with `count`
0 and this consumer inside `cond_wait` supports one sentence: at this
instant the ring was empty and this waiter was published. It does not say
whether a producer is about to signal, whether the wakeup that returns will
be spurious, or whether the other consumer will take the next item first.
`continue` ends the snapshot.

The registers at the stop are the architectural registers of each thread,
the same names as in the assembly lessons. If GDB stopped every thread, no
thread of this process is retiring instructions, and the core runs whatever
else the scheduler has. If a future session stops only one thread, the
others keep running and the snapshot is partial. The wrapper's batch stop
is the small case: one extra thread, a join, `info threads`, then
`continue` until `pthread_create_join=ok`. Practice the commands there. Use
the deadlock lab's `ETIMEDOUT` results when the question is the cycle.

An ASan abort is a different kind of stop. A redzone or use-after-free
report means an instrumented load or store touched an address the shadow map
calls inaccessible. The access stack is the faulting instruction. The
allocation stack, when the runtime prints one, is the `malloc` that owned
the neighboring bytes. The ownership mode's wrapper success is the lack of
that abort on the instrumented binary, which is a different file from the
four SHA-256 lines. Name it as the `-fsanitize=address,undefined` build when
you cite it. "No ASan report on ownership" is the whole claim the `PASS`
line earns.

A `PASS` on this section means the opposite of a quiet program. It means
the tool saw the race the source contains. That is the success condition
for an intentional specimen. It is still one instrumented execution. TSan
does not promise to report every race in every schedule, and it does not
understand synchronization it was not told about. A home-grown spin on a
plain `volatile` flag, for example, is not a pthread mutex and is not a C11
atomic release/acquire pair; the runtime may report a race because, in the
model, there is one.

The plain `thread_lab race` line earlier in the report and the TSan section
are two observations of related source. The first is an uninstrumented run
of undefined behavior, useful as a specimen of "a number came out." The
second is an instrumented run whose purpose is to name the conflicting
accesses. Keep both, and do not let either one's exit code rewrite the
other.

## 11. ToyOS: two kernel tasks, cooperative and preemptive

The course kernel at
[projects/toyos/src/tasks.c](../../projects/toyos/src/tasks.c)
is the first system in the comparison. At the milestone that file
implements, `tasks_init` creates two ring-0 tasks with 8192-byte stacks and
prints that there are no user processes yet. Each task has a `struct context`
holding `r15`, `r14`, `r13`, `r12`, `rbx`, `rbp`, `rsp`, and `rip` — the
callee-saved registers plus the instruction pointer
`context_switch` exchanges. The states are `TASK_RUNNABLE`, `TASK_RUNNING`,
and `TASK_BLOCKED`.

One worker calls `schedule` in a loop. That is the cooperative path: the
task gives up the core at a source line it chose. The other worker spins on
`timer_ticks` and is switched by `scheduler_tick`, which the timer interrupt
calls. That is the preemptive path: the task loses the core between its own
instructions. [Week 7's task lesson](../../07-memory-processes-and-scheduling/sections/day-004-kernel-tasks-stacks-contexts.md)
records the transitions that exist: created to runnable to running, running
back to runnable on a switch, running to blocked when the task finishes.
`TASK_BLOCKED` there means finished. There is no wait channel, no condition
predicate, and no wakeup list. `schedule` walks a two-slot array on one CPU
and updates state without a lock, which Week 7 notes is viable because the
interrupt behavior is tightly constrained.

There is no C memory model in this scheduler. The file is freestanding
kernel C. It does not include `<stdatomic.h>`, it does not call
`pthread_mutex_lock`, and the two tasks share one kernel address space with
private stacks. A data race in the C abstract-machine sense is a hosted,
multi-threaded C notion from Days 2 and 5. ToyOS's correctness argument for
these tasks is the one Week 7 uses: explicit states, explicit stacks, a
context record, and a single core. Later milestones add user address spaces
and a small syscall surface. They do not import glibc's pthread condvar or
the C11 order parameters into this scheduler. When you compare, compare
mechanisms that are actually in the milestone you built.

## 12. A teaching kernel's sleep, and Linux beside it

The second system is the small teaching OS the wrapper names: pin a
revision, then read its `proc` structure, its spinlock, and its
`sleep`/`wakeup`. The idea those functions exist to teach, which you should
confirm against that pinned source before quoting it, is Day 4's protocol
living inside the kernel. The lab's version of that protocol lives in glibc.

```text
waiter, holding the spinlock that protects the predicate
  sees the predicate false
  sleep(channel, lock)
      records the channel
      releases the lock
      marks the process asleep
      yields the CPU
waker, holding the same lock
  makes the predicate true
  wakeup(channel)
      marks sleepers on that channel runnable
waiter runs again
  reacquires the lock
  rechecks the predicate
  sleeps again if another thread consumed the event
```

The channel stands in for the condition variable. The spinlock stands in for
the mutex. The recheck stands in for the `while`. Lost wakeup is the same
bug as Day 4's trace: signal (here, `wakeup`) before the sleeper is published
on the channel, with the lock dropped across the gap. The teaching kernel
closes the gap by requiring the lock to be held on entry to `sleep` and by
releasing it inside `sleep` after the sleeper is recorded. Spurious or
stolen wakeups are why the waiter loops. This course does not vendor a
particular xv6 tree, so line numbers belong in your report only next to the
revision you opened. Week 7's comparison lesson lists the usual landmarks
(`proc`, the switch assembly, traps) and asks for that revision explicitly.

The third system is Linux, and it is the one the lab actually runs.

A pthread is a task created so that it shares the process address space,
the file-descriptor table, and the signal dispositions, with a private stack,
a private register set, and thread-local storage. Day 1 separates that POSIX
description from the `clone` flags a given glibc uses. The shared address
space is why the queue's `count` is one virtual address for every thread,
translated by one set of page tables. The private registers are why a
context switch has to save them, which ToyOS does in miniature in
`struct context` and Linux does on the way into `schedule` and at syscall
entry.

When the lab blocks, it blocks in the kernel on a futex. Day 4 quoted
`futex_wait_setup` and `futex_wait` from tree `c9b2385d6793`: the hash-bucket
lock is held across the test of the userspace word, a spurious wakeup retries,
and the task state store that decides to sleep carries a barrier documented
on `set_current_state`. That is Linux's wait path. glibc's `pthread_cond_t`
remains userspace; the futex is the syscall it uses when it needs the
scheduler. The C memory model remains the rule for the lab's own accesses:
the queue is defined because the mutex edges order `count`, the relaxed
counter is defined because the object is atomic, and `thread_lab race` is
undefined because neither is true there. The kernel's `smp_store_release`
mappings from Day 5 are how *this kernel tree* orders its own flag stores on
x86-64, AArch64, and RISC-V. They are not a third implementation of
`memory_order_release` inside the lab binary.

Put the three side by side only at the level each one actually implements:

```text
                    ToyOS kernel tasks     teaching sleep/wakeup     Linux + C lab
------------------  ---------------------  ------------------------  -------------------------
who runs            2 ring-0 tasks         a process table           tasks, pthread via clone
stacks              private kernel stacks  per-process kernel stack  per-thread user stack
preemption          timer tick and yield   a scheduler you pin       the host scheduler
waiting             BLOCKED = finished     sleep on a channel        futex, condvar in glibc
predicate recheck   none on that path      the sleep loop            while around cond_wait
memory model        freestanding, no C11   the OS's own locks        C11/pthreads for the lab
coherence story     one CPU in that code   depends on the port       the host's caches, Day 6
```

The last row records scope. ToyOS at that milestone does
not have a second cache to bounce. A teaching OS ported to several harts
inherits that hart's memory model and has to say so. The lab inherits the
host, and Day 5's point still stands: an x86-64 litmus count is a fact about
the host.

## 13. Version discipline, written as part of the answer

Every comparison sentence in the report should carry the versions that make
it true.

```text
lab binaries        the four SHA-256 lines
host kernel         uname -a from the report
excerpt tree        c9b2385d6793, Makefile version 5.10.134
                    paths quoted on Days 4–6 only
C compiler          cc --version, plus -O1/-O2/sanitizer flags per section
C library           the glibc (or other libc) that provided pthread and,
                    if TSan ran, the sanitizer runtime
ToyOS               the course tree's tasks.c at the milestone you built
teaching OS         revision, architecture, and the files you actually opened
tools               gdb and valgrind versions when they ran; the skip text
                    when they did not
```

A sentence such as "Linux waits the way ToyOS waits" fails this test on its
face: ToyOS's blocked state in the scheduler milestone is completion, and
Linux's futex wait is a published waiter plus a recheck. A sentence such as
"the kernel uses `stlr` for release" fails it unless the architecture is
AArch64 and the tree is the one whose `arch/arm64/include/asm/barrier.h` you
are looking at. On the x86 header of the same tree, store-release is a
compiler barrier and a plain store. The report is where those qualifications
live, next to the hashes.

Tool absence follows the same rule. A skip is a recorded observation of the
environment. The wrapper's TSan skip says the instrumented race was not
shown. The race in the source is unchanged. Fill the comparison and the
limits section from the source you read and the logs you have. Leave a blank
row blank. A stack trace belongs to the binary that produced it, and a line
number belongs to the revision you opened.

## 14. Mastery: Explain · Draw · Observe · Build

### Explain

- Which question GDB answers at a stop, which question Memcheck answers
  about `queue_lab`, and which question TSan answers about `thread_lab race`.
- Why a TSan skip, including a missing `libtsan` or a quiet exit, does not
  clear the data race, and why a TSan report on the intentional specimen is
  the section's success.
- Why `WEEK11 PASS` can be true in the same report as an intentional race
  specimen.
- How ToyOS's `TASK_BLOCKED` differs, in the milestone you can point at,
  from a teaching-OS `sleep` and from a Linux futex wait.
- Which two kernel identities a careful report names: the running `uname`,
  and the excerpt tree `c9b2385d6793`.

### Draw

Draw the four-layer picture from section 3 and place each tool on the arrow
it observes. Draw the three-system table from section 12 with one extra
column you fill only from files you opened: a function name for ToyOS
(`schedule` or `scheduler_tick`), a function name from the pinned teaching
OS, and `futex_wait` from the excerpt tree.

### Observe

Run
[challenges/day-007-debug-report.sh](../challenges/day-007-debug-report.sh)
and keep the temporary report path it prints. In your notes, copy the
identity block, the `WEEK11 PASS` line, the race line, the deadlock line,
every `PASS` / `SKIP` / `FAIL-EVIDENCE` line, and the limits line. If TSan
skipped, quote the skip text and write what it establishes about the tool
and what it leaves unchanged about the source.

### Build

Write the debugging report the wrapper's landmarks section asks for, as
prose around those logs. Include the binary hashes, the layered tool
results, the three-system comparison at the resolution of section 12, and an
explicit list of what this run does not prove: unexecuted schedules,
fairness, a kernel you did not boot, and a C behavior inferred from one
ISA's litmus count. The report is the deliverable. Completing the wrapper
is the evidence underneath it, and the week's mastery is the explanation
you can give from that evidence, which remains yours to finish.
