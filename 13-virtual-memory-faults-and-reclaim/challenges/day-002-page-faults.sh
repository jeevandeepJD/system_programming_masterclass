#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-vm"
printf '%s\n' \
  'PURPOSE: observe demand zero-fill and a minor fault without treating the count as exact.' \
  'PREDICT: the first load reads 0, mincore flips after the touch, and the minor-fault delta is at least 1.' \
  'EVIDENCE: checked markers, compiler identity, and the binary hash.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/fault_lab"
printf '\n[fault evidence]\n'
"$PROJECT/build/fault_lab" demo
printf '\n[source landmark]\n'
grep -n 'zero_fill\|fault_count_caveat\|read_rss_not_asserted' \
  "$PROJECT/src/fault_lab.c"
printf '%s\n' \
  'PASS: the anonymous page was zero-filled and became resident after a store.' \
  'LIMIT: ru_minflt can include other faults. Rss after a read is printed and not asserted.'
