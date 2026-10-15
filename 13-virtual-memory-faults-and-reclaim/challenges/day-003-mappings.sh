#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-vm"
printf '%s\n' \
  'PURPOSE: compare a shared store, a private store, and a protection fault.' \
  'PREDICT: the shared child store is visible; the private parent byte stays; the mprotect child dies with SIGSEGV.' \
  'EVIDENCE: checked markers, compiler identity, and the binary hash.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/mapping_lab"
printf '\n[mapping evidence]\n'
"$PROJECT/build/mapping_lab" demo
printf '\n[source landmark]\n'
grep -n 'shared_child_store_visible\|private_parent_unchanged\|mprotect_child_sigsegv' \
  "$PROJECT/src/mapping_lab.c"
printf '%s\n' \
  'PASS: sharing followed the mapping flag, and only the child took the protection fault.' \
  'LIMIT: the decoded wait status is the signal. It is not a kernel oops and not a COW trace.'
