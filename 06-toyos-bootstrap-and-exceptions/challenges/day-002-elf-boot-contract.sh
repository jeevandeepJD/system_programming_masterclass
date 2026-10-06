#!/usr/bin/env bash
# Inspect the linked ELF, Multiboot2 recognition, and packaged GRUB contract.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week6/toyos.elf"

printf '%s\n' \
  'PURPOSE: trace linker and Multiboot2 choices into the built boot artifact.' \
  'PREDICT: ELF64 x86-64, _start entry, early .multiboot2, and a 1 MiB kernel base.' \
  'EVIDENCE: readelf metadata, grub2-file result, symbols, and staged grub.cfg.'

make -C "$TOYOS" --no-print-directory week6

printf '\n[Multiboot2 recognition]\n'
grub2-file --is-x86-multiboot2 "$ELF"
printf 'recognized: %s\n' "$ELF"

printf '\n[ELF header]\n'
readelf -h "$ELF" | awk '/Class:|Type:|Machine:|Entry point address:/'

printf '\n[relevant sections]\n'
readelf -SW "$ELF" | awk '/\.multiboot2|\.text[[:space:]]|\.rodata|\.data[[:space:]]|\.bss/'

printf '\n[loadable segments]\n'
readelf -lW "$ELF" | awk '/^  LOAD/{print}'

printf '\n[boundary symbols]\n'
nm -n "$ELF" | awk '$3=="_start" || $3=="__kernel_start" || $3=="__kernel_end" {print}'

printf '\n[packaged GRUB configuration]\n'
cat "$TOYOS/build/week6/iso/boot/grub/grub.cfg"

printf '%s\n' \
  'PASS: the exact Week 6 ELF is recognized and its package contract is inspectable.' \
  'LIMIT: recognition and packaging do not alone prove transfer or execution.'
