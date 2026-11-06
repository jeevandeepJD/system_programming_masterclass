#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
make -C "$ROOT/projects/linux-kernel-sync" "build/primitive_select"
"$ROOT/projects/linux-kernel-sync/scripts/check-source.py" >/dev/null
"$ROOT/projects/linux-kernel-sync/build/primitive_select"
printf 'PASS week16-day1\n'
