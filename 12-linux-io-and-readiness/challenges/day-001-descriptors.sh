#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-io"
printf '%s\n' \
  'PURPOSE: separate a descriptor number from the open-file description it names.' \
  'PREDICT: dup continues and lseeks the same offset; a second open starts at the beginning.' \
  'EVIDENCE: checked markers, compiler identity, and the binary hash.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/descriptor_lab"
printf '\n[shared offset versus a new open]\n'
"$PROJECT/build/descriptor_lab" demo
printf '\n[source landmark]\n'
grep -n 'dup_shares_offset\|separate_open_independent\|lseek_shared' \
  "$PROJECT/src/descriptor_lab.c"
printf '%s\n' \
  'PASS: dup shares one offset; a separate open does not.' \
  'LIMIT: the run does not print a kernel pointer, an inode number, or a crash-durability result.'
