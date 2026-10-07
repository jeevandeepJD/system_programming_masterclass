#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-system-programming"
REPORT_DIR=$(mktemp -d /tmp/week11-debug-report.XXXXXX)
REPORT="$REPORT_DIR/report.txt"
printf '%s\n' \
  'PURPOSE: generate a reproducible thread debugging report with identity, intentional failures, tools, and limits.' \
  'PREDICT: WEEK11 PASS; bounded deadlock detection; optional tools produce evidence or accurate skips.' \
  "EVIDENCE: complete temporary report $REPORT"
make -C "$PROJECT" --no-print-directory build
{
  printf 'Week 11 debugging report\nGenerated: '; date -u '+%Y-%m-%dT%H:%M:%SZ'
  printf '\n== identity ==\n'; uname -a; cc --version | awk 'NR==1'
  sha256sum "$PROJECT/build/thread_lab" "$PROJECT/build/queue_lab" \
    "$PROJECT/build/deadlock_lab" "$PROJECT/build/litmus_lab"
  printf '\n== deterministic regression ==\n'
  make -C "$PROJECT" --no-print-directory test-week11
  printf '\n== intentional bounded failures/non-assertions ==\n'
  "$PROJECT/build/thread_lab" race
  "$PROJECT/build/deadlock_lab" detect
  "$PROJECT/build/thread_lab" false-sharing
  "$PROJECT/build/litmus_lab"
  printf '\n== gdb ==\n'
  if command -v gdb >/dev/null 2>&1; then
    if gdb -q -batch -ex 'break mode_basic' -ex 'run basic' -ex 'info threads' \
        -ex 'continue' --args "$PROJECT/build/thread_lab" >"$REPORT_DIR/gdb.log" 2>&1 &&
        grep -F 'pthread_create_join=ok' "$REPORT_DIR/gdb.log" >/dev/null; then
      echo 'PASS: GDB batch capture.'
    else
      echo 'SKIP: GDB unavailable under current symbols/ptrace/runtime policy; diagnostic retained.'
    fi
  else echo 'SKIP: gdb not installed.'; fi
  printf '\n== valgrind ==\n'
  if command -v valgrind >/dev/null 2>&1; then
    if valgrind --quiet --leak-check=full --errors-for-leak-kinds=all \
        --error-exitcode=91 "$PROJECT/build/queue_lab" >"$REPORT_DIR/valgrind.log" 2>&1; then
      echo 'PASS: Valgrind Memcheck queue run.'
    else echo 'FAIL-EVIDENCE: Valgrind reported an error; diagnostic retained.'; fi
  else echo 'SKIP: Valgrind not installed.'; fi
  compiler=${CC:-cc}
  printf '\n== asan-ubsan ==\n'
  if "$compiler" -std=c17 -D_POSIX_C_SOURCE=200809L -pthread \
      -fsanitize=address,undefined -fno-omit-frame-pointer -O1 \
      "$PROJECT/src/thread_lab.c" -o "$REPORT_DIR/thread_lab-asan" \
      2>"$REPORT_DIR/asan-build.log"; then
    if ASAN_OPTIONS=detect_leaks=1 "$REPORT_DIR/thread_lab-asan" ownership \
        >"$REPORT_DIR/asan-run.log" 2>&1; then
      echo 'PASS: ASan/UBSan ownership run.'
    else echo 'FAIL-EVIDENCE: ASan/UBSan runtime reported failure; diagnostic retained.'; fi
  else echo 'SKIP: ASan/UBSan instrumentation unavailable.'; fi
  printf '\n== tsan ==\n'
  if "$compiler" -std=c17 -D_POSIX_C_SOURCE=200809L -pthread -fsanitize=thread -O1 \
      "$PROJECT/src/thread_lab.c" -o "$REPORT_DIR/thread_lab-tsan" \
      2>"$REPORT_DIR/tsan-build.log"; then
    set +e
    TSAN_OPTIONS=exitcode=66 "$REPORT_DIR/thread_lab-tsan" race \
      >"$REPORT_DIR/tsan-run.log" 2>&1
    status=$?
    set -e
    if [ "$status" -eq 66 ] && grep -Fi 'data race' "$REPORT_DIR/tsan-run.log" >/dev/null; then
      echo 'PASS: ThreadSanitizer detected intentional race.'
    elif grep -Eqi 'unexpected memory mapping|ThreadSanitizer.*(fatal|failed)|FATAL: ThreadSanitizer' "$REPORT_DIR/tsan-run.log"; then
      echo 'SKIP: TSan built but runtime is unavailable/incompatible here.'
    else
      echo "SKIP: TSan produced no recognized race report (status=$status); not proof of safety."
    fi
  else echo 'SKIP: TSan instrumentation unavailable.'; fi
  printf '\n== comparison landmarks ==\n'
  printf '%s\n' 'ToyOS: pin course milestone; inspect scheduler/lock/wakeup capabilities.' \
    'xv6: pin revision; inspect proc.c, spinlock, sleep/wakeup, VM, and any thread lab.' \
    'Linux: pin kernel/libc; inspect task/clone, futex, scheduler, TLS ABI, and ptrace interfaces.'
  printf '\n== limits ==\n'
  echo 'No exhaustive schedules, stress, fairness proof, kernel rebuild, runtime guarantee, or portability inference from one ISA.'
} >"$REPORT" 2>&1
awk '/^Week 11|^Generated|^== |WEEK11 PASS|PASS:|SKIP:|FAIL-EVIDENCE:|data_race=|deadlock_cycle|caveat=/' "$REPORT"
printf '%s\n' "PASS: report generated at $REPORT" \
  'LIMIT: tool output still requires a source-level lifetime and happens-before explanation.'
