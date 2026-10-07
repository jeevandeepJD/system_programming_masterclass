# Week 10 — Linux Processes and Signals

**Format:** seven hands-on-first lessons using the canonical
[`projects/linux-system-programming`](../projects/linux-system-programming/) lab

**Suggested workload:** approximately 21 focused hours, adjustable to mastery

> Predict the observable state, run one bounded experiment, explain the
> ownership and lifetime, and stop the claim where the evidence stops.

The project is an offline C17/POSIX lab. It creates only descendants of the
invoking process, sends signals only to itself or those descendants, reaps
every child, and needs neither root nor network access. The wrappers in
`challenges/` do not patch or copy the project. Each follows:

```text
purpose → prediction → evidence → explanation → limits
```

Run wrappers from this week directory. Each wrapper resolves the canonical
project relative to itself, builds only through the project's existing
Makefile, uses checked commands, and labels optional-tool skips.

## Daily roadmap

1. [Process mental model](sections/day-001-process-mental-model.md) —
   `task_struct`, PID identity, address space, files, credentials, kernel
   state, and process versus program.
2. [Fork, copy-on-write, and lineage](sections/day-002-fork-cow-and-lineage.md)
   — duplicated execution context, shared open-file descriptions, private
   memory illusion, and parent/child state.
3. [Exec, ELF, argv, env, and descriptors](sections/day-003-exec-elf-and-inheritance.md)
   — image replacement, initial stack, environment, and `FD_CLOEXEC`.
4. [Wait, zombies, orphans, and reparenting](sections/day-004-wait-zombies-and-reparenting.md)
   — status decoding, retained exit records, deterministic subreaping, and
   lifetime cleanup.
5. [Signals and safe return](sections/day-005-signals-masks-and-safe-handlers.md)
   — `sigaction`, masks, pending state, async-signal-safety, self-pipe, and
   architecture-limited signal-frame discussion.
6. [Shell execution and job-control concepts](sections/day-006-shell-process-groups-and-sessions.md)
   — fork/exec/wait orchestration, process groups, sessions, terminal
   foreground ownership, and a safe bounded observation lab.
7. [Integrated debugging report](sections/day-007-proc-strace-gdb-comparison-report.md)
   — `/proc`, `strace`, GDB, injected failures, and a version-disciplined
   ToyOS/xv6/Linux source comparison.

Each day also has one executable wrapper and one generated four-question HTML
theory check. `quizzes.json` is the source of quiz content.

## Safety and evidence discipline

Do not paste bare commands from the prose. Use the wrapper so paths, build
preconditions, time bounds, expected markers, and cleanup behavior remain
visible. PID values, scheduling, syscall spellings, and `/proc` snapshots vary.
The stable evidence is the checked invariant: decoded status, observed state,
expected `errno`, a marker emitted by the canonical program, or a documented
optional-tool skip.

The orphan exercise uses Linux `PR_SET_CHILD_SUBREAPER`; this intentionally
avoids depending on container PID 1. The zombie is observed briefly and then
reaped. The signal exercises target the current process or a child. The shell
day observes the current non-interactive environment and creates a bounded
child group; it does not implement an interactive shell or manipulate an
unrelated terminal.

## Linux source landmarks and version discipline

Useful current-tree landmarks include `include/linux/sched.h`,
`kernel/fork.c`, `kernel/exit.c`, `kernel/signal.c`, `fs/exec.c`,
`fs/binfmt_elf.c`, `fs/proc/`, and architecture-specific syscall/signal code
under `arch/<isa>/`. Names and call paths change. Record the kernel release
(`uname -r`), source revision or tag, configuration, architecture, and exact
file before making line-level claims. A source landmark explains an
implementation; the man-page/POSIX contract defines what an application may
rely on.

Architecture comparison is deliberately narrow. Syscall entry and signal
frame construction/return are architecture-specific. Fork/exec/wait policy,
PID namespaces, file-table lifetime, credentials, and process-group rules are
kernel and API concepts, not properties to infer from x86-64 register names.

## Completion checkpoint

Produce a report that can answer:

1. What state is copied, shared, reset, or replaced across `fork` and `exec`?
2. Which object owns a descriptor's offset, and when does close-on-exec act?
3. Why is a zombie not a running process, and who must consume its status?
4. When blocked, pending, selected, delivered, handled, and returned, where
   can a signal be observed?
5. How do session, process-group, and terminal foreground identities differ?
6. Which conclusion came from source, ELF metadata, `/proc`, syscall trace,
   debugger state, or a checked program assertion?
7. What remains unproved because of timing, namespace, tool, kernel-version,
   or architecture limits?

Do not auto-fill mastery, confidence, hours, or completion.
