#!/usr/bin/env bash
# Audit GDT/TSS state and the canonical iretq transition into ring 3.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week8/toyos.elf"

printf '%s\n' \
  'PURPOSE: connect x86 privilege descriptors, TSS RSP0, and the crafted iretq frame.' \
  'PREDICT: selectors 0x1b/0x23 enter CPL3; int80 returns through TSS stack; host status 33.' \
  'EVIDENCE: canonical source, checked QEMU marker, symbols, and exact-ELF disassembly.'

printf '\n[GDT, TSS, and user gate contracts]\n'
awk 'NR>=143 && NR<=179 || NR>=204 && NR<=218 || NR>=232 && NR<=237 {print}' \
  "$TOYOS/src/arch.c"

printf '\n[initial user-return frame]\n'
cat "$TOYOS/src/user_entry.S"

printf '\n[canonical ring-3 observation]\n'
make -C "$TOYOS" --no-print-directory week8
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week8/toyos.iso" 33 \
  "SELFTEST: ring3/TSS/int80 syscalls PASS"

printf '\n[exact artifact entry symbols and instructions]\n'
nm -n "$ELF" | awk '$3=="arch_set_kernel_stack" || $3=="enter_user" ||
  $3=="syscall_int_stub" || $3=="syscall_dispatch" {print}'
objdump -d --disassemble=enter_user "$ELF"
objdump -d --disassemble=syscall_int_stub "$ELF"

printf '%s\n' \
  'PASS: the selected canonical path reached CPL3, entered through int80, and exited normally.' \
  'LIMIT: static descriptors plus one QEMU path do not test malformed return frames, SMP, or hardware diversity.'
