#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-kernel-contexts"
export LINUX_SRC="${LINUX_SRC:-/home/jd/Documents/linux-6.12}"
printf '%s\n' \
  'PURPOSE: read preempt_count sleep rules, then show a userspace wait is not atomic context.' \
  'PREDICT: hardirq, softirq, and WQ_BH may not sleep; a worker thread and process context may.' \
  'EVIDENCE: in_hardirq, might_sleep, the in_atomic warning, and the poll markers.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/context_lab"
"$PROJECT/scripts/check-source.sh" 6
printf '\n[userspace wait]\n'
"$PROJECT/build/context_lab" demo
printf '%s\n' \
  'PASS: the snapshot contains the context masks, and the zero-timeout poll did not wait.' \
  'LIMIT: kernel_preempt_claim=none. The lab did not read preempt_count. Scheduler choice is Week 15.'
