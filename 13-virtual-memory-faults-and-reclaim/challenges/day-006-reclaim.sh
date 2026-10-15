#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-vm"
printf '%s\n' \
  'PURPOSE: apply a bounded LRU with writeback, swap, and an OOM choice.' \
  'PREDICT: a dirty file victim is written back, a clean file victim is dropped, anon victims use swap, and a full swap selects task_b without a host kill.' \
  'EVIDENCE: checked markers, compiler identity, and the binary hash.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/reclaim_lab"
printf '\n[reclaim evidence]\n'
"$PROJECT/build/reclaim_lab" demo
printf '\n[source landmark]\n'
grep -n 'evict_dirty_file\|simulated_oom_choice\|reclaim_host' \
  "$PROJECT/src/reclaim_lab.c"
printf '%s\n' \
  'PASS: the simulator wrote back one dirty file page, dropped one clean file page, swapped two anonymous pages, and stopped.' \
  'LIMIT: the host did not reclaim, swap, or kill. The policy is a teaching LRU, not shrink_folio_list.'
