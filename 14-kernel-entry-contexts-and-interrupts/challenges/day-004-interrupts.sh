#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-kernel-contexts"
export LINUX_SRC="${LINUX_SRC:-/home/jd/Documents/linux-6.12}"
printf '%s\n' \
  'PURPOSE: find the IRQ descriptor path, then read /proc/interrupts only if it opens.' \
  'PREDICT: set_intr_gate, IRQF_SHARED, handle_level_irq, gic_eoi_irq, and the PLIC claim comment are in v6.12.' \
  'EVIDENCE: those landmarks, and a readable or unreadable proc file. Counts are not assertions.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/interrupts_lab"
"$PROJECT/scripts/check-source.sh" 4
printf '\n[userspace proc view]\n'
"$PROJECT/build/interrupts_lab" demo
if [ -r /proc/interrupts ]; then
  printf '\n[proc interrupts summary]\n'
  awk '
    NR == 1 { printf "header_has_cpu=%s\n", (index($0, "CPU") ? "yes" : "no") }
    index($0, ",") { commas++; if (commas <= 20) print }
    END { printf "comma_lines=%d\n", commas + 0 }
  ' /proc/interrupts
else
  printf '%s\n' 'proc_interrupts=unreadable' 'comma_lines=skipped'
fi
printf '%s\n' \
  'PASS: the descriptor, shared-line flag, level flow, and chip end paths are in this snapshot.' \
  'LIMIT: reading /proc/interrupts is not an IRQ handler and does not request an IRQ.'
