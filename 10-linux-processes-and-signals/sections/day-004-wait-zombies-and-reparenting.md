# Day 4 — Wait Status, Zombies, Orphans, and Reparenting

**Target time:** approximately 3 hours

> Exit ends execution, not every trace of process identity. A waiter must
> consume status before the kernel can release the retained child record.

## 1. Observe, then always reap

Run `challenges/day-004-wait-zombies-orphans.sh`. Predict three outcomes:
normal exit 7 and signal termination remain distinguishable; a child reaches
`Z` briefly before its parent reaps it; and an orphaned grandchild is
reparented to the lab's subreaper rather than relying on the host's PID 1.

The zombie mode polls only a child it created, for at most 100 one-millisecond
delays. Once state `Z` is observed, it immediately calls `waitpid` and checks
exit 23. This is a controlled lifetime demonstration, not permission to leave
zombies behind.

## 2. Wait status is encoded

`waitpid` returns a PID on success, zero only with suitable nonblocking
options when no selected child has changed state, and `-1` on error. The
integer status is encoded. Test the corresponding predicate before using an
extractor:

- `WIFEXITED(status)` before `WEXITSTATUS(status)`;
- `WIFSIGNALED(status)` before `WTERMSIG(status)`;
- `WIFSTOPPED(status)` before `WSTOPSIG(status)`;
- `WIFCONTINUED(status)` for a continued child where supported/requested.

Do not compare raw status to an exit code. Do not call `WEXITSTATUS` for a
signaled child. `waitid` offers a different interface and `siginfo_t`; choose
an API deliberately rather than mixing representations.

`waitpid` can be interrupted. Whether to retry depends on the program's event
model. Waiting for one known child should also verify that the returned PID is
the requested PID, as the canonical status mode does.

## 3. Zombie is retained status, not running code

When a child exits, its user execution is over and most resources are
released. Linux retains enough task identity, accounting, and status for an
eligible parent to wait. That retained state appears as zombie state. It is
not consuming CPU and cannot resume ordinary execution.

The parent creates the zombie by delaying wait, not because the child forgot
to clean itself. The cure is a correct reaping policy. Signals alone do not
“kill a zombie”; execution has already ended.

Polling `/proc/PID/stat` requires care because the command name is enclosed in
parentheses and can contain awkward characters. The lab's generated child has
a controlled name, so its compact `fscanf` is acceptable only within that
scope. General parsers should locate the final closing parenthesis according
to the procfs format and handle PID reuse.

## 4. Orphaning and reparenting

An orphan is a process whose original parent has terminated while it still
exists. Linux reparents it to an eligible child subreaper or ultimately the
namespace's init process. Reparenting supplies a task that can receive status
and reap later; it does not terminate the orphan.

Containers make “PPID becomes 1” an unreliable teaching assertion. The
canonical mode calls `prctl(PR_SET_CHILD_SUBREAPER, 1)`, then creates a middle
child and grandchild. A readiness pipe ensures the grandchild records the
middle PID before the middle exits. The grandchild waits boundedly for PPID
change, reports before/after IDs, and exits. The top process waits for both
descendants and verifies adoption.

This is Linux-specific. Subreapers are used by supervisors to contain
daemonizing descendants, but a production supervisor also needs signal
forwarding, shutdown policy, repeated nonblocking waits, and race handling.

## 5. Lifetime and C discipline

Every successful fork adds a potential wait obligation. Maintain a ledger:

```text
middle child       created → exits → status consumed by original parent
grandchild         created → orphaned → adopted → report read → reaped
pipe read end      parent owns until exact report arrives
pipe write end     descendant owns until report or failure
```

Close unused endpoints immediately. Otherwise an EOF-dependent reader can
block because it unknowingly owns a remaining writer. Use fixed-size records
with checked partial I/O. Preserve `errno` before cleanup if the diagnostic
depends on it.

The lab's failure mode waits for one child, then calls `waitpid` again and
requires `-1/ECHILD`. This injected failure proves the status was consumed and
there is no waitable child. It does not prove behavior for every PID-selection
option.

## 6. Races and better handles

Process exit, `/proc` lookup, signal delivery, and PID reuse can race. A PID
read earlier can later name no process or a different process after reuse.
Linux pidfds provide stable references for selected operations and integrate
with polling. They are an important production tool but absent from this
portable C17/POSIX-centered lab.

Avoid “check then act” logic that assumes a `/proc` state remains true. The
zombie observation is evidence at one instant; `waitpid` is the authoritative
consumption operation. Similarly, `kill(pid, 0)` is a permission/existence
probe at an instant, not a durable reservation.

## 7. Linux, xv6, and ToyOS landmarks

For a pinned Linux source revision, inspect `kernel/exit.c` for exit,
notification, reparenting, and wait paths, plus `fs/proc/array.c` for task
state formatting. Names and decomposition move between releases. Record
`uname -r`, source tag, architecture, and config.

xv6-riscv's `kernel/proc.c` provides a much smaller exit/wait and parent
relationship under its process-table locking rules. Verify whether a given
revision reparents to `initproc` and how status is copied. ToyOS may have one
fixed user process and no general wait tree; state the actual absence.

Linux adds threads, ptrace relationships, namespaces, subreapers, job-control
states, richer wait selection, and many concurrency constraints. The shared
invariant is that termination status has an owner and a consumption point.

## 8. Explain the evidence

For the zombie, narrate fork, child `_exit(23)`, retained status, `/proc`
observation, `waitpid`, guarded status decode, and release. For reparenting,
narrate both pipes, readiness ordering, middle exit, new parent selection,
report, both waits, and cleanup.

Keep three labels distinct: **exited** describes execution; **zombie**
describes retained waitable state; **orphan** describes a changed parent
relationship. A process can become orphaned while running, and an exited
child can be a zombie without being an orphan.

## Practical challenge

**Purpose:** decode child transitions and prove that every created descendant
has a bounded cleanup path.

**Prediction:** state `Z` is observed then reaped; the grandchild changes from
the middle PID to the subreaper PID; double wait reports `ECHILD`.

**Evidence:** checked canonical modes, selected `/proc` state, source,
process-only trace when available, and complete Week 10 regression.

**Limits:** no PID-reuse stress, pidfds, ptrace stops, multithreaded waiting,
namespace-init behavior, or production supervisor loop is demonstrated.
