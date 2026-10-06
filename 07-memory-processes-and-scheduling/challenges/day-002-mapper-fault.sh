#!/usr/bin/env bash
# Contrast a canonical successful mapping with the injected missing mapping.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week7/toyos.elf"
FAULT_ELF="$TOYOS/build/week7-page/toyos.elf"

printf '%s\n' \
  'PURPOSE: relate page-table construction and TLB invalidation to #PF evidence.' \
  'PREDICT: 0x40000000 maps successfully; 0x70000000 yields vector 14, not-present write supervisor.' \
  'EVIDENCE: normal/fault serial checks plus exact-ELF source and disassembly.'

printf '\n[mapper contract]\n'
awk 'NR>=131 && NR<=169 || NR>=242 && NR<=248 {print}' "$TOYOS/src/memory.c"

printf '\n[successful mapping observation]\n'
make -C "$TOYOS" --no-print-directory week7
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week7/toyos.iso" 33 \
  "SELFTEST: mapper PASS"

printf '\n[injected page-fault observation]\n'
make -C "$TOYOS" --no-print-directory expected-fault-week7-page

printf '\n[exact artifacts]\n'
nm -n "$ELF" | awk '$3=="map_page" || $3=="bootstrap_pml4" {print}'
objdump -d --disassemble=map_page "$ELF"
objdump -dS --disassemble=kernel_main "$FAULT_ELF"

printf '%s\n' \
  'PASS: selected mapped and unmapped paths produced their predicted evidence.' \
  'LIMIT: no huge-page split, permission rewrite, SMP shootdown, or arbitrary flags were tested.'
