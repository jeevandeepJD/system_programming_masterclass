#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-vm"
printf '%s\n' \
  'PURPOSE: record brk and maps results without treating them as a libc contract.' \
  'PREDICT: a small and a large malloc appear in some maps region; munmap removes the explicit mapping.' \
  'EVIDENCE: checked markers, compiler identity, and the binary hash.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/allocator_lab"
printf '\n[allocator evidence]\n'
"$PROJECT/build/allocator_lab" demo
printf '\n[source landmark]\n'
grep -n 'glibc_mmap_threshold_asserted\|regions_are_observations\|munmap_absent_from_maps' \
  "$PROJECT/src/allocator_lab.c"
printf '%s\n' \
  'PASS: the run recorded where this libc placed the objects and that munmap dropped the VMA.' \
  'LIMIT: brk_changed and the region names are this run. They are not glibc M_MMAP_THRESHOLD.'
