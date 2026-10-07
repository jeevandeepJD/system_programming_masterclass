#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-system-programming"
printf '%s\n' \
  'PURPOSE: connect pthread creation/join to shared process state, per-thread execution state, and object lifetime.' \
  'PREDICT: basic returns the caller token and ownership returns 42; thread IDs and scheduling are not predicted.' \
  'EVIDENCE: canonical source, binary hash, environment identity, and checked program markers.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/thread_lab"
printf '\n[basic create/join]\n'
"$PROJECT/build/thread_lab" basic
printf '\n[heap ownership transfer]\n'
"$PROJECT/build/thread_lab" ownership
printf '\n[lifetime-relevant source]\n'
awk 'NR>=53 && NR<=117 {print}' "$PROJECT/src/thread_lab.c"
printf '%s\n' \
  'PASS: create/join and one-free ownership protocols completed.' \
  'LIMIT: no raw clone trace, detached-thread lifetime, TLS ABI inspection, stack exhaustion, or scheduling guarantee.'
