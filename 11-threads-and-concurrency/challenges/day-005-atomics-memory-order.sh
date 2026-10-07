#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-system-programming"
REPORT=$(mktemp -d /tmp/week11-day5.XXXXXX)
printf '%s\n' \
  'PURPOSE: contrast relaxed atomic counting with a relaxed litmus and inspect one compiler/target mapping.' \
  'PREDICT: atomic count is exact; both-zero count varies and no x86 observation proves portable C behavior.' \
  "EVIDENCE: checked runs, compiler/target identity, source protocol, and assembly under $REPORT."
make -C "$PROJECT" --no-print-directory build
"$PROJECT/build/thread_lab" atomic
"$PROJECT/build/litmus_lab"
compiler=${CC:-cc}
"$compiler" --version | awk 'NR==1'
uname -m
if "$compiler" -std=c17 -D_POSIX_C_SOURCE=200809L -pthread -O2 -S \
    "$PROJECT/src/thread_lab.c" -o "$REPORT/thread_lab.s" 2>"$REPORT/assembly.log"; then
  echo "EVIDENCE: host-target assembly generated at $REPORT/thread_lab.s"
  awk '/increment_atomic:/{show=1} show{print} show && /size.*increment_atomic/{exit}' "$REPORT/thread_lab.s"
else
  echo 'SKIP: assembly generation unavailable; see assembly.log.'
fi
printf '%s\n' \
  'PASS: atomic count invariant and bounded relaxed litmus completed.' \
  'LIMIT: finite observations and one host assembly do not prove C portability, exhaustive outcomes, or universal ISA lowering.'
