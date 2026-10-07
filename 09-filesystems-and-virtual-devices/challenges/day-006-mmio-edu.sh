#!/usr/bin/env bash
# Verify checked EDU MMIO liveness and invalid-register software rejection.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
NORMAL="$TOYOS/build/week9/toyos.elf"
FAULT="$TOYOS/build/week9-device/toyos.elf"

printf '%s\n' \
  'PURPOSE: distinguish volatile access, register policy, device semantics, and ordering limits.' \
  'PREDICT: liveness returns the inverse; offset 3 is rejected before MMIO with host status 49.' \
  'EVIDENCE: source, exact instructions, normal device check, and dedicated invalid-register profile.'

printf '\n[mapping and checked register access]\n'
awk 'NR>=5 && NR<=15 || NR>=35 && NR<=70 {print}' "$TOYOS/src/edu.c"

printf '\n[normal liveness operation]\n'
make -C "$TOYOS" --no-print-directory week9
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week9/toyos.iso" 33 \
  "EDU: PCI discovery/BAR/MMIO liveness PASS" -device edu

printf '\n[controlled invalid-register rejection]\n'
make -C "$TOYOS" --no-print-directory expected-fault-week9-device

printf '\n[exact access artifacts]\n'
sha256sum "$NORMAL" "$FAULT"
objdump -d --disassemble=edu_checked_write "$FAULT"

printf '%s\n' \
  'PASS: selected MMIO completed and invalid offset 3 was rejected in software as predicted.' \
  'LIMIT: volatile is not a portable ordering API; no physical hardware, SMP race, IRQ, or DMA is tested.'
