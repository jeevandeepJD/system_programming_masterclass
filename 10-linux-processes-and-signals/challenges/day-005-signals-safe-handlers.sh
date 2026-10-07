#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd); PROJECT="$ROOT/projects/linux-system-programming"
printf '%s\n' 'PURPOSE: observe blocked pending delivery and keep asynchronous handler work signal-safe.' 'PREDICT: SIGUSR1 is pending before unblock; SIGUSR2 contributes one self-pipe byte; failures are EINVAL/ESRCH.' 'EVIDENCE: canonical modes, exact handler source, artifact hash, and optional signal trace.'
make -C "$PROJECT" --no-print-directory build
sha256sum "$PROJECT/build/signal_lab"
"$PROJECT/build/signal_lab" pending
"$PROJECT/build/signal_lab" self-pipe
"$PROJECT/build/signal_lab" failures
awk 'NR<=105 {print}' "$PROJECT/src/signal_lab.c"
if command -v strace >/dev/null 2>&1; then
  LOG=$(mktemp /tmp/week10-signal-trace.XXXXXX); trap 'rm -f "$LOG"' EXIT
  strace -e trace=signal,read,write -o "$LOG" "$PROJECT/build/signal_lab" self-pipe >/dev/null
  awk '/SIGUSR2|rt_sig|write|read/{print}' "$LOG"
else printf '%s\n' 'SKIP: strace unavailable'; fi
printf '%s\n' 'PASS: pending and self-pipe invariants passed.' 'LIMIT: single-threaded, one event, no pipe saturation, real-time queue, alternate stack, or frame dump.'
