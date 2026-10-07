#!/usr/bin/env bash
# Trace Week 9 user open/read/write registers, copying, descriptors, and lifetimes.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
KERNEL="$TOYOS/build/week9/toyos.elf"
USER="$TOYOS/build/week9/user.elf"

printf '%s\n' \
  'PURPOSE: follow user pointers and fd values through checked copies into VFS state.' \
  'PREDICT: /hello is read through fd 5..8, /note is written, and normal host status is 33.' \
  'EVIDENCE: user source/disassembly, dispatcher/VFS contracts, hashes, and checked output.'

printf '\n[user open/read/write ABI]\n'
awk 'NR>=39 && NR<=65 || NR>=79 && NR<=96 {print}' "$TOYOS/src/user_program.S"

printf '\n[path copy and descriptor dispatch]\n'
awk 'NR>=169 && NR<=183 || NR>=184 && NR<=263 {print}' \
  "$TOYOS/src/process.c"

printf '\n[canonical user-to-ramfs observation]\n'
make -C "$TOYOS" --no-print-directory week9
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week9/toyos.iso" 33 \
  "USER: ramfs open/read through fd" -device edu

printf '\n[exact artifacts and user instructions]\n'
sha256sum "$KERNEL" "$USER"
objdump -d --disassemble=_user_start "$USER"

printf '%s\n' \
  'PASS: the exact user ELF reached bounded path/data copies and descriptor-backed ramfs operations.' \
  'LIMIT: no libc errno, per-process fd table, close, dup, rollback, blocking, or concurrent lifetime exists.'
