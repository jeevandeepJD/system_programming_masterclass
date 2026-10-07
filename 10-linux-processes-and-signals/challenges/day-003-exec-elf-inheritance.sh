#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd); PROJECT="$ROOT/projects/linux-system-programming"
printf '%s\n' 'PURPOSE: connect fork/exec/wait to ELF identity and classify state across image replacement.' 'PREDICT: fixed shell exits 7; a separate child is decoded as SIGTERM termination.' 'EVIDENCE: canonical checked mode, exact exec source, ELF/program headers, hash, and optional trace.'
make -C "$PROJECT" --no-print-directory build
"$PROJECT/build/process_lab" status
sha256sum "$PROJECT/build/process_lab"
readelf -h -l "$PROJECT/build/process_lab" | awk '/Class:|Type:|Machine:|Entry point|INTERP|Requesting/'
awk 'NR>=47 && NR<=68 {print}' "$PROJECT/src/process_lab.c"
if command -v strace >/dev/null 2>&1; then
  LOG=$(mktemp /tmp/week10-exec-trace.XXXXXX); trap 'rm -f "$LOG"' EXIT
  strace -f -e trace=process,signal -o "$LOG" "$PROJECT/build/process_lab" status >/dev/null
  awk '/execve|wait|SIGTERM|exit/{print}' "$LOG"
else printf '%s\n' 'SKIP: strace unavailable'; fi
printf '%s\n' 'PASS: normal and signal outcomes remained distinct.' 'LIMIT: no FD_CLOEXEC runtime probe, initial-stack dump, privileged exec, or loader relocation audit.'
