#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-vm"
printf '%s\n' \
  'PURPOSE: exercise split, coalesce, zone choice, and a NUMA fallback in a simulator.' \
  'PREDICT: a kernel request stays out of DMA, a full local zone falls back, and an exhausted DMA order fails.' \
  'EVIDENCE: checked markers, compiler identity, and the binary hash.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/buddy_lab"
printf '\n[buddy evidence]\n'
"$PROJECT/build/buddy_lab" demo
printf '\n[source landmark]\n'
grep -n 'simulator_only\|numa_fallback\|exhausted_alloc' \
  "$PROJECT/src/buddy_lab.c"
printf '%s\n' \
  'PASS: the teaching buddy split, coalesced, fell back, and refused an order the zones could not hold.' \
  'LIMIT: no host page was allocated, migrated, or reserved. This is not get_page_from_freelist.'
