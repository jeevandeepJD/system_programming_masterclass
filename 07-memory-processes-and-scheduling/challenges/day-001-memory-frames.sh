#!/usr/bin/env bash
# Audit the canonical Multiboot parser and bounded physical-frame allocator.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week7/toyos.elf"
FAULT_ELF="$TOYOS/build/week7-allocator/toyos.elf"

printf '%s\n' \
  'PURPOSE: connect memory-map validation and bitmap invariants to runtime evidence.' \
  'PREDICT: normal status 33; injected second free is rejected with marker and status 39.' \
  'EVIDENCE: source contracts, both checked profiles, and exact fault-ELF symbols/disassembly.'

printf '\n[map and allocator contracts]\n'
awk 'NR>=1 && NR<=15 || NR>=39 && NR<=43 || NR>=71 && NR<=129 || NR>=184 && NR<=241 {print}' \
  "$TOYOS/src/memory.c"

printf '\n[canonical allocator observation]\n'
make -C "$TOYOS" --no-print-directory week7
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week7/toyos.iso" 33 \
  "SELFTEST: frame allocator PASS"

printf '\n[injected allocator double-free]\n'
make -C "$TOYOS" --no-print-directory expected-fault-week7-allocator

printf '\n[normal and injected artifact symbols]\n'
nm -n "$ELF" | awk '$3=="memory_init" || $3=="frame_alloc" || $3=="frame_free" {print}'
nm -n "$FAULT_ELF" |
  awk '$3=="memory_init" || $3=="frame_is_used" || $3=="frame_free" {print}'
objdump -dS --disassemble=frame_free "$FAULT_ELF"

printf '%s\n' \
  'PASS: normal ownership transfer and the selected double-free rejection matched predictions.' \
  'LIMIT: exhaustion, malformed maps, other misuse, SMP, and RAM above 128 MiB are untested.'
