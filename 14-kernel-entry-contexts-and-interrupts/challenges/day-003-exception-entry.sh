#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-kernel-contexts"
TOY="$ROOT/projects/toyos"
export LINUX_SRC="${LINUX_SRC:-/home/jd/Documents/linux-6.12}"
printf '%s\n' \
  'PURPOSE: separate a page-fault entry from the SIGSEGV a child can observe.' \
  'PREDICT: ToyOS vector 14 is ISR_ERR; X86_PF_WRITE is bit 1; the child dies with SIGSEGV.' \
  'EVIDENCE: source landmarks, the ToyOS bit masks, and the reaped child.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/fault_lab"
"$PROJECT/scripts/check-source.sh" 3
printf '\n[toyos and error-code bits]\n'
grep -n 'ISR_ERR   14' "$TOY/src/isr.S"
grep -n 'frame->error & 1U\|frame->error & 2U\|frame->error & 4U' "$TOY/src/arch.c"
grep -n 'X86_PF_WRITE' "$LINUX_SRC/arch/x86/include/asm/trap_pf.h"
grep -n 'ESR_ELx_EC_DABT_LOW' "$LINUX_SRC/arch/arm64/include/asm/esr.h" | awk 'NR==1'
grep -n 'EXC_STORE_PAGE_FAULT' "$LINUX_SRC/arch/riscv/include/asm/csr.h"
printf '\n[userspace fault and signal]\n'
"$PROJECT/build/fault_lab" demo
printf '%s\n' \
  'PASS: vector 14 is an error-code stub, and the child SIGSEGV was reaped.' \
  'LIMIT: irq_claim=none. The lab does not trace exc_page_fault or resume the store.'
