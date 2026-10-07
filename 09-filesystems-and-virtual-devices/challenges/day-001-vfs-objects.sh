#!/usr/bin/env bash
# Audit Week 9 VFS object identity, ownership, and open-file offsets.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week9/toyos.elf"

printf '%s\n' \
  'PURPOSE: distinguish path, inode, open file, descriptor, owner, and lifetime.' \
  'PREDICT: two static inodes and four static file slots support independent per-open offsets.' \
  'EVIDENCE: canonical source, exact symbols/artifact hash, and checked Week 9 execution.'

printf '\n[object and lifetime definitions]\n'
awk 'NR>=5 && NR<=24 || NR>=43 && NR<=72 || NR>=111 && NR<=129 {print}' \
  "$TOYOS/src/vfs.c"

printf '\n[canonical observation]\n'
make -C "$TOYOS" --no-print-directory week9
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week9/toyos.iso" 33 \
  "VFS: ramfs inode/path/open/read/write PASS" -device edu

printf '\n[artifact identity and object symbols]\n'
sha256sum "$ELF"
nm -n "$ELF" | awk '$3=="inodes" || $3=="files" || $3=="vfs_open" ||
  $3=="vfs_read" || $3=="vfs_write" {print}'

printf '%s\n' \
  'PASS: the checked image used static inode identity and per-open file-slot state.' \
  'LIMIT: no dentry, directories, close, references, reclaim, concurrency, mount, or persistence is tested.'
