# Linux System Programming Lab — Weeks 10–11

An offline-first C17/POSIX lab for Linux. It uses only the compiler, libc,
pthreads, `/proc`, and local optional debugging tools. No network access,
services, root privileges, or unrelated processes are used.

This shared lab supports the prepared
[Week 10 process/signal package](../../10-linux-processes-and-signals/overview.md)
and
[Week 11 pthread-concurrency package](../../11-threads-and-concurrency/overview.md).
Prepared means the material and tests are available; it does not record
learner completion or mastery. The course resume point remains in
[`metadata/WEEKLY_PROGRESS.md`](../../metadata/WEEKLY_PROGRESS.md).

## Repository map

```text
projects/linux-system-programming/
├── Makefile
├── README.md
├── src/
│   ├── process_lab.c
│   ├── signal_lab.c
│   ├── proc_lab.c
│   ├── thread_lab.c
│   ├── queue_lab.c
│   ├── deadlock_lab.c
│   └── litmus_lab.c
├── scripts/
│   ├── test-week10.sh
│   ├── test-week11.sh
│   └── optional-tools.sh
├── gdb/
│   └── week10.gdb
└── build/                    generated binaries and optional-tool logs
```

## Quick start

```sh
make clean build
make test-week10
make test-week11
make test-all       # repeats both suites, then optional-tool checks
make clean
```

Compilation is strict: C17, POSIX.1-2008 feature selection, warnings as errors,
conversion/shadow warnings, and pthread flags where needed. Override the
compiler with `make CC=gcc` or `make CC=clang`.

## Week 10: processes, signals, and Linux observation

`process_lab` provides small, bounded modes:

```sh
./build/process_lab status    # fork/exec plus normal and signal wait status
./build/process_lab tree      # child PID/PPID records returned over a pipe
./build/process_lab zombie    # observe Z in /proc, then always wait/reap
./build/process_lab orphan    # orphan grandchild adopted by this subreaper
./build/process_lab failures  # ENOENT -> 127 and checked ECHILD double-wait
```

The orphan demonstration calls Linux `PR_SET_CHILD_SUBREAPER`; this makes
reparenting deterministic even in containers where PID 1 differs. The original
process waits for both descendants. The zombie exists only long enough to read
its `/proc/PID/stat`, then is reaped. Thus successful runs leave no descendants.

`signal_lab` installs handlers with `sigaction`:

```sh
./build/signal_lab pending    # block, raise, verify pending, unblock/deliver
./build/signal_lab self-pipe  # handler performs only async-signal-safe write
./build/signal_lab failures   # SIGKILL handler EINVAL; absent PID probe ESRCH
```

The self-pipe turns asynchronous notification into normal-flow work. Handlers
do not call stdio, allocate, lock, or access non-`sig_atomic_t` shared state.
Signals target only the current process or a child it created. The absent-PID
case uses signal 0, which never delivers a signal.

`proc_lab` reads `/proc/self/status`, `/proc/self/stat`, and `/proc/self/exe`.
Useful syscall traces:

```sh
strace -f -e trace=process,signal ./build/process_lab status
strace -f -e trace=process ./build/process_lab orphan
strace -e trace=signal,read,write ./build/signal_lab self-pipe
```

## Week 11: threads and memory ordering

```sh
./build/thread_lab basic          # create, return value, join
./build/thread_lab mutex          # protected shared counter
./build/thread_lab atomic         # C11 relaxed atomic counter
./build/thread_lab ownership      # heap ownership transfer and one free
./build/queue_lab                 # bounded producer-consumer queue
./build/deadlock_lab detect       # opposite order + timed detection, no hang
./build/deadlock_lab ordered      # global 0-before-1 lock order
./build/thread_lab false-sharing  # adjacent vs padded atomic timing
./build/litmus_lab                # relaxed store-buffering observations
```

The queue waits in `while` predicate loops, accounting for spurious wakeups.
The deadlock exercise synchronizes opposite lock acquisition but uses
`pthread_mutex_timedlock`, so the demonstration detects the cycle and exits
instead of hanging.

`thread_lab race` intentionally contains a C data race. A data race is undefined
behavior, so it is excluded from deterministic tests and should be run through
ThreadSanitizer where supported (`make tools`). ThreadSanitizer is unavailable
on the prepared host/runtime, so this optional check reports `SKIP`; that is
neither a failed core test nor evidence that the race is safe. A numerically
correct result would not make it safe. Mutex and atomic modes are checked
exactly.

False-sharing timings depend on scheduling, topology, frequency scaling, and
cache layout; they are educational measurements, not assertions. Likewise,
whether the relaxed litmus observes both-zero on x86 (or any machine) is not
proof of what the C language permits. Language guarantees come from the C11
memory model, not one architecture or sample run.

## Debugging and analysis hooks

`make tools` detects each optional tool and prints `PASS` or a clear `SKIP`:

- `strace`: captures process syscalls in `build/strace-process.log`.
- GDB: runs `gdb/week10.gdb`, writing `build/gdb-session.log`.
- Valgrind: leak/error-checks the bounded queue.
- AddressSanitizer + UBSan: checks the ownership mode.
- ThreadSanitizer: attempts to detect the intentional race. Some Linux
  toolchain/runtime combinations cannot run TSan; that is reported as `SKIP`.

Manual examples:

```sh
gdb -x gdb/week10.gdb --args ./build/proc_lab
valgrind --leak-check=full --error-exitcode=91 ./build/queue_lab
cc -std=c17 -D_POSIX_C_SOURCE=200809L -pthread -fsanitize=thread -O1 \
  src/thread_lab.c -o build/thread_lab-tsan
./build/thread_lab-tsan race
```

## Scope and limits

- Linux is required for `/proc`, `prctl`, and the demonstrated process states.
- PID values, thread scheduling, trace details, benchmark timing, and relaxed
  litmus counts vary by run.
- The deterministic suites check invariants, not unstable ordering or timing.
- Fault modes are contained and expected: missing executable, repeated wait,
  uncatchable-signal handler installation, and nonexistent-PID signal-0 probe.
- `make clean` removes all binaries, traces, and sanitizer logs under `build/`.
