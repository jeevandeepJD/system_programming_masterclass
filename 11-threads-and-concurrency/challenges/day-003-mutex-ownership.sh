#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-system-programming"
printf '%s\n' \
  'PURPOSE: verify a mutex-protected invariant, explicit heap ownership transfer, and one global lock order.' \
  'PREDICT: count=800000, joined_value=42 with one parent free, and ordered locking completes.' \
  'EVIDENCE: checked modes plus source ranges for critical sections, transfer, and lock order.'
make -C "$PROJECT" --no-print-directory build
"$PROJECT/build/thread_lab" mutex
"$PROJECT/build/thread_lab" ownership
"$PROJECT/build/deadlock_lab" ordered
printf '\n[mutex and ownership source]\n'
awk 'NR>=32 && NR<=47 || NR>=92 && NR<=118 {print}' "$PROJECT/src/thread_lab.c"
printf '\n[global lock order source]\n'
awk 'NR>=69 && NR<=84 {print}' "$PROJECT/src/deadlock_lab.c"
if command -v strace >/dev/null 2>&1; then
  TRACE=$(mktemp /tmp/week11-futex.XXXXXX)
  strace -f -e trace=futex -o "$TRACE" "$PROJECT/build/thread_lab" mutex >/dev/null
  echo "EVIDENCE: optional futex-boundary trace captured at $TRACE"
else
  echo 'SKIP: strace unavailable; no futex-boundary evidence.'
fi
printf '%s\n' \
  'PASS: protected invariant, ownership lifecycle, and lock-order policy completed.' \
  'LIMIT: trace presence/absence does not count all mutex operations; no fairness, robust-mutex, or priority claim.'
