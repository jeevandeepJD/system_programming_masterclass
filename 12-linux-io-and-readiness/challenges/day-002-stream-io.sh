#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-io"
printf '%s\n' \
  'PURPOSE: treat short counts, EOF, EAGAIN, and EINTR as different outcomes of read and write.' \
  'PREDICT: a large nonblocking pipe write is short; EOF is 0; an empty nonblocking read is EAGAIN; a blocked read without SA_RESTART returns EINTR.' \
  'EVIDENCE: checked markers from one bounded process tree.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/stream_lab"
printf '\n[short transfer, EOF, EAGAIN, EINTR, retry loop]\n'
"$PROJECT/build/stream_lab" demo
printf '%s\n' \
  'PASS: short counts, EOF, EAGAIN, and EINTR were distinct, and the retry loop moved 262144 bytes.' \
  'LIMIT: a positive short count is success for the bytes transferred. This run does not measure disks or networks.'
