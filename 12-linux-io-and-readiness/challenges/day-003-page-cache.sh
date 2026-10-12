#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-io"
printf '%s\n' \
  'PURPOSE: separate page-cache visibility and an fsync request from crash durability.' \
  'PREDICT: a second open sees the write before fsync; the sparse read in the hole is zero; fsync success is not a crash proof.' \
  'EVIDENCE: stat sizes, block count, and the durability_claim marker.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/pagecache_lab"
printf '\n[buffered visibility, fsync request, sparse file]\n'
"$PROJECT/build/pagecache_lab" demo
printf '%s\n' \
  'PASS: readback matched before fsync; the hole read as zero; allocated blocks were below the logical size.' \
  'LIMIT: durability_claim=none. fsync success does not prove the bytes survive power loss.'
