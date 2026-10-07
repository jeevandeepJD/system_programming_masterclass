# Week 10 — Linux Processes and Signals

**Format:** seven daily sections, each a mechanism lesson with one bounded
wrapper in [`projects/linux-system-programming`](../projects/linux-system-programming/)

**Suggested workload:** approximately 21 focused hours, adjustable to mastery

> A program file is bytes. A process is a kernel task that owns an address
> space, a file table, credentials, and a place in a parent/child tree.
> This week follows that task from creation through image replacement,
> death, signals, and the names a shell needs so a terminal can interrupt
> a job. The last day asks `/proc`, strace, and GDB to show the same
> objects, and compares them with ToyOS and xv6.

The excerpts in Days 4–7 come from the local kernel tree `c9b2385d6793`,
a 5.10.134-based source. Function names, call order, and line numbers
move between releases. Record `uname -r` beside that id before treating
a line as true of the machine you are running. The POSIX and man-page
contracts are what a C program may rely on. A function body is one
implementation of that contract, in one tree. Mastery stays open until
you produce the evidence yourself. These notes leave completion,
confidence, and hours blank.

## Why the seven days are one mechanism

Week 1 ended at exceptions and privilege. Weeks 2 and 3 gave that
stream a C abstract machine and an ELF image. Weeks 6–9 built one ToyOS
address space, an `iretq` back into it, one user syscall path, and a
small virtual filesystem. Linux gives each task its own address space
and keeps a kernel record after that space is gone, because another task
may still need the exit word.

```text
ELF file on disk
    │  execve maps segments, builds the initial stack
    ▼
task_struct: pid, mm, files, creds, signal state
    │  fork duplicates the task; pages stay shared until a write
    ▼
child task, same open-file descriptions, private address space
    │  execve replaces the image and keeps the pid
    ▼
new ELF running under the old pid
    │  exit stores a word in the task and drops the mm
    ▼
EXIT_ZOMBIE until wait4 copies the word into the parent
    │  a signal can divert a live task
    ▼
pending bit → get_signal → architecture frame → user handler → rt_sigreturn
    │  a shell groups tasks so one signal names a job
    ▼
PGID inside a SID, one foreground group on the controlling terminal
    │  the same fields, printed or stopped
    ▼
/proc text, TASK_TRACED under strace or GDB
```

Days 1–3 establish the live task. Days 4–7 follow it after that picture
gets interesting: the exit word, the asynchronous entry, the job name,
and the tools that read the kernel objects back.

### Day 1 — the task is more than the file

[sections/day-001-process-mental-model.md](sections/day-001-process-mental-model.md)

An ELF file has no instruction pointer and no credentials. A process is
the kernel's `task_struct` plus the memory descriptor, file table, signal
state, and credentials it references. The pid is a name in a pid
namespace. `/proc/self` is a generated view of that task.

### Day 2 — fork copies a context and shares the pages until a write

[sections/day-002-fork-cow-and-lineage.md](sections/day-002-fork-cow-and-lineage.md)

`fork` creates a second task. Descriptor tables are separate; the open
file descriptions they point at are shared, so an offset can move under
both processes. User pages are copy-on-write. The parent/child link this
creates is the link Day 4's `wait` and reparenting walk.

### Day 3 — exec replaces the image and keeps the identity

[sections/day-003-exec-elf-and-inheritance.md](sections/day-003-exec-elf-and-inheritance.md)

`execve` unmaps the old image, maps the new ELF, builds a new user stack,
and starts at the new entry. The pid stays. Close-on-exec descriptors
disappear across that boundary. Signal dispositions reset according to
the exec rules. The exit word Day 4 stores describes this image, which
may be a program the parent did not originally exec.

### Day 4 — the exit word lives in the task

[sections/day-004-wait-zombies-and-reparenting.md](sections/day-004-wait-zombies-and-reparenting.md)

