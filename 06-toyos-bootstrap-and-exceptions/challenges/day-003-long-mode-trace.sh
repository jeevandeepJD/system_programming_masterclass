#!/usr/bin/env bash
# Correlate the 32-bit bootstrap, long-mode entry, symbols, and boot regression.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week6/toyos.elf"

printf '%s\n' \
  'PURPOSE: inspect the state-transition code and verify its tested boot path.' \
  'PREDICT: 512 × 2 MiB identity maps 1 GiB; EDI/ESI carry C arguments.' \
  'EVIDENCE: mixed bootstrap disassembly, table/stack symbols, and serial PASS.'

make -C "$TOYOS" --no-print-directory week6

printf '\n[bootstrap and handoff disassembly]\n'
start=$(nm -n "$ELF" | awk '$3=="_start"{print "0x"$1}')
long_mode=$(nm -n "$ELF" | awk '$3=="long_mode_entry"{print "0x"$1}')
kernel_main=$(nm -n "$ELF" | awk '$3=="kernel_main"{print "0x"$1}')
printf '%s\n' '[32-bit entry decoded in i386 mode]'
objdump -D -m i386 --start-address="$start" --stop-address="$long_mode" "$ELF"
printf '%s\n' '[64-bit handoff decoded in x86-64 mode]'
objdump -D -m i386:x86-64 --start-address="$long_mode" \
  --stop-address="$kernel_main" "$ELF"

printf '\n[bootstrap state symbols]\n'
nm -n "$ELF" | awk \
  '$3=="bootstrap_pml4" || $3=="bootstrap_pdpt" || $3=="bootstrap_pd" ||
   $3=="long_mode_entry" || $3=="kernel_main" {print}'

printf '\n[dynamic normal-profile evidence]\n'
make -C "$TOYOS" --no-print-directory test-week6

printf '%s\n' \
  'PASS: static transition evidence and all canonical Week 6 regressions completed.' \
  'LIMIT: the marker appears after arch_init; it is not a cycle-exact mode trace.'
