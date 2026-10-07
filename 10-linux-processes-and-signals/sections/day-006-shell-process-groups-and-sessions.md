# Day 6 — Shell Execution, Process Groups, Sessions, and Job Control

**Target time:** approximately 3 hours

> A shell is a process orchestrator. Job control adds process-group, session,
> and controlling-terminal policy; it is not merely fork plus exec.

## 1. Use the safe bounded lab

Run `challenges/day-006-shell-job-control.sh`. Predict that the wrapper can
report its own PID, parent, process group, session, and whether standard input
is a terminal. It then runs the canonical fixed fork/exec/wait status mode and
uses a short-lived Python helper to create and verify one child process group.
All descendants are waited for.

The experiment does not call `setsid`, seize a controlling terminal, use
`tcsetpgrp`, stop an interactive job, or implement a shell. Those operations
can disrupt the IDE terminal. The lesson builds the model while keeping the
runtime observation non-interactive and bounded.

## 2. A minimal command launch

For a simple external command, a shell conceptually parses input, performs
expansion, prepares redirections, forks or uses a spawn facility, adjusts the
child's descriptors and process-group membership, execs the target, then
waits or records a background job.

Each verb has failure and lifetime consequences. A pipe must exist before the
children that inherit it. Redirection opens can fail before exec. Unused pipe
ends must close in every process. Exec failure must be distinguished from the
new command's exit. The parent must retain enough identity to wait and report
status.

Builtins complicate the model. `cd` must affect the shell's own cwd, so
executing it only in a child would not implement normal shell semantics.
Pipelines, compound commands, substitutions, and traps add further rules.
No full shell is required this week.

## 3. Process groups

A process group is a set of processes identified by a process-group ID
(PGID). It lets a shell and terminal direct a signal at a job containing a
pipeline rather than one PID. Group membership is not the same as ancestry:
siblings and descendants can be arranged into one job group under documented
constraints.

`setpgid` is race-sensitive because both shell and child run concurrently.
Robust shells arrange calls so the desired grouping succeeds regardless of
which side runs first, while handling expected race outcomes carefully.
Today's helper creates one child, has the child call `setpgid(0,0)`, reports
its IDs over a pipe, and has the parent verify/reap it. That demonstrates a
group leader whose PID equals PGID; it does not claim pipeline-grade race
handling.

Signals can target a process group using APIs with the documented negative
PID convention or terminal-generated delivery. Never experiment with broad
negative targets in an uncontrolled environment. The wrapper sends no
job-control signals.

## 4. Sessions and controlling terminals

A session contains one or more process groups. A session leader can acquire a
controlling terminal under system rules. The terminal tracks one foreground
process group. This gives the kernel terminal driver a destination for
interactive signals such as `SIGINT`, `SIGTSTP`, and `SIGQUIT`.

Background groups that attempt terminal reads can receive `SIGTTIN`;
terminal-mode-dependent background writes can receive `SIGTTOU`. When the
foreground job stops or exits, an interactive shell regains terminal
foreground ownership and restores terminal modes.

PID, PPID, PGID, SID, and foreground PGID answer different questions:

```text
PID   which task/thread-group identity?
PPID  which current parent relationship?
PGID  which job-oriented process group?
SID   which session?
tpgid which group currently owns terminal foreground access?
```

A non-interactive IDE task may have no controlling terminal on stdin. That is
a valid result, not a failed lab. `/proc/self/stat` exposes identifiers, while
`isatty` and terminal APIs establish whether a descriptor is a terminal.

## 5. Pipelines and descriptor lifetime

For `producer | consumer`, the shell creates a pipe, starts both commands in
one job group, maps producer stdout to the write end and consumer stdin to the
read end, and closes all unrelated copies. If the shell retains a write end,
the consumer may wait forever for EOF. If a child inherits unrelated
descriptors across exec, resources and privileges leak.

Use atomically close-on-exec descriptors where available. After `dup2`, close
the original endpoints. Treat every launch as a transaction with partial
success: if the second fork fails, terminate/reap or otherwise manage the
first child and release descriptors.

Exit reporting for pipelines is shell policy. The last command, `pipefail`,
signal termination, asynchronous notifications, and stopped/continued states
are not encoded by one universal number. Keep kernel wait facts separate from
shell presentation.

## 6. Foreground waiting without races

An interactive shell should not block blindly on one PID when a job contains
multiple processes and can stop or continue. It tracks members, waits on the
group with suitable options, updates job state, and handles `SIGCHLD` without
unsafe handler work or lost wakeups.

A self-pipe or synchronous signal-wait design can integrate child changes into
an event loop. The handler should not mutate complex job tables. Signal masks
must close the race between checking job state and sleeping. This connects
Day 5's notification discipline to shell orchestration.

The canonical lab checks only simple exit and signal termination. It neither
handles `WUNTRACED`/`WCONTINUED` nor maintains a job table, so do not claim
those paths were tested.

## 7. Failure boundaries

Name the earliest detector:

- parse/expansion error: shell before process creation;
- pipe/open failure: launcher setup;
- fork/spawn failure: launcher, possibly after partial construction;
- process-group race: `setpgid` caller;
- exec failure: pre-exec child, communicated to parent;
- program exit/signal: wait status;
- terminal ownership error: `tcsetpgrp` and session/terminal rules.

Never collapse all failures to “command not found.” Preserve `errno` and
context, but ensure child diagnostics cannot deadlock on inherited stdio locks.
The project's fixed ENOENT mapping is one contained example.

## 8. Linux source and comparison

Pinned Linux landmarks include `kernel/fork.c`, `kernel/exit.c`,
`kernel/signal.c`, and tty code under `drivers/tty/`, with UAPI definitions in
headers. User-space shell semantics belong to the shell's source and POSIX,
not solely the kernel. Record shell version as well as kernel version when
investigating interactive behavior.

xv6-riscv's shell is intentionally small and lacks Unix job control; examine
`user/sh.c` in the chosen revision. Its fork/exec/pipe/wait structure is
excellent for dataflow but not evidence of sessions or terminal foreground
groups. ToyOS's fixed user launch is smaller still.

Linux provides the process/session/tty mechanisms, while Bash, dash, zsh, and
other shells choose parsing and user-facing policy. Do not attribute shell
syntax to a CPU architecture or kernel syscall entry path.

## 9. Write the bounded report

Record the observed PID/PPID/PGID/SID and terminal result. Explain why the
current environment may already place the wrapper in a group created by
Cursor's launcher. Then trace the canonical status child through fork, fixed
exec, wait, and decode. Finally explain the helper's child-led process group
and guaranteed reap.

List what a real interactive shell still needs: parser and expansion rules,
redirections, pipelines, race-safe group setup, controlling-terminal handoff,
termios save/restore, job table, `SIGCHLD` integration, builtins, and shutdown
cleanup.

## Practical challenge

**Purpose:** connect simple fork/exec/wait launch to the identities required
for job control without modifying the active terminal.

**Prediction:** the fixed command exits 7; one helper child becomes a process
group leader; terminal availability depends on the invoking environment.

**Evidence:** checked canonical output, self-only proc/session observations,
one bounded child-group report, source, and full descendant cleanup.

**Limits:** no full shell, parser, pipeline, session creation, terminal
foreground transfer, stopping/continuing, background I/O, or signal broadcast
is performed.
