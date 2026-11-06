# Week 16 Linux kernel synchronization lab

This safe userspace project supports Week 16 without pretending to implement kernel primitives. Its seven programs cover primitive selection, C17 acquire/release publication, an `atomic_flag` teaching spinlock, mutex contention and PI-attribute support, atomic lifetime plus stable sequence snapshots and per-thread counters, a labeled userspace epoch/RCU simulator, and a read-only self/`/proc` report.

Run `make test-week16` for the deterministic core suite. Both GCC and Clang are expected to compile with `-std=c17 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow`. The suite uses bounded alarms and thread counts, needs no root, makes no network requests, loads no modules, and changes no tracing mounts, cgroups, sysctls, CPU settings, affinity, or scheduling policy. It creates no temporary files outside `build/`.

`scripts/check-source.py` reads `/home/jd/Documents/linux-6.12`, requires commit `adc218676eef25575469234709c2d87185ca223a`, checks ordered locking, barrier, refcount, seqlock, RCU, lockdep, and KCSAN landmarks, and hashes the source Makefile before and after. It never invokes a kernel build.

`make tools` is optional. It does not run `perf`, `trace-cmd`, `bpftrace`, or `lockstat`; it prints an honest `SKIP` describing why explicit permission and learner action are required.
