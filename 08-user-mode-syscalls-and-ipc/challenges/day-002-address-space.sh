#!/usr/bin/env bash
# Inspect the process PML4, shared kernel mappings, user permissions, and CR3.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week8/toyos.elf"

printf '%s\n' \
  'PURPOSE: audit address-space ownership, kernel sharing, U/S propagation, and activation.' \
  'PREDICT: a distinct root shares supervisor kernel reachability and maps user leaves through U/S parents.' \
  'EVIDENCE: source contracts, normal marker/status, and exact mapper/CR3 instructions.'

printf '\n[address-space construction and activation]\n'
awk 'NR>=139 && NR<=179 || NR>=189 && NR<=225 {print}' "$TOYOS/src/memory.c"

printf '\n[user image and stack policy]\n'
awk 'NR>=263 && NR<=306 {print}' "$TOYOS/src/process.c"

printf '\n[canonical process observation]\n'
make -C "$TOYOS" --no-print-directory week8
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week8/toyos.iso" 33 \
  "PROCESS: PID 1 private PML4 and embedded ELF loaded"

printf '\n[exact artifact symbols and activation]\n'
nm -n "$ELF" | awk '$3=="address_space_create" || $3=="map_page_in" ||
  $3=="address_space_activate" || $3=="process_start" {print}'
objdump -d --disassemble=address_space_activate "$ELF"

printf '%s\n' \
  'PASS: the canonical process root, user mappings, and activation composed successfully.' \
  'LIMIT: no second process, teardown, hostile isolation test, PCID, SMP shootdown, NX, or W^X is exercised.'
