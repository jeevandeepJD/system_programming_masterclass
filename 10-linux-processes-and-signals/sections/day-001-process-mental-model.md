# Day 1 — A Process Is More Than a Program

**Target time:** approximately 3 hours

> An executable file is passive input. A process is a live kernel-managed
> execution context with identity, resources, authority, and changing state.

## 1. Start with a bounded observation

Run `challenges/day-001-process-model.sh` from the Week 10 directory. Before
running it, predict which facts remain constant across `/proc/self/status`,
`/proc/self/stat`, and `/proc/self/exe`, and which can differ on every run.
The wrapper builds the canonical project, runs `proc_lab`, identifies its ELF,
and prints selected source and host facts. It does not inspect arbitrary
processes.

Expected stable outcomes are `proc_inspection=ok`, an ELF executable whose
machine matches the host build, and status/stat records referring to the
process that opened them. Exact PIDs, parent PIDs, scheduling fields, virtual
addresses, and executable paths can vary. Explain invariants rather than
memorizing a sample.

## 2. Process versus program

A program is usually a file containing instructions and metadata. An ELF file
can describe loadable segments, an entry point, interpreter requirements,
symbols, and relocation information. It has no current instruction pointer,
open descriptor table, pending signals, scheduler state, or effective
credentials merely because it exists.

A process is one execution context known to the kernel. Linux represents the
schedulable entity with `struct task_struct`; threads are tasks that share
selected resources. The familiar “process” is therefore not one indivisible C
object. It is a graph of referenced kernel objects: memory descriptors, file
tables, filesystem context, signal-handling state, credentials, namespaces,
and scheduler/accounting state. Some references may be shared with sibling
threads or related tasks. The exact fields are internal and version-dependent.

Starting the same executable twice creates distinct process identities and
mutable states. Conversely, `execve` can replace one process's program image
without changing its PID. That single fact defeats the shorthand “a process
is a running program.” The phrase is useful only as an introduction.

## 3. Build an ownership ledger

Use this conceptual ledger, then verify only the fields exposed by the lab:

```text
state                         conceptual owner/reference
PID and task state            kernel task identity
virtual mappings              memory descriptor/address space
descriptor numbers            per-process file descriptor table
open offset/status flags      shared open-file description
cwd/root/umask                filesystem context
UID/GID/capabilities          credential object
blocked signal mask           calling thread
signal dispositions           process-wide signal state
pending signals               thread- or process-directed queues
registers/kernel stack        individual task
```

This distinction predicts behavior. After a normal `fork`, parent and child
have separate descriptor tables whose entries refer to the same open-file
descriptions, so an offset may be shared while closing one descriptor table
entry does not close the other's entry. Threads created with suitable
`clone` flags can share the table itself. Never infer sharing merely because
two integer descriptor values are equal.

Credentials are similarly richer than one UID. Real, effective, and saved
IDs, supplementary groups, capabilities, securebits, namespaces, and
no-new-privileges can affect authorization. `/proc/PID/status` exposes a
projection, not every policy decision.

## 4. PID identity and namespaces

A PID is a number in a PID namespace, not a globally eternal handle. It can be
reused after a task is fully released. Recording only `1234` is therefore
weak evidence. Add the namespace context, observation time, parent
relationship, executable identity, and, where relevant, a pidfd. A process
inside a container may see a different PID than an observer in an ancestor
namespace.

`getpid()` reports the thread-group ID used as the conventional process ID.
Linux also has task IDs for individual threads. `/proc/self` resolves relative
to the reading process, while `/proc/thread-self` names the calling thread.
Day 1 stays with single-threaded programs so those identities do not blur the
experiment.

## 5. Address space and kernel state

The address space gives user code a virtual view. It includes executable
segments, shared libraries, stack, heap, mappings, and unmapped gaps. The page
tables and virtual-memory areas are kernel-managed state; physical frames can
be shared, copied, reclaimed, or backed by files without changing the
program's ordinary pointer values.

Not all process state lives in the address space. Descriptor-table entries,
credentials, pending signals, wait relationships, accounting, namespace
membership, and scheduler state are kernel objects. A core dump or userspace
memory inspection therefore cannot by itself reconstruct every process
property.

`/proc/PID/maps` would expose mappings, but the canonical `proc_lab` limits
itself to status, stat, and exe. That restraint matters: do not claim the
wrapper measured copy-on-write pages or permissions it never read.

## 6. Files and lifetime

The word “file” hides several identities. A descriptor is a small index in a
table. Its entry points to an open-file description, which owns current offset
and file status flags. That object refers onward to filesystem objects or a
pipe/socket/device implementation. Descriptor flags such as `FD_CLOEXEC`
belong to the descriptor entry, not the shared open-file description.

Reference counts let a kernel object outlive one name or one descriptor.
Closing a descriptor releases a reference. Process exit closes remaining
descriptors. An unlinked regular file can remain accessible through an open
description. These are lifetime rules, not ELF or CPU architecture facts.

In C, check every operation at the boundary where failure is reported.
`open`, `read`, `readlink`, `fclose`, and `snprintf` have different success
contracts. The project's `proc_lab` checks truncation before using its path,
leaves room for `readlink` termination, checks stream errors, and closes the
stream. Error handling is part of the process model because leaks and stale
references alter lifetime.

## 7. Linux implementation landmarks

For a chosen Linux revision, begin with:

- `include/linux/sched.h` for `task_struct` and related declarations;
- `kernel/fork.c` for task creation and shared/copied resources;
- `fs/file.c` and `include/linux/fdtable.h` for descriptor tables;
- `mm/` and `include/linux/mm_types.h` for memory state;
- `kernel/cred.c` and `include/linux/cred.h` for credentials;
- `fs/proc/` for the exported observations used today.

Do not quote current mainline line numbers against the running distribution
kernel. Record `uname -r`; obtain the matching source package/config if making
an implementation claim. Field names are not an application ABI.

## 8. Architecture lens: only where it belongs

The ELF `Machine` field and syscall entry path are architecture-specific.
x86-64 commonly enters via `syscall`; AArch64 uses `svc`; RISC-V uses `ecall`.
Register assignments and entry assembly differ. Once the request reaches
generic process-management code, the conceptual PID, file, credential, and
wait policies are not explained by the entry instruction.

The wrapper identifies ELF metadata but does not disassemble the entry gate or
prove which instruction libc used. State that limit explicitly.

## 9. Explain the evidence

Write a one-page process inventory. Separate:

1. identity: PID, PPID, executable link, ELF hash;
2. user memory: what an address space conceptually contains;
3. kernel references: files, credentials, namespaces, signal and wait state;
4. mutable task state: scheduling state, registers, accounting;
5. evidence type: program assertion, procfs text, ELF metadata, or source;
6. variation: values expected to differ next run.

Then answer the central question: replacing an image with `execve` preserves
process identity while changing program content, and running one ELF twice
creates two identities. A program and a process cannot be synonyms.

## Practical challenge

**Purpose:** inventory the identities, owners, and lifetimes that make a Linux
process more than executable bytes.

**Prediction:** the canonical observer reports its PID/PPID, stat record,
executable target, and `proc_inspection=ok`; exact IDs vary.

**Evidence:** checked C output, selected `/proc` fields, project source, ELF
header, artifact hash, and recorded kernel release.

**Limits:** no arbitrary process inspection, namespace traversal, memory-map
measurement, thread sharing, privilege transition, or syscall-entry trace is
performed.
