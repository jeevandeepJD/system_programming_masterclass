#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-kernel-contexts"
export LINUX_SRC="${LINUX_SRC:-/home/jd/Documents/linux-6.12}"
printf '%s\n' \
  'PURPOSE: read syscall entry and return, then complete one userspace syscall.' \
  'PREDICT: getpid matches syscall(SYS_getpid); the pipe byte comes back as k.' \
  'EVIDENCE: entry_SYSCALL_64, x64_sys_call, and the lab markers.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/syscall_lab"
"$PROJECT/scripts/check-source.sh" 2
printf '\n[table row and toyos gate]\n'
grep -n 'write' "$LINUX_SRC/arch/x86/entry/syscalls/syscall_64.tbl" | awk 'NR==1'
grep -n 'set_idt_gate(128, syscall_int_stub' "$ROOT/projects/toyos/src/arch.c"
printf '\n[userspace syscall]\n'
"$PROJECT/build/syscall_lab" demo
printf '%s\n' \
  'PASS: the snapshot contains the SYSCALL path, and the lab completed getpid and a pipe write.' \
  'LIMIT: entry_path_claim=none. The lab does not show SYSRET versus IRET.'
