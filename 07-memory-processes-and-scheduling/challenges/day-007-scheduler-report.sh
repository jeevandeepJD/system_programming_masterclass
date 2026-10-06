#!/usr/bin/env bash
# Generate a temporary Week 7 memory-and-scheduler evidence report.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
REPORT=$(mktemp /tmp/toyos-week7-scheduler-report.XXXXXX.txt)
ELF="$TOYOS/build/week7/toyos.elf"
PAGE_ELF="$TOYOS/build/week7-page/toyos.elf"
ALLOCATOR_ELF="$TOYOS/build/week7-allocator/toyos.elf"
CONTEXT_ELF="$TOYOS/build/week7-context/toyos.elf"

printf '%s\n' \
  'PURPOSE: collect reproducible memory, scheduling, fault, and comparison evidence.' \
  'PREDICT: normal/page/allocator/context statuses are 33/35/39/41 with exact markers.' \
  "EVIDENCE: temporary report at $REPORT; add causal interpretation separately."

{
  printf 'ToyOS Week 7 scheduler evidence report\n'
  printf 'Generated (UTC): '; date -u '+%Y-%m-%dT%H:%M:%SZ'
  printf 'Canonical project: %s\n\n' "$TOYOS"

  printf '== Tool identity ==\n'
  gcc --version | awk 'NR==1'
  ld --version | awk 'NR==1'
  qemu-system-x86_64 --version | awk 'NR==1'

  printf '\n== Profile contracts ==\n'
  awk 'NR>=56 && NR<=61 || NR>=77 && NR<=90 {print}' "$TOYOS/Makefile"

  printf '\n== Complete canonical Week 7 regression ==\n'
  make -C "$TOYOS" --no-print-directory test-week7

  printf '\n== Exact artifact identities ==\n'
  sha256sum "$ELF" "$PAGE_ELF" "$ALLOCATOR_ELF" "$CONTEXT_ELF"
  for artifact in "$ELF" "$PAGE_ELF" "$ALLOCATOR_ELF" "$CONTEXT_ELF"; do
    printf '%s\n' "$artifact"
    readelf -h "$artifact" | awk '/Class:|Machine:|Entry point address:/'
  done

  printf '\n== Ordered subsystem symbols ==\n'
  nm -n "$ELF" | awk '$3=="memory_init" || $3=="frame_alloc" ||
    $3=="map_page" || $3=="kalloc" || $3=="context_switch" ||
    $3=="scheduler_tick" || $3=="tasks_start" {print}'
  nm -n "$PAGE_ELF" |
    awk '$3=="kernel_main" || $3=="interrupt_handler" {print}'
  nm -n "$ALLOCATOR_ELF" |
    awk '$3=="memory_init" || $3=="frame_is_used" || $3=="frame_free" {print}'
  nm -n "$CONTEXT_ELF" |
    awk '$3=="schedule" || $3=="tasks_init" || $3=="context_switch" {print}'

  printf '\n== Source contracts ==\n'
  awk 'NR>=39 && NR<=43 || NR>=94 && NR<=129 || NR>=147 && NR<=169 ||
       NR>=232 && NR<=248 {print}' "$TOYOS/src/memory.c"
  awk 'NR>=25 && NR<=51 || NR>=98 && NR<=153 {print}' "$TOYOS/src/tasks.c"
  cat "$TOYOS/src/switch.S"
  awk 'NR>=234 && NR<=243 {print}' "$TOYOS/src/arch.c"

  printf '\n== Predicted state trace ==\n'
  printf '%s\n' \
    'init:       t0=RUNNABLE t1=RUNNABLE current=none stack=bootstrap' \
    'first run:  t0=RUNNING  t1=RUNNABLE current=t0   stack=t0' \
    'timer:      t0=RUNNABLE t1=RUNNING  current=t1   stack=t1; t0 IRQ frame retained' \
    'completion: each RUNNING task becomes BLOCKED; no wakeup path'

  printf '\n== Injected-fault interpretations ==\n'
  printf '%s\n' \
    'allocator: second frame_free(probe) sees an already-clear bit; marker checked; host status 39.' \
    'page: CR2=0x70000000, vector 14, not-present write supervisor; marker checked; host status 35.' \
    'context: injected rsp += 1 violates 8-mod-16 rule before context_switch; marker checked; host status 41.'

  printf '\n== Comparison landmarks ==\n'
  printf '%s\n' \
    'xv6-riscv: kernel/kalloc.c kernel/vm.c kernel/proc.c kernel/swtch.S kernel/trap.c; record revision.' \
    'Linux: arch/x86/entry/ arch/x86/mm/ mm/ kernel/sched/; inspect current revision.' \
    'ToyOS has ring-0 kernel tasks, not isolated user processes.'

  printf '\n== Evidence limits ==\n'
  printf '%s\n' \
    'QEMU TCG is not physical hardware; no SMP, APIC, fairness, or timing proof.' \
    'The injected cases cover one double-free, one unmapped write, and one RSP misalignment only.' \
    'No ring 3, syscall ABI, user ELF loader, or per-process address spaces.'
} >"$REPORT" 2>&1

printf '\n[key report evidence]\n'
awk '/^ToyOS Week|^Generated|^Canonical project|^== |PASS|EXPECTED-FAULT|Evidence limits/{print}' \
  "$REPORT"

printf '\nPASS: scheduler report generated at %s\n' "$REPORT"
printf '%s\n' 'LIMIT: generated evidence still requires the learner’s causal explanation.'