`do_exit` stores `exit_code`, drops the address space, and `exit_notify`
sets `EXIT_ZOMBIE` until a parent collects the word through `wait4`. The
word is kernel state. The program that produced it has already been
unmapped. If the parent exits first, `forget_original_parent` gives the
child a new `real_parent`: another thread, a child subreaper, or the
namespace init. The lab adopts a grandchild with `PR_SET_CHILD_SUBREAPER`
so the new parent is the lab process.

### Day 5 — a handler is user code entered asynchronously

[sections/day-005-signals-masks-and-safe-handlers.md](sections/day-005-signals-masks-and-safe-handlers.md)

`__send_signal` posts a pending bit. A blocked signal stays pending.
`get_signal`, on the way back to user mode, dequeues an unblocked signal
and either runs a handler, ignores it, or takes the default action. The
handler is ordinary user code, so it may call only functions that cannot
touch inconsistent libc state. The self-pipe moves the event onto the
program's normal path. The frame that makes the handler look like a call,
and the `rt_sigreturn` that resumes the interrupted registers, are built
by architecture code. x86-64, AArch64, and RISC-V each have their own.

### Day 6 — a job is a process group inside a session

[sections/day-006-shell-process-groups-and-sessions.md](sections/day-006-shell-process-groups-and-sessions.md)

A shell is a loop of `fork`, `exec`, and `wait`, plus group identity so
the terminal can signal every member of a pipeline. `setpgid` moves a
task between groups. `setsid` points the session id and the group id at
the caller's pid and clears the controlling terminal. One group is the
foreground group; keyboard signals go there. Process groups are kernel
policy, the same idea on every ISA. The lab records ids and one child
group leader. An interactive shell, `setsid`, and `tcsetpgrp` stay off
the terminal you are using.

### Day 7 — read the objects back, and compare

[sections/day-007-proc-strace-gdb-comparison-report.md](sections/day-007-proc-strace-gdb-comparison-report.md)

`proc_pid_status` prints `task_struct` fields when you read the file.
`Z` is `EXIT_ZOMBIE`. A ptrace stop is `TASK_TRACED`. strace is a tracer
that prints syscalls from that stop. GDB is the same stop plus ELF
symbols, so a breakpoint at `main` is a symbol address after startup has
already run. ToyOS has one address space and an explicit marker. xv6 has
fork, exec, and wait in a small process table. This Linux tree has the
task, the exit word, the signal queues, the session, and the generated
`/proc` text. The report keeps those three apart, and it records
`uname -r` before any line number.

## How the lab stays inside one process family

The project is an offline C17/POSIX lab. Wrappers in
[`challenges/`](challenges/) are how you run it. Each one resolves the
canonical project, builds through that project's Makefile, and checks a
marker. They create only descendants of the invoking process, signal only
that process or those descendants, reap every child, and run without root
or a network.

The orphan exercise uses `PR_SET_CHILD_SUBREAPER` so the new parent is the
lab process. The zombie is polled briefly and then waited for. The shell
day reads the current session and creates one child group. It leaves the
terminal's foreground group where the launcher put it. PID values and
`/proc` text vary by run. The stable evidence is a decoded status, a state
letter, an expected `errno`, a program marker, or an explicit skip when
strace or GDB is absent.

## What a finished week can answer

After the seven sections and their wrappers, you should be able to
answer, with a source or a run attached to each sentence:

1. Which state is copied, shared, reset, or replaced across `fork` and
   `exec`?
2. Which object owns a descriptor's offset, and when does close-on-exec
   act?
3. Why does the kernel keep a dead task until a parent collects the
   status, and who is that parent after reparenting?
4. Where is a signal while it is blocked, pending, selected, delivered,
   and returned from?
5. How do session, process group, and terminal foreground differ?
6. Which sentence came from source, ELF metadata, `/proc`, a trace, a
   debugger stop, or a checked assertion?
7. What is still unproved because of timing, namespaces, a missing tool,
   kernel version, or architecture?

Write the answers in the Day 7 report. A `WEEK10 PASS` line means the
checked invariants held on that run. The report is how you show the
mechanism.
