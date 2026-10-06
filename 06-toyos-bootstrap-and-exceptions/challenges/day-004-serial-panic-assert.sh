#!/usr/bin/env bash
# Compare ToyOS normal serial completion with its deliberate assertion panic.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"

printf '%s\n' \
  'PURPOSE: distinguish normal completion from the C-level ASSERT/panic path.' \
  'PREDICT: normal status 33; assertion status 37 with a PANIC and expected marker.' \
  'EVIDENCE: source path, serial markers, host status checks, and log checks.'

printf '\n[serial and panic implementation]\n'
awk 'NR>=53 && NR<=126 {printf "%4d  %s\n", NR, $0}' "$TOYOS/src/arch.c"

printf '\n[normal profile]\n'
make -C "$TOYOS" --no-print-directory week6
"$TOYOS/scripts/check-qemu.sh" \
  "$TOYOS/build/week6/toyos.iso" 33 "WEEK6: PASS"

printf '\n[assertion profile]\n'
make -C "$TOYOS" --no-print-directory expected-fault-week6-assert

printf '%s\n' \
  'PASS: normal and assertion protocols produced their distinct checked outcomes.' \
  'LIMIT: emulated COM1 output does not measure a physical UART or every panic path.'
