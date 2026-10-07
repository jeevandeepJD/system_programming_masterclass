#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd); PROJECT="$ROOT/projects/linux-system-programming"
printf '%s\n' 'PURPOSE: observe retained exit status, deterministic reparenting, guarded status decoding, and cleanup.' 'PREDICT: Z is observed then reaped; grandchild moves to the subreaper; double wait yields ECHILD.' 'EVIDENCE: bounded canonical zombie/orphan/failure modes and their exact source.'
make -C "$PROJECT" --no-print-directory build
"$PROJECT/build/process_lab" zombie
"$PROJECT/build/process_lab" orphan
"$PROJECT/build/process_lab" failures
awk 'NR>=109 && NR<=197 {print}' "$PROJECT/src/process_lab.c"
printf '%s\n' 'PASS: every created descendant reached a checked wait path.' 'LIMIT: no PID-reuse stress, pidfds, namespace-init dependency, ptrace stops, or supervisor loop.'
