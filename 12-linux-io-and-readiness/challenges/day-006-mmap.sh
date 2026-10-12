#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-io"
printf '%s\n' \
  'PURPOSE: compare a MAP_SHARED store, msync, pread, and pwrite without ranking mechanisms.' \
  'PREDICT: the store is visible to pread; pwrite is visible in the mapping; the two timings are one observation.' \
  'EVIDENCE: match markers, msync completion, and performance_claim=none.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/mmap_lab"
printf '\n[MAP_SHARED, msync, read comparison]\n'
"$PROJECT/build/mmap_lab" demo
printf '%s\n' \
  'PASS: mapping and file I/O observed the same bytes; msync completed as a request.' \
  'LIMIT: one pair of timings is not a universal performance claim, and msync is not a crash-durability proof.'
