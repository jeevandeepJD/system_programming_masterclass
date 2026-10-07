#!/usr/bin/env bash
# Trace the DPL3 int 0x80 ABI, saved frame, dispatcher, and iretq return.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week8/toyos.elf"
FAULT_ELF="$TOYOS/build/week8-syscall/toyos.elf"

printf '%s\n' \
  'PURPOSE: connect gate entry, register ABI, interrupt_frame ownership, dispatch, and return.' \
  'PREDICT: normal status 33; unknown RAX returns -38 with marker and host status 45.' \
  'EVIDENCE: both checked profiles plus current user/stub/dispatcher source and exact disassembly.'

printf '\n[register ABI in the user image]\n'
awk 'NR>=1 && NR<=69 {print}' "$TOYOS/src/user_program.S"

printf '\n[gate, frame, dispatch, and return contracts]\n'
awk 'NR>=58 && NR<=103 {print}' "$TOYOS/src/isr.S"
awk 'NR>=10 && NR<=17 {print}' "$TOYOS/include/toyos.h"
awk 'NR>=184 && NR<=263 {print}' "$TOYOS/src/process.c"

printf '\n[normal integrated syscall path]\n'
make -C "$TOYOS" --no-print-directory week8
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week8/toyos.iso" 33 \
  "SELFTEST: ring3/TSS/int80 syscalls PASS"

printf '\n[injected unknown syscall]\n'
make -C "$TOYOS" --no-print-directory expected-fault-week8-syscall

printf '\n[exact entry and dispatch instructions]\n'
nm -n "$ELF" | awk '$3=="syscall_int_stub" || $3=="interrupt_handler" ||
  $3=="syscall_dispatch" {print}'
objdump -d --disassemble=syscall_dispatch "$FAULT_ELF"

printf '%s\n' \
  'PASS: canonical int80 calls returned and selected unknown dispatch was rejected as predicted.' \
  'LIMIT: the injected profile calls the dispatcher directly; it does not retest gate entry or iretq.'
