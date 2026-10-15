#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-kernel-contexts"
export LINUX_SRC="${LINUX_SRC:-/home/jd/Documents/linux-6.12}"
printf '%s\n' \
  'PURPOSE: name the trace hooks in source and record only what this host actually showed.' \
  'PREDICT: trace_irq_handler_entry and irqsoff_trace are in the tree; tracefs is not enabled.' \
  'EVIDENCE: host uname, proc status, and explicit non-claims. Optional tools are make tools.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/report_lab"
"$PROJECT/scripts/check-source.sh" 7
printf '\n[host report]\n'
"$PROJECT/build/report_lab" demo
printf '%s\n' \
  'PASS: the report names this host and does not write tracefs or load a module.' \
  'LIMIT: mastery_claim=none. A SKIP from make tools is a result, not a filled-in trace.' \
  'OPTIONAL: make -C projects/linux-kernel-contexts tools'
