# Week 12 — Linux I/O and Readiness

**Format:** seven daily sections collected into one weekly module

**Suggested workload:** approximately 21 focused hours, adjustable to mastery

> A descriptor is a small integer so user code never holds a kernel address.
> The VFS makes different objects answer the same calls. Blocking and
> readiness exist so a task can leave the CPU until one of those objects can
> move bytes. The page cache bridges that byte API to pages and blocks.

The lab is [projects/linux-io](../projects/linux-io/). Wrappers build through
its Makefile and print only the claim the run supports. Kernel names below
were read from Linux 6.12, commit
`adc218676eef25575469234709c2d87185ca223a`, whose top-level `Makefile` sets
`VERSION = 6`, `PATCHLEVEL = 12`, `SUBLEVEL = 0`. Those names are that
snapshot. The host that runs the lab may be another kernel; the wrappers
print `uname`. Re-open the pinned tree, or the sources for the kernel you
boot, before treating a symbol, field, or errno path as current.

## Required order: syntax, Linux 0.12, evolution, ToyOS, then 6.12

Each day opens in this order, and the order is required:

1. the few C forms that day's source actually uses;
2. the Linux 0.12 path, read end to end while the whole mechanism still
   fits in one function;
3. why that early design changed, in the constraint the fixed tables, type
   switches, block buffer cache, one-page pipe, `select` scan, executable-only
   fault, or printk-and-Bochs picture could not keep;
4. the ToyOS reconstruction of that mechanism from Weeks 7–9;
5. the Linux 6.12 tree
   `adc218676eef25575469234709c2d87185ca223a`, then a check against current
   upstream before a symbol is treated as current.

Linux 0.12 is the first conceptual implementation. Its fixed tables, Minix
inodes, buffer cache, `select`, and 80386 demand paging make the control
flow visible. They are not the authority for the modern API: VFS
operations, the unified page cache, `poll`/`epoll`, `mmap`, SMP lifetime
rules, or today's errno and durability contracts. Those belong to the
6.12/current source.

The shared
[Linux 0.12 map](../references/EARLY_LINUX_012_MAP.md)
extracts the early `open`, `read`, pipe, and `select` paths with printed
book pages. Use it beside the day sections. Return here for how the seven
days fit together.

## Why these seven days belong together

User space asks for bytes. The kernel keeps objects, pages, and wait queues, and the descriptor is the integer boundary between them.

```text
descriptor
  fd table -> open-file description (offset, flags, refcount) -> file
        |
        v
VFS
  one read / write / stat shape; the object supplies the operations
        |
        v
bytes and waiting
  short counts, EOF, EINTR, EAGAIN
  a task sleeps, or select/poll/epoll reports that progress is possible
        |
        v
page cache
  buffered write is a cache update; fsync/msync is a request, not a crash proof
  MAP_SHARED names those same pages
        |
        v
pipes
  a finite buffer, EOF only when every writer is gone, PIPE_BUF atomicity
        |
        v
evidence
  /proc/self/fd, EBADF, EPIPE, an epoll errno, optional strace
```

In the excerpt tree, `fs/file.c` `dup` / `do_dup2` takes another reference
with `get_file` and stores that same `struct file` in a second fd slot.
`fs/open.c` `do_sys_open` is the path that allocates a new open. `fs/read_write.c`
`vfs_read` / `vfs_write` are the common entry. `fs/sync.c` `vfs_fsync` is the
durability request. `mm/filemap.c` `generic_file_write_iter` and
`filemap_fault` are the page-cache side of write and of a mapped fault.
`fs/pipe.c` `pipe_write` returns `-EPIPE` when no readers remain, and
`pipe_read` stops when no writers remain. `fs/select.c` implements `select`
and `poll`. `fs/eventpoll.c` defines `EPOLLET` and wakes through
`ep_poll_callback`. Quote those only from the tree you opened.

## Daily roadmap

### Day 1 — Descriptors, open files, and offsets

[Day 1](sections/day-001-descriptors-open-files-and-offsets.md) splits three
layers: the integer in the process, the open-file description, and the file
object. `dup` shares the description, so a read or `lseek` on either
descriptor moves one offset. A second `open` of the same path allocates
another description, and the lab reads the leading bytes again. Wrapper:
[challenges/day-001-descriptors.sh](challenges/day-001-descriptors.sh).

### Day 2 — Read, write, short I/O, and nonblocking

[Day 2](sections/day-002-read-write-short-io-and-nonblocking.md) treats a
positive short count as success for those bytes. A zero read on a pipe whose
writers are gone is EOF. `EAGAIN` is the nonblocking empty result. `EINTR`,
with `SA_RESTART` off, is a signal interrupting the wait; the retry loop
tries again. The lab forces each one: a pipe capacity smaller than the
write, a closed writer, `O_NONBLOCK`, and `SIGUSR1` delivered while `read`
is the current syscall. Wrapper:
[challenges/day-002-stream-io.sh](challenges/day-002-stream-io.sh).

### Day 3 — VFS, the page cache, and durability

