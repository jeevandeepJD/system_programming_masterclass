#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-system-programming"
REPORT=$(mktemp -d /tmp/week11-day2.XXXXXX)
printf '%s\n' \
  'PURPOSE: contrast a checked mutex result with an intentional C data race and honest TSan runtime classification.' \
  'PREDICT: mutex count is exact; racy count is not evidence; TSan may detect or be unavailable/inconclusive.' \
  "EVIDENCE: source accesses, compiler result, runtime exit status, and log under $REPORT."
make -C "$PROJECT" --no-print-directory build
"$PROJECT/build/thread_lab" mutex
printf '\n[intentional bounded race: never a correctness test]\n'
"$PROJECT/build/thread_lab" race
compiler=${CC:-cc}
if "$compiler" -std=c17 -D_POSIX_C_SOURCE=200809L -pthread -fsanitize=thread -O1 \
    "$PROJECT/src/thread_lab.c" -o "$REPORT/thread_lab-tsan" 2>"$REPORT/tsan-build.log"; then
  set +e
  TSAN_OPTIONS=exitcode=66 "$REPORT/thread_lab-tsan" race >"$REPORT/tsan-run.log" 2>&1
  status=$?
  set -e
  if [ "$status" -eq 66 ] && grep -Fi 'data race' "$REPORT/tsan-run.log" >/dev/null; then
    echo 'PASS: ThreadSanitizer reported the intentional race.'
  elif grep -Eqi 'unexpected memory mapping|ThreadSanitizer.*(fatal|failed)|FATAL: ThreadSanitizer' "$REPORT/tsan-run.log"; then
    echo 'SKIP: ThreadSanitizer binary built but runtime is unavailable/incompatible here.'
  else
    echo "SKIP: ThreadSanitizer run produced no recognized race report (status=$status); this is not proof of safety."
  fi
else
  echo 'SKIP: ThreadSanitizer instrumentation could not be built.'
fi
printf '%s\n' "EVIDENCE: TSan logs retained at $REPORT" \
  'LIMIT: dynamic instrumentation covers only executed paths; a correct racy total or absent report proves nothing.'
