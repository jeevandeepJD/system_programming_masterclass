#!/usr/bin/env bash
# Correlate loader validation with the exact embedded ELF64 user image.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
KERNEL_ELF="$TOYOS/build/week8/toyos.elf"
USER_ELF="$TOYOS/build/week8/user.elf"

printf '%s\n' \
  'PURPOSE: connect ELF bounds, PT_LOAD mappings, entry, and four-page stack to exact artifacts.' \
  'PREDICT: x86-64 PT_LOAD ranges stay below 0x4001c000 and the canonical image exits normally.' \
  'EVIDENCE: current loader source, readelf output, QEMU check, hashes, and loader symbols.'

printf '\n[ELF validation and loading contracts]\n'
awk 'NR>=4 && NR<=148 || NR>=263 && NR<=281 {print}' "$TOYOS/src/process.c"

printf '\n[build exact canonical image]\n'
make -C "$TOYOS" --no-print-directory week8

printf '\n[user ELF identity, header, and load plan]\n'
sha256sum "$USER_ELF" "$KERNEL_ELF"
readelf -h -l "$USER_ELF"
nm -n "$USER_ELF" | awk '$3=="_user_start" || $3=="hello" ||
  $3=="pipe_fds" || $3=="io_buffer" {print}'

printf '\n[canonical loaded-image observation]\n'
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week8/toyos.iso" 33 \
  "PROCESS: PID 1 private PML4 and embedded ELF loaded"

printf '\n[kernel loader symbols]\n'
nm -n "$KERNEL_ELF" | awk '$3=="load_elf" || $3=="process_init" ||
  $3=="user_elf_start" || $3=="user_elf_end" {print}'

printf '%s\n' \
  'PASS: the exact embedded ELF passed current validation, mapped, entered, and completed.' \
  'LIMIT: this does not fuzz malformed ELF, prove rollback, validate entry containment, or enforce segment W^X.'
