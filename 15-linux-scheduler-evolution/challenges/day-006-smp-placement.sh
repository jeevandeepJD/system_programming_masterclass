#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
make -C "$ROOT/projects/linux-scheduler" build
"$ROOT/projects/linux-scheduler/scripts/check-source.py" >/dev/null
"$ROOT/projects/linux-scheduler/build/affinity_lab"
printf 'PASS week15-day6\n'
