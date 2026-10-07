#!/usr/bin/env bash
# Verify bounded page-wise user copying and deterministic bad-pointer rejection.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week8-user-pointer/toyos.elf"

printf '%s\n' \
  'PURPOSE: connect user page-walk permissions, page-boundary copying, and syscall bounds.' \
  'PREDICT: 0x1234 is rejected in software without #PF; marker appears with host status 43.' \
  'EVIDENCE: normal copy marker, injected profile, source contracts, and exact fault-ELF disassembly.'

printf '\n[user translation and copy contracts]\n'
awk 'NR>=226 && NR<=289 {print}' "$TOYOS/src/memory.c"

printf '\n[dispatcher length and error policy]\n'
awk 'NR>=184 && NR<=249 {print}' "$TOYOS/src/process.c"

printf '\n[normal bounded-copy observation]\n'
make -C "$TOYOS" --no-print-directory week8
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week8/toyos.iso" 33 \
  "SELFTEST: bounded user copy and pipe PASS"

printf '\n[injected invalid pointer]\n'
make -C "$TOYOS" --no-print-directory expected-fault-week8-user-pointer

printf '\n[exact rejection artifact]\n'
sha256sum "$ELF"
nm -n "$ELF" | awk '$3=="user_translate" || $3=="user_copy" ||
  $3=="copy_from_user" || $3=="process_start" {print}'
objdump -d --disassemble=copy_from_user "$ELF"

printf '%s\n' \
  'PASS: selected valid copies completed and 0x1234 was rejected before dereference.' \
  'LIMIT: no concurrent unmap, noncanonical-address fuzzing, partial-copy API check, or demand fault is tested.'
