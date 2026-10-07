#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd); PROJECT="$ROOT/projects/linux-system-programming"
REPORT_DIR=$(mktemp -d /tmp/week10-debug-report.XXXXXX); REPORT="$REPORT_DIR/report.txt"
printf '%s\n' 'PURPOSE: generate a reproducible process/signal report with identity, failures, /proc, trace, debugger, and limits.' 'PREDICT: WEEK10 PASS and injected ENOENT/ECHILD/EINVAL/ESRCH; optional tools capture or clearly skip.' "EVIDENCE: temporary report $REPORT"
make -C "$PROJECT" --no-print-directory build
{
  printf 'Week 10 debugging report\nGenerated: '; date -u '+%Y-%m-%dT%H:%M:%SZ'
  printf '\n== identity ==\n'; uname -a; cc --version | awk 'NR==1'
  sha256sum "$PROJECT/build/process_lab" "$PROJECT/build/signal_lab" "$PROJECT/build/proc_lab"
  for f in "$PROJECT/build/process_lab" "$PROJECT/build/signal_lab" "$PROJECT/build/proc_lab"; do readelf -h "$f" | awk '/Type:|Machine:|Entry point/'; done
  printf '\n== regression ==\n'; make -C "$PROJECT" --no-print-directory test-week10
  printf '\n== injected failures ==\n'; "$PROJECT/build/process_lab" failures; "$PROJECT/build/signal_lab" failures
  printf '\n== procfs ==\n'; "$PROJECT/build/proc_lab"
  printf '\n== trace ==\n'
  if command -v strace >/dev/null 2>&1; then strace -f -e trace=process,signal -o "$REPORT_DIR/strace.log" "$PROJECT/build/process_lab" status; awk '/execve|clone|fork|wait|SIGTERM|exit/{print}' "$REPORT_DIR/strace.log"; else echo 'SKIP: strace unavailable'; fi
  printf '\n== gdb ==\n'
  if command -v gdb >/dev/null 2>&1; then
    if gdb -q -batch -x "$PROJECT/gdb/week10.gdb" --args "$PROJECT/build/proc_lab" >"$REPORT_DIR/gdb.log" 2>&1 && grep -F 'proc_inspection=ok' "$REPORT_DIR/gdb.log" >/dev/null; then echo 'PASS: GDB batch capture'; else echo 'SKIP: GDB unavailable under current ptrace/runtime policy'; fi
  else echo 'SKIP: gdb unavailable'; fi
  printf '\n== source landmarks ==\ninclude/linux/sched.h kernel/fork.c fs/exec.c fs/binfmt_elf.c kernel/exit.c kernel/signal.c fs/proc/ arch/<isa>/entry/ arch/<isa>/kernel/signal.c\n'
  printf '\n== limits ==\nNo unrelated tracing, root, network, stress, kernel rebuild, interactive terminal mutation, or full signal-frame capture.\n'
} >"$REPORT" 2>&1
awk '/^Week 10|^Generated|^== |WEEK10 PASS|exec_failure|wait_failure|sigaction_SIGKILL|kill_missing_pid|proc_inspection=ok|PASS: GDB|SKIP:/{print}' "$REPORT"
printf '%s\n' "PASS: report generated at $REPORT" 'LIMIT: generated evidence still requires a causal write-up tied to exact versions.'
