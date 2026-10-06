#!/usr/bin/env bash
# Trace the canonical PIT/PIC IRQ0 path into involuntary task scheduling.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week7/toyos.elf"

printf '%s\n' \
  'PURPOSE: connect legacy timer setup, IRQ frame ownership, EOI, ticks, and preemption.' \
  'PREDICT: IRQ0 arrives as vector 32 near 100 Hz and task 0 is involuntarily switched.' \
  'EVIDENCE: exact source chain, IRQ symbols, task marker, and preemption marker.'

printf '\n[PIT/PIC and IRQ dispatch]\n'
awk 'NR>=174 && NR<=193 || NR>=196 && NR<=210 || NR>=222 && NR<=243 {print}' \
  "$TOYOS/src/arch.c"
printf '\n[IRQ entry and task probe]\n'
awk 'NR>=52 && NR<=96 {print}' "$TOYOS/src/isr.S"
awk 'NR>=81 && NR<=96 || NR>=155 && NR<=166 {print}' "$TOYOS/src/tasks.c"

printf '\n[canonical preemption observations]\n'
make -C "$TOYOS" --no-print-directory week7
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week7/toyos.iso" 33 \
  "KERNEL-TASK 0: preemption probe"
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week7/toyos.iso" 33 \
  "SELFTEST: PIT/PIC preemption PASS"

printf '\n[IRQ symbols]\n'
nm -n "$ELF" | awk '$3=="irq0_stub" || $3=="interrupt_handler" || $3=="scheduler_tick" {print}'

printf '%s\n' \
  'PASS: the selected PIT/PIC path caused the expected preemption evidence.' \
  'LIMIT: timing precision, nested IRQs, APIC/x2APIC, SMP, and latency are not validated.'
