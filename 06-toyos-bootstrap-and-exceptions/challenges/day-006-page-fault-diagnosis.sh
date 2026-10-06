#!/usr/bin/env bash
# Reproduce the page fault and prepare exact-ELF symbolized GDB diagnosis.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
ELF="$TOYOS/build/week6-page/toyos.elf"

printf '%s\n' \
  'PURPOSE: diagnose the deliberate non-present supervisor write using exact artifacts.' \
  'PREDICT: vector 14, CR2 0x70000000, error bits “not-present write supervisor”.' \
  'EVIDENCE: checked serial output, symbols/disassembly, and matching GDB script ELF.'

printf '\n[dynamic page-fault evidence]\n'
make -C "$TOYOS" --no-print-directory expected-fault-week6-page

printf '\n[relevant exact-ELF symbols]\n'
nm -n "$ELF" | awk \
  '$3=="kernel_main" || $3=="interrupt_handler" || $3=="isr14" ||
   $3=="isr_common" || $3=="interrupt_stack" {print}'

printf '\n[fault-injection disassembly and source]\n'
objdump -dS --disassemble=kernel_main "$ELF"

printf '\n[GDB artifact contract]\n'
awk '/^file |^target |^break |^continue|^# At/{print}' "$TOYOS/gdb/fault.gdb"
grep -Fq 'file build/week6-page/toyos.elf' "$TOYOS/gdb/fault.gdb"

cat <<'EOF'

[optional supervised live session]
Purpose: stop at interrupt_handler and inspect frame, CR2, and saved RIP.
Prediction: frame->vector is 14 and $cr2 is 0x70000000.
Terminal 1, from projects/toyos: make debug-fault-week6-page
Terminal 2, from projects/toyos: make gdb-fault
At the breakpoint, evidence queries: p/x *frame; p/x $cr2; x/10i frame->rip
Stop GDB/QEMU when finished; do not present unobserved values as live evidence.
EOF

printf '%s\n' \
  'PASS: deterministic fault evidence and exact symbol artifacts are available.' \
  'LIMIT: this wrapper does not claim a live GDB observation or test nested faults.'
