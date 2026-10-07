# Day 1 — Threads, Processes, `clone`, Stacks, and TLS

**Target time:** approximately 3 hours

> A thread is not “a small process.” Start from which resources are shared,
> which execution state is private, and which lifetime event makes access safe.

## 1. Run before naming the machinery

Run `challenges/day-001-thread-model.sh`. Before running it, predict the one
stable line from `thread_lab basic`: creation succeeds, the worker returns the
same pointer it received, and `pthread_join` gives that pointer back to the
creator. Predict nothing about numeric thread IDs or scheduling order.

The mode passes the address of `token`, an automatic object in `mode_basic`, to
the worker. This is safe only because the creating thread remains inside that
function until after the join. The join is both a lifetime decision and a
synchronization event. If the function returned, or if the object belonged to
a detached thread's expired caller frame, dereferencing the pointer would be
invalid even if the bytes appeared unchanged.

Record the binary hash, compiler version, architecture, and kernel release.
These identify the artifact and environment. They do not turn a single run
into a universal scheduling claim.

## 2. Relate a pthread to a process precisely

POSIX describes threads within a process. Threads share the process address
space and many process resources: mapped code and data, heap objects, open
file descriptions reached through descriptors, signal dispositions, current
working directory, and credentials. Each thread has its own execution context:
registers, stack, scheduling state, blocked signal mask, `errno`, and
thread-specific data.

“The heap is shared” is shorthand, not a safety property. Sharing means each
thread can address the same allocated object. It does not establish ownership,
object lifetime, synchronization, or permission to perform conflicting
accesses. A pointer being reachable by two threads is exactly where a design
must state who may read, mutate, and free it.

A process can have one thread or many. Creating a pthread does not create an
independent POSIX process with a new address space. Conversely, Linux exposes
tasks as a flexible implementation concept; resource-sharing choices make
tasks behave as threads or process-like children. Keep the POSIX contract
separate from Linux's internal representation.

## 3. Build `clone` intuition without replacing the API

On Linux, libc commonly creates pthreads using a `clone`-family system call
with flags that request sharing of the virtual memory, file-descriptor table,
signal-handler table, and thread-group identity, plus thread-lifecycle
coordination. Exact flags, syscall spelling (`clone` or `clone3`), setup calls,
and TLS details vary by libc, kernel, architecture, and version.

This is useful intuition: Linux task creation is parameterized resource
sharing, not two unrelated primitives implemented from nothing. It is not an
application contract. Correct C should use `pthread_create`, `pthread_join`,
and documented synchronization rather than calling raw `clone` based on one
trace. A trace also shows a decoded syscall boundary, not every libc action or
kernel-internal branch.

Compare with `fork`: after `fork` in a multithreaded process, only the calling
thread exists in the child, and until `exec` the child is severely constrained
in what it may safely call. A pthread creation instead adds concurrent
execution inside the shared process. Copy-on-write after `fork` and shared
address space between pthreads are different semantics.

## 4. Understand the per-thread stack

Every thread needs stack storage for call frames, automatic objects, saved
registers, and implementation bookkeeping. Stacks occupy distinct virtual
address ranges in the same address space. “Private stack” means the runtime
assigns it to one thread's execution; it does not mean hardware makes its
addresses inaccessible to peers. If one thread publishes a pointer into its
stack, another can often dereference it while the object remains alive.

That capability is dangerous. The pointed-to automatic object's lifetime ends
when its block exits. The entire thread stack ceases to support its old frames
when the thread terminates. `pthread_join` waits for termination, but joining
does not revive expired automatic objects. A worker must not return a pointer
to its own local variable. Safe patterns include caller-owned storage whose
lifetime spans the join, heap allocation with an explicit transfer protocol,
or copying a result into synchronized shared storage.

Stack size is finite and configurable. Deep recursion or large automatic
arrays can exhaust a worker stack. Measuring the main thread's stack limit
does not necessarily identify every pthread's configured size. Guard pages and
allocation policy are implementation details worth observing but not assuming.

## 5. Distinguish TLS from ordinary globals

Thread-local storage gives each thread a distinct instance of an object
declared with C's `_Thread_local` or provided through pthread-specific keys.
The declaration can look global while the storage is per-thread. Addresses,
initialization, dynamic-loader support, and access sequences depend on the ABI
and model selected by the toolchain.

TLS solves “one instance per thread”; it does not solve arbitrary cross-thread
communication. Publishing a pointer to one thread's TLS reintroduces lifetime
and synchronization questions. Destructors for pthread keys run under defined
termination rules, but code must tolerate iteration limits and destructor
ordering constraints. `errno` is the familiar conceptual example: one
thread's failed operation must not overwrite another's diagnostic state.

Contrast four locations:

- a global counter is one shared object and needs synchronization for writes;
- a heap allocation is shared if its pointer is shared, with ownership chosen
  by the program;
- a stack local is tied to block/thread lifetime even if its address escapes;
- a TLS object has a distinct instance and thread-related lifetime.

Storage location alone never supplies a complete ownership policy.

## 6. Treat creation and join as synchronization

Actions sequenced before a successful `pthread_create` are visible to the new
thread under the pthread synchronization contract. Termination and successful
join establish the corresponding completion relationship: the joiner can
consume results produced by the finished thread. This is why the basic mode
does not need a mutex around `token`: the creator initializes it before
creation, the worker changes no shared state, and the creator checks only
after join.

If both threads modified `token` concurrently, creation and join would not
order the middle of their executions. “I eventually join” does not legalize a
race that occurred before the join. Synchronization must order every
conflicting non-atomic access.

Join also reclaims thread resources and documents one owner of the returned
value. A joinable thread should be joined exactly once. A detached thread
cannot later be joined, so any data it uses must outlive its asynchronous work
through another protocol. `pthread_t` is an opaque handle; do not print or
compare it as though it were a portable integer, and do not confuse it with a
Linux TID.

## 7. Debug lifetime before timing

When a threaded program intermittently fails, first draw an object-lifetime
table: allocation, publication, permitted users, synchronization, final use,
and destruction. Then draw the happens-before graph. Only after both are
coherent should scheduling or performance become the primary hypothesis.

Typical failures include passing the address of a loop variable to several
workers, freeing a task immediately after enqueueing it, returning a stack
address, destroying a mutex while a waiter may still reference it, or letting
`main` return while detached workers retain pointers into process state.
These are C lifetime errors or protocol errors, not mysterious pthread
behavior.

## Practical challenge

**Purpose:** connect pthread creation and joining to Linux task intuition,
shared process resources, per-thread stacks/TLS, and explicit object lifetime.

**Prediction:** the canonical basic and ownership modes report checked values;
artifact metadata varies, but no output ordering is required.

**Evidence:** exact source ranges, binary identity, program assertions, and an
ownership timeline from initialization through join and free.

**Limits:** no raw-clone programming, stack exhaustion, detached-thread race,
TLS ABI disassembly, scheduling guarantee, or proof that one run covers every
lifetime error.
