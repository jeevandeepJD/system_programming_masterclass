# Day 6 — Process/FD State, Bounded Pipe IPC, and Missing Boundaries

**Target time:** approximately 3 hours

> IPC is not merely moving bytes. It is naming endpoints, owning buffers,
> defining blocking and lifetime, and deciding what happens when peers vanish.

## 1. Inventory the one process

ToyOS stores one static `init_process` containing:

```text
root          process-owned PML4 pointer
entry         ELF entry address
user_stack    initial user RSP
kernel_stack  16 KiB supervisor allocation
pid           fixed value 1
```

The process exists for kernel lifetime. There is no state enum, parent,
children, credentials, signal state, working directory, scheduling context,
or teardown. `getpid` simply returns the static PID.

Week 7's two kernel tasks are not reused as a process scheduler in Week 8.
For milestones 8 and 9, `kernel_main` calls `process_init` and
`process_start` instead of `tasks_init` and `tasks_start`. `yield` executes
only `pause` and returns zero; it does not switch to another process.

This is why “process” must be read as an ownership/isolation teaching object,
not a complete lifecycle implementation.

## 2. Decode the descriptor convention

Descriptors are small integers interpreted by syscall policy:

- fd 1 writes to serial output;
- fd 3 reads from the global pipe;
- fd 4 writes to the global pipe.

`SYS_PIPE` resets the global pipe state and copies `{3, 4}` to an eight-byte
user array. There is no per-process descriptor table in Week 8, no open-file
object, close operation, flags, access-mode field, reference count, or
descriptor allocation algorithm.

Calling these numbers file descriptors is useful at the ABI boundary, but
the implementation is a fixed convention. Ownership is global: any code able
to invoke the dispatcher with fd 3 or 4 reaches the same pipe state. Since
there is only one process, cross-process access control is not tested.

Week 9 adds VFS descriptors for other numbers, but this week's wrappers build
milestone 8 and must not import Week 9 behavior.

## 3. Trace the bounded pipe

The pipe state is three static objects:

```text
pipe_data[64]
pipe_length
pipe_read_position
```

Creating a pipe sets both counters to zero. A write succeeds only when the
entire request fits in `64 - pipe_length`; it appends bytes and advances
`pipe_length`. It does not reuse space consumed by reads.

A read computes `available = pipe_length - pipe_read_position`, copies the
smaller of requested and available, and advances `pipe_read_position`. An
empty pipe therefore returns zero immediately. An over-capacity write returns
`-1` immediately.

This is a bounded linear buffer, not a circular pipe. There is no blocking,
wakeup, EOF, broken-pipe indication, atomic-write rule, concurrent reader,
concurrent writer, or endpoint closure. Re-running `pipe` destroys previous
global contents by resetting counters.

## 4. Follow the canonical user round trip

The embedded user program:

1. invokes `pipe(&pipe_fds)`;
2. loads fd 4 from user memory;
3. writes `"USER: bounded pipe round-trip\n"`;
4. loads fd 3;
5. reads the same number of bytes into `io_buffer`;
6. uses the returned count as serial-write length.

The kernel never directly uses those user pointers. `SYS_PIPE` uses
`copy_to_user`; `SYS_WRITE` uses `copy_from_user` before choosing serial or
pipe policy; `SYS_READ` fills a kernel stack buffer and then uses
`copy_to_user`.

This staging gives the 256-byte local buffer temporary ownership of syscall
payloads. The pipe array owns appended bytes until reset or kernel exit.
`io_buffer` is process-image memory and remains owned by the process.

The normal marker `SELFTEST: bounded user copy and pipe PASS` is emitted by
the exit syscall after the user program reaches its end. It proves the
canonical sequence completed. It does not independently compare every byte
inside the kernel; visible echoed text and control flow provide supporting
evidence.

## 5. Place shared memory at the correct boundary

Shared-memory IPC would map the same physical frames into two or more process
address spaces, with explicit lifetime and synchronization. ToyOS has only
one user address space and provides no shared-memory syscall. Its copied
kernel mappings are shared implementation mappings, not user IPC.

A credible shared-memory design would need to answer:

- who allocates and names the object;
- which process mappings hold references;
- when physical frames can be reclaimed;
- what user permissions each mapping has;
- how peers synchronize data and ownership;
- what happens across fork, exec, exit, or unmap.

The current page mapper and frame allocator demonstrate ingredients, but the
policy and lifecycle are absent. Do not claim the pipe is shared memory: it
copies through a kernel-owned array.

## 6. Place signals at the correct boundary

Signals require asynchronous event state and a safe user-return modification.
A kernel may need pending/masked sets, dispositions, user signal frames,
alternate stacks, restart semantics, process/thread targeting, and a
validated trampoline or return syscall.

ToyOS has none of those. The interrupt frame proves that the kernel can see
and restore user continuation state, but mutating that frame into a signal
delivery is not implemented. `SYS_EXIT` terminates the VM, not one process.
Unexpected user exceptions panic the kernel under current policy rather than
becoming per-process signals.

The useful conceptual boundary is:

```text
synchronous syscall: user intentionally enters with an ABI request
exception: CPU reports a fault tied to an instruction
signal: kernel policy schedules an asynchronous/synchronous notification
```

Architecture supplies entry and saved state. Signal semantics are operating
system policy and user ABI.

## 7. Compare pipe and descriptor ownership

xv6 has per-process descriptor tables referring to reference-counted file
objects; pipes have lock-protected buffers, sleep/wakeup behavior, and endpoint
closure. Linux adds scalable descriptor tables, open-file descriptions,
polling, nonblocking modes, capacity policy, namespaces, permissions, and
many IPC families. Both comparisons require source revision checks.

ToyOS's fixed descriptors make the data path visible without hiding it behind
large abstractions. Its limits are exactly the missing ownership questions:
no endpoint object, no refcount, no close, no peer lifetime, and no wait
queue.

## Practical challenge

Use `challenges/day-006-pipe-ipc.sh` from the repository root.

**Purpose:** trace descriptor interpretation, user-copy staging, and bounded
pipe state through the canonical program.

**Predict:** descriptors are `{3,4}`, the message fits within 64 bytes, empty
reads are short, over-capacity writes fail atomically, and normal status is
33.

**Evidence:** the wrapper checks the integrated marker, inspects source and
user disassembly, and reports exact limits.

## Mastery prompts

Build an ownership timeline for the pipe message from user `.rodata` to the
kernel stack buffer, global pipe, second kernel buffer, user `.data`, and
serial device. Then specify the minimum new state required for two processes,
blocking reads, close/EOF, shared memory, and signals.
