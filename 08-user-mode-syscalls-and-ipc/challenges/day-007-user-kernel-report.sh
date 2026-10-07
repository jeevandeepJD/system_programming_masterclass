#!/usr/bin/env bash
# Generate a temporary Week 8 user/kernel transition and fault evidence report.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
REPORT=$(mktemp /tmp/toyos-week8-user-kernel-report.XXXXXX.txt)
ELF="$TOYOS/build/week8/toyos.elf"
USER_ELF="$TOYOS/build/week8/user.elf"
POINTER_ELF="$TOYOS/build/week8-user-pointer/toyos.elf"
SYSCALL_ELF="$TOYOS/build/week8-syscall/toyos.elf"

printf '%s\n' \
  'PURPOSE: collect reproducible privilege, syscall, copy, IPC, fault, and comparison evidence.' \
  'PREDICT: normal/pointer/syscall host statuses are 33/43/45 with exact checked markers.' \
  "EVIDENCE: temporary report at $REPORT; add causal interpretation separately."

{
  printf 'ToyOS Week 8 user/kernel evidence report\n'
  printf 'Generated (UTC): '; date -u '+%Y-%m-%dT%H:%M:%SZ'
  printf 'Canonical project: %s\n\n' "$TOYOS"

  printf '== Tool identity ==\n'
  gcc --version | awk 'NR==1'
  ld --version | awk 'NR==1'
  qemu-system-x86_64 --version | awk 'NR==1'

  printf '\n== Complete canonical Week 8 regression ==\n'
  make -C "$TOYOS" --no-print-directory test-week8

  printf '\n== Exact artifact identities ==\n'
  sha256sum "$ELF" "$USER_ELF" "$POINTER_ELF" "$SYSCALL_ELF"
  for artifact in "$ELF" "$USER_ELF" "$POINTER_ELF" "$SYSCALL_ELF"; do
    printf '%s\n' "$artifact"
    readelf -h "$artifact" | awk '/Class:|Machine:|Entry point address:/'
  done

  printf '\n== Ordered boundary symbols ==\n'
  nm -n "$ELF" | awk '$3=="address_space_create" || $3=="load_elf" ||
    $3=="process_init" || $3=="enter_user" || $3=="syscall_int_stub" ||
    $3=="user_translate" || $3=="copy_from_user" ||
    $3=="syscall_dispatch" {print}'
  nm -n "$USER_ELF" | awk '$3=="_user_start" || $3=="pipe_fds" ||
    $3=="io_buffer" {print}'

  printf '\n== Source contracts ==\n'
  awk 'NR>=143 && NR<=179 || NR>=204 && NR<=218 || NR>=232 && NR<=266 {print}' \
    "$TOYOS/src/arch.c"
  awk 'NR>=58 && NR<=103 {print}' "$TOYOS/src/isr.S"
  cat "$TOYOS/src/user_entry.S"
  awk 'NR>=79 && NR<=168 || NR>=184 && NR<=306 {print}' "$TOYOS/src/process.c"
  awk 'NR>=189 && NR<=289 {print}' "$TOYOS/src/memory.c"

  printf '\n== Integrated predicted trace ==\n'
  printf '%s\n' \
    'user: CPL3, process CR3, user RSP; RAX/RDI/RSI/RDX describe request.' \
    'gate: IDT[128] DPL3 interrupt gate; CPU loads TSS RSP0 and saves user return state.' \
    'stub: kernel stack owns interrupt_frame; vector/error/GPRs normalized for C.' \
    'dispatch: policy bounds lengths; walker validates each user page; kernel buffer stages bytes.' \
    'return: result replaces saved RAX; pops restore GPRs; iretq restores user RIP/CS/RFLAGS/RSP/SS.'

  printf '\n== Fault detection boundaries ==\n'
  printf '%s\n' \
    'pointer: software page-table walk rejects 0x1234 before user entry; marker/status 43.' \
    'syscall: direct synthetic-frame dispatch maps UINT64_MAX to -38 before user entry; marker/status 45.' \
    'neither injected profile is an architectural user page fault or signal.'

  printf '\n== Comparison landmarks ==\n'
  printf '%s\n' \
    'xv6-riscv: kernel/proc.c vm.c exec.c trampoline.S trap.c syscall.c pipe.c; verify revision.' \
    'Linux: arch/x86/entry/ arch/x86/mm/ mm/ fs/ ipc/ kernel/signal.c; inspect current revision.'

  printf '\n== Evidence limits ==\n'
  printf '%s\n' \
    'One process; no scheduling, fork/exec replacement, teardown, demand paging, SMP, locks, or W^X.' \
    'Pipe is a global 64-byte copied linear buffer; no blocking, close/EOF, shared memory, or signals.' \
    'The ABI is DPL3 int 0x80 plus iretq, explicitly not a SYSCALL/SYSRET fast path.'
} >"$REPORT" 2>&1

printf '\n[key report evidence]\n'
awk '/^ToyOS Week|^Generated|^Canonical project|^== |PASS|EXPECTED-FAULT|Evidence limits/{print}' \
  "$REPORT"

printf '\nPASS: Week 8 report generated at %s\n' "$REPORT"
printf '%s\n' 'LIMIT: generated evidence still requires the learner’s causal explanation.'
