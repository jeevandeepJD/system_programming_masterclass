#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-system-programming"
printf '%s\n' \
  'PURPOSE: validate a bounded producer-consumer queue built from mutex-protected predicates and condition loops.' \
  'PREDICT: 2000 values, sum 2001000, empty final queue, and while-predicate marker; wake order is unspecified.' \
  'EVIDENCE: canonical queue assertions and source for wait, transition, notification, sentinel, join, and free.'
make -C "$PROJECT" --no-print-directory build
"$PROJECT/build/queue_lab"
printf '\n[predicate transitions]\n'
awk 'NR>=30 && NR<=53 {print}' "$PROJECT/src/queue_lab.c"
printf '\n[shutdown and result ownership]\n'
awk 'NR>=80 && NR<=116 {print}' "$PROJECT/src/queue_lab.c"
printf '%s\n' \
  'PASS: queue count, sum, predicate loops, shutdown, joins, and frees were checked.' \
  'LIMIT: no wake-order/fairness guarantee, forced spurious wakeup, cancellation, producer-failure recovery, or schedule proof.'
