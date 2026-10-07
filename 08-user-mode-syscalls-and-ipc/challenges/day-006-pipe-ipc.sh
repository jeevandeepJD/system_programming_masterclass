#!/usr/bin/env bash
# Trace fixed descriptors and the bounded global pipe through the user program.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
KERNEL_ELF="$TOYOS/build/week8/toyos.elf"
USER_ELF="$TOYOS/build/week8/user.elf"

printf '%s\n' \
  'PURPOSE: account for process state, fd conventions, copied payload ownership, and pipe limits.' \
  'PREDICT: pipe returns {3,4}; the canonical message fits 64 bytes and round-trips before status 33.' \
  'EVIDENCE: current state/handlers, checked output, and exact kernel/user instructions.'

printf '\n[process and bounded pipe state]\n'
awk 'NR>=8 && NR<=20 || NR>=55 && NR<=70 || NR>=150 && NR<=168 {print}' \
  "$TOYOS/src/process.c"

printf '\n[descriptor and copy policy]\n'
awk 'NR>=184 && NR<=249 {print}' "$TOYOS/src/process.c"

printf '\n[canonical user IPC sequence]\n'
awk 'NR>=17 && NR<=38 || NR>=72 && NR<=96 {print}' "$TOYOS/src/user_program.S"

printf '\n[canonical pipe observation]\n'
make -C "$TOYOS" --no-print-directory week8
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week8/toyos.iso" 33 \
  "SELFTEST: bounded user copy and pipe PASS"

printf '\n[exact artifact symbols and user calls]\n'
nm -n "$KERNEL_ELF" | awk '$3=="pipe_write" || $3=="pipe_read" ||
  $3=="syscall_dispatch" {print}'
objdump -d --disassemble=_user_start "$USER_ELF"

printf '%s\n' \
  'PASS: fixed descriptors, bounded copies, and the one-process pipe round-trip matched predictions.' \
  'LIMIT: this is a 64-byte nonblocking linear global buffer, not shared memory, signals, or POSIX pipe lifetime.'
