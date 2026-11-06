#!/usr/bin/env python3
"""Read-only validation of pinned Linux synchronization landmarks."""
from pathlib import Path
import hashlib
import os
import re
import subprocess

root = Path(os.environ.get("LINUX_SRC", "/home/jd/Documents/linux-6.12"))
expected = "adc218676eef25575469234709c2d87185ca223a"
makefile = root / "Makefile"
before = hashlib.sha256(makefile.read_bytes()).hexdigest()
commit = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
if commit != expected:
    raise SystemExit(f"FAIL expected_commit={expected} actual_commit={commit}")

checks = {
    "include/linux/spinlock.h": ["spin_lock_irqsave", "spin_unlock_irqrestore"],
    "include/asm-generic/barrier.h": ["smp_store_release", "smp_load_acquire"],
    "kernel/locking/mutex.c": ["__mutex_lock", "mutex_unlock"],
    "kernel/locking/rtmutex.c": ["rt_mutex_adjust_prio_chain", "rt_mutex_slowlock"],
    "include/linux/refcount.h": ["refcount_inc_not_zero", "refcount_dec_and_test"],
    "include/linux/seqlock.h": ["read_seqcount_begin", "read_seqcount_retry", "write_seqcount_begin", "write_seqcount_end"],
    "include/linux/rcupdate.h": ["rcu_read_lock", "rcu_read_unlock", "call_rcu"],
    "kernel/rcu/tree.c": ["synchronize_rcu", "call_rcu"],
    "include/linux/lockdep.h": ["lockdep_assert_held", "lockdep_assert_not_held"],
    "kernel/kcsan/core.c": ["kcsan", "watchpoint"],
}
normalize = lambda value: re.sub(r"\s+", " ", value)
for relative, fragments in checks.items():
    text = normalize((root / relative).read_text(encoding="utf-8", errors="replace"))
    position = -1
    for fragment in fragments:
        next_position = text.find(fragment, position + 1)
        if next_position < 0:
            raise SystemExit(f"FAIL ordered landmark {relative}: {fragment}")
        position = next_position
        print(f"landmark={relative}:{fragment}")

after = hashlib.sha256(makefile.read_bytes()).hexdigest()
if before != after:
    raise SystemExit("FAIL source Makefile changed")
print(f"commit={commit}")
print(f"source_makefile_sha256={before}")
print("source_makefile_unchanged=yes")
print("PASS source-check")
