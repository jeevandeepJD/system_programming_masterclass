#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-kernel-contexts"
export LINUX_SRC="${LINUX_SRC:-/home/jd/Documents/linux-6.12}"
printf '%s\n' \
  'PURPOSE: separate softirq, threaded IRQ, and workqueue, then run a userspace poll analog.' \
  'PREDICT: an empty eventfd poll returns 0; after a write it returns 1; a 20 ms timerfd becomes ready.' \
  'EVIDENCE: __do_softirq, request_threaded_irq, queue_work_on, WQ_BH, and analog markers.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/analog_lab"
"$PROJECT/scripts/check-source.sh" 5
printf '\n[softirq names and host threads]\n'
grep -n 'This API is deprecated' "$LINUX_SRC/include/linux/interrupt.h"
grep -n 'ksoftirqd/%u' "$LINUX_SRC/kernel/softirq.c"
if [ -r /proc/softirqs ]; then
  awk 'NR==1 || $1 == "HI:" || $1 == "TASKLET:" { print }' /proc/softirqs
else
  printf '%s\n' 'proc_softirqs=unreadable'
fi
if command -v ps >/dev/null 2>&1; then
  ps -e -o comm= | awk '
    /^ksoftirqd\// { k++ }
    /^irq\// { i++ }
    END {
      printf "ksoftirqd_tasks=%s\n", (k+0 ? "present" : "absent")
      printf "irq_threads=%s\n", (i+0 ? "present" : "absent")
    }'
else
  printf '%s\n' 'process_scan=skipped'
fi
printf '\n[userspace analog]\n'
"$PROJECT/build/analog_lab" demo
printf '%s\n' \
  'PASS: the three deferral names are in v6.12, and poll reported the eventfd and timerfd.' \
  'LIMIT: not_a_softirq=yes. eventfd is not __do_softirq, an IRQ thread, or worker_thread.'
