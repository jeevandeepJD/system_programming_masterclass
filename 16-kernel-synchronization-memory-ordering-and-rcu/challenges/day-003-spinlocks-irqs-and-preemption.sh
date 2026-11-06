#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
make -C "$ROOT/projects/linux-kernel-sync" "build/spinlock_lab"
"$ROOT/projects/linux-kernel-sync/scripts/check-source.py" >/dev/null
"$ROOT/projects/linux-kernel-sync/build/spinlock_lab"
printf 'PASS week16-day3\n'
