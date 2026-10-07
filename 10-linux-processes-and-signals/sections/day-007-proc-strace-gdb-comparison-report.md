# Day 7 — `/proc`, `strace`, GDB, Failures, and Comparison Report

**Target time:** approximately 3 hours

> A debugging report is a chain from artifact identity to observation to
> causal claim. Tool output without version, scope, and limits is not proof.

## 1. Generate the evidence bundle

Run `challenges/day-007-debug-report.sh`. Predict `WEEK10 PASS`, hashes for the
three canonical binaries, checked injected failures, `/proc` output, and
either captured or explicitly skipped `strace` and GDB evidence. The wrapper
creates its report under a fresh temporary directory and prints the path. It
does not modify the project beyond the project's normal build products.

Optional tools are genuinely optional. `SKIP: strace unavailable` is more
accurate than fabricated syscall evidence. A debugger blocked by ptrace policy
should be reported as unavailable with its diagnostic, not silently converted
to success.

## 2. Establish identity first

Record:

- UTC capture time and `uname -a`;
- compiler, libc/tool, `strace`, and GDB versions where present;
- absolute canonical project path;
- SHA-256 hashes and ELF class/machine/type for each binary;
- exact invocation and expected marker;
- whether output was source, procfs, trace, debugger, or program assertion.

A source tree can differ from the built binary. A binary can differ from the
one attached in GDB. A process can exec between two observations. Identity is
not paperwork; it is the premise behind symbolic interpretation.

## 3. Read `/proc` as a changing interface

`/proc` is a kernel-generated view, not stored process memory. Reads at
different times need not form one atomic snapshot. Fields are documented by
procfs manuals and kernel source, but visibility can be restricted by mount
options, namespaces, credentials, LSM policy, or process exit.

The canonical `proc_lab` reads its own `status`, first `stat` line, and `exe`
symlink. It checks opens, input, close, `snprintf` bounds, `readlink`, and NUL
termination. `/proc/self/exe` can include a deleted marker or be inaccessible
under some conditions; today's successful run establishes only this
environment.

For the zombie experiment, `/proc/PID/stat` state `Z` is a momentary
observation followed by authoritative `waitpid`. For process groups and
sessions, parse documented fields carefully; a comm field with spaces makes
naive whitespace splitting unsafe in general.

## 4. Use `strace` for syscall evidence

`strace -f` follows descendants and can show clone-family creation, exec,
wait, signal, pipe, read, and write calls. Its display is decoded tool output,
not raw kernel source. Syscall names can differ by architecture, libc choice,
and tracer decoding. Attaching also changes timing.

Filter narrowly. The wrapper traces only the fixed `process_lab status` mode,
stores output in the temporary report directory, and extracts process/signal
landmarks. It never traces unrelated processes. Explain an observed
`clone`/`clone3` as the kernel interface underlying libc `fork`, if that is
what the actual trace shows; do not predict a spelling and call it observed.

Trace return values locate boundaries. An exec line followed by exit 7 differs
from an exec returning `ENOENT`. A wait returning a status must still be
decoded according to API macros. Tracing does not reveal all userspace state
or kernel-internal branches.

## 5. Use GDB for stopped-state evidence

The project provides `gdb/week10.gdb`, which breaks at `main`, runs, and
continues. In batch mode against `proc_lab`, a successful capture combines a
known binary, symbol breakpoint, and checked program marker. If debug
information is absent, function symbols may still permit a breakpoint, but
locals and source lines can be limited.

Before interactive debugging, predict the stop and select variables to inspect.
Afterward, label only values actually captured. Fork debugging requires an
explicit follow-fork policy; exec changes mappings and symbols; signals can be
passed, stopped, or suppressed by debugger configuration. GDB changes process
control and timing.

Never confuse a breakpoint at `main` with the process's first instruction.
ELF startup and dynamic-loader work precede it. Never present the command file
as evidence if GDB did not run.

## 6. Interpret injected failures

The process failure mode forks and attempts a fixed nonexistent path. In the
child, `ENOENT` maps to `_exit(127)`; another exec error maps to 126. The
parent checks normal exit and exact 127, then attempts a second wait and
requires `ECHILD`.

