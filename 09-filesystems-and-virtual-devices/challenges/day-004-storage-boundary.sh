#!/usr/bin/env bash
# Establish the implemented ramfs boundary versus a conceptual storage stack.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week9/toyos.elf"

printf '%s\n' \
  'PURPOSE: separate direct in-memory file bytes from absent cache/block/journal layers.' \
  'PREDICT: Week 9 links ramfs and EDU liveness but no file page cache, block I/O, or journaling.' \
  'EVIDENCE: build inputs, source/symbol inventory, direct copy loops, and checked canonical execution.'

printf '\n[linked Week 9 implementation]\n'
awk 'NR>=10 && NR<=23 || NR>=56 && NR<=63 || NR>=90 && NR<=97 {print}' \
  "$TOYOS/Makefile"

printf '\n[authoritative ramfs byte arrays and direct copies]\n'
awk 'NR>=5 && NR<=18 || NR>=75 && NR<=109 {print}' "$TOYOS/src/vfs.c"

printf '\n[storage-layer symbol inventory]\n'
make -C "$TOYOS" --no-print-directory week9
nm -n "$ELF" | awk '$3 ~ /vfs|edu|inode|file|block|journal|cache|writeback/ {print}'
if rg -n 'journal|writeback|block_request|page_cache|sector' \
    "$TOYOS/src" "$TOYOS/include"; then
  printf '%s\n' 'NOTE: matched text requires manual classification.'
else
  printf '%s\n' 'OBSERVED: selected implementation has no named page-cache/block/journal path.'
fi

printf '\n[canonical observation]\n'
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week9/toyos.iso" 33 \
  "WEEK9: PASS" -device edu

printf '%s\n' \
  'PASS: evidence supports direct static ramfs storage and a separate EDU liveness path.' \
  'LIMIT: the file→page-cache→filesystem→block chain is conceptual and is not claimed as ToyOS execution.'
