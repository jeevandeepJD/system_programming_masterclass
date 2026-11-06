#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
make -C "$ROOT/projects/linux-kernel-sync" "build/mutex_pi_lab"
"$ROOT/projects/linux-kernel-sync/scripts/check-source.py" >/dev/null
"$ROOT/projects/linux-kernel-sync/build/mutex_pi_lab"
printf 'PASS week16-day4\n'
