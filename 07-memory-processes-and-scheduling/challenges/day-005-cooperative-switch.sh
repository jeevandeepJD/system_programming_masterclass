#!/usr/bin/env bash
# Audit cooperative scheduling against the AMD64 ABI and exact switch code.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week7/toyos.elf"

printf '%s\n' \
  'PURPOSE: connect scheduler policy, ABI register ownership, and switch mechanism.' \
  'PREDICT: callee-saved GPRs plus RSP/RIP survive; task 1 voluntary yields complete.' \
  'EVIDENCE: build flags, C policy, source/ELF disassembly, and cooperative marker.'

printf '\n[ABI-relevant build and context]\n'
awk '/^CFLAGS :=/{show=1} show{print} /-Wall/{if(show){exit}}' "$TOYOS/Makefile"
awk '/struct context/{show=1} show{print} /^};/{if(show){exit}}' "$TOYOS/include/toyos.h"

printf '\n[scheduler policy]\n'
awk 'NR>=90 && NR<=96 || NR>=130 && NR<=161 {print}' "$TOYOS/src/tasks.c"

printf '\n[canonical cooperative observation]\n'
make -C "$TOYOS" --no-print-directory week7
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week7/toyos.iso" 33 \
  "SELFTEST: cooperative switch PASS"

printf '\n[exact switch disassembly]\n'
objdump -d --disassemble=context_switch "$ELF"

printf '%s\n' \
  'PASS: the selected cooperative path agrees with the saved context contract.' \
  'LIMIT: the test does not cover SIMD state, arbitrary register patterns, SMP, or every interrupt point.'
