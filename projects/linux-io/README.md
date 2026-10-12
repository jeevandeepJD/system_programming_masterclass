# Linux I/O Lab — Week 12

An offline-first C17 lab for Linux file descriptors, short transfers, the page
cache, pipes, readiness, and `mmap`. It uses the compiler, libc, `/proc`,
local pipes, and temporary files under `/tmp`. It does not use the network,
open a socket, or require root.

This lab supports the prepared
[Week 12 I/O and readiness package](../../12-linux-io-and-readiness/overview.md).
Prepared means the material and tests are available. It does not record
learner completion. The course resume point remains in
[`metadata/WEEKLY_PROGRESS.md`](../../metadata/WEEKLY_PROGRESS.md).

## Repository map

```text
projects/linux-io/
├── Makefile
├── README.md
├── src/
│   ├── descriptor_lab.c
│   ├── stream_lab.c
│   ├── pagecache_lab.c
│   ├── pipe_lab.c
│   ├── readiness_lab.c
│   ├── mmap_lab.c
│   └── debug_lab.c
├── scripts/
│   ├── test-week12.sh
│   └── optional-tools.sh
└── build/                    generated binaries and optional strace logs
```

## Quick start

```sh
make clean build
make test-week12
make tools          # optional strace; PASS or a clear SKIP
make CC=gcc test-week12
make CC=clang test-week12
make clean
```

Compilation is strict: C17, `_GNU_SOURCE` for `epoll` and pipe-capacity
controls, warnings as errors, and conversion plus shadow warnings. `cc`, GCC,
and Clang are all expected to build this tree with those flags.

## What each program checks

```sh
./build/descriptor_lab demo   # dup shares the offset; a new open does not
./build/stream_lab demo       # short write, EOF, EAGAIN, EINTR, retry loop
./build/pagecache_lab demo    # buffered readback, fsync request, sparse hole
./build/pipe_lab demo         # dup, EOF, dup2, backpressure, PIPE_BUF
./build/readiness_lab demo    # select, poll, epoll LT, epoll ET drain
./build/mmap_lab demo         # MAP_SHARED, msync, pread/pwrite, one timing
./build/debug_lab demo        # /proc/self/fd, EBADF, EPIPE, epoll errno
```

Every mode is named `demo` so a wrapper can run one bounded scenario. Marker
lines are the contract the shell test greps. A timing number is printed for
the mapping comparison and is not an assertion.

## Safety

- Temporary directories are created with `mkdtemp` under `/tmp` and removed
  before the program exits. The suite fails if one of those directories remains.
- Pipe capacity is set to 64 KiB before fill and short-write tests.
- Waits use a zero timeout, a 4-second alarm, or a child deadline. A failed
  parent reaps a leftover child.
- `SIGPIPE` is ignored only for the deliberate `EPIPE` write.
- No socket, bind, connect, or listen call is present.

## Scope and limits

- Linux is required for `/proc/self/fd`, `epoll`, and `F_SETPIPE_SZ`.
- `dup` sharing an offset is POSIX. The kernel file names in the Week 12
  overview were read from tree `c9b2385d6793` (Makefile version 5.10.134)
  and are not a description of every running kernel.
- A matching readback before `fsync`, and a successful `fsync` or `msync`,
  are not crash-durability evidence. The programs print `durability_claim=none`
  and `msync_is_not_crash_proof=yes`.
- The `mmap` timings are one run. The suite asserts `performance_claim=none`.
- `strace` may be missing or blocked by ptrace policy. `make tools` then
  prints `SKIP`. A skip is not a failed core test.
