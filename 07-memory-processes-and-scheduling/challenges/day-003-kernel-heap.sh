#!/usr/bin/env bash
# Inspect heap construction, bump arithmetic, ownership, and current call sites.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"

printf '%s\n' \
  'PURPOSE: audit the canonical 32 KiB mapped bump heap and its lifetime assumptions.' \
  'PREDICT: 16-byte alignment, no free path, and permanent task-record/stack allocations.' \
  'EVIDENCE: allocator source, all kalloc call sites, and checked heap marker.'

printf '\n[heap construction and allocator]\n'
awk 'NR>=6 && NR<=9 || NR>=171 && NR<=182 || NR>=250 && NR<=261 {print}' \
  "$TOYOS/src/memory.c"

printf '\n[current allocation owners]\n'
awk '/kalloc\(/ {printf "%s:%d:%s\n", FILENAME, FNR, $0}' \
  "$TOYOS"/src/*.c "$TOYOS"/include/*.h

printf '\n[canonical heap observation]\n'
make -C "$TOYOS" --no-print-directory week7
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week7/toyos.iso" 33 \
  "SELFTEST: kernel heap PASS"

printf '%s\n' \
  'PASS: mapped heap construction and one allocation completed in the selected run.' \
  'LIMIT: no exhaustion, reclamation, poisoning, guard page, or concurrent allocation is tested.'
