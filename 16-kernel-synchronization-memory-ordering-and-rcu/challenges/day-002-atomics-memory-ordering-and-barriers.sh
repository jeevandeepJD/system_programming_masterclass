#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
make -C "$ROOT/projects/linux-kernel-sync" "build/message_order"
"$ROOT/projects/linux-kernel-sync/scripts/check-source.py" >/dev/null
"$ROOT/projects/linux-kernel-sync/build/message_order"
printf 'PASS week16-day2\n'
