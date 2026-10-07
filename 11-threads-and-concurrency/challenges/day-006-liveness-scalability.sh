#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-system-programming"
printf '%s\n' \
  'PURPOSE: distinguish bounded deadlock detection, deadlock prevention, and a noisy false-sharing measurement.' \
  'PREDICT: timed cycle detection exits, global ordering completes, and adjacent/padded timings have no required winner.' \
  'EVIDENCE: return-code markers, lock-order source, elapsed samples, and environment identity.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
getconf _NPROCESSORS_ONLN 2>/dev/null || true
"$PROJECT/build/deadlock_lab" detect
"$PROJECT/build/deadlock_lab" ordered
printf '\n[three timing samples; observational only]\n'
for run in 1 2 3; do
  printf 'sample=%d ' "$run"
  "$PROJECT/build/thread_lab" false-sharing | tr '\n' ' '
  printf '\n'
done
printf '\n[bounded cycle and order source]\n'
awk 'NR>=26 && NR<=84 {print}' "$PROJECT/src/deadlock_lab.c"
printf '%s\n' \
  'PASS: cycle detection was bounded and ordered locking completed.' \
  'LIMIT: no fairness/starvation proof, affinity control, line-layout verification, hardware counters, randomized order, or speedup claim.'
