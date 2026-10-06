#!/usr/bin/env bash
# Generate a temporary, reproducible Week 6 fault-debug evidence report.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
REPORT=$(mktemp /tmp/toyos-week6-fault-report.XXXXXX.txt)

printf '%s\n' \
  'PURPOSE: collect canonical source, ELF, tool, and regression evidence in one report.' \
  'PREDICT: normal 33; divide/page 35; assert 37; every expected marker is present.' \
  "EVIDENCE: the generated temporary report at $REPORT"

{
  printf 'ToyOS Week 6 fault-debug evidence\n'
  printf 'Generated (UTC): '
  date -u '+%Y-%m-%dT%H:%M:%SZ'
  printf 'Canonical project: %s\n\n' "$TOYOS"

  printf '== Tool versions ==\n'
  gcc --version | awk 'NR==1'
  ld --version | awk 'NR==1'
  qemu-system-x86_64 --version | awk 'NR==1'
  gdb --version | awk 'NR==1'

  printf '\n== Build and artifact identity ==\n'
  make -C "$TOYOS" --no-print-directory week6
  sha256sum "$TOYOS/build/week6/toyos.elf"
  readelf -h "$TOYOS/build/week6/toyos.elf" |
    awk '/Class:|Machine:|Entry point address:/'

  printf '\n== Contract source excerpts ==\n'
  awk 'NR>=5 && NR<=22 {printf "%4d  %s\n", NR, $0}' "$TOYOS/src/kernel.c"
  awk 'NR>=196 && NR<=209 {printf "%4d  %s\n", NR, $0}' "$TOYOS/src/arch.c"
  awk 'NR>=249 && NR<=284 {printf "%4d  %s\n", NR, $0}' "$TOYOS/src/arch.c"

  printf '\n== Ordered exception symbols ==\n'
  nm -n "$TOYOS/build/week6/toyos.elf" |
    awk '$3=="kernel_main" || $3=="interrupt_handler" ||
         $3=="isr_common" || $3=="isr14" {print}'

  printf '\n== Complete canonical Week 6 regression ==\n'
  make -C "$TOYOS" --no-print-directory test-week6

  printf '\n== Evidence limits ==\n'
  printf '%s\n' \
    'QEMU TCG is not physical hardware.' \
    'The suite covers selected normal, divide, page, and assertion paths.' \
    'No user mode, demand paging, nested-fault, or per-CPU behavior is tested.' \
    'xv6/Linux comparisons require separately identified source revisions.'
} >"$REPORT" 2>&1

printf '\n[key report evidence]\n'
awk '/^ToyOS Week|^Generated|^Canonical project|^== |PASS|LOG-CHECK|Evidence limits/{print}' \
  "$REPORT"

printf '\nPASS: report generated at %s\n' "$REPORT"
printf '%s\n' \
  'LIMIT: the report is raw evidence; the learner must supply causal interpretation.'
