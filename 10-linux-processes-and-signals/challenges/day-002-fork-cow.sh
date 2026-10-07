#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd); PROJECT="$ROOT/projects/linux-system-programming"
printf '%s\n' 'PURPOSE: classify fork lineage and inherited pipe references without overclaiming physical-page COW.' 'PREDICT: two unique children report one parent; record order is not guaranteed; all are reaped.' 'EVIDENCE: canonical tree output/source, artifact hash, regression, and optional narrow process trace.'
make -C "$PROJECT" --no-print-directory build
sha256sum "$PROJECT/build/process_lab"
"$PROJECT/build/process_lab" tree
awk 'NR>=70 && NR<=96 {print}' "$PROJECT/src/process_lab.c"
if command -v strace >/dev/null 2>&1; then
  LOG=$(mktemp /tmp/week10-fork-trace.XXXXXX); trap 'rm -f "$LOG"' EXIT
  strace -f -e trace=process -o "$LOG" "$PROJECT/build/process_lab" tree >/dev/null
  awk '/clone|fork|wait|exit/{print}' "$LOG"
else printf '%s\n' 'SKIP: strace unavailable'; fi
make -C "$PROJECT" --no-print-directory test-week10 >/dev/null
printf '%s\n' 'PASS: lineage and cleanup invariants passed.' 'LIMIT: output does not measure page frames, write faults, multithreaded fork, or scheduler order.'
