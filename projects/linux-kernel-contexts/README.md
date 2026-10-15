# Linux Kernel Contexts — Week 14

An offline-first C17 lab and a read-only inspection of Linux 6.12 entry,
exception, interrupt, and context code. The C programs use libc, a pipe, a
child fault, a child signal, `eventfd`, `timerfd`, and `poll`. They are
userspace observations. They do not install a kernel module, write a tracer,
or implement an IRQ handler.

`LINUX_SRC` defaults to `/home/jd/Documents/linux-6.12`. The landmark script
checks that tree and does not modify it. A different directory is accepted
only when it is the same v6.12 snapshot: `VERSION = 6`, `PATCHLEVEL = 12`,
`SUBLEVEL = 0`, and commit `adc218676eef25575469234709c2d87185ca223a`.

This lab supports the prepared
[Week 14 entry and context package](../../14-kernel-entry-contexts-and-interrupts/overview.md).
Prepared means the material and tests are available. It does not record
learner completion. The course resume point remains in
[`metadata/WEEKLY_PROGRESS.md`](../../metadata/WEEKLY_PROGRESS.md).

## Repository map

```text
projects/linux-kernel-contexts/
├── Makefile
├── README.md
├── src/
│   ├── syscall_lab.c
│   ├── fault_lab.c
│   ├── interrupts_lab.c
│   ├── analog_lab.c
│   ├── context_lab.c
│   └── report_lab.c
├── scripts/
│   ├── check-source.sh
│   ├── test-week14.sh
│   └── optional-tools.sh
└── build/                    generated binaries and optional tool logs
```

## Quick start

```sh
make clean build
make test-week14
make tools          # optional strace, perf, and tracefs; PASS or a clear SKIP
make CC=gcc test-week14
make CC=clang test-week14
make clean
```

Compilation is strict: C17, `_GNU_SOURCE` for `eventfd` and `timerfd`,
warnings as errors, and conversion plus shadow warnings. `cc`, GCC, and
Clang are all expected to build this tree with those flags.

## What each program checks

```sh
./scripts/check-source.sh 1   # v6.12 identity, Kconfig, kbuild, context files
./build/syscall_lab demo      # getpid through syscall(2) and a one-byte pipe
./build/fault_lab demo        # child SIGSEGV and a caught SIGUSR1
./build/interrupts_lab demo   # /proc/interrupts only when it can be read
./build/analog_lab demo       # eventfd and timerfd through poll
./build/context_lab demo      # sleepability table plus a userspace wait
./build/report_lab demo       # uname, proc fields, and explicit non-claims
```

Every mode is named `demo`. Marker lines are the contract the shell test
greps. IRQ counts from `/proc/interrupts` are not assertions; they change
between runs.

## Safety

- No temporary directory is created by the C programs. The negative landmark
  check uses `mkdtemp` under `/tmp` and removes it.
- The fault parent arms a three-second alarm and waits for both children.
- `poll` uses a zero timeout or a 500 ms cap. The timer is armed for 20 ms.
- `/proc/interrupts` is opened read-only and only when `fopen` succeeds.
- Tracefs is not opened for write. `make tools` skips tracer setup.
- No socket, module load, or privilege change is present.

## Scope and limits

- The userspace runs show that a syscall returned, that a child was killed
  by `SIGSEGV`, that a child caught `SIGUSR1`, and that `poll` can report
  an `eventfd` or a `timerfd`. Those are not traces of `entry_SYSCALL_64`,
  `exc_page_fault`, or `handle_irq_event`.
- `eventfd` and `timerfd` are labeled analogs. A ready descriptor is not a
  softirq, a threaded IRQ, or a workqueue.
- The sleepability lines are the lesson table. `kernel_preempt_claim=none`
  means the program did not read `preempt_count`.
- `strace` and `perf` may be missing or blocked. `make tools` then prints
  `SKIP`. A skip is not a failed core test. A readable tracefs file is still
  a skip: this lab does not enable a tracer.
- Week 15 is the scheduler dive. This tree stops at entry context, deferred
  work, and the rule for when a context may sleep.
