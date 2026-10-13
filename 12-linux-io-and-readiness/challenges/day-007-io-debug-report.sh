#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-io"
mkdir -p "$PROJECT/build"
REPORT="$PROJECT/build/week12-report.txt"
printf '%s\n' \
  'PURPOSE: record descriptor identity, deliberate I/O failures, and an optional strace.' \
  'PREDICT: WEEK12 PASS; EBADF, EPIPE, and the epoll errno values are reported; strace passes or skips.' \
  "EVIDENCE: $REPORT"
make -C "$PROJECT" --no-print-directory build
{
  printf 'Week 12 I/O debugging report\nGenerated: '
  date -u '+%Y-%m-%dT%H:%M:%SZ'
  printf '\n== identity ==\n'
  uname -a
  cc --version | awk 'NR==1'
  sha256sum "$PROJECT"/build/*_lab
  printf '\n== deterministic regression ==\n'
  make -C "$PROJECT" --no-print-directory test-week12
  printf '\n== /proc/self/fd and deliberate failures ==\n'
  "$PROJECT/build/debug_lab" demo
  printf '\n== strace ==\n'
  if command -v strace >/dev/null 2>&1; then
    if strace -o "$PROJECT/build/strace-io.log" "$PROJECT/build/debug_lab" demo \
        >/dev/null 2>"$PROJECT/build/strace-io.err" &&
        grep -E 'openat|read|write|close|epoll_ctl|readlink' \
          "$PROJECT/build/strace-io.log" >/dev/null; then
      echo 'PASS: strace captured open/read/write/close/epoll.'
    else
      echo 'SKIP: strace unavailable under current ptrace/runtime policy; diagnostic retained.'
    fi
  else
    echo 'SKIP: strace not installed.'
  fi
  printf '\n== comparison landmarks ==\n'
  printf '%s\n' \
    'ToyOS: pin the Week 8/9 milestone; descriptors and the small pipe are not a VFS or a page cache.' \
    'xv6: pin a revision before quoting file.c or pipe.c.' \
    'Linux excerpt: v6.12 at adc218676eef25575469234709c2d87185ca223a. Re-read that tree or the booted sources before calling a line current.'
  printf '\n== limits ==\n'
  echo 'No root, no network, no crash-durability claim, and no universal mmap ranking.'
} >"$REPORT" 2>&1
awk '/^Week 12|^Generated|^== |WEEK12 PASS|PASS:|SKIP:|proc_fd_matches|deliberate_|epoll_|durability_claim|performance_claim/' \
  "$REPORT"
printf '%s\n' "PASS: report generated at $REPORT" \
  'LIMIT: errno and /proc/self/fd identify the failure; they do not by themselves explain the surrounding protocol.'
