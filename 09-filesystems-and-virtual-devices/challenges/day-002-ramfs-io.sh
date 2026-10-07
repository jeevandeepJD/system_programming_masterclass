#!/usr/bin/env bash
# Trace tiny ramfs lookup, open, offset, read, write, and missing-path behavior.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
NORMAL="$TOYOS/build/week9/toyos.elf"
FAULT="$TOYOS/build/week9-filesystem/toyos.elf"

printf '%s\n' \
  'PURPOSE: verify exact lookup and bounded ramfs state transitions.' \
  'PREDICT: normal I/O passes; /missing returns -2 and the fault profile has host status 47.' \
  'EVIDENCE: source contracts, normal checked QEMU, dedicated rejection profile, and exact ELFs.'

printf '\n[lookup, read, write, and initialization contracts]\n'
awk 'NR>=25 && NR<=129 {print}' "$TOYOS/src/vfs.c"

printf '\n[normal ramfs observation]\n'
make -C "$TOYOS" --no-print-directory week9
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week9/toyos.iso" 33 \
  "VFS: ramfs inode/path/open/read/write PASS" -device edu

printf '\n[controlled missing-path rejection]\n'
make -C "$TOYOS" --no-print-directory expected-fault-week9-filesystem

printf '\n[artifact identities and operations]\n'
sha256sum "$NORMAL" "$FAULT"
nm -n "$NORMAL" | awk '$3=="vfs_open" || $3=="vfs_read" ||
  $3=="vfs_write" || $3=="vfs_init" {print}'

printf '%s\n' \
  'PASS: implemented lookup/I/O completed and the selected missing path was rejected as predicted.' \
  'LIMIT: no EOF-specific, slot-exhaustion, read-only-write, overflow, close, or concurrent test is synthesized.'
