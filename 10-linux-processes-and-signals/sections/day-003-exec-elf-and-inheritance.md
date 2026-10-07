# Day 3 — Exec, ELF, argv, env, and Descriptor Inheritance

**Target time:** approximately 3 hours

> A successful `execve` does not create a new process and does not return. It
> replaces the calling process image while selected kernel state survives.

## 1. Begin with the status mode

Run `challenges/day-003-exec-elf-inheritance.sh`. Predict a child that executes
`/bin/sh -c 'exit 7'`, a parent that decodes exit 7, and an ELF inspection of
the canonical lab. The same mode creates another child, terminates it with
`SIGTERM`, and distinguishes signal termination from a normal numeric exit.

The wrapper supplies commands, expected markers, paths, and limits. Do not
substitute an unbounded trace of a login shell. The canonical child executes a
fixed command and the parent reaps it.

## 2. The exec contract

`execve(path, argv, envp)` asks the kernel to replace the caller's executable
image. On success, old user mappings are discarded according to exec rules,
new mappings and an initial stack are created, execution begins at an entry
selected through the executable format and dynamic-loader contract, and the
call never returns. The PID remains the same.

On failure, `execve` returns `-1`, sets `errno`, and the old image continues.
That asymmetry requires immediate error handling. In a post-fork child, report
the error through a safe channel if needed and call `_exit` with a documented
mapping. The project maps `ENOENT` to 127 in its injected-failure mode, a shell
convention rather than a kernel-mandated universal mapping.

The convenience function `execl` used by the lab constructs `argv` and
inherits the caller's environment. Its variadic list must end with a correctly
typed null pointer. The kernel-facing concept remains exec.

## 3. ELF is a format contract, not the whole launch

For a native ELF executable, the header names class, byte order, machine,
type, and entry. Program headers describe segments and attributes. A
dynamically linked executable commonly names an interpreter in `PT_INTERP`;
the kernel loads the interpreter and transfers control according to the ABI.
The dynamic linker maps dependencies, performs relocations, initializes
runtime state, and eventually reaches application startup.

`main` is not normally the ELF entry point. Startup code receives the initial
stack, prepares libc, and invokes `main`. A stripped binary can run without a
symbol named `main`; symbols aid linking/debugging but are not the execution
contract.

Scripts add another path: a shebang can select an interpreter. Exact parsing
and recursion limits are OS implementation details. Today's child explicitly
execs `/bin/sh`; it does not rely on a script shebang.

## 4. `argv`, environment, and the initial stack

`argv` is a null-terminated array of pointers to argument strings, conventionally
with `argv[0]` naming the invoked program. The kernel does not enforce every
social convention about that string. `envp` is another null-terminated pointer
array whose strings commonly have `NAME=value` form. Together with auxiliary
vector entries and architecture ABI data, these appear in the new image's
initial user stack.

Argument and environment bytes are bounded by system limits and available
space. Treat environment input as untrusted: variables can alter locale,
allocation, dynamic loading, tool behavior, and child configuration. Privileged
execution applies additional rules, but this lab performs no privilege
transition.

The old C pointers do not survive as meaningful objects. The kernel copies
launch data into the new image. A successful exec destroys the old userspace
heap and stack; only values intentionally represented in surviving kernel
state, files, arguments, environment, or external storage can cross.

## 5. State that survives, resets, or closes

A disciplined exec ledger includes:

```text
PID and parent relationship       preserved
new executable mappings/stack     replaced
ordinary caught dispositions      reset to default
ignored signal dispositions       generally remain ignored
blocked signal mask               preserved
pending signals                   preserved by Linux rules
open descriptors                  preserved unless FD_CLOEXEC
open-file descriptions/offsets    remain behind surviving descriptors
credentials                       transformed by documented exec rules
cwd/root/umask                    preserved
threads other than caller         do not survive
```

Consult `execve(2)` for the complete list. Memory locks, alternate signal
stacks, mappings, timers, shared memory attachments, robust mutex state, and
dumpability have specific rules. “Everything stays except code” and
“everything resets” are both wrong.

## 6. Descriptor inheritance and close-on-exec

