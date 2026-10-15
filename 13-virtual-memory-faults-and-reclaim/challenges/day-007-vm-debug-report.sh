#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-vm"
mkdir -p "$PROJECT/build"
REPORT="$PROJECT/build/week13-report.txt"
printf '%s\n' \
  'PURPOSE: collect process-local VM evidence and optional machine-wide counters.' \
  'PREDICT: WEEK13 PASS; /proc/self markers are present; perf and vmstat pass or skip without inducing pressure.' \
  "EVIDENCE: $REPORT"
make -C "$PROJECT" --no-print-directory build
{
  printf 'Week 13 VM debugging report\nGenerated: '
  date -u '+%Y-%m-%dT%H:%M:%SZ'
  printf '\n== identity ==\n'
  uname -a
  cc --version | awk 'NR==1'
  sha256sum "$PROJECT"/build/*_lab
  printf '\n== deterministic regression ==\n'
  make -C "$PROJECT" --no-print-directory test-week13
  printf '\n== /proc/self observation ==\n'
  "$PROJECT/build/debug_lab" demo
  printf '\n== optional tools ==\n'
  "$PROJECT/scripts/optional-tools.sh"
  printf '\n== comparison landmarks ==\n'
  printf '%s\n' \
    'ToyOS: Week 7 maps a page and invalidates it; the page fault is diagnosed, not demand-filled. Week 8 adds one user root. There is no VMA list, buddy, swap, or OOM killer.' \
    'xv6: pin a revision before quoting kernel/vm.c or kernel/kalloc.c.' \
    'Linux excerpt: v6.12 at adc218676eef25575469234709c2d87185ca223a. Re-read that tree or the booted sources before calling a line current.'
  printf '\n== limits ==\n'
  echo 'No root, no network, no host reclaim, no host swap, and no OOM kill.'
} >"$REPORT" 2>&1
awk '/^Week 13|^Generated|^== |WEEK13 PASS|PASS |SKIP |maps_anon|zero_fill|pressure_induced|observation_only|vmstat_scope|perf_claim/' \
  "$REPORT"
printf '%s\n' "PASS: report generated at $REPORT" \
  'LIMIT: /proc/self describes this process. /proc/vmstat and perf do not, and a passing report is not mastery.'
