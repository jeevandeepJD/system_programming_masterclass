# Day 2 — Fork, Copy-on-Write, and Parent–Child State

**Target time:** approximately 3 hours

> `fork` creates a new task and a new return path. “Copies the process” is a
> useful prediction only after every resource is classified as copied, shared,
> reference-counted, or reset.

## 1. Run the lineage experiment first

Run `challenges/day-002-fork-cow.sh`. Predict that the canonical `tree` mode
will report two distinct child PIDs, each naming the wrapper program's child
as parent, and that the complete Week 10 test will still pass. Predict also
that the source will show a pipe created before either fork.

The pipe is deliberate. Output ordering from multiple processes is unstable,
so each child writes one fixed-size record to a kernel pipe and exits. The
parent reads two records and waits until no child remains. The invariant is
the relationships in each record, not which child reports first.

## 2. One call, two returns

On success, `fork()` returns once in each resulting process. The parent
receives the child's positive PID; the child receives zero. On failure there
is no child and the caller receives `-1` with `errno` set. Correct C code tests
failure before branching on zero.

Both tasks resume from the logical point after `fork`, but they are not
executing one shared C stack. Each has its own virtual address space view and
register state. Local variables initially contain the same bytes, including
copies of pipe descriptor integers, yet subsequent ordinary writes in one
address space are private unless the memory was explicitly shared.

The scheduler chooses which task runs first. Parent-first output is not a
contract. A successful run that happened to print children in index order
does not prove ordering.

## 3. Classify state precisely

A useful fork ledger is:

```text
property                          relationship after fork
PID/task identity                 new and distinct
PPID relationship                child points to parent
user virtual mappings             logically copied, usually COW
page frames                       initially shareable until writes
descriptor table entries          copied entries
open-file descriptions            referenced/shared
file offset/status flags           shared through open description
descriptor FD_CLOEXEC bit          copied per descriptor
signal dispositions               inherited
blocked signal mask               inherited
pending signals                   child starts with none
credentials/cwd/root/umask         inherited via kernel rules
CPU usage and selected timers      reset or separately accounted
fork return value                  deliberately different
```

Consult `fork(2)` for the complete application contract. Linux has extensions
and corner cases, and multithreaded fork adds severe restrictions before
exec. A compact table should never masquerade as the full specification.

## 4. Copy-on-write is an implementation strategy

Eagerly copying every writable page would be expensive, especially when the
child immediately calls `execve`. Linux normally maps parent and child to the
same physical pages read-only or with equivalent write-protection accounting.
When either writes, a fault lets the kernel create a private writable copy.
This is copy-on-write (COW).

COW means “same initial contents, private write semantics,” not “one shared C
object.” It can be defeated or altered by explicitly shared mappings. Kernel
page deduplication, huge pages, pinning, and memory pressure complicate the
physical story. The API guarantee is the process-visible memory behavior; the
exact frame choreography is implementation evidence.

The canonical lab does not inspect page-frame numbers or minor-fault counters.
Its `tree` mode demonstrates lineage and inherited pipe access, not COW page
allocation. The wrapper prints source landmarks and labels COW as a reasoned
implementation comparison.

## 5. Why the pre-fork pipe works

Before forking, the parent creates one pipe with read and write descriptors.
Each child inherits entries referring to the same pipe endpoints. It closes
the read endpoint, writes a complete record with a checked loop, closes its
write endpoint, and calls `_exit`.

The parent closes its write endpoint before reading. This is vital lifetime
discipline: a read waiting for EOF would never see EOF while any write
reference remains open. After two exact-size reads, it closes the read end and
waits until `wait` reports `ECHILD`.

`checked_write` retries `EINTR` and handles partial writes. `checked_read`
retries interruption, rejects premature EOF, and advances by the number of
bytes actually transferred. Even though a small pipe record is normally
atomic relative to `PIPE_BUF`, robust code still checks the syscall result.

The child uses `_exit`, not `exit`. After fork, copied stdio buffers may
contain bytes buffered before the split; `exit` could flush them again. In
larger programs, inherited `atexit` handlers and libc state are another
reason the pre-exec child path should stay narrow.

## 6. Parent and child are relationships, not ownership

The parent does not “own” all child memory or files. The kernel records
relationships needed for signals, wait, and reparenting. The child has an
independent PID and can outlive the original parent. Permissions for
inspection or signaling depend on credentials, namespaces, ptrace policy, and
other checks, not simply ancestry.

After a child terminates, selected identity and status remain so an eligible
waiter can consume them. Day 4 names that state a zombie. Parent/child
relationships therefore have a post-execution lifetime as well as a creation
path.

## 7. Failure and cleanup paths

Fork can fail because of process limits, memory pressure, cgroup limits, or
kernel constraints. In a loop, failure after one successful child creates a
partial-construction problem: the parent must close endpoints and reap
already-created children. The compact lab exits on fork failure and is aimed
at a controlled environment; production code should preserve enough state to
clean up partial success.

Every descriptor should have a clear owner in every branch. Every child
should have a known reaper. Avoid C patterns that duplicate cleanup across
many branches; structured labels can be safer if they preserve the first
failure and never close an uninitialized descriptor.

## 8. Linux source landmarks

For a recorded source revision inspect `kernel/fork.c`, especially the
creation path around `kernel_clone`/`copy_process` as named in that revision.
Memory duplication work is under `mm/`, while descriptor-table operations
are under `fs/` and signal structures under `kernel/signal.c`. Look for the
flags that determine whether a resource is shared or duplicated rather than
assuming every task creation is ordinary `fork`.

Linux libc may implement `fork` using a clone-family syscall. `strace` may
therefore display `clone` or `clone3` rather than a literal `fork`. That is not
a contradiction; it is a distinction between the C API and kernel entry used
by that libc/kernel combination.

## 9. ToyOS and xv6 comparison

ToyOS in this course has a deliberately smaller process story; verify its
current milestone before claiming fork or COW support. If absent, compare an
absence, not imagined code.

xv6-riscv's `kernel/proc.c` contains educational process allocation, fork,
exit, and wait paths. Its page-table copying policy depends on the exact
revision; classic teaching versions eagerly copy user pages, while COW labs
change that behavior and add reference counting and fault handling. Record
the repository commit and whether the COW lab is applied.

Linux supports richer sharing through clone flags, threads, namespaces,
pidfds, cgroups, overcommit policy, and many memory-mapping cases. More
features mean more lifetime states, not a different basic need to classify
each resource.

## 10. Write the causal account

Explain the canonical run without saying merely “fork copied the parent”:

1. the root created a pipe;
2. each successful fork created a distinct PID and return value;
3. inherited descriptor entries referenced the pipe;
4. each child sampled PID/PPID into a record and used checked `write`;
5. children closed references and `_exit`ed;
6. the parent consumed two records, closed its endpoint, and reaped all;
7. record order remained unconstrained.

Then separate observed evidence from expected COW implementation. If you want
to claim actual faults or page sharing, design another bounded measurement
with matching-kernel facilities; today's output is insufficient.

## Practical challenge

**Purpose:** classify parent/child identities and inherited resources without
turning “copy” into an inaccurate blanket statement.

**Prediction:** two unique child PIDs report the same parent PID; records may
arrive in either order; all children are reaped.

**Evidence:** canonical source, checked `tree` output, process-related trace
when `strace` is present, project test marker, and artifact identity.

**Limits:** the exercise does not measure physical pages, prove write-fault
COW, cover multithreaded fork, inject partial-loop fork failure, or establish
scheduler ordering.
