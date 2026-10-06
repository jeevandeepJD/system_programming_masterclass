#!/usr/bin/env bash
# Inspect and validate the canonical ToyOS freestanding build contract.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week6/toyos.elf"

printf '%s\n' \
  'PURPOSE: connect compiler/linker options to the canonical Week 6 ELF.' \
  'PREDICT: entry is _start; kernel_main is present; no unresolved symbols remain.' \
  'EVIDENCE: selected flags, ELF entry/symbols, and undefined-symbol count.'

printf '\n[build contract]\n'
awk '/^(CPPFLAGS|CFLAGS|ASFLAGS|LDFLAGS) :=/{print}' "$TOYOS/Makefile"

printf '\n[canonical build]\n'
make -C "$TOYOS" --no-print-directory week6

printf '\n[ELF identity and entry]\n'
readelf -h "$ELF" | awk '/Class:|Machine:|Type:|Entry point address:/'

printf '\n[entry symbols]\n'
nm -n "$ELF" | awk '$3=="_start" || $3=="kernel_main" {print}'

undefined=$(nm -u "$ELF" | wc -l)
printf '\n[link result]\nundefined symbols: %s\n' "$undefined"
test "$undefined" -eq 0

printf '%s\n' \
  'PASS: the inspected ELF satisfies this wrapper’s static contract checks.' \
  'LIMIT: static linkage does not prove successful boot or runtime behavior.'
