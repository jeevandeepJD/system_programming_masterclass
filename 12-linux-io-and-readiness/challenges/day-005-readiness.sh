#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-io"
printf '%s\n' \
  'PURPOSE: compare select, poll, level-triggered epoll, and edge-triggered epoll on one pipe.' \
  'PREDICT: LT stays ready while bytes remain; ET does not repeat until a new edge, so the reader drains to EAGAIN.' \
  'EVIDENCE: timeout-zero waits and the short read that follows a ready descriptor.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/readiness_lab"
printf '\n[select, poll, epoll LT, epoll ET]\n'
"$PROJECT/build/readiness_lab" demo
printf '%s\n' \
  'PASS: LT reported the remainder; ET stayed quiet until the drain and the next edge.' \
  'LIMIT: readiness is not a promise that the next read returns the full request.'
