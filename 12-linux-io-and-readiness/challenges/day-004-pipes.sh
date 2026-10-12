#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-io"
printf '%s\n' \
  'PURPOSE: connect dup, dup2, writer lifetime, a full pipe, and PIPE_BUF.' \
  'PREDICT: EOF waits for every write end; a full nonblocking pipe returns EAGAIN; a PIPE_BUF write is all-or-EAGAIN.' \
  'EVIDENCE: redirection text, fill count, and atomic-write markers.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/pipe_lab"
printf '\n[pipe sharing, EOF, dup2, backpressure, PIPE_BUF]\n'
"$PROJECT/build/pipe_lab" demo
printf '%s\n' \
  'PASS: one writer remaining was not EOF; dup2 redirected stdout; the full pipe pushed back; PIPE_BUF writes were atomic.' \
  'LIMIT: no sockets, no network, and no claim about pipes larger than the bounded capacity set in the lab.'
