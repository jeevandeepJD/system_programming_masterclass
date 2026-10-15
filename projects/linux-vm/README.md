# Linux VM Lab — Week 13

An offline-first C17 lab for address spaces, demand faults, shared and
private mappings, allocator observations, and two deterministic policy
simulators. It uses the compiler, libc, `/proc/self`, `mmap`, and temporary
files under `/tmp`. It does not use the network, open a socket, or require
root.

This lab supports the prepared
[Week 13 virtual-memory package](../../13-virtual-memory-faults-and-reclaim/overview.md).
Prepared means the material and tests are available. It does not record
learner completion. The course resume point remains in
[`metadata/WEEKLY_PROGRESS.md`](../../metadata/WEEKLY_PROGRESS.md).

## Repository map

```text
projects/linux-vm/
├── Makefile
├── README.md
├── src/
│   ├── address_lab.c
│   ├── fault_lab.c
│   ├── mapping_lab.c
│   ├── allocator_lab.c
│   ├── buddy_lab.c
│   ├── reclaim_lab.c
│   └── debug_lab.c
├── scripts/
│   ├── test-week13.sh
│   └── optional-tools.sh
└── build/                    generated binaries and optional tool logs
```

## Quick start

```sh
make clean build
make test-week13
make tools          # optional perf stat and /proc/vmstat; PASS or a clear SKIP
make CC=gcc test-week13
make CC=clang test-week13
make clean
```

Compilation is strict: C17, `_GNU_SOURCE` for `MAP_ANONYMOUS` and `mincore`,
warnings as errors, and conversion plus shadow warnings. `cc`, GCC, and
Clang are all expected to build this tree with those flags.

## What each program checks

```sh
./build/address_lab demo    # page size, maps, untouched smaps, stack
./build/fault_lab demo      # zero-fill, mincore, minor-fault delta, Rss
./build/mapping_lab demo    # MAP_SHARED, MAP_PRIVATE, child SIGSEGV
./build/allocator_lab demo  # brk and maps observations, munmap
./build/buddy_lab demo      # simulated buddy, zones, and NUMA fallback
./build/reclaim_lab demo    # simulated LRU, writeback, swap, OOM choice
./build/debug_lab demo      # /proc/self maps, smaps, and status
```

Every mode is named `demo` so a wrapper can run one bounded scenario. Marker
lines are the contract the shell test greps.

## Safety

- The only temporary directory is `mkdtemp` of `/tmp/week13-map.XXXXXX`.
  The mapping lab unlinks its files, removes the directory, and the suite
  fails if any `/tmp/week13-*` path remains.
- Children are waited for. A timeout or a failing parent kills and reaps a
  child it still owns. The `mprotect` store runs only in the child.
- Mappings used as evidence are one page. The allocator lab's large
  `malloc` is 2 MiB and is freed before exit.
- The buddy and reclaim programs allocate nothing on the host. Their frames,
  zones, swap slots, and OOM choice are arrays inside the process.
- No socket, bind, connect, listen, sysctl write, or request for root is
  present. The lab does not try to exhaust memory, force swap, or invoke
  the OOM killer.

## Scope and limits

- Linux is required for `/proc/self/maps`, `smaps`, `status`, and `mincore`.
- `mmap`, `mprotect`, `fork`, and `wait` status decoding are POSIX. The
  kernel file names in the Week 13 overview were read from Linux 6.12,
  commit `adc218676eef25575469234709c2d87185ca223a` (Makefile `VERSION = 6`,
  `PATCHLEVEL = 12`, `SUBLEVEL = 0`), and are not a description of every
  running kernel.
- `getrusage` minor faults are a lower bound. Other activity in the process
  can raise the count, so the suite checks `minor_fault_delta_ge_1=yes` and
  requires the caveat line. It does not check an exact delta.
- Rss after a read is printed and not asserted. A read fault may install
  the shared zero page, which need not be charged as a private resident
  page. Rss is required to match one page only after a store.
- `brk_changed` and the `maps` names of `malloc` results are observations
  of this libc on this run. The suite does not encode a glibc mmap
  threshold.
- Buddy fallback and the LRU/writeback/swap/OOM sequence are teaching
  policies. They are not `get_page_from_freelist` or `shrink_folio_list`.
- `perf stat` and `/proc/vmstat` may be missing or restricted. `make tools`
  then prints `SKIP`. A skip is not a failed core test. `/proc/vmstat` is
  system-wide. One `perf` run is not a universal fault rate.
