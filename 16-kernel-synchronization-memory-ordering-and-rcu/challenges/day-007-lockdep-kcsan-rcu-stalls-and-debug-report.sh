#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
make -C "$ROOT/projects/linux-kernel-sync" "build/debug_report"
"$ROOT/projects/linux-kernel-sync/scripts/check-source.py" >/dev/null
"$ROOT/projects/linux-kernel-sync/build/debug_report"
printf 'PASS week16-day7\n'