[Day 3](sections/day-003-vfs-page-cache-and-durability.md) starts from the
VFS question: one `read` / `write` / `stat` shape, many objects. For a
regular file the page cache is the memory of those bytes. The lab writes,
reads the bytes back through a second descriptor before `fsync`, then calls
`fsync` and records that the call returned. A sparse file has a logical
`st_size` past a hole, a zero byte in that hole, and fewer allocated blocks
than the logical size. None of that is a claim that power loss keeps the
data. Wrapper:
[challenges/day-003-page-cache.sh](challenges/day-003-page-cache.sh).

### Day 4 — Pipes, redirection, and backpressure

[Day 4](sections/day-004-pipes-redirection-and-backpressure.md) uses a pipe
as a bounded kernel buffer. `dup` of the write end is one buffer, not two.
`dup2` onto standard output is how a child aims `write` at that buffer.
Closing one writer leaves the pipe open; closing the last writer makes
`read` return 0. Filling the pipe makes a nonblocking writer get `EAGAIN`,
which is backpressure. A write of at most `PIPE_BUF` bytes is atomic: the
empty pipe accepts all of it, and the full pipe returns `EAGAIN` rather than
a prefix. Wrapper:
[challenges/day-004-pipes.sh](challenges/day-004-pipes.sh).

### Day 5 — select, poll, epoll, and readiness

[Day 5](sections/day-005-select-poll-epoll-and-readiness.md) is the scheduling
half of I/O. `select` and `poll` report an empty pipe and then a pipe that
has bytes. Level-triggered `epoll` reports again while bytes remain after a
partial read. Edge-triggered `epoll` does not repeat that edge; the lab
drains until `EAGAIN`, then sees the next write as a new edge. A ready
descriptor can still return a short read. Every wait in the lab uses a zero
timeout so a missed event fails instead of sleeping. Wrapper:
[challenges/day-005-readiness.sh](challenges/day-005-readiness.sh).

### Day 6 — mmap, shared mappings, and data movement

[Day 6](sections/day-006-mmap-shared-mappings-and-data-movement.md) maps a
file `MAP_SHARED`. A store is visible to `pread` before `msync`, because the
mapping and the read share page-cache pages. `msync(MS_SYNC)` is then the
same kind of request as `fsync`: completion is not a crash-durability proof.
A later `pwrite` is visible in the mapping. The program also times one
`pread` of the file and one copy from the mapping, prints both, and marks
`performance_claim=none`. Wrapper:
[challenges/day-006-mmap.sh](challenges/day-006-mmap.sh).

### Day 7 — I/O debugging and a comparison report

[Day 7](sections/day-007-io-debugging-and-comparison-report.md) reads
`/proc/self/fd` for a descriptor this process opened, then causes three
failures on purpose: `read` after `close` (`EBADF`), `write` on a pipe with
no reader and `SIGPIPE` ignored (`EPIPE`), and `epoll_ctl` on a closed fd
(`EBADF`) plus a regular file (`EPERM`). Optional `strace` records the
syscalls or prints `SKIP` when ptrace is unavailable. ToyOS at the Week 8/9
milestone has descriptors and a small pipe, not this VFS or page cache.
Quote xv6 `file.c` or `pipe.c` only after pinning a revision. Wrapper:
[challenges/day-007-io-debug-report.sh](challenges/day-007-io-debug-report.sh).

## Safety and evidence

The programs create temporary files only under `mkdtemp` directories in
`/tmp`, unlink them, and remove the directories. The suite checks that those
directories are gone. Children are waited for; a failing parent kills and
reaps a child it still owns. Pipe fills set a 64 KiB capacity and stop.
Readiness waits use a zero timeout. The `EINTR` parent arms a four-second
alarm. Nothing in the lab opens a socket or asks for root.

`fsync` and `msync` success, and a readback that matches before either call,
are page-cache and request results. The markers `durability_claim=none` and
`msync_is_not_crash_proof=yes` are part of the test, not a comment you may
drop. The `mmap` timings are one machine and one size.

## Completion checkpoint

Finishing the wrappers does not complete mastery. Answer from the source and
the logs, and leave a question blank when the run did not speak to it:

1. Which integer is the descriptor, which state lives in the open-file
   description, and which object is the file?
2. Which return is a short count, which is EOF, which is `EAGAIN`, and which
   is `EINTR`?
3. What did the second `open` see before `fsync`, and why is that not a
   crash-durability result?
4. Why did one closed writer fail to produce EOF, and why did the full-pipe
   `PIPE_BUF` write return `EAGAIN` instead of a prefix?
5. After a partial read, which of level-triggered and edge-triggered epoll
   reported again, and what did the edge-triggered path drain to?
6. Which bytes were visible through `MAP_SHARED` before `msync`, and which
   timing comparison are you refusing to generalize?
7. Which errno came from a closed descriptor, a pipe with no reader, and an
   `epoll_ctl` on a regular file?
8. Which kernel names did you re-read in commit
   `adc218676eef25575469234709c2d87185ca223a`, and which host
   `uname` actually ran the lab?

Do not auto-fill mastery, confidence, hours, or completion. The
[quizzes](quizzes.json) sit beside this overview and do not grade the report.
