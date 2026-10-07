#!/usr/bin/env bash
# Audit x86 PCI config-space enumeration, BAR0 decoding, and EDU identification.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week9/toyos.elf"

printf '%s\n' \
  'PURPOSE: connect config-port transactions to EDU ID matching and BAR0 mapping.' \
  'PREDICT: bus0/function0 scan finds 1234:11e8, maps a memory BAR, and validates ID 0x010000ed.' \
  'EVIDENCE: current source, QEMU/tool identity, exact instructions, symbols, and checked device marker.'

printf '\n[PCI address, scan, BAR, and ID contracts]\n'
awk 'NR>=5 && NR<=34 || NR>=44 && NR<=63 {print}' "$TOYOS/src/edu.c"

printf '\n[canonical PCI/EDU observation]\n'
make -C "$TOYOS" --no-print-directory week9
qemu-system-x86_64 --version | awk 'NR==1'
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week9/toyos.iso" 33 \
  "EDU: PCI discovery/BAR/MMIO liveness PASS" -device edu

printf '\n[artifact identity and discovery instructions]\n'
sha256sum "$ELF"
nm -n "$ELF" | awk '$3=="pci_read" || $3=="edu_init" ||
  $3=="map_page" {print}'
objdump -d --disassemble=edu_init "$ELF"

printf '%s\n' \
  'PASS: the canonical x86/QEMU path found the exact EDU ID and mapped its assigned memory BAR.' \
  'LIMIT: no bridge/multifunction walk, ECAM, BAR sizing, 64-bit BAR, resource assignment, IRQ, or DMA is covered.'