The signal failure mode asks `sigaction` to install a `SIGKILL` handler and
requires `EINVAL`. It then calls `kill(999999999, 0)` and requires `ESRCH`.
Signal zero performs permission/existence checks without delivery. The chosen
PID is outside ordinary Linux PID ranges in this environment assumption; if a
future kernel permits it, the lab's expectation would need revision.

Injected failures are useful because the expected detector and cleanup are
specified. They are not fuzzing and do not cover resource exhaustion, partial
fork construction, pipe saturation, permission denial, or races.

## 7. Linux source landmarks, exactly but cautiously

Anchor a source tour to a tag matching the running kernel where possible:

```text
include/linux/sched.h     task representation
kernel/fork.c             task creation/resource sharing
fs/exec.c                 generic exec orchestration
fs/binfmt_elf.c           ELF binary format
kernel/exit.c             exit, wait, reparenting
kernel/signal.c           generic signal machinery
fs/proc/                  procfs task presentation
arch/<isa>/entry/         syscall entry
arch/<isa>/kernel/signal.c signal-frame setup/return
```

These are landmarks, not promised stable paths or call graphs. Record commit,
config, distribution patches, and architecture. Search within that revision,
then cite symbols and lines from it. Separate UAPI guarantees from internals.

Architecture comparison belongs only at syscall entry and signal frame here.
x86-64, AArch64, and RISC-V use different trap instructions, register ABIs,
entry assembly, and user-context layouts. Generic fork/exec/wait/signal policy
continues above those boundaries.

## 8. ToyOS, xv6, and Linux

Build a comparison by capabilities actually present:

```text
question             ToyOS                 xv6-riscv             Linux
task model           course milestone      fixed process table    tasks + sharing flags
image launch         fixed/minimal          compact exec           format handlers + ELF
wait/reparent        verify implementation  compact exit/wait      namespaces/subreapers
signals              trap-level subset      small kill mechanism   POSIX/Linux signal model
observation          serial/GDB symbols     console/GDB             procfs/ptrace/trace tools
```

Replace broad cells with exact source citations from the revisions used.
ToyOS may not implement fork, exec, wait, or POSIX signals; absence is a valid
result. xv6 teaching branches vary, especially COW and labs. Linux richness
adds concurrency and lifetime obligations rather than making the smaller
systems “incorrect.”

Choose one invariant per comparison. For example: who owns exit status; which
object keeps an open offset; what state survives image replacement; where a
pending asynchronous event is checked; and how a user return context is
constructed.

## 9. Debugging workflow

When output surprises you:

1. identify binary hash, process identity, kernel/tool versions, and command;
2. reproduce with the smallest canonical mode;
3. determine whether failure is before fork, before exec, in the new image,
   during signal delivery, or during wait;
4. inspect self-owned `/proc` state;
5. add a narrow trace if available;
6. stop in GDB only with an explicit predicted observation;
7. inspect matching source at the earliest contradicted invariant;
8. verify all children/descriptors were cleaned despite failure.

Do not start by collecting every syscall or attaching to unrelated services.
More output can erase the causal boundary.

## 10. Required final report

Write sections for identity, process state, fork/COW classification,
exec/inheritance, wait/reparenting, signals, job-control model, injected
failures, tool evidence, and three-system comparison. For every conclusion
name evidence and a limit.

Example: “The child was observed as `Z` in `/proc/PID/stat` before a checked
`waitpid` decoded exit 23. This proves a retained waitable status in this run;
it does not measure PID-reuse windows or all procfs visibility policies.”

Conclude with cleanup: the full regression passed, every demonstration child
was reaped, signals targeted only self/children, and temporary report location
is known. A pass marker establishes tested invariants, not general process
correctness.

## Practical challenge

**Purpose:** produce a reproducible process/signal debugging report joining
runtime, `/proc`, trace, debugger, source, and comparison evidence.

**Prediction:** all canonical Week 10 modes pass; expected failures are
recognized; optional tools either yield scoped evidence or a clear skip.

**Evidence:** versions, hashes, ELF metadata, exact invocations, checked
outputs, temporary logs, selected source landmarks, and explicit non-claims.

**Limits:** no arbitrary-process tracing, root operations, network, stress,
kernel rebuild, full signal-frame capture, interactive terminal manipulation,
or proof beyond the recorded versions and runs.