Descriptors are inherited by default. The `FD_CLOEXEC` descriptor flag tells
the kernel to close that entry during successful exec. Set it atomically when
creating or accepting a descriptor (`O_CLOEXEC`, `pipe2`, appropriate accept
variants) when possible. A separate `fcntl` after creation leaves a race in a
multithreaded program: another thread could exec between creation and flagging.

Close-on-exec does not destroy an open-file description if another descriptor
still references it. It closes one descriptor-table reference. Offset and
file status flags belong to the open description; `FD_CLOEXEC` belongs to the
entry.

The canonical status mode inherits ordinary descriptors but does not build a
dedicated proof of `FD_CLOEXEC`. The wrapper exposes source and documentation
landmarks; it must not claim a runtime close-on-exec observation. A follow-up
lab could open two harmless pipes, atomically mark one endpoint, exec a fixed
helper, and have the helper enumerate only its own `/proc/self/fd`.

## 7. Error handling after fork

Between `fork` and `exec` in a multithreaded process, only async-signal-safe
operations are generally safe until exec. Locks may have been held by vanished
threads in the child copy. Prefer `posix_spawn` where it expresses the launch,
or keep the child path to prepared descriptor operations and exec.

The lab is single-threaded. Its failure path checks `errno` immediately and
uses `_exit(127)` or `_exit(126)`. It does not call `perror`, allocate, or
flush copied stdio state in the child. The parent alone interprets wait status.

Always distinguish “could not execute” from “new program ran and returned a
code.” An error pipe with close-on-exec can carry pre-exec `errno`: EOF implies
successful exec closed the write end, while a fixed-size error record reports
failure. That design is beyond the compact lab but follows directly from
lifetime analysis.

## 8. Linux source landmarks and version discipline

In a selected Linux revision, inspect `fs/exec.c` for generic exec orchestration,
`fs/binfmt_elf.c` for ELF loading, `fs/binfmt_script.c` for shebang handling,
and the architecture tree for register setup and entry details. Descriptor
close-on-exec handling connects to file-table code.

Record the exact kernel tag and configuration. Distribution kernels may carry
patches; current mainline names can differ from a running release. ELF details
also depend on compiler/linker flags and whether the output is PIE. Hash and
inspect the actual artifact rather than describing a generic binary.

## 9. Architecture lens

ELF machine type, relocation forms, ABI registers, initial stack alignment,
and transfer to user mode vary by ISA. Syscall entry also varies. The generic
exec policy—replace image, preserve PID, apply descriptor and signal rules—is
not usefully explained by comparing x86 `syscall`, AArch64 `svc`, and RISC-V
`ecall`. Keep architecture discussion at the entry, ELF, and user-register
boundary.

## 10. Compare ToyOS, xv6, and Linux

ToyOS should be described only from its implemented process milestone. If it
loads one fixed user image and lacks exec, say so. Do not retrofit Linux's
argument, environment, or descriptor semantics onto it.

xv6-riscv provides a compact exec path, typically in `kernel/exec.c`, that
builds a user address space and argument stack for its simpler filesystem and
ABI. Verify the revision. Linux adds executable-format registration, dynamic
linking contracts, credentials, namespaces, security hooks, close-on-exec,
auxiliary vectors, ASLR, and broad compatibility.

## 11. Explain the checked run

Write the sequence:

1. parent forks and checks failure;
2. child calls `execl` with fixed argument vector;
3. success replaces the child image while retaining its PID;
4. `/bin/sh` exits 7;
5. parent waits for the exact PID;
6. `WIFEXITED` guards `WEXITSTATUS`;
7. a separate child is signaled, so `WIFSIGNALED` guards `WTERMSIG`.

Then classify each claim as source, ELF metadata, runtime marker, or API
contract. The run does not reveal the full initial stack or dynamic-linker
relocations merely because `readelf` describes the file.

## Practical challenge

**Purpose:** trace image replacement while preserving process identity and
classify argument, environment, signal, and descriptor state.

**Prediction:** the fixed exec child exits 7, the second child is terminated
by `SIGTERM`, and wait decoding keeps these outcomes distinct.

**Evidence:** checked mode output, exact source, ELF headers/program headers,
artifact hash, and an optional process trace.

**Limits:** no privileged exec, multithreaded fork/exec, close-on-exec runtime
probe, dynamic-linker relocation audit, shebang recursion, or full initial
stack dump is performed.
